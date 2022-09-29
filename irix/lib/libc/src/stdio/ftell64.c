/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/ftell.c	1.13"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
	#pragma weak ftell64 = _ftell64
	#pragma weak ftello64 = _ftell64
	#pragma weak _ftello64 = _ftell64
#if (_MIPS_SIM == _MIPS_SIM_NABI32)
	/* off_t == 64 bits */
	#pragma weak ftello = _ftell64
	#pragma weak _ftello = _ftell64
#endif
#endif
/*
 * Return file offset.
 * Coordinates with buffering.
 */
#include "synonyms.h"
#include "mplib.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int64_t
ftell64(FILE *iop)
{
	register long adjust;
	off64_t	tres;
	LOCKDECLINIT(l, LOCKFILE(iop));

	if (iop->_cnt < 0)
		iop->_cnt = 0;
	if (iop->_flag & _IOREAD)
		adjust = -iop->_cnt;
	else if (iop->_flag & (_IOWRT | _IORW)) 
	{
		adjust = 0;
		if (((iop->_flag & (_IOWRT | _IONBF)) == _IOWRT) && (iop->_base != 0))
			adjust = iop->_ptr - iop->_base;
	}
	else {
		setoserror(EBADF);	/* file descriptor refers to no open file */
		UNLOCKFILE(iop, l);
		return EOF;
	}

	tres = lseek64(fileno(iop), 0LL, 1);
	if (tres >= 0)
		tres += (off64_t)adjust;
	UNLOCKFILE(iop, l);
	return (int64_t)tres;
}
