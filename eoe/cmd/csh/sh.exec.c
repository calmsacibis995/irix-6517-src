/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.6 $"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.

		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley Software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <pfmt.h>
#include "sh.h"
#include "sh.wconst.h"


/*
 * C shell
 */

/*
 * System level search and execute of a command.
 * We look in each directory for the specified command name.
 * If the name contains a '/' then we execute only the full path name.
 * If there is no search path then we execute only full path names.
 */

/* 
 * As we search for the command we note the first non-trivial error
 * message for presentation to the user.  This allows us often
 * to show that a file has the wrong mode/no access when the file
 * is not in the last component of the search path, so we must
 * go on after first detecting the error.
 */
char	*exerr;			/* Execution error message */
wchar_t	*expath;		/* Path for exerr */

/*
 * Xhash is an array of HSHSIZ bits (HSHSIZ / 8 chars), which are used
 * to hash execs.  If it is allocated (havhash true), then to tell
 * whether ``name'' is (possibly) present in the i'th component
 * of the variable path, you look at the bit in xhash indexed by
 * hash(hashname("name"), i).  This is setup automatically
 * after .login is executed, and recomputed whenever ``path'' is
 * changed.
 * The two part hash function is designed to let texec() call the
 * more expensive hashname() only once and the simple hash() several
 * times (once for each path component checked).
 * Byte size is assumed to be 8.
 */
#define	HSHSIZ		8192			/* 1k bytes */
#define HSHMASK		(HSHSIZ - 1)
#define HSHMUL		243

char	xhash[HSHSIZ / 8];

#define hash(a, b)	((a) * HSHMUL + (b) & HSHMASK)
#define bit(h, b)	((h)[(b) >> 3] & 1 << ((b) & 7))	/* bit test */
#define bis(h, b)	((h)[(b) >> 3] |= 1 << ((b) & 7))	/* bit set */

static int	hashname(wchar_t *);
static void	pexerr(void);
static void	texec(wchar_t *, wchar_t **);

/*
 * echo it
 */
void
xechoit(wchar_t **t)
{
#ifdef TRACE
	tprintf("TRACE- xechoit()\n");
#endif
	if(adrof(S_echo)) {
	    flush();
	    haderr = 1;
	    blkpr(t); shprintf("\n");
	    haderr = 0;
	}
}

/*
 * exec error
 */
static void
pexerr(void)
{
#ifdef TRACE
	tprintf("TRACE- pexerr()\n");
#endif
	/*
	 * Couldn't find the damn thing
	 */
	setname(expath);
	/* xfree(expath); */
	if(exerr)
	    bferr(exerr);
	bferr(gettxt(_SGI_DMMX_csh_cmdnotfnd, "Command not found"));
}

/*
 * Dummy search path for just absolute search when no path
 */
wchar_t	*justabs[] = { S_, 0 };

