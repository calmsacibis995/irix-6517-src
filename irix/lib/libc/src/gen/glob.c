/* glob.c
 *
 *      generate pathnames matching a pattern
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ident "$Revision: 1.20 $"

#define _GLOB_INTERNAL			/* keep out static functions */
#include <glob.h>
#ifdef __STDC__
	#pragma weak glob = _glob
	#pragma weak globfree = _globfree
	#pragma weak nglob = __nglob
	#pragma weak nglobfree = __nglobfree
#endif

/*
 * glob(3) -- a superset of the one defined in POSIX 1003.2.
 *
 * The [!...] convention to negate a range is supported (SysV, Posix, ksh).
 *
 * Optional extra services, controlled by flags not defined by POSIX:
 *
 * GLOB_QUOTE:
 *	Escaping convention: \ inhibits any special meaning the following
 *	character might have (except \ at end of string is retained).
 * GLOB_MAGCHAR:
 *	Set in gl_flags if pattern contained a globbing character.
 * GLOB_NOMAGIC:
 *	Same as GLOB_NOCHECK, but it will only append pattern if it did
 *	not contain any magic characters.  [Used in csh style globbing]
 * GLOB_TILDE:
 *	expand ~user/foo to the /home/dir/of/user/foo
 * GLOB_BRACE:
 *	expand {1,2}{a,b} to 1a 1b 2a 2b 
 * gl_matchc:
 *	Number of matches in the current invocation of glob.
 */

#include "synonyms.h"
#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 *  These below defines were used in the banyan release of IRIX 6.2.
 *  These defines still need to be carried for backward compatibilty.
 *  In order for MIPS ABI BB2.0 compliance the new values for the below
 *  defines are defined within <glob.h>.
 */
#define	OGLOB_NOMATCH	-2
#define	OGLOB_ABORTED	-4

#define	DOLLAR		'$'
#define	DOT		'.'
#define	EOS		'\0'
#define	LBRACKET	'['
#define	NOT		'!'
#define	QUESTION	'?'
#define	QUOTE		'\\'
#define	RANGE		'-'
#define	RBRACKET	']'
#define	SEP		'/'
#define	STAR		'*'
#define	TILDE		'~'
#define	UNDERSCORE	'_'
#define	LBRACE		'{'
#define	RBRACE		'}'
#define	SLASH		'/'
#define	COMMA		','

#ifndef DEBUG

#define	M_QUOTE		0x8000
#define	M_PROTECT	0x4000
#define	M_MASK		0xffff
#define	M_ASCII		0x00ff

typedef u_short Char;

#else

#define	M_QUOTE		0x80
#define	M_PROTECT	0x40
#define	M_MASK		0xff
#define	M_ASCII		0x7f

typedef char Char;

#endif


#define	CHAR(c)		((Char)((c)&M_ASCII))
#define	META(c)		((Char)((c)|M_QUOTE))
#define	M_ALL		META('*')
#define	M_END		META(']')
#define	M_NOT		META('!')
#define	M_ONE		META('?')
#define	M_RNG		META('-')
#define	M_SET		META('[')
#define	ismeta(c)	(((c)&M_QUOTE) != 0)


static int	 compare (const void *, const void *);
static void	 g_Ctoc (const Char *, char *);
static int	 g_lstat (Char *, struct stat64 *);
static DIR	*g_opendir (Char *);
static Char	*g_strchr (Char *, int);
#ifdef notdef
static Char	*g_strcat (Char *, const Char *);
#endif
static int	 g_stat (Char *, struct stat64 *);
static int	 glob0 (const Char *, sgi_glob_t *, int);
static int	 glob1 (Char *, sgi_glob_t *, int);
static int	 glob2 (Char *, Char *, Char *, sgi_glob_t *, int);
static int	 glob3 (Char *, Char *, Char *, Char *, sgi_glob_t *, int);
static int	 globextend (const Char *, sgi_glob_t *);
static const Char *	 globtilde (const Char *, Char *, sgi_glob_t *);
static int	 globexp1 (const Char *, sgi_glob_t *, int);
static int	 globexp2 (const Char *, const Char *, sgi_glob_t *, int *, int);
static int	 match (Char *, Char *, Char *);
#ifdef DEBUG
static void	 qprintf (const char *, Char *);
#endif

