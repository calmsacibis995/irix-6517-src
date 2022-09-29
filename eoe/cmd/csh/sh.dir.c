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

#include "sh.h"
#include "sh.dir.h"
#include "sh.wconst.h"
#include <unistd.h>

/*
 * C Shell - directory management
 */

struct directory *dcwd;			/* the one we are in now */

int	printd;				/* force name to be printed */
struct	directory dhead;		/* "head" of loop */

static wchar_t		*dcanon(wchar_t *, wchar_t *);
static struct directory	*dfind(wchar_t *);
static wchar_t		*dfollow(wchar_t *);
static void		dnewcwd(struct directory *);

static	wchar_t *fakev[] = { S_dirs, NOSTR };

/*
 * print dir with tilde
 */
void
dtildepr(wchar_t *home, wchar_t *dir)
{
#ifdef TRACE
	tprintf("TRACE- dtildepr()\n");
#endif
	if( !eq(home, S_SLASH) && prefix(home, dir))
	    shprintf("~%t", dir + wslen(home));
	else
	    shprintf("%t", dir);
}

/*
 * dfree - free the directory (or keep it if it still has ref count)
 */
void
dfree(struct directory *dp)
{
#ifdef TRACE
	tprintf("TRACE- dfree()\n");
#endif
	if(dp->di_count)
	    dp->di_next = dp->di_prev = 0;
	else
	    xfree(dp->di_name), xfree(dp);
}

/*
 * dodirs - list all directories in directory loop
 */
void
dodirs(wchar_t **v)
{
	register struct directory *dp;
	register wchar_t *hp;
	bool lflag;

#ifdef TRACE
	tprintf("TRACE- dodirs()\n");
#endif
	hp = value(S_home);
	if(*hp == '\0')
	    hp = NOSTR;
	if(*++v) {
	    if(eq(*v, S_MINl) && (*++v == NOSTR))
		lflag = 1;
	    else {
		err_unknflag(**v);
		err_usage("dirs [ -l ]");
	    }
	} else
	    lflag = 0;

	dp = dcwd;
	do {
	    if(dp == &dhead)
		continue;
	    if( !lflag && hp)
		dtildepr(hp, dp->di_name);
	    else
		shprintf("%t", dp->di_name);
	    shprintf(" ");
	} while((dp = dp->di_prev) != dcwd);
	shprintf("\n");
}

/*
 * dnewcwd
 * make a new directory in the loop the current one
 * and export its name to the PWD environment variable.
 */
static void
dnewcwd(struct directory *dp)
{
#ifdef TRACE
	tprintf("TRACE- dnewcwd()\n");
#endif
	dcwd = dp;

	/*
	 * If we have a fast version of getwd available
	 * and hardpaths is set, it would be reasonable
	 * here to verify that dcwd->di_name really does
	 * name the current directory.  Later...
	 */
	set(S_cwd, savestr(dcwd->di_name));
	setenv(S_PWD, dcwd->di_name);
	if(printd)
	    dodirs(fakev);
}

/*
 * dinit - initialize current working directory
 */
/*ARGSUSED*/
void
dinit(wchar_t *hp)
{
	register wchar_t *cp;
	register struct directory *dp;

#ifdef	TRACE
	tprintf("TRACE - dinit()\n");
#endif
	/*
	 * The normal code below makes a performance optimization by assuming
	 * that in a login shell the initial working directory is $HOME.
	 * At SGI we cannot do that because /bin/wsh execs a login shell
	 * whenever it is invoked from the command line.  If this is done
	 * when $cwd != $HOME, then 'dirs' disagrees with 'pwd'.
	 */
	cp = getwd_(NOSTR);
	if( !cp) {
	    haderr = 1;
	    error(gettxt(_SGI_DMMX_csh_badcurdir,
			 "Cannot determine current working directory"));
	    done(1);
	}

	dp = (struct directory *)salloc(1, sizeof(struct directory));
	dp->di_name = cp;
	dp->di_count = 0;
	dhead.di_next = dhead.di_prev = dp;
	dp->di_next = dp->di_prev = &dhead;
	printd = 0;
	dnewcwd(dp);
}

/*
 * dochngd - implement chdir command.
 */
