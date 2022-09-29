/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:print/printf.c	1.14"
/*LINTLIBRARY*/
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "prt_extern.h"

/*VARARGS1*/
int
printf(const char *format, ...)
{
	register int count;
	va_list ap;
	int rv;
	LOCKDECLINIT(l, LOCKFILE(stdout));

	va_start(ap, format);
	if (!(stdout->_flag & _IOWRT)) {
		/* if no write flag */
		if (stdout->_flag & _IORW) {
			/* if ok, cause read-write */
			stdout->_flag |= _IOWRT;
		} else {
			/* else error */
			UNLOCKFILE(stdout, l);
			setoserror(EBADF);
			return EOF;
		}
	}
	count = _doprnt(format, ap, stdout);
	va_end(ap);
	rv = (ferror(stdout)? EOF: count);
	UNLOCKFILE(stdout, l);
	return(rv);
}
