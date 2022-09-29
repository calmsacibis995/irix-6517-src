/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstdio/fgetws.c	1.1"

/*
 * Fgetws reads EUC characters from the "iop", converts
 * them to process codes, and places them in the wchar_t
 * array pointed to by "ptr". Fgetws reads until n-1 process
 * codes are transferred to "ptr", or EOF.
 */

#include <stdio.h>
#include <widec.h>

#pragma weak fgetws = _fgetws

#include "synonyms.h"

wchar_t *
fgetws(register wchar_t *ptr, register int size, register FILE *iop)
{
	register wchar_t *ptr0 = ptr;
	register int c;

	for (size--; size > 0; size--) { /* until size-1 */
		if ((c = getwc(iop)) == EOF) {
			if (ptr == ptr0) /* no data */
				return(NULL);
			break; /* no more data */
		}
		*ptr++ = c;
		if (c == '\n')   /* new line character */
			break;
	}
	*ptr = 0;
	return(ptr0);
}
