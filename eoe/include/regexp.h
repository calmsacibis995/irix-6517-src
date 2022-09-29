#ifndef __REGEXP_H__
#define __REGEXP_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#ident  "$Revision: 1.23 $"

#include <standards.h>

#if _NO_XOPEN4
#include <string.h>
#endif /* _NO_XOPEN4 */

#ifdef _LANGUAGE_C_PLUS_PLUS
#define _SET(a,b) a = (char *)b
#define _STATIC static
#else
#define _SET(a,b)
#define _STATIC
#endif

#define	_CBRA	2
#define	_CCHR	4
#define	_CDOT	8
#define	_CCL	12
#define	_CXCL	16
#define	_CDOL	20
#define	_CCEOF	22
#define	_CKET	24
#define	_CBACK	36
#define _NCCL	40

#define	_STAR	01
#define _RNGE	03

#define	_NBRA	9

#define _PLACE(c)	_ep[c >> 3] |= _bittab[c & 07]
#define _ISTHERE(c)	(_ep[c >> 3] & _bittab[c & 07])
#define __ecmp(s1, s2, n)	(!strncmp(s1, s2, n))

_STATIC char *compile(char *, char *, const char *, int);
_STATIC int step(const char *, const char *);
_STATIC int advance(const char *, const char *);
static void _getrnge(const char *_str);

static char	*_braslist[_NBRA];
static char	*_braelist[_NBRA];
_STATIC int	sed, nbra;
_STATIC char	*loc1, *loc2, *locs;
static int	_nodelim;

_STATIC int	circf;
static int	_low;
static int	_size;

static unsigned char	_bittab[] = { 1, 2, 4, 8, 16, 32, 64, 128 };

static void
_getrnge(const char *_str)
{
	_low = *_str++ & 0377;
	_size = ((*_str & 0377) == 255)? 20000: (*_str &0377) - _low;
}

