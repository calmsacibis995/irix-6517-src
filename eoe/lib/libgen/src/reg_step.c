/*
 * reg_step.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.4 $"


#ifdef __STDC__
	#pragma weak loc1 = _loc1
	#pragma weak loc2 = _loc2
	#pragma weak locs = _locs
	#pragma weak braelist = _braelist
	#pragma weak braslist = _braslist
	#pragma weak step = _step
	#pragma weak advance = _radvance
#endif
#include "synonyms.h"

#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "_wchar.h"
#include "_regexp.h"

#define ecmp(s1, s2, n)	(!strncmp(s1, s2, n))
#define Popwchar(p, l) mbtowc(&l, p, MB_LEN_MAX)
#define	uletter(c) (isalpha(c) || c == '_')

char	*loc1 = (char *)0, *loc2 = (char *)0, *locs = (char *)0;
char	*braslist[NBRA] = { (char *)0};
char	*braelist[NBRA] = { (char *)0};
static unsigned char _bittab[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
static void getrnge(char *);
static int cclass(char *, char **, int);
static int	low;
static int	size;
static int _advance(char *, char *);
static char *start;

int
step(register char *p1, register char *p2)
{
	register int c;

	wchar_t cl;
	int n;

	/* check if match is restricted to beginning of string */
	start = p1;
	if(*p2++) { 
		loc1 = p1;
		return(_advance(p1, p2));
	}
	if(*p2 == CCHR) {
	/* fast check for first character */
		c = p2[1];
		do {
			if(*p1 != c) 
				continue;
			if(_advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while(*p1++);
	} else if(multibyte)
		do {
			if(_advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
			n = Popwchar(p1, cl);
			if(n < 0)
				/* skip past illegal multibyte characters */
				p1++;
			else
				p1 += n;
		} while(n);
	else
		/* regular algorithm */
		do {
			if(_advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while(*p1++);
	return(0);
}

int
advance(register char *lp, register char *ep)
{
	/* ignore flag to see if expression is anchored */
	start = lp;
	return(_advance(lp, ++ep));
}

static int 
_advance(register char *lp, register char *ep)
{
	char *rp;
	register char *curlp;
	register wchar_t c, d;
	register int n;
	wchar_t cl;
	int neg;
	char *bbeg;
	size_t ct;

	for(;;) {
		neg = 0;
		switch(*ep++) {
		
		case CCHR:
			if(*ep++ == *lp++)
				continue;
			return(0);

		case MCCHR:
			ep += Popwchar(ep, cl);
			c = cl;
			if((n = Popwchar(lp, cl)) <= 0 || c !=  cl)
				return(0);
			lp += n;
			continue;
	
		case CDOT:
			/* 
			 * do not match illegal multibyte characters 
			 * or null 
			 */
			if((n = Popwchar(lp, cl)) > 0) {
				lp +=n;
				continue;
			}
			return(0);
	
		case CDOL:
			if(*lp == 0)
				continue;
			return(0);
	
		case CCEOF:
			loc2 = lp;
			return(1);
	
		case CCL: 
			c = (unsigned char)*lp++;
			if(ISTHERE(c)) {
				ep += 32;
				continue;
			}
			return(0);
		
		case NMCCL:
			neg = 1;
			/* fall through */
		
		case MCCL:
			rp = lp;
			if(cclass(ep, &rp, neg) != 1)
				return(0);
			ep += *(ep + 32) + 32;
			lp = rp;
			continue;
			
		case CBRA:
			braslist[*ep++] = lp;
			continue;
	
		case CKET:
			braelist[*ep++] = lp;
			continue;
		
		case MCCHR | RNGE:
			ep += Popwchar(ep, cl);
			c = cl;
			getrnge(ep);
			while(low--) {
				if((n = Popwchar(lp, cl)) <= 0 || cl != c)
					return(0);
				lp += n;
			}
			curlp = lp;
			while(size--) {
				if((n = Popwchar(lp, cl)) <= 0 || cl != c)
					break;
				lp += n;
			}
			if(size < 0)
				n = Popwchar(lp, cl);
			if(n == -1)
				return(0);
			lp += (n ? n : 1);
			ep += 2;
			goto mstar;
				
		case CCHR | RNGE:
			c = *ep++;
			getrnge(ep);
			while(low--)
				if(*lp++ != c)
					return(0);
			curlp = lp;
			while(size--) 
				if(*lp++ != c)
					break;
			if(size < 0)
				lp++;
			ep += 2;
			goto star;
	
		case CDOT | RNGE:
			getrnge(ep);
			while(low--) {
				if((n = Popwchar(lp, cl)) <= 0)
					return(0);
				lp += n;
			}
			curlp = lp;
			while(size--) {
				if((n = Popwchar(lp, cl)) <= 0) 
					break;
				lp += n;
			}
			if(size < 0)
				n = Popwchar(lp, cl);
			if(n == -1)
				return(0);
			lp += (n ? n : 1);
			ep += 2;
			goto mstar;
		
		case NMCCL | RNGE:
			neg = 1;
			/* fall through */
		
		case MCCL | RNGE:
			getrnge(ep + *(ep + 32) + 32);
			rp = lp;
			while(low--) {
				if(cclass(ep, &rp, neg) != 1)
					return(0);
			}
			curlp = rp;
			while(size-- &&	(c=(cclass(ep, &rp, neg))) == 1);
			if(c == -1)
				return(0);
			lp = rp;
			if(size < 0) {
				if((n = Popwchar(lp, cl)) == -1)
					return(0);
				lp += (n ? n : 1);
			}
			ep += *(ep + 32) + 34;
			goto mstar;
		
		case CCL | RNGE:
			getrnge(ep + 32);
			while(low--) {
				c = (unsigned char)*lp++;
				if(!ISTHERE(c))
					return(0);
			}
			curlp = lp;
			while(size--) {
				c = (unsigned char)*lp++;
				if(!ISTHERE(c))
					break;
			}
			if(size < 0)
				lp++;
			ep += 34;		/* 32 + 2 */
			goto star;
	
		case CBACK:
			bbeg = braslist[*ep];
			ct = (size_t) (braelist[*ep++] - bbeg);
	
			if(ecmp(bbeg, lp, ct)) {
				lp += ct;
				continue;
			}
			return(0);
	
		case CBACK | STAR:
			bbeg = braslist[*ep];
			ct = (size_t) (braelist[*ep++] - bbeg);
			curlp = lp;
			while(ecmp(bbeg, lp, ct))
				lp += ct;
	
			while(lp >= curlp) {
				if(_advance(lp, ep))	
					return(1);
				lp -= ct;
			}
			return(0);
	
		case CDOT | STAR:
			curlp = lp;
			if(!multibyte)
				while(*lp++);
			else {
				while((n = Popwchar(lp, cl)) > 0)
					lp += n;
				if(n == -1)
					return(0);
				if(n == 0)
					lp++;
			}
			goto mstar;
	
		case CCHR | STAR:
			curlp = lp;
			while(*lp++ == *ep);
			ep++;
			goto star;
	
		case MCCHR | STAR:
			curlp = lp;
			ep += Popwchar(ep, cl);
			c = cl;
			while((n = Popwchar(lp, cl))  > 0 && cl == c)
				lp += n;
			if(n == -1)
				return(0);
			lp += (n ? n : 1);
			goto mstar;
		
		case NMCCL | STAR:
			neg = 1;
			/* fall through */
		
		case MCCL | STAR:
			curlp = rp = lp;
			while((d = cclass(ep, &rp, neg)) == 1);
			if(d == -1)
				return(0);
			lp = rp;
			ep += *(ep + 32) + 32;
			goto mstar;
		
		case CCL | STAR:
			curlp = lp;
			do {
				c = (unsigned char)*lp++;
			} while(ISTHERE(c));
			ep += 32;
			goto star;
	
		case CBRC:
			if(lp == start && locs == (char *)0)
				continue;
			c = (unsigned char)*lp;
			d = (unsigned char)*(lp-1);
			if((isdigit(c) || uletter(c) || c >= 0200 && MB_CUR_MAX > 1) && !isdigit(d) && !uletter(d) && (d < 0200 || MB_CUR_MAX == 1))
				continue;
			return(0);
		
		case CLET:
			d = (unsigned char)*lp;
			if(!isdigit(d) && !uletter(d) && (d < 0200 || MB_CUR_MAX == 1))
				continue;
			return(0);
		
		default: 
			return(0);
		}
	}

mstar:
	if(multibyte) {
		do {
			register char *p1, *p2;
			lp--;
			p1 = lp - eucw2;
			p2 = lp - eucw3;
			/* check if previous character is from
		       	 * supplementary code sets 1, 2, or 3 and
			 * back up appropriate number of bytes
			 */
			if((unsigned char)*lp >= 0200) {
				if(p1 >= curlp && (unsigned char)*p1 == SS2)
					lp = p1;
				else if(p2 >= curlp && (unsigned char)*p2 == SS3)
					lp = p2;
				else
					lp = lp - eucw1 + 1;
			}
			if(lp == locs)
				break;
			if(_advance(lp, ep))
				return(1);
		} while(lp > curlp);
		return(0);
	}
star:
	do {
		if(--lp == locs && *ep == CCEOF)
			break;
		if(_advance(lp, ep))
			return(1);
	} while(lp > curlp);
	return(0);
}

static void
getrnge(register char *str)
{
	low = *str++ & 0377;
	size = (*str == 255)? 20000: (*str &0377) - low;
}


static int
cclass(register char *ep, register char **rp, int neg)
{
	register char *lp;
	register wchar_t c, d, f;
	register int n;
	wchar_t cl;
	char *endep; 
	lp = *rp;
	if((n = Popwchar(lp, cl)) == -1)
		return(-1);
	*rp = lp + (n ? n : 1);
	c = cl;
	/* look for eight bit characters in bitmap */
	if(c <= 0177 || c <= 0377 && iscntrl(c))
		return(ISTHERE(c) && !neg || !ISTHERE(c) && neg);
	else {
		/* look past bitmap for multibyte characters */
		endep = *(ep + 32) + ep + 32;
		ep += 33;
		for(;;) {
			if(ep >= endep)
				return(neg);
			ep += Popwchar(ep, cl);
			d = cl;
			if(d == '-') {
				ep += Popwchar(ep, cl);
				d = cl;
				if(f <= c && c <= d)
					return(!neg);
			}
			if(d == c)
				return(!neg);
			f = d;
		}
	}
}
