/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/makenew.c	1.9"
#include	"curses_inc.h"

/* This routine sets up a window buffer and returns a pointer to it. */

WINDOW	*
_makenew(register int nlines, register int ncols, int begy, int begx)
{
    /* order the register allocations against highest usage */
    register	WINDOW	*win;

#ifdef	DEBUG
    if (outf)
	fprintf(outf, "MAKENEW(%d, %d, %d, %d)\n", nlines, ncols, begy, begx);
#endif	/* DEBUG */

    if ((win = (WINDOW *) malloc(sizeof (WINDOW))) == NULL)
	goto out_no_win;
    if ((win->_y = (chtype **) malloc((size_t)nlines * sizeof (chtype *))) == NULL)
	goto out_win;
#ifdef	_VR3_COMPAT_CODE
    if ((_y16update) &&
	((win->_y16 = (_ochtype **) calloc(1, (size_t)nlines * sizeof (_ochtype *))) == NULL))
    {
	goto out_y16;
    }
#endif	/* _VR3_COMPAT_CODE */
    if ((win->_firstch = (short *) malloc(2 * (size_t)nlines * sizeof(short))) == NULL)
    {
#ifdef	_VR3_COMPAT_CODE
	if ((_y16update) && (win->_y16 != NULL))
	     free((char *) win->_y16);
out_y16:
#endif	/* _VR3_COMPAT_CODE */
	free((char *) win->_y);
out_win:
	free((char *) win);
out_no_win:
	curs_errno = CURS_BAD_MALLOC;
#ifdef	DEBUG
	strcpy(curs_parm_err, "_makenew");
#endif	/* DEBUG */
	return ((WINDOW *) NULL);
    }
    else
	win->_lastch = win->_firstch + nlines;

    win->_cury = win->_curx = 0;
    win->_maxy = (short) nlines;
    win->_maxx = (short) ncols;
    win->_begy = (short) begy;
    win->_begx = (short) begx;
    win->_clear = (char) ((((begy + SP->Yabove + begx) == 0) &&
	(nlines >= (LINES + SP->Yabove)) && (ncols >= COLS)));
    win->_leave = (char) (win->_scroll = win->_use_idl = win->_use_keypad =
	win->_notimeout = win->_immed = win->_sync = FALSE);
    win->_use_idc = TRUE;
    win->_ndescs = win->_tmarg = 0;
    win->_bmarg = (short) (nlines - 1);
    win->_bkgd = _BLNKCHAR;
    win->_delay = win->_parx = win->_pary = -1;
    win->_attrs = A_NORMAL;
    win->_flags = _WINCHANGED;
    win->_parent = win->_padwin = (WINDOW *) NULL;
    (void) memset((char *) win->_firstch, 0, ((size_t)nlines * sizeof(short)));
    {
	register	short	*lastch = win->_lastch,
				*elastch = lastch + nlines;

	ncols--;
	while (lastch < elastch)
	    *lastch++ = (short) ncols;
    }

    win->_insmode = FALSE;
    win->_index = 0;
    win->_nbyte = -1;

#ifdef	DEBUG
    if(outf)
    {
	fprintf(outf, "MAKENEW: win->_clear = %d\n", win->_clear);
	fprintf(outf, "MAKENEW: win->_flags = %0.2o\n", win->_flags);
	fprintf(outf, "MAKENEW: win->_maxy = %d\n", win->_maxy);
	fprintf(outf, "MAKENEW: win->_maxx = %d\n", win->_maxx);
	fprintf(outf, "MAKENEW: win->_begy = %d\n", win->_begy);
	fprintf(outf, "MAKENEW: win->_begx = %d\n", win->_begx);
    }
#endif	/* DEBUG */
    return (win);
}
