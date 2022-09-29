/* regcomp.c
 * $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/gen/RCS/regcomp.c,v 1.25 1998/06/12 22:58:35 holzen Exp $
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
 *	@(#)regcomp.c	8.5 (Berkeley) 3/20/94
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)regcomp.c	8.5 (Berkeley) 3/20/94";
#endif /* LIBC_SCCS and not lint */

#ident "$Id: regcomp.c,v 1.25 1998/06/12 22:58:35 holzen Exp $"
#include <sys/types.h>
#include "synonyms.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <iconv.h>
#include <limits.h>
#include <stdlib.h>
#include <nl_types.h>
#include <langinfo.h>
#include <wctype.h>
#define	_REGCOMP_INTERNAL		/* keep out static functions */
#include <regex.h>

#pragma weak regcomp = _regcomp

/* OLD regcomp() flags */
#define	OREG_ICASE	0002	/* ignore case in match */
#define	OREG_NOSUB	0004	/* report only success/fail in regexec() */
#define	OREG_NEWLINE	0010	/* change the handling of <newline> */

/* OLD regerror() flags */
#define OREG_BADPAT     0002    /* invalid regular expression */
#define	OREG_ECOLLATE	0003	/* invalid collating element referenced */
#define	OREG_ECTYPE	0004	/* invalid character class type referenced */
#define	OREG_EESCAPE	0005	/* trailing \ in pattern */
#define	OREG_ESUBREG	0006	/* number in \digit invalid or in error */
#define	OREG_EBRACK	0007	/* [] imbalance */
#define	OREG_EPAREN	0010	/* \(\) or () imbalance */
#define	OREG_EBRACE	0011	/* \{\} imbalance */
#define	OREG_BADBR	0012	/* contents of \{\} invalid */
#define	OREG_ERANGE	0013	/* invalid endpoint in expression */
#define	OREG_ESPACE	0014	/* out of memory */
#define	OREG_BADRPT	0015	/* ?,*,or + not preceeded by valid r.e. */

#include "utils.h"
#include "regex2.h"

#include "cclass.h"
#include "cname.h"
#include "_locutils.h"

/* moved this to _locutils.h: #include "colldata.h" */

/*
 * parse structure, passed up and down to avoid global variables and
 * other clumsinesses
 */
struct parse {
	char *next;		/* next character in RE */
	char *end;		/* end of string (-> NUL normally) */
	int error;		/* has an error been seen? */
	sop *strip;		/* malloced strip */
	sopno ssize;		/* malloced strip size (allocated) */
	sopno slen;		/* malloced strip length (used) */
	int ncsalloc;		/* number of csets allocated */
	struct re_guts *g;
#	define	NPAREN	10	/* we need to remember () 1-9 for back refs */
	sopno pbegin[NPAREN];	/* -> ( ([0] unused) */
	sopno pend[NPAREN];	/* -> ) ([0] unused) */
};

/* ========= begin header generated by ./mkh ========= */
#ifdef __cplusplus
extern "C" {
#endif

/* === regcomp.c === */
static void p_ere(struct parse *p, int stop, int new);
static void p_ere_exp(struct parse *p, int new);
static void p_str(struct parse *p, int new);
static void p_bre(struct parse *p, int end1, int end2, int new);
static int p_simp_re(struct parse *p, int starordinary, int new);
static int p_count(struct parse *p, int new);
static void p_bracket(struct parse *p, int new);
static void p_b_term(struct parse *p, cset *cs, int new);
static void p_b_cclass(struct parse *p, cset *cs, int new);
static void p_b_eclass(struct parse *p, cset *cs, int new);
static wchar_t p_b_symbol(struct parse *p, cset *cs, int new);
static wchar_t p_b_coll_symbol(struct parse *p, int endc, cset *cs, int new);
static wchar_t othercase(wint_t ch);
static void bothcases(struct parse *p, wint_t ch, int new);
static void ordinary(struct parse *p, wint_t ch, int new);
static void nonnewline(struct parse *p, int new);
static void repeat(struct parse *p, sopno start, int from, int to, int new);
static int seterr(struct parse *p, int e);
static cset *allocset(struct parse *p, int new);
static void freeset(register struct parse *p, register cset *cs);
static int freezeset(register struct parse *p, register cset *cs);
static int firstch(struct parse *p, cset *cs);
static int nch(struct parse *p, cset *cs);
static void mcadd(struct parse *p, cset *cs, char *cp, int len, int new);
#ifdef notdef
static void mcsub(cset *cs, char *cp);
static int mcin(cset *cs, char *cp);
static char *mcfind(cset *cs, char *cp);
#endif
/*static void mcinvert(struct parse *p, cset *cs); */
static void mccase(struct parse *p, cset *cs);
static void wccase(struct parse *p, cset *cs, int new);
static int isinsets(struct re_guts *g, int c);
static int samesets(struct re_guts *g, int c1, int c2);
static void categorize(struct parse *p, struct re_guts *g);
static sopno dupl(struct parse *p, sopno start, sopno finish, int new);
static void doemit(struct parse *p, sop op, size_t opnd, int new);
static void doinsert(struct parse *p, sop op, size_t opnd, sopno pos, int new);
static void dofwd(struct parse *p, sopno pos, sop value);
static void enlarge(struct parse *p, sopno size, int new);
static void stripsnug(struct parse *p, struct re_guts *g, int new);
static void findmust(struct parse *p, struct re_guts *g);
static sopno pluscount(struct parse *p, struct re_guts *g);

/* New Prototypes */
static void setopt(struct parse *p, cset *cs, int new);
static void mbchar(struct parse *p, char *firstbyte, int new);
static int WCadd(struct parse *p, register cset *cs, wchar_t ch, int new);

#ifdef __cplusplus
}
#endif
/* ========= end header generated by ./mkh ========= */

static const char nuls[10]={0};	/* place to point scanner in event of error */

/*
 * macros for use with parse structure
 * BEWARE:  these know that the parse structure is named `p' !!!
 */
#define	PEEK()	(*p->next)
#define	PEEK2()	(*(p->next+1))
#define	MORE()	(p->next < p->end)
#define	MORE2()	(p->next+1 < p->end)
#define	SEE(c)	(MORE() && PEEK() == (c))
#define	SEETWO(a, b)	(MORE() && MORE2() && PEEK() == (a) && PEEK2() == (b))
#define	EAT(c)	((SEE(c)) ? (NEXT(), 1) : 0)
#define	EATTWO(a, b)	((SEETWO(a, b)) ? (NEXT2(), 1) : 0)
#define	NEXT()	(p->next++)
#define	NEXT2()	(p->next += 2)
#define	NEXTn(n)	(p->next += (n))
#define	GETNEXT()	(*p->next++)
#define	SETERROR(e)	seterr(p, (e))
#define	REQUIRE(co, e)	((co) || SETERROR(e))
#define	MUSTSEE(c, e)	(REQUIRE(MORE() && PEEK() == (c), e))
#define	MUSTEAT(c, e)	(REQUIRE(MORE() && GETNEXT() == (c), e))
#define	MUSTNOTSEE(c, e)	(REQUIRE(!MORE() || PEEK() != (c), e))
#define	EMIT(op, sopnd, new)	doemit(p, (sop)(op), (size_t)(sopnd), (int)new)
#define	INSERT(op, pos, new)	doinsert(p, (sop)(op), HERE()-(pos)+1, pos, (int)new)
#define	AHEAD(pos)		dofwd(p, pos, HERE()-(pos))
#define	ASTERN(sop, pos, new)	EMIT(sop, HERE()-pos, (int)new)
#define	HERE()		(p->slen)
#define	THERE()		(p->slen - 1L)
#define	THERETHERE()	(p->slen - 2L)
#define	DROP(n)	(p->slen -= (n))

#define ISMULTI ( MB_CUR_MAX != 1 )

#ifndef NDEBUG
static int never = 0;		/* for use in asserts; shuts lint up */
#else
#define	never	0		/* some <assert.h>s have bugs too */
#endif

/*
 - regcomp - interface for parser and compilation
 = extern int regcomp(sgi_regex_t *, const char *, int);
 = #define	REG_BASIC	0000
 = #define	REG_EXTENDED	0001
 = #define	REG_ICASE	0002 (old value)
 = #define	REG_NOSUB	0004 (old value)
 = #define	REG_NEWLINE	0010 (old value)
 = #define	REG_NOSPEC	0020
 = #define	REG_PEND	0040
 = #define	REG_DUMP	0200
 */

extern	void	regfree(sgi_regex_t *);
static	int __regcomp(sgi_regex_t *, const char *, int, int);

int				/* 0 success, otherwise REG_something */
regcomp(sgi_regex_t *preg, const char *pattern, int cflags)
{
	/*
	 * This is the backward compatible version of regcomp().  This will
	 * always get called from an application which was link-loaded using
	 * a previous version of libc.so.
	 *
	 * add argument to indicate old or new version of regcomp():
	 *
	 *	zero means old; one means new
	 */
	return __regcomp(preg, pattern, cflags, (int)0);
}

/*
 * BB3.0 version
 */
/* ARGSUSED */
int				/* 0 success, otherwise REG_something */
_xregcomp(int ver, regex_t *preg, const char *pattern, int cflags)
{
	return __nregcomp(preg, pattern, cflags);
}

