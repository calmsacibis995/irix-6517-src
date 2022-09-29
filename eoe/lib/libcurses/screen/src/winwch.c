/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/winwch.c	1.2"
#include	"curses_inc.h"

/*
**	Get a process code at (curx,cury).
*/
chtype
winwch(WINDOW *win)
{
	wchar_t	wchar;
	int	length;
#ifdef	_WCHAR16
	chtype	a;

	a = _ATTR(win->_y[win->_cury][win->_curx]);
#endif	/* _WCHAR16 */

	length = _curs_mbtowc(&wchar,wmbinch(win,win->_cury,win->_curx),
							sizeof(wchar_t));
#ifdef	_WCHAR16
	return((chtype) (a | wchar));
#else	/* _WCHAR16 */
	return ((chtype) wchar);
#endif	/* _WCHAR16 */
}

