/*
 * Internationalized regular expressions
 *      on wchar_t basis
 *      by frank@ceres.esd.sgi.com Dec 3 1992
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#ident  "$Revision: 1.10 $"

/*
#include "synonyms.h"
*/
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <wctype.h>
#include <unistd.h>
#include <wsregexp.h>
#include <msgs/uxsgicore.h>

extern int	wsncmp(wchar_t *, wchar_t *, int);

#define NEQ(s1, s2, n)	(!wsncmp(s1, s2, n))

/*
 * compile additional internationalized expressions
 */
struct reclass {
	wchar_t	*class;			/* ptr to class string */
	int	size;			/* size of class string */
	int	(*func)();		/* ptr to function */
};

static int f_iswalpha(wchar_t wc) { return(iswalpha(wc)); }
static int f_iswupper(wchar_t wc) { return(iswupper(wc)); }
static int f_iswlower(wchar_t wc) { return(iswlower(wc)); }
static int f_iswdigit(wchar_t wc) { return(iswdigit(wc)); }
static int f_iswxdigit(wchar_t wc) { return(iswxdigit(wc)); }
static int f_iswalnum(wchar_t wc) { return(iswalnum(wc)); }
static int f_iswspace(wchar_t wc) { return(iswspace(wc)); }
static int f_iswpunct(wchar_t wc) { return(iswpunct(wc)); }
static int f_iswprint(wchar_t wc) { return(iswprint(wc)); }
static int f_iswgraph(wchar_t wc) { return(iswgraph(wc)); }
static int f_iswcntrl(wchar_t wc) { return(iswcntrl(wc)); }

static wchar_t	S_alpha[]  = { 'a', 'l', 'p', 'h', 'a' };
static wchar_t	S_upper[]  = { 'u', 'p', 'p', 'e', 'r' };
static wchar_t	S_lower[]  = { 'l', 'o', 'w', 'e', 'r' };
static wchar_t	S_digit[]  = { 'd', 'i', 'g', 'i', 't' };
static wchar_t	S_xdigit[] = { 'x', 'd', 'i', 'g', 'i', 't' };
static wchar_t	S_alnum[]  = { 'a', 'l', 'n', 'u', 'm' };
static wchar_t	S_space[]  = { 's', 'p', 'a', 'c', 'e' };
static wchar_t	S_punct[]  = { 'p', 'u', 'n', 'c', 't' };
static wchar_t	S_print[]  = { 'p', 'r', 'i', 'n', 't' };
static wchar_t	S_graph[]  = { 'g', 'r', 'a', 'p', 'h' };
static wchar_t	S_cntrl[]  = { 'c', 'n', 't', 'r', 'l' };
static wchar_t	S_clend[]  = { ':', ']' };

static struct reclass reclass[] = {
	{ S_alpha,	5,	f_iswalpha	},
	{ S_upper,	5,	f_iswupper	},
	{ S_lower,	5,	f_iswlower	},
	{ S_digit,	5,	f_iswdigit	},
	{ S_xdigit,	6,	f_iswxdigit	},
	{ S_alnum,	5,	f_iswalnum	},
	{ S_space,	5,	f_iswspace	},
	{ S_punct,	5,	f_iswpunct	},
	{ S_print,	5,	f_iswprint	},
	{ S_graph,	5,	f_iswgraph	},
	{ S_cntrl,	5,	f_iswcntrl	},
	{ 0,		5,	0,		},
};

