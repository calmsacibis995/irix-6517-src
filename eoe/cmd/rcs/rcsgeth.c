/* Print log messages and other information about RCS files.  */

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
 * $Log: rcsgeth.c,v $
 * Revision 1.1  1996/06/29 00:01:13  msf
 * Initial revision
 *
 */



#include "rcsbase.h"
#include "rcspriv.h"

struct rcslockers {                   /* lockers in locker option; stored   */
     char const		* login;      /* lockerlist			    */
     struct rcslockers  * lockerlink;
     }  ;

struct  stateattri {                  /* states in state option; stored in  */
     char const		* status;     /* statelist			    */
     struct  stateattri * nextstate;
     }  ;

struct  authors {                     /* login names in author option;      */
     char const		* login;      /* stored in authorlist		    */
     struct     authors * nextauthor;
     }  ;

struct Revpairs{                      /* revision or branch range in -r     */
     int		  numfld;     /* option; stored in revlist	    */
     char const		* strtrev;
     char const		* endrev;
     struct  Revpairs   * rnext;
     } ;

struct Datepairs{                     /* date range in -d option; stored in */
     struct Datepairs *dnext;
     char               strtdate[datesize];   /* duelst and datelist      */
     char               enddate[datesize];
     char ne_date; /* datelist only; distinguishes < from <= */
     };

static char extractdelta P((struct hshentry const*));
static int checkrevpair P((char const*,char const*));
static int extdate P((struct hshentry*));
static int getnumericrev P((void));
static struct hshentry const *readdeltalog P((void));
static void exttree P((struct hshentry*));
static void getauthor P((const char*));
static void getdatepair P((const char*));
static void getlocker P((const char*));
static void getrevpairs P((const char*));
static void getscript P((struct hshentry*));
static void getstate P((const char*));
static int putabranch P((struct hshentry const*));
static int putadelta P((struct hshentry const*,struct hshentry const*,int));
static int putforest P((struct branchhead const*));
static int putree P((struct hshentry const*));
static int putrunk P((void));
static void recentdate P((struct hshentry const*,struct Datepairs*));
static void trunclocks P((void));

/* Memory heap managed locally for options that persist across calls */
static struct rcsHeap optsHeap = {NULL, NULL, NULL};

/* Local variables, with information pertaining to current GetHdr (rlog)
 * request -- their use spans rcsGetHdr() and at least one of:
 * putadelta(), extractdelta(), extdate(), getdatepair(), getnumericrev(),
 * getrevpairs(), getauthor(), getlocker(), trunclocks(), getstate()
 */
static char const *insDelFormat;	/* fmt for lines changed */
static int branchflag;	/*set on -b */
#ifdef REMOVE
static int exitstatus;
#endif
static int lockflag;			/* only locked revisions; set on -l */
static struct Datepairs *datelist,	/* ranges of dates; set on -d */
			 *duelst;	/* sets of dates; set on -d */
static struct Revpairs   *revlist,	/* set on -r */
			 *Revlst;	/* per-file: -r and -b (candidate)
					 *          revisions; numeric rep'n
					 */
static struct authors *authorlist;	/* set on -w */
static struct rcslockers *lockerlist;	/* set on -l */
static struct stateattri *statelist;	/* set on -s */

/* Used to communicate output stream, and return struct, across fcns */
static int (*wm)P((RCSSTREAM *, const void *, size_t));
static void *OutStream;
static struct rcsGetHdrFields *retdata;

/* Used to build up name/val return list */
static int nvlnum;			/* number of name/val pairs to return */
struct nvlist {
    const struct rcsNameVal *ptr;	/* name/val struct to return */
    struct nvlist *next;		/* next item holder */
};
static struct nvlist nvlhead = {NULL, NULL};
static struct nvlist *nvlptr;
static const char *nsmatch;		/* namespace name (or '*') to match */

static char printbuf[2 * MAXPATHLEN];	/* buffer for output stream output */



