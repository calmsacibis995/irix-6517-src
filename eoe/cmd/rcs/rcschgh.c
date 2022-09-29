/* Change RCS file attributes.  */

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
 * $Log: rcschgh.c,v $
 * Revision 1.1  1996/06/28 23:05:34  msf
 * Initial revision
 *
 */


#include "rcsbase.h"
#include "rcspriv.h"

struct  Lockrev {
	char const *revno;
        struct  Lockrev   * nextrev;
};

struct  Symrev {
	char const *revno;
	char const *ssymbol;
        int     override;
        struct  Symrev  * nextsym;
};

struct Message {
	char const *revno;
	struct cbuf message;
	struct Message *nextmessage;
};

struct  Status {
	char const *revno;
	char const *status;
        struct  Status  * nextstatus;
};

enum changeaccess {append, erase};
struct chaccess {
	char const *login;
	enum changeaccess command;
	struct chaccess *nextchaccess;
};

struct delrevpair {
	char const *strt;
	char const *end;
        int     code;
};

static int branchpoint P((struct hshentry*,struct hshentry*));
static int buildeltatext P((struct hshentries const*));
static int doaccess P((void));
static int doassoc P((void));
static int dolocks P((void));
static int domessages P((void));
static int rcs_setstate P((char const*,char const*));
static int removerevs P((void));
static int setlock P((char const*));
static struct Lockrev **rmnewlocklst P((char const*));
static struct hshentry *searchcutpt P((char const*,int,struct hshentries*));
static void buildtree P((void));
static void cleanup P((void));
static int getaccessor P((char*,enum changeaccess));
static int getassoclst P((int,char*));
static void getchaccess P((char const*,enum changeaccess));
static void getdelrev P((char*));
static int getmessage P((const char *, char *));
static int getstates P((const char*, char *));
static void scanlogtext P((struct hshentry*,int));

static struct buf numrev;
static char const *headstate;
static int chgheadstate, exitstatus, lockhead, unlockcaller;
static struct Lockrev *newlocklst, *rmvlocklst;
static struct Message *messagelst, **nextmessage;
static struct Status *statelst, **nextstate;
static struct Symrev *assoclst, **nextassoc;
static struct chaccess *chaccess, **nextchaccess;
static struct delrevpair delrev;
static struct hshentry *cuthead, *cuttail, *delstrt;
static struct hshentries *gendeltas;

/* Memory heaps managed locally: one for -A files; one for persistent opts */
static struct rcsHeap accessHeap = {NULL, NULL, NULL};
static struct rcsHeap optsHeap = {NULL, NULL, NULL};

rcsChangeHdr(conn, rcspath, nameval, namevalNum, opts)
    RCSCONN conn;
    const char *rcspath;
    struct rcsNameVal nameval[];
    int namevalNum;
    struct rcsChangeHdrOpts *opts;
{
/* Routine to read or write header information */

	int i;
	char *ptr;
	const char *cptr;
	char *textfile;
	char const *branchsym, *commsyml;
	int branchflag, changed, expmode;
	static int initflag;
	int strictlock, strict_selected, textflag;
	int keepRCStime;
	size_t commsymlen;
	static struct buf branchnum;
	struct Lockrev *lockpt;
	struct Lockrev **curlock, **rmvlock;
        struct Status  * curstate;
	static char *obsRevisions;		/* rcs -o revisions */
	static bool has_been_called = false;	/* for first time init */
	struct rcsNameVal *nvptr;		/* arg currently being parsed */
	int lasterr;				/* last name/val arg error */
	int rc;

	nosetid();

	/* In case of internal core code error, return fatal error
	 * to caller, rather than exiting.
	 */
	if ((rc = sigsetjmp(rcsApiEnv, 1)) != 0) return (rc);

	/* Initialize variables for this invocation only */
	nextassoc = &assoclst;
	nextchaccess = &chaccess;
	nextmessage = &messagelst;
	nextstate = &statelst;
	branchsym = commsyml = textfile = 0;
	branchflag = strictlock = false;
	commsymlen = 0;
	curlock = &newlocklst;
	rmvlock = &rmvlocklst;
	expmode = -1;
	initflag= textflag = false;
        strict_selected = 0;
	lasterr = 0;
	changed = false;


	/* Initialize variables for duration of connection (unless overridden)*/
	if (!has_been_called) {	
            initflag= false;
	    rcsMailtxt = obsRevisions = NULL;
	    suppress_mail = false;
	    has_been_called = true;
	    bufautobegin(&branchnum);
	}

	/* Process per-connection options */
	if (opts) { /* handle options; else keep prevous settings/defaults */

	    /* Free old heap (which holds the old per-connection options */
	    rcsHeapFree(&optsHeap);

	    initflag = opts->initflag;
	    suppress_mail = opts->nomail;

	    if (rcsMailtxt) {
		rcsMailtxt = NULL;
	    }
	    if (opts->mailtxt) {
		i = strlen(opts->mailtxt);
		if (!(rcsMailtxt = rcsHeapAlloc(&optsHeap, i+2)))
		    faterror("Out of Memory");
		memcpy(rcsMailtxt, opts->mailtxt, i+1);

		/* Ensure that mail message ends with a newline */
		if (!i || rcsMailtxt[i-1] != '\n') {
		    rcsMailtxt[i++] = '\n';
		    rcsMailtxt[i] = '\0';
		}
	    }

	    if (obsRevisions) {
		obsRevisions = NULL;
	    }
	    if (opts->obsRevisions && *opts->obsRevisions) {
		char *ptr;
		size_t len;
		len = strlen(opts->obsRevisions);
		if (!(ptr = rcsHeapAlloc(&optsHeap, ++len)))
		    faterror("Out of Memory");
		memcpy(ptr, opts->obsRevisions, len);
		getdelrev(ptr);
	    }
	}