static int __glob(const char *, int, int (*)(const char *, int), sgi_glob_t *, int);
static void __globfree(sgi_glob_t *);

void
globfree(sgi_glob_t *pglob)
{
	/*
	 * This is the backward compatible version of globfree().  This will
	 * always get called from an application which was link-loaded using
	 * a previous version of libc.so.
	 */
	__globfree(pglob);
}

/*
 * BB3.0 version
 */
/* ARGSUSED */
void
_xglobfree(const int ver, glob_t *pglob)
{
	__nglobfree(pglob);
}

void
__nglobfree(glob_t *pglob)
{
	sgi_glob_t	oglob;
	sgi_glob_t	*oglobp;

	/*
	 * Convert the MIPS ABI/XPG4 version of the glob_t structure into 
	 * the version used for SGI's implementation before 
	 * calling __globfree().
	 */

	/* new to old conversion */
	oglobp = &oglob;
	oglobp->gl_pathc = pglob->gl_pathc;
	oglobp->gl_pathv = pglob->gl_pathv;
	oglobp->gl_offs = pglob->gl_offs;
		
	oglobp->gl_matchc = (size_t)pglob->gl_pad[0];
	oglobp->gl_flags = (size_t)pglob->gl_pad[1];
	oglobp->gl_errfunc = (int (*)(const char *, int))pglob->gl_pad[2];

	__globfree(oglobp);

	/* old to new conversion */
	pglob->gl_pathc = oglobp->gl_pathc;
	pglob->gl_pathv = oglobp->gl_pathv;
	pglob->gl_offs = oglobp->gl_offs;
	pglob->gl_pad[0] = (void *)oglobp->gl_matchc;
	pglob->gl_pad[1] = (void *)oglobp->gl_flags;
	pglob->gl_pad[2] = (void *)oglobp->gl_errfunc;
}

int
glob(const char *pattern, int flags, int (*errfunc)(const char *, int),
			sgi_glob_t *pglob)
{
	/*
	 * This is the backward compatible version of glob().  This will
	 * always get called from an application which was link-loaded using
	 * a previous version of libc.so.
	 *
	 * add argument to indicate old or new version of glob():
	 *
	 *	zero means old; one means new
	 */
	return __glob(pattern, flags, errfunc, pglob, (int)0);
}

/*
 * BB3.0 version
 */
/* ARGSUSED */
int
_xglob(const int ver, const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob)
{
	return __nglob(pattern, flags, errfunc, pglob);
}

int
__nglob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob)
{
	sgi_glob_t	oglob;
	sgi_glob_t	*oglobp;
	int	retval = 0;

	/*
	 * Convert the MIPS ABI/XPG4 version of the glob_t structure into 
	 * the version used for SGI's implementation before 
	 * calling __glob().
	 */

	/* new to old conversion */
	oglobp = &oglob;
	oglobp->gl_pathc = pglob->gl_pathc;
	oglobp->gl_pathv = pglob->gl_pathv;
	oglobp->gl_offs = pglob->gl_offs;
		
	oglobp->gl_matchc = (size_t)pglob->gl_pad[0];
	oglobp->gl_flags = (size_t)pglob->gl_pad[1];
	oglobp->gl_errfunc = (int (*)(const char *, int))pglob->gl_pad[2];
	/*
	 * add argument to indicate old or new version of regcomp():
	 *
	 *	zero means old; one means new
	 */
	retval = __glob(pattern, flags, errfunc, oglobp, (int)1);

	/* old to new conversion */
	pglob->gl_pathc = oglobp->gl_pathc;
	pglob->gl_pathv = oglobp->gl_pathv;
	pglob->gl_offs = oglobp->gl_offs;
	pglob->gl_pad[0] = (void *)oglobp->gl_matchc;
	pglob->gl_pad[1] = (void *)oglobp->gl_flags;
	pglob->gl_pad[2] = (void *)oglobp->gl_errfunc;

	return(retval);
}

