/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/winswch.c	1.2"
#include	"curses_inc.h"

/*
**	Insert to 'win' a process code at (curx,cury).
*/
int
winswch(WINDOW *win, chtype c)
{
	int	i, width;
	char	buf[CSMAX];
	chtype	a;
	wchar_t	code;
#ifdef	_WCHAR16
	a = _ATTR(c);
	code = c&A_CHARTEXT;
#else	/* _WCHAR16 */
	a = 0;
	code = (wchar_t) c;
#endif	/* _WCHAR16 */

	/* translate the process code to character code */
	if ((width = _curs_wctomb(buf, code & TRIM)) < 0)
		return ERR;

	/* now call winsch to do the real work */
	for(i = 0; i < width; ++i)
		if(winsch(win,a|buf[i]) == ERR)
			return ERR;
	return OK;
}
