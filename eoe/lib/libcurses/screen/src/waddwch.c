/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/waddwch.c	1.2"
#include	"curses_inc.h"


/*
**	Add to 'win' a character at (curx,cury).
*/
int
waddwch(WINDOW *win, chtype c)
{
	int	width;
	char	buf[CSMAX];
	chtype	a;
	wchar_t	code;
	char	*p;

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

	/* now call waddch to do the real work */
	p = buf;
	while(width--)
		if (waddch(win, a|(unsigned int)(0xFF & *p++)) == ERR)
			return ERR;
	return OK;
}
