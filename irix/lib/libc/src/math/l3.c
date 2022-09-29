/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/l3.c	1.13"
/*LINTLIBRARY*/
/*
 * Convert longs to and from 3-byte disk addresses
 */
#ifdef __STDC__
	#pragma weak l3tol = _l3tol
	#pragma weak ltol3 = _ltol3
#endif
#include "synonyms.h"

void
ltol3(char *cp, long *lp, int n)
{
	register int i;
	register char *a, *b;

	a = cp;
	b = (char *)lp;
	for(i=0; i < n; ++i) {
#if interdata || u370 || u3b || M32 || _MIPSEB
		b++;
		*a++ = *b++;
		*a++ = *b++;
		*a++ = *b++;
#endif
#if vax || _MIPSEL
		*a++ = *b++;
		*a++ = *b++;
		*a++ = *b++;
		b++;
#endif
#if pdp11
		*a++ = *b++;
		b++;
		*a++ = *b++;
		*a++ = *b++;
#endif
	}
}

void
l3tol(long *lp, char *cp, int n)
{
	register int i;
	register char *a, *b;

	a = (char *)lp;
	b = cp;
	for(i=0; i < n; ++i) {
#if interdata || u370 || u3b || M32 || _MIPSEB
		*a++ = 0;
		*a++ = *b++;
		*a++ = *b++;
		*a++ = *b++;
#endif
#if vax || _MIPSEL
		*a++ = *b++;
		*a++ = *b++;
		*a++ = *b++;
		*a++ = 0;
#endif
#if pdp11
		*a++ = *b++;
		*a++ = 0;
		*a++ = *b++;
		*a++ = *b++;
#endif
	}
}
