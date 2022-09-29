/* Check in revisions of RCS files from working files.  */

/* Copyright 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
   Copyright 1996 Silicon Graphics
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

/*
 * $Log: rcschki.c,v $
 * Revision 1.1  1996/06/28 23:07:50  msf
 * Initial revision
 *
 */

#include "rcspriv.h"
#include "rcsbase.h"

struct Symrev {
       char const *ssymbol;
       int override;
       struct Symrev * nextsym;
};

static char const *getcurdate P((void));
static int addbranch P((struct hshentry*,struct buf*,int));
static int addelta P((const char*));
static int addsyms P((char const*));
static int removelock P((struct hshentry*, int warn));
static int xpandfile P((RILE*,struct hshentry const*,int,FILE*));
static struct cbuf getlogmsg P((int));
static int cleanup P((void));
static void incnum P((char const*,struct buf*));
static void addassoclst P((int,char const*));

static char const default_state[] = DEFAULTSTATE;

/* Memory heap managed locally for options that persist across calls */
static struct rcsHeap optsHeap = {NULL, NULL, NULL};

/* Local variables, with information pertaining to current ci request -
 * their use spans rcsCheckIn() and cifinish()
 */
static RILE *workptr;			/* working file pointer		*/
static struct buf newdelnum = {NULL, 0};/* new revision number		*/
static bool removedlock;		/* was lock for olddelta removed*/
static struct cbuf msg;

/* Flags which are local to this command (ci), but remembered from
 * file to file (unless changed by opts).
 */
static bool forceciflag;		/* forces check in		*/
static bool lockflag;			/* lock new delta		*/

static int rcsinitflag;		/* admin data must be initialized
				 * (because file did not exist, or
				 * was empty - created by rcs -i)
				 */
static int keepflag;

static RCSTMP *keepworkingfile;	/* keep (or regenerate) working file,
				 * e.g. ci -l.  (This is handle to checked
				 * out file.)
				 */
static const char *rev;		/* new revision requested */
static struct rcsStreamOpts *outOpts;  /* attributes of output file, if any */
static int serverfile;		/* is output file to be on server? */

static const char *dummyworkname;	/* name of tempfile used as workfile */
static struct hshentries *gendeltas;	/* deltas to be generated	*/
static struct hshentry *targetdelta;	/* old delta to be generated	*/
static struct hshentry newdelta;	/* new delta to be inserted	*/
static struct stat workstat;		/* stat of workfile             */
static struct Symrev *assoclst, **nextassoc;

