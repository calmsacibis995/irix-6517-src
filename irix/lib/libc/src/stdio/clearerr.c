/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/clearerr.c	1.3"
/*LINTLIBRARY*/

#ifdef __STDC__
#ifndef  _LIBC_ABI
	#pragma weak clearerr_unlocked = clearerr
	#pragma weak clearerr_locked = clearerr
#endif /* _LIBC_ABI */
#endif
#include "synonyms.h"
#include "mplib.h"
#include <stdio.h>

#undef clearerr

void
clearerr(iop)
	FILE *iop;
{
	LOCKDECLINIT(l, LOCKFILE(iop));
	iop->_flag &= (unsigned char)~(_IOERR | _IOEOF);
	UNLOCKFILE(iop, l);
}
