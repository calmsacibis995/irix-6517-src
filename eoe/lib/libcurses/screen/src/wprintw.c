/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/wprintw.c	1.10"
/*
 * printw and friends
 *
 */

# include	"curses_inc.h"

#ifdef __STDC__
#include	<stdarg.h>
#else
#include <varargs.h>
#endif

/*
 *	This routine implements a printf on the given window.
 */
int
/*VARARGS*/
#ifdef __STDC__
wprintw(WINDOW *win, char *fmt, ...)
#else
wprintw(va_alist)
va_dcl
#endif
{
	va_list ap;
#ifndef __STDC__
	register WINDOW	*win;
	register char * fmt;
#endif

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
	win = va_arg(ap, WINDOW *);
	fmt = va_arg(ap, char *);
#endif
	return vwprintw(win, fmt, ap);
	va_end(ap);
}