void
dochngd(wchar_t **v)
{
	register wchar_t *cp;
	register struct directory *dp;
	int omask; 

#ifdef TRACE
	tprintf("TRACE- dochngd()\n");
#endif
	printd = 0;
	if(*++v == NOSTR) {
	    if( !(cp = value(S_home)) || !*cp)
		bferr(gettxt(_SGI_DMMX_csh_nohome, "No home directory"));
	    if(chdir_(cp) < 0)
		bferr(gettxt(_SGI_DMMX_csh_cannothome,
			    "Can't change to home directory"));
	    cp = savestr(cp);
	} else if ((dp = dfind(*v)) != 0) {
	    printd = 1;
	    if(chdir_(dp->di_name) < 0)
		Perror(dp->di_name);			/* no return */
	    dcwd->di_prev->di_next = dcwd->di_next;
	    dcwd->di_next->di_prev = dcwd->di_prev;
	    goto flushcwd;
	} else {

	    /*  Fix for chdir hanging for NFS directories (/hosts/<machine>/). Enable 
		SIGINT so that ^C is caught and processed. 
	    */

	    omask = sigsetmask(sigblock(0) & ~sigmask(SIGINT));
	    cp = dfollow(*v);
	    (void) sigsetmask (omask);
	}

	dp = (struct directory *)salloc(1, sizeof(struct directory));
	dp->di_name = cp;
	dp->di_count = 0;
	dp->di_next = dcwd->di_next;
	dp->di_prev = dcwd->di_prev;
	dp->di_prev->di_next = dp;
	dp->di_next->di_prev = dp;
flushcwd:
	dfree(dcwd);
	dnewcwd(dp);
}

/*
 * dfollow
 *
 * change to arg directory; fall back on cdpath if not valid
 */
static wchar_t *
dfollow(wchar_t *cp)
{
	register wchar_t *dp;
	register wchar_t *p;
	register struct varent *c;
	register wchar_t *q;
	register wchar_t **cdp;
	wchar_t buf[MAXPATHLEN];	/* local buffer */

#ifdef TRACE
	tprintf("TRACE- dfollow()\n");
#endif
	cp = globone(cp);
	if(chdir_(cp) >= 0)		/* change to directory */
	    goto gotcha2;

	/*
	 * try to interpret components of cdpath
	 */
	if(cp[0] != '/' 
	    && !prefix(S_DOTSLA, cp) 
	    && !prefix(S_DOTDOTSLA, cp)
	    && (c = adrof(S_cdpath))) {

		for(cdp = c->vec; *cdp; cdp++) {
		    for(dp = buf, p = *cdp; *dp++ = *p++;);
		    dp[-1] = '/';
		    wscpy(dp, cp);
		    if(chdir_(buf) >= 0) {
			dp = buf;
			goto gotcha;
		    }
		}
	}
	/*
	 * Try de-referencing the variable named by the argument.
	 */
	dp = value(cp);
	if((dp[0] != '/' && dp[0] != '.') || (chdir_(dp) < 0)) {
	    (void)wscpy(buf, cp);
	    xfree(cp);
	    Perror(buf);
	}
gotcha:
	xfree(cp);
	printd = 1;
	cp = savestr(dp);
gotcha2:
	if(*cp != '/') {
	    int cwdlen;
	    int len;

	    if((cwdlen = wslen(dcwd->di_name)) == 1)
		cwdlen = 0;				/* root */
	    len = wslen(cp);
	    dp = wcalloc(cwdlen + len + 2);

	    for(p = dp, q = dcwd->di_name; *p++ = *q++;);
	    if(cwdlen)
		p[-1] = '/';
	    else
		p--;
	    wscpy(p, cp);
	    xfree(cp);
	    cp = dp;
	    dp += cwdlen;
	} else
	    dp = cp;
	return(dcanon(cp, dp));
}

/*
 * dopushd - push new directory onto directory stack.
 *	with no arguments exchange top and second.
 *	with numeric argument (+n) bring it to top.
 */
