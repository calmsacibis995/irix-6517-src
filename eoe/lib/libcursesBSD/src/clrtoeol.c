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
static char sccsid[] = "@(#)clrtoeol.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"curses.ext"

/*
 *	This routine clears up to the end of line
 *
 */
wclrtoeol(win)
reg WINDOW	*win; {

	reg char	*sp, *end;
	reg int		y, x;
	reg char	*maxx;
	reg int		minx;

	y = win->_cury;
	x = win->_curx;
	end = &win->_y[y][win->_maxx];

	if (_fast_term && win == stdscr) {
	    register unsigned n, n4, spaces = 0x20202020;

	    sp = win->_y[y] + x;
	    if (end - sp > 10) {
		/* go to 32-bit alignment */
		for (; (long)sp & 0x3; sp++)
		    *sp = ' ';

#if (_MIPS_SZPTR == 32)
		/* find # of words to clr, and blk clr 4 at a time */
                n = ((unsigned)end - (unsigned)sp) / 4;
		for (n4 = n / 4; n4 != 0; --n4) {
                    ((int *)sp)[0] = spaces;
                    ((int *)sp)[1] = spaces;
                    ((int *)sp)[2] = spaces;
                    ((int *)sp)[3] = spaces;
                    sp += 4 * sizeof (int);
                }

		/* pick up 0-3 words on end */
		for (n &= 0x3; n != 0; --n) {
                    ((int *)sp)[0] = spaces;
                    sp += sizeof (int);
                }
#else
		/* 64bits */
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
	    return OK;
	}

	minx = _NOCHANGE;
	maxx = &win->_y[y][x];
	for (sp = maxx; sp < end; sp++)
		if (*sp != ' ') {
			maxx = sp;
			if (minx == _NOCHANGE)
				minx = sp - win->_y[y];
			*sp = ' ';
		}
	/*
	 * update firstch and lastch for the line
	 */
	touchline(win, y, win->_curx, win->_maxx - 1);
# ifdef DEBUG
	fprintf(outf, "CLRTOEOL: minx = %d, maxx = %d, firstch = %d, lastch = %d\n", minx, maxx - win->_y[y], win->_firstch[y], win->_lastch[y]);
# endif
}