int				/* 0 success, otherwise REG_something */
__nregcomp(regex_t *preg, const char *pattern, int cflags)
{
	sgi_regex_t	oregex;
	sgi_regex_t	*oregexp;
	unsigned char *cp1, *cp2;
	int i;
	int	retval = 0;

	/*
	 * Convert the MIPS ABI/XPG4 version of the regex_t structure into 
	 * the version used for SGI's implementation before 
	 * calling __regcomp().
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
	 * add argument to indicate old or new version of regcomp():
	 *
	 *	zero means old; one means new
	 */
	retval = __regcomp(oregexp, pattern, cflags, (int)1);

	/* old to new conversion */
	preg->re_nsub = oregexp->re_nsub;

	/*
	 * Store the re_magic element from the sgi_regex_t structure
	 * into the re_pad[0] struct element of the regex_t structure.
	 */
	cp1 = (unsigned char *)&preg->re_pad;
	cp2 = (unsigned char *)&oregexp->re_magic;
	for(i=0; i<sizeof(oregexp->re_magic); i++)
		*cp1++ = *cp2++;

	preg->re_pad[1] = (unsigned char *)oregexp->re_endp;
	preg->re_pad[2] = (unsigned char *)oregexp->re_g;

	return(retval);
}

static int			/* 0 success, otherwise REG_something */
__regcomp(sgi_regex_t *preg, const char *pattern, int cflags, int new)
{
	struct parse pa;
	register struct re_guts *g;
	register struct parse *p = &pa;
	register int i;
	register size_t len;
	long tmplong;

#ifndef REDEBUG
	cflags = cflags &~ REG_DUMP;
#endif
	if ((cflags&REG_EXTENDED) && (cflags&REG_NOSPEC))
		return(REG_INVARG);

	if (cflags&REG_PEND) {
		if (preg->re_endp < pattern)
			return(REG_INVARG);
		len = preg->re_endp - pattern;
	} else
		len = strlen((char *)pattern);

	/* do the mallocs early so failure handling is easy */
	g = (struct re_guts *)malloc(sizeof(struct re_guts) +
							(NC-1)*sizeof(cat_t));
	if (!new)
		if (g == NULL)
			return(OREG_ESPACE);
	else
		if (g == NULL)
			return(REG_ESPACE);
	p->ssize = (sopno)(len/(size_t)2*(size_t)3 + (size_t)1); /* ugh */
	p->strip = (sop *)malloc(p->ssize * sizeof(sop));
	p->slen = 0;
	if (p->strip == NULL) {
		free((char *)g);
		if (!new)
			return(OREG_ESPACE);
		else
			return(REG_ESPACE);
	}

	/* set things up */
	p->g = g;
	p->next = (char *)pattern;	/* convenience; we do not modify it */
	p->end = p->next + len;
	p->error = 0;
	p->ncsalloc = 0;
	for (i = 0; i < NPAREN; i++) {
		p->pbegin[i] = 0;
		p->pend[i] = 0;
	}
	g->csetsize = NC;
	g->sets = NULL;
	g->setbits = NULL;
	g->ncsets = 0;

        g->nmbs   = 0;       
	g->mbslen = (long) len;
        g->mbs    = (wchar_t *) malloc(g->mbslen * sizeof(wchar_t));
	if (g->mbs== NULL) {
		free((char *)g);
	        free((char *) p->strip);
		if (!new)
			return(OREG_ESPACE);
		else
			return(REG_ESPACE);
	}

	g->cflags = cflags;
	g->iflags = 0;
	g->nbol = 0;
	g->neol = 0;
	g->must = NULL;
	g->mlen = 0;
	g->nsub = 0;
	g->ncategories = 1;	/* category 0 is "everything else" */
	g->categories = &g->catspace[-(CHAR_MIN)];
	(void) memset((char *)g->catspace, 0, NC*sizeof(cat_t));
	g->backrefs = 0;

	/* do it */
	EMIT(OEND, 0L, new);
	g->firststate = THERE();
	if (cflags&REG_EXTENDED)
		p_ere(p, OUT, new);
	else if (cflags&REG_NOSPEC)
		p_str(p, new);
	else
		p_bre(p, OUT, OUT, new);
	EMIT(OEND, 0L, new);
	g->laststate = THERE();

	/* tidy up loose ends and fill things in */
	categorize(p, g);
	stripsnug(p, g, new);
	findmust(p, g);
	g->nplus = pluscount(p, g);
	g->magic = MAGIC2;
	preg->re_nsub = g->nsub;
	preg->re_g = g;
	preg->re_magic = MAGIC1;

	/* readjust multibyte array size */
	if (p->g->nmbs == 0)
	{
	    free ( (char * ) p->g->mbs );
	     p->g->mbs = NULL;
	}
	else
	{
	  tmplong = (long)p->g->nmbs;
	  if (tmplong < p->g->mbslen)
	  {
	    p->g->mbs = realloc(p->g->mbs, p->g->nmbs * (sizeof(wchar_t)));
	    if (p->g->mbs == NULL)
		if (!new)
			SETERROR(OREG_ESPACE);
		else
			SETERROR(REG_ESPACE);
	  }
	}

#ifndef REDEBUG
	/* not debugging, so can't rely on the assert() in regexec() */
	if (g->iflags&BAD)
		SETERROR(REG_ASSERT);
#endif

	/* win or lose, we're done */
	if (p->error != 0)	/* lose */
		regfree(preg);
	return(p->error);
}

/*
 - p_ere - ERE parser top level, concatenation and alternation
 == static void p_ere(register struct parse *p, int stop, int new);
 */
static void
p_ere(register struct parse *p,
	int stop, int new)	/* character this ERE should end at */
{
	register char c;
	register sopno prevback;
	register sopno prevfwd;
	register sopno conc;
	register int first = 1;		/* is this the first alternative? */

	for (;;) {
		/* do a bunch of concatenated expressions */
		conc = HERE();
		while (MORE() && (c = PEEK()) != '|' && c != stop)
			p_ere_exp(p, new);

		if (!EAT('|'))
			break;		/* NOTE BREAK OUT */

		if (first) {
			INSERT(OCH_, conc, new);	/* offset is wrong */
			prevfwd = conc;
			prevback = conc;
			first = 0;
		}
		ASTERN(OOR1, prevback, new);
		prevback = THERE();
		AHEAD(prevfwd);			/* fix previous offset */
		prevfwd = HERE();
		EMIT(OOR2, 0L, new);		/* offset is very wrong */
	}

	if (!first) {		/* tail-end fixups */
		AHEAD(prevfwd);
		ASTERN(O_CH, prevback, new);
	}

	assert(!MORE() || SEE(stop));
}

/*
 - p_ere_exp - parse one subERE, an atom possibly followed by a repetition op
 == static void p_ere_exp(register struct parse *p);
 */