_STATIC char *
compile(char *instring, char *_ep, const char *endbuf, int _seof)
{
	INIT	/* Dependent declarations and initializations */
	register int c;
	register int eof = _seof;
	char *lastep = instring;
	int cclcnt;
	char bracket[_NBRA], *bracketp;
	int closed;
	int neg;
	int lc;
	int i, cflg;
	int iflag; /* used for non-ascii characters in brackets */

	lastep = 0;
	if((c = GETC()) == eof || c == '\n') {
		if(c == '\n') {
			UNGETC(c);
			_nodelim = 1;
		}
		if(*_ep == 0 && !sed)
			ERROR(41);
		RETURN(_ep);
	}
	bracketp = bracket;
	circf = closed = nbra = 0;
	if(c == '^')
		circf++;
	else
		UNGETC(c);
	for (;;) {
		if(_ep >= endbuf)
			ERROR(50);
		c = GETC();
		if(c != '*' && ((c != '\\') || (PEEKC() != '{')))
			lastep = _ep;
		if(c == eof) {
			*_ep++ = _CCEOF;
			if (bracketp != bracket)
				ERROR(42);
			RETURN(_ep);
		}

		if(_ep >= endbuf)
			ERROR(50);

		switch(c) {

		case '.':
			*_ep++ = _CDOT;
			continue;

		case '\n':
			if(!sed) {
				UNGETC(c);
				*_ep++ = _CCEOF;
				_nodelim = 1;
				if(bracketp != bracket)
					ERROR(42);
				RETURN(_ep);
			}
			else ERROR(36);
		case '*':
			if(lastep == 0 || *lastep == _CBRA || *lastep == _CKET)
				goto defchar;
			*lastep |= _STAR;
			continue;

		case '$':
			if(PEEKC() != eof && PEEKC() != '\n')
				goto defchar;
			*_ep++ = _CDOL;
			continue;

		case '[':
			if(&_ep[17] >= endbuf)
				ERROR(50);

			*_ep++ = _CCL;
			lc = 0;
			for(i = 0; i < 16; i++)
				_ep[i] = 0;

			neg = 0;
			if((c = GETC()) == '^') {
				neg = 1;
				c = GETC();
			}
			iflag = 1;
			do {
				c &= 0377;
				if(c == '\0' || c == '\n')
					ERROR(49);
				if((c & 0200) && iflag) {
					iflag = 0;
					if(&_ep[32] >= endbuf)
						ERROR(50);
					_ep[-1] = _CXCL;
					for(i = 16; i < 32; i++)
						_ep[i] = 0;
				}
				if(c == '-' && lc != 0) {
					if((c = GETC()) == ']') {
						_PLACE('-');
						break;
					}
					if((c & 0200) && iflag) {
						iflag = 0;
						if(&_ep[32] >= endbuf)
							ERROR(50);
						_ep[-1] = _CXCL;
						for(i = 16; i < 32; i++)
							_ep[i] = 0;
					}
					while(lc < c ) {
						_PLACE(lc);
						lc++;
					}
				}
				lc = c;
				_PLACE(c);
			} while((c = GETC()) != ']');
			
			if(iflag)
				iflag = 16;
			else
				iflag = 32;
			
			if(neg) {
				if(iflag == 32) {
					for(cclcnt = 0; cclcnt < iflag; cclcnt++)
						_ep[cclcnt] ^= 0377;
					_ep[0] &= 0376;
				} else {
					_ep[-1] = _NCCL;
					/* make nulls match so test fails */
					_ep[0] |= 01;
				}
			}

			_ep += iflag;

			continue;

		case '\\':
			switch(c = GETC()) {

			case '(':
				if(nbra >= _NBRA)
					ERROR(43);
				*bracketp++ = nbra;
				*_ep++ = _CBRA;
				if(_ep >= endbuf)
					ERROR(50);
				*_ep++ = nbra++;
				continue;

			case ')':
				if(bracketp <= bracket) 
					ERROR(42);
				*_ep++ = _CKET;
				if(_ep >= endbuf)
					ERROR(50);
				*_ep++ = *--bracketp;
				closed++;
				continue;

			case '{':
				if(lastep == (char *) 0)
					goto defchar;
				*lastep |= _RNGE;
				cflg = 0;
			nlim:
				c = GETC();
				i = 0;
				do {
					if('0' <= c && c <= '9')
						i = 10 * i + c - '0';
					else
						ERROR(16);
				} while(((c = GETC()) != '\\') && (c != ','));
				if(i >= 255)
					ERROR(11);
				*_ep++ = i;
				if(c == ',') {
					if(cflg++)
						ERROR(44);
					if((c = GETC()) == '\\') {
						if(_ep >= endbuf)
							ERROR(50);
						*_ep++ = 255;
					} else {
						UNGETC(c);
						goto nlim;
						/* get 2'nd number */
					}
				}
				if(GETC() != '}')
					ERROR(45);
				if(!cflg) {	/* one number */
					if(_ep >= endbuf)
						ERROR(50);
					*_ep++ = i;
				} else if((_ep[-1] & 0377) < (_ep[-2] & 0377))
					ERROR(46);
				continue;

			case '\n':
				ERROR(36);

			case 'n':
				c = '\n';
				goto defchar;

			default:
				if(c >= '1' && c <= '9') {
					if((c -= '1') >= closed)
						ERROR(25);
					*_ep++ = _CBACK;
					if(_ep >= endbuf)
						ERROR(50);
					*_ep++ = c;
					continue;
				}
			}
	/* Drop through to default to use \ to turn off special chars */

		defchar:
		default:
			lastep = _ep;
			*_ep++ = _CCHR;
			if(_ep >= endbuf)
				ERROR(50);
			*_ep++ = c;
		}
	}
}

_STATIC int
step(const char *_p1, const char *_p2)
{
	register int c;


	if(circf) {
		loc1 = (char *)_p1;
		return(advance(_p1, _p2));
	}
	/* fast check for first character */
	if(*_p2 == _CCHR) {
		c = _p2[1];
		do {
			if(*_p1 != c)
				continue;
			if(advance(_p1, _p2)) {
				loc1 = (char *)_p1;
				return(1);
			}
		} while(*_p1++);
		return(0);
	}
		/* regular algorithm */
	do {
		if(advance(_p1, _p2)) {
			loc1 = (char *)_p1;
			return(1);
		}
	} while(*_p1++);
	return(0);
}

