/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wscmp.c	1.1"

#pragma weak wscmp = _wscmp
#pragma weak wcscmp = _wscmp

/*
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
*/

#include "synonyms.h"
#include <widec.h>

int
wscmp(const wchar_t *s1, const wchar_t *s2)
{
	register const wchar_t *wus1, *wus2;

	wus1 = s1;
	wus2 = s2;

	if (wus1 == wus2)
		return(0);

	while (*wus1 == *wus2++)
		if (*wus1++ == 0)
			return(0);
	return((int)(*wus1 - *--wus2));
}
