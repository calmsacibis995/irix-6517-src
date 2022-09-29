/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/def_prog.c	1.3"
#include <standards.h>
#include "curses_inc.h"

int
def_prog_mode(void)
{
    /* ioctl errors are ignored so pipes work */
#ifdef SYSV
# if _ABIAPI
    (void) ioctl(cur_term -> Filedes, TCGETS, &(PROGTTY));
# else
    (void) ioctl(cur_term -> Filedes, TCGETA, &(PROGTTY));
# endif
#else
    (void) ioctl(cur_term -> Filedes, TIOCGETP, &(PROGTTY));
#endif
    return (OK);
}
