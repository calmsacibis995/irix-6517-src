/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/reset_sh.c	1.5"
#include	<standards.h>
#include	"curses_inc.h"

int
reset_shell_mode(void)
{
#ifdef	DIOCSETT
    /*
     * Restore any virtual terminal setting.  This must be done
     * before the TIOCSETN because DIOCSETT will clobber flags like xtabs.
     */
    cur_term -> old.st_flgs |= TM_SET;
    (void) ioctl(cur_term->Filedes, DIOCSETT, &cur_term -> old);
#endif	/* DIOCSETT */
    if (cur_term == 0) {
	return (ERR);
    }	
    if (_BR(SHELLTTY))
    {
	(void) ioctl(cur_term -> Filedes,
#ifdef	SYSV
# if _ABIAPI
	    TCSETSW,
# else
	    TCSETAW,
# endif
#else	/* SYSV */
	    TIOCSETN,
#endif	/* SYSV */
		    &SHELLTTY);
#ifdef	LTILDE
	if (cur_term -> newlmode != cur_term -> oldlmode)
	    (void) ioctl(cur_term -> Filedes, TIOCLSET, &cur_term -> oldlmode);
#endif	/* LTILDE */
    }
    return (OK);
}
