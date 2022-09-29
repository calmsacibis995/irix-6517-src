/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 	Portions Copyright(c) 1988, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

#ident	"@(#)ucblibc:port/print/printf.c	1.1"

#ifdef __STDC__
	#pragma weak BSDprintf = _BSDprintf
#endif
#include "synonyms.h"

/*LINTLIBRARY*/
#include <stdio.h>
#include <stdarg.h>
#include "bsd_extern.h"

#define	_doprnt		_BSD_doprnt

/*VARARGS1*/
int
BSDprintf(const char *format, ...)
{
	register int count;
	va_list ap;

	va_start(ap, format);
	if (!(stdout->_flag & _IOWRT)) {
		/* if no write flag */
		if (stdout->_flag & _IORW) {
			/* if ok, cause read-write */
			stdout->_flag |= _IOWRT;
		} else {
			/* else error */
			return EOF;
		}
	}
	count = _doprnt(format, ap, stdout);
	va_end(ap);
	return(count);
}
