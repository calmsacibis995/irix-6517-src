/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak snprintf = _snprintf
#endif
#include "synonyms.h"
#include "shlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <values.h>
#include "prt_extern.h"

/*VARARGS3*/
int
snprintf(char *string, ssize_t cnt, const char *format, ...)
{
	register int count;
	FILE siop;
	va_list ap;

	if (cnt <= 0)
		return 0;

	siop._cnt = cnt - 1;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD; /* distinguish dummy file descriptor */
	va_start(ap, format);
	count = _doprnt(format, ap, &siop);
	va_end(ap);
	*siop._ptr = '\0'; /* plant terminating null character */
	return(count);
}