/* Global variables */
const char *author;		/* login of user (author of change)     */
 
	
int rcsCheckIn(conn, workfilehdl, rcspath, opts )
	RCSCONN conn;
	RCSTMP workfilehdl;
	const char *rcspath;
	struct rcsCheckInOpts *opts;

	/* This must be called with a workfile handle, representing
	 * either a tempfile or the actual input file (either way,
	 * produced via rcsReadInData()).  The rcspath is the full
	 * path of an RCS archive.  It is compared against the "cache",
	 * to see whether we need to reinitialize the internal
	 * representation of the archive.
	 * (In the server, this has the effect of setting several global
	 * variables, including finptr, ..., which explains why only
	 * one archive file may be opened at a time.)
	 *
	 * "opts" contains the following fields:
	 *
	 * newrev is the new delta number.  If it is not specified
	 * (NULL ptr), then use usual RCS rules; work from old
         * delta, and increment.  This corresponds to -r[rev].
         *
         * oldrev is the old delta number.  If unstated (NULL), it is
         * determined from the new delta number, locks, etc. according
	 * to usual RCS rules.  If it is specified (which is a new,
	 * lower-level capability), the following rules apply:
	 *
	 * 1. oldrev must be a specific delta, and not a branch (e.g.
	 *    1.2.2.7, not 1.2.2).  If you want to add to the end
	 *    of a branch, leave oldrev NULL, and specify the branch as
	 *    the new delta.
	 *
	 * 2. The effect of specifying oldrev is equivalent to having
	 *    locked oldrev before doing the check in.  Thus, whether
	 *    or not the old delta was locked by the user beforehand,
	 *    it is unlocked when the check in is complete.  However,
	 *    if the old delta was locked by a different user, then
	 *    the lock is honored, and the caller must explicitly
	 *    break the lock before adding the delta.
	 *
	 * 3. The new delta newrev must be compatible with the old delta
	 *    oldrev.  Compatability is defined as usual for specifying a
	 *    new delta, given a locked delta:
	 *    If specified, it must be a "higher" delta (either a
	 *    new branch from o, or the highest delta along that
	 *    branch).  
	 *
	 *    ...
	 *
	 * To facilitate mix and match of options across multiple
	 * files, the old and new rev numbers must be explicitly
	 * passed for each file.  If opts is NULL, the previously
	 * remembered values (or defaults, if first call) are used.
	 * This allows efficient implementation of ci as loop,
	 * calling rcsCheckIn() for each file, but with NULL opts.
	 * (The complete loop would include rcsReadOutData, and
	 *  a resetting of the output stream provided in opts to
	 *  fetch the newly checked out version.)
	 *
	 * The "opts" booleans correspond to the usual ci options, 
	 * wth the qualification that instead of -l[rev] -u[rev] ...,
	 * one merely notes (boolean values) whether the flags are set.
	 * It is up to the calling program to figure out what the
	 * desired new rev number is, and to pass it accordingly.
	 * (The ci command says that the last flag on the command
	 * line rules.  This belongs in main().)
	 *
	 */
{

	static char altdate[datesize];
	char olddate[datesize];
	char newdatebuf[datesize + zonelenmax];
	char targetdatebuf[datesize + zonelenmax];
	static char *state;
	char const *krev;
	int initflag, mustread;
	static int usestatdate; /* Use mod time of file for -d.  */

	static bool has_been_called = false;	/* for first time init */
	size_t len;
	int rc;

	/* In case of internal core code error, return fatal error
	 * to caller, rather than exiting.
	 */
	if ((rc = sigsetjmp(rcsApiEnv, 1)) != 0) return (rc);

	/* Get name of working file (or temp copy) from handle.
	 * Open it for reading.
	 */
	if (rcsMapTmpHdlToName(workfilehdl, &dummyworkname) < 0)
	    return ERR_APPL_ARGS;
	if (!(workptr = Iopen(dummyworkname, FOPEN_R_WORK, &workstat))) {
	    eerror(dummyworkname);
	    if ((int)workfilehdl < TEMPNAMES) return ERR_INTERNAL;
	    else return ERR_NO_FILE;
	}

	/* If running normally, no effect; else preserve RCS semantics */
	setrid();

	if (!has_been_called || opts) {	/* set defaults */

	    if (has_been_called) rcsHeapFree(&optsHeap);  /* discard old opts */

	    state = 0;
	    altdate[0]= '\0'; /* empty alternate date for -d */
	    usestatdate=false;
	    nextassoc = &assoclst;
	    msg.size = 0;
	    msg.string = NULL;
	    keepworkingfile = NULL;
	    outOpts = NULL;
	    forceciflag = false;
	    has_been_called = true;
	    lockflag = false;
	    initflag = false;
	    mustread = false;
	}

	/* Initialize error state (mostly provided by lex analyzer) */
	nerror = 0;

	/* Use opts to override defaults */
	/* -r: keepworkingfile = lockflag = false; (implied by no -l, -u, -k)
	 * -l: keepworkingfile = lockflag = true;
	 * -u: keepworkingfile = true;  lockflag = false;
	 * -i: initflag = true;  (create new file)
	 * -j: mustread = true;  (RCS file must exist)
	 * -f: forceciflag = true;
	 * -k: keepflag = true;  (keep keywords from workfile)
	 * -m: sets msg (delta message)
	 * -n: sets symbolic name to this delta
	 * -N: override symbolic name 
	 * -s: sets state
	 * -t: sets textfile (new file description)
	 * -d: use date for checkin time (free form); sets altdate,
	 *	usestatdate = true;
	 * -M: mtimeflag = true;  (set modify time of checked out file)
	 * -w: sets author	(now done globally)
	 * -x: sets suffixes	(now done globally)
	 * -V: sets RCSversion  (need to do globally)
	 *			(need to change setRCSversion/rcsutil.c;
	 *			 split into separate fcn that is called
	 *			 w/no argument for -V<nothing>: print version
	 * -z: calls zone_set to set output time format (TZ shift; def GMT)
	 *			(need to do globally)
	 * -T: Ttimeflag = true; (sets RCS mod time to delta time, delta later)
	 */

	if (opts) {
	    keepworkingfile = opts->outHandle;  /* ci -l, ci -u */
	    outOpts = opts->outOpts;
	    serverfile = opts->serverfile;
	    if (opts->lockflag) lockflag = true;
	    if (opts->forceflag) forceciflag = true;
	    if (opts->keywordflag) keepflag = true;
	    if (opts->mustexist) mustread = true;
	    if (opts->initflag) initflag = true;

	    if (opts->msg) {
		char *inputmsg;
		len = strlen(opts->msg);
		if (!(inputmsg = rcsHeapAlloc(&optsHeap, len+1)))
		    faterror("Out of Memory");
		memcpy(inputmsg, opts->msg, len+1);
		msg = cleanlogmsg(inputmsg, len);
		if (!msg.size) {
		    error("missing message");
		    return ERR_APPL_ARGS;
		}
	    }

	    if (opts->nameflag) {
		if (!*(opts->nameflag)) {
		    error("missing symbolic names in nameflag");
		    cleanup();
		    return ERR_APPL_ARGS;
		}
		else {
		    if (!checkssym(opts->nameflag)) {
			cleanup();
			return ERR_APPL_ARGS;
		    }
		    addassoclst(false, opts->nameflag);
		}
	    }

	    if (opts->Nameflag) {
		if (!*(opts->Nameflag)) {
		    error("missing symbolic names in Nameflag");
		    cleanup();
		    return ERR_APPL_ARGS;
		}
		else {
		    if (!checkssym(opts->Nameflag)) {
			cleanup();
			return ERR_APPL_ARGS;
		    }
		    addassoclst(true, opts->nameflag);
		}
	    }

	    if (opts->stateflag) {
		if (!*(opts->stateflag)) {
		    error("missing state value for state flag");
		    cleanup();
		    return ERR_APPL_ARGS;
		}
		else {
		    if (!checksid(opts->stateflag)) {
			cleanup();
			return ERR_APPL_ARGS;
		    }
		    len = strlen(opts->stateflag);
		    if (!(state = rcsHeapAlloc(&optsHeap, ++len)))
			faterror("Out of Memory");
		    memcpy(state, opts->stateflag, len);
	 	}
	    }
		
	    if (opts->dateflag) {
		altdate[0] = '\0';
		if (!(usestatdate = !*(opts->dateflag)))
		    str2date(opts->dateflag, altdate);
	    }
	}
		
	targetdelta = 0;

	/* Open or reopen for update the archive file, depending upon
	 * whether it is already open (for read)
	 */
	if (!strcmp(rcsParamName, rcspath)) {
	    if ((rc = rcsreopen(mustread)) < 0) return rc;
	}

	else {
	    if (rcsParamName[0]) rcsCloseArch();  /* close different archive */
	    if((rc = rcsOpenArch(true, mustread, rcspath)) <0) return rc;
	}
	    
	/* See whether RCS file is to be created (noted in rcsinitflag) */
	if (!finptr){			    /* New RCS file */
#		if has_setuid && has_getuid
		    if (euid() != ruid()) {
			workerror("setuid initial checkin prohibited; use `rcs -i -a' first");
			cleanup();
			return ERR_APPL_ARGS;
		    }
#		endif
		rcsinitflag = true;
	}
	else {
		if (initflag) {
			rcserror("already exists");
			cleanup();
			return ERR_ARCHIVE_EXISTS;
		}
		if (!(checkaccesslist())) {
			cleanup();
			return ERR_NO_ACCESS;
		}
		rcsinitflag = !Head; /* do we need to initialize admin data? */
	}

	/*
	 * RCSname contains the name of the RCS file (set
	 * by application call to rcsOpenArch). 
	 * workname contains the workfile name (RCS file w/o ,v)
	 * dummyworkname contains the name of the working file (obtained from
	 * RCSTMP handle, above).
	 * If the RCS file exists, finptr contains the file descriptor for the
	 * RCS file, and RCSstat is set.  The admin node is initialized.
         */

	diagnose("%s  <--  %s\n", RCSname, dummyworkname);

	if (finptr) {
		if (same_file(RCSstat, workstat, 0)) {
			rcserror("RCS file is the same as working file %s.",
				dummyworkname
			);
			(cleanup());
			return ERR_APPL_ARGS;
		}
	}

	krev = rev = opts->newrev;
        if (keepflag) {
                /* get keyword values from working file */
		if (!getoldkeys(workptr)) {
			 nerror++;
			 /* cleanup and return */
		}
		if (!krev  && 
		    (!opts->oldrev || !*(krev = opts->oldrev)) &&
                    !*(krev = prevrev.string)) {
			workerror("can't find a revision number");
                        /* return */
                }
		if (!*prevdate.string && *altdate=='\0' && usestatdate==false)
			workwarn("can't find a date");
		if (!*prevauthor.string && !author)
			workwarn("can't find an author");
		if (!*prevstate.string && !state)
			workwarn("can't find a state");
        } /* end processing keepflag */

	if (nerror) {
		cleanup();
		return ERR_APPL_ARGS;
	}

	/* Read the delta tree.  */
	if (finptr)
	    gettree();

        /* expand symbolic revision number (if krev non-NULL) */
	/* we call fexpandsym even if krev is null, to init newdelnum */
	if (!fexpandsym(krev, &newdelnum, workptr)) {
	    nerror++;
	    cleanup();
	    return ERR_BAD_NEWREV;
	}

        /* splice new delta into tree */
	if ((removedlock = addelta(opts->oldrev)) < 0) {
	    cleanup();
	    return removedlock;
	}

	newdelta.num = newdelnum.string;
	newdelta.branches = 0;
	newdelta.lockedby = 0; /* This might be changed by addlock().  */
	newdelta.selector = true;
	newdelta.name = 0;
	clear_buf(&newdelta.ig);
	clear_buf(&newdelta.igtext);
	/* set author */
	if (author)
		newdelta.author=author;     /* set author given by -w         */
	else if (keepflag && *prevauthor.string)
		newdelta.author=prevauthor.string; /* preserve old author if possible*/
	else    newdelta.author=getcaller();/* otherwise use caller's id      */
	newdelta.state = default_state;
	if (state)
		newdelta.state=state;       /* set state given by -s          */
	else if (keepflag && *prevstate.string)
		newdelta.state=prevstate.string;   /* preserve old state if possible */
	if (usestatdate) {
	    time2date(workstat.st_mtime, altdate);
	}
	if (*altdate!='\0')
		newdelta.date=altdate;      /* set date given by -d           */
	else if (keepflag && *prevdate.string) {
		/* Preserve old date if possible.  */
		str2date(prevdate.string, olddate);
		newdelta.date = olddate;
	} else
		newdelta.date = getcurdate();  /* use current date */
	/* now check validity of date -- needed because of -d and -k          */
	if (targetdelta &&
	    cmpdate(newdelta.date,targetdelta->date) < 0) {
		rcserror("Date %s precedes %s in revision %s.",
			date2str(newdelta.date, newdatebuf),
			date2str(targetdelta->date, targetdatebuf),
			targetdelta->num
		);
		return (cleanup());
	}

	if (lockflag  &&  addlock(&newdelta, true) < 0)
		return(cleanup());

	if (keepflag && *prevname.string)
	    if (addsymbol(newdelta.num, prevname.string, false)  <  0)
		return (cleanup());
	if (!addsyms(newdelta.num))
	    return(cleanup());
    
	return cifinish();		/* separated out for later
					 * reuse (more flexible sequencing)
					 */
}


