/* Check out working files from revisions of RCS files.  */

/* Copyright 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
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
 * $Log: rcschko.c,v $
 * Revision 1.1  1996/06/28 23:09:54  msf
 * Initial revision
 *
 */



#include "rcspriv.h"
#include "rcsbase.h"

	int
rcsReadOutData(writeMethod, outStream, conn, hdl)
	int (*writeMethod)(RCSSTREAM *, const void *, size_t);
	RCSSTREAM *outStream;
	RCSCONN conn;
	RCSTMP hdl;
{

	/* Write the contents of the file indicated by hdl to
	 * the caller-provided stream.
	 *
	 * Returns: 0 (success)
	 *	    negative error codes on failure
	 */

	/* No need to use declarecache, setupcache, cache; fast2stream
	 * manipulates the RILE struct directly
	 */
	RILE *tmpptr;		/* temp file pointer */
	const char *tmpname;	/* temp file name */
	struct stat tmpstat;	/* temp file stat buffer */
	int rc;

	/* In case of internal core code error, return fatal error
	 * to caller, rather than exiting.
	 */
	if ((rc = sigsetjmp(rcsApiEnv, 1)) != 0) return (rc);

	if (rcsMapTmpHdlToName(hdl, &tmpname) < 0)
	    return ERR_APPL_ARGS;

	if (!(tmpptr = Iopen(tmpname, FOPEN_R_WORK, &tmpstat))) {
	    eerror(tmpname);
	    if ((int)hdl < TEMPNAMES) return ERR_INTERNAL;
	    else return ERR_NO_FILE;
	}

	/* Read file, copy data to stream */
	rc = fast2stream(writeMethod, outStream, tmpptr);

	Ifclose(tmpptr);
	return rc;
}
	
static char *addjoin P((char*));
static char const *getancestor P((char const*,char const*));
static int buildjoin P((char const*));
static int preparejoin P((char*));
static int rmlock P((struct hshentry const*));
#ifdef REMOVE
static int rmworkfile P((void));
#endif /* REMOVE */
static void cleanup P((void));

/* Memory heap managed locally for options that persist across calls */
static struct rcsHeap optsHeap = {NULL, NULL, NULL};

static char const quietarg[] = "-q";

static char const *expandarg, *suffixarg, *versionarg, *zonearg;
static char const **joinlist; /* revisions to be joined (reused memory) */
static int joinlength;	      /* number of entries available in joinlist */
static FILE *neworkptr;		/* output file (new work file) */
static int exitstatus;
static int lastjoin;			/* last element set in joinlist  */
static struct hshentries *gendeltas;	/* deltas to be generated	*/
static struct hshentry *targetdelta;	/* final delta to be generated	*/
static int lockflag; 		/* -1 -> unlock, 0 -> do nothing, 1 -> lock */

int rcsCheckOut(conn, rcspath, hdl, attrib, opts)
	RCSCONN conn;		/* connection */
	const char *rcspath;	/* full pathname to RCS archive file */
	RCSTMP *hdl;		/* returned handle to tmp file */
	struct rcsStreamOpts *attrib;  /* returned attributes - may be NULL */
	struct rcsCheckOutOpts *opts;
{

	/* The static variables (lockflag, joinflag, date, rev, state, expmode)
	 * represent the options that get reused (if opts is NULL).
	 * lockflag is static to the file, because co -l forces cleanup
	 * to close the archive (to remove the ,file, lock).
	 */
	static char *joinflag;
	static char const *date, *rev, *state;
	static int expmode;

	char const *joinname, *newdate;
	int changelock;  /* 1 if a lock has been changed, -1 if error */

	/* numericrev is static, because its memory gets reused
	 * (possibly grown), rather than reclaimed
	 *
	 * finaldate is what date points to, so it must be static, too
	 */
	static struct buf numericrev;	/* expanded revision number	*/
	static char finaldate[datesize];

	int fd;
	bool isopen;
	int rc;

	static bool has_been_called = false;	/* for first time init */

	/* In case of internal core code error, return fatal error
	 * to caller, rather than exiting.
	 */
	if ((rc = sigsetjmp(rcsApiEnv, 1)) != 0) return (rc);

	/* If running as server, no effect; else preserve RCS semantics */
	setrid();