static int
__glob(const char *pattern, int flags, int (*errfunc)(const char *, int), sgi_glob_t *pglob, int new)
{
	const u_char *patnext;
	int c;
	Char *bufnext, *bufend, patbuf[MAXPATHLEN+1];

	patnext = (u_char *) pattern;
	if (!(flags & GLOB_APPEND)) {
		pglob->gl_pathc = 0;
		pglob->gl_pathv = NULL;
		if (!(flags & GLOB_DOOFFS))
			pglob->gl_offs = 0;
	}
	pglob->gl_flags = flags & ~GLOB_MAGCHAR;
	pglob->gl_errfunc = errfunc;
	pglob->gl_matchc = 0;

	bufnext = patbuf;
	bufend = bufnext + MAXPATHLEN;
	if (flags & GLOB_QUOTE) {
		/* Protect the quoted characters. */
		while (bufnext < bufend && (c = *patnext++) != EOS) 
			if (c == QUOTE) {
				if ((c = *patnext++) == EOS) {
					c = QUOTE;
					--patnext;
				}
				*bufnext++ = c | M_PROTECT;
			}
			else
				*bufnext++ = c;
	}
	else 
	    while (bufnext < bufend && (c = *patnext++) != EOS) 
		    *bufnext++ = c;
	*bufnext = EOS;

	if (flags & GLOB_BRACE)
	    return globexp1(patbuf, pglob, new);
	else
	    return glob0(patbuf, pglob, new);
}

/*
 * Expand recursively a glob {} pattern. When there is no more expansion
 * invoke the standard globbing routine to glob the rest of the magic
 * characters
 */
static int globexp1(const Char *pattern, sgi_glob_t *pglob, int new)
{
	const Char* ptr = pattern;
	int rv;

	/* Protect a single {}, for find(1), like csh */
	if (pattern[0] == LBRACE && pattern[1] == RBRACE && pattern[2] == EOS)
		return glob0(pattern, pglob, new);

	while ((ptr = (const Char *) g_strchr((Char *) ptr, LBRACE)) != NULL)
		if (!globexp2(ptr, pattern, pglob, &rv, new))
			return rv;

	return glob0(pattern, pglob, new);
}


/*
 * Recursive brace globbing helper. Tries to expand a single brace.
 * If it succeeds then it invokes globexp1 with the new pattern.
 * If it fails then it tries to glob the rest of the pattern and returns.
 */
static int globexp2(const Char *ptr, const Char *pattern, sgi_glob_t *pglob,
		int *rv, int new)
{
	int     i;
	Char   *lm, *ls;
	const Char *pe, *pm, *pl;
	Char    patbuf[MAXPATHLEN + 1];

	/* copy part up to the brace */
	for (lm = patbuf, pm = pattern; pm != ptr; *lm++ = *pm++)
		continue;
	ls = lm;

	/* Find the balanced brace */
	for (i = 0, pe = ++ptr; *pe; pe++)
		if (*pe == LBRACKET) {
			/* Ignore everything between [] */
			for (pm = pe++; *pe != RBRACKET && *pe != EOS; pe++)
				continue;
			if (*pe == EOS) {
				/* 
				 * We could not find a matching RBRACKET.
				 * Ignore and just look for RBRACE
				 */
				pe = pm;
			}
		}
		else if (*pe == LBRACE)
			i++;
		else if (*pe == RBRACE) {
			if (i == 0)
				break;
			i--;
		}

	/* Non matching braces; just glob the pattern */
	if (i != 0 || *pe == EOS) {
		*rv = glob0(pattern, pglob, new);
		return 0;
	}

	for (i = 0, pl = pm = ptr; pm <= pe; pm++)
		switch (*pm) {
		case LBRACKET:
			/* Ignore everything between [] */
			for (pl = pm++; *pm != RBRACKET && *pm != EOS; pm++)
				continue;
			if (*pm == EOS) {
				/* 
				 * We could not find a matching RBRACKET.
				 * Ignore and just look for RBRACE
				 */
				pm = pl;
			}
			break;

		case LBRACE:
			i++;
			break;

		case RBRACE:
			if (i) {
			    i--;
			    break;
			}
			/* FALLTHROUGH */
		case COMMA:
			if (i && *pm == COMMA)
				break;
			else {
				/* Append the current string */
				for (lm = ls; (pl < pm); *lm++ = *pl++)
					continue;
				/* 
				 * Append the rest of the pattern after the
				 * closing brace
				 */
				for (pl = pe + 1; (*lm++ = *pl++) != EOS;)
					continue;

				/* Expand the current pattern */
#ifdef DEBUG
				qprintf("globexp2:", patbuf);
#endif
				*rv = globexp1(patbuf, pglob, new);

				/* move after the comma, to the next string */
				pl = pm + 1;
			}
			break;

		default:
			break;
		}
	*rv = 0;
	return 0;
}



