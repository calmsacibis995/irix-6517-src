/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/setvbuf.c	1.18"
/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>
#include "stdiom.h"
#include "stdlib.h"

int
setvbuf(register FILE *iop,
	char	*abuf,
	register int type,
	register size_t size)
{

	register Uchar	*buf = (Uchar *)abuf;
	long findex = iop - _iob;
	register int	sflag;
	LOCKDECLINIT(l, LOCKFILE(iop));

	sflag = iop->_flag & _IOMYBUF;
	iop->_flag &= ~(_IOMYBUF | _IONBF | _IOLBF);
	switch (type)  
	{
	/*note that the flags are the same as the possible values for type*/
	case _IONBF:
		iop->_flag |= _IONBF;	 /* file is unbuffered */
#ifndef _STDIO_ALLOCATE
		if ((findex < 2) && (findex >= 0))
		{
			/* use special buffer for std{in,out} */
			buf = (findex == 0) ? _sibuf : _sobuf;
			size = BUFSIZ;
		}
		else /* needed for ifdef */
#endif
		if ((findex < (_lastbuf - _iob)) && (findex >= 0))
		{
			buf = _smbuf[findex];
			size = _SMBFSZ - PUSHBACK;
		}
                else if ((buf = (Uchar *)malloc(_SMBFSZ * sizeof(Uchar))) != 0)
		{
                        iop->_flag |= _IOMYBUF;
			size = _SMBFSZ - PUSHBACK;
		}
		else  {
			UNLOCKFILE(iop, l);
			return EOF;
		}
		break;
	case _IOLBF:
	case _IOFBF:
		iop->_flag |= type;	/* buffer file */
		/* need at least an 8 character buffer for out_of_sync concerns. */
		if (size <= _SMBFSZ) 
		   	buf = 0;
		if (buf == 0) 
		{
		   	size = BUFSIZ;
		   	if ((buf = (Uchar *)malloc(sizeof(Uchar) * (BUFSIZ + _SMBFSZ))) != 0)
				iop->_flag |= _IOMYBUF;
			else {
				UNLOCKFILE(iop, l);
                		return EOF;
			}
		}
		else
			size -= _SMBFSZ;
		break;
	default:
		UNLOCKFILE(iop, l);
		return EOF;
	}
	if(iop->_base != 0 && sflag)
		free((char *)iop->_base);
	iop->_base = buf;
        setbufend(iop, buf + size);
	iop->_ptr = buf;
	iop->_cnt = 0;
	UNLOCKFILE(iop, l);
	return 0;
}
