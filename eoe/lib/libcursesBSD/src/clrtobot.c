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
static char sccsid[] = "@(#)clrtobot.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"curses.ext"

/*
 *	This routine erases everything on the window.
 *
 */
wclrtobot(win)
reg WINDOW	*win; {

	reg int		y = win->_cury;
	reg char	*sp, *end, *maxx;
	reg int		startx = win->_curx, minx;

	/* Fast path */
	if (_fast_term && win == stdscr) {
	    register int _maxx = win->_maxx, _maxy = win->_maxy;
	    register unsigned n, spaces = 0x20202020;
	    register char **yp = &win->_y[y];

	    /* Clear to bottom (first line partial, rest are full) */
	    for (; y < _maxy; y++, yp++) {
		end = *yp + _maxx;
		sp = *yp + startx;
		startx = 0;

		if (end - sp > 10) {
		    /* go to 32-bit alignment */
		    for (; (long)sp & 0x3; sp++)
			*sp = ' ';

#if (_MIPS_SZPTR == 32)
		    /* Don't bother aligning */
		    n = ((unsigned)end - (unsigned)sp) / 4;
		    while (n >= 5) {
			((int *)sp)[0] = spaces;
			((int *)sp)[1] = spaces;
			((int *)sp)[2] = spaces;
			((int *)sp)[3] = spaces;
			((int *)sp)[4] = spaces;
			sp += 5 * sizeof (int);
			n -= 5;
		    }
		    while (n != 0) {
			*((int *)sp) = spaces;
			sp += sizeof (int);
			n -= 1;
		    }
#else
		    if (end - sp > 12)
			for (; (long)sp & 0xf; sp += sizeof(int))
			    *(int *)sp = 0x20202020;

		    /* it's now 16bytes aligned */
		    for (; (long)sp < ((long)end & ~0xf); sp += sizeof(long)) {
			*(long *) sp = 0x2020202020202020LL;
			sp += sizeof(long);
			*(long *) sp = 0x2020202020202020LL;
		    }

		    for (; (long)sp < ((long)end & ~0x3); sp += sizeof(int))
			*(int *)sp = 0x20202020;
#endif 		    
		}
		for (; sp < end; sp++)
		    *sp = ' ';
	    }
	    return OK;
	}

	for (y = win->_cury; y < win->_maxy; y++) {
		minx = _NOCHANGE;
		end = &win->_y[y][win->_maxx];
		for (sp = &win->_y[y][startx]; sp < end; sp++)
			if (*sp != ' ') {
				maxx = sp;
				if (minx == _NOCHANGE)
					minx = sp - win->_y[y];
				*sp = ' ';
			}
		if (minx != _NOCHANGE)
			touchline(win, y, minx, maxx - &win->_y[y][0]);
		startx = 0;
	}
}