	if (!has_been_called || opts) {  /* set defaults */

	    /* If we've saved options (i.e. not first time), free old ones.
	     * If it is the first time, init numericrev (buf gets reused later)
	     */
	    if (has_been_called) rcsHeapFree(&optsHeap);
	    else bufautobegin(&numericrev);

	    state = 0;
	    rev = 0; 
	    date = 0;
	    lockflag = 0;
	    joinflag = 0;
	    expmode = -1;
	    has_been_called = true;
	}

	/* Initialize error state (mostly provided by lex analyzer) */
	nerror = 0;
	exitstatus = 0;

	/* Use opts to override defaults */
	/* -r: rev = opts->rev 
	 * -f: no forceflag (application controls overwrite of local workfile)
	 * -l: lockflag = 1
	 * -u: lockflag = -1 (cannot have both -u and -l; -l takes precidence)
	 * -p: tostdout removed (application fetches results)
	 * -I: removed (not interactive)
	 * -q: removed (application provides its own diagnostics)
	 * -d: date is set to parsed user date string
	 * -j: joinflag = opts->joinflag
	 * -M: removed (application changes time on working file)
	 * -s: state = opts->stateflag
	 * -T: Ttimeflag removed (set in connection - set RCS mod time)
	 * -w: author removed (set in connection)
	 * -x: suffixes removed (set in connection)
	 * -V: set RCSversion - must be done globally (did call setRCSversion)
	 * -z: call zone_set - must be done globally; ALSO
	 *		       set zonearg for buildjoin (to invoke co)
	 * -k: set keyword expansion mode
	 */

	if (opts) {
	    if (opts->rev && *opts->rev) rev = hstr_save(&optsHeap, opts->rev);

	    if (opts->lockflag) lockflag = 1;
	    else if (opts->unlockflag) lockflag = -1;

	    if (opts->dateflag) {
		str2date(opts->dateflag, finaldate);
		date = finaldate;
	    }
	    if (opts->joinflag && *opts->joinflag)
		joinflag = hstr_save(&optsHeap, opts->joinflag);

	    if (opts->stateflag && *opts->stateflag)
		state = hstr_save(&optsHeap, opts->stateflag);

	    if (opts->keyexpand && *opts->keyexpand) {
		if ((expmode = str2expmode(opts->keyexpand)) < 0) {
		    error("unknown option %s", opts->keyexpand);
		    return ERR_APPL_ARGS;
		}
	    }
        } /* end of option processing */


	isopen = !strcmp(rcsParamName, rcspath);	/* archive is opened */

	/* Ensure that we have a valid copy of the archive file, opened.
	 * If the file is already opened, but we need it locked, reopen it.
	 * Else:
	 *    If the file is already opened and changed, close it.
	 *		(file is now closed).
	 *    If the file is already opened and unchanged, rewind.
	 *    If the file is closed, open it.
	 *    Parse the beginning of the file.
	 */
	if (isopen && lockflag) {if ((rc = rcsreopen(true)) < 0) return rc;}
	else {
	    if (isopen) {
		struct stat sbuf;
		if (stat(RCSname, &sbuf) < 0 || !same_file(sbuf, RCSstat, 0)
		    			|| !same_attr(sbuf, RCSstat, 0)) {
		    Izclose(&finptr);
		    isopen = false;
		}
	    }
	    else if (rcsParamName[0]) rcsCloseArch(); /* close different arch */

	    if (!isopen) {
	        if ((rc = rcsOpenArch(lockflag ? true : false, true, rcspath))
			< 0)  return rc;
	    }
	}
	
	/*
	 * RCSname contains the name of the RCS file, and finptr
	 * points at it.
	 * Also, RCSstat has been set.
         */

        gettree();  /* reads in the delta tree */

