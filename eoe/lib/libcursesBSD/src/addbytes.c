/*
 * Copyright (c) 1987 Regents of the University of California.
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
static char sccsid[] = "@(#)addbytes.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

#include <ctype.h>
# include	"curses.ext"
# include	<stdlib.h>

bool	_fast_term;

/*
 *	This routine adds the character to the current position
 *
 */
waddbytes(win, bytes, count)
reg WINDOW	*win;
reg char	*bytes;
reg int		count;
{
#define	SYNCH_OUT()	{win->_cury = y; win->_curx = x;}
#define	SYNCH_IN()	{y = win->_cury; x = win->_curx;}
/* Warning: this macro is unsafe because it references c twice */
#define isprint_sp(c)	( isprint(c) || isspace(c) )

	reg int		x, y;

	reg char *wy;
	reg short *wf;
	reg short *wl;
	reg short ws;

	ws = win->_flags & _STANDOUT;

	SYNCH_IN();
# ifdef FULLDEBUG
	fprintf(outf, "ADDBYTES('%c') at (%d, %d)\n", c, y, x);
# endif
	while (count--) {
	    register int c;
	    static char blanks[] = "        ";

	    c = *bytes++;
	    if (isprint_sp(c)) {

		    wy = win->_y[y];
		    wf = win->_firstch+y;
		    wl = win->_lastch+y;

		    if (_fast_term) {
			*wf = win->_ch_off;
			*wl = win->_ch_off + win->_maxx - 1;
		    }
		    
# ifdef FULLDEBUG
		    fprintf(outf, "ADDBYTES: 1: y = %d, x = %d, firstch = %d, lastch = %d\n", y, x, win->_firstch[y], win->_lastch[y]);
# endif
		    do {
			    c |= ws;
# ifdef	FULLDEBUG
			    fprintf(outf, "ADDBYTES(%0.2o, %d, %d)\n", win, y, x);
# endif
			    if (_fast_term) {
				wy[x] = c;
			    } else if (wy[x] != c) {
				    reg int	newx;
				    newx = x + win->_ch_off;
				    if (*wf == _NOCHANGE) {
					    *wf = *wl = newx;
				    } else if (newx > *wl)
					    *wl = newx;
				    else if (newx < *wf)
					    *wf = newx;
# ifdef FULLDEBUG
				    fprintf(outf, "ADDBYTES: change gives f/l: %d/%d [%d/%d]\n",
					    win->_firstch[y], win->_lastch[y],
					    win->_firstch[y] - win->_ch_off,
					    win->_lastch[y] - win->_ch_off);
# endif
				    wy[x] = c;
			    }
			    x += 1;
			    if (x >= win->_maxx) {
				    x = 0;
	    newline:
				    if (++y >= win->_maxy)
					    if (win->_scroll) {
						    SYNCH_OUT();
						    scroll(win);
						    SYNCH_IN();
						    --y;
					    }
					    else
						    return ERR;
				    wy = win->_y[y];
				    wf = win->_firstch+y;
				    wl = win->_lastch+y;

				    if (_fast_term) {
					*wf = win->_ch_off;
					*wl = win->_ch_off + win->_maxx - 1;
				    }
			    }

			    /* Usage of the unsafe macro is ok here */
			    if (!count || !(isprint_sp(c=*bytes)))
				break;

			    c = *bytes++;
			    count -= 1;
		    } while (1);
# ifdef FULLDEBUG
		    fprintf(outf, "ADDBYTES: 2: y = %d, x = %d, firstch = %d, lastch = %d\n", y, x, win->_firstch[y], win->_lastch[y]);
# endif
	    } else
		    switch (c) {
		      case '\t':
			    SYNCH_IN();
			    if (waddbytes(win, blanks, 8-(x%8)) == ERR) {
				return ERR;
			    }
			    SYNCH_OUT();
			    break;

		      case '\n':
			    SYNCH_OUT();
			    wclrtoeol(win);
			    SYNCH_IN();
			    if (!NONL)
				    x = 0;
			    goto newline;
		      case '\r':
			    x = 0;
			    break;
		      case '\b':
			    if (--x < 0)
				    x = 0;
			    break;
		      default:
				abort();
		    }
    }
    SYNCH_OUT();
    return OK;
}
