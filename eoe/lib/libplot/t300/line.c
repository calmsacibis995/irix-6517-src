/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

#include "con.h"
#include "math.h"

/* external declarations */
extern int	xconv(int);
extern int	yconv(int);
extern int	xsc(int);
extern int	ysc(int);
extern void	movep(int, int);
extern void	inplot(void);
extern int	abval(int);
extern void	spew(int);
extern void	outplot(void);
extern float	dist2(int, int, int, int);

/* foward declarations */
void	iline(int, int, int, int);

void
line(int x0, int y0, int x1, int y1)
{
	iline(xconv(xsc(x0)),yconv(ysc(y0)),xconv(xsc(x1)),yconv(ysc(y1)));
	return;
}

void
cont(int x0, int y0)
{
	iline(xnow,ynow,xconv(xsc(x0)),yconv(ysc(y0)));
	return;
}

void
iline(int cx0, int cy0, int cx1, int cy1)
{
	int maxp,tt,j,np;
	char chx,chy;
	float xd,yd;

	movep(cx0,cy0);
	maxp = sqrt(dist2(cx0,cy0,cx1,cy1))/2.;
	xd = cx1-cx0;
	yd = cy1-cy0;
	if(xd >= 0)chx = RIGHT;
	else chx = LEFT;
	if(yd >= 0)chy = UP;
	else chy = DOWN;
	if(maxp==0){
		xd=0;
		yd=0;
	}
	else{
		xd /=maxp;
		yd /=maxp;
	}
	inplot();
	for (tt=0; tt<=maxp; tt++){
		j= cx0+xd*tt-xnow;
		xnow += j;
		j = abval(j);
		while(j-- > 0)spew(chx);
		j = cy0+yd*tt-ynow;
		ynow += j;
		j = abval(j);
		while(j-- > 0)spew(chy);
		spew ('.');
	}
	outplot();
	return;
}
