/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)curses:screen/wctomb.c	1.3"
/*LINTLIBRARY*/

#include <widec.h>
#include "synonyms.h"
#include <ctype.h>
#include <stdlib.h>
#include "curses_wchar.h"

int
_curs_wctomb(char *s, wchar_t wchar)
{
	char *olds = s;
	register int size, index;
	unsigned char d;

	if(!s)
		return(0);
	if(wchar < 0200 || wchar < 0400 && iscntrl(wchar)
			|| (wchar > 0177 && wchar < 0240)) {
		*s++ = (char)wchar;
		return(1);
	}
#if !defined(_WCHAR16)
	switch(wchar & EUCMASK) {
			
		case P11:
			size = eucw1;
			break;
			
		case P01:
			*s++ = (char)SS2;
			size = eucw2;
			break;
			
		case P10:
			*s++ = (char)SS3;
			size = eucw3;
			break;
			
		default:
			return(-1);
	}
#else
	switch(wchar & H_EUCMASK) {
		
		case H_P11:
			size = eucw1;
			break;
		
		case H_P01:
			*s++ = (char)SS2;
			size = eucw2;
			break;
		
		case H_P10:
			*s++ = (char)SS3;
			size = eucw3;
			break;
		
		default:
			return(-1);
	}
#endif
	if((index = size) <= 0)
		return(-1);	
	while(index--) {
		d = (unsigned char) (wchar | 0200);
#if !defined(_WCHAR16)
		wchar >>= 7;
#else
		wchar >>= 8;
#endif
		if(iscntrl(d) || (d > 0177 && d < 0240))
			return(-1);
		s[index] = d;
	}
	return((int) (s + size - olds));
}