rcsGetHdr(conn, rcspath, opts, ret, writeMethod, outputStream)
    RCSCONN conn;
    const char *rcspath;
    struct rcsGetHdrOpts *opts;
    struct rcsGetHdrFields *ret;
    register int
      (*writeMethod)P((RCSSTREAM *outputStream, const void *ptr, size_t num));
    register RCSSTREAM *outputStream;
{
	struct Datepairs *currdate;
	char const *accessListString, *accessFormat;  /* fmt for ACL output */
	char const *headFormat, *symbolFormat;	      /* fmt for Head output */
	struct access const *curaccess;
	struct assoc const *curassoc;
	struct hshentry const *delta;
	struct rcslock const *currlock;
	static int selectflag;	   /* select revisions to print */
	static int onlyRCSflag;  /* print only RCS pathname */
	static int onlylockflag;  /* print only files with locks */
	static int pre5;	/* are archives  pre-version-5 files? */
	int revno;
	int rc;
	static unsigned long hdrfields;	  /* which header fields to display */
	static bool has_been_called = false;	/* for first time init */
	struct rcsNameVal *nvptr;
	const struct rcsNameVal **nvpptr;
	bool isopen;			/* is the archive already open? */
	int i;
	char printbuf[2 * MAXPATHLEN];	/* buffer for printed header output */
	char *printptr;			/* pointer into printbuf */
	char *ptr;
	const char **pptr;
	struct rcsGetHdrFields flds;	/* dummy holder, if user didn't pass */

	/* To construct arrays from linked lists, we gather data into
	 * a generic linked list, and then allocate an array to hold the
	 * list entries.  Since the generic list is reused, we talloc it,
	 * and only grow it as needed (if the next list has more entries)
	 */
	struct genlist {	/* generic list */
	    const void *ptr;	/* item stored; null means end of list */
	    struct genlist *next; /* next item holder */
	};

	static struct genlist genhead = {NULL, NULL};	/* head of list */
	struct genlist *glptr;

	/* In case of internal core code error, return fatal error
	 * to caller, rather than exiting.
	 */
	if ((rc = sigsetjmp(rcsApiEnv, 1)) != 0) return (rc);

	if (!has_been_called || opts) {	/* set defaults */

	    if (has_been_called) rcsHeapFree(&optsHeap);  /* discard old opts */
	    selectflag = true;
	    onlylockflag = false;
	    onlyRCSflag = false;
	    branchflag = false;
	    lockflag = false;
	    datelist = NULL;
	    duelst = NULL;
	    lockerlist = NULL;
	    statelist = NULL;
	    authorlist = NULL;
	    revlist = NULL;
	    hdrfields = 0;	/* 0 means default, i.e. all fields */
	    has_been_called = true;

	    /* Some output formats are different for pre-Version 5
	     * systems.  Set the default here, and reset if running
	     * pre-5.
	     */
	    pre5 = false;
	    accessListString = "\naccess list:";
	    accessFormat = "\n\t%s";
	    headFormat = "RCS file: %s\nWorking file: %s\nhead:%s%s\nbranch:%s%s\nlocks:%s";
	    insDelFormat = "  lines: +%ld -%ld";
	    symbolFormat = "\n\t%s: %s";
	}

	/* Initialize error state (mostly provided by lex analyser) */
	nerror = 0;
	if (!ret) ret = &flds;

	/* Use opts to override defaults */
	/* -L: onlylockflag = opts->onlylocked
	 * -R: (only print RCS name) onlyRCSflag = opts->onlyRCSname;
         *
         * -h: (only print header info)
         *	look at RCS_HDR_BASIC & ~RCS_HDR_SYMBOLS
         *	selectflag = !opts->nogetrev 
	 *
	 * -t: (-h plus descriptive text); look also at RCS_HDR_TEXT
	 * -N: (no symbolic names); ~RCS_HDR_SYMBOLS
	 * -b: branchflag = opts->defaultbranch;
	 * -d: getdatepair(revsel[i]->val)
	 * -l: getlocker(revsel[i]->val)
	 * -r: getrevpairs(revsel[i]->val)
	 * -s: getstate(revsel[i]->val)
	 * -w: getauthor(revsel[i]->val)
         * -V: setRCSversion() [later]
	 * -z: zone_set() [later]
	 */

	if (opts) {
	    hdrfields = opts->hdrfields;
	    onlylockflag = opts->onlylocked;
	    onlyRCSflag = opts->onlyRCSname;
	    branchflag = opts->defaultbranch;
	    /* RCSversion = opts->something */

	    /* Some output formats are different for pre-Version 5 systems */
	    if ((pre5 = RCSversion < VERSION(5))) {
	        accessListString = "\naccess list:   ";
	        accessFormat = "  %s";
	        headFormat = "RCS file:        %s;   Working file:    %s\nhead:           %s%s\nbranch:         %s%s\nlocks:         ";
	        insDelFormat = "  lines added/del: %ld/%ld";
	        symbolFormat = "  %s: %s;";
	    }

	    /* Only get revision tree if we are selecting revisions */
	    selectflag = !opts->nogetrev;

	    /* Parse revision selection flags */
	    for (i = opts->revselnum, nvptr = opts->revsel; --i >= 0; nvptr++){
		switch(*nvptr->name) {
		case 'd':  /* "date" (rlog -d) */
		    getdatepair(nvptr->val);
		    break;
	    
                case 'l':  /* "locker" (rlog -l) */
                    lockflag = true;
		    getlocker(nvptr->val);
                    break;

                case 'r':  /* "revision" (rlog -r) */
		    getrevpairs(nvptr->val);
                    break;

                case 's':  /* "state" (rlog -s) */
		    getstate(nvptr->val);
                    break;

                case 'w':  /* "writer" (rlog -w) */
		    getauthor(nvptr->val);
                    break;
                };
	    }
        } /* end of option processing */


	if (nerror) {	/* syntax error in arguments */
	  return ERR_APPL_ARGS;
	}

	/* Handle input file */
	/* Ensure that we have a valid copy of the archive file, opened.
	 * If the file is already opened and changed, close it (file
	 *  is now closed).
	 * If the file is closed, open it.
	 * Parse the beginning of the file.
	 */
	if ((isopen = !strcmp(rcsParamName, rcspath))) {
	    struct stat sbuf;
	    if (stat(RCSname, &sbuf) < 0 || !same_file(sbuf, RCSstat, 0) ||
			!same_attr(sbuf, RCSstat, 0)) {
		Izclose(&finptr);
		isopen = false;
	    }
	}
	else if (rcsParamName[0]) rcsCloseArch(); /* close different archive */

	if (!isopen) {
	    if ((rc = rcsOpenArch(false, true, rcspath)) < 0) return rc;
	}

	/*
	 * RCSname contains the name of the RCS file,
	 * and finptr the file descriptor;
	 * workname contains the name of the working file.
         */

	/* Keep only those locks given by -l.  */
	if (lockflag)
	    trunclocks();

        /* do nothing if -L is given and there are no locks*/
	if (onlylockflag && !Locks) return ERR_BAD_LOCK;

	/* RCS path */
	ret->rcspath = RCSname;
	if (onlyRCSflag) return 0;

	/* Return header fields; write to output stream (if non-null) */
	if (hdrfields & RCS_HDR_BASIC || !hdrfields) {
	    ret->rcspath = RCSname;
	    ret->workpath = workname;
	    ret->head = Head ? Head->num : "";
	    ret->branch = Dbranch ? Dbranch : "";
	    ret->strict = StrictLocks ? true : false;

	    if (writeMethod) {
		sprintf(printbuf, headFormat, RCSname, workname,
			Head ? " " : "", ret->head,
			Dbranch ? " " : "", ret->branch,
			StrictLocks ? "strict" : "");
		if (swrite(writeMethod, outputStream, printbuf,
			   strlen(printbuf)) < 0)
		    return ERR_WRITE_STREAM;
	    }
	}

	gettree();

	if (!getnumericrev())
	    return ERR_BAD_OLDREV;

	/*  Get list of locks */
	{
	    int llnum = 0;				/* num of locks */
	    glptr = &genhead;
            currlock = Locks;
            while( currlock ) {
		nvptr = ftalloc(struct rcsNameVal);
		nvptr->name = currlock->login;
		nvptr->val = currlock->delta->num;
		
		if (writeMethod) {
		    sprintf(printbuf, symbolFormat, currlock->login,
				currlock->delta->num);
		    if (swrite(writeMethod, outputStream, printbuf,
			       strlen(printbuf)) < 0)
			return ERR_WRITE_STREAM;
		}
		glptr->ptr = nvptr;
		if (!glptr->next) {
		    glptr->next = talloc(struct genlist);
		    glptr = glptr->next;
		    glptr->next = NULL;
		}
		else glptr = glptr->next;
		llnum++;
                currlock = currlock->nextlock;
            }

	    /* Construct result array of locks */
	    nvpptr = ret->locks = ftnalloc(const struct rcsNameVal *, llnum+1);
	    for (i = llnum, glptr = &genhead; --i >= 0; glptr = glptr->next) {
		*nvpptr++ = glptr->ptr;
	    }
	    *nvpptr = NULL;

	    if (writeMethod) {
		if (StrictLocks && pre5) {
                    sprintf(printbuf, "%s","  ;  strict" + (Locks?3:0));
	        }
	        else printbuf[0] = '\0';
		strcat(printbuf, accessListString);

		if (swrite(writeMethod, outputStream, printbuf,
			   strlen(printbuf)) < 0) {
		    return ERR_WRITE_STREAM;
		}
	    }
	}

	/* Get list of access names */
	{
	    int acnum = 0;
	    glptr = &genhead;
            curaccess = AccessList;
            while(curaccess) {
		if (writeMethod) {
		    sprintf(printbuf, accessFormat, curaccess->login);
		    if (swrite(writeMethod, outputStream, printbuf,
				strlen(printbuf)) < 0) {
			return ERR_WRITE_STREAM;
		    }
		}
		acnum++;
		glptr->ptr = curaccess->login;
		if (!glptr->next) {
		    glptr->next = talloc(struct genlist);
		    glptr = glptr->next;
		    glptr->next= NULL;
		}
		else glptr = glptr->next;
                curaccess = curaccess->nextaccess;
            }

	    /* Construct result array of strings */
	    pptr = ret->access = ftnalloc(const char *, acnum+1);
	    for (i = acnum, glptr = &genhead; --i >= 0; glptr = glptr->next) {
		*pptr++ = glptr->ptr;
	    }
	    *pptr = NULL;
	}

	if ((hdrfields & RCS_HDR_SYMBOLS) || !hdrfields) {
	    int symnum = 0;
	    if (writeMethod) {
		sprintf(printbuf, "\nsymbolic names:");
		if (swrite(writeMethod, outputStream, printbuf,
				strlen(printbuf)) < 0) {
			return ERR_WRITE_STREAM;
		}
	    }
	    /* Get list of symbols */
	    glptr = &genhead;
	    for (curassoc=Symbols; curassoc; curassoc=curassoc->nextassoc) {
		nvptr = ftalloc(struct rcsNameVal);
		nvptr->name = curassoc->symbol;
		nvptr->val = curassoc->num;
		
		if (writeMethod) {
		    sprintf(printbuf, symbolFormat, curassoc->symbol,
				curassoc->num);
		    if (swrite(writeMethod, outputStream, printbuf,
			       strlen(printbuf)) < 0)
			return ERR_WRITE_STREAM;
		}
		glptr->ptr = nvptr;
		if (!glptr->next) {
		    glptr->next = talloc(struct genlist);
		    glptr = glptr->next;
		    glptr->next = NULL;
		}
		else glptr = glptr->next;
		symnum++;
	    }

	    /* Construct result array of symbol/val pairs */
	    nvpptr = ret->symbols = ftnalloc(const struct rcsNameVal*,symnum+1);
	    for (i = symnum, glptr = &genhead; --i >= 0; glptr = glptr->next) {
		*nvpptr++ = glptr->ptr;
	    }
	    *nvpptr = NULL;
	}

	/* Set up to gather all name/val pairs.  First do list(s) of
	 * namespace(s) in file header (if requested).  Next, do list(s)
	 * of namespace(s) in each revision (if requested).  Then do
	 * specific namespace name lookups.  Then build the return array.
	 */
	nvlnum = 0;
	nvlptr = &nvlhead;	/* nvlhead is used only for name/val list */

	if (opts->namespace) {
	    const char *match;
	    nsmatch = opts->namespace;

	    /* Get list of namespaces, and name/val pairs */
	    if (writeMethod &&
		swrite(writeMethod, outputStream, "\nnamespaces:", 12) < 0)
		    return ERR_WRITE_STREAM;

	    match = (nsmatch[0] == '*' && nsmatch[1] == '\0')
		    ? NULL : opts->namespace;

	    for (nvptr = getfirstnameval(NULL, match, rcsNameSpace); nvptr;
		 nvptr = getnextnameval()) {

		if (writeMethod) {
		    sprintf(printbuf, "\n\t%s.%s:  %s",
			    nvptr->namespace ? nvptr->namespace : "",
			    nvptr->name, nvptr->val);
		    if (swrite(writeMethod, outputStream, printbuf,
			       strlen(printbuf)) < 0)
			return ERR_WRITE_STREAM;
		}

		nvlptr->ptr = nvptr;
		if (!nvlptr->next) {
		    nvlptr->next = talloc(struct nvlist);
		    nvlptr = nvlptr->next;
		    nvlptr->next = NULL;
		}
		else nvlptr = nvlptr->next;
		nvlnum++;
	    }

	}
	else nsmatch = NULL;

	/* Print comment leader, keyword expansion, total revisions */
	if (writeMethod) {
	    if (pre5) {
		sprintf(printbuf, "\ncomment leader:  \"%.*s\"",
			Comment.size, Comment.string);
		if (swrite(writeMethod, outputStream, printbuf,
		           strlen(printbuf)) < 0)
		    return ERR_WRITE_STREAM;
	    }
	    if (!pre5  ||  Expand != KEYVAL_EXPAND) {
		sprintf(printbuf, "\nkeyword substitution: %s",
				expand_names[Expand]);
		if (swrite(writeMethod, outputStream, printbuf,
				strlen(printbuf)) < 0)
		    return ERR_WRITE_STREAM;
	    }
	    sprintf(printbuf, "\ntotal revisions: %d", TotalDeltas);
	    if (swrite(writeMethod, outputStream, printbuf,
			   strlen(printbuf)) < 0)
		return ERR_WRITE_STREAM;
	}

	/* Return comment leader, keyword expansion, total revisions */

	/* cannot use fstr_save: cbuf may not be null-terminated */
	ret->comment = ftnalloc(const char, Comment.size + 1);
	memcpy((void *)ret->comment, Comment.string, Comment.size);
	((char *)ret->comment)[Comment.size] = '\0';
	ret->keysub = expand_names[Expand];
	ret->totalrev = TotalDeltas;

	/* If all we wanted was basic header info (and possibly 
	 * specific namespace values), just retrieve the namespace info
	 * and return.  (Note that we've already retrived the file header
	 * namespaces collectively [opts->names].)
	 */
	if (hdrfields == RCS_HDR_BASIC && !selectflag) {
	    if (opts->nameselnum > 0) {
		for (i = opts->nameselnum, nvptr = opts->namesel;
			--i >= 0; nvptr++) {
		    if (nvptr->revision && *nvptr->revision) continue;
		    if ((nvlptr->ptr = getnameval(nvptr))) {
			if (!nvlptr->next) {
		    	    nvlptr->next = talloc(struct nvlist);
			    nvlptr = nvlptr->next;
			    nvlptr->next = NULL;
			}
			else nvlptr = nvlptr->next;
			nvlnum++;
		    }
		}
	    }
	    nvpptr = ret->names = ftnalloc(const struct rcsNameVal*, nvlnum+1);
	    for (i = nvlnum, nvlptr = &nvlhead; --i >= 0; nvlptr=nvlptr->next) {
		*nvpptr++ = nvlptr->ptr;
	    }
	    *nvpptr = NULL;
	    return 0;
	}
		 
	revno = 0;
	ret->selectrev = 0;

	/* Handle revision information */
	if (selectflag && Head) {
		exttree(Head);

	    /*  get most recently date of the dates pointed by duelst  */
	    currdate = duelst;
	    while( currdate) {
		VOID strcpy(currdate->strtdate, "0.0.0.0.0.0");
		recentdate(Head, currdate);
		currdate = currdate->dnext;
	    }

	    revno = extdate(Head);

	    ret->selectrev = revno;
	    if (writeMethod) {
		sprintf(printbuf, ";\tselected revisions: %d", revno);
		if (swrite(writeMethod, outputStream, printbuf,
			   strlen(printbuf)) < 0)
		    return ERR_WRITE_STREAM;
	    }

	    if (writeMethod && swrite(writeMethod, outputStream, "\n", 1) < 0)
		return ERR_WRITE_STREAM;
	}

	/* Put descriptive text */
	if (writeMethod && (!hdrfields || (hdrfields & RCS_HDR_TEXT))) {
	    if (swrite(writeMethod, outputStream, "description:\n", 13) < 0)
		        return ERR_WRITE_STREAM;
	}

	/* Retrieve description to rcsDescBuf; write to output stream 
	 * if non-null
         */
	if (!hdrfields || (hdrfields & RCS_HDR_TEXT)) {
	    rcsWriteMethod = writeMethod;
	    rcsOutputStream = outputStream;
	    getdesc(0x2);
	    rcsWriteMethod = NULL;
	    rcsOutputStream = NULL;
	    ret->description = rcsDescBuf.string;
	}

	/* Print revision information */
	if (revno) {
	    while (! (delta = readdeltalog())->selector  ||  --revno)
		continue;
	    if (delta->next && countnumflds(delta->num)==2)
		/* Read through delta->next to get its insertlns.  */
		while (readdeltalog() != delta->next)
		    continue;

	    /* Pass output stream to revision dumping routines;
	     * indicate that first delta still needs to be returned
	     * (retdata non-NULL means to store delta's data;
	     *  the first delta stored automatically sets retdata NULL)
	     */
	    wm = writeMethod;
	    OutStream = outputStream;
	    retdata = ret;

	    /* Dump revision data */
	    if (putrunk() || putree(Head)) return ERR_WRITE_STREAM;
	}

	/* Collect any specific namespace values requested */
	if (opts->nameselnum > 0) {
	    for (i = opts->nameselnum, nvptr=opts->namesel; --i >= 0; nvptr++) {
		if (nvptr->revision && *nvptr->revision) continue;
		if ((nvlptr->ptr = getnameval(nvptr))) {
		    if (!nvlptr->next) {
		        nvlptr->next = talloc(struct nvlist);
			nvlptr = nvlptr->next;
			nvlptr->next = NULL;
		    }
		    else nvlptr = nvlptr->next;
		    nvlnum++;
		}
	    }
	}

	/* Construct array of all name/val pairs returned
	 * (this includes both ones requested by namespace, and by
	 *  specific identifiers)
	 */
	nvpptr = ret->names = ftnalloc(const struct rcsNameVal*, nvlnum+1);
	for (i = nvlnum, nvlptr = &nvlhead; --i >= 0; nvlptr=nvlptr->next) {
		*nvpptr++ = nvlptr->ptr;
	}
	*nvpptr = NULL;

	return 0;
}