cifinish()
{
	/* Write out updated RCS file */

	int lockthis;			/* set lock on new delta
					 * (if ci -l flag set, unless ci of
					 *  unchanged file not orig. locked)
					 */

	int dolog;			/* append revision history to
					 * log (true unless ci of unchanged
					 * file, w/o -f [force])
					 */
					 
	int changework;			/* (assuming keepworkingfile true)
					 * working file must be regenerated,
					 * because it has changed
					 * (e.g. keyword expansion w/ci -u)
					 */

	struct hshentry *workdelta;	/* the delta ultimately affected;
					 * the new delta if a ci occurs,
					 * the old delta if not
					 * (some ci flags affect admin data,
					 *  sometimes on old delta)
					 */

	bool newhead;	      /* Did the checkin create a new Head revision? */
	const char *diffname;	       /* name of temp file with delta diffs */
	char const *expname;	      /* name of temp file with new revision */
	bool changedRCS;		   /* does RCS file actually change? */
	time_t wtime;
	int r = 0;			                     /* return value */

	putadmin();
	puttree(Head, frewrite);

	rcsNoStdin = true;
	putdesc(false, /* textfile */0);
	rcsNoStdin = false;

	changework = Expand < MIN_UNCHANGED_EXPAND;
	dolog = true;
	lockthis = lockflag;
	workdelta = &newdelta;

        /* build rest of file */
	if (rcsinitflag) {
		diagnose("initial revision: %s\n", newdelta.num);
                /* get logmessage */
                newdelta.log=getlogmsg(keepflag);
		putdftext(&newdelta, workptr, frewrite, false);
		RCSstat.st_mode = workstat.st_mode;
		RCSstat.st_nlink = 0;
		changedRCS = true;
        } else {
		diffname = maketemp(0);
		newhead  =  Head == &newdelta;
		if (!newhead)
			foutptr = frewrite;
		expname = buildrevision(
			gendeltas, targetdelta, (FILE*)0, false
		);
		if (
		    !forceciflag  &&
		    strcmp(newdelta.state, targetdelta->state) == 0  &&
		    (changework = rcsfcmp(
			workptr, &workstat, expname, targetdelta
		    )) <= 0
		) {
		    diagnose("file is unchanged; reverting to previous revision %s\n",
			targetdelta->num
		    );
		    r = 1;
		    if (removedlock < lockflag) {
			diagnose("previous revision was not locked; ignoring -l option\n");
			lockthis = 0;
		    }
		    dolog = false;
		    if (! (changedRCS = lockflag<removedlock
			    || assoclst))
			workdelta = targetdelta;
		    else {
			/*
			 * We have started to build the wrong new RCS file.
			 * Start over from the beginning.
			 */
			long hwm = ftell(frewrite);
			int bad_truncate;
			Orewind(frewrite);

			/*
			* Work around a common ftruncate() bug:
			* NFS won't let you truncate a file that you
			* currently lack permissions for, even if you
			* had permissions when you opened it.
			* Also, Posix 1003.1b-1993 sec 5.6.7.2 p 128 l 1022
			* says ftruncate might fail because it's not supported.
			*/
#			if !has_ftruncate
#			    undef ftruncate
#			    define ftruncate(fd,length) (-1)
#			endif
			bad_truncate = ftruncate(fileno(frewrite), (off_t)0);

			Irewind(finptr);
			Lexinit();
			getadmin();
			gettree();
			if (!(workdelta = genrevs(
			    targetdelta->num, (char*)0, (char*)0, (char*)0,
			    &gendeltas
			)))
			    return(cleanup());
			workdelta->log = targetdelta->log;
			if (newdelta.state != default_state)
			    workdelta->state = newdelta.state;
			if (lockthis<removedlock &&
			     removelock(workdelta,true)<0)
			    return (cleanup());
			if (!addsyms(workdelta->num))
			    return (cleanup());
			if (dorewrite(true, true) != 0) {
			    nerror++;
			    return (cleanup());
			}
			fastcopy(finptr, frewrite);
			if (bad_truncate)
			    while (ftell(frewrite) < hwm)
				/* White out any earlier mistake with '\n's.  */
				/* This is unlikely.  */
				afputc('\n', frewrite);
		    }
		} else {
		    int wfd = Ifileno(workptr);
		    struct stat checkworkstat;
		    char const *diffv[6 + !!OPEN_O_BINARY], **diffp;
#		    if large_memory && !maps_memory
			FILE *wfile = workptr->stream;
			long wfile_off;
#		    endif
#		    if !has_fflush_input && !(large_memory && maps_memory)
		        off_t wfd_off;
#		    endif

		    diagnose("new revision: %s; previous revision: %s\n",
			newdelta.num, targetdelta->num
		    );
		    newdelta.log = getlogmsg(keepflag);
#		    if !large_memory
			Irewind(workptr);
#			if has_fflush_input
			    if (fflush(workptr) != 0)
				Ierror();
#			endif
#		    else
#			if !maps_memory
			    if (
			    	(wfile_off = ftell(wfile)) == -1
			     ||	fseek(wfile, 0L, SEEK_SET) != 0
#			     if has_fflush_input
			     ||	fflush(wfile) != 0
#			     endif
			    )
				Ierror();
#			endif
#		    endif
#		    if !has_fflush_input && !(large_memory && maps_memory)
			wfd_off = lseek(wfd, (off_t)0, SEEK_CUR);
			if (wfd_off == -1
			    || (wfd_off != 0
				&& lseek(wfd, (off_t)0, SEEK_SET) != 0))
			    Ierror();
#		    endif
		    diffp = diffv;
		    *++diffp = DIFF;
		    *++diffp = DIFFFLAGS;
#		    if OPEN_O_BINARY
			if (Expand == BINARY_EXPAND)
			    *++diffp = "--binary";
#		    endif
		    *++diffp = newhead ? "-" : expname;
		    *++diffp = newhead ? expname : "-";
		    *++diffp = 0;
		    switch (runv(wfd, diffname, diffv)) {
			case DIFF_FAILURE: case DIFF_SUCCESS: break;
			default: rcsfaterror("diff failed");
		    }
#		    if !has_fflush_input && !(large_memory && maps_memory)
			if (lseek(wfd, wfd_off, SEEK_CUR) == -1)
			    Ierror();
#		    endif
#		    if large_memory && !maps_memory
			if (fseek(wfile, wfile_off, SEEK_SET) != 0)
			    Ierror();
#		    endif
		    if (newhead) {
			Irewind(workptr);
			putdftext(&newdelta, workptr, frewrite, false);
			if (!putdtext(targetdelta,diffname,frewrite,true))
			    return (cleanup());
		    } else
			if (!putdtext(&newdelta,diffname,frewrite,true))
			    return (cleanup());

		    /*
		    * Check whether the working file changed during checkin,
		    * to avoid producing an inconsistent RCS file.
		    */
		    if (
			fstat(wfd, &checkworkstat) != 0
		     ||	workstat.st_mtime != checkworkstat.st_mtime
		     ||	workstat.st_size != checkworkstat.st_size
		    ) {
			workerror("file changed during checkin");
			return (cleanup());
		    }

		    changedRCS = true;
                }
        }

	/* Deduce time_t of new revision if it is needed later.  */
	wtime = (time_t)-1;
	if (outOpts || Ttimeflag)
		wtime = date2time(workdelta->date);

	if (donerewrite(changedRCS,
		!Ttimeflag ? (time_t)-1
		: finptr && wtime < RCSstat.st_mtime ? RCSstat.st_mtime
		: wtime
	) != 0)
		return (cleanup());

        if (!keepworkingfile) {
		Izclose((RILE **)&(workptr));
		r = 0;
		/* r = un_link(workname); - appl removes: Get rid of old file */
        } else {
		int fd;		/* output file descriptor */
		FILE *exfile;	/* output (expansion, perhaps) file pointer */
		const char *newworkname;  /* output file name (temp name) */
		mode_t newworkmode;	  /* output file mode */

		newworkmode = WORKMODE(RCSstat.st_mode,
			!   (Expand==VAL_EXPAND  ||  lockthis < StrictLocks)
		);

		/* Create new temp file */
		if ((fd = rcsMakeTmpHdl(keepworkingfile, &newworkname,
					newworkmode)) < 0) {
		    nerror++;
		    return cleanup();
		}
	 	exfile = fdopen(fd, FOPEN_W_WORK);

		/* Expand if it might change */
		if (changework ) {
		    Irewind(workptr);
		    /* Expand keywords in file.  */
		    locker_expansion = lockthis;
		    workdelta->name = 
			namedrev(
				assoclst ? assoclst->ssymbol
				: keepflag && *prevname.string ? prevname.string
				: rev,
				workdelta
			);
		    switch (xpandfile(
			workptr, workdelta, dolog, exfile
		    )) {
			default:
			    Ozclose(&exfile);
			    rcsFreeTmpHdl(*keepworkingfile);
			    *keepworkingfile = NULL;
			    return (cleanup());

			case 0:
			    /*
			     * No expansion occurred, but output file still
			     * created (straight copy).  Use it.
			     */
			    /* fall into */
			case 1:
			    Izclose((RILE **)&workptr);
			    aflush(exfile);
		    }
		    
		}
		else {	/* just copy output to new temp file */
		    Irewind(workptr);
		    fastcopy(workptr, exfile);
		}
		if (outOpts) {	/* return time/mode of newly created delta */
		    outOpts->mode = newworkmode;
		    outOpts->mtime = wtime;
		}
		Ozclose(&exfile);
        }
	if (r < 0) {
	    eerror(dummyworkname);
	    return (cleanup());
	}
	diagnose("done\n");

	/* tempunlink(); */
	{
	    int rc = cleanup();
	    if (rc < 0) return(rc);
	}
	return r;
}       /* end of main (ci) */

	static int
