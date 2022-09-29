/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/winchnstr.c	1.3"
#include	"curses_inc.h"

/*
 * Read in ncols worth of data from window win and assign the
 * chars to string. NULL terminate string upon completion.
 * Return the number of chtypes copied.
 */

int
winchnstr(register WINDOW *win, chtype *string, int ncols)
{
    register	chtype	*ptr = &(win->_y[win->_cury][win->_curx]);
    register	int	counter = 0;
    register	int	maxcols = win->_maxx - win->_curx;
    int			eucw, scrw, s;
    chtype		rawc, attr, wc;

    if (ncols < 0)
	ncols = MAXINT;

    while (ISCBIT(*ptr))
    {
	ptr--;
	maxcols++;
    }

    while ((counter < ncols) && maxcols > 0)
    {
	eucw = mbeucw((int) RBYTE(*ptr));
	scrw = mbscrw((int) RBYTE(*ptr));

	if (counter + eucw > ncols)
	    break;
	for (s = 0; s < scrw; s++, maxcols--, ptr++)
	{
	    attr = _ATTR(*ptr);
	    rawc = _CHAR(*ptr);
	    if ((wc = RBYTE(rawc)) == MBIT)
		continue;
	    *string++ = wc | attr;
	    counter++;
	    if ((wc = LBYTE(rawc) | MBIT) == MBIT)
		continue;
	    *string++ = wc | attr;
	    counter++;
	}
    }
    if (counter < ncols)
	*string = (chtype) 0;
    return (counter);
}