#ifdef REMOVE
	static void
cleanup()
{
	if (nerror) exitstatus = EXIT_FAILURE;
	Izclose(&finptr);
}

#if RCS_lint
#	define exiterr rlogExit
#endif
	void
exiterr()
{
	_exit(EXIT_FAILURE);
}
#endif /* REMOVE */


	static int
putrunk()
/*  function:  print revisions chosen, which are in trunk      */

{
	register struct hshentry const *ptr;

	for (ptr = Head;  ptr;  ptr = ptr->next)
		if (putadelta(ptr, ptr->next, true)) return ERR_WRITE_STREAM;

	return 0;
}



	static int
putree(root)
	struct hshentry const *root;
/*   function: print delta tree (not including trunk) in reverse
               order on each branch                                        */

{
	if (!root) return 0;

        if (putree(root->next) || putforest(root->branches))
		 return ERR_WRITE_STREAM;

	return 0;
}




	static int
putforest(branchroot)
	struct branchhead const *branchroot;
/*   function:  print branches that has the same direct ancestor    */
{
	if (!branchroot) return 0;

        if (putforest(branchroot->nextbranch) ||
            putabranch(branchroot->hsh) ||
            putree(branchroot->hsh))
		return ERR_WRITE_STREAM;

	return 0;
}




	static int