	/* Ugly kludge for rcs -Afile:
	 * Since the RCS code is structured to allow only one file
	 * open at a time, and memory mgmt is set up to free per-file
	 * memory once a file is closed, we need to deal with all other
	 * files before working with the real RCS file.  So we look to
	 * see whether we have an rcs -A operation (which requires parsing
	 * a different RCS file), and deal with that first.
	 */
	for (i = namevalNum, nvptr = nameval; i>0 ; i--, nvptr++) {
	    if ((nvptr->flags & rcsNameValFILE) &&
		(!nvptr->namespace || !nvptr->namespace[0]) &&
		( nvptr->name[0] == 'a' && !strcmp(nvptr->name, Kaccess)))
	    {
		    const char *val;
		        if (!(val = nvptr->val)) val = "";

			if (rcsParamName[0])
			     rcsCloseArch();  /* close old file */

			if (rcsOpenArch(false,true, val) < 0) {
			    lasterr = nvptr->errcode = ERR_BAD_NAME;
			    continue;
			}

			/* walk through other file's access list;
			 * add to list of logins to add to our RCS file
			 */
			while (AccessList) {
			    /* Save in second heap; cannot use fstr_save,
			     * since "other" RCS file now open.
			     */
			    size_t len = strlen(AccessList->login);
			    char *login = rcsHeapAlloc(&accessHeap, ++len);
			    if (!login) faterror("Out of Memory");
			    memcpy(login, AccessList->login, len);
			    getchaccess(login, append);
			    AccessList = AccessList->nextaccess;
			}
			/* Close other RCS file; clear rcsParamName (since it
			 * is used to know what RCS file is cur. open)
			 */
			Izclose(&finptr);
			rcsParamName[0] = '\0';
	    }
	}
		
	/* Open or reopen for update the archive file, depending upon
	 * whether it is already open (for read)
	 *
	 * Do this here, because some per-file argument processing
	 * uses per-file memory (ftalloc), which would otherwise
	 * be wiped out when the new file were opened.
	 */
	if (!strcmp(rcsParamName, rcspath)) {
	    if ((rc = rcsreopen(initflag ? false : true )) < 0) {
		 cleanup();
		 return rc;
	    }
	}

	else {
	    if (rcsParamName[0]) rcsCloseArch();  /* close different archive */
	    if((rc = rcsOpenArch(true, initflag ? false : true, rcspath)) <0) {
		cleanup();
		return rc;
	    }
	}
	    
	/* Process per-file arguments */
	for (i = namevalNum, nvptr = nameval; i>0 ; i--, nvptr++) {
	    if (!nvptr->namespace || !nvptr->namespace[0]) {
						/* RCS-defined field */
		const char *val;
		if (!(val = nvptr->val)) val = "";

		switch(nvptr->name[0]) {
		case 'a':	/* "access", "author" */

		    if (nvptr->revision) {		/* delta header */
			if (strcmp(nvptr->name, Kauthor)) {
			    lasterr = nvptr->errcode = ERR_BAD_NAME;
			    continue;
			}
			/* Attempt to change "author" field of delta */
			lasterr = nvptr->errcode = ERR_READONLY;
			continue;
		    }

		    /* admin header, with field beginning with 'a' */
		    if (strcmp(nvptr->name, Kaccess)) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }

		    /* rcs -a, rcs -A, rcs -e ("access" field) */
		    if (nvptr->flags & rcsNameValFILE) { /* rcs -A<val> */
			break;
		    }

		    if (nvptr->flags & rcsNameValDELETE) { /* rcs -e */
			if (rc = getaccessor(fstr_save(val), erase)) {
			    lasterr = rc;
			    continue;
			}
			break;
		    }

		    /* rcs -a */
		    if (rc = getaccessor(fstr_save(val), append)) {
		        lasterr = rc;
		        continue;
		    }
		    break;

		case 'b':	/* "branch", "branches" */

		    if (nvptr->revision) {		/* delta header */
			if (strcmp(nvptr->name, K_branches)) {
			    lasterr = nvptr->errcode = ERR_BAD_NAME;
			    continue;
			}
			/* Attempt to change "branches" field of delta */
			lasterr = nvptr->errcode = ERR_READONLY;
			continue;
		    }

		    /* admin header, with field beginning with 'b' */
		    if (strcmp(nvptr->name, Kbranch)) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }

		    /* rcs -b ("branch") field */
		    branchflag = true;
		    if (nvptr->flags & rcsNameValDELETE)
			branchsym = "";
		    else
		        branchsym = val;
		    break;

		case 'c':	/* "comment" */
		    /* if delta header, or admin field not "comment", err */
		    if (nvptr->revision || strcmp(nvptr->name, Kcomment)) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }

		    /* rcs -c ("comment") field */
		    if (nvptr->flags & rcsNameValDELETE) {
			commsyml = "";
			commsymlen = 0;
		    }
		    else {
			commsyml = val;
			commsymlen = strlen(commsyml);
		    }
		    break;

		case 'd':	/* "date", "desc" */
		{
		    size_t len;

		    /* admin header */
		    if (!nvptr->revision) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }

		    /* delta header */
		    if (!strcmp(nvptr->name, Kdate)) {	/* "date" */
			lasterr = nvptr->errcode = ERR_READONLY;
			continue;
		    }

