/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/memchr.c	1.3.1.10"
/*LINTLIBRARY*/
/*
 * Return the ptr in sptr at which the character c1 appears;
 *   NULL if not found in n chars; don't stop at \0.
 */
#include "synonyms.h"
#include <stddef.h>
#include <string.h>

VOID * 
memchr(sptr, c1, n)
const VOID *sptr;
int c1;
size_t n;
{
 	register const char *sp = sptr;
	register const char *end = sp + n;
	register unsigned char c = (unsigned char)c1;

	while(sp != end) {
		if(*sp == c) return ((VOID *)sp);
		sp++;
	}
        return NULL;
}
