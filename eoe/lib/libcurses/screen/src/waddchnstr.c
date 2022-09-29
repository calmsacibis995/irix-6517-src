/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/waddchnstr.c	1.4"
#include	"curses_inc.h"

/*
 * Add ncols worth of data to win, using string as input.
 * Return the number of chtypes copied.
 */
int
waddchnstr(register WINDOW *win, chtype *string, int ncols)
{
    int		my_x = win->_curx;
    int		my_y = win->_cury;
    int		remcols;
    int		b;
    int		sw;
    int		ew;

    if (ncols < 0) {
	remcols = win->_maxx - my_x;
	while (*string && remcols) {
	    sw = mbscrw((int) _CHAR(*string));
	    ew = mbeucw((int) _CHAR(*string));
	    if (remcols < sw)
		    break;
	    for (b = 0; b < ew; b++) {
		if (waddch(win, *string++) == ERR)
			goto out;
	    }
	    remcols -= sw;
	}
    }
    else
    {
	remcols = win->_maxx - my_x;
	while ((*string) && (remcols > 0) && (ncols > 0)) {
	    sw = mbscrw((int) _CHAR(*string));
	    ew = mbeucw((int) _CHAR(*string));
	    if ((remcols < sw) || (ncols < ew))
		break;
	    for (b = 0; b < ew; b++) {
		if (waddch(win, *string++) == ERR)
			goto out;
	    }
	    remcols -= sw;
	    ncols -= ew;
	}
    }
out:
    /* restore cursor position */
    win->_curx = (short) my_x;
    win->_cury = (short) my_y;

    win->_flags |= _WINCHANGED;

    /* sync with ancestor structures */
    if (win->_sync)
	wsyncup(win);

    return (win->_immed ? wrefresh(win) : OK);
}
