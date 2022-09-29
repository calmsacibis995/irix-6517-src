/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wctype/trwctype.c	1.1.1.1"

/*
	__trwctype(c) converts lower-case character to upper-case 
*/
#include "synonyms.h"
#include <ctype.h>
#include <wchar.h>
#include "_wchar.h"
#include <locale.h>
#include <string.h>

#pragma weak _trwctype = __trwctype

wint_t
__trwctype(wint_t c, wctype_t x)
{
	register int s;
	register wchar_t w;

	if (iscodeset1(c)) {
		s = 0;
	} else if (iscodeset2(c)) {
		s = 1;
	} else if (iscodeset3(c)) {
		s = 2;
	} else
		return(c);
	/*
	 *	pick up the 7 bits data from c
	 */
	w = c & 0x001FFFFF; /* under 21 bit mask */
	if (_wcptr[s] == 0 || _wctbl[s].code == 0 ||
	    w < _wcptr[s]->cmin || w > _wcptr[s]->cmax)
		return(c);
	if( !iswctype(c,x) ) return(c);
	w =  _wctbl[s].code[w - _wcptr[s]->cmin]; /* fetch value */
	return( w | (s==0?P11:s==1?P01:P10));   /* conv to 32-bit PC */
}
