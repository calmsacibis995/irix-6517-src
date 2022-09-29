/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/mvprintw.c	1.8"
# include	"curses_inc.h"

#ifdef __STDC__
#include	<stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * implement the mvprintw commands.  Due to the variable number of
 * arguments, they cannot be macros.  Sigh....
 *
 */

int
/*VARARGS*/
#ifdef __STDC__
mvprintw(int y, int x, char *fmt, ...)
#else
mvprintw(va_alist)
va_dcl
#endif
{
#ifndef __STDC__
	register int	y, x;
	register char *fmt;
#endif
	int retval;
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
	y = va_arg(ap, int);
	x = va_arg(ap, int);
	fmt = va_arg(ap, char *);
#endif
	if (move(y, x) == OK)
		retval = vwprintw(stdscr, fmt, ap);
	else
		retval = ERR;

	va_end(ap);
	return(retval);
}
