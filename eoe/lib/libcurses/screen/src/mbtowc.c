/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)curses:screen/mbtowc.c	1.4"
/*LINTLIBRARY*/

#include <widec.h>
#include "synonyms.h"
#include <ctype.h>
#include <stdlib.h>
#include "curses_wchar.h"

int
_curs_mbtowc(wchar_t *wchar, const char *s, size_t n)
{
	register int length;
	register wchar_t intcode;
	register int c;
	char *olds = (char *)s;
	wchar_t mask;
	
	if(s == (char *)0)
		return(0);
	if(n == 0)
		return(-1);
	c = (unsigned char)*s++;
	if(c < 0200) {
		if(wchar)
			*wchar = (wchar_t)c;
		return(c ? 1 : 0);
	}
	intcode = 0;
	if (c == SS2) {
		if(!(length = eucw2)) 
			goto lab1;
#if !defined(_WCHAR16)
		mask = P01;
#else
		mask = H_P01;
#endif
		goto lab2;
	} else if(c == SS3) {
		if(!(length = eucw3)) 
			goto lab1;
#if !defined(_WCHAR16)
		mask = P10;
#else
		mask = H_P10;
#endif
		goto lab2;
	} 
lab1:
	if(iscntrl(c) || (c > 0177 && c < 0240)) {
		if(wchar)
			*wchar = (wchar_t)c;
		return(1);
	}
	length = eucw1 - 1;
#if !defined(_WCHAR16)
	mask = P11;
#else
	mask = H_P11;
#endif
	intcode = c & 0177;
lab2:
	if((size_t) length + 1 > n || length < 0)
		return(-1);
	while(length--) {
		if((c = (unsigned char)*s++) < 0200 || iscntrl(c)
					|| (c > 0177 && c < 0240))
			return(-1);
#if !defined(_WCHAR16)
		intcode= (intcode << 7) | (c & 0x7F);
#else
		intcode= (intcode << 8) | (c & 0x7F);
#endif
	}
	if(wchar)
		*wchar = intcode | mask;
	return (int) ((char *)s - olds);
}	
