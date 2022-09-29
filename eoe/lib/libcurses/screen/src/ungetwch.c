/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/ungetwch.c	1.2"
#include	"curses_inc.h"

/*
**	Push a process code back into the input stream
*/
int
ungetwch(int code)
/* wchar_t	code; XXX changed to agree with curses.h. May not be right */
	{
	int	i, n;
	char	buf[CSMAX];

	n = _curs_wctomb(buf, code & TRIM);
	for(i = n-1; i >= 0; --i)
		if(ungetch(buf[i]) == ERR)
		{
		/* remove inserted characters */
		for(i = i+1; i < n; ++i)
			tgetch(0);
		return ERR;
		}

	return OK;
	}
