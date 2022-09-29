/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/getchar.c	1.8"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * A subroutine version of the macro getchar.
 */
#ifdef __STDC__
#ifndef  _LIBC_ABI
	#pragma weak getchar_unlocked = _getchar_unlocked
#endif /* _LIBC_ABI */
#endif

#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>

#undef getchar
#undef getchar_unlocked

int
getchar()
{
	register FILE *iop = stdin;

	return getc(iop);
}

#if !_LIBC_ABI
int
_getchar_unlocked()
{
	register FILE *iop = stdin;

	return getc_unlocked(iop);
}
#endif
