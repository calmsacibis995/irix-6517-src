/*
 * Copyright (c) 1981 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)printw.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * printw and friends
 *
 */

# include	"curses.ext"
#ifdef sgi
#include <stdarg.h>
#endif
#include 	<stdio.h>
#include	<values.h>

/*
 *	This routine implements a printf on the standard screen.
 */
printw(char	*fmt,...)
{
	va_list ap;
	FILE siop;
	char buf[512];
	int count;

	va_start(ap,fmt);
	siop._cnt = sizeof buf;
        siop._base = siop._ptr = (unsigned char *)buf;
        siop._flag = _IOREAD;	/* distinguish dummy file descriptor */
        count = _doprnt(fmt, ap, &siop);
        *siop._ptr = '\0';	/* plant terminating null character */
	va_end(ap);

	return addstr(buf);
}

