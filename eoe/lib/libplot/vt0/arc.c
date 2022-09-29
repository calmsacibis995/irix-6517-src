/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

#include <unistd.h>

extern int vti;

void
arc(int xi, int yi, int x0, int y0, int x1, int y1)
{
	char c;
	c = 6;
	write(vti,&c,1);
	write(vti,&xi,12);
}
