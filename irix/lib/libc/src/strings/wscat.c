/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstring/wscat.c	1.1"

#pragma weak wscat = _wscat
#pragma weak wcscat = _wscat

/*
 * Concatenate s2 on the end of s1. s1's space must be large enough.
 * return s1.
 */
#include "synonyms.h"
#include <widec.h>

wchar_t *
wscat(register wchar_t *s1, const wchar_t *s2)
{
	register wchar_t *os1 = s1;

	while (*s1++) /* find end of s1 */
		;
	--s1;
	while (*s1++ = *s2++) /* copy s2 to s1 */
		;
	return(os1);
}
