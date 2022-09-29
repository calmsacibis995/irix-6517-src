/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/winnstr.c	1.4"
#include	"curses_inc.h"

/*
 * Copy n chars in window win from current cursor position to end
 * of window into char buffer str.  Return the number of chars copied.
 */

int
winnstr(register WINDOW *win, register char *str, register int ncols)
{
    register	int	counter = 0;
    int			cy = win->_cury;
    register	chtype	*ptr = &(win->_y[cy][win->_curx]),
			*pmax = &(win->_y[cy][win->_maxx]);
    chtype		wc;
    int			eucw, scrw, s;


    while (ISCBIT(*ptr))
	ptr--;

    if (ncols < -1)
	ncols = MAXINT;

    while (counter < ncols)
    {
	scrw = mbscrw((int) RBYTE(*ptr));
	eucw = mbeucw((int) RBYTE(*ptr));
	if (counter + eucw > ncols)
	    break;

	for (s = 0; s < scrw; s++, ptr++)
	{
	    if ((wc = RBYTE(*ptr)) == MBIT)
		continue;
	    *str++ = (char) wc;
	    counter++;
	    if ((wc = LBYTE(*ptr) | MBIT) == MBIT)
		continue;
	    *str++ = (char) wc;
	    counter++;
	}

	if (ptr >= pmax)
	{
	    if (++cy == win->_maxy)
		break;

	    ptr = &(win->_y[cy][0]);
	    pmax = ptr + win->_maxx;
	}
    }
    if (counter < ncols)
	*str = '\0';

    return (counter);
}