/*
 * expand tilde from the passwd file.
 */
static const Char *
globtilde(const Char *pattern, Char *patbuf, sgi_glob_t *pglob)
{
	struct passwd *pwd;
	char *h;
	const Char *p;
	Char *b;

	if (*pattern != TILDE || !(pglob->gl_flags & GLOB_TILDE))
		return pattern;

	/* Copy up to the end of the string or / */
	for (p = pattern + 1, h = (char *) patbuf; *p && *p != SLASH; 
	     *h++ = *p++)
		continue;

	*h = EOS;

	if (((char *) patbuf)[0] == EOS) {
		/* 
		 * handle a plain ~ or ~/ by expanding $HOME 
		 * first and then trying the password file
		 */
		if ((h = getenv("HOME")) == NULL) {
			if ((pwd = getpwuid(getuid())) == NULL)
				return pattern;
			else
				h = pwd->pw_dir;
		}
	}
	else {
		/*
		 * Expand a ~user
		 */
		if ((pwd = getpwnam((char*) patbuf)) == NULL)
			return pattern;
		else
			h = pwd->pw_dir;
	}

	/* Copy the home directory */
	for (b = patbuf; *h; *b++ = *h++)
		continue;
	
	/* Append the rest of the pattern */
	while ((*b++ = *p++) != EOS)
		continue;

	return patbuf;
}
	

/*
 * The main glob() routine: compiles the pattern (optionally processing
 * quotes), calls glob1() to do the real pattern matching, and finally
 * sorts the list (unless unsorted operation is requested).  Returns 0
 * if things went well, nonzero if errors occurred.  It is not an error
 * to find no matches.
 */

