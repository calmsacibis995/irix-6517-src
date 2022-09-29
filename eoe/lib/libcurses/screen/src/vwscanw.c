/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/vwscanw.c	1.9"
/*
 * scanw and friends
 *
 */

# include	"curses_inc.h"

#ifdef __STDC__
#include	<stdarg.h>
#else
#include <varargs.h>
#endif

/*
 *	This routine actually executes the scanf from the window.
 *
 *	This code calls vsscanf, which is like sscanf except
 * 	that it takes a va_list as an argument pointer instead
 *	of the argument list itself.  We provide one until
 *	such a routine becomes available.
 */

/*VARARGS2*/
int
vwscanw(WINDOW *win, char *fmt, va_list ap)
{
	wchar_t code[256];
	char	*buf;
	register int n;

	if (wgetwstr(win, code) == ERR)
		n = ERR;
	else {
		buf = _strcode2byte(code, NULL, -1);
		n = vsscanf(buf, fmt, ap);
	}

	return n;
}