		    if (strcmp(nvptr->name, Kdesc)) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }

		    /* rcs -t- ("desc") field */
		    textflag = true;
		    textfile = ftnalloc(char, (len = strlen(val))+2);
		    memcpy(textfile+1, val, len+1);
		    *textfile = '-';	/* -<string>; add rcsnameValFILE?? */
		    break;
		}

		case 'e':	/* "expand" */
		    /* if delta header, or admin field not "expand", err */
		    if (nvptr->revision || strcmp(nvptr->name, Kexpand)) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }

		    /* rcs -k ("expand") field */
		    if (nvptr->flags & rcsNameValDELETE) {
			expmode = str2expmode("kv");
		    }
		    else if ((rc = str2expmode(val)) < 0) {
			lasterr = nvptr->errcode = ERR_BAD_VALUE;
			continue;
		    }
		    else expmode = rc;
		    break;

		case 'h':	/* "head" */
		    /* if delta header, or admin field not "head", err */
		    if (nvptr->revision  || strcmp(nvptr->name, Khead)) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }

		    /* Attempt to change admin "head" field */
		    lasterr = nvptr->errcode = ERR_READONLY;
		    continue;

		case 'l':	/* "locks", "log" */
		    if (nvptr->revision) {		/* delta header */ 
			if (strcmp(nvptr->name, Klog)) {
			    lasterr = nvptr->errcode = ERR_BAD_NAME;
			    continue;
			}
			/* rcs -m ("log") field */
			if ((rc = getmessage(nvptr->revision,
						fstr_save(val))) < 0)
			{
			    lasterr = nvptr->errcode = rc;
			    continue;
			}
			break;
		    }

		    /* admin header */
		    if (strcmp(nvptr->name, Klocks)) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }
		    /* rcs -l, rcs -u ("locks") field */

		    /* <revno> or <login>:<revno> value, or NULL */
		    /* set ptr to point to <revno>, or NULL */

		    if (!*val) cptr = NULL;
		    else if (cptr = strchr(val, ':')) {
			++cptr; 		/* skip "<login>:" */
		    }
		    else cptr = val;

		    if (nvptr->flags & rcsNameValDELETE) {  /* rcs -u */
			if (!cptr || !*cptr) {
			    unlockcaller = true;
			    break;
			}

			/* Add lock to remove list */
			if (!cptr) cptr = val;
			*rmvlock = lockpt = ftalloc(struct Lockrev);
			lockpt->revno = cptr;
			lockpt->nextrev = 0;
			rmvlock = &lockpt->nextrev;
			curlock = rmnewlocklst(lockpt->revno);
			break;
		    }

		    /* rcs -l */
		    if (cptr && cptr != val) {/* <login>:<revno> */
			int j = strlen(author);
			if (strncmp(author, val, j) || val[j] != ':') {
			    /* attempt to add lock owned by someone else */
			    lasterr = nvptr->errcode = ERR_BAD_VALUE;
			    continue;
			}
		    }

		    if (!cptr || !*cptr) {
			/* Lock head or default branch */
			lockhead = true;
			break;
		    }
		    *curlock = lockpt = ftalloc(struct Lockrev);
		    lockpt->revno = cptr;
		    lockpt->nextrev = 0;
		    curlock = &lockpt->nextrev;
		    break;

		case 'n':	/* "next" */
		    /* if admin header, or delta field not "next", err */
		    if (!nvptr->revision || strcmp(nvptr->name, Knext)) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }

		    /* Attempt to change delta "next" field */
		    lasterr = nvptr->errcode = ERR_READONLY;
		    continue;
		    
		case 's':	/* "symbols", "strict", "state" */
		    if (nvptr->revision) {		 /* delta header */
			if (strcmp(nvptr->name, Kstate)) {
			    lasterr = nvptr->errcode = ERR_BAD_NAME;
			    continue;
			}
			/* rcs -s ("state") field */
			if ((rc = getstates(nvptr->revision,
						fstr_save(val))) < 0)
			{
			    lasterr = nvptr->errcode = rc;
			    continue;
			}
			break;
		    }

		    /* admin header */
		    if (!strcmp(nvptr->name, Kstrict)) {
			strict_selected = true;
			if (nvptr->flags & rcsNameValDELETE);	/* rcs -U */
			else {					/* rcs -L */
			   /* -L takes precidence */
			   strictlock = true;
			}
			break;
		    }

		    if (strcmp(nvptr->name, Ksymbols)) {
			lasterr = nvptr->errcode = ERR_BAD_NAME;
			continue;
		    }
   
		    /* rcs -n, rcs -N ("symbols") field */
		    ptr = fstr_save(val);
		    if (nvptr->flags & rcsNameValDELETE) {
			char *ptr2;
			/* make <name>:<rev> look like <name> for delete */
			if (ptr2 = strchr(ptr, ':')) *ptr2 = '\0';
		    }
		    if ((rc =
			  getassoclst(nvptr->flags & rcsNameValUNIQUE, ptr))
			< 0) {
			lasterr = nvptr->errcode = rc;
			continue;
		    }
		    break;

		default:
		    lasterr = nvptr->errcode = ERR_BAD_NAME;
		    continue;
		}

		nvptr->errcode = 0;
		continue;
	    } /* end of RCS-defined fields */

	    /* Set application block field */
	    if ((rc = addnameval(nvptr->revision, nvptr->namespace, nvptr->name,
			     nvptr->val,
			    ((nvptr->flags & rcsNameValUNIQUE) ? false : true)
		      )
		    ) < 0) {
		lasterr = nvptr->errcode = rc;
		continue;
	    }
	    changed = true;

	}  /* end of loop on values to set */

	/* Flags set per-session (not here):
	 * -I (interactive)
	 * -q (quiet)
	 * -x (suffixes)
	 */

#if 0	/* FIX: Do we allow telling the server the version of the files?
	 * If so, this is on a per-session basis.
	 * -V: setRCSversion(val);
	 */
#endif
#if 0
	/* FIX: Add timezone (-z) to per-session data.
	 * zone_set(val)
	 */	
