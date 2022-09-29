/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstdio/getws.c	1.1"

/*
 * Getws reads EUC characters from stdin, converts them to process
 * codes, and places them in the array pointed to by "s". Getws
 * reads until a new-line character is read or an EOF.
 */

#include <stdio.h>
#include <widec.h>

#pragma weak getws = _getws

#include "synonyms.h"

wchar_t *
getws(wchar_t *ptr)
{
	wchar_t *ptr0 = ptr;
	register int c;

	for ( ; ; ) {
		if ((c = getwc(stdin)) == EOF) {
			if (ptr == ptr0) /* no data */
				return(NULL);
			break; /* no more data */
		}
		if (c == '\n') /* new line character */
			break;
		*ptr++ = c;
	}
	*ptr = 0;
	return(ptr0);
}
