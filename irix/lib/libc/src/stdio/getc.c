/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/getc.c	1.3"
/*LINTLIBRARY*/

#ifdef __STDC__
#ifndef  _LIBC_ABI
	#pragma weak getc_unlocked = _getc_unlocked
	#pragma weak __semgetc = getc
	#pragma weak _semgetc = getc
#endif /* _LIBC_ABI */
#endif

#include "synonyms.h"
#include "mplib.h"
#include <stdio.h>

#undef getc
#undef getc_unlocked

int
getc(iop)
	register FILE *iop;
{
	int rv;
	LOCKDECLINIT(l, LOCKFILE(iop));

	if (--iop->_cnt < 0)
		rv = _filbuf(iop);
	else
		rv = *iop->_ptr++;
	UNLOCKFILE(iop, l);
	return(rv);
}

int
_getc_unlocked(iop)
	register FILE *iop;
{
	if (--iop->_cnt < 0)
		return _filbuf(iop);
	else
		return *iop->_ptr++;
}