cleanup()
{
	int exitstatus = 0;
	if (nerror) exitstatus = -1;
	/* Izclose(&finptr);	closed when Arch closed */
	Izclose(&workptr);	/* data for this change; change now complete */
	/* Ozclose(&exfile);	now localized; file hdl returned as output */
	/* Ozclose(&fcopy); 	closed when Arch closed */
	/* ORCSclose();		closed when Arch closed */
	/* dirtempunlink();	closed when Conn closed, or cd done */
	/* ffree(); 		per-file memory freed when Arch closed */
	rcsCloseArch();		/* common cleanup (closing Arch) */
	return exitstatus;
}

#if RCS_lint
#	define exiterr ciExit
#endif

#if 0
	void
exiterr()
{
	ORCSerror();
	dirtempunlink();
	tempunlink();
	_exit(EXIT_FAILURE);
}
#endif

/*****************************************************************/
/* the rest are auxiliary routines                               */


	static int
addelta(olddelnum)
	const char *olddelnum;
/* Function: Appends a delta to the delta tree, whose number is
 * given by newdelnum.  Updates Head, newdelnum, newdelnumlength,
 * and the links in newdelta.
 * Return -1 on error, 1 if a lock is removed, 0 otherwise.
 */
{
	register char *tp;
	register int i;
	int removedlock;
	int newdnumlength;  /* actual length of new rev. num. */
	int olddnumlength;  /* actual length of old rev. num. (-1 if unspec.) */

	newdnumlength = countnumflds(newdelnum.string);
	olddnumlength = (olddelnum && *olddelnum) ?
			countnumflds(olddelnum) : -2;

	if (olddnumlength % 2) {
	    rcserror("%s does not represent a specific revision", olddelnum);
	    return ERR_BAD_OLDREV;
	}

	if (rcsinitflag) {
                /* this covers non-existing RCS file and a file initialized with rcs -i */
		/* If olddelnum specified, create it (just as we may
		 * create a newdelnum delta out of whole cloth)
		 */
		if (olddnumlength > 2) {
		    rcserror("Branch point doesn't exist for revision %s.",
			olddelnum);
		    return -1;
		}
		if (olddnumlength == 1) {
			/* concatenate olddelnum and ".1" */
			char *tmp = ftnalloc(char, 4);
			*tmp = *olddelnum;
			memcpy(tmp+1, ".1", sizeof(".1"));
			olddelnum = tmp;
		}
		if (olddnumlength > 0) { 
		   Head = &newdelta;
		   newdelta.next = 0;
		   return 0; 		/* FUTURE: create two deltas */
		}

		if (newdnumlength==0 && Dbranch) {
			bufscpy(&newdelnum, Dbranch);
			newdnumlength = countnumflds(Dbranch);
		}
		if (newdnumlength==0) bufscpy(&newdelnum, "1.1");
		else if (newdnumlength==1) bufscat(&newdelnum, ".1");
		else if (newdnumlength>2) {
		    rcserror("Branch point doesn't exist for revision %s.",
			newdelnum.string
		    );
		    return -1;
                } /* newdnumlength == 2 is OK;  */
                Head = &newdelta;
		newdelta.next = 0;
		return 0;
        }
        if (newdnumlength==0 || newdnumlength < olddnumlength) {
		const char *oldnum;

		if (newdnumlength > 0) {
		    if (cmpnumfld(newdelnum.string, olddelnum,
				  newdnumlength) != 0) {
			rcserror("revision %s cannot follow revision %s",
				  newdelnum.string, olddelnum);
			return -1;
		    }
		}

		/* if old delta number provided, use it, assuming
		 * either it is unlocked or it is locked by the
		 * author, in which case it is unlocked.  If it
		 * is locked by another user, that lock is honored.
		 */
		if (olddnumlength > 0) {
		    switch (removedlock =
		      checklock(olddelnum, true, false, &targetdelta))
		    {
		    case 1: /* found an old lock */
		    case 0: /* no existing lock */
			i = 1;  /* treat as if we found the delta locked */
			oldnum = olddelnum;
			break;

		    default: /* delta was locked by someone else */
			rcserror(
			  "can't add delta to delta %s; lock not owned by %s",
			  olddelnum, getcaller());
			return ERR_BAD_LOCK;
		    }
		}

		else {
                    /* derive new revision number from locks */
		    removedlock = i = findlock(true, &targetdelta);
		}

		switch(i) {
		  default:
		    /* found two or more old locks */
		    return ERR_BAD_LOCK;

		  case 1:
                    /* found an old lock */
                    /* check whether locked revision exists */
		    oldnum = targetdelta->num;
		    if (!(targetdelta = 
			       genrevs(oldnum,(char*)0,(char*)0,
				       (char*)0,&gendeltas)))
			return ERR_BAD_LOCK;
                    if (targetdelta==Head) {
                        /* make new head */
                        newdelta.next=Head;
                        Head= &newdelta;
		    } else if (!targetdelta->next && countnumflds(targetdelta->num)>2) {
                        /* new tip revision on side branch */
                        targetdelta->next= &newdelta;
			newdelta.next = 0;
                    } else {
                        /* middle revision; start a new branch */
			bufscpy(&newdelnum, "");
			return addbranch(targetdelta, &newdelnum, 1);
                    }
		    incnum(targetdelta->num, &newdelnum);
		    return removedlock;

		  case 0:
                    /* no existing lock; try Dbranch */
                    /* update newdelnum */
		    if (StrictLocks || !myself(RCSstat.st_uid)) {
			rcserror("no lock set by %s", getcaller());
			return ERR_BAD_LOCK;
                    }
                    if (Dbranch) {
			bufscpy(&newdelnum, Dbranch);
                    } else {
			incnum(Head->num, &newdelnum);
                    }
		    newdnumlength = countnumflds(newdelnum.string);
                    /* now fall into next statement */
                }
        }
	if (olddnumlength > 0) {
		/* New rules for compatability:
		 *  olddnumlen must be even (i.e. specify a rev no., not a
		 *  branch); if you want to add to the end of the branch,
		 *  leave olddelnum empty, and set newdelnum to the branch.
		 *
		 *  if newdnumlen < olddelnumlen, the new rev no. must
		 *  be a substring of the old rev no., e.g. 1.2.1 is
		 *  part of 1.2.1.3.   This case is treated the same as
		 *  if the newdelta were not specified, and the old delta
		 *  had been locked.  That is, a new revision is created
		 *  after the old revision on the same branch, if the old
		 *  revision is at the end of that branch; else a new
		 *  branch is created.  See newdelnumlen == 0, supra.
		 *
		 *  If newdnumlen == olddnumlen:
		 *    if the rev no's are the same, then a branch is
		 *    made, even if the old delta is at the end of its
		 *    branch.
		 *    if the rev no's match except in the last digit,
		 *    e.g. 1.2.1.4 and 1.2.1.6, then the last digit of the
		 *    the new delta must be greater than the last digit of
		 *    the old delta, and the old delta must be at the end
		 *    of its branch.  The requested delta is then created.
	 	 *    if the rev no's don't match in all but their last
		 *    digits, it is an error.
		 *
		 *  If newdnumlen == olddnumlen+1 or olddnumlen+2:
		 *    There must not already exist a branch identified
		 *    by newdelnum (e.g. 1.3.2 is not allowed if any
		 *    delta numbered 1.3.2.x exists, even if 1.3.2.1 does
		 *    not exist; else we would allow insertion in the
		 *    middle of an existing branch).
		 *    
		 *    If newdnumlen is odd, the new rev. number used
		 *    is newdelnum.1.  The new revision is created as
		 *    a branch even if olddelnum is at the tip of its
		 *    branch.
		 *
		 *  If newdnumlen > olddnumlen+2:
		 *    This is an error; branches upon branches are not
		 *    automatically created.
		 */
		
		if (newdnumlength == olddnumlength) {
		    if (compartial(newdelnum.string, olddelnum,
				  newdnumlength-1)) {
		        rcserror("revision %s incompatible with %s",
			       newdelnum.string, olddelnum);
			return -1;
		    }	
		    switch(cmpnumfld(newdelnum.string, olddelnum,
				  newdnumlength)) {
		    case -1:
			rcserror("revision %s too low; must be higher than %s",
				newdelnum.string, olddelnum);
			return -1;
		      
		    case 0:  /* matching delta; force a new branch */
			if (!(targetdelta = genrevs(olddelnum,
                                              (char*)0,(char*)0,(char*)0,
                                              &gendeltas))) {
			    rcserror("old revision %s does not exist",
				     olddelnum);
			    return -1;
			}
			if (cmpnum(targetdelta->num, olddelnum)) {
			    rcserror("can't find branch point %s",
				     olddelnum);
			    return -1;
			}
			bufscpy(&newdelnum, "");
			return addbranch(targetdelta, &newdelnum, 0);

		    case 1:  /* new delta is at higher number;
			      * handle as a regular delta (but check that
			      * olddelta is at tip of branch or head of tree)
			      */
			break;
		    }
		}
		if (newdnumlength > olddnumlength) {
		    if (newdnumlength == olddnumlength + 1) {
		      bufscat(&newdelnum, ".1");
		      newdnumlength++;
		    }
		    else if (newdnumlength > olddnumlength + 2) {
		      rcserror("cannot create branch %s at %s",
			       newdelnum.string, olddelnum);
		      return -1;
		    }
		    /* newdelta is a valid branch from olddelta (len > 2) */
		}
	}

        if (newdnumlength<=2) {
                /* add new head per given number */
                if(newdnumlength==1) {
                    /* make a two-field number out of it*/
		    if (cmpnumfld(newdelnum.string,Head->num,1)==0)
			incnum(Head->num, &newdelnum);
		    else
			bufscat(&newdelnum, ".1");
                }
		if (cmpnum(newdelnum.string,Head->num) <= 0) {
		    rcserror("revision %s too low; must be higher than %s",
			  newdelnum.string, Head->num
		    );
		    return -1;
                }
		if (olddnumlength > 0 && (i = cmpnum(olddelnum, Head->num)) ) {
		    if (i < 0) {
		        rcserror("old revision %s too low; must match head %s",
				 olddelnum, Head->num);
		    }
		    if (i > 0) {
		        rcserror("old revision %s does not exist", olddelnum);
		    }
		    return -1;
		}
		targetdelta = Head;
		if (0 <=
		  (removedlock = removelock(Head, olddnumlength <= 0))) {
		    if (!genrevs(Head->num,(char*)0,(char*)0,(char*)0,&gendeltas))
			return -1;
		    newdelta.next = Head;
		    Head = &newdelta;
		}
		return removedlock;
        } else {
                /* put new revision on side branch */
                /*first, get branch point */
	        if (olddnumlength > 0) {
		  if (!(targetdelta = genrevs(olddelnum,(char*)0,
					     (char*)0,(char*)0,&gendeltas)) ||
		          cmpnum(targetdelta->num, olddelnum)) {
		      rcserror("can't find branch point %s", olddelnum);
		      return -1;
		  }
	        }    
		else {
		  tp = newdelnum.string;
		  for (i = newdnumlength - ((newdnumlength&1) ^ 1);  --i;  )
			while (*tp++ != '.')
				continue;
		  *--tp = 0; /* Kill final dot to get old delta temporarily. */
		  if (!(targetdelta=genrevs(newdelnum.string,(char*)0,(char*)0,(char*)0,&gendeltas)))
		    return -1;
		    if (cmpnum(targetdelta->num, newdelnum.string) != 0) {
		        rcserror("can't find branch point %s", newdelnum.string);
		        return -1;
                    }
		    *tp = '.'; /* Restore final dot. */
		}
		return addbranch(targetdelta, &newdelnum, 0);
        }
}



	static int
