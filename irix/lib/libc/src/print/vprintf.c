/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:print/vprintf.c	1.7.1.4"
/*LINTLIBRARY*/

#ifdef __STDC__
	#pragma weak vprintf = _vprintf
#endif
#include "synonyms.h"
#include "mplib.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "prt_extern.h"

/*VARARGS1*/
int
vprintf(const char *format, va_list ap)
{
	register int count;
	int rv;
	LOCKDECLINIT(l, LOCKFILE(stdout));

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
	rv = (ferror(stdout)? EOF: count);
	UNLOCKFILE(stdout, l);
	return(rv);
}
