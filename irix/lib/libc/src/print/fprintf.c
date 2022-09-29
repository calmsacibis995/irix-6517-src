/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:print/fprintf.c	1.15"
/*LINTLIBRARY*/
#include "synonyms.h"
#include "mplib.h"
#include "shlib.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "prt_extern.h"

/*VARARGS2*/
int
fprintf(FILE *iop, const char *format, ...)
{
	register int count;
	unsigned int flag;
	va_list ap;
	int rv;
	LOCKDECLINIT(l, LOCKFILE(iop));

	va_start(ap, format);
	flag = iop->_flag;

	if (!(flag & _IOWRT)) {
		/* if no write flag */
		if ((flag & _IORW) && (flag & (_IOREAD | _IOEOF)) != _IOREAD) {
			/* if ok, cause read-write */
			iop->_flag = (flag & ~(_IOREAD | _IOEOF)) | _IOWRT;
		} else {
			/* else error */
			UNLOCKFILE(iop, l);
			setoserror(EBADF);
			return EOF;
		}
	}
	count = _doprnt(format, ap, iop);
	va_end(ap);
	rv = (ferror(iop)? EOF: count);
	UNLOCKFILE(iop, l);
	return(rv);
}
