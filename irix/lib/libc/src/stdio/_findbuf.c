/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/_findbuf.c	1.6"
/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdlib.h>
#include <stdio.h>
#include "stdiom.h"
#include <sys/types.h>
#include <sys/stat.h>

/*
* If buffer space has been pre-allocated use it otherwise malloc space.
* PUSHBACK with the buffer at the start causes the buffer contents to be
* shifted down, and the count to be bumped forward. At least 4 bytes
* of pushback are required to meet international specifications.
* Extra space at the end of the buffer allows for synchronization problems.
* If malloc() fails stdio bails out; assumption being the system is in trouble.
*/
Uchar *
_findbuf(register FILE *iop)	/* associate a buffer with stream; return 0 for success */
{
	register int fd = iop->_file;
	register Uchar *buf;
	size_t size = BUFSIZ;
	Uchar *endbuf;
	struct stat64 stbuf;		/* used to get file system block size */
	long findex = iop - _iob;

	assert(ISLOCKFILE(iop));

	if (iop->_flag & _IONBF)	/* need a small buffer, at least */
	{
	trysmall:;
		size = _SMBFSZ - PUSHBACK;
		if ((findex < (_lastbuf - _iob)) && (findex >= 0))
			buf = _smbuf[findex];
		else if ((buf = (Uchar *)malloc(_SMBFSZ * sizeof(Uchar))) != 0)
			iop->_flag |= _IOMYBUF;
	}
#ifndef _STDIO_ALLOCATE
	else if ((findex < 2) && (findex >= 0))
	{
               /*
                * The input iop might have come out of heap.
                * We can not assume that heap addresses are
                * always greater than data addresses. So, we
                * need to check iop against both the upper and
                * lower iob limits.
                */
		buf = (findex == 0) ? _sibuf : _sobuf; /* special buffer for std{in,out} */
	}
#endif
	else {
		size = BUFSIZ;

		/*
		 * The operating system can tell us the 
		 * right size for a buffer
		 */
		if( fstat64( fd, &stbuf ) == 0 ) {
			size = stbuf.st_blksize;
			if (size < 1)
				size = BUFSIZ;
			else {
				/*
				 * Many of the file systems return 'optimum'
				 * values for blksize - these are often
				 * quite large (64K+). Using these values
				 * can bloat prgrams that use stdio quite
				 * a bit. A space-time tradeoff is to
				 * permit good sizes, but not go
				 * overboard
				 */
				if (size > (16*1024))
					size = 16 * 1024;

				/*
				 * SMALL-MEMORY MACHINES:
				 * Don't use huge buffers for small read-only
				 * files (like configuration files).  We try
				 * and scale the buffer size using the filesz.
				 */
				if (iop->_flag & _IOREAD) {	/* RD-ONLY */
					if (stbuf.st_size < size)
						size = BUFSIZ;
				}
			}
		}

		if ((buf = (Uchar *)malloc(sizeof(Uchar)*((unsigned int)size+_SMBFSZ))) != 0)
			iop->_flag |= _IOMYBUF;
		else
			goto trysmall;
	}
	if (buf == 0) 
		return 0; 	/* malloc() failed */
	iop->_base = buf;
	iop->_ptr = buf;
	endbuf = iop->_base + size;
	setbufend(iop, endbuf);
	if (!(iop->_flag & _IONBF) && isatty(fd))
		iop->_flag |= _IOLBF;
	return endbuf;
}