static void
p_ere_exp(register struct parse *p, int new)
{
	register char c;
	register sopno pos;
	register int count;
	register int count2;
	register sopno subno;
	int wascaret = 0;

	assert(MORE());		/* caller should have ensured this */
	c = GETNEXT();

	pos = HERE();
	switch (c) {
	case '(':
		if (!new) {
			REQUIRE(MORE(), OREG_EPAREN);
		} else {
			REQUIRE(MORE(), REG_EPAREN);
		}
		p->g->nsub++;
		subno = (sopno)p->g->nsub;
		if (subno < (sopno)NPAREN)
			p->pbegin[subno] = HERE();
		EMIT(OLPAREN, subno, new);
		if (!SEE(')'))
			p_ere(p, ')', new);
		if (subno < (sopno)NPAREN) {
			p->pend[subno] = HERE();
			assert(p->pend[subno] != 0);
		}
		EMIT(ORPAREN, subno, new);
		if(!new) {
			MUSTEAT(')', OREG_EPAREN);
		} else {
			MUSTEAT(')', REG_EPAREN);
		}
		break;
#ifdef POSIX_MISTAKE
	case ')':		/* happens only if no current unmatched ( */
		/*
		 * You may ask, why the ifndef?  Because I didn't notice
		 * this until slightly too late for 1003.2, and none of the
		 * other 1003.2 regular-expression reviewers noticed it at
		 * all.  So an unmatched ) is legal POSIX, at least until
		 * we can get it fixed.
		 */
		if(!new) {
			SETERROR(OREG_EPAREN);
		} else {
			SETERROR(REG_EPAREN);
		}
		break;
#endif
	case '^':
		EMIT(OBOL, 0L, new);
		p->g->iflags |= USEBOL;
		p->g->nbol++;
		wascaret = 1;
		break;
	case '$':
		EMIT(OEOL, 0L, new);
		p->g->iflags |= USEEOL;
		p->g->neol++;
		break;
	case '|':
		SETERROR(REG_EMPTY);
		break;
	case '*':
	case '+':
	case '?':
		if(!new) {
			SETERROR(OREG_BADRPT);
		} else {
			SETERROR(REG_BADRPT);
		}
		break;
	case '.':
		if(!new) {
			if (p->g->cflags&OREG_NEWLINE)
				nonnewline(p, new);
			else
				EMIT(OANY, 0L, new);
		} else {
			if (p->g->cflags&REG_NEWLINE)
				nonnewline(p, new);
			else
				EMIT(OANY, 0L, new);
		}
		break;
	case '[':
		p_bracket(p, new);
		break;
	case '\\':
		if(!new) {
			REQUIRE(MORE(), OREG_EESCAPE);
		} else {
			REQUIRE(MORE(), REG_EESCAPE);
		}
		c = GETNEXT();

		if (ISMULTI)  /* handle supplementary multibyte character */
                   mbchar(p, p->next-1, new);
                else                         /* handle ordinary character */
		   ordinary(p, c, new);
		break;
	case '{':	/* okay as ordinary except if digit follows */
		if(!new) {
			REQUIRE(MORE(), OREG_EBRACE);
		} else {
			REQUIRE(MORE(), REG_EBRACE);
		}
		if (isdigit(PEEK()))
			break;
		/* FALLTHROUGH */
	default:
	        if (ISMULTI)    /* handle multibyte supplementary character */ 
                   mbchar(p, p->next-1, new);
                else                           /* handle ordinary character */
		  ordinary(p, c, new);
		break;
	}

	if (!MORE())
		return;
	c = PEEK();
	/* we call { a repetition if followed by a digit */
	if (!( c == '*' || c == '+' || c == '?' ||
				(c == '{' && MORE2()) ))
		return;		/* no repetition, we're done */
	NEXT();

	if(!new) {
		REQUIRE(!wascaret, OREG_BADRPT);
	} else {
		REQUIRE(!wascaret, REG_BADRPT);
	}
	switch (c) {
	case '*':	/* implemented as +? */
		/* this case does not require the (y|) trick, noKLUDGE */
		INSERT(OPLUS_, pos, new);
		ASTERN(O_PLUS, pos, new);
		INSERT(OQUEST_, pos, new);
		ASTERN(O_QUEST, pos, new);
		break;
	case '+':
		INSERT(OPLUS_, pos, new);
		ASTERN(O_PLUS, pos, new);
		break;
	case '?':
		/* KLUDGE: emit y? as (y|) until subtle bug gets fixed */
		INSERT(OCH_, pos, new);		/* offset slightly wrong */
		ASTERN(OOR1, pos, new);		/* this one's right */
		AHEAD(pos);			/* fix the OCH_ */
		EMIT(OOR2, 0L, new);		/* offset very wrong... */
		AHEAD(THERE());			/* ...so fix it */
		ASTERN(O_CH, THERETHERE(), new);
		break;
	case '{':
		count = p_count(p, new);
		if (EAT(',')) {
			if (MORE()) {
				register char c;
				if (c=PEEK()) {
					if (isdigit(c)) {
						count2 = p_count(p, new);
						if(!new) {
						       REQUIRE(count <= count2,
								OREG_BADBR);
						} else {
						       REQUIRE(count <= count2,
								REG_BADBR);
						}
					} else if (c == '}')	/* single number with comma */
						count2 = INFINITY;
					else
						if(!new) {
							SETERROR(OREG_BADBR);
						} else {
							SETERROR(REG_BADBR);
						}
				} else
					if(!new) {
						SETERROR(OREG_BADBR);
					} else {
						SETERROR(REG_BADBR);
					}
			} else
				if(!new) {
					SETERROR(OREG_BADBR);
				} else {
					SETERROR(REG_BADBR);
				}
		} else		/* just a single number */
			count2 = count;
		if (!p->error)	/* invalid bounds - no repeat */
			repeat(p, pos, count, count2, new);

		if (!EAT('}')) {	/* error heuristics */
			while (MORE() && PEEK() != '}')
				NEXT();
			if  (!p->error)	{ /* Don't reset error again */
				if(!new) {
					REQUIRE(MORE(), OREG_EBRACE);
					SETERROR(OREG_BADBR);
				} else {
					REQUIRE(MORE(), REG_EBRACE);
					SETERROR(REG_BADBR);
				}
			}
		}
		break;
	}

	if (!MORE())
		return;
	c = PEEK();
	if (!( c == '*' || c == '+' || c == '?' ||
				(c == '{' && MORE2() && isdigit(PEEK2())) ) )
		return;
	if(!new) {
		SETERROR(OREG_BADRPT);
	} else {
		SETERROR(REG_BADRPT);
	}
}

/*
 - p_str - string (no metacharacters) "parser"
 == static void p_str(register struct parse *p);
 */
static void
p_str(register struct parse *p, int new)
{
  uch c;

  REQUIRE(MORE(), REG_EMPTY);
  while (MORE())
    {
      c = GETNEXT();
      if (ISMULTI)              /* handle multibyte supplemental character */
        mbchar(p, p->next-1, new);
      else                      /* handle ordinary character */	 
        ordinary(p, c, new);
    }
}

/*
 - p_bre - BRE parser top level, anchoring and concatenation
 == static void p_bre(register struct parse *p, register int end1, \
 ==	register int end2);
 * Giving end1 as OUT essentially eliminates the end1/end2 check.
 *
 * This implementation is a bit of a kludge, in that a trailing $ is first
 * taken as an ordinary character and then revised to be an anchor.  The
 * only undesirable side effect is that '$' gets included as a character
 * category in such cases.  This is fairly harmless; not worth fixing.
 * The amount of lookahead needed to avoid this kludge is excessive.
 */
static void
p_bre(register struct parse *p,
	register int end1,	/* first terminating character */
	register int end2,	/* second terminating character */
	int new)
{
	register int first = 1;			/* first subexpression? */
	register int wasdollar = 0;

	if (EAT('^')) {
		/* The first ^ in the first subexpression is an anchor */
		/* Move BOL to before the subexpression                */
		/* This enable the back reference not to repeat anchor */
		/* in the subexpression                                */
		if ((OP(p->strip[THERE()]) == (unsigned long)OLPAREN) &&
			(first == 1) && (p->g->nsub == 1L)) {
			register sop sn;
			register sop subno;

			sn = p->strip[THERE()];
			p->slen--;
			EMIT(OBOL, 0L, new);
			subno = OPND(sn);
			p->pbegin[subno] = HERE();
			EMIT(OP(sn),OPND(sn), new);
			
		} else
			EMIT(OBOL, 0L, new);

		p->g->iflags |= USEBOL;
		p->g->nbol++;
	}
	while (MORE() && !SEETWO(end1, end2)) {
		wasdollar = p_simp_re(p, first, new);
		first = 0;
	}
	if (wasdollar) {	/* oops, that was a trailing anchor */
		DROP(1L);
		EMIT(OEOL, 0L, new);
		p->g->iflags |= USEEOL;
		p->g->neol++;
	}

}

/*
 - p_simp_re - parse a simple RE, an atom possibly followed by a repetition
 == static int p_simp_re(register struct parse *p, int starordinary);
 */
static int			/* was the simple RE an unbackslashed $? */
p_simp_re(register struct parse *p,
	int starordinary, /* is a leading * an ordinary character? */
	int new)
{
	register int c;
	register int count;
	register int count2;
	register sopno pos;
	register long i;
	register sopno subno;
#	define	BACKSL	(1<<CHAR_BIT)

	pos = HERE();		/* repetion op, if any, covers from here */

	assert(MORE());		/* caller should have ensured this */
	c = GETNEXT();
	if (c == '\\') {
		if(!new) {
			REQUIRE(MORE(), OREG_EESCAPE);
		} else {
			REQUIRE(MORE(), REG_EESCAPE);
		}
		c = BACKSL | (unsigned char)GETNEXT();
	}
	switch (c) {
	case '.':
		if(!new) {
			if (p->g->cflags&OREG_NEWLINE)
				nonnewline(p, new);
			else
				EMIT(OANY, 0L, new);
		} else {
			if (p->g->cflags&REG_NEWLINE)
				nonnewline(p, new);
			else
				EMIT(OANY, 0L, new);
		}
		break;
	case '[':
		p_bracket(p, new);
		break;
	case BACKSL|'{':
		if(!new) {
			SETERROR(OREG_BADRPT);
		} else {
			SETERROR(REG_BADRPT);
		}
		break;
	case BACKSL|'(':
		p->g->nsub++;
		subno = (sopno)p->g->nsub;
		if (subno < NPAREN)
			p->pbegin[subno] = HERE();
		EMIT(OLPAREN, subno, new);
		/* the MORE here is an error heuristic */
		if (MORE() && !SEETWO('\\', ')'))
			p_bre(p, '\\', ')', new);
		if (subno < NPAREN) {
			p->pend[subno] = HERE();
			assert(p->pend[subno] != 0);
		}
		EMIT(ORPAREN, subno, new);
		if(!new) {
			REQUIRE(EATTWO('\\', ')'), OREG_EPAREN);
		} else {
			REQUIRE(EATTWO('\\', ')'), REG_EPAREN);
		}
		break;
	case BACKSL|')':	/* should not get here -- must be user */
	case BACKSL|'}':
		if(!new) {
			SETERROR(OREG_EPAREN);
		} else {
			SETERROR(REG_EPAREN);
		}
		break;
	case BACKSL|'1':
	case BACKSL|'2':
	case BACKSL|'3':
	case BACKSL|'4':
	case BACKSL|'5':
	case BACKSL|'6':
	case BACKSL|'7':
	case BACKSL|'8':
	case BACKSL|'9':
		i = (c&~BACKSL) - '0';
		assert(i < NPAREN);
		if (p->pend[i] != 0) {
			assert(i <= p->g->nsub);
			EMIT(OBACK_, i, new);
			assert(p->pbegin[i] != 0);
			assert(OP(p->strip[p->pbegin[i]]) == OLPAREN);
			assert(OP(p->strip[p->pend[i]]) == ORPAREN);
			(void) dupl(p, p->pbegin[i]+1, p->pend[i], new);
			EMIT(O_BACK, i, new);
		} else
			if(!new) {
				SETERROR(OREG_ESUBREG);
			} else {
				SETERROR(REG_ESUBREG);
			}
		p->g->backrefs = 1;
		break;
	case '*':
		if(!new) {
			REQUIRE(starordinary, OREG_BADRPT);
		} else {
			REQUIRE(starordinary, REG_BADRPT);
		}
		/* FALLTHROUGH */
	default:
	  if (ISMULTI)      /* handle multibyte supplementary character */
                  mbchar(p, p->next-1, new);
                else                       /* handle ordinary character */
		  ordinary(p, c &~ BACKSL, new);
		break;
	}

	if (EAT('*')) {		/* implemented as +? */
		/* this case does not require the (y|) trick, noKLUDGE */
		INSERT(OPLUS_, pos, new);
		ASTERN(O_PLUS, pos, new);
		INSERT(OQUEST_, pos, new);
		ASTERN(O_QUEST, pos, new);
	} else if (EATTWO('\\', '{')) {
		count = p_count(p, new);
		if (EAT(',')) {
			if (MORE() && isdigit(PEEK())) {
				count2 = p_count(p, new);
				if(!new) {
					REQUIRE(count <= count2, OREG_BADBR);
				} else {
					REQUIRE(count <= count2, REG_BADBR);
				}
			} else		/* single number with comma */
				count2 = INFINITY;
		} else		/* just a single number */
			count2 = count;
		repeat(p, pos, count, count2, new);
		if (!EATTWO('\\', '}')) {	/* error heuristics */
			while (MORE() && !SEETWO('\\', '}'))
				NEXT();
			if(!new) {
				REQUIRE(MORE(), OREG_EBRACE);
				SETERROR(OREG_BADBR);
			} else {
				REQUIRE(MORE(), REG_EBRACE);
				SETERROR(REG_BADBR);
			}
		}
	} else if (c == (unsigned char)'$')	/* $ (but not \$) ends it */
		return(1);

	return(0);
}