void
dopushd(wchar_t **v)
{
	register struct directory *dp;
	register wchar_t *cp;

#ifdef TRACE
	tprintf("TRACE- dopushd()\n");
#endif
	printd = 1;
	if(*++v == NOSTR) {
	    if((dp = dcwd->di_prev) == &dhead)
		dp = dhead.di_prev;
	    if(dp == dcwd)
		bferr(gettxt(_SGI_DMMX_csh_noodir, "No other directory"));
	    if(chdir_(dp->di_name) < 0)
		Perror(dp->di_name);			/* no return */
	    dp->di_prev->di_next = dp->di_next;
	    dp->di_next->di_prev = dp->di_prev;
	    dp->di_next = dcwd->di_next;
	    dp->di_prev = dcwd;
	    dcwd->di_next->di_prev = dp;
	    dcwd->di_next = dp;
	} else {
	    if(dp = dfind(*v)) {
		if(chdir_(dp->di_name) < 0)
		    Perror(dp->di_name);		/* no return */
	    } else {
		cp = dfollow(*v);
		dp = (struct directory *)salloc(1, sizeof(struct directory));
		dp->di_name = cp;
		dp->di_count = 0;
		dp->di_prev = dcwd;
		dp->di_next = dcwd->di_next;
		dcwd->di_next = dp;
		dp->di_next->di_prev = dp;
	    }
	}
	dnewcwd(dp);
}

/*
 * dfind - find a directory if specified by numeric (+n) argument
 */
static struct directory *
dfind(wchar_t *cp)
{
	register struct directory *dp;
	register wchar_t *ep;
	register int i;

#ifdef TRACE
	tprintf("TRACE- dfind()\n");
#endif
	if(*cp++ != '+')
	    return(0);
	for(ep = cp; digit(*ep); ep++);		/* check number */
	if(*ep)
	    return(0);				/* other than digit */
	i = getn(cp);
	if(i <= 0)
	    return(0);
	for(dp = dcwd; i; i--) {
	    if((dp = dp->di_prev) == &dhead)
		dp = dp->di_prev;
	    if(dp == dcwd)
		bferr(gettxt(_SGI_DMMX_csh_dsnotdeep,
		    "Directory stack not that deep"));
	}
	return(dp);
}

/*
 * dopopd - pop a directory out of the directory stack
 *	with a numeric argument just discard it.
 */
void
dopopd(wchar_t **v)
{
	register struct directory *dp, *p;

#ifdef TRACE
	tprintf("TRACE- dopopd()\n");
#endif
	printd = 1;
	if(*++v == NOSTR)
	    dp = dcwd;
	else {
	    if( !(dp = dfind(*v)))
/*XXXX MMX_csh_invarg */
		bferr(gettxt(_SGI_DMMX_csh_invarg, "%s - Invalid argument"));
	}
	if((dp->di_prev == &dhead) && (dp->di_next == &dhead))
	    bferr(gettxt(_SGI_DMMX_csh_dsempty, "Directory stack empty"));
	if(dp == dcwd) {
	    if((p = dp->di_prev) == &dhead)
		p = dhead.di_prev;
	    if(chdir_(p->di_name) < 0)
		Perror(p->di_name);			/* no return */
	}
	dp->di_prev->di_next = dp->di_next;
	dp->di_next->di_prev = dp->di_prev;
	if(dp == dcwd)
	    dnewcwd(p);
	else
	    dodirs(fakev);
	dfree(dp);
}

/*
 * dcanon - canonicalize the pathname, removing excess ./ and ../ etc.
 *	We are of course assuming that the file system is standardly
 *	constructed (always have ..'s, directories have links).
 *
 *	If the hardpaths shell variable is set, resolve the
 *	resulting pathname to contain no symbolic link components.
 */