putabranch(root)
	struct hshentry const *root;
/*   function  :  print one branch     */

{
	if (!root) return 0;

        if (putabranch(root->next) || putadelta(root, root, false))
		return ERR_WRITE_STREAM;

	return 0;
}





	static int
putadelta(node,editscript,trunk)
	register struct hshentry const *node, *editscript;
	int trunk;
/*  function: Print delta node if node->selector is set.        */
/*      editscript indicates where the editscript is stored     */
/*      trunk indicated whether this node is in trunk           */
{
	static char emptych[] = EMPTYLOG;

	char const *s;
	size_t n;
	struct branchhead const *newbranch;
	struct buf branchnum;
	char datebuf[datesize + zonelenmax];
	int pre5 = RCSversion < VERSION(5);

	/* To construct arrays from linked lists, we gather data into
	 * a generic linked list, and then allocate an array to hold the
	 * list entries.  Since the generic list is reused, we talloc it,
	 * and only grow it as needed (if the next list has more entries)
	 */
	struct genlist {	/* generic list */
	    const void *ptr;	/* item stored; null means end of list */
	    struct genlist *next; /* next item holder */
	};

	static struct genlist genhead = {NULL, NULL};	/* head of list */
	struct genlist *glptr;

	if (!node->selector)
            return 0;

	if (wm) {	/* only print if we have a write method */
	    sprintf(printbuf,
		"----------------------------\nrevision %s%s",
		node->num,  pre5 ? "        " : ""
	    );
	    if (swrite(wm, OutStream, printbuf, strlen(printbuf)) < 0)
		return ERR_WRITE_STREAM;

            if ( node->lockedby ) {
	        sprintf(printbuf, pre5+"\tlocked by: %s;", node->lockedby);
	        if (swrite(wm, OutStream, printbuf, strlen(printbuf)) < 0)
		    return ERR_WRITE_STREAM;
	    }

	    sprintf(printbuf, "\ndate: %s;  author: %s;  state: %s;",
		    date2str(node->date, datebuf),
		    node->author, node->state
	    );
	    if (swrite(wm, OutStream, printbuf, strlen(printbuf)) < 0)
		return ERR_WRITE_STREAM;
	}

	/* Lines inserted/deleted */
        if ( editscript ) {
	   if (wm) {
           	if(trunk)
		    sprintf(printbuf, insDelFormat,
                             editscript->deletelns, editscript->insertlns);
           	else
	            sprintf(printbuf, insDelFormat,
                             editscript->insertlns, editscript->deletelns);

		if (swrite(wm, OutStream, printbuf, strlen(printbuf)) < 0)
			return ERR_WRITE_STREAM;
	    }

	    if (retdata) {
		if (trunk) {
		    retdata->inserted = editscript->deletelns;
		    retdata->deleted  = editscript->insertlns;
		}
		else {
		    retdata->deleted  = editscript->deletelns;
		    retdata->inserted = editscript->insertlns;
		}
	    }
	}

	/* Branches */
        newbranch = node->branches;

	/* Only process if we have some place to put data (output stream,
	 * or first revision's data to be returned)
	 */
        if ( newbranch && (wm || retdata)) {
	    int bnum = 0;
	    bufautobegin(&branchnum);
	    if (wm && swrite(wm, OutStream, "\nbranches:", 10) < 0)
		return ERR_WRITE_STREAM;
	    glptr = &genhead;
            while( newbranch ) {
		getbranchno(newbranch->hsh->num, &branchnum);

		if (wm) {
		    sprintf(printbuf, "  %s;", branchnum.string);
		    if (swrite(wm, OutStream, printbuf, strlen(printbuf)) < 0)
			return ERR_WRITE_STREAM;
		}

		if (retdata) {
		    glptr->ptr = fstr_save(branchnum.string);
		    if (!glptr->next) {
		        glptr->next = talloc(struct genlist);
		        glptr = glptr->next;
		        glptr->next = NULL;
		    }
		    else glptr = glptr->next;
		    bnum++;
		}

                newbranch = newbranch->nextbranch;
            }

	    /* Construct result array of strings */
	    if (retdata) {
		int i;
		const char **pptr =
	        	retdata->branches = ftnalloc(const char*, bnum+1);

	        for (i = bnum, glptr = &genhead; --i >= 0;
		     				glptr = glptr->next) {
		    *pptr++ = glptr->ptr;
	        }
	        *pptr = NULL;
	    }

	   bufautoend(&branchnum);
	}
	else {
	    if (retdata) {
		retdata->branches = ftalloc(const char *);
		retdata->branches[0] = NULL;
	    }
        }

	/* Return namespace(s) requested for this revision */
	if (nsmatch) {
	    struct rcsNameVal *nvptr;
	    const char *match = (nsmatch[0] == '*' && nsmatch[1] == '\0') ?
				NULL : nsmatch;

	    /* Get list of namespaces, and name/val pairs */
	    if (wm &&
		swrite(wm, OutStream, "\nnamespaces:", 12) < 0)
		    return ERR_WRITE_STREAM;

	    for (nvptr = getfirstnameval(node->num, match, node->namesp); nvptr;
		 nvptr = getnextnameval()) {

		if (wm) {
		    sprintf(printbuf, "\n\t%s.%s:  %s",
			    nvptr->namespace ? nvptr->namespace : "",
			    nvptr->name, nvptr->val);
		    if (swrite(wm, OutStream, printbuf,
			       strlen(printbuf)) < 0)
			return ERR_WRITE_STREAM;
		}

		nvlptr->ptr = nvptr;
		if (!nvlptr->next) {
		    nvlptr->next = talloc(struct nvlist);
		    nvlptr = nvlptr->next;
		    nvlptr->next = NULL;
		}
		else nvlptr = nvlptr->next;
		nvlnum++;
	    }
	}

	/* Log message */
	if (wm) {
	    s = node->log.string;
	    if (!(n = node->log.size)) {
		s = emptych;
		n = sizeof(emptych)-1;
	    }
	    if (swrite(wm, OutStream, "\n", 1) < 0 ||
		swrite(wm, OutStream, s, n) < 0 ||
		s[n-1] != '\n' && swrite(wm, OutStream, "\n", 1) < 0) {
		    return ERR_WRITE_STREAM;
	    }
	}


	/* Fill in remaining retdata fields */
	if (retdata) {
	    retdata->revision = node->num;
	    retdata->locker = node->lockedby;
	    retdata->date = fstr_save(datebuf);
	    retdata->author = node->author;
	    retdata->state = node->state;
	    retdata->nextrev = node->next ? node->next->num : NULL;
	    retdata = NULL;
	}

	return 0;
}


	static struct hshentry const *