void
doexec(struct command *t)
{
	register wchar_t *dp, **pv, **av;
	register struct varent *v;
	wchar_t *sav;
	bool slash = any('/', t->t_dcom[0]);
	int hashval, i, hashval1;
	wchar_t *blk[2];

#ifdef TRACE
	tprintf("TRACE- doexec()\n");
#endif

	/*
	 * Glob the command name.  If this does anything, then we
	 * will execute the command only relative to ".".  One special
	 * case: if there is no PATH, then we execute only commands
	 * which start with '/'.
	 */
	dp = globone(t->t_dcom[0]);
	sav = t->t_dcom[0];
	exerr = 0; expath = t->t_dcom[0] = dp;
	xfree(sav);
	v = adrof(S_path);
	if( !v && (expath[0] != '/'))
	    pexerr();
	slash |= gflag;

	/*
	 * Glob the argument list, if necessary.
	 * Otherwise trim off the quote bits.
	 */
	gflag = 0; av = &t->t_dcom[1];
	tglob(av);
	if(gflag) {
	    av = glob(av);
	    if( !av)
		err_nomatch();
	}
	blk[0] = t->t_dcom[0];
	blk[1] = 0;
	av = blkspl(blk, av);
	trim(av);
	xechoit(av);			/* Echo command if -x */

	/*
	 * Since all internal file descriptors are set to close on exec,
	 * we don't need to close them explicitly here.  Just reorient
	 * ourselves for error messages.
	 */
	SHIN = 0; SHOUT = 1; SHDIAG = 2; OLDSTD = 0;

	/*
	 * We must do this AFTER any possible forking (like `foo`
	 * in glob) so that this shell can still do subprocesses.
	 */
	(void)sigrelse(SIGCHLD);
	(void)sigrelse(SIGINT);

	/*
	 * If no path, no words in path, or a / in the filename
	 * then restrict the command search.
	 */
	if( !v || !v->vec[0] || slash)
	    pv = justabs;
	else
	    pv = v->vec;
	sav = strspl(S_SLASH, *av);	/* / command name for postpending */

	if(havhash)
	    hashval = hashname(*av);
	i = 0;

	do {
	    	if (!slash && pv[0][0] == '/' && havhash) {
	    		hashval1 = hash(hashval, i);
	    		if (!bit(xhash, hashval1))
	    			goto cont;
	    	}
	    

	    if( !pv[0][0] || eq(pv[0], S_DOT)) {
		texec(*av, av);				/* don't make ./xxx */
	    } else {
		dp = strspl(*pv, sav);
		texec(dp, av);
		xfree(dp);
	    }
cont:
	    pv++;
	    i++;
	} while(*pv);
	xfree(sav);
	xfree(av);			/* free memory */

	pexerr();
}


/*
 * Execute command f, arg list t.
 * Record error message if not found.
 * Also do shell scripts here.
 */
static void
texec(wchar_t *f, wchar_t **t)
{
	register struct varent *v;
	register wchar_t **vp;
	wchar_t *lastsh[2];
	
	/* for execv interface */

	char *f_str;		/* for the first argument */
	wchar_t **ot;		/* save the original pointer */
	char **t_str, **ot_str;	/* for the second argunebt */
	int len;		/* length of the second argument */
	int i;			/* work */

	char chbuf[CSHBUFSIZ * MB_LEN_MAX];

#ifdef TRACE
	tprintf("TRACE- texec()\n");
#endif

	ot = t;				/* save the original pointer */
	f_str = tstostr(NOSTR, f, 0);	/* allocate the first argument */
	len = blklen(t);		/* get the length */

	ot_str = t_str = (char **)salloc(len + 1, sizeof(char **));
	for(i = 0; i < len; i++) {
	    *ot_str++ = tstostr(NOSTR, *ot++, 0);
	}
	*ot_str = 0;
	execv(f_str, t_str);		/* exec the command */

	/*
	 * exec failed, free the area
	 */
	xfree(f_str);			/* Free the first */
	ot_str = t_str;
	for(i = 0; i < len; i++) {	/* Free the second list */
	    xfree(*ot_str++);
	}
	xfree(t_str);			/* Free the holder */

	/*
	 * check error
	 */
	switch(errno) {
	    case ENOEXEC:
		/*
		 * check that this is not a binary file
		 */
		{       
		    register int ff = open_(f, O_RDONLY);
		    wchar_t ch;

		    if(ff != -1
			&& (read_(ff, &ch, 1) == 1)
			&& !isprint(ch)
			&& !isspace(ch)) {
			    haderr = 1;
			    showstr(MM_ERROR,
				gettxt(_SGI_MMX_csh_cantexec,
				    "Cannot execute binary file"),
				0);
			    Perror(f);
			    haderr = 0;
			    (void)close(ff);
			    return;
		    } 
		    (void) close(ff);
		} 
		/*
		 * If there is an alias for shell, then
		 * put the words of the alias in front of the
		 * argument list replacing the command name.
		 * Note no interpretation of the words at this point.
		 */
		v = adrof1(S_shell, &aliases);
		if( !v) {
#ifdef OTHERSH
		    register int ff = open_(f, 0);
		    wchar_t ch;
#endif
		    vp = lastsh;
		    vp[0] = adrof(S_shell)? value(S_shell) : S_SHELLPATH;
		    vp[1] = NOSTR;
#ifdef OTHERSH
		    if ((ff != -1)
			 && (read_(ff, &ch, 1) == 1)
			 && (ch != '#'))
			     vp[0] = S_OTHERSH;
		    (void)close(ff);
#endif
		} else
		    vp = v->vec;

		t[0] = f;
		t = blkspl(vp, t);		/* Splice up the new arglst */
		f = *t;

		/*
		 * prepare wchar_t to char for exec
		 */
		ot = t;				/* save the original pointer */
		f_str = tstostr(NOSTR, f, 0);	/* allocate first argument */
		len = blklen(t);		/* get the length */

		ot_str = t_str = (char **)salloc(len + 1, sizeof(char **));
		for(i = 0; i < len; i++) {
		    *ot_str++ = tstostr(NOSTR, *ot++, 0);
		}
		*ot_str = 0;
		execv(f_str, t_str);		/* exec the command */

		/*
		 * free the area
		 */
		xfree(f_str);			/* Free the first */
		ot_str = t_str;
		for(i = 0; i < len; i++) {	/* Free the second list */
		    xfree(*ot_str++);
		}
		xfree(t_str);			/* Free the holder */
		xfree(t);

	    case ENOMEM:
		Perror(f);

	    case ENOENT:
		break;

	    case EPERM:
		error(gettxt(_SGI_DMMX_csh_setuid,
			     "%s: Setuid program execute permission denied"),
		      tstostr(chbuf, f, NOFLAG));
		break;
	/*
	 * We recognize the magic number, but can't exec it (i.e. MIPS II on
	 * a R3000).
	 */
	    case EBADRQC:
		error(gettxt(_SGI_DMMX_csh_badcomp,
			     "%s: Program not supported by architecture"),
		      tstostr(chbuf, f, NOFLAG));
		break;

	    default:
		if( !exerr) {
		    exerr = strerror(errno);
		    expath = savestr(f);
		}
	}
}

