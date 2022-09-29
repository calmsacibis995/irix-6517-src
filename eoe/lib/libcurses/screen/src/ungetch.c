/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/ungetch.c	1.5"
#include "curses_inc.h"

/* Place a char onto the beginning of the input queue. */

int
ungetch(int ch)
{
    register	int	i = cur_term->_chars_on_queue, j = i - 1;
    register	chtype	*inputQ = cur_term->_input_queue;

    /* Place the character at the beg of the Q */

    register chtype	r;

    if(ISCBIT(ch))
	{
	r = (chtype) RBYTE(ch);
	ch = LBYTE(ch);
	/* do the right half first to maintain the byte order */
	if(r != MBIT && ungetch((int) r) == ERR)
		return ERR;
	}

    while (i > 0)
	inputQ[i--] = inputQ[j--];
    cur_term->_ungotten++;
    inputQ[0] = (chtype) (-ch);
    cur_term->_chars_on_queue++;
    return(OK);
}