#endif
	/*
	 * RCSname contains the name of the RCS file.
         * if !initflag, finptr contains the RILE *pointer for the
         * RCS file. The admin node is initialized.
         */

	diagnose("RCS file: %s\n", RCSname);

	changed |=  initflag | textflag;
	keepRCStime = Ttimeflag;
	if (!initflag) {
		if (!(checkaccesslist())) {
		    cleanup();
		    return ERR_NO_PERM;
		}
		gettree(); /* Read the delta tree.  */
	}

        /*  update admin. node    */
	if (strict_selected) {
		changed  |=  StrictLocks ^ strictlock;
		StrictLocks = strictlock;
	}
	if (
	    commsyml &&
	    (
		commsymlen != Comment.size ||
		memcmp(commsyml, Comment.string, commsymlen) != 0
	    )
	) {
		Comment.string = commsyml;
		Comment.size = strlen(commsyml);
		changed = true;
	}
	if (0 <= expmode  &&  Expand != expmode) {
		Expand = expmode;
		changed = true;
	}

        /* update default branch */
	if (branchflag && expandsym(branchsym, &branchnum)) {
	    if (countnumflds(branchnum.string)) {
		if (cmpnum(Dbranch, branchnum.string) != 0) {
			Dbranch = branchnum.string;
			changed = true;
		}
            } else
		if (Dbranch) {
			Dbranch = 0;
			changed = true;
		}
	}

	changed |= doaccess();	/* Update access list.  */

	changed |= doassoc();	/* Update association list.  */

	changed |= dolocks();	/* Update locks.  */

	changed |= domessages();	/* Update log messages.  */

        /*  update state attribution  */
        if (chgheadstate) {
            /* change state of default branch or head */
	    if (!Dbranch) {
		if (!Head)
		    rcswarn("can't change states in an empty tree");
		else if (strcmp(Head->state, headstate) != 0) {
		    Head->state = headstate;
		    changed = true;
		}
	    } else
		changed |= rcs_setstate(Dbranch,headstate);
        }
	for (curstate = statelst;  curstate;  curstate = curstate->nextstatus)
	    changed |= rcs_setstate(curstate->revno,curstate->status);

	cuthead = cuttail = 0;
	if (delrev.strt && removerevs()) {  /* implies -o; no delta() allowed */
            /*  rebuild delta tree if some deltas are deleted   */
            if ( cuttail )
		VOID genrevs(
			cuttail->num, (char *)0, (char *)0, (char *)0,
			&gendeltas
		);
            buildtree();
	    changed = true;
	    keepRCStime = false;
        }

	if (nerror) {
	    cleanup();
	    return -1;
	}

	/* Start rewrite of RCS file here */
	putadmin();
	if ( Head )
	    puttree(Head, frewrite);

	rcsNoStdin = true;
	putdesc(textflag, textfile);
	rcsNoStdin = false;

        if ( Head) {
	    if (delrev.strt || messagelst) {
		if (!cuttail || buildeltatext(gendeltas)) {
		    advise_access(finptr, MADV_SEQUENTIAL);
		    scanlogtext((struct hshentry *)0, false);
                    /* copy rest of delta text nodes that are not deleted    */
		    changed = true;
		}
            }
        }

	if (initflag) {
		/* Adjust things for donerewrite's sake.  */
		if (stat(workname, &RCSstat) != 0) {
#		    if bad_creat0
			mode_t m = umask(0);
			(void) umask(m);
			RCSstat.st_mode = (S_IRUSR|S_IRGRP|S_IROTH) & ~m;
#		    else
			changed = -1;
#		    endif
		}
		RCSstat.st_nlink = 0;
		keepRCStime = false;
	}
	
	if (donerewrite(changed,
		keepRCStime ? RCSstat.st_mtime : (time_t)-1
	) != 0) {
	    cleanup();
	    return ERR_INTERNAL;
	}

	cleanup();
	return lasterr ? (namevalNum == 1 ? lasterr : ERR_PARTIAL) : 0;
}       /* end of header (rcs) */



	static void
cleanup()
{
	if (nerror) exitstatus = EXIT_FAILURE;
	/* Izclose(&finptr); */
	/* Ozclose(&fcopy); */
	/* ORCSclose(); */
	/* dirtempunlink(); */
	rcsCloseArch();		  /* common cleanup (closing Arch) */
	rcsHeapFree(&accessHeap); /* cleanup -A heap used for this file */
}


	static int
getassoclst(flag, sp)
int     flag;
char    * sp;
/*  Function:   associate a symbolic name to a revision or branch,      */
/*              and store in assoclst                                   */

{
        struct   Symrev  * pt;
	char const *temp;
        int                c;

	while ((c = *++sp) == ' ' || c == '\t' || c =='\n')
	    continue;
        temp = sp;
	if (!(sp = checksym(sp, ':')))  /*  check for invalid symbolic name  */
	    return ERR_BAD_VALUE;

	c = *sp;   *sp = '\0';
        while( c == ' ' || c == '\t' || c == '\n')  c = *++sp;

        if ( c != ':' && c != '\0') {
	    error("invalid string %s after option -n or -N",sp);
            return ERR_BAD_VALUE;
        }

	pt = talloc(struct Symrev);
        pt->ssymbol = temp;
        pt->override = flag;
	if (c == '\0')  /*  delete symbol  */
	    pt->revno = 0;
        else {
	    while ((c = *++sp) == ' ' || c == '\n' || c == '\t')
		continue;
	    pt->revno = sp;
        }
	pt->nextsym = 0;
	*nextassoc = pt;
	nextassoc = &pt->nextsym;
	return 0;
}


	static void
getchaccess(login, command)
	char const *login;
	enum changeaccess command;
{
	register struct chaccess *pt;

	if (!(pt = rcsHeapAlloc(&accessHeap, sizeof(struct chaccess))))
	    faterror("Out of Memory");
	pt->login = login;
	pt->command = command;
	pt->nextchaccess = 0;
	*nextchaccess = pt;
	nextchaccess = &pt->nextchaccess;
}



	static int
getaccessor(opt, command)
	char *opt;
	enum changeaccess command;
/*   Function:  get the accessor list of options -e and -a,     */
/*		and store in chaccess				*/


{
        register c;
	register char *sp;

	sp = opt;
	while ((c = *++sp) == ' ' || c == '\n' || c == '\t' || c == ',')
	    continue;
        if ( c == '\0') {
	    if (command == erase  &&  sp-opt == 1) {
		getchaccess((char*)0, command);
		return 0;
	    }
	    error("missing login name after option -a or -e");
	    return ERR_APPL_ARGS;
        }

        while( c != '\0') {
		getchaccess(sp, command);
		if (!(sp = checkid(sp,','))) return ERR_APPL_ARGS;
		c = *sp;   *sp = '\0';
                while( c == ' ' || c == '\n' || c == '\t'|| c == ',')c =(*++sp);
        }
	return 0;
}

	static int
getmessage(revno, option)
	const char *revno;
	char *option;
{
	struct Message *pt;
	struct cbuf cb;
	cb = cleanlogmsg(option, strlen(option));
	if (!cb.size) return ERR_BAD_VALUE;
	pt = ftalloc(struct Message);
	pt->revno = revno;
	pt->message = cb;
	*nextmessage = pt;
	nextmessage = &pt->nextmessage;
	return 0;
}

	static int