addbranch(branchpoint, num, removedlock)
	struct hshentry *branchpoint;
	struct buf *num;
	int removedlock;
/* adds a new branch and branch delta at branchpoint.
 * If num is the null string, appends the new branch, incrementing
 * the highest branch number (initially 1), and setting the level number to 1.
 * the new delta and branchhead are in globals newdelta and newbranch, resp.
 * the new number is placed into num.
 * Return -1 on error, 1 if a lock is removed, 0 otherwise.
 * If REMOVEDLOCK is 1, a lock was already removed.
 */
{
	struct branchhead *bhead, **btrail;
	struct buf branchnum;
	int result;
	int field, numlength;
	static struct branchhead newbranch;  /* new branch to be inserted */

	numlength = countnumflds(num->string);

	if (!branchpoint->branches) {
                /* start first branch */
                branchpoint->branches = &newbranch;
                if (numlength==0) {
			bufscpy(num, branchpoint->num);
			bufscat(num, ".1.1");
		} else if (numlength&1)
			bufscat(num, ".1");
		newbranch.nextbranch = 0;

	} else if (numlength==0) {
                /* append new branch to the end */
                bhead=branchpoint->branches;
                while (bhead->nextbranch) bhead=bhead->nextbranch;
                bhead->nextbranch = &newbranch;
		bufautobegin(&branchnum);
		getbranchno(bhead->hsh->num, &branchnum);
		incnum(branchnum.string, num);
		bufautoend(&branchnum);
		bufscat(num, ".1");
		newbranch.nextbranch = 0;
        } else {
                /* place the branch properly */
		field = numlength - ((numlength&1) ^ 1);
                /* field of branch number */
		btrail = &branchpoint->branches;
		while (0 < (result=cmpnumfld(num->string,(*btrail)->hsh->num,field))) {
			btrail = &(*btrail)->nextbranch;
			if (!*btrail) {
				result = -1;
				break;
			}
                }
		if (result < 0) {
                        /* insert/append new branchhead */
			newbranch.nextbranch = *btrail;
			*btrail = &newbranch;
			if (numlength&1) bufscat(num, ".1");
                } else {
                        /* branch exists; append to end */
			bufautobegin(&branchnum);
			getbranchno(num->string, &branchnum);
			targetdelta = genrevs(
				branchnum.string, (char*)0, (char*)0, (char*)0,
				&gendeltas
			);
			bufautoend(&branchnum);
			if (!targetdelta)
			    return -1;
			if (cmpnum(num->string,targetdelta->num) <= 0) {
				rcserror("revision %s too low; must be higher than %s",
				      num->string, targetdelta->num
				);
				return -1;
                        }
			if (!removedlock
			    && 0 <= (removedlock = removelock(targetdelta,true))
			) {
			    if (numlength&1)
				incnum(targetdelta->num,num);
			    targetdelta->next = &newdelta;
			    newdelta.next = 0;
			}
			return removedlock;
			/* Don't do anything to newbranch.  */
                }
        }
        newbranch.hsh = &newdelta;
	newdelta.next = 0;
	if (branchpoint->lockedby)
	    if (strcmp(branchpoint->lockedby, getcaller()) == 0)
		return removelock(branchpoint,true); /* This returns 1.  */
	return removedlock;
}

	static int
