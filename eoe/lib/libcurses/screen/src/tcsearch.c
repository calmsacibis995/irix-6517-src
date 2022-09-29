/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include "curses_inc.h"

#ident	"@(#)curses:screen/tcsearch.c	1.3"
int
_tcsearch(char *cap, short offsets[], char *names[], int size, int n)
{
    register	int	l = 0, u = size - 1;
    int		m, cmp;

    while (l <= u)
    {
	m = (l + u) / 2;
	cmp = ((n == 0) ? strcmp(cap, names[offsets[m]]) :
			  strncmp(cap, names[offsets[m]], (size_t) n));

	if (cmp < 0)
	    u = m - 1;
	else
	    if (cmp > 0)
		l = m + 1;
	    else
		return (offsets[m]);
    }
    return (-1);
}
