/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/pfmt.c	1.5"

/*LINTLIBRARY*/

#ifdef __STDC__
	#pragma weak pfmt = _pfmt
#endif
#include "synonyms.h"
#include <pfmt.h>
#include <stdio.h>
#include <stdarg.h>

/* pfmt() - format and print */

int
pfmt(FILE *stream, long flag, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	return __pfmt_print(stream, flag, format, NULL, NULL, args);
}
