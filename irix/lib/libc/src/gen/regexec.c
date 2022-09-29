/* regexec.c
 * $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/gen/RCS/regexec.c,v 1.11 1997/12/12 18:43:26 danc Exp $
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
 * Copyright (c) 1992, 1993, 1994 Henry Spencer.
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer.
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
 *
 *	@(#)regexec.c	8.3 (Berkeley) 3/20/94
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)regexec.c	8.3 (Berkeley) 3/20/94";
#endif /* LIBC_SCCS and not lint */

#ident "$Revision: 1.11 $"


/*
 * the outer shell of regexec()
 *
 * This file includes engine.c *twice*, after muchos fiddling with the
 * macros that code uses.  This lets the same code operate on two different
 * representations for state sets.
 */
#include <sys/types.h>
#include "synonyms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <wchar.h>
#include <iconv.h>              /* used in engine.c to convert input to UCS-4 */
#include <alloca.h>
#define	_REGCOMP_INTERNAL	/* keep out static functions */
#include <regex.h>

#ifdef __STDC__
        #pragma weak regexec = _regexec
#endif

#include "utils.h"
#include "regex2.h"

#ifndef NDEBUG
static int nope = 0;		/* for use in asserts; shuts lint up */
#endif

/* macros for manipulating states, small version */
#define	states	long
#define	states1	states		/* for later use in regexec() decision */
#define	CLEAR(v)	((v) = 0L)
#define	SET0(v, n)	((v) &= ~(1L << (ulong_t)(n)))
#define	SET1(v, n)	((v) |= (1L << (ulong_t)(n)))
#define	ISSET(v, n)	((v) & (1L << (ulong_t)(n)))
#define	ASSIGN(d, s)	((d) = (states)(s))
#define	EQ(a, b)	((a) == (states)(b))
#define	STATEVARS	int dummy	/* dummy version */
#define	STATESETUP(m, n, nw)	/* nothing */
#define	STATETEARDOWN(m)	/* nothing */
#define	SETUP(v)	((v) = 0L)
#define	onestate	long
#define	INIT(o, n)	((o) = (long)((ulong_t)1 << (ulong_t)(n)))
#define	INC(o)	((o) <<= 1)
#define	ISSTATEIN(v, o)	((v) & (long)(o))
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define	FWD(dst, src, n)	((dst) |= ((long)(src)&(long)(here)) << (ulong_t)(n))
#define	BACK(dst, src, n)	((dst) |= ((long)(src)&(long)(here)) >> (ulong_t)(n))
#define	ISSETBACK(v, n)	((v) & ((long)here >> (ulong_t)(n)))
/* function names */
#define SNAMES			/* engine.c looks after details */

/* OLD regcomp() flags */
#define	OREG_NOSUB	0004	/* report only success/fail in regexec() */
#define	OREG_NEWLINE	0010	/* change the handling of <newline> */

/* OLD regerror() flags */
#define	OREG_NOMATCH	0001	/* regexec() failed to match */
#define	OREG_ESPACE	0014	/* out of memory */

#include "engine.c"

/* now undo things */
#undef	states
#undef	CLEAR
#undef	SET0
#undef	SET1
#undef	ISSET
#undef	ASSIGN
#undef	EQ
#undef	STATEVARS
#undef	STATESETUP
#undef	STATETEARDOWN
#undef	SETUP
#undef	onestate
#undef	INIT
#undef	INC
#undef	ISSTATEIN
#undef	FWD
#undef	BACK
#undef	ISSETBACK
#undef	SNAMES

/* macros for manipulating states, large version */
#define	states	char *
#define	CLEAR(v)	memset((void *)v, (int)0, (long)m->g->nstates)
#define	SET0(v, n)	((v)[(ulong_t)n] = 0L)
#define	SET1(v, n)	((v)[(ulong_t)n] = 1L)
#define	ISSET(v, n)	((v)[(ulong_t)n])
#define	ASSIGN(d, s)	memcpy((void *)d, (void *)s, (long)m->g->nstates)
#define	EQ(a, b)	(memcmp((void *)a, (void *)b, (long)m->g->nstates) == (int)0)
#define	STATEVARS	int vn; char *space
#define	STATESETUP(m, nv, nw)	{ \
	(m)->space = malloc((nv)*(m)->g->nstates); \
	if ((m)->space == NULL) \
		if((nw)) \
			return(REG_ESPACE); \
		else \
			return(OREG_ESPACE); \
	(m)->vn = (int)0; \
}
#define	STATETEARDOWN(m)	{ free((m)->space); }
#define	SETUP(v)	((v) = &m->space[m->vn++ * m->g->nstates])
#define	onestate	long
#define	INIT(o, n)	((o) = (long)(n))
#define	INC(o)	((o)++)
#define	ISSTATEIN(v, o)	((v)[(ulong_t)o])
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define	FWD(dst, src, n)	((dst)[(ulong_t)here+((ulong_t)n)] |= (long)(src)[(ulong_t)here])
#define	BACK(dst, src, n)	((dst)[(ulong_t)here-((ulong_t)n)] |= (long)(src)[(ulong_t)here])
#define	ISSETBACK(v, n)	((v)[(ulong_t)here - ((ulong_t)n)])
/* function names */
#define	LNAMES			/* flag */