	if (!Head) {
		/* Create temp file output */
		if ((fd = rcsMakeTmpHdl(hdl, &workname, 
			attrib->mode = WORKMODE(RCSstat.st_mode, lockflag>0)))
			< 0) {
		    cleanup();
		    return fd;
		}
		neworkptr = fdopen(fd, FOPEN_W_WORK);

		diagnose("%s  -->  %s\n", RCSname, workname);

                /* no revisions; create empty file */
		diagnose("no revisions present; generating empty revision 0.0\n");
		if (lockflag)
			warn(
				"no revisions, so nothing can be %slocked",
				lockflag < 0 ? "un" : ""
			);
		Ozclose(&fcopy);
		changelock = 0;
		newdate = 0;
        } else {
		int locks = lockflag ? findlock(false, &targetdelta) : 0;
		if (rev) {
                        /* expand symbolic revision number */
			if (!expandsym(rev, &numericrev)) {
				cleanup();
				return ERR_BAD_OLDREV;
			}
                                
		} else {
			switch (locks) {
			    default:
				cleanup();
				return ERR_BAD_LOCK;
			    case 0:
				bufscpy(&numericrev, Dbranch?Dbranch:"");
				break;
			    case 1:
				bufscpy(&numericrev, targetdelta->num);
				break;
			}
		}
                /* get numbers of deltas to be generated */
		if (!(targetdelta=genrevs(numericrev.string,date,author,state,&gendeltas))) {
                        cleanup();
			return ERR_BAD_OLDREV;
		}

                /* check reservations */
		changelock =
			lockflag < 0 ?
				rmlock(targetdelta)
			: lockflag == 0 ?
				0
			:
				addlock(targetdelta, true);
		if (changelock < 0) {
		    cleanup();
		    return ERR_REV_LOCKED;
		}
		if (changelock > 0 && !checkaccesslist()) {
		    cleanup();
		    return ERR_NO_ACCESS;
		}
		if (dorewrite(lockflag, changelock) != 0) {
		    cleanup();
		    return ERR_SYSTEM;
		}

		if (0 <= expmode)
			Expand = expmode;
		if (0 < lockflag  &&  Expand == VAL_EXPAND) {
			rcserror("cannot combine -kv and -l");
			cleanup();
			return ERR_APPL_ARGS;
		}

		if (joinflag && !preparejoin(joinflag)) {
			cleanup();
			return ERR_BAD_JOINREV;
		}

		/* Create temp file output */
		if ((fd = rcsMakeTmpHdl(hdl, &workname, 
		             attrib->mode = WORKMODE(RCSstat.st_mode,
		 		 	!(Expand==VAL_EXPAND ||
					   (lockflag<=0 && StrictLocks)))
			  )
		     ) < 0
	           )
		{
		    cleanup();
		    return fd;
		}
		neworkptr = fdopen(fd, FOPEN_W_WORK);
		diagnose("%s  -->  %s\n", RCSname, workname);

		diagnose("revision %s%s\n",targetdelta->num,
			 0<lockflag ? " (locked)" :
			 lockflag<0 ? " (unlocked)" : "");

                /* skip description */
                getdesc(false); /* don't echo*/

		locker_expansion = 0 < lockflag;
		targetdelta->name = namedrev(rev, targetdelta);
		joinname = buildrevision(
			gendeltas, targetdelta,
			neworkptr,
			Expand < MIN_UNEXPAND
		);
		/* fcopy is always neworkptr (set via buildrevision) */
		fcopy = 0;  /* Don't close it twice.  */

		if_advise_access(changelock && gendeltas->first!=targetdelta,
			finptr, MADV_SEQUENTIAL
		);

		if (donerewrite(changelock,
			Ttimeflag ? RCSstat.st_mtime : (time_t)-1
		) != 0) {
			cleanup();
			return ERR_SYSTEM;
		}


		if (changelock) {
			locks += lockflag;
			if (1 < locks)
				rcswarn("You now have %d locks.", locks);
		}

		newdate = targetdelta->date;
		if (joinflag) {
			newdate = 0;
			if (!joinname) {
				aflush(neworkptr);
				joinname = workname;
			}
			if (Expand == BINARY_EXPAND)
				workerror("merging binary files");
			if (!buildjoin(joinname)) {
				cleanup();
				return ERR_SYSTEM;
			}
		}
        }
	aflush(neworkptr);

	/* No need to use chnamemod() to move neworkname to workfile */

	attrib->mtime = newdate ? date2time(newdate) : 0;
	diagnose("done\n");

	/* tempunlink(); */
	cleanup();
	return(exitstatus);

}       /* end of main (co) */

	static void
cleanup()
{
	if (nerror) exitstatus = EXIT_FAILURE;
	/* Izclose(&finptr);	closed when Arch closed */
	/* ORCSclose();		closed when Arch closed */
#ifdef REMOVE /* workstdout always null */
#	if !large_memory
		if (fcopy!=workstdout) Ozclose(&fcopy);
#	endif
#endif /* REMOVE */
	/* if (neworkptr!=workstdout) */  Ozclose(&neworkptr);

	/* Close the archive if we opened the file for update, creating
	 * a ,file, lockfile.  Closing the archive removes the lock.
	 * If we could handle files already opened for update, we would
	 * not need this call; the already-read file could be reused.
	 */
	if (lockflag) rcsCloseArch();
	/* dirtempunlink();	closed when archive closed */
}

