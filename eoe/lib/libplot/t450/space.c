/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

# include "con.h"

void
space(int x0, int y0, int x1, int y1)
{
	botx = -2047.;
	boty = -2047.;
	obotx = x0;
	oboty = y0;
	scalex = 4096./(x1-x0);
	scaley = 4096./(y1-y0);
}