readdeltalog()
/*  Function : get the log message and skip the text of a deltatext node.
 *	       Return the delta found.
 *             Assumes the current lexeme is not yet in nexttok; does not
 *             advance nexttok.
 */
{
        register struct  hshentry  * Delta;
	struct buf logbuf;
	struct cbuf cb;

	if (eoflex())
		fatserror("missing delta log");
        nextlex();
	if (!(Delta = getnum()))
		fatserror("delta number corrupted");
	getkeystring(Klog);
	if (Delta->log.string)
		fatserror("duplicate delta log");
	bufautobegin(&logbuf);
	cb = savestring(&logbuf);
	Delta->log = bufremember(&logbuf, cb.size);

	ignorephrases(Ktext);
	getkeystring(Ktext);
        Delta->insertlns = Delta->deletelns = 0;
        if ( Delta != Head)
                getscript(Delta);
        else
                readstring();
	return Delta;
}


	static void
getscript(Delta)
struct    hshentry   * Delta;
/*   function:  read edit script of Delta and count how many lines added  */
/*              and deleted in the script                                 */

{
        int ed;   /*  editor command  */
	declarecache;
	register RILE *fin;
        register  int   c;
	register long i;
	struct diffcmd dc;

	fin = finptr;
	setupcache(fin);
	initdiffcmd(&dc);
	while (0  <=  (ed = getdiffcmd(fin,true,(FILE *)0,&dc)))
	    if (!ed)
                 Delta->deletelns += dc.nlines;
	    else {
                 /*  skip scripted lines  */
		 i = dc.nlines;
		 Delta->insertlns += i;
		 cache(fin);
		 do {
		     for (;;) {
			cacheget_(c)
			switch (c) {
			    default:
				continue;
			    case SDELIM:
				cacheget_(c)
				if (c == SDELIM)
				    continue;
				if (--i)
					unexpected_EOF();
				nextc = c;
				uncache(fin);
				return;
			    case '\n':
				break;
			}
			break;
		     }
		     ++rcsline;
		 } while (--i);
		 uncache(fin);
            }
}







	static void
