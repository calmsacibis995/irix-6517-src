/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/waddwchnstr.c	1.4"
#include	"curses_inc.h"

/*
 * Add ncols worth of data to win, using string as input.
 * Return the number of chtypes copied.
 * Note: chtype contains 32/16 bit process code.
 */
int
waddwchnstr(register WINDOW *win, chtype *string, int ncols)
{
    int		my_x = win->_curx;
    int		my_y = win->_cury;
    __psint_t	my_maxx;
    int		counter;
    chtype	*ptr = &(win->_y[my_y][my_x]);
    chtype	*sptr = ptr;
    char	mbbuf[CSMAX+1];
    int		mp, s, scrw;
    chtype	rawc;
#ifdef	_WCHAR16
    chtype	attr;
#endif	/*_WCHAR16*/
    int		my_x1 = my_x;


    while (ISCBIT(*ptr)) {
	ptr--;
	my_x1--;
    }
    while (ptr < sptr)
	*ptr++ = win->_bkgd;

    if (ncols == -1)
	ncols = MAXINT;

    counter = win->_maxx - my_x;
    while ((ncols > 0) && (*string) && (counter > 0))
    {
#ifdef	_WCHAR16
	attr = _ATTR(*string);
	rawc = _CHAR(*string);
#else	/*_WCHAR16*/
	rawc = *string;
#endif	/*_WCHAR16*/

	/* conver wchar_t to mbuti byte string */
	for (mp = 0; (size_t) mp < sizeof(mbbuf); mp++)
	    mbbuf[mp] = '\0';
	if (_curs_wctomb(mbbuf, (wchar_t) rawc) <= 0)
	    goto out;

	/* if there are no cols on screen, end */
	if ((scrw = wcscrw(rawc)) > counter)
	    goto out;

	/* store multi-byte string into chtype */
	for (s = 0, mp = 0; s < scrw; s++, mp += 2)
	{
	    *ptr = (chtype) (0xFFFF & ((0xFF & mbbuf[mp]) | (mbbuf[mp+1] << 8)));
	    SETMBIT(*ptr);
	    if (mp > 0)
		SETCBIT(*ptr);
	    else
		CLRCBIT(*ptr);
#ifdef	_WCHAR16
	    *ptr |= attr;
#endif	/*_WCHAR16*/
	    ptr++;
	}
	ncols--;
	string++;
	counter -= scrw;
    }
out :

    while (ISCBIT(*ptr))
	*ptr++ = win->_bkgd;

    my_maxx = ptr - sptr + my_x;

    if (my_x1 < win->_firstch[my_y])
	win->_firstch[my_y] = (short) my_x1;

    if (my_maxx > win->_lastch[my_y])
	win->_lastch[my_y] = (short) my_maxx;

    win->_flags |= _WINCHANGED;

    /* sync with ancestor structures */
    if (win->_sync)
	wsyncup(win);

    return (win->_immed ? wrefresh(win) : OK);
}
