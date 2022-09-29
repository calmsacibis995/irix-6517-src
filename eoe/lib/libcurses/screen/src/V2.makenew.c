/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/V2.makenew.c	1.5"

#include "curses_inc.h"

#ifdef _VR2_COMPAT_CODE
extern WINDOW *_makenew();

WINDOW *
makenew(int num_lines, int num_cols, int begy, int begx)
{
	return _makenew(num_lines, num_cols, begy, begx);
}
#endif