getstates(revno, sp)
	const char *revno;
	char *sp;
{
	char const *temp;
	struct Status	*pt;
	register	c;

	/* Skip lead whitespace */
	for (c = *sp; c == ' ' || c == '\t' || c == '\n'; c = *++sp);
	temp = sp;

	if (!(sp = checkid(sp,':')) || *sp == ':') 
				/* check for invalid state attribute */
	    return ERR_BAD_VALUE;

	/* Trim trailing whitespace */
	c = *sp; *sp = '\0';
	while (c == ' ' || c == '\t' || c == '\n' ) c == *++sp;
	if (c) return ERR_BAD_VALUE;

	/* temp points to a trimmed state value (no whitespace) */

	if (!revno || !*revno) {
	    chgheadstate = true;
	    headstate = temp;
	    return 0;
	}

	/* Skip lead whitespace (note that trailing whitespace is left
	 * in revno; this reproduces original logic
	 */
	for (c = *revno; c == ' ' || c == '\t' || c == '\n'; c = *++revno);
	pt = ftalloc(struct Status);
	pt->status = temp;
	pt->revno = revno;
	pt->nextstatus = 0;
	*nextstate = pt;
	nextstate = &pt->nextstatus;
	return 0;
}

	static void
getdelrev(sp)
char    *sp;
/*   Function:  get revision range or branch to be deleted,     */
/*              and place in delrev                             */
{
        int    c;
        struct  delrevpair      *pt;
	int separator;

	pt = &delrev;

	while ((c = (*++sp)) == ' ' || c == '\n' || c == '\t')
		continue;

	/* Support old ambiguous '-' syntax; this will go away.  */
	if (strchr(sp,':'))
		separator = ':';
	else {
		if (strchr(sp,'-')  &&  VERSION(5) <= RCSversion)
		    warn("`-' is obsolete in `-o%s'; use `:' instead", sp);
		separator = '-';
	}

	if (c == separator) { /* -o:rev */
	    while ((c = (*++sp)) == ' ' || c == '\n' || c == '\t')
		continue;
            pt->strt = sp;    pt->code = 1;
            while( c != ' ' && c != '\n' && c != '\t' && c != '\0') c =(*++sp);
            *sp = '\0';
	    pt->end = 0;
            return;
        }
        else {
            pt->strt = sp;
            while( c != ' ' && c != '\n' && c != '\t' && c != '\0'
		   && c != separator )  c = *++sp;
            *sp = '\0';
            while( c == ' ' || c == '\n' || c == '\t' )  c = *++sp;
            if ( c == '\0' )  {  /*   -o rev or branch   */
		pt->code = 0;
		pt->end = 0;
                return;
            }
	    if (c != separator) {
		error("invalid range %s %s after -o", pt->strt, sp);
            }
	    while ((c = *++sp) == ' ' || c == '\n' || c == '\t')
		continue;
	    if (!c) {  /* -orev: */
		pt->code = 2;
		pt->end = 0;
                return;
            }
        }
	/* -orev1:rev2 */
	pt->end = sp;  pt->code = 3;
        while( c!= ' ' && c != '\n' && c != '\t' && c != '\0') c = *++sp;
        *sp = '\0';
}




	static void
scanlogtext(delta,edit)
	struct hshentry *delta;
	int edit;
/* Function: Scans delta text nodes up to and including the one given
 * by delta, or up to last one present, if !delta.
 * For the one given by delta (if delta), the log message is saved into
 * delta->log if delta==cuttail; the text is edited if EDIT is set, else copied.
 * Assumes the initial lexeme must be read in first.
 * Does not advance nexttok after it is finished, except if !delta.
 */
{
	struct hshentry const *nextdelta;
	struct cbuf cb;

	for (;;) {
		foutptr = 0;
		if (eoflex()) {
                    if(delta)
			rcsfaterror("can't find delta for revision %s",
				delta->num
			);
		    return; /* no more delta text nodes */
                }
		nextlex();
		if (!(nextdelta=getnum()))
			fatserror("delta number corrupted");
		if (nextdelta->selector) {
			foutptr = frewrite;
			aprintf(frewrite,DELNUMFORM,nextdelta->num,Klog);
                }
		getkeystring(Klog);
		if (nextdelta == cuttail) {
			cb = savestring(&curlogbuf);
			if (!delta->log.string)
			    delta->log = cleanlogmsg(curlogbuf.string, cb.size);
			nextlex();
			delta->igtext = getphrases(Ktext);
		} else {
			if (nextdelta->log.string && nextdelta->selector) {
				foutptr = 0;
				readstring();
				foutptr = frewrite;
				putstring(foutptr, false, nextdelta->log, true);
				afputc(nextc, foutptr);
			} else
				readstring();
			ignorephrases(Ktext);
		}
		getkeystring(Ktext);

		if (delta==nextdelta)
			break;
		readstring(); /* skip over it */

	}
	/* got the one we're looking for */
	if (edit)
		editstring((struct hshentry*)0);
	else
		enterstring();
}


	static struct Lockrev **
rmnewlocklst(which)
	char const *which;
/* Remove lock to revision WHICH from newlocklst.  */
{
	struct Lockrev *pt, **pre;

	pre = &newlocklst;
	while ((pt = *pre))
	    if (strcmp(pt->revno, which) != 0)
		pre = &pt->nextrev;
	    else {
		*pre = pt->nextrev;
		/* tfree(pt); */
	    }
        return pre;
}



	static int
doaccess()
{
	register struct chaccess *ch;
	register struct access **p, *t;
	register int changed = false;

	for (ch = chaccess;  ch;  ch = ch->nextchaccess) {
		switch (ch->command) {
		case erase:
			if (!ch->login) {
			    if (AccessList) {
				AccessList = 0;
				changed = true;
			    }
			} else
			    for (p = &AccessList; (t = *p); p = &t->nextaccess)
				if (strcmp(ch->login, t->login) == 0) {
					*p = t->nextaccess;
					changed = true;
					break;
				}
			break;
		case append:
			for (p = &AccessList;  ;  p = &t->nextaccess)
				if (!(t = *p)) {
					*p = t = ftalloc(struct access);
					t->login = ch->login;
					t->nextaccess = 0;
					changed = true;
					break;
				} else if (strcmp(ch->login, t->login) == 0)
					break;
			break;
		}
	}
	return changed;
}


	static struct hshentry *
searchcutpt(object, length, store)
	char const *object;
	int length;
	struct hshentries *store;
