/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/subwin.c	1.5"
#include	"curses_inc.h"

WINDOW	*
subwin(WINDOW *win, int l, int nc, int by, int bx)
{
    return (derwin(win,l,nc,by - win->_begy,bx - win->_begx));
}