addsyms(num)
	char const *num;
{
	register struct Symrev *p;

	for (p = assoclst;  p;  p = p->nextsym)
		if (addsymbol(num, p->ssymbol, p->override)  <  0)
			return false;
	return true;
}


	static void
incnum(onum,nnum)
	char const *onum;
	struct buf *nnum;
/* Increment the last field of revision number onum by one and
 * place the result into nnum.
 */
{
	register char *tp, *np;
	register size_t l;

	l = strlen(onum);
	bufalloc(nnum, l+2);
	np = tp = nnum->string;
	VOID strcpy(np, onum);
	for (tp = np + l;  np != tp;  )
		if (isdigit(*--tp)) {
			if (*tp != '9') {
				++*tp;
				return;
			}
			*tp = '0';
		} else {
			tp++;
			break;
		}
	/* We changed 999 to 000; now change it to 1000.  */
	*tp = '1';
	tp = np + l;
	*tp++ = '0';
	*tp = 0;
}



	static int
removelock(delta, warn)
struct hshentry * delta;
int warn;
/* function: Finds the lock held by caller on delta,
 * removes it, and returns nonzero if successful.
 * Print an error message and return -1 if there is no such lock,
 * and warn set.
 * An exception is if !StrictLocks, and caller is the owner of
 * the RCS file. If caller does not have a lock in this case,
 * return 0; return 1 if a lock is actually removed.
 */
{
	register struct rcslock *next, **trail;
	char const *num;

        num=delta->num;
	for (trail = &Locks;  (next = *trail);  trail = &next->nextlock)
	    if (next->delta == delta)
		if (strcmp(getcaller(), next->login) == 0) {
		    /* We found a lock on delta by caller; delete it.  */
		    *trail = next->nextlock;
		    delta->lockedby = 0;
		    return 1;
		} else {
		    rcserror("revision %s locked by %s", num, next->login);
		    return -1;
                }
	if ((!StrictLocks && myself(RCSstat.st_uid)) || !warn)
	    return 0;
	rcserror("no lock set by %s for revision %s", getcaller(), num);
	return -1;
}



	static char const *
