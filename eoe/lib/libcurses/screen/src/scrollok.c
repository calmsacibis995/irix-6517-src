/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/scrollok.c	1.8"
#include	"curses_inc.h"

int
scrollok(WINDOW *win, bool bf)
{
    win->_scroll = (char) ((bf) ? TRUE : FALSE);
    return (OK);
}