_STATIC int
advance(const char *_lp, const char *_ep)
{
	register char *curlp;
	int c;
	char *bbeg; 
	register char neg;
	int ct;

	for (;;) {
		neg = 0;
		switch(*_ep++) {

		case _CCHR:
			_SET(loc2,_lp);
			if(*_ep++ == *_lp++)
				continue;
			return(0);
	
		case _CDOT:
			_SET(loc2,_lp);
			if(*_lp++)
				continue;
			return(0);
	
		case _CDOL:
			_SET(loc2,_lp);
			if(*_lp == 0)
				continue;
			return(0);
	
		case _CCEOF:
			loc2 = (char *) _lp;
			return(1);
	
		case _CXCL: 
			_SET(loc2,_lp);
			c = (unsigned char)*_lp++;
			if(_ISTHERE(c)) {
				_ep += 32;
				continue;
			}
			return(0);
		
		case _NCCL:	
			neg = 1;

		case _CCL: 
			_SET(loc2,_lp);
			c = *_lp++;
			if(((c & 0200) == 0 && _ISTHERE(c)) ^ neg) {
				_ep += 16;
				continue;
			}
			return(0);
		
		case _CBRA:
			_braslist[*_ep++] = (char *) _lp;
			continue;
	
		case _CKET:
			_braelist[*_ep++] = (char *) _lp;
			continue;
	
		case _CCHR | _RNGE:
			c = *_ep++;
			(void)_getrnge(_ep);
			while(_low--) {
				_SET(loc2,_lp);
				if(*_lp++ != c)
					return(0);
				}
			curlp = (char *)_lp;
			while(_size--) {
				_SET(loc2,_lp);
				if(*_lp++ != c)
					break;
				}
			if(_size < 0)
				_lp++;
			_ep += 2;
			goto star;
	
		case _CDOT | _RNGE:
			(void)_getrnge(_ep);
			while(_low--) {
				_SET(loc2,_lp);
				if(*_lp++ == '\0')
					return(0);
				}
			curlp = (char *)_lp;
			while(_size--) {
				_SET(loc2,_lp);
				if(*_lp++ == '\0')
					break;
				}
			if(_size < 0)
				_lp++;
			_ep += 2;
			goto star;
	
		case _CXCL | _RNGE:
			(void)_getrnge(_ep + 32);
			while(_low--) {
				_SET(loc2,_lp);
				c = (unsigned char)*_lp++;
				if(!_ISTHERE(c))
					return(0);
			}
			curlp = (char *)_lp;
			while(_size--) {
				_SET(loc2,_lp);
				c = (unsigned char)*_lp++;
				if(!_ISTHERE(c))
					break;
			}
			if(_size < 0)
				_lp++;
			_ep += 34;		/* 32 + 2 */
			goto star;
		
		case _NCCL | _RNGE:
			neg = 1;
		
		case _CCL | _RNGE:
			(void)_getrnge(_ep + 16);
			while(_low--) {
				_SET(loc2,_lp);
				c = *_lp++;
				if(((c & 0200) || !_ISTHERE(c)) ^ neg)
					return(0);
			}
			curlp = (char *)_lp;
			while(_size--) {
				_SET(loc2,_lp);
				c = *_lp++;
				if(((c & 0200) || !_ISTHERE(c)) ^ neg)
					break;
			}
			if(_size < 0)
				_lp++;
			_ep += 18; 		/* 16 + 2 */
			goto star;
	
		case _CBACK:
			bbeg = _braslist[*_ep];
			ct = _braelist[*_ep++] - bbeg;
	
			if(__ecmp(bbeg, _lp, ct)) {
				_lp += ct;
				continue;
			}
			return(0);
	
		case _CBACK | _STAR:
			bbeg = _braslist[*_ep];
			ct = _braelist[*_ep++] - bbeg;
			curlp = (char *)_lp;
			_SET(loc2,_lp);
			while(__ecmp(bbeg, _lp, ct))
				_lp += ct;
	
			_SET(loc2,_lp);
			while(_lp >= curlp) {
				if(advance(_lp, _ep))	return(1);
				_lp -= ct;
			}
			return(0);
	
	
		case _CDOT | _STAR:
			curlp = (char *)_lp;
			_SET(loc2,_lp);
			while(*_lp++);
			goto star;
	
		case _CCHR | _STAR:
			curlp = (char *)_lp;
			_SET(loc2,_lp);
			while(*_lp++ == *_ep);
			_ep++;
			goto star;
	
		case _CXCL | _STAR:
			curlp = (char *)_lp;
			_SET(loc2,_lp);
			do {
				c = (unsigned char)*_lp++;
			} while(_ISTHERE(c));
			_ep += 32;
			goto star;
		
		case _NCCL | _STAR:
			neg = 1;

		case _CCL | _STAR:
			curlp = (char *)_lp;
			do {
				_SET(loc2,_lp);
				c = *_lp++;
			} while(((c & 0200) == 0 && _ISTHERE(c)) ^ neg);
			_ep += 16;
			goto star;
	
		star:
			do {
				if(--_lp == locs)
					break;
				if(advance(_lp, _ep))
					return(1);
			} while(_lp > curlp);
			return(0);

		}
	}
}

#ifdef __cplusplus
}
#endif
#endif /* !__REGEXP_H__ */
