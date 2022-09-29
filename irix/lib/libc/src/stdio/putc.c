/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/putc.c	1.4"
/*LINTLIBRARY*/

#ifdef __STDC__
#ifndef  _LIBC_ABI
	#pragma weak putc_unlocked = _putc_unlocked
	#pragma weak __semputc = putc
	#pragma weak _semputc = putc
#endif /* _LIBC_ABI */
#endif
#include "synonyms.h"
#include "mplib.h"
#include <stdio.h>

#undef putc
#undef putc_unlocked

int
putc(ch, iop)
	int ch;
	register FILE *iop;
{
	int rv;
	LOCKDECLINIT(l, LOCKFILE(iop));

	if (--iop->_cnt < 0)
		rv =  _flsbuf(ch, iop);
	else
		rv =  *iop->_ptr++ = (unsigned char)ch;
	UNLOCKFILE(iop, l);
	return(rv);
}

int
_putc_unlocked(ch, iop)
	int ch;
	register FILE *iop;
{
	if (--iop->_cnt < 0)
		return _flsbuf(ch, iop);
	else
		return *iop->_ptr++ = (unsigned char)ch;
}