static int
recompicl(wchar_t **sp, long **ep)
{
	register wchar_t *s = *sp;
	register long *e = *ep;
	register struct reclass *rcl;

	switch(*s++) {
	    /*
	     * [:class:]
	     * A category class expression, any character of class 'class'.
	     * classes are:
	     *		alpha,upper,lower,digit,xdigit,alnum
	     *		space,punct,print,graph,cntrl
	     */
	    case ':' :
		for(rcl = reclass; rcl->class; rcl++) {
		    if( !wsncmp(s, rcl->class, rcl->size)) {
			s += rcl->size;
			if(wsncmp(s, S_clend, 2))
			    return(ERR_SYNTAX);		/* syntax error */
			s += 2;
			*e++ = ICLASS;
			*e++ = (long)rcl->func;		/* ICLASS,function */
			goto end_ire;
		    }
		}
		return(ERR_ILLCLASS);		/* illegal class */
	    /*
	     * [=c=]
	     * An equivalence class. Any collation element defined as
	     * having the same order in the current collation sequence as c.
	     * As an example, if A and a belong to the same equivalence
	     * class, then both [[=A=]b] and [[=a=]b] are equivalent
	     * to [Aab].
	     */
	    case '=' :
		*e++ = EQCLASS;
		*e++ = *s++;
		if((*s != '=') || (s[1] != ']'))
		    return(ERR_EQUIL);		/* illegal equiv. syntax */
		s += 2;
		break;
	    /*
	     * [.cc.]
	     * A collating synbol. Multi-character collating elements must
	     * be represented as collating symbols to distinguish them from
	     * single-character collating elements. As an example, if the
	     * string ch is a valid collating element, then [.ch.] will be
	     * treated as an element matching the same string of characters,
	     * while ch will be treated as a simple list of c and h. If the
	     * string is not a valid collating element in the current
	     * collating sequence definition, the symbol will be treated as
	     * an invalid expression.
	     */
	    case '.' :
		while(*s++ != ']');		/* ignored for now */
		break;
	    /*
	     * illegal regexp
	     */
	    default :
		return(ERR_SYNTAX);		/* syntax error */
	}
end_ire:
	*ep = e;
	*sp = s;
	return(0);
}

/*
 * compile [s] and [^s] expression
 */
static int
recompcl(wchar_t **sp, long **ep, long *lep)
{
	register long *el;
	register wchar_t wc;
	register int x;
	wchar_t *s = *sp;
	long *e = *ep;

	*e = CCL;				/* cmd = CCL | CNEG */
	if((wc = *s++) == '^') {
	    *e |= CNEG;
	    wc = *s++;
	}
	e += 2;					/* cmd,size,XXXX */
	el = e;
	do {
	    if(e >= lep)
		return(ERR_REOVFLOW);		/* regexp overflow */
	    if( !wc || (wc == '\n'))
		return(ERR_SIMBAL);		/* [ ] imbalance */
	    if(wc == '[') {
		if(x = recompicl(&s, &e))
		    return(x);			/* syntax err in intnl regexp */
		continue;
	    }
	    if(wc == '-' && (e > el)) {
		if(*s == ']') {
		    s++;
		    *e++ = wc;			/* is real - */
		    break;
		}
		wc = CRNGE;			/* c-c */
	    }
	    *e++ = wc;
	} while((wc = *s++) != ']');
	el[-1] = e - el;			/* size of [...] */
	*ep = e;
	*sp = s;
	return(0);
}

/*
 * compile a regular expression for calls to wsrematch() or wsrestep()
 *
 * The first argument points to structure, allocated by the caller, to
 * hold global data for further calls to the regexp routines. Before
 * calling wsrecompile() the caller must initialize the following data:
 *
 *	sed	- (1) if sed(1) is calling (different syntax for delimiter)
 *	str	- ptr to the regexp to be wsrecompile()'d
 *
 * The parameter ep is a long ptr, to which the regexp is compiled.
 * This field is used to hold commands, wchar_t chars and function ptrs.
 * If a ptr or the wchar_t is greater than a long wsrecompile(), returns
 * -1, and the regexp software cannot be used.
 *
 * The parameter lep points to the last place in ep. If the compile
 * expression cannot fit in ep, the error code ERR_REOVFLOW is returned.
 *
 * The parameter eof is the character which marks the end of the regular
 * expression.  For example, in ed(1), this character is usually a /.
 *
 * If wsrecompile() was successful, it returns the ptr to the end of
 * the regexp in ep. Otherwise, 0 is returned and 'err' in 'rexd' is
 * set to indicate the error, see <wsregexp.h>.
 */
