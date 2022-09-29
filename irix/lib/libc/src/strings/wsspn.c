/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wsspn.c	1.1"

#pragma weak wsspn = _wsspn
#pragma weak wcsspn = _wsspn

/*
 * Return the number of characters in the maximum leading segment
 * of string which consists solely of characters from charset.
 */
#include "synonyms.h"
#include <widec.h>

size_t
wsspn(const wchar_t *string, const wchar_t *charset)
{
	register const wchar_t *p, *q;

	for (q = string; *q; ++q) {
		for (p = charset; *p && *p != *q; ++p)
			;
		if (*p == 0)
			break;
	}
	return((size_t)(q - string));
}
