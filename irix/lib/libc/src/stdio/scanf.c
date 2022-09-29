/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/scanf.c	1.13"
/*LINTLIBRARY*/
#include "synonyms.h"
#include "mplib.h"
#include <sgidefs.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "stdiom.h"

/*VARARGS1*/
int
scanf(const char *fmt, ...)
{
	va_list ap;
	int rv;
	LOCKDECLINIT(l, LOCKFILE(stdin));

	va_start(ap,fmt );
	rv = (_doscan(stdin,(unsigned char *)fmt, ap));
	UNLOCKFILE(stdin, l);
	return(rv);
}

/*VARARGS2*/
int
fscanf(FILE *iop, const char *fmt, ...)
{
	va_list ap;
	int rv;
	LOCKDECLINIT(l, LOCKFILE(iop));

	va_start(ap,fmt );
	rv = (_doscan(iop,(unsigned char *)fmt, ap));
	UNLOCKFILE(iop, l);
	return(rv);
}

/*VARARGS2*/
int
sscanf(register const char *str, const char *fmt, ...)
{
	va_list ap;
	FILE strbuf;

	va_start(ap,fmt );
	/* The dummy FILE * created for sscanf has the _IOWRT
	 * flag set to distinguish it from scanf and fscanf
	 * invocations. */
	strbuf._flag = _IOREAD | _IOWRT;
	strbuf._ptr = strbuf._base = (unsigned char*)str;
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
	strbuf._cnt = (long)strlen(str);
#else
	strbuf._cnt = (int)strlen(str);
#endif
	strbuf._file = -1;
	return(_doscan(&strbuf,(unsigned char *)fmt, ap));
}