#include "engine.c"

/*
 - regexec - interface for matching
 = extern int regexec(const regex_t *, const char *, size_t, \
 =					regmatch_t [], int);
 = #define	REG_NOTBOL	00001
 = #define	REG_NOTEOL	00002
 = #define	REG_STARTEND	00004
 = #define	REG_TRACE	00400	// tracing of execution
 = #define	REG_LARGE	01000	// force large representation
 = #define	REG_BACKR	02000	// force use of backref code
 *
 * We put this here so we can exploit knowledge of the state representation
 * when choosing which matcher to call.  Also, by this point the matchers
 * have been prototyped.
 */

static int __regexec(const sgi_regex_t *, const char *, size_t, regmatch_t *,
	int, int);

int				/* 0 success, OREG_NOMATCH failure */
regexec(const sgi_regex_t *preg, const char *string, size_t nmatch,
	sgi_regmatch_t pmatch[], int eflags)
{
	/*
	 * This is the backward compatible version of regexec().  This will
	 * always get called from an application which was link-loaded using
	 * a previous version of libc.so.
	 *
	 * add argument to indicate old or new version of regexec():
	 *
	 *	zero means old; one means new
	 */
	return  __regexec(preg, string, nmatch, (regmatch_t *)pmatch, 
			eflags, (int)0);
}

/*
 * BB3.0 version
 */
/* ARGSUSED */
int				/* 0 success, REG_NOMATCH failure */
_xregexec(int ver, const regex_t *preg, const char *string, size_t nmatch,
	regmatch_t pmatch[], int eflags)
{
	return __nregexec(preg, string, nmatch, pmatch, eflags);
}


int				/* 0 success, REG_NOMATCH failure */
__nregexec(const regex_t *preg, const char *string, size_t nmatch,
	regmatch_t pmatch[], int eflags)
{
	sgi_regex_t	oregex;
	sgi_regex_t	*oregexp;
	unsigned char *cp1, *cp2;
	int i;

	/*
	 * Convert the MIPS ABI/XPG4 version of the regex_t structure into 
	 * the version used for SGI's implementation before 
	 * calling __regexec().
	 */

	/* new to old conversion */
	oregexp = &oregex;
	oregexp->re_nsub = preg->re_nsub;
		
	/*
	 * Store the re_pad[0] element from the regex_t structure
	 * into the re_magic struct element of the sgi_regex_t structure.
	 */
	cp1 = (unsigned char *)&preg->re_pad;
	cp2 = (unsigned char *)&oregexp->re_magic;
	for(i=0; i<sizeof(oregexp->re_magic); i++)
		*cp2++ = *cp1++;

	oregexp->re_endp = (const char *)preg->re_pad[1];
        oregexp->re_g = (struct re_guts *)preg->re_pad[2];
	/*
	 * add argument to indicate old or new version of regexec():
	 *
	 *	zero means old; one means new
	 */
	return  __regexec(oregexp, string, nmatch, pmatch, eflags, (int)1);
}

static int			/* 0 success, REG_NOMATCH failure */
__regexec(const sgi_regex_t *preg, const char *string, size_t nmatch,
	regmatch_t pmatch[], int eflags, int new)
{
	register struct re_guts *g = preg->re_g;
	register int	retval;
	register int	mask;
#ifdef REDEBUG
#	define	GOODFLAGS(f)	(f)
#else
#	define	GOODFLAGS(f)	((f)&(REG_NOTBOL|REG_NOTEOL|REG_STARTEND))
#endif

	retval = REG_BADPAT;
	mask = REG_LARGE;
	
	if (preg->re_magic != MAGIC1 || g->magic != MAGIC2)
		return(retval);
	assert(!(g->iflags&BAD));
	if (g->iflags&BAD)		/* backstop for no-debug case */
		return(retval);

	eflags = GOODFLAGS(eflags);

	if (g->nstates <= CHAR_BIT*sizeof(states1) && !(eflags&mask))
		return(smatcher(g, (char *)string, nmatch, pmatch, 
			eflags, new));
	else
		return(lmatcher(g, (char *)string, nmatch, pmatch,
			eflags, new));
}