/*
 - p_count - parse a repetition count
 == static int p_count(register struct parse *p);
 */
static int			/* the value */
p_count(register struct parse *p, int new)
{
	register int count = 0;
	register int ndigits = 0;

	if(!new) {
		if (!MORE())
			SETERROR(OREG_EBRACE);
	} else {
		if (!MORE())
			SETERROR(REG_EBRACE);
	}
	while (MORE() && isdigit(PEEK()) && count <= DUPMAX) {
		count = count*10 + (GETNEXT() - '0');
		ndigits++;
	}

	if(!new) {
		REQUIRE(ndigits > 0 && count <= DUPMAX, OREG_BADBR);
	} else {
		REQUIRE(ndigits > 0 && count <= DUPMAX, REG_BADBR);
	}
	return(count);
}

/*
 - p_bracket - parse a bracketed character list
 == static void p_bracket(register struct parse *p, int new);
 *
 * Note a significant property of this code:  if the allocset() did SETERROR,
 * no set operations are done.
 */
static void
p_bracket(register struct parse *p, int new)
{
	register cset *cs = allocset(p, new);
	register int mask;

	/* Dept of Truly Sickening Special-Case Kludges */
	if (p->next + 5 < p->end && strncmp(p->next, "[:<:]]", 6) == 0) {
		EMIT(OBOW, 0L, new);
		NEXTn(6);
		return;
	}
	if (p->next + 5 < p->end && strncmp(p->next, "[:>:]]", 6) == 0) {
		EMIT(OEOW, 0L, new);
		NEXTn(6);
		return;
	}

	if (EAT('^'))
	  cs->inverted = 1;         /* make note to invert set at end */
	if (((MORE2() && PEEK2() != '-' && PEEK() != ']') || !MORE2()) && EAT(']'))
		CHadd(cs, ']');
	else if (cs->inverted && MORE2() && PEEK2() != ']' && EAT('-'))
		CHadd(cs, '-');
	while (MORE() && !SEETWO('-', ']')) {
		p_b_term(p, cs, new);
		if (PEEK() == ']')
			break;
	}
	if (EAT('-'))
		CHadd(cs, '-');
	if(!new) {
		MUSTEAT(']', OREG_EBRACK);
	} else {
		MUSTEAT(']', REG_EBRACK);
	}

	if (p->error != 0)	/* don't mess things up further */
		return;

	/* Handle the Ignore Case option by adding "other" case
	 * of each character in the set to the set
         */

	if(!new)
		mask = OREG_ICASE;
	else
		mask = REG_ICASE;
	if (p->g->cflags&mask) {
		register int i;
		register int ci;

		for (i = p->g->csetsize - 1; i >= 0; i--)
			if (CHIN(cs, i) && isalpha(i)) {
				ci = (char) othercase((wint_t) i);
				if (ci != i)
					CHadd(cs, ci);
			}
		if (cs->nmultis != 0)
			mccase(p, cs);

		if ((cs->nwcs != 0) && (cs->nc != 0)) {
		        wccase(p, cs, new);
		}
	}

	if ( cs->inverted )
	{
	    if ( new )
		mask = REG_NEWLINE;
	    else
		mask = OREG_NEWLINE;

	    if ( p->g->cflags & mask )
		CHadd ( cs, '\n' );
	}

	/* Inversion is handled by setting a flag in the cset that 
	 * is interpreted by the matcher as "any character NOT in this set"
         */

	/* TODO: remove ISMULTI test when setopt can handle mb sets correctly */
	if ( (! ISMULTI) && nch(p, cs) == 1 && cs->inverted != 1) {  /* optimize singleton sets */
	        setopt(p, cs, new);
	} else
		EMIT(OANYOF, freezeset(p, cs), new);
}

/*
 - p_b_term - parse one term of a bracketed character list
 == static void p_b_term(register struct parse *p, register cset *cs, int new);
 */
static void
p_b_term(register struct parse *p, register cset *cs, int new)
{
	register char c;
	register wchar_t start, finish;

	/* classify what we've got */
	switch ((MORE()) ? PEEK() : '\0') {
	case ']':
	case '[':
		c = (MORE2()) ? PEEK2() : '\0';
		break;
	default:
		c = '\0';
		break;
	}

	switch (c) {
	case ':':		/* character class */
		NEXT2();
		if(!new) {
			REQUIRE(MORE(), OREG_EBRACK);
		} else {
			REQUIRE(MORE(), REG_EBRACK);
		}
		c = PEEK();
		if(!new) {
			REQUIRE(c != '-' && c != ']', OREG_ECTYPE);
		} else {
			REQUIRE(c != '-' && c != ']', REG_ECTYPE);
		}
		p_b_cclass(p, cs, new);
		if(!new) {
			REQUIRE(MORE(), OREG_EBRACK);
			REQUIRE(EATTWO(':', ']'), OREG_ECTYPE);
		} else {
			REQUIRE(MORE(), REG_EBRACK);
			REQUIRE(EATTWO(':', ']'), REG_ECTYPE);
		}
		break;
	case '=':		/* equivalence class */
		NEXT2();
		if(!new) {
			REQUIRE(MORE(), OREG_EBRACK);
		} else {
			REQUIRE(MORE(), REG_EBRACK);
		}
		c = PEEK();
		if(!new) {
			REQUIRE(c != '-', OREG_ECOLLATE);
		} else {
			REQUIRE(c != '-', REG_ECOLLATE);
		}
		p_b_eclass(p, cs, new);
		if(!new) {
			REQUIRE(MORE(), OREG_EBRACK);
			REQUIRE(EATTWO('=', ']'), OREG_ECOLLATE);
		} else {
			REQUIRE(MORE(), REG_EBRACK);
			REQUIRE(EATTWO('=', ']'), REG_ECOLLATE);
		}
		break;
	default:		/* symbol, ordinary character, or range */

		start = p_b_symbol(p, cs, new);
		if (SEE('-') && MORE2() && PEEK2() != ']') {
			/* range */
			NEXT();
			if (EAT('-'))
				finish = (wchar_t) '-';
			else
				finish = p_b_symbol(p, cs, new);
		} else
			finish = start;

		if (start == finish)             	/* if just one symbol */
		  {
		  if (start > CHAR_MAX)            /* add to wcs if an MBC */
		    WCadd(p, cs, start, new);
		  else
		    CHadd(cs, (char) start);
		  }
		else
		  {
		  /* add range to normal cset if regular one-byte values */
		  /* xxx what about signed chars here... */
		    if(!new) {
		      REQUIRE(start < finish, OREG_ERANGE);
		    } else {
		      REQUIRE(start < finish, REG_ERANGE);
		    }

		    __lu_collorder(cs, start, finish);

		    /*		for (i = start; i <= finish; i++) */
		    /*			CHadd(cs, i); */

		  }
		break;
	}
}

/*
 - p_b_cclass - parse a character-class name and deal with it
 == static void p_b_cclass(register struct parse *p,register cset *cs,int new);
 */
