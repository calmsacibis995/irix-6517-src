/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libw:port/wstdio/fputws.c	1.1"
#ifdef __STDC__
	#pragma weak fputws = _fputws
#endif

/*
 * Fputws transforms the process code string pointed to by "ptr"
 * into a byte string in EUC, and writes the string to the named
 * output "iop".
 */
#include <synonyms.h>
#include <stdio.h>
#include <wchar.h>

int
fputws(const wchar_t *ptr, register FILE *iop)
{
	const wchar_t *ptr0 = ptr;

	for ( ; *ptr; ptr++) { /* putwc till NULL */
		if (putwc(*ptr, iop) == EOF)
			return(EOF);
	}
	if (fflush(iop)) /* flush line */
		return(EOF);
	return((int)(ptr - ptr0));
}
