/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/pnoutref.c	1.11"
#include	"curses_inc.h"

/* wnoutrefresh for pads. */

int
pnoutrefresh(WINDOW *pad, int pby, int pbx,
	     int sby, int sbx, int sey, int sex)
{
    extern	int	wnoutrefresh();

    return (_prefresh(wnoutrefresh, pad, pby, pbx, sby, sbx, sey, sex));
}