static wchar_t *
dcanon(wchar_t *cp, wchar_t *p)
{
	register wchar_t *sp;		/* rightmost component */
	register wchar_t *p1;
	bool slash, dotdot, hardpaths;

#ifdef TRACE
	tprintf("TRACE- dcannon()\n");
#endif
	if(*cp != '/')
	    abort();

	if(hardpaths = (adrof(S_hardpaths) != NULL)) {
	    /*
	     * Be paranoid: don't trust the initial prefix
	     * to be symlink-free.
	     */
	    p = cp;
	}

	/*
	 * Loop invariant: cp points to the overall path start,
	 * p to its as yet uncanonicalized trailing suffix.
	 */
	while(*p) {			/* for each component */
	    sp = p;
	    while(*++p == '/');		/* flush extra slashes */
	    if(p > ++sp)
		wscpy(sp, p);
	    p = sp;			/* save start of component */
	    slash = 0;

	    /*
	     * find next slash or end of path
	     */
	    while(*++p) {
		if(*p == '/') {
		    slash = 1;			/* slash found */
		    *p = '\0';
		    break;
		}
	    }
	    /*
	     * check if another component after slash
	     */
	    if(*sp == '\0') {
		if(--sp == cp)
		    break;			/* only slash */
		*sp = '\0';			/* remove tailing slash */
		continue;
	    }
	    /*
	     * squeeze out component consisting of "."
	     */
	    if((sp[0] == '.') && (sp[1] == '\0')) {
		if(slash) {
		    wscpy(sp, p + 1);
		    p = --sp;
		} else
		    if(--sp != cp)
			*sp = '\0';
		continue;
	    }

	    /*
	     * At this point we have a path of the form "x/yz",
	     * where "x" is null or rooted at "/", "y" is a single
	     * component, and "z" is possibly null.  The pointer cp
	     * points to the start of "x", sp to the start of "y",
	     * and p to the beginning of "z", which has been forced
	     * to a null.
	     */

	    /*
	     * Process symbolic link component.  Provided that either
	     * the hardpaths shell variable is set or "y" is really
	     * ".." we replace the symlink with its contents.  The
	     * second condition for replacement is necessary to make
	     * the command "cd x/.." produce the same results as the
	     * sequence "cd x; cd ..".
	     *
	     * Note that the two conditions correspond to different
	     * potential symlinks.  When hardpaths is set, we must
	     * check "x/y"; otherwise, when "y" is known to be "..",
	     * we check "x".
	     */
	    dotdot = wscmp(sp, S_DOTDOT)? 0 : 1;
	    if(hardpaths || dotdot) {
		int cc;
		wchar_t *newcp;
		wchar_t link[MAXPATHLEN];

		/*
		 * Isolate the end of the component that is to
		 * be checked for symlink-hood.
		 */
		sp--;
		if( !hardpaths)
		    *sp = '\0';

		/*
		 * See whether the component is really a symlink by
		 * trying to read it.  If the read succeeds, it is.
		 */
		if((hardpaths || (sp > cp))
		    && (cc = readlink_(cp, link, MAXPATHLEN)) >= 0) {
			if (slash)
			    *p = '/';		/* restore path */

			/*
			 * Point p at the start of the trailing
			 * path following the symlink component.
			 * It's already there is hardpaths is set.
			 */
			if( !hardpaths) {
			    *(p = sp) = '/';	/* restore path */
			}
			if(*link != '/') {
			    /*
			     * Relative path: replace the symlink
			     * component with its value.  First,
			     * set sp to point to the slash at
			     * its beginning.  If hardpaths is
			     * set, this is already the case.
			     */
			    if (! hardpaths) {
				while (*--sp != '/');
			    }

			    /*
			     * Terminate the leading part of the
			     * path, including trailing slash.
			     */
			    sp++;
			    *sp = '\0';

			    /*
			     * New length is: "x/" + link + "z"
			     * Copy new path into newcp and restart
			     * canonicalization at expanded "/y".
			     */
			    p1 = newcp = wcalloc(wslen(cp) + cc + wslen(p) + 1);
			    p1 = wscpyend(p1, cp);
			    p1 = wscpyend(p1, link);
			    wscpyend(p1, p);
			    p = sp - cp - 1 + newcp;
			} else {
			    /*
			     * New length is: link + "z"
			     * Copy new path into newcp and restart
			     * canonicalization at beginning.
			     */
			    p1 = newcp = wcalloc(cc + wslen(p) + 1);
			    p1 = wscpyend(p1, link);
			    wscpyend(p1, p);
			    p = newcp;
			}
			xfree(cp);
			cp = newcp;
			continue;	/* canonicalize the link */
		}
		/*
		 * The component wasn't a symlink after all
		 */
		if( !hardpaths)
		    *sp = '/';
	    }

	    if(dotdot) {
		if(sp != cp)
		    while(*--sp != '/');
		if(slash) {
		    wscpy(sp + 1, p + 1);
		    p = sp;
		} else {
		    if(cp == sp)
			*++sp = '\0';
		    else
			*sp = '\0';
		}
		continue;
	    }
	    if(slash)
		*p = '/';
	}
	return(cp);
}