/*   Function:  Search store and return entry with number being object. */
/*		cuttail = 0, if the entry is Head; otherwise, cuttail   */
/*              is the entry point to the one with number being object  */

{
	cuthead = 0;
	while (compartial(store->first->num, object, length)) {
		cuthead = store->first;
		store = store->rest;
	}
	return store->first;
}



	static int
branchpoint(strt, tail)
struct  hshentry        *strt,  *tail;
/*   Function: check whether the deltas between strt and tail	*/
/*		are locked or branch point, return 1 if any is  */
/*		locked or branch point; otherwise, return 0 and */
/*		mark deleted					*/

{
        struct  hshentry    *pt;
	struct rcslock const *lockpt;

	for (pt = strt;  pt != tail;  pt = pt->next) {
            if ( pt->branches ){ /*  a branch point  */
		rcserror("can't remove branch point %s", pt->num);
		return true;
            }
	    for (lockpt = Locks;  lockpt;  lockpt = lockpt->nextlock)
		if (lockpt->delta == pt) {
		    rcserror("can't remove locked revision %s", pt->num);
		    return true;
		}
	    pt->selector = false;
	    diagnose("deleting revision %s\n",pt->num);
        }
	return false;
}



	static int
removerevs()
/*   Function:  get the revision range to be removed, and place the     */
/*              first revision removed in delstrt, the revision before  */
/*		delstrt in cuthead (0, if delstrt is head), and the	*/
/*		revision after the last removed revision in cuttail (0	*/
/*              if the last is a leaf                                   */

{
	struct  hshentry *target, *target2, *temp;
	int length;
	int cmp;

	if (!expandsym(delrev.strt, &numrev)) return 0;
	target = genrevs(numrev.string,(char*)0,(char*)0,(char*)0,&gendeltas);
        if ( ! target ) return 0;
	cmp = cmpnum(target->num, numrev.string);
	length = countnumflds(numrev.string);

	if (delrev.code == 0) {  /*  -o  rev    or    -o  branch   */
	    if (length & 1)
		temp=searchcutpt(target->num,length+1,gendeltas);
	    else if (cmp) {
		rcserror("Revision %s doesn't exist.", numrev.string);
		return 0;
	    }
	    else
		temp = searchcutpt(numrev.string, length, gendeltas);
	    cuttail = target->next;
            if ( branchpoint(temp, cuttail) ) {
		cuttail = 0;
                return 0;
            }
            delstrt = temp;     /* first revision to be removed   */
            return 1;
        }

	if (length & 1) {   /*  invalid branch after -o   */
	    rcserror("invalid branch range %s after -o", numrev.string);
            return 0;
        }

	if (delrev.code == 1) {  /*  -o  -rev   */
            if ( length > 2 ) {
                temp = searchcutpt( target->num, length-1, gendeltas);
                cuttail = target->next;
            }
            else {
                temp = searchcutpt(target->num, length, gendeltas);
                cuttail = target;
                while( cuttail && ! cmpnumfld(target->num,cuttail->num,1) )
                    cuttail = cuttail->next;
            }
            if ( branchpoint(temp, cuttail) ){
		cuttail = 0;
                return 0;
            }
            delstrt = temp;
            return 1;
        }

	if (delrev.code == 2) {   /*  -o  rev-   */
            if ( length == 2 ) {
                temp = searchcutpt(target->num, 1,gendeltas);
		if (cmp)
                    cuttail = target;
                else
                    cuttail = target->next;
            }
            else  {
		if (cmp) {
                    cuthead = target;
                    if ( !(temp = target->next) ) return 0;
                }
                else
                    temp = searchcutpt(target->num, length, gendeltas);
		getbranchno(temp->num, &numrev);  /* get branch number */
		VOID genrevs(numrev.string, (char*)0, (char*)0, (char*)0, &gendeltas);
            }
            if ( branchpoint( temp, cuttail ) ) {
		cuttail = 0;
                return 0;
            }
            delstrt = temp;
            return 1;
        }

        /*   -o   rev1-rev2   */
	if (!expandsym(delrev.end, &numrev)) return 0;
	if (
		length != countnumflds(numrev.string)
	    ||	(length>2 && compartial(numrev.string, target->num, length-1))
	) {
	    rcserror("invalid revision range %s-%s",
		target->num, numrev.string
	    );
            return 0;
        }

	target2 = genrevs(numrev.string,(char*)0,(char*)0,(char*)0,&gendeltas);
        if ( ! target2 ) return 0;

        if ( length > 2) {  /* delete revisions on branches  */
            if ( cmpnum(target->num, target2->num) > 0) {
		cmp = cmpnum(target2->num, numrev.string);
                temp = target;
                target = target2;
                target2 = temp;
            }
	    if (cmp) {
                if ( ! cmpnum(target->num, target2->num) ) {
		    rcserror("Revisions %s-%s don't exist.",
			delrev.strt, delrev.end
		    );
                    return 0;
                }
                cuthead = target;
                temp = target->next;
            }
            else
                temp = searchcutpt(target->num, length, gendeltas);
            cuttail = target2->next;
        }
        else { /*  delete revisions on trunk  */
            if ( cmpnum( target->num, target2->num) < 0 ) {
                temp = target;
                target = target2;
                target2 = temp;
            }
            else
		cmp = cmpnum(target2->num, numrev.string);
	    if (cmp) {
                if ( ! cmpnum(target->num, target2->num) ) {
		    rcserror("Revisions %s-%s don't exist.",
			delrev.strt, delrev.end
		    );
                    return 0;
                }
                cuttail = target2;
            }
            else
                cuttail = target2->next;
            temp = searchcutpt(target->num, length, gendeltas);
        }
        if ( branchpoint(temp, cuttail) )  {
	    cuttail = 0;
            return 0;
        }
        delstrt = temp;
        return 1;
}



	static int
