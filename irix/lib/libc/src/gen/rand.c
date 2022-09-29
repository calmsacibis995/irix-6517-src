/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/rand.c	1.5"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
#ifdef __STDC__
#ifndef  _LIBC_ABI
	#pragma weak rand_r = _rand_r
#endif /* _LIBC_ABI */
#endif
#include "synonyms.h"
#include "mplib.h"

static int randx=1;

void
srand(unsigned x)
{
	randx = (int) x;
}

int
rand(void)
{
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
 	if (__us_rsthread_misc) {
		int rv;
		__libc_lockrand();
		randx = randx * (int)1103515245L + 12345;
		rv = (randx >>16) & 0x7fff;
		__libc_unlockrand();
		return(rv);
	} else
#endif /* _LIBC_ABI */
		randx = randx * (int)1103515245L + 12345;
		return((randx >>16) & 0x7fff);
}

int
rand_r(unsigned int *s)
{
	int *rx = (int *)s;
	*rx = *rx * (int)1103515245L + 12345;
	return((*rx >>16) & 0x7fff);
}
