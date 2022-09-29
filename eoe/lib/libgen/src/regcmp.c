/*
 * regcmp.c
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

#ident "$Revision: 1.5 $"


#ifdef __STDC__
	#pragma weak regcmp = _regcmp
#endif 

#include "synonyms.h"

#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include "_wchar.h"
#include "_range.h"

#define Popwchar	if((n = mbtowc(&cl, sp, MB_LEN_MAX)) == -1) \
				goto cerror; \
			sp += n; \
			c = cl;

#define SSIZE	50
#define TGRP	48
#define A256	02
#define ZERO	01
#define	NBRA	10
#define CIRCFL	32
#define SLOP	5
#define	EOFS	0

#define	CBRA	60
#define GRP	40
#define SGRP	56
#define PGRP	68
#define EGRP	44
#define RNGE	03
#define	CCHR	20
#define	CDOT	64
#define	CCL	24
#define	NCCL	8
#define	CDOL	28
#define	CEOF	52
#define	CKET	12

#define MCCHR	36
#define MCCL	72
#define NMCCL	76
#define BCCL	80
#define NBCCL	84

#define	STAR	01
#define PLUS	02
#define MINUS	16

char *  __rpush(char *);
char *	__rpop(void);
char *	*__sp_;
char *	*__stmax;
__psint_t	__i_size;
/*VARARGS1*/
char *
#ifdef __STDC__
regcmp(const char *cs1, ...)
#else
regcmp(cs1, va_alist)
char *cs1;
va_dcl
#endif
{
	register wchar_t c;
	register char *ep;
	register const char *sp;
	char *oldep;
	const char *adx;
	__psint_t i;
	int n,cflg;
	char *lastep, *sep, *eptr;
	int nbra,ngrp;
	int cclcnt, neg;
	char *stack[SSIZE];
	wchar_t cl, lc;
	va_list ap;
	__sp_ = stack;
	*__sp_ = (char *) 0;
	__stmax = &stack[SSIZE];
#ifdef __STDC__
	va_start(ap, cs1);
#else
	va_start(ap);
#endif
	adx = cs1;
	i = nbra = ngrp = 0;
	while(adx) {
		i += strlen(adx);
		adx = va_arg(ap, const char *);	
	}
	va_end(ap);
#ifdef __STDC__
	va_start(ap, cs1);
#else
	va_start(ap);
#endif
	sp = cs1;
	adx = va_arg(ap, const char *);
	if((sep = ep = malloc((unsigned int)(2*i+SLOP))) == (char *)0) {
		va_end(ap);
		return(0);
	}
	c = (unsigned char) *sp++;
	if (c == EOFS) {
		if (adx)
			sp = adx;
		else
			goto cerror;
		adx = va_arg(ap, const char *);
	}
	if (c=='^') {
		c = (unsigned char) *sp++;
		*ep++ = CIRCFL;
	}
	if ((c=='*') || (c=='+') || (c=='{')) {
		goto cerror;
	}
	sp--;
	for (;;) {
		if(multibyte) {
			Popwchar
		} else {
			c = (unsigned char) *sp++;
		}
		if (c == EOFS) {
			if (adx) {
				sp = adx;
				adx = va_arg(ap, const char *);
				continue;
			}
			*ep++ = CEOF;
			if (--nbra > NBRA || *__sp_ != (char *) 0) {
				goto cerror;
			}
			__i_size = ep - sep;
			va_end(ap);
			return(sep);
		}
		if ((c!='*') && (c!='{')  && (c!='+'))
			lastep = ep;
		switch (c) {

		case '(':
			if (!__rpush(ep)) {
				goto cerror;
			}
			*ep++ = CBRA;
			ep++;
			continue;
		case ')':
			if (!(eptr=__rpop())) {
				goto cerror;
			}
			c = (unsigned char) *sp;
			if (c == '$') {
				sp++;
				if ('0' > (c = (unsigned char) *sp++) || c > '9') {
					goto cerror;
				}
				*ep++ = CKET;
				*ep++ = *++eptr = (char) nbra++;
				*ep++ = (char) (c-'0');
				continue;
			}
			*ep++ = EGRP;
			*ep++ = (char) ngrp++;
			switch (c) {
			case '+':
				*eptr = PGRP;
				break;
			case '*':
				*eptr = SGRP;
				break;
			case '{':
				*eptr = TGRP;
				break;
			default:
				*eptr = GRP;
				continue;
			}
			i = ep - eptr - 2;
			for (cclcnt = 0; i >= 256; cclcnt++)
				i -= 256;
			if (cclcnt > 3)  {
				goto cerror;
			}
			*eptr |= cclcnt;
			*++eptr = (char) i;
			continue;

		case '\\':
			if(multibyte) {
				Popwchar
			} else {
				c = (unsigned char) *sp++;
			}
			goto defchar;

		case '{':
			if (*lastep == CBRA || *lastep == CKET)
				goto cerror;
			*lastep |= RNGE;
			cflg = 0;
		nlim:
			if ((c = (unsigned char) *sp++) == '}') goto cerror;
			i = 0;
			do {
				if ('0' <= c && c <= '9')
					i = (i*10+(c-'0'));
				else goto cerror;
			} while (((c = (unsigned char) *sp++) != '}') && (c != ','));
			if (i>255) goto cerror;
			*ep++ = (char) i;
			if (c==',') {
				if (cflg++) goto cerror;
				if((c = (unsigned char) *sp++) == '}') {
					*ep++ = (char)-1;
					continue;
				}
				else {
					sp--;
					goto nlim;
				}
			}
			if (!cflg) *ep++ = (char) i;
			else if (((int)(unsigned char)ep[-1]) < ((int)(unsigned char)ep[-2])) goto cerror;
			continue;

		case '.':
			*ep++ = CDOT;
			continue;

		case '+':
			if (*lastep==CBRA || *lastep==CKET)
				goto cerror;
			*lastep |= PLUS;
			continue;

		case '*':
			if (*lastep==CBRA || *lastep==CKET)
			goto cerror;
			*lastep |= STAR;
			continue;

		case '$':
			if ((*sp != EOFS) || (adx))
				goto defchar;
			*ep++ = CDOL;
			continue;
		case '[':
			lc = 0;
			if(multibyte) {
				Popwchar
			} else {
				c = (unsigned char) *sp++;
			}
			neg = 0;
			if (c == '^') {
				neg = 1;
				if(multibyte) {
					Popwchar
				} else {
					c = (unsigned char) *sp++;
				}
			}
			if (multibyte) {
				if (neg) {
					*ep++ = NMCCL;
				} else {
					*ep++ = MCCL;
				}
			} else {
				if (neg) {
					*ep++ = NBCCL;
				} else {
					*ep++ = BCCL;
				}
			}
			ep++;
			cclcnt = 1;
			do {
				if (c==EOFS) {
					goto cerror;
				}
				if ((c == '-') && (lc != 0)) {
					if(multibyte) {
						Popwchar
					} else {
						c = (unsigned char) *sp++;
					}
					if (c == ']') {
						*ep++ = (unsigned char) '-';
						cclcnt++;
						break;
					}
					if (!multibyte || c <= 0177) {
						if (lc < c) {
							*ep++ = MINUS;
							cclcnt++;
						}
					} else if (valid_range(lc, c) && lc < c) {
						*ep++ = MINUS;
						cclcnt++;
					}
				}
				lc = c;
				if(!multibyte || c <= 0177 || c <= 0377 && iscntrl(c)) {
					*ep++ = (char) c;
					cclcnt++;
				} else {
					oldep = ep;
					if ((n = wctomb(ep, c)) == -1) {
						goto cerror;
					}
					ep += n;
					cclcnt += ep - oldep;
				}
				if(multibyte) {
					Popwchar
				} else {
					c = (unsigned char) *sp++;
				}
			} while (c != ']');
			if (cclcnt >= 256) {
				goto cerror;
			}
			lastep[1] = (char) cclcnt;
			continue;

		defchar:
		default:
			if (!multibyte || c <= 0177) {
				*ep++ = CCHR;
				*ep++ = (char) c;
			} else {
				*ep++ = MCCHR;
				if ((n = wctomb(ep,c)) == -1) {
					goto cerror;
				}
				ep += n;
			}
		}
	}
   cerror:
	free(sep);
	va_end(ap);
	return(0);
}

char *
__rpop(void)
{
	return (*__sp_ == (char *) -1L)?(char *) 0:*__sp_--;
}

char *
__rpush(char *ptr)
{
	if (++__sp_ > __stmax) return(0);
	*__sp_ = (char *)ptr;
	return((char *)1L);
}
