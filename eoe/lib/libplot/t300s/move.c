/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

/* external declarations */
extern int      xconv(int);
extern int      yconv(int);
extern int      xsc(int);
extern int      ysc(int);
extern void     movep(int, int);


void
move(int xi, int yi)
{
		movep(xconv(xsc(xi)),yconv(ysc(yi)));
		return;
}