/*ARGSUSED*/
void
execash(wchar_t **t, struct command *kp)
{
#ifdef TRACE
	tprintf("TRACE- execash()\n");
#endif
	rechist();
	(void)signal(SIGINT, parintr);
	(void)signal(SIGQUIT, parintr);
	(void)signal(SIGTERM, parterm);		/* if doexec loses, screw */
	lshift(kp->t_dcom, 1);
	exiterr++;
	doexec(kp);				/* no return */
}

void
dohash(void)
{
	register struct dirent *dp;
	register int cnt;
	DIR *dirp;
	int i = 0;
	struct varent *v = adrof(S_path);
	wchar_t **pv;
	wchar_t *p;
	int cflag;
	wchar_t curdir_[MAXNAMLEN + 1];

#ifdef TRACE
	tprintf("TRACE- dohash()\n");
#endif
	havhash = 1;
	for(cnt = 0; cnt < sizeof(xhash)/sizeof(*xhash); cnt++)
	    xhash[cnt] = 0;
	if( !v)
	    return;
	for(pv = v->vec; *pv; pv++, i++) {
	    if(pv[0][0] != '/')
		continue;
	    dirp = opendir_(*pv);
	    if( !dirp)
		continue;
	    while(dp = readdir(dirp)) {
		if( ! dp->d_ino)
		    continue;
		if((dp->d_name[0] == '.') && (dp->d_name[1] == '\0'
		    || dp->d_name[1] == '.' && dp->d_name[2] == '\0'))
			continue;
		p = strtots(curdir_, dp->d_name, &cflag);
		if(cflag)
		    err_fntruncated(dp->d_name);
		bis(xhash, hash(hashname(p), i));
	    }
	    closedir(dirp);
	}
}

void
dounhash(void)
{
#ifdef TRACE
	tprintf("TRACE- dounhash()\n");
#endif
	havhash = 0;
}

/*
 * Hash a command name.
 */
static int
hashname(wchar_t *cp)
{
	register long h = 0;

#ifdef TRACE
/*
	tprintf("TRACE- hashname()\n");
*/
#endif
	while(*cp)
	    h = hash(h, *cp++);
	assert(h < HSHSIZ);
	return((int)h);
}