static void
p_b_cclass(register struct parse *p, register cset *cs, int new)
{
    /* TODO: possible optimization: sb characters are currently
       also added in the multi byte section */

    register char *sp = p->next;
    register struct cclass *cp;
    register size_t len;
    register int i;
    int (*isclass)(int) = NULL;
    int mb_class_size = 0;

    while (MORE() && isalpha(PEEK()))
	NEXT();
    len = p->next - sp;

    for (cp = (struct cclass *)&cclasses[0]; cp->name[0] != '\0' ; cp++)
	if (strncmp(cp->name, sp, len) == 0 && cp->name[len] == '\0') {
	    isclass = cp->function;
	    break;
	}

    if ( isclass )
	for (i = 0; i < p->g->csetsize; i++)
	    if ((*isclass)(i))
		CHadd(cs, (char) i);

    /* check for supplementary tables */
    if (ISMULTI)
    {
	char name[CCLASSLN1];
	strncpy ( name, sp, len );
	name[len] = '\0';

	/* query locale for supplemental character set */
	if ((mb_class_size = __lu_cclass(cs, name )) != -1)
	    cs->nwcs += mb_class_size;
	else
	{
	    cs->nwcs = 0;
	    free(cs->wcs);
	    cs->wcs = NULL;
	}	  
    }
    if ( isclass == NULL && mb_class_size == 0 )
	/* oops, didn't find it */
	if(!new) {
	    SETERROR(OREG_ECTYPE);
	} else {
	    SETERROR(REG_ECTYPE);
	}
}

/*
 - p_b_eclass - parse an equivalence-class name and deal with it
 == static void p_b_eclass(register struct parse *p,register cset *cs,int new);
 *
 * This implementation is incomplete because:
 *   1) it only acts on the first 256 characters of ctype information no 
 *      matter the locale.  This is because the current structure of the 
 *      LC_COLLATE file is a straight 256 slot table.  The 
 *      underlying runtime supports nothing else.
 * Note that this function has been completely rewritten;  in particular,
 *      p_b_coll_elem no longer exists, and it's functionality has become
 *      part of this function.
 */

static void
p_b_eclass(register struct parse *p, register cset *cs, int new)
{

        register int  len;
	register char *sp = p->next;

	while (MORE() && !SEETWO('=', ']'))
		NEXT();
	if (!MORE()) {
	  if (!new)
	    SETERROR(OREG_EBRACK);
	  else
	    SETERROR(REG_EBRACK);
	  return;
	}

	len = (int) (p->next - sp);

	if (__lu_eclass(cs, sp, len) == 0)
	  {
	  /* symbol not found in collation tables */
	  if (!new)
	    SETERROR(OREG_ECOLLATE);
	  else
	    SETERROR(REG_ECOLLATE);
	  }

	return;
}

/*
 - p_b_symbol - parse a character or [..]ed multicharacter collating symbol
 * This function performs three tests on the input stream:
 *  1) Determine if parsing a collation symbol in [.symbol.] expression
 *     If so, send symbol to p_b_coll_symbol for processing
 *  2) Determine if next character starts a multibyte character.  
 *     If so, convert to wchar_t and return
 *  3) Just an ASCII-range character, return it as wchar_t
 *
 == static wchar_t p_b_symbol(register struct parse *p, cset *cs, int new);
 */
static wchar_t			/* value of symbol */
p_b_symbol(register struct parse *p, cset *cs, int new)
{
	register wchar_t value;
	register int retval;
	wchar_t wc;

	if(!new) {
		REQUIRE(MORE(), OREG_EBRACK);
	} else {
		REQUIRE(MORE(), REG_EBRACK);
	}

	/* determine if parsing a collating symbol [.<name>.], 
	 * a regular (ASCII) character, or a multibyte character
	 */

	if (EATTWO('[', '.')) {
	  /* collating symbol */
	  value = p_b_coll_symbol(p, '.', cs, new);
	  if (!new) {
	    REQUIRE(EATTWO('.', ']'), OREG_ECOLLATE);
	  } else {
	    REQUIRE(EATTWO('.', ']'), REG_ECOLLATE);
	  }
	  return(value);
	}
	else if (ISMULTI) {
	  /* convert multibyte to wchar_t */
	  if ((retval = mbtowc(&wc, p->next, MB_LEN_MAX)) > 0 )
	    {
	    if ( retval > 1 && ((towupper(wc) != wc) || (towlower(wc) != wc)) )
	      {
		  cs->case_eq = realloc(cs->case_eq, (cs->nc+1) * sizeof(wchar_t));
		  cs->case_eq[cs->nc++] = othercase ( wc );
	      }
	    NEXTn(retval);
	    return(wc);
	    }
	  else /* mbtowc returned 0 or worse, -1 */
	    {
#ifdef REDEBUG
	    assert(0);
#endif
	    if (!new) 
	      {
	      SETERROR(OREG_BADRPT);
	      } 
	    else 
	      {
	      SETERROR(REG_BADRPT);
	      }
	    return OUT;
	    }
	}
	else
	/* regular symbol */
	  return((wchar_t) GETNEXT());
}

/*
 - p_b_coll_symbol - parse a collating-element name and look it up
 == static wchar_t p_b_coll_symbol(register struct parse *p, int endc);
 */
static wchar_t			/* value of collating element */
p_b_coll_symbol(register struct parse *p,
                int endc, /* name ended by endc,']' */
		cset *cs,
		int new)
{
	register char *sp = p->next;
	register struct cname *cp;
	register int len;
	char value;

	while (MORE() && !SEETWO(endc, ']'))
		NEXT();
	if (!MORE()) {
		if(!new) {
			SETERROR(OREG_EBRACK);
		} else {
			SETERROR(REG_EBRACK);
		}
		return(0);
	}

	len = (int)(p->next - sp);

	/* This will be replaced with a scan of the order_name sequence
	   from the POSIX version of the locale definition */
	   
	for (cp = (struct cname *) &cnames[0]; cp->name[0] != NULL; cp++)
	        if (strncmp(cp->name, sp, len) == 0 && cp->name[len] == '\0')
			return((wchar_t) cp->code);	/* known name */

       /* if nothing found try collating symbols saved in collating table */

	if (__lu_collsymbol(&value, sp, len) == 0) {
	  mcadd(p, cs, sp, (int) len, new);
	  return((wchar_t) value);  /* found substitution string */
	}

	/* try to convert to one single char */
	if ( ISMULTI )
	{
	    wchar_t wc;
	    if ( mbtowc ( &wc, sp, MB_LEN_MAX ) == len )
		return wc;
	}
	else if ( len == 1 )
	{
	    return (wchar_t) *sp ;
	}
	
	/* if nothing still found */
	if(!new) {
		SETERROR(OREG_ECOLLATE);
	} else {
		SETERROR(REG_ECOLLATE);
	}

	return(0);
}

/*
 - othercase - return the case counterpart of an alphabetic
 == static char othercase(int ch);
 */
static wchar_t			/* if no counterpart, return ch */
othercase(wint_t ch)
{
	assert(iswalpha(ch));
	if (iswupper(ch))
		return(towlower(ch));
	else if (iswlower(ch))
		return(towupper(ch));
	else			/* peculiar, but could happen */
		return(ch);
}

/*
 - bothcases - emit a dualcase version of a two-case character
 == static void bothcases(register struct parse *p, int ch);
 *
 * Boy, is this implementation ever a kludge...
 */
static void
bothcases(register struct parse *p, wint_t ch, int new)
{
    /* TODO: Have bothcases handle wchar correctly */
	register char *oldnext = p->next;
	register char *oldend = p->end;
	char bracket [ MB_LEN_MAX + 2 ];
	int mblen;

	assert(othercase(ch) != ch);	/* p_bracket() would recurse */
	p->next = bracket;

	mblen = wctomb ( bracket, ch );
	if ( mblen < 1 ) mblen = 0;

	bracket [ mblen ]	= ']';
	bracket [ mblen + 1 ]	= '\0';

	p->end = bracket + mblen + 1;

	p_bracket(p, new);
	assert(p->next == bracket+mblen+1);
	p->next = oldnext;
	p->end = oldend;
}

/*
 - ordinary - emit an ordinary character
 == static void ordinary(register struct parse *p, register int ch);
 */
static void
ordinary(register struct parse *p, wint_t ch, int new)
{
	register cat_t *cap = p->g->categories;
	register int mask;

	if(!new)
		mask = OREG_ICASE;
	else
		mask = REG_ICASE;

	if ((p->g->cflags&mask) && (ch <= CHAR_MAX) && isalpha(ch) && 
	    (othercase(ch) != ch))
	         bothcases(p, ch, new);
	else {
	        long index = (long)p->g->nmbs;

      		assert(p->g->mbs != NULL);

		if (index >= p->g->mbslen) {
			/* increase the size by 50% */
			p->g->mbslen = (p->g->mbslen+1) / 2 * 3;
			p->g->mbs = realloc(p->g->mbs, p->g->mbslen
					    * sizeof(wchar_t));
		}

		if (p->g->mbs == NULL) {
			if (!new)
				SETERROR(OREG_ESPACE);
			else
				SETERROR(REG_ESPACE);
			return;
		}

	        p->g->mbs[index] = ch;
		p->g->nmbs++;
		EMIT(OCHAR, index, new);
		if (cap[ch] == 0)
			cap[ch] = p->g->ncategories++;
	}
}

/*
 - nonnewline - emit REG_NEWLINE version of OANY
 == static void nonnewline(register struct parse *p);
 *
 * Boy, is this implementation ever a kludge...
 */
