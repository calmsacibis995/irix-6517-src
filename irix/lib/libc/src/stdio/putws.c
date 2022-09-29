/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstdio/putws.c	1.1"

/*
 * Putws transforms process codes in wchar_t array pointed to by
 * "ptr" into a byte string in EUC, and writes the string followed
 * by a new-line character to stdout.
 */
#include <stdio.h>
#include <widec.h>

#pragma weak putws = _putws

#include "synonyms.h"

int
putws(register wchar_t *ptr)
{
	register wchar_t *ptr0 = ptr;

	for ( ; *ptr; ptr++) { /* putwc till NULL */
		if (putwc(*ptr, stdout) == EOF)
			return(EOF);
	}
	putwc('\n', stdout); /* append a new line */

	if (fflush(stdout))  /* flush line */
		return(EOF);
	return((int)(ptr - ptr0 + 1));
}
