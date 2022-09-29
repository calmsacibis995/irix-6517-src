/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/keypad.c	1.8"

#include	"curses_inc.h"

/* TRUE => special keys should be passed as a single value by getch. */

int
keypad(WINDOW *win, bool bf)
{
    /*
     * Since _use_keypad is a bit and not a bool, we must check
     * bf, in case someone uses an odd number instead of 1 for TRUE
     */

    win->_use_keypad = (unsigned) ((bf) ? TRUE : FALSE);
    if (bf && (!SP->kp_state))
    {
	tputs(keypad_xmit, 1, _outch);
	(void) fflush(SP->term_file);
	SP->kp_state = TRUE;
	return (setkeymap());
    }
    return (OK);
}