static int
glob0(const Char *pattern, sgi_glob_t *pglob, int new)
{
	const Char *qpatnext;
	int c, err, gotescape;
	size_t oldpathc;
	Char *bufnext, patbuf[MAXPATHLEN+1];

	patbuf[0] = 0;

	qpatnext = globtilde(pattern, patbuf, pglob);
	oldpathc = pglob->gl_pathc;
	bufnext = patbuf;

	gotescape = 0;

	/* We don't need to check for buffer overflow any more. */
	while ((c = *qpatnext++) != EOS) {
		switch (c) {
		case '\\':
			if (pglob->gl_flags & GLOB_NOESCAPE) {
				*bufnext++ = CHAR(c);
				gotescape = 0;
				break;
			}
			if (gotescape) {
				*bufnext++ = CHAR(c);
				gotescape = 0;
				break;
			}
			gotescape = 1;
			break;
		case LBRACKET:
			if (gotescape) {
				*bufnext++ = LBRACKET;
				gotescape = 0;
				break;
			}
			c = *qpatnext;
			if (c == '\\') {
				++qpatnext;
				c = *qpatnext++;
				*bufnext++ = CHAR(c);
				c = *qpatnext++;
			}
			if (c == NOT)
				++qpatnext;
			if (*qpatnext == EOS ||
			    g_strchr((Char *) qpatnext+1, RBRACKET) == NULL) {
				*bufnext++ = LBRACKET;
				if (c == NOT)
					--qpatnext;
				break;
			}
			*bufnext++ = M_SET;
			if (c == NOT)
				*bufnext++ = M_NOT;
			c = *qpatnext++;
			do {
				*bufnext++ = CHAR(c);
				if (*qpatnext == RANGE &&
				    (c = qpatnext[1]) != RBRACKET) {
					*bufnext++ = M_RNG;
					*bufnext++ = CHAR(c);
					qpatnext += 2;
				}
			} while ((c = *qpatnext++) != RBRACKET);
			pglob->gl_flags |= GLOB_MAGCHAR;
			*bufnext++ = M_END;
			break;
		case QUESTION:
			if (gotescape) {
				*bufnext++ = QUESTION;
				gotescape = 0;
				break;
			}
			pglob->gl_flags |= GLOB_MAGCHAR;
			*bufnext++ = M_ONE;
			break;
		case STAR:
			if (gotescape) {
				*bufnext++ = STAR;
				gotescape = 0;
				break;
			}
			pglob->gl_flags |= GLOB_MAGCHAR;
			/* collapse adjacent stars to one, 
			 * to avoid exponential behavior
			 */
			if (bufnext == patbuf || bufnext[-1] != M_ALL)
			    *bufnext++ = M_ALL;
			break;
		default:
			*bufnext++ = CHAR(c);
			gotescape = 0;
			break;
		}
	}
	*bufnext = EOS;
#ifdef DEBUG
	qprintf("glob0:", patbuf);
#endif
	if ((err = glob1(patbuf, pglob, new)) != 0)
		return(err);
	/*
	 * If there was no match we are going to append the pattern 
	 * if GLOB_NOCHECK was specified or if GLOB_NOMAGIC was specified
	 * and the pattern did not contain any magic characters
	 * GLOB_NOMAGIC is there just for compatibility with csh.
	 */
	if (pglob->gl_pathc == oldpathc && 
	    ((pglob->gl_flags & GLOB_NOCHECK) || 
	      ((pglob->gl_flags & GLOB_NOMAGIC) &&
	       !(pglob->gl_flags & GLOB_MAGCHAR))))
	{
		return(globextend(pattern, pglob));
	}
	qsort(pglob->gl_pathv + pglob->gl_offs + oldpathc,
		pglob->gl_pathc - oldpathc, sizeof(char *), compare);
	return(0);
}
static int
compare(const void *p, const void *q)
{
	return(strcoll(*(char **)p, *(char **)q));
}

static int
glob1(Char *pattern, sgi_glob_t *pglob, int new)
{
	Char pathbuf[MAXPATHLEN+1];

	/* A null pathname is invalid -- POSIX 1003.1 sect. 2.4. */
	if (*pattern == EOS)
		return(0);
	return(glob2(pathbuf, pathbuf, pattern, pglob, new));
}

/*
 * The functions glob2 and glob3 are mutually recursive; there is one level
 * of recursion for each segment in the pattern that contains one or more
 * meta characters.
 */
