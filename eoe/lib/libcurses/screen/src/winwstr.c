/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/winwstr.c	1.4"
#include	"curses_inc.h"

int
winwstr(register WINDOW *win, register wchar_t *wstr)
{
    register	int	counter = 0;
    int			cy = win->_cury;
    register	chtype	*ptr = &(win->_y[cy][win->_curx]),
			*pmax = &(win->_y[cy][win->_maxx]);
    chtype		*p1st = &(win->_y[cy][0]);
    wchar_t		wc;
    int			ew, sw, s;
    char		*cp, cbuf[CSMAX+1];

    while (ISCBIT(*ptr) && (p1st < ptr))
	ptr--;

    while (ptr < pmax)
    {
	wc = (wchar_t) RBYTE(*ptr);
	sw = mbscrw(wc);
	ew = mbeucw(wc);

	cp = cbuf;
	for (s = 0; s < sw; s++, ptr++)
	{
	    if ((wc = (wchar_t) RBYTE(*ptr)) == MBIT)
		continue;
	    *cp++ = (char) wc;
	    if ((wc = (wchar_t) LBYTE(*ptr) | MBIT) == MBIT)
		continue;
	    *cp++ = (char) wc;
	}
	*cp = '\0';

	if (_curs_mbtowc(&wc, cbuf, CSMAX) <= 0)
	    break;
	
	*wstr++ = wc;
    }

    *wstr = (wchar_t)0;

    return (counter);
}