doassoc()
/* Add or delete (if !revno) association that is stored in assoclst.  */
{
	char const *p;
	int changed = false;
	struct Symrev const *curassoc;
	struct assoc **pre, *pt;

        /*  add new associations   */
	for (curassoc = assoclst;  curassoc;  curassoc = curassoc->nextsym) {
	    char const *ssymbol = curassoc->ssymbol;

	    if (!curassoc->revno) {  /* delete symbol  */
		for (pre = &Symbols;  ;  pre = &pt->nextassoc)
		    if (!(pt = *pre)) {
			rcswarn("can't delete nonexisting symbol %s", ssymbol);
			break;
		    } else if (strcmp(pt->symbol, ssymbol) == 0) {
			*pre = pt->nextassoc;
			changed = true;
			break;
		    }
	    }
	    else {
		if (curassoc->revno[0]) {
		    p = 0;
		    if (expandsym(curassoc->revno, &numrev))
			p = fstr_save(numrev.string);
		} else if (!(p = tiprev()))
		    rcserror("no latest revision to associate with symbol %s",
			    ssymbol
		    );
		if (p)
		    changed |= addsymbol(p, ssymbol, curassoc->override);
	    }
        }
	return changed;
}



	static int
dolocks()
/* Function: remove lock for caller or first lock if unlockcaller is set;
 *           remove locks which are stored in rmvlocklst,
 *           add new locks which are stored in newlocklst,
 *           add lock for Dbranch or Head if lockhead is set.
 */
{
	struct Lockrev const *lockpt;
	struct hshentry *target;
	int changed = false;

	if (unlockcaller) { /*  find lock for caller  */
            if ( Head ) {
		if (Locks) {
		    switch (findlock(true, &target)) {
		      case 0:
			/* remove most recent lock (it isn't owned by caller)*/
			changed |= 
				(checklock(Locks->delta->num,true,false,0) > 0);
			break;
		      case 1:
			diagnose("%s unlocked\n",target->num);
			changed = true;
			break;
		    }
		} else {
		    rcswarn("No locks are set.");
		}
            } else {
		rcswarn("can't unlock an empty tree");
            }
        }

        /*  remove locks which are stored in rmvlocklst   */
	for (lockpt = rmvlocklst;  lockpt;  lockpt = lockpt->nextrev)
	    if (expandsym(lockpt->revno, &numrev)) {
		target = genrevs(numrev.string, (char *)0, (char *)0, (char *)0, &gendeltas);
                if ( target )
		   if (!(countnumflds(numrev.string)&1) && cmpnum(target->num,numrev.string))
			rcserror("can't unlock nonexisting revision %s",
				lockpt->revno
			);
                   else
			changed |= (checklock(target->num,true,false,0) > 0);
                        /* checklock does its own diagnose */
            }

        /*  add new locks which stored in newlocklst  */
	for (lockpt = newlocklst;  lockpt;  lockpt = lockpt->nextrev)
	    changed |= setlock(lockpt->revno);

	if (lockhead) /*  lock default branch or head  */
	    if (Dbranch)
		changed |= setlock(Dbranch);
	    else if (Head)
		changed |= setlock(Head->num);
	    else
		rcswarn("can't lock an empty tree");
	return changed;
}



	static int
setlock(rev)
	char const *rev;
/* Function: Given a revision or branch number, finds the corresponding
 * delta and locks it for caller.
 */
{
        struct  hshentry *target;
	int r;

	if (expandsym(rev, &numrev)) {
	    target = genrevs(numrev.string, (char*)0, (char*)0,
			     (char*)0, &gendeltas);
            if ( target )
	       if (!(countnumflds(numrev.string)&1) && cmpnum(target->num,numrev.string))
		    rcserror("can't lock nonexisting revision %s",
			numrev.string
		    );
	       else {
		    if ((r = addlock(target, false)) < 0  && 
			     checklock(target->num,true,false,0) > 0)
			r = addlock(target, true);
		    if (0 <= r) {
			if (r)
			    diagnose("%s locked\n", target->num);
			return r;
		    }
	       }
	}
	return 0;
}


	static int
domessages()
{
	struct hshentry *target;
	struct Message *p;
	int changed = false;

	for (p = messagelst;  p;  p = p->nextmessage)
	    if (
		expandsym(p->revno, &numrev)  &&
		(target = genrevs(
			numrev.string, (char*)0, (char*)0, (char*)0, &gendeltas
		))
	    ) {
		/*
		 * We can't check the old log -- it's much later in the file.
		 * We pessimistically assume that it changed.
		 */
		target->log = p->message;
		changed = true;
	    }
	return changed;
}


	static int
rcs_setstate(rev,status)
	char const *rev, *status;
/* Function: Given a revision or branch number, finds the corresponding delta
 * and sets its state to status.
 */
{
        struct  hshentry *target;

	if (expandsym(rev, &numrev)) {
	    target = genrevs(numrev.string, (char*)0, (char*)0,
			     (char*)0, &gendeltas);
            if ( target )
	       if (!(countnumflds(numrev.string)&1) && cmpnum(target->num,numrev.string))
		    rcserror("can't set state of nonexisting revision %s",
			numrev.string
		    );
	       else if (strcmp(target->state, status) != 0) {
                    target->state = status;
		    return true;
	       }
	}
	return false;
}





	static int
buildeltatext(deltas)
	struct hshentries const *deltas;
/*   Function:  put the delta text on frewrite and make necessary   */
/*              change to delta text                                */
{
	register FILE *fcut;	/* temporary file to rebuild delta tree */
	char const *cutname;

	fcut = 0;
	cuttail->selector = false;
	scanlogtext(deltas->first, false);
        if ( cuthead )  {
	    cutname = maketemp(3);
	    if (!(fcut = fopenSafer(cutname, FOPEN_WPLUS_WORK))) {
		efaterror(cutname);
            }

	    while (deltas->first != cuthead) {
		deltas = deltas->rest;
		scanlogtext(deltas->first, true);
            }

	    snapshotedit(fcut);
	    Orewind(fcut);
	    aflush(fcut);
        }

	while (deltas->first != cuttail)
	    scanlogtext((deltas = deltas->rest)->first, true);
	finishedit((struct hshentry*)0, (FILE*)0, true);
	/* Ozclose(&fcopy); - closed in CloseRcsDataFile */

	if (fcut) {
	    char const *diffname = maketemp(0);
	    char const *diffv[6 + !!OPEN_O_BINARY];
	    char const **diffp = diffv;
	    *++diffp = DIFF;
	    *++diffp = DIFFFLAGS;
#	    if OPEN_O_BINARY
		if (Expand == BINARY_EXPAND)
		    *++diffp == "--binary";
#	    endif
	    *++diffp = "-";
	    *++diffp = resultname;
	    *++diffp = 0;
	    switch (runv(fileno(fcut), diffname, diffv)) {
		case DIFF_FAILURE: case DIFF_SUCCESS: break;
		default: rcsfaterror("diff failed");
	    }
	    Ofclose(fcut);
	    return putdtext(cuttail,diffname,frewrite,true);
	} else
	    return putdtext(cuttail,resultname,frewrite,false);
}



	static void
