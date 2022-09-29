/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/outch.c	1.3"

#include	"curses_inc.h"

int	outchcount;

/* Write out one character to the tty and increment outchcount. */

int
_outch(int c)
{
	return(_outwch((chtype)c));
}

int
_outwch(chtype c)
{
    register chtype	o;

#ifdef	DEBUG
#ifndef	LONGDEBUG
    if (outf)
	if (c < ' ' || c == 0177)
	    fprintf(outf, "^%c", c^0100);
	else
	    fprintf(outf, "%c", c&0177);
#else	/* LONGDEBUG */
	if (outf)
	    fprintf(outf, "_outch: char '%s' term %x file %x=%d\n",
		unctrl(c&0177), SP, cur_term->Filedes, fileno(SP->term_file));
#endif	/* LONGDEBUG */
#endif	/* DEBUG */

    outchcount++;

    /* ASCII code */
    if(!ISMBIT(c))
	putc((int)c, SP->term_file);
    /* C1 code */
    else if(!(c & 0x60) && !ISCBIT(c))
	putc((int)c, SP->term_file);
    /* international code */
    else if((o = RBYTE(c)) != MBIT)
	{
	putc((int)o, SP->term_file);
	if(_csmax > 1 && ((o = LBYTE(c)|MBIT) != MBIT))
		{
		SETMBIT(o);
		putc((int)o, SP->term_file);
		}
	}
    return(0);
}