#if RCS_lint
#	define exiterr coExit
#endif

#ifdef REMOVE
	void
exiterr()
{
	ORCSerror();
	dirtempunlink();
	tempunlink();
	_exit(EXIT_FAILURE);
}
#endif /* REMOVE */


/*****************************************************************
 * The following routines are auxiliary routines
 *****************************************************************/
#ifdef REMOVE
	static int
rmworkfile()
/*
 * Prepare to remove workname, if it exists, and if
 * it is read-only.
 * Otherwise (file writable):
 *   if !quietmode asks the user whether to really delete it (default: fail);
 *   otherwise failure.
 * Returns true if permission is gotten.
 */
{
	if (workstat.st_mode&(S_IWUSR|S_IWGRP|S_IWOTH) && !forceflag) {
	    /* File is writable */
	    if (!yesorno(false, "writable %s exists%s; remove it? [ny](n): ",
			workname,
			myself(workstat.st_uid) ? "" : ", and you do not own it"
	    )) {
		error(!quietflag && ttystdin()
			? "checkout aborted"
			: "writable %s exists; checkout aborted", workname);
		return false;
            }
        }
	/* Actual unlink is done later by caller. */
	return true;
}
#endif /* REMOVE */

	static int
rmlock(delta)
	struct hshentry const *delta;
/* Function: removes the lock held by caller on delta.
 * Returns -1 if someone else holds the lock,
 * 0 if there is no lock on delta,
 * and 1 if a lock was found and removed.
 */
{       register struct rcslock * next, * trail;
	char const *num;
	struct rcslock dummy;
        int whomatch, nummatch;

        num=delta->num;
        dummy.nextlock=next=Locks;
        trail = &dummy;
	while (next) {
		whomatch = strcmp(getcaller(), next->login);
                nummatch=strcmp(num,next->delta->num);
                if ((whomatch==0) && (nummatch==0)) break;
			/*found a lock on delta by caller*/
                if ((whomatch!=0)&&(nummatch==0)) {
                    rcserror("revision %s locked by %s; use co -r or rcs -u",
			num, next->login
		    );
                    return -1;
                }
                trail=next;
                next=next->nextlock;
        }
	if (next) {
                /*found one; delete it */
                trail->nextlock=next->nextlock;
                Locks=dummy.nextlock;
		next->delta->lockedby = 0;
                return 1; /*success*/
        } else  return 0; /*no lock on delta*/
}




/*****************************************************************
 * The rest of the routines are for handling joins
 *****************************************************************/


	static char *
addjoin(joinrev)
	char *joinrev;
/* Add joinrev's number to joinlist, yielding address of char past joinrev,
 * or 0 if no such revision exists.
 */
{
	register char *j;
	register struct hshentry *d;
	char terminator;
	struct buf numrev;
	struct hshentries *joindeltas;

	j = joinrev;
	for (;;) {
	    switch (*j++) {
		default:
		    continue;
		case 0:
		case ' ': case '\t': case '\n':
		case ':': case ',': case ';':
		    break;
	    }
	    break;
	}
	terminator = *--j;
	*j = 0;
	bufautobegin(&numrev);
	d = 0;
	if (expandsym(joinrev, &numrev))
	    d = genrevs(numrev.string,(char*)0,(char*)0,(char*)0,&joindeltas);
	bufautoend(&numrev);
	*j = terminator;
	if (d) {
		joinlist[++lastjoin] = d->num;
		return j;
	}
	return 0;
}

	static int
preparejoin(j)
	register char *j;
/* Parse join list J and place pointers to the
 * revision numbers into joinlist.
 */
{
        lastjoin= -1;
        for (;;) {
                while ((*j==' ')||(*j=='\t')||(*j==',')) j++;
                if (*j=='\0') break;
                if (lastjoin>=joinlength-2) {
		    joinlist =
			(joinlength *= 2) == 0
			? tnalloc(char const *, joinlength = 16)
			: trealloc(char const *, joinlist, joinlength);
                }
		if (!(j = addjoin(j))) return false;
                while ((*j==' ') || (*j=='\t')) j++;
                if (*j == ':') {
                        j++;
                        while((*j==' ') || (*j=='\t')) j++;
                        if (*j!='\0') {
				if (!(j = addjoin(j))) return false;
                        } else {
				rcsfaterror("join pair incomplete");
                        }
                } else {
                        if (lastjoin==0) { /* first pair */
                                /* common ancestor missing */
                                joinlist[1]=joinlist[0];
                                lastjoin=1;
                                /*derive common ancestor*/
				if (!(joinlist[0] = getancestor(targetdelta->num,joinlist[1])))
                                       return false;
                        } else {
				rcsfaterror("join pair incomplete");
                        }
                }
        }
	if (lastjoin < 1)
		rcsfaterror("empty join");
	return true;
}



	static char const *