getcurdate()
/* Return a pointer to the current date.  */
{
	static char buffer[datesize]; /* date buffer */

	if (!buffer[0])
		time2date(now(), buffer);
        return buffer;
}

	static int
xpandfile(unexfile, delta, dolog, exfile)
	RILE *unexfile;
	struct hshentry const *delta;
	int dolog;
	FILE *exfile;
/*
 * Read unexfile and copy it to exfile,
 * performing keyword substitution with data from delta.
 * Return -1 if unsuccessful, 1 if expansion occurred, 0 otherwise.
 */
{
	int e, r;
	r = e = 0;
	if (MIN_UNEXPAND <= Expand)
		fastcopy(unexfile,exfile);
	else {
		for (;;) {
			e = expandline(
				unexfile, exfile, delta, false,
				(FILE*)0, dolog
			);
			if (e < 0)
				break;
			r |= e;
			if (e <= 1)
				break;
		}
	}
	return r & 1;
}




/* --------------------- G E T L O G M S G --------------------------------*/


	static struct cbuf
getlogmsg(keepflag)
	int keepflag;
/* Obtain and yield a log message.
 * If a log message is given with -m, yield that message.
 * If this is the initial revision, yield a standard log message.
 * Otherwise, reads a character string from the terminal.
 * Stops after reading EOF or a single '.' on a
 * line. getlogmsg prompts the first time it is called for the
 * log message; during all later calls it asks whether the previous
 * log message can be reused.
 */
{
	static char const
		emptych[] = EMPTYLOG,
		initialch[] = "Initial revision";
	static struct cbuf const
		emptylog = { emptych, sizeof(emptych)-sizeof(char) },
		initiallog = { initialch, sizeof(initialch)-sizeof(char) };
	static struct buf logbuf;
	static struct cbuf logmsg;

	register char *tp;
	register size_t i;
	char const *caller;

	if (msg.size) return msg;

	if (keepflag) {
		/* generate std. log message */
		caller = getcaller();
		i = sizeof(ciklog)+strlen(caller)+3;
		bufalloc(&logbuf, i + datesize + zonelenmax);
		tp = logbuf.string;
		VOID sprintf(tp, "%s%s at ", ciklog, caller);
		VOID date2str(getcurdate(), tp+i);
		logmsg.string = tp;
		logmsg.size = strlen(tp);
		return logmsg;
	}

	if (!targetdelta && (
		cmpnum(newdelnum.string,"1.1")==0 ||
		cmpnum(newdelnum.string,"1.0")==0
	))
		return initiallog;

	if (logmsg.size) {
                /*previous log available*/
	    if (yesorno(true, "reuse log message of previous file? [yn](y): "))
		return logmsg;
        }

        /* now read string from stdin */
	rcsNoStdin = true;
	logmsg = getsstdin("m", "log message", "", &logbuf);
	rcsNoStdin = false;

        /* now check whether the log message is not empty */
	if (logmsg.size)
		return logmsg;
	return emptylog;
}

