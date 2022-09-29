/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/wscrl.c	1.2"
#include	"curses_inc.h"

/* Scroll the given window up/down n lines. */

int
wscrl(register WINDOW *win, int n)
{
    register	int	curx, cury, savimmed, savsync;

#ifdef	DEBUG
    if (outf)
	if (win == stdscr)
	    fprintf(outf, "scroll(stdscr, %d)\n", n);
	else
	    if (win == curscr)
		fprintf(outf, "scroll(curscr, %d)\n", n);
	    else
		fprintf(outf, "scroll(%x, %d)\n", win, n);
#endif	/* DEBUG */
    if (!win->_scroll || (win->_flags & _ISPAD))
	return (ERR);

    savimmed = win->_immed;
    savsync = win->_sync;
    win->_immed = win->_sync = FALSE;

    curx = win->_curx; cury = win->_cury;

    if (cury >= win->_tmarg && cury <= win->_bmarg)
	win->_cury = win->_tmarg;
    else
	win->_cury = 0;

    (void) winsdelln(win, -n);
    win->_curx = (short) curx;
    win->_cury = (short) cury;

    win->_sync = (bool) savsync;

    if (win->_sync)
	wsyncup(win);

    return ((win->_immed = (bool) savimmed) ? wrefresh(win) : OK);
}
