/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:color_cont.c	1.4"

#include "curses_inc.h"

int
color_content(short color, short *r, short *g, short *b)
{
    register _Color *ctp;

    if (color < 0 || color > COLORS || !can_change ||
	(ctp = cur_term->_color_tbl) == (_Color *) NULL)
        return (ERR);

    ctp += color;
    *r = ctp->r;
    *g = ctp->g;
    *b = ctp->b;
    return (OK);
}
