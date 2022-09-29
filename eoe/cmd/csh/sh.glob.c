/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.9 $"

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


#include "sh.h"
#include "sh.proc.h"
#include "sh.wconst.h"
#include <alloca.h>
#include <unistd.h>
#include <widec.h>
#include <strings.h>

/*
 * C Shell
 */

int	globcnt;

wchar_t	*gpath, *gpathp, *lastgpathp;
int	globbed;
bool	noglob;
bool	nonomatch;
wchar_t	*entp;
wchar_t	**sortbas;

static void	acollect(wchar_t *);
static void	addpath(wchar_t);
static int	amatch(wchar_t *, wchar_t *);
static void	backeval(wchar_t *, bool);
static void	collect(wchar_t *);
static int	execbrc(wchar_t *, wchar_t *);
static void	expand(wchar_t *);
static int	match(wchar_t *, wchar_t *);
static void	matchdir_(wchar_t *);
static void	psave(wchar_t);
static void	pword(void);
static int	sortscmp(const void *, const void *);

#define sort()	qsort( (wchar_t *)sortbas, &gargv[gargc] - sortbas, \
		      sizeof(*sortbas), sortscmp), sortbas = &gargv[gargc]

#if 0
#define	GDEBUG		/* glob() debug on/off */
#endif

wchar_t **
glob(wchar_t **v)
{
	wchar_t agpath[CSHBUFSIZ];
	wchar_t **agargv;

	agargv = (wchar_t **)alloca(gavsiz*sizeof(*agargv));

#ifdef TRACE
	tprintf("TRACE- glob()\n");
#endif
	gpath = agpath;
	gpathp = gpath;
	*gpathp = 0;
	lastgpathp = &gpath[CSHBUFSIZ - 2];
	ginit(agargv);
	globcnt = 0;
#ifdef GDEBUG
	shprintf("glob entered: ");
	blkpr(v);
	shprintf("\n");
#endif
	noglob = (adrof(S_noglob) != 0);
	nonomatch = adrof(S_nonomatch) != 0;
	globcnt = noglob | nonomatch;
	while(*v)
	    collect(*v++);
#ifdef GDEBUG
	shprintf("glob done, globcnt=%d, gflag=%d: ",
	    globcnt,
	    gflag);
	blkpr(gargv); shprintf("\n");
#endif
	if( !globcnt && (gflag & 1)) {
	    blkfree(gargv), gargv = 0;
	    return(0);
	}
	return(gargv = copyblk(gargv));
}

void
ginit(wchar_t **agargv)
{

	agargv[0] = 0; gargv = agargv; sortbas = agargv; gargc = 0;
	gnleft = ncargs - 4;
}

static void
collect(wchar_t *as)
{
	register int i;

#ifdef TRACE
	tprintf("TRACE- collect()\n");
#endif
	if (any('`', as)) {
#ifdef GDEBUG
		shprintf("doing backp of %s\n", as);
#endif
		(void) dobackp(as, 0);
#ifdef GDEBUG
		shprintf("backp done, acollect'ing\n");
#endif
		for (i = 0; i < pargc; i++)
			if (noglob) {
				Gcat(pargv[i], S_ /*""*/);
				sortbas = &gargv[gargc];
			} else
				acollect(pargv[i]);
		if (pargv)
			blkfree(pargv), pargv = 0;
#ifdef GDEBUG
		shprintf("acollect done\n");
#endif
	} else if (noglob || eq(as, S_LBRA /*"{" */) || eq(as, S_BRABRA /*"{}"*/)) {
		Gcat(as, S_ /*""*/);
		sort();
	} else
		acollect(as);
}

static void
acollect(wchar_t *as)
{
	register long ogargc = gargc;

#ifdef TRACE
	tprintf("TRACE- acollect(%t)\n", as);
#endif
	gpathp = gpath; *gpathp = 0; globbed = 0;
	expand(as);
	if (gargc == ogargc) {
		if (nonomatch) {
			Gcat(as, S_ /*""*/);
			sort();
		}
	} else
		sort();
}

/*
 * String compare for qsort.  Also used by filec code in sh.file.c.
 */
static int
sortscmp(const void *a1, const void *a2)
{
	return(wscmp(*((wchar_t **)a1), *((wchar_t **)a2)));
}

