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
/*
 * Return file offset.
 * Coordinates with buffering.
 */
#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
	/* long == 64 bits */
	#pragma weak ftell64 = ftell
	#pragma weak _ftell64 = ftell
	#pragma weak ftello64 = ftell
	#pragma weak _ftello64 = ftell
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64) || (_MIPS_SIM == _MIPS_SIM_ABI32)
	/* off_t == long */
	#pragma weak ftello = ftell
	#pragma weak _ftello = ftell
#endif
#endif
#include "synonyms.h"
#include "mplib.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

long
ftell(FILE *iop)
{
	register long adjust;
	off_t	tres;
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

	tres = lseek(fileno(iop), 0L, 1);
	if (tres >= 0)
		tres += adjust;
	UNLOCKFILE(iop, l);
	return (long)tres;
}
