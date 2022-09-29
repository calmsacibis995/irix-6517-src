/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wspbrk.c	1.1"

#pragma weak wspbrk = _wspbrk
#pragma weak wcspbrk = _wspbrk

/*
 * Return ptr to first occurance of any character from 'brkset'
 * in the wchar_t array 'string'; NULL if none exists.
 */
#include "synonyms.h"
#define  WNULL	(wchar_t *)0
#include <widec.h>

wchar_t *
wspbrk(const wchar_t *string, const wchar_t *brkset)
{
	const wchar_t *p;

	do {
		for (p = brkset; *p && *p != *string; ++p)
			;
		if (*p)
			return((wchar_t *)string);
	} while (*string++);
	return(WNULL);
}
