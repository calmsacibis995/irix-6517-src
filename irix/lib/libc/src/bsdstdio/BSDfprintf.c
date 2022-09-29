/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

#ident	"@(#)ucblibc:port/print/fprintf.c	1.2"

#ifdef __STDC__
	#pragma weak BSDfprintf = _BSDfprintf
#endif
#include "synonyms.h"

/*LINTLIBRARY*/
#include <stdio.h>
#include <stdarg.h>
#include "bsd_extern.h"

#define	_doprnt _BSD_doprnt

/*VARARGS2*/
int
BSDfprintf(FILE *iop, const char *format, ...)
{
	register int count;
	va_list ap;

	va_start(ap, format);
	if (!(iop->_flag & _IOWRT)) {
		/* if no write flag */
		if (iop->_flag & _IORW) {
			/* if ok, cause read-write */
			iop->_flag |= _IOWRT;
		} else {
			/* else error */
			return EOF;
		}
	}
	count = _doprnt(format, ap, iop);
	va_end(ap);
	return(count);
}
