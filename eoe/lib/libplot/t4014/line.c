/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

extern void	move(int, int);
extern void	cont(int, int);

void
line(int x0, int y0, int x1, int y1)
{
	move(x0,y0);
	cont(x1,y1);
}
