/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/assert.c	1.4.2.1"
/*LINTLIBRARY*/
/*
 *	called from "assert" macro; prints without printf or stdio.
 */

#ifdef __STDC__
	#pragma weak _assert = __assert
#endif
#include "synonyms.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "priv_extern.h"

#define WRITE(s, n)	(void) _write(2, (s), (n))
#define WRITESTR(s1, n, s2)	WRITE((s1), n), \
				WRITE((s2), (unsigned) strlen(s2))

void
_assert(const char *assertion, const char *filename, int line_num)
{
	char linestr[] = ", line NNNNNN";
	register char *p = &linestr[7];
	register int div, digit;
#ifndef _LIBC_NOMP
	char pidstr[] = ", pid PPPPPP";
	pid_t pid;
#endif

	WRITESTR("Assertion failed: ", 18, assertion);
	WRITESTR(", file ", 7, filename);
	for (div = 100000; div != 0; line_num %= div, div /= 10)
		if ((digit = line_num/div) != 0 || p != &linestr[7] || div == 1)
			*p++ = (char)(digit + '0');
	*p = '\0';
	WRITE(linestr, (unsigned) strlen(linestr));
#ifndef _LIBC_NOMP
	if (__multi_thread) {
		p = &pidstr[6];
		pid = getpid();
		for (div = 100000; div != 0; pid %= div, div /= 10)
			if ((digit = pid/div) != 0 || p != &pidstr[6] || div == 1)
				*p++ = (char)(digit + '0');
		*p = '\0';
		WRITE(pidstr, (unsigned) strlen(pidstr));
	}
#endif
	WRITE("\n", 1);
	(void) abort();
}
