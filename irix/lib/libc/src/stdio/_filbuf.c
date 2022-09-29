/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/_filbuf.c	1.8"
/*LINTLIBRARY*/

#ifdef __STDC__
	#pragma weak _filbuf = __filbuf
#endif
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>
#include <errno.h>
#include "stdiom.h"
#include <sys/types.h>
#include <unistd.h>		/* prototype for read() */

int
_filbuf(register FILE *iop)	/* fill buffer, return first character or EOF */
{
	register ssize_t res;
	register size_t nbyte;
	register Uchar *endbuf;

	assert(ISLOCKFILE(iop));
	if (!(iop->_flag & _IOREAD))	/* check, correct permissions */
	{
		if (iop->_flag & _IORW)
			iop->_flag |= _IOREAD; /* change direction to read - fseek */
		else {
			setoserror(EBADF);
			return EOF;
		}
	}

	if (iop->_base == 0)
	{
		if ((endbuf = _findbuf(iop)) == 0) /* get buffer and end_of_buffer*/
			return EOF;
	}
	else
		endbuf = _bufend(iop);
	
	/*
	* Flush all line-buffered streams before we
	* read no-buffered or line-buffered input.
	*/
	if (iop->_flag & (_IONBF | _IOLBF))
		_flushlbf();
	/*
	* Fill buffer or read 1 byte for unbuffered, handling any errors.
	*/
	iop->_ptr = iop->_base;
	if (iop->_flag & _IONBF)
		nbyte = 1;
	else
		nbyte = (size_t)(endbuf - iop->_base);
	if ((res = read(iop->_file, (char *)iop->_base, nbyte)) > 0)
	{
		iop->_cnt = res - 1;
		setbufclean(iop);
		return *iop->_ptr++;
	}
	else
	{
		iop->_cnt = 0;
		if (res == 0)
			iop->_flag |= _IOEOF;
		else
			iop->_flag |= _IOERR;
		return EOF;
	}
}
