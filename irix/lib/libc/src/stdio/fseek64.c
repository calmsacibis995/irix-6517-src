/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/fseek.c	1.15"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/

#include <sgidefs.h>	/* for _MIPS_SIM defs */
#ifdef __STDC__
	#pragma weak fseek64 = _fseek64
	#pragma weak fseeko64 = _fseek64
	#pragma weak _fseeko64 = _fseek64
#if (_MIPS_SIM == _MIPS_SIM_NABI32)
	/* off_t == 64 bits */
	#pragma weak fseeko = _fseek64
	#pragma weak _fseeko = _fseek64
#endif
#endif
/*
 * Seek for standard library.  Coordinates with buffering.
 */
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include "stdiom.h"
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

int
fseek64(FILE *iop, int64_t offset, int ptrname)
{
	off64_t	p;
	LOCKDECLINIT(l, LOCKFILE(iop));

	if(iop->_flag & _IOREAD) {
		if (ptrname == 1 && iop->_base && !(iop->_flag&_IONBF)) {
			/*  Added for xfs support */
			if (offset <= (long long)LONG_MAX) {
				/* see if we can seek within the buffer */
				if (!bufdirty(iop) && !(iop->_flag & _IORW)
				    && offset >= iop->_base - iop->_ptr
				    && offset < iop->_cnt)
				{
					iop->_cnt -= offset;
					iop->_ptr += offset;
					UNLOCKFILE(iop, l);
					return 0;
				}
			}
			/* the underlying file offset is different from
			 * the read ptr, adjust the offset value so that
			 * the seek makes sense. */
			offset -= (off64_t)iop->_cnt;
		}
	} else if(iop->_flag & (_IOWRT | _IORW)) {
		if (fflush(iop) == EOF) {
			UNLOCKFILE(iop, l);
			return(-1);
		}
	} else {
		/* File is not open for reading or writing */
		UNLOCKFILE(iop, l);
		setoserror(EBADF);
		return(-1);
	}
	iop->_flag &= (unsigned short)~_IOEOF;
	iop->_cnt = 0;
	iop->_ptr = iop->_base;
	if(iop->_flag & _IORW) {
		iop->_flag &= (unsigned short)~(_IOREAD | _IOWRT);
	}
	if (bufendadj(iop))
		resetbufend(iop);
	p = lseek64(fileno(iop), (off64_t)offset, ptrname);
	UNLOCKFILE(iop, l);
	return((p == -1LL)? -1: 0);
}