long *
wsrecompile(struct rexdata *rexd, long *ep, long *lep, wchar_t eof)
	/* struct rexdata *rexd;	regexp data structure */
	/* long *ep;			ptr to buf for compiled regexp */
	/* long *lep;			ptr to last place in ep */
	/* wchar_t eof;			regexp delimiter */
{
	wchar_t *sp;
	register wchar_t wc;
	register long *le;
	register int i;
	int nre;			/* n'th regexp */
	int cflg;			/* flag for \{ \} */
	char *brp;			/* current ptr in bracket[] */
	char bracket[RE_NBRA];

#ifndef sgi
	/* CONSTANTCONDITION */
	if(sizeof(wchar_t) > sizeof(long)) {
	    rexd->err = ERR_SIZE;
	    return(0);				/* wchar_t too big */
	}
#endif
	rexd->err = 0;
	sp = rexd->str;				/* ptr to regexp string */
	le = 0;
	wc = *sp;
	if((wc == eof) || (wc == '\n')) {
	    if(wc == '\n')
		rexd->nodelim = 1;
	    else
		sp++;
	    if( ep == lep || ( !*ep && !rexd->sed)) {
		rexd->err = ERR_NORMBR;		/* No rememb. search string */
		return(0);
	    }
	    goto ret_ep;
	}
	brp = bracket;				/* reset bracket ptr */
	rexd->circf = rexd->nbra = nre = 0;
	if(wc == '^') {
	    rexd->circf++;
	    sp++;
	}
	for(;;) {
	    if(ep >= lep)
		goto rex_overflow;			/* regexp overflow */
	    wc = *sp++;
	    if((wc != '*') && ((wc != '\\') || (*sp != '{')) )
		le = ep;
	    if(wc == eof) {
		*ep++ = CCEOF;
		if(brp != bracket) {
		    rexd->err = ERR_BRA;		/* \( \) imbalance */
		    return(0);
		}
		break;
	    }
	    switch(wc) {
		/*
		 * . matches any character
		 */
		case '.':
		    *ep++ = CDOT;
		    continue;
		case '\n':
		    if(!rexd->sed) {
			sp--;
			*ep++ = CCEOF;
			rexd->nodelim = 1;
			if(brp == bracket)
			    goto ret_ep;
			rexd->err = ERR_BRA;		/* \( \) imbalance */
		    } else
			rexd->err = ERR_DELIM;		/* ill/miss delimiter */
		    return(0);
		/*
		 * r*
		 * Zero or more successive occurrences of the regexp r.
		 * The longest leftmost match is choosen.
		 */
		case '*':
		    if( !le || (*le == CBRA) || (*le == CKET))
			goto defchar;
		    *le |= STAR;
		    continue;
		/*
		 * $
		 * The end of the string being compared.
		 */
		case '$':
		    if((*sp != eof) && (*sp != '\n'))
			goto defchar;
		    *ep++ = CDOL;
		    continue;
		/*
		 * [s]
		 * Any character in the non-empty set s, where s is
		 * a sequence of characters. Ranges may be specified
		 * as c-c. The character ] may be included in the set
		 * by placing it first in the set. The character - may
		 * be included in the set by placing it first or last
		 * in the set. The character ^ may be included in the
		 * set by placing it anywhere other than the first in
		 * the set.
		 *
		 * [^s]
		 * Any character not in the set s, where s is defined
		 * as above.
		 */
		case '[':
		    i = recompcl(&sp, &ep, lep);	/* compile ranges */
		    if( !i)
			continue;
		    rexd->err = i;
		    return(0);				/* error */
		/*
		 * \c
		 * The characters with special meanings.
		 */
		case '\\':
		    switch(wc = *sp++) {
			/*
			 * \(r\)
			 * the regular expression r
			 * The \( and \) sequences are ignored.
			 */
			case '(':
			    if(rexd->nbra >= RE_NBRA) {
				rexd->err = ERR_2MLBRA;
				return(0);		/* too many \( */
			    }
			    *ep++ = CBRA;
			    *ep++ = rexd->nbra;
			    *brp++ = rexd->nbra;
			    rexd->nbra++;
			    continue;
			case ')':
			    if(brp <= bracket)  {
				rexd->err = ERR_BRA;
				return(0);		/* \( \) imbalance */
			    }
			    *ep++ = CKET;
			    *ep++ = *--brp;
			    nre++;
			    continue;
			/*
			 * r\{m,n\}
			 * Any number of m through n successives
			 * occurrences of the regexp r.
			 *
			 * r\{m\}
			 * matches exactly m occurrences.
			 *
			 * r\{m,\}
			 * matches at least m occurrences.
			 */
			case '{':
			    if( !le)
				goto defchar;
			    *le |= RNGE;
			    cflg = 0;
			nlim:
			    wc = *sp++;
			    i = 0;
			    do {
				if(isdigit(wc))
				    i = (10 * i) + (wc - '0');
				else {
				    rexd->err = ERR_NBR;
				    return(0);
				}
				wc = *sp++;
			    } while((wc != '\\') && (wc != ','));
			    if(i >= RE_MAX_RANGE) {
				rexd->err = ERR_RANGE;
				return(0);
			    }
			    *ep++ = i;
			    if(wc == ',') {
				/*
				 * \{m,n\} was specified
				 */
				if(cflg++) {
				    rexd->err = ERR_2MNBR;
				    return(0);
				}
				if(*sp == '\\') {
				    *ep++ = RE_MAX_RANGE;
				    sp++;
				} else
				    goto nlim;		/* convert n */
			    }
			    if(*sp++ != '}') {
				rexd->err = ERR_MISSB;
				return(0);
			    }
			    if( !cflg)
				*ep++ = i;		/* m = n */
			    else
				if(ep[-1] < ep[-2]) {
				    rexd->err = ERR_BADRNG;
				    return(0);
				}
			    continue;
			/*
			 * \newline is not valid
			 */
			case '\n':
			    rexd->err = ERR_DELIM;
			    return(0);
			/*
			 * \n for newline
			 */
			case 'n':
			    wc = '\n';			/* \n for newline */
			    break;
			/*
			 * \n where n is a number in the range 1 to 9
			 *
			 * When \n appears in a regexp, it stands for the
			 * regexp x, where x is the nth regexp enclosed
			 * in \( and \) sequences that appeared earlier in
			 * the concatenated regexp.
			 * \(r\)\(y\)z\2 matches regexp y
			 */
			default:
			    if(wc >= '1' && wc <= '9') {
				if((wc -= '1') >= nre) {
				    rexd->err = ERR_DIGIT;
				    return(0);
				}
				*ep++ = CBACK;
				*ep++ = (long)wc;
				continue;
			    }
		    }
		/*
		 * a normal character
		 */
		default:
defchar:
		    le = ep;
		    *ep++ = CCHR;		/* regular char */
		    *ep++ = (long)wc;
	    }
	}
ret_ep:
	rexd->str = sp;			/* normal return */
	return(ep);
rex_overflow:
	rexd->err = ERR_REOVFLOW;	/* regexp overflow */
	return(0);
}