exttree(root)
struct hshentry  *root;
/*  function: select revisions , starting with root             */

{
	struct branchhead const *newbranch;

	if (!root) return;

	root->selector = extractdelta(root);
	root->log.string = 0;
        exttree(root->next);

        newbranch = root->branches;
        while( newbranch ) {
            exttree(newbranch->hsh);
            newbranch = newbranch->nextbranch;
        }
}




	static void
getlocker(argv)
const char    * argv;
/*   function : get the login names of lockers from command line   */
/*              and store in lockerlist.                           */

{
        register char c;
	register const char *ptr;
	char *val;
	struct rcslockers *newlocker;
	int i;
        argv--;
	while ((c = *++argv)==',' || c==' ' || c=='\t' || c=='\n' || c==';')
	    continue;
        if (  c == '\0') {
	    lockerlist = 0;
            return;
        }

        while( c != '\0' ) {
	    newlocker = rcsHeapAlloc(&optsHeap, sizeof(struct rcslockers));
            newlocker->lockerlink = lockerlist;
	    ptr = argv;		/* remember beginning of name */
            lockerlist = newlocker;
	    while ((c = *++argv) && c!=',' && c!=' ' && c!='\t' && c!='\n' && c!=';')
		continue;
	    val = rcsHeapAlloc(&optsHeap, (i = argv-ptr)+1);
	    memcpy(val, ptr, i);
	    val[i] = '\0';
	    newlocker->login = val;
            if ( c == '\0' ) return;
	    while ((c = *++argv)==',' || c==' ' || c=='\t' || c=='\n' || c==';')
		continue;
        }
}



	static void
getauthor(argv)
const char   *argv;
/*   function:  get the author's name from command line   */
/*              and store in authorlist                   */

{
        register    c;
        struct     authors  * newauthor;
	register const char *ptr;
	char *val;
	int i;

        argv--;
	while ((c = *++argv)==',' || c==' ' || c=='\t' || c=='\n' || c==';')
	    continue;
        if ( c == '\0' ) {
	    authorlist = rcsHeapAlloc(&optsHeap, sizeof(struct authors));
	    authorlist->login = getusername(false);
	    authorlist->nextauthor = 0;
            return;
        }

        while( c != '\0' ) {
	    newauthor = rcsHeapAlloc(&optsHeap, sizeof(struct authors));
            newauthor->nextauthor = authorlist;
	    ptr = argv;		/* remember beginning of author */
            authorlist = newauthor;
	    while ((c = *++argv) && c!=',' && c!=' ' && c!='\t' && c!='\n' && c!=';')
		continue;
	    val = rcsHeapAlloc(&optsHeap, (i = ptr-argv) + 1);
	    memcpy(val, ptr, i);
	    val[i] = '\0';
	    newauthor->login = val;
            if ( c == '\0') return;
	    while ((c = *++argv)==',' || c==' ' || c=='\t' || c=='\n' || c==';')
		continue;
        }
}




	static void
getstate(argv)
const char   * argv;
/*   function :  get the states of revisions from command line  */
/*               and store in statelist                         */

{
        register  char  c;
        struct    stateattri    *newstate;
	register const char *ptr;
	char *val;
	int i;

        argv--;
	while ((c = *++argv)==',' || c==' ' || c=='\t' || c=='\n' || c==';')
	    continue;
        if ( c == '\0'){
	  /* ignore blank states 
	    error("missing state attributes after -s options");
	   */
            return;
        }

        while( c != '\0' ) {
	    newstate = rcsHeapAlloc(&optsHeap, sizeof(struct stateattri));
            newstate->nextstate = statelist;
            newstate->status = argv;
	    ptr = argv;		/* remember beginning of state */
            statelist = newstate;
	    while ((c = *++argv) && c!=',' && c!=' ' && c!='\t' && c!='\n' && c!=';')
		continue;
	    val = rcsHeapAlloc(&optsHeap, (i = ptr-argv) + 1);
	    memcpy(val, ptr, i);
	    val[i] = '\0';
	    newstate->status = val;
            if ( c == '\0' ) return;
	    while ((c = *++argv)==',' || c==' ' || c=='\t' || c=='\n' || c==';')
		continue;
        }
}



	static void
trunclocks()
/*  Function:  Truncate the list of locks to those that are held by the  */
/*             id's on lockerlist. Do not truncate if lockerlist empty.  */

{
	struct rcslockers const *plocker;
	struct rcslock *p, **pp;

	if (!lockerlist) return;

        /* shorten Locks to those contained in lockerlist */
	for (pp = &Locks;  (p = *pp);  )
	    for (plocker = lockerlist;  ;  )
		if (strcmp(plocker->login, p->login) == 0) {
		    pp = &p->nextlock;
		    break;
		} else if (!(plocker = plocker->lockerlink)) {
		    *pp = p->nextlock;
		    break;
		}
}



	static void
