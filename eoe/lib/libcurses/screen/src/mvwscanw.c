/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/mvwscanw.c	1.10"

# include	"curses_inc.h"

#ifdef __STDC__
#include	<stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * implement the mvscanw commands.  Due to the variable number of
 * arguments, they cannot be macros.  Another sigh....
 *
 */

int
/*VARARGS*/
#ifdef __STDC__
mvwscanw(WINDOW *win, int y, int x, char *fmt, ...)
#else
mvwscanw(va_alist)
va_dcl
#endif
{
#ifndef __STDC__
	register WINDOW	*win;
	register int	y, x;
	register char	*fmt;
#endif
	int retval;
	va_list ap;

#ifdef __STDC__
        va_start(ap, fmt);
#else
	va_start(ap);
	win = va_arg(ap, WINDOW *);
	y = va_arg(ap, int);
	x = va_arg(ap, int);
	fmt = va_arg(ap, char *);
#endif

	if (wmove(win, y, x) == OK)
		retval = vwscanw(win, fmt, ap);
	else
		retval = ERR;

	va_end(ap);
	return retval;
}