/*
 * check for equvivalence class
 */
static int
f_eqchk(wchar_t wc, wchar_t ewc)
{
	return((wc == ewc)? 1 : 0);
}

/*
 * try to match [s]
 */
static long *
remccl(register wchar_t wc, register long *ep, int neg, register long size)
{
	register long *el;
	int (*func)(wchar_t);

	if( !wc)
	    return(0);				/* end of string */
	el = ep + size;
	for(; size; size--) {
	    switch(*ep++) {
		case CRNGE :			/* x-y */
		    if(wc < ep[-2])
			continue;		/* smaller than x */
		    if(wc <= *ep++)
			break;			/* matched */
		    continue;
		case ICLASS :			/* [:class:] */
		    func = (int (*)(wchar_t))*ep++;
		    if(func(wc))		/* call isw....() */
			break;			/* matched */
		    continue;
		case EQCLASS :
		    if(f_eqchk(wc, (wchar_t)*ep++))
			break;			/* matched */
		    continue;
		default :
		    if(wc != ep[-1])		/* c in [s] */
			continue;
	    }
	    break;
	}
	if((size > 0) ^ neg)
	    return(el);				/* end of [s] */
	return(0);				/* no match */
}

/*
 * match a string
 */
int
wsrematch(register struct rexdata *rexd, register wchar_t *lp, register long *ep)
{
	register wchar_t wc;
	register int low, size;		/* \{m,n\} */
	long ssize;			/* size of [s] */
	long *le;			/* ptr to last expression */
	wchar_t *curlp;
	wchar_t *brp;			/* ptr to \digit expression */
	int neg;			/* [^...] flag */
	int sizeb;			/* size of brp entry */

	for(;;) {
	    neg = 0;
	    switch(*ep++) {
		case CCHR:			/* c */
		    rexd->loc2 = lp;
		    if(*ep++ == *lp++)
			continue;			/* is there */
		    return(0);

		case CDOT:			/* . */
		    rexd->loc2 = lp;
		    if(*lp++)
			continue;			/* any char there */
		    return(0);

		case CDOL:			/* $ = \0 */
		    rexd->loc2 = lp;
		    if( !*lp)
			continue;
		    return(0);

		case CCEOF:			/* EOF */
		    rexd->loc2 = lp;
		    return(1);				/* match */

		case CBRA:			/* \( */
		    rexd->brsl[*ep++] = lp;
		    continue;

		case CKET:			/* \) */
		    rexd->brel[*ep++] = lp;
		    continue;

		case CCL | CNEG :		/* [^s] */
		    neg = 1;
		case CCL :			/* [s]  */
		    rexd->loc2 = lp;
		    wc = *lp++;
		    ssize = *ep++;
		    if( !(ep = remccl(wc, ep, neg, ssize)))
			return(0);			/* no match */
		    continue;

		case CCHR | RNGE:		/* c\{m,n\} */
		    wc = (wchar_t)*ep++;
		    low = (unsigned)*ep++;
		    size = (unsigned)*ep++ - low;
		    while(low--) {
			rexd->loc2 = lp;
			if(*lp++ != wc)
			    return(0);			/* no match */
		    }
		    curlp = lp;
		    while(size--) {
			rexd->loc2 = lp;
			if(*lp++ != wc)
			    break;
		    }
		    if(size < 0)
			lp++;
		    goto star;
	
		case CDOT | RNGE:		/* .\{m,n\} */
		    low = (unsigned)*ep++;
		    size = (unsigned)*ep++ - low;
		    while(low--) {
			rexd->loc2 = lp;
			if( !*lp++)
			    return(0);			/* no match */
		    }
		    curlp = lp;
		    while(size--) {
			rexd->loc2 = lp;
			if( !*lp++)
			    break;
		    }
		    if(size < 0)
			lp++;
		    goto star;
	
		case CCL | RNGE | CNEG :	/* [^s]\{m,n\} */
		    neg = 1;
		case CCL | RNGE :		/* [s]\{m,n\} */
		    ssize = *ep++;			/* size of [s] */
		    le = ep + ssize;
		    low = (unsigned)*le++;		/* m */
		    size = (unsigned)*le++ - low;	/* n - m */
		    while(low--) {
			rexd->loc2 = lp;
			wc = *lp++;
			if( !remccl(wc, ep, neg, ssize))
			    return(0);			/* m not reached */
		    }
		    curlp = lp;
		    while(size--) {
			rexd->loc2 = lp;
			wc = *lp++;
			if( !remccl(wc, ep, neg, ssize))
			    break;			/* within range */
		    }
		    if(size < 0)
			lp++;
		    ep = le;
		    goto star;
	
		case CBACK:			/* \digit */
		    brp = rexd->brsl[*ep];
		    sizeb = (int)(rexd->brel[*ep++] - brp);
		    if(NEQ(brp, lp, sizeb)) {
			lp += sizeb;
			continue;			/* already matched */
		    }
		    return(0);
	
		case CBACK | STAR:		/* \digit* */
		    brp = rexd->brsl[*ep];
		    sizeb = (int)(rexd->brel[*ep++] - brp);
		    rexd->loc2 = curlp = lp;
		    while(NEQ(brp, lp, sizeb))
			lp += sizeb;
		    rexd->loc2 = lp;
		    while(lp >= curlp) {
			if(wsrematch(rexd, lp, ep))
			    return(1);
			lp -= sizeb;
		    }
		    return(0);
	
		case CDOT | STAR:		/* .* */
		    rexd->loc2 = curlp = lp;
		    while(*lp++);
		    goto star;

		case CCHR | STAR:		/* c* */
		    rexd->loc2 = curlp = lp;
		    while(*lp++ == *ep);
		    ep++;
		    goto star;

		case CCL | STAR | CNEG :	/* [^s]* */
		    neg = 1;
		case CCL | STAR :		/* [s]* */
		    ssize = *ep++;			/* size of [s] */
		    curlp = lp;
		    do {
			rexd->loc2 = lp;
			wc = *lp++;
		    } while(remccl(wc, ep, neg, ssize));
		    ep += ssize;
		    goto star;

star:
		    do {
			if(--lp == rexd->locs)
			    break;
			if(wsrematch(rexd, lp, ep))
			    return(1);			/* match */
		    } while(lp > curlp);
		    return(0);				/* no match */
	    }
	}
}

