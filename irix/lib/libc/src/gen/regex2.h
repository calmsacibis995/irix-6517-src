/* regex2.h
 * $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/gen/RCS/regex2.h,v 1.9 1998/06/12 22:58:35 holzen Exp $
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
 *	@(#)regex2.h	8.4 (Berkeley) 3/20/94
 */

#ident "$Revision: 1.9 $"

/*
 * First, the stuff that ends up in the outside-world include file
 = typedef off_t regoff_t;
 = typedef struct {
 = 	int re_magic;
 = 	size_t re_nsub;		// number of parenthesized subexpressions
 = 	const char *re_endp;	// end pointer for REG_PEND
 = 	struct re_guts *re_g;	// none of your business :-)
 = } regex_t;
 = typedef struct {
 = 	regoff_t rm_so;		// start of match
 = 	regoff_t rm_eo;		// end of match
 = } regmatch_t;
 */
/*
 * internals of regex_t
 */
#define	MAGIC1	((('r'^0200)<<8) | 'e')

typedef	ssize_t		sgi_regoff_t;

typedef struct
{
	sgi_regoff_t	rm_so;
	sgi_regoff_t	rm_eo;
} sgi_regmatch_t;

/*
 * The internal representation is a *strip*, a sequence of
 * operators ending with an endmarker.  (Some terminology etc. is a
 * historical relic of earlier versions which used multiple strips.)
 * Certain oddities in the representation are there to permit running
 * the machinery backwards; in particular, any deviation from sequential
 * flow must be marked at both its source and its destination.  Some
 * fine points:
 *
 * - OPLUS_ and O_PLUS are *inside* the loop they create.
 * - OQUEST_ and O_QUEST are *outside* the bypass they create.
 * - OCH_ and O_CH are *outside* the multi-way branch they create, while
 *   OOR1 and OOR2 are respectively the end and the beginning of one of
 *   the branches.  Note that there is an implicit OOR2 following OCH_
 *   and an implicit OOR1 preceding O_CH.
 *
 * In state representations, an operator's bit is on to signify a state
 * immediately *preceding* "execution" of that operator.
 */
typedef unsigned long sop;	/* strip operator */
typedef long sopno;
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define	OPRMASK	0xf800000000000000
#define	OPDMASK	0x07ffffffffffffff
#define	OPSHIFT	((unsigned)59)
#else
#define	OPRMASK	0xf8000000
#define	OPDMASK	0x07ffffff
#define	OPSHIFT	((unsigned)27)
#endif
#define	OP(n)	((unsigned long)(n)&OPRMASK)
#define	OPND(n)	((unsigned long)(n)&OPDMASK)
#define	SOP(op, opnd)	((unsigned long)(op)|(long)(opnd))
/* operators			   	   meaning	operand		*/
/*						(back, fwd are offsets)	*/
#define	OEND	((sop)1<<OPSHIFT)	/* endmarker	-		*/
#define	OCHAR	((sop)2<<OPSHIFT)	/* character	unsigned char	*/
#define	OBOL	((sop)3<<OPSHIFT)	/* left anchor	-		*/
#define	OEOL	((sop)4<<OPSHIFT)	/* right anchor	-		*/
#define	OANY	((sop)5<<OPSHIFT)	/* .		-		*/
#define	OANYOF	((sop)6<<OPSHIFT)	/* [...]	set number	*/
#define	OBACK_	((sop)7<<OPSHIFT)	/* begin \d	paren number	*/
#define	O_BACK	((sop)8<<OPSHIFT)	/* end \d	paren number	*/
#define	OPLUS_	((sop)9<<OPSHIFT)	/* + prefix	fwd to suffix	*/
#define	O_PLUS	((sop)10<<OPSHIFT)	/* + suffix	back to prefix	*/
#define	OQUEST_	((sop)11<<OPSHIFT)	/* ? prefix	fwd to suffix	*/
#define	O_QUEST	((sop)12<<OPSHIFT)	/* ? suffix	back to prefix	*/
#define	OLPAREN	((sop)13<<OPSHIFT)	/* (		fwd to )	*/
#define	ORPAREN	((sop)14<<OPSHIFT)	/* )		back to (	*/
#define	OCH_	((sop)15<<OPSHIFT)	/* begin choice	fwd to OOR2	*/
#define	OOR1	((sop)16<<OPSHIFT)	/* | pt. 1	back to OOR1 or OCH_ */
#define	OOR2	((sop)17<<OPSHIFT)	/* | pt. 2	fwd to OOR2 or O_CH */
#define	O_CH	((sop)18<<OPSHIFT)	/* end choice	back to OOR1	*/
#define	OBOW	((sop)19<<OPSHIFT)	/* begin word	-		*/
#define	OEOW	((sop)20<<OPSHIFT)	/* end word	-		*/
/* Note that the operand OMBCHAR exists only
 * in case it is necessary to special case 
 * handling the C locale for performance reasons 
 */
