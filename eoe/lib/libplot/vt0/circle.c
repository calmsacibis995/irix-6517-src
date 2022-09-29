/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.3 $"

#include <unistd.h>

extern int vti;

/* ARGSUSED */
void
circle(int x, int y, int r)
{
	char c;
	c = 5;
	write(vti,&c,1);
	write(vti,&x,6);
}