recentdate(root, pd)
	struct hshentry const *root;
	struct Datepairs *pd;
/*  function:  Finds the delta that is closest to the cutoff date given by   */
/*             pd among the revisions selected by exttree.                   */
/*             Successively narrows down the interval given by pd,           */
/*             and sets the strtdate of pd to the date of the selected delta */
{
	struct branchhead const *newbranch;

	if (!root) return;
	if (root->selector) {
	     if ( cmpdate(root->date, pd->strtdate) >= 0 &&
		  cmpdate(root->date, pd->enddate) <= 0)
		VOID strcpy(pd->strtdate, root->date);
        }

        recentdate(root->next, pd);
        newbranch = root->branches;
        while( newbranch) {
           recentdate(newbranch->hsh, pd);
           newbranch = newbranch->nextbranch;
	}
}






	static int
extdate(root)
struct  hshentry        * root;
/*  function:  select revisions which are in the date range specified     */
/*             in duelst  and datelist, start at root                     */
/* Yield number of revisions selected, including those already selected.  */
{
	struct branchhead const *newbranch;
	struct Datepairs const *pdate;
	int revno, ne;

	if (!root)
	    return 0;

        if ( datelist || duelst) {
            pdate = datelist;
            while( pdate ) {
		ne = pdate->ne_date;
		if (
			(!pdate->strtdate[0]
			|| ne <= cmpdate(root->date, pdate->strtdate))
		    &&
			(!pdate->enddate[0]
			|| ne <= cmpdate(pdate->enddate, root->date))
		)
                        break;
                pdate = pdate->dnext;
            }
	    if (!pdate) {
                pdate = duelst;
		for (;;) {
		   if (!pdate) {
			root->selector = false;
			break;
		   }
		   if (cmpdate(root->date, pdate->strtdate) == 0)
                      break;
                   pdate = pdate->dnext;
                }
            }
        }
	revno = root->selector + extdate(root->next);

        newbranch = root->branches;
        while( newbranch ) {
	   revno += extdate(newbranch->hsh);
           newbranch = newbranch->nextbranch;
        }
	return revno;
}



	static char
extractdelta(pdelta)
	struct hshentry const *pdelta;
/*  function:  compare information of pdelta to the authorlist, lockerlist,*/
/*             statelist, revlist and yield true if pdelta is selected.    */

{
	struct rcslock const *plock;
	struct stateattri const *pstate;
	struct authors const *pauthor;
	struct Revpairs const *prevision;
	int length;

	if ((pauthor = authorlist)) /* only certain authors wanted */
	    while (strcmp(pauthor->login, pdelta->author) != 0)
		if (!(pauthor = pauthor->nextauthor))
		    return false;
	if ((pstate = statelist)) /* only certain states wanted */
	    while (strcmp(pstate->status, pdelta->state) != 0)
		if (!(pstate = pstate->nextstate))
		    return false;
	if (lockflag) /* only locked revisions wanted */
	    for (plock = Locks;  ;  plock = plock->nextlock)
		if (!plock)
		    return false;
		else if (plock->delta == pdelta)
		    break;
	if ((prevision = Revlst)) /* only certain revs or branches wanted */
	    for (;;) {
                length = prevision->numfld;
		if (
		    countnumflds(pdelta->num) == length+(length&1) &&
		    0 <= compartial(pdelta->num, prevision->strtrev, length) &&
		    0 <= compartial(prevision->endrev, pdelta->num, length)
		)
		     break;
		if (!(prevision = prevision->rnext))
		    return false;
            }
	return true;
}



	static void
getdatepair(argv)
   const char   * argv;
/*  function:  get time range from command line and store in datelist if    */
/*             a time range specified or in duelst if a time spot specified */

{
        register   char         c;
        struct     Datepairs    * nextdate;
	char const		* rawdate;
	int                     switchflag;
	char 			rawbuf[128];

        argv--;
	while ((c = *++argv)==',' || c==' ' || c=='\t' || c=='\n' || c==';')
	    continue;
        if ( c == '\0' ) {
	    /* Ignore blank dates
	    error("missing date/time after -d"); 
	     */
            return;
        }

        while( c != '\0' )  {
	    switchflag = false;
	    nextdate = rcsHeapAlloc(&optsHeap, sizeof(struct Datepairs));
            if ( c == '<' ) {   /*   case: -d <date   */
                c = *++argv;
		if (!(nextdate->ne_date = c!='='))
		    c = *++argv;
                (nextdate->strtdate)[0] = '\0';
	    } else if (c == '>') { /* case: -d'>date' */
		c = *++argv;
		if (!(nextdate->ne_date = c!='='))
		    c = *++argv;
		(nextdate->enddate)[0] = '\0';
		switchflag = true;
	    } else {
                rawdate = argv;
		while( c != '<' && c != '>' && c != ';' && c != '\0')
		     c = *++argv;
		if (*argv) {
		    /* make copy, to null-terminate string */
		    memcpy(rawbuf, rawdate, argv-rawdate);
		    rawbuf[argv-rawdate] = '\0';
		    rawdate = rawbuf;
		}
		if ( c == '>' ) switchflag=true;
		str2date(rawdate,
			 switchflag ? nextdate->enddate : nextdate->strtdate);
		if ( c == ';' || c == '\0') {  /*  case: -d date  */
		    VOID strcpy(nextdate->enddate,nextdate->strtdate);
                    nextdate->dnext = duelst;
                    duelst = nextdate;
		    goto end;
		} else {
		    /*   case:   -d date<  or -d  date>; see switchflag */
		    int eq = argv[1]=='=';
		    nextdate->ne_date = !eq;
		    argv += eq;
		    while ((c = *++argv) == ' ' || c=='\t' || c=='\n')
			continue;
		    if ( c == ';' || c == '\0') {
			/* second date missing */
			if (switchflag)
			    *nextdate->strtdate= '\0';
			else
			    *nextdate->enddate= '\0';
			nextdate->dnext = datelist;
			datelist = nextdate;
			goto end;
		    }
                }
            }
            rawdate = argv;
	    while( c != '>' && c != '<' && c != ';' && c != '\0')
 		c = *++argv;
	    if (*argv) {
		memcpy(rawbuf, rawdate, argv-rawdate);
		rawbuf[argv-rawdate] = '\0';
		rawdate = rawbuf;
	    }
	    str2date(rawdate,
		     switchflag ? nextdate->strtdate : nextdate->enddate);
            nextdate->dnext = datelist;
	    datelist = nextdate;
     end:
	    if (RCSversion < VERSION(5))
		nextdate->ne_date = 0;
	    if ( c == '\0')  return;
	    while ((c = *++argv) == ';' || c == ' ' || c == '\t' || c =='\n')
		continue;
        }
}



	static int
