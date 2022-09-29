/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include "con.h"

extern void	move(int, int);
void		spew(int);

int
abval(int q)
{
	return (q>=0 ? q : -q);
}

int
xconv(int xp)
{
	/* x position input is -2047 to +2047, output must be 0 to PAGSIZ*HORZRES */
	xp += 2048;
	/* the computation is newx = xp*(PAGSIZ*HORZRES)/4096 */
	return (xoffset + xp /xscale);
}

int
yconv(int yp)
{
	/* see description of xconv */
	yp += 2048;
	return (yp / yscale);
}

void
inplot(void)
{
	spew(ESC);
	spew(PLOTIN);
}

void
outplot(void)
{
	spew(ESC);
	spew(PLOTOUT);
	fflush(stdout);
}

void
spew(int ch)
{
	if(ch == UP){
		putc(ESC,stdout);
		ch = DOWN;
	}
	putc(ch, stdout);
}

void
tobotleft(void)
{
	move(-2048,-2048);
}

void
reset(void)
{
	signal(SIGINT,SIG_IGN);
	outplot();
	ioctl(OUTF, TCSETAW, &ITTY);
	exit(0);
}

float
dist2(int x1, int y1, int x2, int y2)
{
	float t,v;
	t = x2-x1;
	v = y1-y2;
	return (t*t+v*v);
}

void
swap(int *pa, int *pb)
{
	int t;
	t = *pa;
	*pa = *pb;
	*pb = t;
}

void
movep(int xg, int yg)
{
	int i,ch;
	if((xg == xnow) && (yg == ynow))return;
	/* if we need to go to left margin, just CR */
	if (xg < xnow/2)
	{
		spew(CR);
		xnow = 0;
	}
	i = (xg-xnow)/HORZRES;
	if(xnow < xg)ch = RIGHT;
	else ch = LEFT;
	xnow += i*HORZRES;
	i = abval(i);
	while(i--)spew(ch);
	i = abval(xg-xnow);
	inplot();
	while(i--) spew(ch);
	outplot();
	i=(yg-ynow)/VERTRES;
	if(ynow < yg)ch = UP;
	else ch = DOWN;
	ynow += i*VERTRES;
	i = abval(i);
	while(i--)spew(ch);
	i=abval(yg-ynow);
	inplot();
	while(i--)spew(ch);
	outplot();
	xnow = xg; ynow = yg;
}

int
xsc(int xi)
{
	int xa;
	xa = (xi - obotx) * scalex + botx;
	return(xa);
}

int
ysc(int yi)
{
	int ya;
	ya = (yi - oboty) *scaley +boty;
	return(ya);
}