static void
nonnewline(register struct parse *p, int new)
{
	register char *oldnext = p->next;
	register char *oldend = p->end;
	char bracket[4];

	p->next = bracket;
	p->end = bracket+3;
	bracket[0] = '^';
	bracket[1] = '\n';
	bracket[2] = ']';
	bracket[3] = '\0';
	p_bracket(p, new);
	assert(p->next == bracket+3);
	p->next = oldnext;
	p->end = oldend;
}

/*
 - repeat - generate code for a bounded repetition, recursively if needed
 == static void repeat(register struct parse *p, sopno start, int from, int to,
			int new);
 */
static void
repeat(register struct parse *p,
	sopno start,	/* operand from here to end of strip */
	int from,	/* repeated from this number */
	int to,		/* to this number of times (maybe INFINITY) */
	int new)
{
	register sopno finish = HERE();
#	define	N	2
#	define	INF	3
#	define	REP(f, t)	((f)*8 + (t))
#	define	MAP(n)	(((n) <= 1) ? (n) : ((n) == INFINITY) ? INF : N)
	register sopno copy;

	if (p->error != 0)	/* head off possible runaway recursion */
		return;

	assert(from <= to);

	switch (REP(MAP(from), MAP(to))) {
	case REP(0, 0):			/* must be user doing this */
		DROP(finish-start);	/* drop the operand */
		break;
	case REP(0, 1):			/* as x{1,1}? */
	case REP(0, N):			/* as x{1,n}? */
	case REP(0, INF):		/* as x{1,}? */
		/* KLUDGE: emit y? as (y|) until subtle bug gets fixed */
		INSERT(OCH_, start, new);	/* offset is wrong... */
		repeat(p, start+1, 1, to, new);
		ASTERN(OOR1, start, new);
		AHEAD(start);			/* ... fix it */
		EMIT(OOR2, 0L, new);
		AHEAD(THERE());
		ASTERN(O_CH, THERETHERE(), new);
		break;
	case REP(1, 1):			/* trivial case */
		/* done */
		break;
	case REP(1, N):			/* as x?x{1,n-1} */
		/* KLUDGE: emit y? as (y|) until subtle bug gets fixed */
		INSERT(OCH_, start, new);
		ASTERN(OOR1, start, new);
		AHEAD(start);
		EMIT(OOR2, 0L, new);		/* offset very wrong... */
		AHEAD(THERE());			/* ...so fix it */
		ASTERN(O_CH, THERETHERE(), new);
		copy = dupl(p, start+1, finish+1, new);
		assert(copy == finish+4);
		repeat(p, copy, 1, to-1, new);
		break;
	case REP(1, INF):		/* as x+ */
		INSERT(OPLUS_, start, new);
		ASTERN(O_PLUS, start, new);
		break;
	case REP(N, N):			/* as xx{m-1,n-1} */
		copy = dupl(p, start, finish, new);
		repeat(p, copy, from-1, to-1, new);
		break;
	case REP(N, INF):		/* as xx{n-1,INF} */
		copy = dupl(p, start, finish, new);
		repeat(p, copy, from-1, to, new);
		break;
	default:			/* "can't happen" */
		SETERROR(REG_ASSERT);	/* just in case */
		break;
	}
}

/*
 - seterr - set an error condition
 == static int seterr(register struct parse *p, int e);
 */
static int			/* useless but makes type checking happy */
seterr(register struct parse *p, int e)
{
	if (p->error == 0)	/* keep earliest error condition */
		p->error = e;
	p->next = (char *)nuls;	/* try to bring things to a halt */
	p->end = (char *)nuls;
	return(0);		/* make the return value well-defined */
}

/*
 - allocset - allocate a set of characters for []
 == static cset *allocset(register struct parse *p);
 */
static cset *
allocset(register struct parse *p, int new)
{
	register int no = p->g->ncsets++;
	register size_t nc;
	register size_t nbytes;
	register cset *cs;
	register size_t css = (size_t)p->g->csetsize;
	register int i;

	if (no >= p->ncsalloc) {	/* need another column of space */
		p->ncsalloc += CHAR_BIT;
		nc = p->ncsalloc;
		assert(nc % CHAR_BIT == 0);
		nbytes = nc / CHAR_BIT * css;
		if (p->g->sets == NULL)
			p->g->sets = (cset *)malloc(nc * sizeof(cset));
		else
			p->g->sets = (cset *)realloc((char *)p->g->sets,
							nc * sizeof(cset));
		if (p->g->setbits == NULL)
			p->g->setbits = (uch *)malloc(nbytes);
		else {
			p->g->setbits = (uch *)realloc((char *)p->g->setbits,
								nbytes);
			/* xxx this isn't right if setbits is now NULL */
			for (i = 0; i < no; i++)
				p->g->sets[i].ptr = p->g->setbits + css*(i/CHAR_BIT);
		}
		if (p->g->sets != NULL && p->g->setbits != NULL)
			(void) memset((char *)p->g->setbits + (nbytes - css),
								0, css);
		else {
			no = 0;
			if(!new) {
				SETERROR(OREG_ESPACE);
			} else {
				SETERROR(REG_ESPACE);
			}
			/* caller's responsibility not to do set ops */
		}
	}

	assert(p->g->sets != NULL);	/* xxx */
	cs = &p->g->sets[no];
	cs->ptr = p->g->setbits + css*((no)/CHAR_BIT);
	cs->mask = 1 << ((no) % CHAR_BIT);
	cs->hash = 0;
	cs->smultis = 0;
	cs->multis = NULL;

	cs->nmultis = 0;
	cs->nwcs = 0;
	cs->wcs = NULL;
	cs->range = 0;
	cs->sorted = 0;
	cs->inverted = 0;
	cs->case_eq = NULL;
	cs->nc = 0;

	return(cs);
}

/*
 - freeset - free a now-unused set
 == static void freeset(register struct parse *p, register cset *cs);
 */
static void
freeset(register struct parse *p, register cset *cs)
{
	register int i;
	register cset *top = &p->g->sets[p->g->ncsets];
	register size_t css = (size_t)p->g->csetsize;

	for (i = 0; i < css; i++)
		CHsub(cs, i);

	if (cs->nmultis != 0)
	  cs->nmultis = 0;

	if (cs->nwcs != 0)
	{
	  cs->nwcs = 0;
	  free(cs->wcs);

	  /* check for case equivalents buffer */
	  if (cs->nc != 0)
	    {
	    free(cs->case_eq);
	    cs->nc = 0;
	    }
	}

	if (cs == top-1)	/* recover only the easy case */
		p->g->ncsets--;
}

/*
 - freezeset - final processing on a set of characters
 == static int freezeset(register struct parse *p, register cset *cs);
 *
 * The main task here is merging identical sets.  This is usually a waste
 * of time (although the hash code minimizes the overhead), but can win
 * big if REG_ICASE is being used.  REG_ICASE, by the way, is why the hash
 * is done using addition rather than xor -- all ASCII [aA] sets xor to
 * the same value!
 */
static int			/* set number */
freezeset(register struct parse *p, register cset *cs)
{
	register uch h = cs->hash;
	register int wcs = cs->nwcs;
	register int mcs = cs->nmultis;
	register int i;
	register cset *top = &p->g->sets[p->g->ncsets];
	register cset *cs2;
	register size_t css = (size_t)p->g->csetsize;

	/* look for an earlier one which is the same */
	for (cs2 = (cset *)&p->g->sets[0]; cs2 < top; cs2++) {
	        if (cs == cs2) continue;
		
		if (   ( cs2->hash == h )
		    && ( wcs == cs2->nwcs )
		    && (  mcs == cs2->nmultis )
		    && ( cs->inverted == cs2->inverted ) )
		{
		        /* maybe */
		        if (memcmp(cs->multis, cs2->multis, cs->smultis) != 0)
			  continue;
			if (memcmp(cs->wcs, cs2->wcs, (cs->nwcs * sizeof(wchar_t))) != 0)
			  continue;
			for (i = 0; i < css; i++)
			        if (!!CHIN(cs2, i) != !!CHIN(cs, i))
			                break;	/* no */
		        if (i == css)
			        break;		/* yes */
		}
	}

	if (cs2 < top) {	/* found one */
		freeset(p, cs);
		cs = cs2;
	}


      /* if the wcs buffer in the set is large, sort the members and use binary search */

      if (wcs >= LU_SORT_SET) {
	      __lu_sort(cs->wcs, wcs);
	      cs->sorted = 1;
      }
              
      return((int)(cs - p->g->sets));
}

/*
 - firstch - return first character in a set (which must have at least one)
 -           note that this operates only on ISO8859-1 character array.
 == static int firstch(register struct parse *p, register cset *cs);
 */
static int			/* character; there is no "none" value */
firstch(register struct parse *p, register cset *cs)
{
	register int i;
	register size_t css = (size_t)p->g->csetsize;

	for (i = 0; i < css; i++)
		if (CHIN(cs, i))
			return((char)i);

	assert(never);
	return(0);
}

/*
 - setopt - optimize a singleton set
 == static void setopt(register struct parse *p, register cset *cs);
 */
static void
setopt(register struct parse *p, register cset *cs, int new)
{
       register wchar_t wc;

       if (cs->nwcs == 1)
       {
	   /* TODO: handle multibyte singleton correclty */
	   /* This is currently never called in mb */
	   wc = cs->wcs[0];    /* the singleton is a multibyte literal */

	   /* force character into an OCHAR operation */

       }
       else
       {
	  wc = (wchar_t) firstch(p, cs);  /* singleton is ISO8859-1 */
	  /* force character into an OCHAR operation */
	  ordinary(p, wc, new);
       }

       /* free this cset */
       freeset(p, cs);

       return;
}