static int
glob2(Char *pathbuf, Char *pathend, Char *pattern, sgi_glob_t *pglob, int new)
{
	struct stat64 sb;
	Char *p, *q;
	int anymeta;

	/*
	 * Loop over pattern segments until end of pattern or until
	 * segment with meta character found.
	 */
	for (anymeta = 0;;) {
		if (*pattern == EOS) {		/* End of pattern? */
			*pathend = EOS;
			if (g_lstat(pathbuf, &sb)) {
				if (!pglob->gl_matchc && 
					   !(pglob->gl_flags & GLOB_NOCHECK))
					if(!new)
						return(OGLOB_NOMATCH);
					else
						return(GLOB_NOMATCH);
				return(0);
			}
			if (((pglob->gl_flags & GLOB_MARK) &&
			    pathend[-1] != SEP) && (S_ISDIR(sb.st_mode)
			    || (S_ISLNK(sb.st_mode) &&
			    (g_stat(pathbuf, &sb) == 0) &&
			    S_ISDIR(sb.st_mode)))) {
				*pathend++ = SEP;
				*pathend = EOS;
			}
			++pglob->gl_matchc;
			return(globextend(pathbuf, pglob));
		}

		/* Find end of next segment, copy tentatively to pathend. */
		q = pathend;
		p = pattern;
		while (*p != EOS && *p != SEP) {
			if (ismeta(*p))
				anymeta = 1;
			*q++ = *p++;
		}

		if (!anymeta) {		/* No expansion, do next segment. */
			pathend = q;
			pattern = p;
			while (*pattern == SEP)
				*pathend++ = *pattern++;
		} else			/* Need expansion, recurse. */
			return(glob3(pathbuf, pathend, pattern, p, pglob, new));
	}
	/* NOTREACHED */
}

static int
glob3(Char *pathbuf, Char *pathend, Char *pattern, Char *restpattern,
		sgi_glob_t *pglob, int new)
{
	register dirent64_t *dp;
	DIR *dirp;
	int err;
	char buf[MAXPATHLEN];

	*pathend = EOS;
	setoserror(0);
	    
	if ((dirp = g_opendir(pathbuf)) == NULL) {
		/* TODO: don't call for ENOENT or ENOTDIR? */
		if (pglob->gl_errfunc) {
			g_Ctoc(pathbuf, buf);
			if (pglob->gl_errfunc(buf, errno) ||
			    pglob->gl_flags & GLOB_ERR)
				if(!new)
					return (OGLOB_ABORTED);
				else
					return (GLOB_ABORTED);
		}
		if(!new)
			if (pglob->gl_flags & GLOB_ERR)
				return (OGLOB_ABORTED);
		else
			if (pglob->gl_flags & GLOB_ERR)
				return (GLOB_ABORTED);
		return(0);
	}

	err = 0;

	/* Search directory for matching names. */
	while (dp = readdir64(dirp)) {
		register u_char *sc;
		register Char *dc;

		/* Initial DOT must be matched literally. */
		if (dp->d_name[0] == DOT && *pattern != DOT)
			continue;
		for (sc = (u_char *) dp->d_name, dc = pathend; 
		     (*dc++ = *sc++) != EOS;)
			continue;
		if (!match(pathend, pattern, restpattern)) {
			*pathend = EOS;
			continue;
		}
		err = glob2(pathbuf, --dc, restpattern, pglob, new);
		if (err)
			break;
	}

	closedir(dirp);
	if ((err || errno) && (pglob->gl_flags & GLOB_ERR))
		if(!new)
			return (OGLOB_ABORTED);
		else
			return (GLOB_ABORTED);
	return(err);
}


/*
 * Extend the gl_pathv member of a glob_t structure to accomodate a new item,
 * add the new item, and update gl_pathc.
 *
 * This assumes the BSD realloc, which only copies the block when its size
 * crosses a power-of-two boundary; for v7 realloc, this would cause quadratic
 * behavior.
 *
 * Return 0 if new item added, error code if memory couldn't be allocated.
 *
 * Invariant of the glob_t structure:
 *	Either gl_pathc is zero and gl_pathv is NULL; or gl_pathc > 0 and
 *	gl_pathv points to (gl_offs + gl_pathc + 1) items.
 */