/*
 * wsrestep(rexd, p1, p2)
 *
 * The second parameter to wsrestep() is a pointer to a string of characters
 * to be checked for a match. This string should be null terminated.
 * The second parameter 'expbuf' is the compiled regular expression which
 * was obtained by a call of the function wsrecompile().
 *
 * The function wsrestep() returns non-zero if the given string matches the
 * regular expression, and zero if the expressions do not match.
 * If there is a match, two wchar_t pointers in rexdata are set.
 *
 * loc1 is set by wsrestep(), it is a pointer to the first character that
 * matched the regular expression.
 *
 * loc2, which is set by the function wsrematch() , points to the character
 * after the last character that matches the regular expression.
 *
 * Thus if the regular expression matches the entire line, loc1 will
 * point to the first character of string and loc2 will point to the
 * null at the end of string.
 *
 * wsrestep() uses the variable circf of rexd which is set by wsrecompile()
 * if the regular expression begins with ^. If this is set then wsrestep()
 * will try to match the regular expression to the beginning of the string
 * only. If more than one regular expression is to be compiled before
 * the first is executed the value of circf should be saved for each
 * compiled expression and circf should be set to that saved value before
 * each call to wsrestep().
 */
int
wsrestep(register struct rexdata *rexd, register wchar_t *p1, register long *p2)
{
	register wchar_t wc;

	if(rexd->circf) {
	    rexd->loc1 = p1;
	    return(wsrematch(rexd, p1, p2));
	}
	/*
	 * fast check for first character
	 */
	if(*p2 == CCHR) {
	    wc = (wchar_t)p2[1];
	    do {
		if(*p1 != wc)
		    continue;
		if(wsrematch(rexd, p1, p2)) {
		    rexd->loc1 = p1;
		    return(1);
		}
	    } while(*p1++);
	    return(0);
	}
	do {
	    if(wsrematch(rexd, p1, p2)) {
		rexd->loc1 = p1;
		return(1);
	    }
	} while(*p1++);
	return(0);
}