#define OMBCHAR ((sop)21<<OPSHIFT)      /* mb char      index to array  */

/*
 * Structure for [] character-set representation.  Character sets are
 * done as bit vectors, grouped 8 to a byte vector for compactness.
 * The individual set therefore has both a pointer to the byte vector
 * and a mask to pick out the relevant bit of each byte.  A hash code
 * simplifies testing whether two sets could be identical.
 *
 * This will get trickier for multicharacter collating elements.  As
 * preliminary hooks for dealing with such things, we also carry along
 * a string of multi-character elements, and decide the size of the
 * vectors at run time.
 */
typedef struct {
	uch *ptr;		/* -> uch [csetsize] */
	uch mask;		/* bit within array */
	uch hash;		/* hash code */
        int nmultis;            /* for speed in nch() */
	size_t smultis;
        char *multis;	        /* -> char[smulti]  ab\0cd\0ef\0\0 */
        wchar_t  *wcs;          /* -> wchar_t[nwcs] for storing wchar_t */
        wchar_t *case_eq;       /* wchar_t case equivalents for supplemental characters */
        int nc;                 /* number of case equivs in cs->case */
        int nwcs;
        int inverted;
        int range;              /* set if fast search option enabled */
        wchar_t range_min, range_max; /* limits to search for range */
        uch sorted;           /* set to 1 if *wcs is a sorted list */
} cset;
/* note that CHadd and CHsub are unsafe, and CHIN doesn't yield 0/1 */
#define	CHadd(cs, c)	((cs)->ptr[(uch)(c)] |= (cs)->mask, (cs)->hash += (c))
#define	CHsub(cs, c)	((cs)->ptr[(uch)(c)] &= ~(cs)->mask, (cs)->hash -= (c))
#define	CHIN(cs, c)	((cs)->ptr[(uch)(c)] & (cs)->mask)
#define	MCadd(p, cs, cp, new)	mcadd(p, cs, cp, new)	/* regcomp() internal fns */
#define	MCsub(p, cs, cp)	mcsub(p, cs, cp)
#define	MCin(p, cs, cp)	mcin(p, cs, cp)

/* stuff for character categories */
typedef unsigned char cat_t;

#include <iconv.h>

/*
 * main compiled-expression structure
 */
struct re_guts {
	int magic;
#		define	MAGIC2	((('R'^0200)<<8)|'E')
	sop *strip;		/* malloced area for strip */
	int csetsize;		/* number of bits in a cset vector */
	int ncsets;		/* number of csets in use */
	cset *sets;		/* -> cset [ncsets] */
	uch *setbits;		/* -> uch[csetsize][ncsets/CHAR_BIT] */
        wchar_t *mbs;           /* -> wchar_t [nmbs] */
        int nmbs;               /* number of multibytes in use */
	long mbslen;		/* number of multibytes allocated */
	int cflags;		/* copy of regcomp() cflags argument */
	sopno nstates;		/* = number of sops */
	sopno firststate;	/* the initial OEND (normally 0) */
	sopno laststate;	/* the final OEND */
	int iflags;		/* internal flags */
#		define	USEBOL	01	/* used ^ */
#		define	USEEOL	02	/* used $ */
#		define	BAD	04	/* something wrong */
	int nbol;		/* number of ^ used */
	int neol;		/* number of $ used */
	int ncategories;	/* how many character categories */
	cat_t *categories;	/* ->catspace[-CHAR_MIN] */
  	wchar_t *must;		/* match must contain this string */
	int mlen;		/* length of must */
	size_t nsub;		/* copy of re_nsub */
	int backrefs;		/* does it use back references? */
	sopno nplus;		/* how deep does it nest +s? */
	/* catspace must be last */
	cat_t catspace[1];	/* actually [NC] */
};

#define	OUT	((wchar_t)-1)	/* a guaranteed non wchar_t value */