/*
 - nch - number of characters in a set
 == static int nch(register struct parse *p, register cset *cs);
 */
static int
nch(register struct parse *p, register cset *cs)
{
	register int i;
	register size_t css = (size_t)p->g->csetsize;
	register int n = 0;

	for (i = 0; i < css; i++)
		if (CHIN(cs, i))
			n++;

	/* counts the number of _bytes_ saved as multi-character elements */
	if (cs->nmultis != 0)
	  n += (int) ( cs->smultis - cs->nmultis - 1 );

	/* count multibyte characters as well */
	n += cs->nwcs;

	return(n);
}

/*
 - mcadd - add a collating element to a cset
 == static void mcadd(struct parse *p, cset *cs, char *cp, int len, int new);
 */
static void
mcadd(register struct parse *p, register cset *cs, register char *cp, int len, int new)
{
  register size_t oldend = cs->smultis;

  cs->smultis += len + 1;
  if (cs->multis == NULL)
    cs->multis = malloc(cs->smultis+1);
  else
    cs->multis = realloc(cs->multis, cs->smultis+1);
  if (cs->multis == NULL) {
    if(!new) {
      SETERROR(OREG_ESPACE);
    } else {
      SETERROR(REG_ESPACE); 
    }
    return;
  }

  memcpy(cs->multis + oldend, cp, len);
  cs->multis[cs->smultis - 1] = '\0';      /* null-terminate added string */
  cs->multis[cs->smultis]     = '\0';      /* null-terminate multis buffer */
  cs->nmultis++;
}

#ifdef notdef
/*
 - mcsub - subtract a collating element from a cset
 == static void mcsub(register cset *cs, register char *cp);
 */
static void
mcsub(register cset *cs, register char *cp)
{
	register char *fp = mcfind(cs, cp);
	register size_t len = strlen(fp);

	assert(fp != NULL);
	(void) memmove(fp, fp + len + 1,
				cs->smultis - (fp + len + 1 - cs->multis));
	cs->smultis -= len + 1;
	cs->multis[cs->smultis] = '\0';
	cs->nmultis--;

	if (cs->smultis == 1) {
		free(cs->multis);
		cs->multis = NULL;
		return;
	}

	cs->multis = realloc(cs->multis, cs->smultis+1);

	/* this assertion takes the place of error checking the realloc() */

	assert(cs->multis != NULL);
}

/*
 - mcin - is a collating element in a cset?
 == static int mcin(register cset *cs, register char *cp);
 */
static int
mcin(register cset *cs, register char *cp)
{
	return(mcfind(cs, cp) != NULL);
}

/*
 - mcfind - find a collating element in a cset
 == static char *mcfind(register cset *cs, register char *cp);
 */
static char *
mcfind(register cset *cs, register char *cp)
{
	register char *p;

	if (cs->nmultis == 0)
		return(NULL);
	for (p = cs->multis; *p != '\0'; p += strlen(p) + 1)
		if (strcmp(cp, p) == 0)
			return(p);
	return(NULL);
}
#endif

/*
 - mcinvert - invert the list of collating elements in a cset
 == static void mcinvert(register struct parse *p, register cset *cs);
 *
 * This would have to know the set of possibilities.  Implementation
 * is deferred because what has to happen for MCEs is difficult.
 * First, anything that has been mcadded must be mcsubed, and all
 * other strings in the substitution set must be mcadded.  Or better yet, 
 * just invert the test in engine.c [this is indeed what we do].
 */
/*
static void
mcinvert(register struct parse *p, register cset *cs)
{
	(void) p; (void) cs;
	assert(cs->multis == NULL);
}
*/

/*
 - mccase - add case counterparts of the list of collating elements in a cset
 == static void mccase(register struct parse *p, register cset *cs);
 */
static void
mccase(register struct parse *p, register cset *cs)
{
        register char *pch;
        char value;
        char originalvalue;
        int i, changed = 0;
        char substring[20];
        int startnum = cs->nmultis;
	size_t len;

	/* caller should ensure that multis is valid */
	assert(cs->multis != NULL);

	/* for each collating element in cs, check case and add other case if there is */

	for (i = 0, pch = cs->multis; i < startnum; i++, pch += strlen(pch) + 1)
	  if( __lu_doessubexist( &value, pch, strlen(pch) ) == 0 ){
	   
	    originalvalue = value;
	    value = tolower(value);
	   
	    if( originalvalue == value ) {
	      /* not successful at transition to lower so try upper */
	      value = toupper(value);	
              if( originalvalue != value ) {
		/* successful transition to upper */
		changed = 1;
	      }
	    }
	    else {
	      /* successful transition to lower  */
	      changed = 1;
	    }
	   
	    if( ( changed == 1 ) && 
		( __lu_getstrfromsubcode(value, substring, &len) == 0 ) ){
	      mcadd(p, cs, substring, (int) len, 0);
	    }
	  }
   
 	return;
}

/*
 - wccase - add case counterparts of the wchar buffer 
 == static void wccase(register struct parse *p, register cset *cs);
 */
static void
wccase(register struct parse *p, register cset *cs, int new)
{
  size_t bufsize = sizeof(wchar_t) * ( cs->nwcs + cs->nc );
  int i;

  assert(cs->case_eq != NULL);

  if ((cs->wcs = realloc(cs->wcs, bufsize)) == NULL) {
      if (!new) {
	  SETERROR(OREG_ESPACE);
      } else {
	  SETERROR(REG_ESPACE); 
      }
      return;
  }
  
  for (i = 0; i < cs->nc; i++)
      /* add to the cs->wcs buffer */
      cs->wcs[ cs->nwcs ++ ] = cs->case_eq[i];
}

/*
 - isinsets - is this character in any sets?
 == static int isinsets(register struct re_guts *g, int c);
 */
static int			/* predicate */
isinsets(register struct re_guts *g, int c)
{
	register uch *col;
	register int i;
	register int ncols = (g->ncsets+(CHAR_BIT-1)) / CHAR_BIT;
	register unsigned uc = (unsigned char)c;

	for (i = 0, col = g->setbits; i < ncols; i++, col += g->csetsize)
		if (col[uc] != 0)
			return(1);
	return(0);
}

/*
 - samesets - are these two characters in exactly the same sets?
 == static int samesets(register struct re_guts *g, int c1, int c2);
 */
static int			/* predicate */
samesets(register struct re_guts *g, int c1, int c2)
{
	register uch *col;
	register int i;
	register int ncols = (g->ncsets+(CHAR_BIT-1)) / CHAR_BIT;
	register unsigned uc1 = (unsigned char)c1;
	register unsigned uc2 = (unsigned char)c2;

	for (i = 0, col = g->setbits; i < ncols; i++, col += g->csetsize)
		if (col[uc1] != col[uc2])
			return(0);
	return(1);
}

/*
 - categorize - sort out character categories
 == static void categorize(struct parse *p, register struct re_guts *g);
 */
static void
categorize(struct parse *p, register struct re_guts *g)
{
	register cat_t *cats = g->categories;
	register int c;
	register int c2;
	register cat_t cat;

	/* avoid making error situations worse */
	if (p->error != 0)
		return;

	for (c = CHAR_MIN; c <= CHAR_MAX; c++)
		if (cats[c] == 0 && isinsets(g, c)) {
			cat = g->ncategories++;
			cats[c] = cat;
			for (c2 = c+1; c2 <= CHAR_MAX; c2++)
				if (cats[c2] == 0 && samesets(g, c, c2))
					cats[c2] = cat;
		}
}

/*
 - dupl - emit a duplicate of a bunch of sops
 == static sopno dupl(register struct parse *p, sopno start, sopno finish);
 */
static sopno			/* start of duplicate */
dupl(register struct parse *p,
	sopno start,	/* from here */
	sopno finish,	/* to this less one */
	int new)
{
	register sopno ret = HERE();
	register sopno len = finish - start;

	assert(finish >= start);
	if (len == 0)
		return(ret);
	enlarge(p, p->ssize + len, new); /* this many unexpected additions */
	assert(p->ssize >= p->slen + len);
	(void) memcpy((char *)(p->strip + p->slen),
		(char *)(p->strip + start), (size_t)len*sizeof(sop));
	p->slen += len;
	return(ret);
}

/*
 - doemit - emit a strip operator
 == static void doemit(register struct parse *p, sop op, size_t opnd);
 *
 * It might seem better to implement this as a macro with a function as
 * hard-case backup, but it's just too big and messy unless there are
 * some changes to the data structures.  Maybe later.
 */
static void
doemit(register struct parse *p, sop op, size_t opnd, int new)
{
	/* avoid making error situations worse */
	if (p->error != 0)
		return;

	/* deal with oversize operands ("can't happen", more or less) */
	assert(opnd < 1<<OPSHIFT);

	/* deal with undersized strip */
	if (p->slen >= p->ssize)
		enlarge(p, (p->ssize+1) / 2 * 3, new);	/* +50% */
	assert(p->slen < p->ssize);

	/* finally, it's all reduced to the easy case */
	p->strip[p->slen++] = SOP(op, opnd);
}