getancestor(r1, r2)
	char const *r1, *r2;
/* Yield the common ancestor of r1 and r2 if successful, 0 otherwise.
 * Work reliably only if r1 and r2 are not branch numbers.
 */
{
	static struct buf t1, t2;

	int l1, l2, l3;
	char const *r;

	l1 = countnumflds(r1);
	l2 = countnumflds(r2);
	if ((2<l1 || 2<l2)  &&  cmpnum(r1,r2)!=0) {
	    /* not on main trunk or identical */
	    l3 = 0;
	    while (cmpnumfld(r1, r2, l3+1)==0 && cmpnumfld(r1, r2, l3+2)==0)
		l3 += 2;
	    /* This will terminate since r1 and r2 are not the same; see above. */
	    if (l3==0) {
		/* no common prefix; common ancestor on main trunk */
		VOID partialno(&t1, r1, l1>2 ? 2 : l1);
		VOID partialno(&t2, r2, l2>2 ? 2 : l2);
		r = cmpnum(t1.string,t2.string)<0 ? t1.string : t2.string;
		if (cmpnum(r,r1)!=0 && cmpnum(r,r2)!=0)
			return r;
	    } else if (cmpnumfld(r1, r2, l3+1)!=0)
			return partialno(&t1,r1,l3);
	}
	rcserror("common ancestor of %s and %s undefined", r1, r2);
	return 0;
}



	static int
buildjoin(initialfile)
	char const *initialfile;
/* Function: merge pairs of elements in joinlist into initialfile
 * If workstdout is set, copy result to stdout.
 * All unlinking of initialfile, rev2, and rev3 should be done by tempunlink().
 */
{
	struct buf commarg;
	struct buf subs;
	char const *rev2, *rev3;
        int i;
	char const *cov[10], *mergev[11];
	char const **p;

	bufautobegin(&commarg);
	bufautobegin(&subs);
	rev2 = maketemp(0);
	rev3 = maketemp(3); /* buildrevision() may use 1 and 2 */

	cov[1] = CO;
	/* cov[2] setup below */
	p = &cov[3];
	if (expandarg) *p++ = expandarg;
	if (suffixarg) *p++ = suffixarg;
	if (versionarg) *p++ = versionarg;
	if (zonearg) *p++ = zonearg;
	*p++ = quietarg;
	*p++ = RCSname;
	*p = 0;

	mergev[1] = MERGE;
	mergev[2] = mergev[4] = "-L";
	/* rest of mergev setup below */

        i=0;
        while (i<lastjoin) {
                /*prepare marker for merge*/
                if (i==0)
			bufscpy(&subs, targetdelta->num);
		else {
			bufscat(&subs, ",");
			bufscat(&subs, joinlist[i-2]);
			bufscat(&subs, ":");
			bufscat(&subs, joinlist[i-1]);
		}
		diagnose("revision %s\n",joinlist[i]);
		bufscpy(&commarg, "-p");
		bufscat(&commarg, joinlist[i]);
		cov[2] = commarg.string;
		if (runv(-1, rev2, cov))
			goto badmerge;
		diagnose("revision %s\n",joinlist[i+1]);
		bufscpy(&commarg, "-p");
		bufscat(&commarg, joinlist[i+1]);
		cov[2] = commarg.string;
		if (runv(-1, rev3, cov))
			goto badmerge;
		diagnose("merging...\n");
		mergev[3] = subs.string;
		mergev[5] = joinlist[i+1];
		p = &mergev[6];
		if (quietflag) *p++ = quietarg;
#ifdef REMOVE
		if (lastjoin<=i+2 && workstdout) *p++ = "-p";
#endif /* REMOVE */
		*p++ = initialfile;
		*p++ = rev2;
		*p++ = rev3;
		*p = 0;
		switch (runv(-1, (char*)0, mergev)) {
		    case DIFF_FAILURE: case DIFF_SUCCESS:
			break;
		    default:
			goto badmerge;
		}
                i=i+2;
        }
	bufautoend(&commarg);
	bufautoend(&subs);
        return true;

    badmerge:
	nerror++;
	bufautoend(&commarg);
	bufautoend(&subs);
	return false;
}