getnumericrev()
/*  function:  get the numeric name of revisions which stored in revlist  */
/*             and then stored the numeric names in Revlst                */
/*             if branchflag, also add default branch                     */

{
        struct  Revpairs        * ptr, *pt;
	int n;
	struct buf s, e;
	char const *lrev;
	struct buf const *rstart, *rend;

	Revlst = 0;
        ptr = revlist;
	bufautobegin(&s);
	bufautobegin(&e);
        while( ptr ) {
	    n = 0;
	    rstart = &s;
	    rend = &e;

	    switch (ptr->numfld) {

	      case 1: /* -rREV */
		if (!expandsym(ptr->strtrev, &s))
		    goto freebufs;
		rend = &s;
		n = countnumflds(s.string);
		if (!n  &&  (lrev = tiprev())) {
		    bufscpy(&s, lrev);
		    n = countnumflds(lrev);
		}
		break;

	      case 2: /* -rREV: */
		if (!expandsym(ptr->strtrev, &s))
		    goto freebufs;
		bufscpy(&e, s.string);
		n = countnumflds(s.string);
		(n<2 ? e.string : strrchr(e.string,'.'))[0]  =  0;
		break;

	      case 3: /* -r:REV */
		if (!expandsym(ptr->endrev, &e))
		    goto freebufs;
		if ((n = countnumflds(e.string)) < 2)
		    bufscpy(&s, ".0");
		else {
		    bufscpy(&s, e.string);
		    VOID strcpy(strrchr(s.string,'.'), ".0");
		}
		break;

	      default: /* -rREV1:REV2 */
		if (!(
			expandsym(ptr->strtrev, &s)
		    &&	expandsym(ptr->endrev, &e)
		    &&	checkrevpair(s.string, e.string)
		))
		    goto freebufs;
		n = countnumflds(s.string);
		/* Swap if out of order.  */
		if (compartial(s.string,e.string,n) > 0) {
		    rstart = &e;
		    rend = &s;
		}
		break;
	    }

	    if (n) {
		pt = ftalloc(struct Revpairs);
		pt->numfld = n;
		pt->strtrev = fstr_save(rstart->string);
		pt->endrev = fstr_save(rend->string);
                pt->rnext = Revlst;
                Revlst = pt;
	    }
	    ptr = ptr->rnext;
        }
        /* Now take care of branchflag */
	if (branchflag && (Dbranch||Head)) {
	    pt = ftalloc(struct Revpairs);
	    pt->strtrev = pt->endrev =
		Dbranch ? Dbranch : fstr_save(partialno(&s,Head->num,1));
	    pt->rnext=Revlst; Revlst=pt;
	    pt->numfld = countnumflds(pt->strtrev);
        }

      freebufs:
	bufautoend(&s);
	bufautoend(&e);
	return !ptr;
}



	static int
checkrevpair(num1,num2)
	char const *num1, *num2;
/*  function:  check whether num1, num2 are legal pair,i.e.
    only the last field are different and have same number of
    fields( if length <= 2, may be different if first field)   */

{
	int length = countnumflds(num1);

	if (
			countnumflds(num2) != length
		||	(2 < length  &&  compartial(num1, num2, length-1) != 0)
	) {
	    rcserror("invalid branch or revision pair %s : %s", num1, num2);
            return false;
        }

        return true;
}



	static void
getrevpairs(argv)
register     const char    * argv;
/*  function:  get revision or branch range from command line, and   */
/*             store in revlist                                      */

{
        register    char    c;
        struct      Revpairs  * nextrevpair;
	register    const char  *ptr;
	char	    *val;
	int separator;
	int i;

	c = *argv;

	/* Support old ambiguous '-' syntax; this will go away.  */
	if (strchr(argv,':'))
	    separator = ':';
	else {
	    if (strchr(argv,'-')  &&  VERSION(5) <= RCSversion)
		warn("`-' is obsolete in `-r%s'; use `:' instead", argv);
	    separator = '-';
	}

	for (;;) {
	    while (c==' ' || c=='\t' || c=='\n')
		c = *++argv;
	    nextrevpair = rcsHeapAlloc(&optsHeap, sizeof(struct Revpairs));
            nextrevpair->rnext = revlist;
            revlist = nextrevpair;
	    nextrevpair->numfld = 1;
	    ptr = argv;		/* remember beginning of revision string */
	    for (;;  c = *++argv) {
		switch (c) {
		    default:
			continue;
		    case '\0': case ' ': case '\t': case '\n':
		    case ',': case ';':
			break;
		    case ':': case '-':
			if (c == separator)
			    break;
			continue;
		}
		break;
	    }
	    val = rcsHeapAlloc(&optsHeap, (i = argv-ptr) + 1);
	    if (i) memcpy(val, ptr, i);
	    val[i] = '\0';
	    nextrevpair->strtrev = val;
	    while (c==' ' || c=='\t' || c=='\n')
		c = *++argv;
	    if (c == separator) {
		while ((c = *++argv) == ' ' || c == '\t' || c =='\n')
		    continue;
		ptr = argv;	/* remember beginning of revision string */
		for (;;  c = *++argv) {
		    switch (c) {
			default:
			    continue;
			case '\0': case ' ': case '\t': case '\n':
			case ',': case ';':
			    break;
			case ':': case '-':
			    if (c == separator)
				break;
			    continue;
		    }
		    break;
		}
		val = rcsHeapAlloc(&optsHeap, (i = argv-ptr) + 1);
		if (i) memcpy(val, ptr, i);
		val[i] = '\0';
		nextrevpair->endrev = val;
		while (c==' ' || c=='\t' || c =='\n')
		    c = *++argv;
		nextrevpair->numfld =
		    !nextrevpair->endrev[0] ? 2 /* -rREV: */ :
		    !nextrevpair->strtrev[0] ? 3 /* -r:REV */ :
		    4 /* -rREV1:REV2 */;
            }
	    if (!c)
		break;
	    else if (c==',' || c==';')
		c = *++argv;
	    else
		error("missing `,' near `%c%s'", c, argv+1);
	}
}