buildtree()
/*   Function:  actually removes revisions whose selector field  */
/*		is false, and rebuilds the linkage of deltas.	 */
/*              asks for reconfirmation if deleting last revision*/
{
	struct	hshentry   * Delta;
        struct  branchhead      *pt, *pre;

        if ( cuthead )
           if ( cuthead->next == delstrt )
                cuthead->next = cuttail;
           else {
                pre = pt = cuthead->branches;
                while( pt && pt->hsh != delstrt )  {
                    pre = pt;
                    pt = pt->nextbranch;
                }
                if ( cuttail )
                    pt->hsh = cuttail;
                else if ( pt == pre )
                    cuthead->branches = pt->nextbranch;
                else
                    pre->nextbranch = pt->nextbranch;
            }
	else {
#ifdef REMOVE
	    if (!cuttail && !quietflag) {
		if (!yesorno(false, "Do you really want to delete all revisions? [ny](n): ")) {
		    rcserror("No revision deleted");
		    Delta = delstrt;
		    while( Delta) {
			Delta->selector = true;
			Delta = Delta->next;
		    }
		    return;
		}
	    }
#endif /* REMOVE */
            Head = cuttail;
	}
        return;
}

#if RCS_lint
/* This lets us lint everything all at once. */

char const *cmdid = "";

#define go(p,e) {int p P((int,char**)); void e P((void)); if(*argv)return p(argc,argv);if(*argv[1])e();}

	int
main(argc, argv)
	int argc;
	char **argv;
{
	go(ciId,	ciExit);
	go(coId,	coExit);
	go(identId,	identExit);
	go(mergeId,	mergeExit);
	go(rcsId,	exiterr);
	go(rcscleanId,	rcscleanExit);
	go(rcsdiffId,	rdiffExit);
	go(rcsmergeId,	rmergeExit);
	go(rlogId,	rlogExit);
	return 0;
}
#endif

#ifdef MAIN

void interrupt(context)
	void *context;
{
	printf("got an interrupt\n");
}

/* test program for pairnames() and getfullRCSname() */

main(argc, argv)
int argc; char *argv[];
{
        int result;
	struct RcsDataFile *rcsfileptr;
	struct RcsSessionState *state;
	struct RcsSession *sess;
	struct RcsHeaderOpts *hdropts;

	/* Create objects to hold arguments */
	state = CreateRcsSessionState();
	state->cmdid = "rcs";			/* name of command (for
						 * use in error messages)
						 */
	hdropts = CreateRcsHeaderOpts();

        while(--argc, ++argv, argc>=1 && ((*argv)[0] == '-')) {
                switch ((*argv)[1]) {

                case 'q':       state->quietflag=true;
                                break;
		case 'I':	state->interactive=true;
				break;
		case 'w':	if (!argv[0][2]) error("-w missing author");
				else state->auth = argv[0]+2;
				break;
		case 'x':	state->suffixes = argv[0]+2;
				break;
                case 'i':       hdropts->initflag=true;
                                break;
		case 'L':	hdropts->strictflag=true;
				break;
		case 'U':	hdropts->nostrictflag=true;
				break;
		case 'M':	hdropts->mailflag=true;
				break;
		case 'T':	hdropts->Timeflag=true;
				break;
		case 'b':	hdropts->branchflag = argv[0]+2;
				break;
		case 'c':	hdropts->commentflag = argv[0]+2;
				break;
		case 'a':	hdropts->add_accessflag = argv[0]+2;
				break;
		case 'A':	hdropts->append_accessflag = argv[0]+2;
				break;
		case 'e':	hdropts->erase_accessflag = argv[0]+2;
				break;
		case 'l':	hdropts->lockflag = argv[0]+2;
				break;
		case 'u':	hdropts->unlockflag = argv[0]+2;
				break;
		case 'n':	hdropts->nameflag = argv[0]+2;
				break;
		case 'N':	hdropts->Nameflag = argv[0]+2;
				break;
		case 'm':	hdropts->messageflag = argv[0]+2;
				break;
		case 'o':	hdropts->delrevflag = argv[0]+2;
				break;
		case 's':	hdropts->stateflag = argv[0]+2;
				break;
		case 'k':	hdropts->keyexpandflag = argv[0]+2;
				break;
                default:        error("unknown option: %s", *argv);
                                break;
                }
        }

	if (!(sess = CreateRcsSession(state, "bonnie", "rcssrv", NULL,
				      NULL, NULL, interrupt, NULL))) {
		printf("Cannot create session\n");
		exit(1);
	}

	if ((sess->openSession)(sess)) {
		printf("Cannot open session\n");
		exit(1);
	}

        for(;--argc>=0;++argv) {
		char *base;
		char *path;
		if ((base = strrchr(argv[0], '/'))) {
			*base++ = '\0';
			path = argv[0];
		}
		else {
			base = argv[0];
			path = 0;
		}

		/* Zero means error; -1 is returned if file doesn't exist,
		 * but we are initializing it (which is okay)
		 */
		if ((result = OpenRcsDataFile(sess,&rcsfileptr,base,path,true, 
				    hdropts->initflag ? false : true)) == 0) {
		    printf("Unable to locate/open RCS file\n");
		    exit(2);
		}
		if (result > 0 && hdropts->initflag) {
		    printf("RCS file for %s already exists\n", base);
		}
		else header(sess, rcsfileptr, hdropts);

		CloseRcsDataFile(sess, rcsfileptr);

        } 
	/* Terminate Session */
	(sess->destroy)(sess);
}
#endif /* MAIN */