/*
 * pick up an regexp error message from catalog
 */
struct rexerr {
	int	err;		/* error code */
	char	*cmsg;		/* catalog */
	char	*dmsg;		/* default */
};

static struct rexerr rexerr[] = {
	{ ERR_SIZE,	_SGI_MMX_regexp_SIZE,
		"software error, wchar_t exceeds long size"	},
	{ ERR_NORMBR,	_SGI_MMX_regexp_NORMBR,
		"no remembered search string"			},
	{ ERR_REOVFLOW,	_SGI_MMX_regexp_REOVFLOW,
		"regexp overflow"				},
	{ ERR_BRA,	_SGI_MMX_regexp_BRA,
		"\\( \\) imbalance"				},
	{ ERR_DELIM,	_SGI_MMX_regexp_DELIM,
		"illegal or missing delimiter"			},
	{ ERR_NBR,	_SGI_MMX_regexp_NBR,
		"bad number in \\{ \\}"				},
	{ ERR_2MNBR,	_SGI_MMX_regexp_2MNBR,
		"more than 2 numbers given in \\{ \\}"		},
	{ ERR_DIGIT,	_SGI_MMX_regexp_DIGIT,
		"``\\digit'' out of range"			},
	{ ERR_2MLBRA,	_SGI_MMX_regexp_2MLBRA,
		"too many \\("					},
	{ ERR_RANGE,	_SGI_MMX_regexp_RANGE,
		"range number too large"			},
	{ ERR_MISSB,	_SGI_MMX_regexp_MISSB,
		"} expected after \\"				},
	{ ERR_BADRNG,	_SGI_MMX_regexp_BADRNG,
		"first number exceeds second in \\{ \\}"	},
	{ ERR_SIMBAL,	_SGI_MMX_regexp_SIMBAL,
		"[ ] imbalance"					},
	{ ERR_SYNTAX,	_SGI_MMX_regexp_SYNTAX,
		"illegal regular expression"			},
	{ ERR_ILLCLASS,	_SGI_MMX_regexp_ILLCLASS,
		"illegal [:class:]"				},
	{ ERR_EQUIL,	_SGI_MMX_regexp_EQUIL,
		"illegal [=c=]"					},
	{ ERR_COLL,	_SGI_MMX_regexp_COLL,
		"illegal [.cc.]"				},
	{ 0, 0, 0 },
};

char *
wsreerr(register int err)
{
	register struct rexerr *rep;

	if(err == -1)
	    err = 0;
	if(err > ERR_MAX)
	    return(0);
	rep = rexerr + err;
	if(rep->err == err)
	    return(gettxt(rep->cmsg, rep->dmsg));
	for(rep = rexerr; rep->err; rep++) {
	    if(rep->err == err)
    		return(gettxt(rep->cmsg, rep->dmsg));
	}
	return(0);
}