/*
 - doinsert - insert a sop into the strip
 == static void doinsert(register struct parse *p, sop op, size_t opnd, sopno pos);
 */
static void
doinsert(register struct parse *p, sop op, size_t opnd, sopno pos, int new)
{
	register sopno sn;
	register sop s;
	register int i;

	/* avoid making error situations worse */
	if (p->error != 0)
		return;

	sn = HERE();
	EMIT(op, opnd, new);		/* do checks, ensure space */
	assert(HERE() == sn+1);
	s = p->strip[sn];

	/* adjust paren pointers */
	assert(pos > 0);
	for (i = 1; i < NPAREN; i++) {
		if (p->pbegin[i] >= pos) {
			p->pbegin[i]++;
		}
		if (p->pend[i] >= pos) {
			p->pend[i]++;
		}
	}

	memmove((char *)&p->strip[pos+1], (char *)&p->strip[pos],
						(HERE()-pos-1)*sizeof(sop));
	p->strip[pos] = s;
}

/*
 - dofwd - complete a forward reference
 == static void dofwd(register struct parse *p, sopno pos, sop value);
 */
static void
dofwd(register struct parse *p, register sopno pos, sop value)
{
	/* avoid making error situations worse */
	if (p->error != 0)
		return;

	assert(value < 1<<OPSHIFT);
	p->strip[pos] = OP(p->strip[pos]) | value;
}

/*
 - enlarge - enlarge the strip
 == static void enlarge(register struct parse *p, sopno size);
 */
static void
enlarge(register struct parse *p, register sopno size, int new)
{
	register sop *sp;

	if (p->ssize >= size)
		return;

	sp = (sop *)realloc(p->strip, size*sizeof(sop));
	if (sp == NULL) {
		if(!new) {
			SETERROR(OREG_ESPACE);
		} else {
			SETERROR(REG_ESPACE);
		}
		return;
	}
	p->strip = sp;
	p->ssize = size;
}

/*
 - stripsnug - compact the strip
 == static void stripsnug(register struct parse *p, register struct re_guts *g);
 */
static void
stripsnug(register struct parse *p, register struct re_guts *g, int new)
{
	g->nstates = p->slen;
	g->strip = (sop *)realloc((char *)p->strip, p->slen * sizeof(sop));
	if (g->strip == NULL) {
		if(!new) {
			SETERROR(OREG_ESPACE);
		} else {
			SETERROR(REG_ESPACE);
		}
		g->strip = p->strip;
	}
}

/*
 - findmust - fill in must and mlen with longest mandatory literal string
 == static void findmust(register struct parse *p, register struct re_guts *g);
 *
 * This algorithm could do fancy things like analyzing the operands of |
 * for common subsequences.  Someday.  This code is simple and finds most
 * of the interesting cases.
 *
 * Note that must and mlen got initialized during setup.
 */

static void
findmust(struct parse *p, register struct re_guts *g)
{
	register sop *scan;
	sop *start, opcode;
	register sop *newstart;
	register sopno newlen;
	register sop s;
	register wchar_t *cp;
	register sopno i, j;

	/* A multibyte buffer */ 
	char mb[MB_LEN_MAX];
        int  len, mustspace = 0, maxspace;

	/* avoid making error situations worse */
	if (p->error != 0)
		return;

	/* find the longest OCHAR sequence in strip */
	newlen = 0;
	scan = g->strip + 1;
	do {
		s = *scan++;
		switch (OP(s)) {
		case OCHAR:		/* sequence member */
		case OMBCHAR:
			if (newlen == 0)		/* new sequence */
				newstart = scan - 1;
			newlen++;
                        if (OP(s) == OMBCHAR)
			   mustspace += MB_LEN_MAX;
			else
			   mustspace ++;
			break;
		case OPLUS_:		/* things that don't break one */
		case OLPAREN:
		case ORPAREN:
			break;
		case OQUEST_:		/* things that must be skipped */
		case OCH_:
			scan--;
			do {
				scan += OPND(s);
				s = *scan;
				/* assert() interferes w debug printouts */
				if (OP(s) != O_QUEST && OP(s) != O_CH &&
							OP(s) != OOR2) {
					g->iflags |= BAD;
					return;
				}
			} while (OP(s) != O_QUEST && OP(s) != O_CH);
			/* fallthrough */
		default:		/* things that break a sequence */
			if (newlen > g->mlen) {		/* ends one */
				start = newstart;
				g->mlen = (int)newlen;
				maxspace = mustspace;
			}
			newlen = 0;
			mustspace = 0;
			break;
		}
	} while (OP(s) != OEND);

	if (g->mlen == 0)		/* there isn't one */
		return;

	/* turn it into a character string */
	/* Note that if multibyte characters involved, mlen is too big */
	g->must = malloc((size_t) ((maxspace + 1) * sizeof(wchar_t)));
	if (g->must == NULL) {		/* argh; just forget it */
		g->mlen = 0;
		return;
	}
	cp = g->must;
	scan = start;

	for (i = g->mlen; i > 0; i--) {
		while ((opcode = OP(s = *scan++)) != OCHAR && opcode != OMBCHAR)
			continue;
		assert(cp < g->must + g->mlen);
		if (opcode == OCHAR)
		  *cp++ = g->mbs[OPND(s)];
		else /* Note that this section only is necessary if we special case */
		     /* the supplementary character codes and use OMBCHAR as an operand */
		  {
		  len = wctomb(mb, g->mbs[OPND(s)]);
		  g->mlen += (len - 1); /* kludgy but necessary */
		  for (j = 0; j < len; j++)
		    *cp++ = mb[j];
		  }
	}
	
	assert(cp == g->must + g->mlen);
	*cp++ = '\0';		/* just on general principles */
}

/*
 - pluscount - count + nesting
 == static sopno pluscount(register struct parse *p, register struct re_guts *g);
 */
static sopno			/* nesting depth */
pluscount(struct parse *p, register struct re_guts *g)
{
	register sop *scan;
	register sop s;
	register sopno plusnest = 0;
	register sopno maxnest = 0;

	if (p->error != 0)
		return(0);	/* there may not be an OEND */

	scan = g->strip + 1;
	do {
		s = *scan++;
		switch (OP(s)) {
		case OPLUS_:
			plusnest++;
			break;
		case O_PLUS:
			if (plusnest > maxnest)
				maxnest = plusnest;
			plusnest--;
			break;
		}
	} while (OP(s) != OEND);
	if (plusnest != 0)
		g->iflags |= BAD;
	return(maxnest);
}

/*
 - mbchar - handle multibyte (supplementary characters)
 == static void mbchar(register struct parse *p, unsigned char *firstbyte, int new)
 * Converts multibyte characters to wide character
 * Operand is index into new data structure saved new field in re_guts, mbs
 * We treat all MB characters as OCHARs, so there was a branch 
 *    [else if (len ==1 ) ordinary(...)]of an if statement changed.
 *    If we use OMBCHARs to special case, change it back
 */
static void 
mbchar(register struct parse *p, char *firstbyte, int new)
{
  register int mblen, mask;
  register long number_mbs;
  wchar_t wc;

  /* convert to a wide character code */
  if ( (mblen = mbtowc ( &wc, firstbyte, MB_LEN_MAX)) == -1 )
  {
      /* invalid character sequence */
      if (!new)
	  SETERROR(OREG_BADPAT);
      else
	  SETERROR(REG_BADPAT);
      return;
  }
  else 
    {
    /* check this on the chance that firstbyte didn't come from a GETNEXT */
    if (p->next == firstbyte+1)
       NEXTn(mblen-1);

    if(!new)
      mask = OREG_ICASE;
    else
      mask = REG_ICASE;

    if ((p->g->cflags&mask) && iswalpha(wc) && (othercase(wc) != wc))
	bothcases(p, wc, new);
    else 
      {
      /* Handle by storing the wide character code in an 'array' of such in re_guts. */
      number_mbs = p->g->nmbs;

      assert(p->g->mbs != NULL);

      if (number_mbs >= p->g->mbslen) {
	/* increase the size by 50% */
	p->g->mbslen = (long)((p->g->mbslen+1) / 2 * 3);
	p->g->mbs = realloc(p->g->mbs, p->g->mbslen*sizeof(wchar_t));
      }

      if (p->g->mbs == NULL) 
	{
	  if (!new)
	    SETERROR(OREG_ESPACE);
	  else
	    SETERROR(REG_ESPACE);
	  return;
	}

      p->g->mbs[number_mbs] = wc;

      EMIT(OCHAR, number_mbs, new); /* OMBCHAR if we don't need to special case it */
      p->g->nmbs++;
      }
    }

   return;  
}

/*
 - WCadd - add a (multibyte or not) character to cset as a wide character
 == static int WCadd(register struct parse *p, register cset *cs, wchar_t wc, int)
 */
static int
WCadd (struct parse *p, register cset *cs, wchar_t ch, int new)
{

  cs->nwcs ++;
  if (cs->wcs == NULL)
    cs->wcs = malloc(cs->nwcs * sizeof(wchar_t));
  else
    cs->wcs = realloc(cs->wcs, cs->nwcs*sizeof(wchar_t));
  if (cs->wcs == NULL) {
    if (!new) 
      SETERROR(OREG_ESPACE);
    else
      SETERROR(REG_ESPACE);
    return(1);
  }

  cs->wcs[cs->nwcs-1] = ch;

  return(1);
}