static int
globextend(const Char *path, sgi_glob_t *pglob)
{
	register char **pathv;
	register int i;
	size_t newsize;
	char *copy;
	const Char *p;

	newsize = sizeof(*pathv) * (2 + pglob->gl_pathc + pglob->gl_offs);
	pathv = pglob->gl_pathv ? 
		    realloc((void *)pglob->gl_pathv, newsize) :
		    malloc(newsize);
	if (pathv == NULL)
		return(GLOB_NOSPACE);

	if (pglob->gl_pathv == NULL && pglob->gl_offs > 0) {
		/* first time around -- clear initial gl_offs items */
		pathv += pglob->gl_offs;
		for (i = (int) pglob->gl_offs; --i >= 0;)
			*--pathv = NULL;
	}
	pglob->gl_pathv = pathv;

	for (p = path; *p++;)
		continue;
	if ((copy = malloc(p - path)) != NULL) {
		g_Ctoc(path, copy);
		pathv[pglob->gl_offs + pglob->gl_pathc++] = copy;
	}
	pathv[pglob->gl_offs + pglob->gl_pathc] = NULL;
	return(copy == NULL ? GLOB_NOSPACE : 0);
}


/*
 * pattern matching function for filenames.  Each occurrence of the *
 * pattern causes a recursion level.
 */
static int
match(register Char *name, register Char *pat, register Char *patend)
{
	int ok, negate_range;
	Char c, k;

	while (pat < patend) {
		c = *pat++;
		switch (c & M_MASK) {
		case M_ALL:
			if (pat == patend)
				return(1);
			do 
			    if (match(name, pat, patend))
				    return(1);
			while (*name++ != EOS);
			return(0);
		case M_ONE:
			if (*name++ == EOS)
				return(0);
			break;
		case M_SET:
			ok = 0;
			if ((k = *name++) == EOS)
				return(0);
			if ((negate_range = ((*pat & M_MASK) == M_NOT)) != EOS)
				++pat;
			while (((c = *pat++) & M_MASK) != M_END)
				if ((*pat & M_MASK) == M_RNG) {
					if (c <= k && k <= pat[1])
						ok = 1;
					pat += 2;
				} else if (c == k)
					ok = 1;
			if (ok == negate_range)
				return(0);
			break;
		default:
			if (*name++ != c)
				return(0);
			break;
		}
	}
	return(*name == EOS);
}

/* Free allocated data belonging to a sgi_glob_t structure. */
void
__globfree(sgi_glob_t *pglob)
{
	register size_t i;
	register char **pp;

	if (pglob->gl_pathv != NULL) {
		pp = pglob->gl_pathv + pglob->gl_offs;
		for (i = pglob->gl_pathc; i--; ++pp)
			if (*pp)
				free(*pp);
		free(pglob->gl_pathv);
	}
}

static DIR *
g_opendir(register Char *str)
{
	char buf[MAXPATHLEN];

	if (!*str)
		strcpy(buf, ".");
	else
		g_Ctoc(str, buf);

	return(opendir(buf));
}

static int
g_lstat(register Char *fn, struct stat64 *sb)
{
	char buf[MAXPATHLEN];

	g_Ctoc(fn, buf);
	return(lstat64(buf, sb));
}

static int
g_stat(register Char *fn, struct stat64 *sb)
{
	char buf[MAXPATHLEN];

	g_Ctoc(fn, buf);
	return(stat64(buf, sb));
}

static Char *
g_strchr(Char *str, int ch)
{
	do {
		if (*str == ch)
			return (str);
	} while (*str++);
	return (NULL);
}

#ifdef notdef
static Char *
g_strcat(Char *dst, const Char* src)
{
	Char *sdst = dst;

	while (*dst++)
		continue;
	--dst;
	while((*dst++ = *src++) != EOS)
	    continue;

	return (sdst);
}
#endif

static void
g_Ctoc(register const Char *str, char *buf)
{
	register char *dc;

	for (dc = buf; (*dc++ = *str++) != EOS;)
		continue;
}

#ifdef DEBUG
static void 
qprintf(const char *str, register Char *s)
{
	register Char *p;

	(void)printf("%s:\n", str);
	for (p = s; *p; p++)
		(void)printf("%c", CHAR(*p));
	(void)printf("\n");
	for (p = s; *p; p++)
		(void)printf("%c", *p & M_PROTECT ? '"' : ' ');
	(void)printf("\n");
	for (p = s; *p; p++)
		(void)printf("%c", ismeta(*p) ? '_' : ' ');
	(void)printf("\n");
}
#endif
