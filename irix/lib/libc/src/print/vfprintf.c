/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:print/vfprintf.c	1.7.1.4"
/*LINTLIBRARY*/

#ifdef __STDC__
	#pragma weak vfprintf = _vfprintf
#endif
#include "synonyms.h"
#include "mplib.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "prt_extern.h"

/*VARARGS2*/
int
vfprintf(FILE *iop, const char *format, va_list ap)
{
	register int count;
	int rv;
	LOCKDECLINIT(l, LOCKFILE(iop));

	if (!(iop->_flag & _IOWRT)) {
		/* if no write flag */
		if (iop->_flag & _IORW) {
			/* if ok, cause read-write */
			iop->_flag |= _IOWRT;
		} else {
			/* else error */
			UNLOCKFILE(iop, l);
			setoserror(EBADF);
			return EOF;
		}
	}
	count = _doprnt(format, ap, iop);
	rv = (ferror(iop)? EOF: count);
	UNLOCKFILE(iop, l);
	return(rv);
}
