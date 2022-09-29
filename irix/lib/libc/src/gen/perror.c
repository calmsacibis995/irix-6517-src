/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/perror.c	1.15"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * perror() - Print the error indicated
 * in the cerror cell.
 */
#include "synonyms.h"
#include <pfmt.h>
#include <string.h>	/* for prototyping */
#include <unistd.h> 	/* for prototyping */
#include <errno.h>
#include "mplib.h"
#include "priv_extern.h"

static const char catalog[] = "uxsyserr";

void
perror(const char *s)
{
	register const char *c = NULL;
	register const char *colon;
	register unsigned int n;
	LOCKDECLINIT(l, LOCKLOCALE);

#ifndef _LIBC_ABI
	if (oserror() >= __IRIXBASE)
		c = __irixerror(oserror());
	else if (oserror() >= _sys_nerr)
		c = __svr4error(oserror());
	else if (oserror() >= 0)
#endif /* _LIBC_ABI */
		c = __sys_errlisterror(oserror());

	if (c == NULL)
		c = __gtxt(catalog, 1, "Unknown error");

	if(s && (n = (int)strlen(s))) {
		colon = __gtxt(catalog, 2, ": ");
		(void) write(2, s, n);
		(void) write(2, colon, strlen(colon));
	}
	(void) write(2, c, (unsigned)strlen(c));
	(void) write(2, (const char *)"\n", 1);
	UNLOCKLOCALE(l);
}
