/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/is_linetou.c	1.3"
#include	"curses_inc.h"

int
is_linetouched(WINDOW *win, int line)
{
    if (line < 0 || line >= win->_maxy)
        return (ERR);
    if (win->_firstch[line] == _INFINITY)
	return (FALSE);
    else
	return (TRUE);
}
