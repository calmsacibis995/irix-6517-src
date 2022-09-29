/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/setbuf.c	2.11"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>
#include "stdiom.h"
#include "stdlib.h"

void
setbuf(register FILE *iop, char *abuf)
{
	register Uchar *buf = (Uchar *)abuf;
	register int fno = iop->_file;  /* file number */
	register int size = BUFSIZ - _SMBFSZ;
	long findex = iop - _iob;
	LOCKDECLINIT(l, LOCKFILE(iop));

	if(iop->_base != 0 && iop->_flag & _IOMYBUF)
		free((char *)iop->_base);
	iop->_flag &= ~(_IOMYBUF | _IONBF | _IOLBF);
	if (buf == 0) 
	{
		iop->_flag |= _IONBF; 
#ifndef _STDIO_ALLOCATE
		if ((findex < 2) && (findex >= 0))
		{
			/* use special buffer for std{in,out} */
			buf = (findex == 0) ? _sibuf : _sobuf;
		}
		else /* needed for ifdef */
#endif
		if ((findex < (_lastbuf - _iob)) && (findex >= 0))
		{
			/*
			 * The input iop might have come out of heap. 
			 * We can not assume that heap addresses are 
			 * always greater than data addresses. So, we
			 * need to check iop against both the upper and
			 * lower iob limits.
			 */
			buf = _smbuf[findex];
			size = _SMBFSZ - PUSHBACK;
                }
                else if ((buf = (Uchar *)malloc(_SMBFSZ * sizeof(Uchar))) != 0)
		{
                       	iop->_flag |= _IOMYBUF;
			size = _SMBFSZ - PUSHBACK;
		}
	}
	else /* regular buffered I/O, standard buffer size */
	{
		if (isatty(fno))
			iop->_flag |= _IOLBF;
	}
	if (buf == 0) {
		UNLOCKFILE(iop, l);
		return ; /* malloc() failed */
	}
	iop->_base = buf;
	setbufend(iop, buf + size);
	iop->_ptr = buf;
	iop->_cnt = 0;
	UNLOCKFILE(iop, l);
}
