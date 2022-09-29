/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/vpfmt.c	1.2"

/*LINTLIBRARY*/

/* vpfmt() - format and print (variable argument list)
 */
#ifdef __STDC__
	#pragma weak vpfmt = _vpfmt
#endif
#include "synonyms.h"
#include <pfmt.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "pfmt_data.h"

int 
vpfmt(stream, flag, format, args)
FILE *stream;
long flag;
const char *format;
va_list args;
{
	return __pfmt_print(stream, flag, format, NULL, NULL, args);
}
