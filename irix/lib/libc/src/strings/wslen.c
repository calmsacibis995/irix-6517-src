/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wslen.c	1.1"

#pragma weak wslen = _wslen
#pragma weak wcslen = _wslen

/*
 * Returns the number of non-NULL characters in s.
 */
#include "synonyms.h"
#include <widec.h>

size_t
wslen(const wchar_t *s)
{
	register const wchar_t *s0 = s + 1;

	while (*s++)
		;
	return((size_t)(s - s0));
}