/*  Make a linked list of Symbolic names  */

        static void
addassoclst(flag, sp)
	int flag;
	char const *sp;
{
        struct Symrev *pt;
	char *name;
	size_t len;
	
	len = strlen(sp);
	if (!(pt = rcsHeapAlloc(&optsHeap, sizeof(struct Symrev))) ||
	    !(name = rcsHeapAlloc(&optsHeap, ++len)))
		faterror("Out of Memory");

	memcpy(name, sp, len);
	pt->ssymbol = name;
	pt->override = flag;
	pt->nextsym = 0;
	*nextassoc = pt;
	nextassoc = &pt->nextsym;
}

#ifdef MAIN
/* test program for pairnames() and getfullRCSname() */

main(argc, argv)
int argc; char *argv[];
{
        int result;
	int initflag;
	int keepflag;
	struct buf olddelnum;
	RILE *workfileptr;
	struct stat workstat;
	const char *rev;
	quietflag = initflag = keepflag = false;

	olddelnum.size = 0;
	bufalloc(&olddelnum, 1);
	olddelnum.string[0] = '\0';

        while(--argc, ++argv, argc>=1 && ((*argv)[0] == '-')) {
                switch ((*argv)[1]) {

		case 'p':       workstdout = stdout;
                                break;
                case 'i':       initflag=true;
                                break;
                case 'q':       quietflag=true;
                                break;
                case 'k':       keepflag=true;
                                break;
		case 'r':	if (argc <= 1) continue;
				rev = *++argv;
				break;
		case 'o':	if (argc <= 1) continue;
				bufscat(&olddelnum, *++argv);
				break;
                default:        error("unknown option: %s", *argv);
                                break;
                }
        }

	setstate(quietflag, true /* interactive */, "msf", X_DEFAULT, "ci");
        do {
		RCSname = workname = 0;
		result = pairnames(argc,argv,rcswriteopen,false,quietflag);
                if (result!=0) {
		    diagnose("RCS pathname: %s; working pathname: %s\nFull RCS pathname: %s\n",
			     RCSname, workname, getfullRCSname()
		    );
		    workfileptr = Iopen(workname, FOPEN_R_WORK, &workstat);
                }
                switch (result) {
                        case 0: continue; /* already paired file */

                        case 1: 
				diagnose("RCS file %s exists\n", RCSname);
                                break;

			case -1:diagnose("RCS file doesn't exist\n");
                                break;
                }
		delta(&olddelnum, rev, workfileptr, &workstat, initflag, keepflag);

        } while (++argv, --argc>=1);

}
#endif /* MAIN */