static void
expand(wchar_t *as)
{
	register wchar_t *cs;
	register wchar_t *sgpathp, *oldcs;
	struct stat stb;
	char chbuf[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- expand()\n");
#endif
	sgpathp = gpathp;
	cs = as;
	if (*cs == '~' && gpathp == gpath) {
		addpath('~');
		for (cs++; alnum(*cs) || *cs == '-';)
			addpath(*cs++);
		if (!*cs || *cs == '/') {
			if (gpathp != gpath + 1) {
				*gpathp = 0;
				if (gethdir(gpath + 1))
					/*
					 * modified from %s to %t
					 */
					error(gettxt(_SGI_DMMX_csh_unkuser,
					    "%s: Unknown user"),
					    tstostr(chbuf, gpath + 1, NOFLAG));
				wscpy(gpath, gpath + 1);
			} else
				wscpy(gpath, value(S_home));
			gpathp = strend(gpath);
		}
	}
	while (!isglob(*cs)) {
		if (*cs == 0) {
			if (!globbed)
				Gcat(gpath, S_ /*"" */);
			else if (stat_(gpath, &stb) >= 0) {
				Gcat(gpath, S_ /*""*/);
				globcnt++;
			}
			else if (lstat_(gpath, &stb) >= 0) {
				Gcat(gpath, S_ /*""*/);
				globcnt++;
			}
			goto endit;
		}
		addpath(*cs++);
	}
	oldcs = cs;
	while (cs > as && *cs != '/')
		cs--, gpathp--;
	if (*cs == '/')
		cs++, gpathp++;
	*gpathp = 0;
	if (*oldcs == '{') {
		(void) execbrc(cs, NOSTR);
		return;
	}
	matchdir_(cs);
endit:
	gpathp = sgpathp;
	*gpathp = 0;
}

static void
matchdir_(wchar_t *pattern)
{
	register struct dirent *dp;
	register DIR *dirp;
	int cflag;
	struct stat stb;
	wchar_t curdir_[MAXNAMLEN + 1];

#ifdef TRACE
	tprintf("TRACE- matchdir()\n");
#endif
        /*
         * BSD's opendir would open "." if argument is NULL, but not S5
         */
	if(*gpath == NULL)
	    dirp = opendir_(S_DOT);
	else
	    dirp = opendir_(gpath);
	if( !dirp) {
	    if(globbed)
		return;
	    goto patherr2;
	}
	if(fstat(dirp->dd_fd, &stb) < 0)
	    goto patherr1;
	if( !isdir(stb)) {
	    errno = ENOTDIR;
	    goto patherr1;
	}

	while(dp = readdir(dirp)) {
	    if( !dp->d_ino)
		continue;
	    strtots(curdir_, dp->d_name, &cflag);
	    if(cflag)
		err_fntruncated(dp->d_name);
	    if(match(curdir_, pattern)) {
		Gcat(gpath, curdir_);
		globcnt++;
	    }
	}
	closedir(dirp);
	return;

patherr1:
	closedir(dirp);
patherr2:
	Perror(gpath);
}

static int
execbrc(wchar_t *p, wchar_t *s)
{
	wchar_t restbuf[CSHBUFSIZ + 2];
	register wchar_t *pe, *pm, *pl;
	int brclev = 0;
	wchar_t *lm, savec, *sgpathp;

#ifdef TRACE
	tprintf("TRACE- execbrc()\n");
#endif
	for (lm = restbuf; *p != '{'; *lm++ = *p++)
		continue;
	for (pe = ++p; *pe; pe++)
	switch (*pe) {

	case '{':
		brclev++;
		continue;

	case '}':
		if (brclev == 0)
			goto pend;
		brclev--;
		continue;

	case '[':
		for (pe++; *pe && *pe != ']'; pe++)
			continue;
		if (!*pe)
			err_missing(']');
		continue;
	}
pend:
	if (brclev || !*pe)
		err_missing('}');
	for (pl = pm = p; pm <= pe; pm++)
	switch(*pm) {

	case '{':
		brclev++;
		continue;

	case '}':
		if (brclev) {
			brclev--;
			continue;
		}
		goto doit;

	case ','|QUOTE:
	case ',':
		if (brclev)
			continue;
doit:
		savec = *pm;
		*pm = 0;
		wscpy(lm, pl);
		wscat(restbuf, pe + 1);
		*pm = savec;
		if (s == 0) {
			sgpathp = gpathp;
			expand(restbuf);
			gpathp = sgpathp;
			*gpathp = 0;
		} else if (amatch(s, restbuf))
			return (1);
		sort();
		pl = pm + 1;
		continue;

	case '[':
		for (pm++; *pm && *pm != ']'; pm++)
			continue;
		if (!*pm)
			err_missing(']');
		continue;
	}
	return (0);
}

static int
match(wchar_t *s, wchar_t *p)
{
	register int c;
	register wchar_t *sentp;
	wchar_t sglobbed = globbed;

#ifdef TRACE
	tprintf("TRACE- match()\n");
#endif
	if(*s == '.' && *p != '.')
	    return(0);
	sentp = entp;
	entp = s;
	c = amatch(s, p);
	entp = sentp;
	globbed = sglobbed;
	return(c);
}

static int
amatch(wchar_t *s, wchar_t *p)
{
	register int scc;
	register int ok;
	register wchar_t lc, c, cc;
	wchar_t *sgpathp;
	struct stat stb;

#ifdef TRACE
	tprintf("TRACE- amatch()\n");
#endif
	globbed = 1;
	for(;;) {
	    scc = *s++ & TRIM;
	    switch(c = *p++) {
		case '{':
		    return(execbrc(p - 1, s - 1));
		case '[':
		    ok = 0;
		    lc = TRIM;
		    while(cc = *p++) {
			if(cc == ']') {
			    if(ok)
				break;
			    return (0);
			}
			if(cc == '-') {
  				wchar_t rc= *p++;
  				/* Both ends of the char range
  				 * must belong to the same codeset...
  				 */
				if( !wcsetno(lc, rc)) {
  					/* But if not, we quietly 
  					 * ignore the '-' operator.
  					 * [x-y] is treated as if
  					 * it were [xy].
  					 */
  					if (scc == (lc = rc))
  					    ok++;
  				}
  				if((lc <= scc) && (scc <= rc)
				    && wcsetno(lc, scc))
  					ok++;
			} else {
			    if(scc == (lc = cc))
				ok++;
			}
		    }
		    if( !cc)
			err_missing(']');
		    continue;
		case '*':
		    if( !*p)
			return(1);
		    if(*p == '/') {
			p++;
			goto slash;
		    }
		    for(s--; *s; s++)
			if(amatch(s, p))
			    return (1);
		    return(0);
		case 0:
		    return(scc == 0);
		default:
		    if((c & TRIM) != scc)
			return(0);
		    continue;
		case '?':
		    if( !scc)
			return(0);
		    continue;
		case '/':
		    if(scc)
			return(0);
slash:
		    s = entp;
		    sgpathp = gpathp;
		    while(*s)
			addpath(*s++);
		    addpath('/');
		    if( !stat_(gpath, &stb) && isdir(stb))
			if(*p == 0) {
			    Gcat(gpath, S_);
			    globcnt++;
			} else
			    expand(p);
		    gpathp = sgpathp;
		    *gpathp = 0;
		    return (0);
	    }
	}
}

int
Gmatch(wchar_t *s, wchar_t *p)
{
	register int ok;
	register wchar_t lc, scc, c, cc;

#ifdef TRACE
	tprintf("TRACE- Gmatch()\n");
#endif
	for(;;) {
	    scc = *s++ & TRIM;
	    switch (c = *p++) {
		case '[':
		    ok = 0;
		    lc = TRIM;
		    while (cc = *p++) {
			if(cc == ']') {
			    if(ok)
				break;
			    return (0);
			}
			if(cc == '-') {
  				wchar_t rc= *p++;
  				/* Both ends of the char range
  				 * must belong to the same codeset...
  				 */
				if( !wcsetno(lc, rc)) {
  					/* But if not, we quietly 
  					 * ignore the '-' operator.
  					 * [x-y] is treated as if
  					 * it were [xy].
  					 */
  					if (scc == (lc = rc))
  					    ok++;
  				}
  				if (lc <= scc && scc <= rc
				    && wcsetno(lc, scc))
  					ok++;
			} else {
			    if(scc == (lc = cc))
				ok++;
			}
		    }
		    if( !cc)
			err_missing(']');
		    continue;
		case '*':
		    if( !*p)
			return(1);
		    for(s--; *s; s++)
			if(Gmatch(s, p))
			    return(1);
		    return(0);
		case 0:
		    return(scc == 0);
		default:
		    if((c & TRIM) != scc)
			return(0);
		    continue;
		case '?':
		    if( !scc)
			return(0);
		    continue;
	    }
	}
}

void
Gcat(wchar_t *s1, wchar_t *s2)
{
	register wchar_t *p;
	register int n;

#ifdef TRACE
	tprintf("TRACE- Gcat()\n");
#endif
	n = wslen(s1) + wslen(s2) + 1;
	gnleft -= n;
	if((gnleft <= 0) || (++gargc >= gavsiz))
	    err_arg2long();
	gargv[gargc] = 0;
	p = gargv[gargc - 1] = wcalloc(n);	/* allocate buffer */

	p = wscpyend(p, s1);			/* copy s1 to buffer */
	wscpy(p, s2);				/* append s2 */
}

static void
addpath(wchar_t c)
{

#ifdef TRACE
	tprintf("TRACE- addpath()\n");
#endif
	if (gpathp >= lastgpathp)
		error(gettxt(_SGI_DMMX_csh_path2long, "Pathname too long"));
	*gpathp++ = c & TRIM;
	*gpathp = 0;
}

void
rscan(wchar_t **t, void (*f)(wchar_t))
{
	register wchar_t *p;

#ifdef TRACE
	tprintf("TRACE- rscan()\n");
#endif
	while(p = *t++) {
	    while(*p)
		(*f)(*p++);
	}
}

/* this function tries to trash readonly strings, such as the signal names.
 * because of that, one can't really safely link with -use_readonly_const
 * It happened to work when csh was linked non-shared.  The hack of not-
 * doing the store if not necessary would work for C locales, and avoids some
 * unnecessar ystores, so it's worth doing, but we can't be sure it would
 * work in all locales, so still can't use  -use_readonly_const.  We might
 * be able to fix all callers to pass extra flags so we don't trim()
 * constant strings, but this code is so bad that I'm not sure we would ever
 * find them all.  Olson, 3/97 
*/
void
trim(wchar_t **t)
{
	register wchar_t *p;

#ifdef TRACE
	tprintf("TRACE- trim()\n");
#endif
	while(p = *t++) {
	    while(*p) {
		if( *p & QUOTE) *p &= TRIM;
		p++;
	    }
	}
}

void
tglob(wchar_t **t)
{
	register wchar_t *p, c;

#ifdef TRACE
	tprintf("TRACE- tglob()\n");
#endif
	while(p = *t++) {
	    if(*p == '~')
		gflag |= 2;
	    else
		if(*p == '{' && (!p[1] || (p[1] == '}' && !p[2])))
		    continue;
	    while(c = *p++) {
		if(isglob(c))
		    gflag |= (c == '{')? 2 : 1;
	    }
	}
}

wchar_t *
globone(wchar_t *str)
{
	register wchar_t **gvp;
	register wchar_t *cp;
	wchar_t *gv[2];

#ifdef TRACE
	tprintf("TRACE- globone()\n");
#endif
	gv[0] = str;
	gv[1] = 0;
	gflag = 0;
	tglob(gv);
	if(gflag) {
	    gvp = glob(gv);
	    if( !gvp) {
		setname(str);
		err_nomatch();
	    }
	    cp = *gvp++;
	    if( !cp)
		cp = S_;
	    else if(*gvp) {
		setname(str);
		ambiguous();
	    } else
		cp = strip(cp);
/*
	    if( !cp || *gvp) {
		setname(str);
		if(cp)
		    ambiguous();
		else
		    bferr(gettxt(_SGI_DMMX_csh_nooutput, "No output"));
	    }
*/
	    xfree(gargv); gargv = 0;
	} else {
	    trim(gv);
	    cp = savestr(gv[0]);
	}
	return(cp);
}

/*
 * Command substitute cp.  If literal, then this is
 * a substitution from a << redirection, and so we should
 * not crunch blanks and tabs, separating words only at newlines.
 */
wchar_t **
dobackp(wchar_t *cp, bool literal)
{
	register wchar_t *lp, *rp;
	wchar_t *ep;
	wchar_t word[CSHBUFSIZ];
	wchar_t **apargv;

	apargv = (wchar_t **)alloca((gavsiz+2)*sizeof(*apargv));
 

#ifdef TRACE
	tprintf("TRACE- dobackp()\n");
#endif
	if (pargv) {
		blkfree(pargv);
		/* abort();  abort commented out as per tcsh example */
	}
	pargv = apargv;
	pargv[0] = NOSTR;
	pargcp = pargs = word;
	pargc = 0;
	pnleft = CSHBUFSIZ - 4;
	for (;;) {
		for (lp = cp; *lp != '`'; lp++) {
			if (*lp == 0) {
				if (pargcp != pargs)
					pword();
#ifdef GDEBUG
				shprintf("leaving dobackp\n");
#endif
				return (pargv = copyblk(pargv));
			}
			psave(*lp);
		}
		lp++;
		for (rp = lp; *rp && *rp != '`'; rp++)
			if (*rp == '\\') {
				rp++;
				if (!*rp)
					goto oops;
			}
		if (!*rp)
oops:
			err_unmatched('`');
		ep = savestr(lp);
		ep[rp - lp] = 0;
		backeval(ep, literal);
#ifdef GDEBUG
		shprintf("back from backeval\n");
#endif
		cp = rp + 1;
	}
}

static void
backeval(wchar_t *cp, bool literal)
{
	int pvec[2];
	int quoted = (literal || (cp[0] & QUOTE))? QUOTE : 0;
	register int icnt = 0, c;
	register wchar_t *ip;
	bool hadnl = 0;
	wchar_t *fakecom[2];
	struct command faket;
	wchar_t ibuf[CSHBUFSIZ];

#ifdef TRACE
	tprintf("TRACE- backeval()\n");
#endif
	faket.t_dtyp = TCOM;
	faket.t_dflg = 0;
	faket.t_dlef = 0;
	faket.t_drit = 0;
	faket.t_dspr = 0;
	faket.t_dcom = fakecom;
	fakecom[0] = S_QPPPQ;
	fakecom[1] = 0;
	/*
	 * We do the psave job to temporarily change the current job
	 * so that the following fork is considered a separate job.
	 * This is so that when backquotes are used in a
	 * builtin function that calls glob the "current job" is not corrupted.
	 * We only need one level of pushed jobs as long as we are sure to
	 * fork here.
	 */
	psavejob();
	/*
	 * It would be nicer if we could integrate this redirection more
	 * with the routines in sh.sem.c by doing a fake execute on a builtin
	 * function that was piped out.
	 */
	mypipe(pvec);
	if (pfork(&faket, -1) == 0) {
		struct wordent paraml;
		struct command *t;

		(void) close(pvec[0]);
		(void) dmove(pvec[1], 1);
		(void) dmove(SHDIAG, 2);
		initdesc();
		arginp = cp;
		strip(cp);
		(void) lex(&paraml);
		if (errstr)
			error_errstr();
		alias(&paraml);
		t = syntax(paraml.next, &paraml, 0);
		if (errstr)
			error_errstr();
		if (t)
			t->t_dflg |= FPAR;
		(void) signal(SIGTSTP, SIG_IGN);
		(void) signal(SIGTTIN, SIG_IGN);
		(void) signal(SIGTTOU, SIG_IGN);
		execute(t, -1);
		exitstat();
	}
	xfree(cp);
	(void) close(pvec[1]);
	do {
		int cnt = 0;
		for (;;) {
			if (icnt == 0) {
				ip = ibuf;
				icnt = read_(pvec[0], ip, CSHBUFSIZ);
				if (icnt <= 0) {
					c = -1;
					break;
				}
			}
			if (hadnl)
				break;
			--icnt;
			c = (*ip++ & TRIM);
			if (c == 0)
				break;
			if (c == '\n') {
				/*
				 * Continue around the loop one
				 * more time, so that we can eat
				 * the last newline without terminating
				 * this word.
				 */
				hadnl = 1;
				continue;
			}
			if (!quoted && issp(c))
				break;
			cnt++;
			psave(c | quoted);
		}
		/*
		 * Unless at end-of-file, we will form a new word
		 * here if there were characters in the word, or in
		 * any case when we take text literally.  If
		 * we didn't make empty words here when literal was
		 * set then we would lose blank lines.
		 */
		if (c != -1 && (cnt || literal))
			pword();
		hadnl = 0;
	} while (c >= 0);
#ifdef GDEBUG
	shprintf("done in backeval, pvec: %d %d\n", pvec[0], pvec[1]);
	shprintf("also c = %c <%x>\n", c, c);
#endif
	(void) close(pvec[0]);
	pwait();
	prestjob();
}

static void
psave(wchar_t c)
{
#ifdef TRACE
	tprintf("TRACE- psave()\n");
#endif

	if (--pnleft <= 0)
		err_word2long();
	*pargcp++ = c;
}

static void
pword(void)
{
#ifdef TRACE
	tprintf("TRACE- pword()\n");
#endif

	psave(0);
	if (pargc == gavsiz)
		error(gettxt(_SGI_DMMX_csh_toomleft, "Too many words from ``"));
	pargv[pargc++] = savestr(pargs);
	pargv[pargc] = NOSTR;
#ifdef GDEBUG
	shprintf("got word %s\n", pargv[pargc-1]);
#endif
	pargcp = pargs;
	pnleft = CSHBUFSIZ - 4;
}
