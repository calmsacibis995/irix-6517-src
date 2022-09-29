/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wscpy.c	1.1"

#pragma weak wscpy = _wscpy
#pragma weak wcscpy = _wscpy

/*
 * Copy string s2 to s1. S1 must be large enough.
 * Return s1.
 */
#include "synonyms.h"
#include <widec.h>

wchar_t *
wscpy(wchar_t *s1, const wchar_t *s2)
{
	register wchar_t *os1 = s1;

	while (*s1++ = *s2++)
		;
	return(os1);
}
