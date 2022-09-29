/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/wsetscrreg.c	1.3"
#include	"curses_inc.h"

/*
 *	Change scrolling region. Since we depend on the values
 *	of tmarg and bmarg in various ways, this can no longer
 *	be a macro.
 */

int
wsetscrreg(WINDOW *win, int topy, int boty)
{
    if (topy < 0 || topy >= win->_maxy || boty < 0 || boty >= win->_maxy)
	return (ERR);

    win->_tmarg = (short) topy;
    win->_bmarg = (short) boty;
    return (OK);
}
