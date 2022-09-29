/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wschr.c	1.1"

#pragma weak wschr = _wschr
#pragma weak wcschr = _wschr

/*
 * Return the ptr in sp at which the character c appears;
 * Null if not found.
 */
#include "synonyms.h"
#include <widec.h>
#define  WNULL	(wchar_t *)0

wchar_t *
wschr(const wchar_t *sp, register wchar_t c)
{
	do {
		if (*sp == c)
			return((wchar_t *)sp); /* found c in sp */
	} while (*sp++);
	return(WNULL); /* c not found */
}
