/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/

#ifdef __STDC__
	#pragma weak vsnprintf = _vsnprintf
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <values.h>
#include "prt_extern.h"

/*VARARGS3*/
int
vsnprintf(char *string, ssize_t cnt, const char *format, va_list ap)
{
	register int count;
	FILE siop;

	if (cnt <= 0)
		return 0;

	siop._cnt = cnt - 1;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD; /* distinguish dummy file descriptor */
	count = _doprnt(format, ap, &siop);
	*siop._ptr = '\0'; /* plant terminating null character */
	return(count);
}
