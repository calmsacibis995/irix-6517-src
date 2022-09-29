/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/winsnwstr.c	1.1"
#include	"curses_inc.h"

/*
**	Insert to 'win' at most n 'characters' of code starting at (cury,curx)
*/
int
winsnwstr(WINDOW *win, wchar_t *code, int n)
{
	register char	*sp;

	/* translate the process code to character code */
	if((sp = _strcode2byte(code,NULL,n)) == NULL)
		return ERR;

	/* now call winsnstr to do the real work */
	return winsnstr(win,sp,-1);
}
