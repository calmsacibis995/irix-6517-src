/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/winsch.c	1.5"
#include	"curses_inc.h"
#include	<ctype.h>

/* Insert a character at (curx, cury). */

int
winsch(register WINDOW *win, chtype c)
{
    register	int	x, endx, n, curx = win->_curx, cury = win->_cury;
    register	chtype	*wcp, a;
    int			rv;

    a = _ATTR(c);
    c &= A_CHARTEXT;

    rv = OK;
    win->_insmode = TRUE;
    if(_scrmax > 1 && (rv = _mbvalid(win)) == ERR)
	goto done;
    /* take care of multi-byte characters */
    if(_mbtrue && ISMBIT(c))
	{
	rv = _mbaddch(win,A_NORMAL,RBYTE(c));
	goto done;
	}
    win->_nbyte = -1;
    curx = win->_curx;

    /* let waddch() worry about these */
    if (c == '\r' || c == '\b')
	return (waddch(win, c));

    /* with \n, in contrast to waddch, we don't clear-to-eol */
    if (c == '\n')
    {
	if (cury >= (win->_maxy-1) || cury == win->_bmarg)
	    return (wscrl(win, 1));
	else
	{
	    win->_cury++;
	    win->_curx = 0;
	    return (OK);
	}
    }

    /* with tabs or control characters, we have to do more */
    if (c == '\t')
    {
	n = (TABSIZE - (curx % TABSIZE));
	if ((curx + n) >= win->_maxx)
	    n = win->_maxx - curx;
	c = ' ';
    }
    else
    {
	if (iscntrl((int) c))
	{
	    if (curx >= win->_maxx-1)
		return (ERR);
	    n = 2;
	}
	else
	    n = 1;
    }

    /* shift right */
    endx = curx + n;
    x = win->_maxx - 1;
    wcp = win->_y[cury] + curx;
    if((rv = _mbinsshift(win,n)) == ERR)
	goto done;

    /* insert new control character */
    if (c < ' ' || c == _CTRL('?'))
    {
	*wcp++ = '^' | win->_attrs | a;
	*wcp = _UNCTRL(c) | win->_attrs | a;
    }
    else
    {
	/* normal characters */
	c = _WCHAR(win, c) | a;
	for ( ; n > 0; --n)
	    *wcp++ = c;
    }

done:
    if (curx < win->_firstch[cury])
	win->_firstch[cury] = (short) curx;
    win->_lastch[cury] = (short) (win->_maxx-1);

    win->_flags |= _WINCHANGED;

    if (win->_sync)
	wsyncup(win);

    return((rv == OK && win->_immed) ? wrefresh(win) : rv);
}
