/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/wborder.c	1.6"
#include	"curses_inc.h"

/*
 *	Draw a box around a window.
 *
 *	ls : the character and attributes used for the left side.
 *	rs : right side.
 *	ts : top side.
 *	bs : bottom side.
 */

#define	_LEFTSIDE	variables[0]
#define	_RIGHTSIDE	variables[1]
#define	_TOPSIDE	variables[2]
#define	_BOTTOMSIDE	variables[3]
#define	_TOPLEFT	variables[4]
#define	_TOPRIGHT	variables[5]
#define	_BOTTOMLEFT	variables[6]
#define	_BOTTOMRIGHT	variables[7]

static	char	acs_values[] =
		{
		    'x', /* ACS_VLINE */
		    'x', /* ACS_VLINE */
		    'q', /* ACS_HLINE */
		    'q', /* ACS_HLINE */
		    'l', /* ACS_ULCORNER */
		    'k', /* ACS_URCORNER */
		    'm', /* ACS_LLCORNER */
		    'j' /* ACS_LRCORNER */
		};

int
wborder(register WINDOW *win, chtype ls, chtype rs, chtype ts,
	chtype bs, chtype tl, chtype tr, chtype bl, chtype br)
{
    register	int	i, endy = win->_maxy - 1, endx = win->_maxx - 2;
    register	chtype	**_y = win->_y;	/* register version */
    chtype		*line_ptr, variables[8];
    register	int	x, sx, xend ;
    chtype		wc;

    _LEFTSIDE = ls;
    _RIGHTSIDE = rs;
    _TOPSIDE = ts;
    _BOTTOMSIDE = bs;
    _TOPLEFT = tl;
    _TOPRIGHT = tr;
    _BOTTOMLEFT = bl;
    _BOTTOMRIGHT = br;

    for (i = 0; i < 8; i++)
    {
	if (_CHAR(variables[i]) == 0
				|| variables[i] & 0xFF00)
	    variables[i] = acs_map[acs_values[i]];
	if(ISCBIT(variables[i]))
		variables[i] = (RBYTE(variables[i])<<8) | LBYTE(variables[i]);
	variables[i] &= ~CBIT;
	variables[i] = _WCHAR(win, variables[i]) | _ATTR(variables[i]);
    }

    /* do top and bottom edges and corners */
    xend = win->_maxx-1;
    x = 0;
    for(; x <= xend; ++x)
	if(!ISCBIT(_y[0][x]))
		break;
    for(; xend >= x; --xend)
	if(!ISCBIT(_y[0][endx]))
	{
	register int	m;
	wc = RBYTE(_y[0][xend]);
	if((m = xend + _scrwidth[TYPE(wc)]) > win->_maxx)
		xend -= 1;
	else
		xend = m - 1;
	endx = xend - 1;
	break;
	}
    sx = x == 0 ? 1 : x ;
    memSset((line_ptr = &_y[0][sx]), _TOPSIDE, endx);
    if( x == 0 )
	*(--line_ptr) = _TOPLEFT;
    if(endx == win->_maxx-2)
    	line_ptr[++endx] = _TOPRIGHT;

    xend = win->_maxx-1;
    x = 0;
    for(; x <= xend; ++x)
	if(!ISCBIT(_y[endy][x]))
		break;
    for(; xend >= x; --xend)
	if(!ISCBIT(_y[endy][xend]))
	{
	register int	m;
	wc = RBYTE(_y[endy][xend]);
	if((m = xend + _scrwidth[TYPE(wc)]) > win->_maxx)
		xend -= 1;
	else
		xend = m - 1;
	endx = xend - 1;
	break;
	}
    sx = x == 0 ? 1 : x ;

    memSset((line_ptr = &_y[endy][sx]), _BOTTOMSIDE, endx);
    if( x == 0)
    	*--line_ptr = _BOTTOMLEFT;
    if( endx == win->_maxx-2)
    	line_ptr[++endx] = _BOTTOMRIGHT;

#ifdef	_VR3_COMPAT_CODE
    if (_y16update)
    {
	(*_y16update)(win, 1, ++endx, 0, 0);
	(*_y16update)(win, 1, endx--, endy, 0);
    }
#endif	/* _VR3_COMPAT_CODE */

    /* left and right edges */
    while (--endy > 0)
    {
	wc = _y[endy][0];
	if(!ISCBIT(wc) && _scrwidth[TYPE(RBYTE(wc))] == 1)
		_y[endy][0] = _LEFTSIDE;
	wc = _y[endy][endx];
	if(!ISCBIT(wc) && _scrwidth[TYPE(RBYTE(wc))] == 1)
		_y[endy][endx] = _RIGHTSIDE;

#ifdef	_VR3_COMPAT_CODE
	if (_y16update)
	{
	    win->_y16[endy][0] = _TO_OCHTYPE(_LEFTSIDE);
	    win->_y16[endy][endx] = _TO_OCHTYPE(_RIGHTSIDE);
	}
#endif	/* _VR3_COMPAT_CODE */
    }
    return (wtouchln((win), 0, (win)->_maxy, TRUE));
}
