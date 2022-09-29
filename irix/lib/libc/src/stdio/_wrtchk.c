/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/_wrtchk.c	1.6"
/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>
#include "stdiom.h"
#include <errno.h>

#undef _wrtchk

int
_wrtchk(FILE *iop)/* check permissions, correct for read & write changes */
{
	unsigned int flags = iop->_flag;
	assert(ISLOCKFILE(iop));

	if ((flags & (_IOWRT | _IOEOF)) != _IOWRT)
	{
		if (!(flags & (_IOWRT | _IORW))
		    || (flags & (_IOREAD | _IOEOF)) == _IOREAD) {
			iop->_flag |= _IOERR;
			setoserror(EBADF);
			return EOF; /* stream is not writeable */
		}
		iop->_flag = (flags & ~(_IOREAD | _IOEOF)) | _IOWRT;
	}

	/* if first I/O to the stream get a buffer */
	if (iop->_base == 0 && _findbuf(iop) == 0)
		return (EOF);
	else if ((iop->_ptr == iop->_base) && !(flags & (_IOLBF | _IONBF)))
	{
		iop->_cnt = _bufend(iop) - iop->_ptr;
	}
	return 0;
}
