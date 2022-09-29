/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:print/vsprintf.c	1.6.1.4"
/*LINTLIBRARY*/

#ifdef __STDC__
	#pragma weak vsprintf = _vsprintf
#endif
#include "synonyms.h"
#include <stdio.h>
#include <stdarg.h>
#include <values.h>
#include "prt_extern.h"

/*VARARGS2*/
int
vsprintf(char *string, const char *format, va_list ap)
{
	register int count;
	FILE siop;

	siop._cnt = MAXINT;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD; /* distinguish dummy file descriptor */
	count = _doprnt(format, ap, &siop);
	*siop._ptr = '\0'; /* plant terminating null character */
	return(count);
}
