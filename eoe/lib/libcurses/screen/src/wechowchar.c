/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/wechowchar.c	1.2"
#include	"curses_inc.h"

/*
 *  These routines short-circuit much of the innards of curses in order to get
 *  a single character output to the screen quickly! It is used by getch()
 *  and getstr().
 *
 *  wechowchar(WINDOW *win, chtype ch) is functionally equivalent to
 *  waddch(WINDOW *win, chtype ch), wrefresh(WINDOW *win)
 */

int
wechowchar (register WINDOW *win, chtype ch)
{
    int	saveimm = win->_immed, rv;

    immedok(win,TRUE);
    rv = waddwch(win,ch);
    win->_immed = (char) saveimm;
    return (rv);
}
