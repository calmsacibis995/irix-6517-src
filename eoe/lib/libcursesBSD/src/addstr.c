/*
 * Copyright (c) 1981 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)addstr.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"curses.ext"
# include	<string.h>

/*
 *	This routine adds a string starting at (_cury,_curx)
 *
 */
#if 0
waddstr(win,str)
reg WINDOW	*win; 
reg char	*str;
{
# ifdef DEBUG
	fprintf(outf, "WADDSTR(\"%s\")\n", str);
# endif
	return waddbytes(win, str, strlen(str));
}
#endif 

addstr (reg char *str)
{
    reg WINDOW *stdscrp = stdscr;
    reg int count, i, ws, x;

    if (_fast_term) {
	reg char *q, *p;

        /* Setup registers for fast fill */
	ws = stdscrp->_flags & _STANDOUT;
	x = stdscrp->_curx;
	i = stdscrp->_maxx - x;
	p = stdscrp->_y[stdscrp->_cury] + x;

	/* Search input string for characters < 020 or > 0177 */
	for (q = str; *q; q++)
	    if ((*q < ' ') | (*q > '~'))	/* cheat, another % */
		goto slow;
	count = q - str;

	/* If we have enough space on the end of the line, */
	if (count < i) {
	    q = str;
	    while (*q != NULL)
		*p++ = *q++ | ws;
	    stdscrp->_curx = x + count;
	    return OK;
	}
    }
slow:
    return waddbytes (stdscrp, str, strlen(str));
}
