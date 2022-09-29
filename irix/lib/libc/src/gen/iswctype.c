/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wctype/iswctype.c	1.1"

/*
	__iswctype(c,x) returns true if 'c' is classified x
*/

#include <wchar.h>
#include "_wchar.h"
#include <ctype.h>
#include <locale.h>
#include "_locale.h"
#include <string.h>

/* backward compat for a while - old wctype.h used a global _ctmp_! */
wchar_t _ctmp_;

int
__iswctype(wint_t c, wctype_t x)
{
	register int s;

	if (iscodeset1(c)) {
		s = 0;
	} else if (iscodeset2(c)) {
		s = 1;
	} else if (iscodeset3(c)) {
		s = 2;
	} else
		return(0);
	/*
	 *	pick up the 7 bits data from c
	 */
	c &= 0x001FFFFF; /* under 21 bit mask */
	if (_wcptr[s] == 0 || _wctbl[s].index == 0 ||
	    c < _wcptr[s]->tmin || c > _wcptr[s]->tmax)
		return(0);
	return((x & _wctbl[s].type[_wctbl[s].index[c - _wcptr[s]->tmin]]) != 0);
}
