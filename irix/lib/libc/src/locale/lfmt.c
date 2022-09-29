/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/lfmt.c	1.3"

/* lfmt() - format, print and log */

#ifdef __STDC__
	#pragma weak lfmt = _lfmt
#endif
#include "synonyms.h"
#include <pfmt.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "pfmt_data.h"
#include "mplib.h"

int
lfmt(FILE *stream, long flag, const char *format, ...)
{
	int ret;
	va_list args;
	const char *text, *sev;
	LOCKDECLINIT(l, LOCKLOCALE);
	
	va_start(args, format);

	if ((ret = __pfmt_print(stream, flag, format, &text, &sev, args)) < 0) {
		UNLOCKLOCALE(l);
		return ret;
	}

	ret = __lfmt_log(text, sev, args, flag, ret);
	UNLOCKLOCALE(l);
	return ret;
}
