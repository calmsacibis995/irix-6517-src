/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/waddnstr.c	1.4"
#include	"curses_inc.h"

/* This routine adds a string starting at (_cury, _curx) */

int
waddnstr(register WINDOW *win, register char *str, int i)
{
    register	chtype	ch;
    register	int	maxx_1 = win->_maxx - 1, cury = win->_cury,
			curx = win->_curx;
    register	chtype	**_y = win->_y;
    int			savimmed = win->_immed, savsync = win->_sync;
    int			rv = OK;
    int			pflag;


#ifdef	DEBUG
    if (outf)
    {
	if (win == stdscr)
	    fprintf(outf, "waddnstr(stdscr, ");
	else
	    fprintf(outf, "waddnstr(%o, ", win);
	fprintf(outf, "\"%s\")\n", str);
    }
#endif	/* DEBUG */

    /* throw away any current partial character */
    win->_nbyte = -1;
    win->_insmode = FALSE;
    pflag = 1;

    win->_immed = win->_sync = FALSE;

    if (i < 0)
	i = MAXINT;

    while ((ch = *str) && (i-- > 0))
    {
	if(pflag == 1)
		{
		if(_scrmax > 1 && (rv = _mbvalid(win)) == ERR)
			break;
		curx = win->_curx;
		cury = win->_cury;
		}
	if(_mbtrue && ISMBIT(ch))
		{
		register int m,k,ty;
		chtype	     c;
		/* make sure we have the whole character */
		c = RBYTE(ch);
		ty = TYPE(c);
		m = cswidth[ty] - (ty == 0 ? 1 : 0);
		for(k = 1; str[k] != '\0' && k <= m; ++k)
			if (!ISMBIT(str[k]))
				break;
		if(k <= m)
			break;
		if (m > i)
			break;
		for(k = 0; k <= m; ++k, ++str)
		{
			if((rv = _mbaddch(win,A_NORMAL,(chtype) RBYTE(*str))) == ERR)
				goto done;
			if (k > 0)
				i--;
		}
		pflag = 1;
		cury = win->_cury;
		curx = win->_curx;
		continue;
		}

	/* do normal characters while not next to edge */
	if ((ch >= ' ') && (ch != _CTRL('?')) && (curx < maxx_1))
	{
	    if(_scrmax >1 && ISMBIT(_y[cury][curx])
			&& (rv = _mbclrch(win,cury,curx)) == ERR)
		break;
	    if (curx < win->_firstch[cury])
		win->_firstch[cury] = (short) curx;
	    if (curx > win->_lastch[cury])
		win->_lastch[cury] = (short) curx;
	    ch = _WCHAR(win, ch);
	    _y[cury][curx] = ch;
#ifdef	_VR3_COMPAT_CODE
	    if (_y16update)
		win->_y16[cury][curx] = _TO_OCHTYPE (ch);
#endif	/* _VR3_COMPAT_CODE */
	    curx++;
	    pflag = 0;
	}
	else
	{
	    win->_curx = (short) curx;
	    /* found a char that is too tough to handle above */
	    if (waddch(win, ch) == ERR)
	    {
		rv = ERR;
		break;
	    }
	    cury = win->_cury;
	    curx = win->_curx;
	    pflag = 1;
	}
	str++;
	win->_curx = (short) curx;
    }

done :
    win->_curx = (short) curx;
    win->_flags |= _WINCHANGED;
    win->_sync = (char) savsync;
    if (win->_sync)
	wsyncup(win);

    return ((win->_immed = (char) savimmed) ? wrefresh(win) : rv);
}
