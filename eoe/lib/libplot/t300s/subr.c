/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.2 $"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "con.h"

extern void	move(int, int);
void		spew(int);
void		delay(void);

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
	spew (INPLOT);
}

void
outplot(void)
{
	spew(ESC);
	spew(ACK);
	spew(ESC);
	spew(ACK);
	fflush(stdout);
}

void
spew(int ch)
{
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

#define DOUBLE 010
#define ADDR 0100
#define COM 060
#define MAXX 070
#define MAXY 07
extern xnow,ynow;
#define SPACES 7

void
movep(int ix,int iy)
{
	int dx,dy,remx,remy,pts,i;
	int xd,yd;
	int addr,command;
	char c;
	if(xnow == ix && ynow == iy)return;
	inplot();
	dx = ix-xnow;
	dy = iy-ynow;
	command = COM|PENUP|((dx<0)<<1)|(dy<0);
	dx = abval(dx);
	dy = abval(dy);
	xd = dx/(SPACES*2);
	yd = dy/(SPACES*2);
	pts = xd<yd?xd:yd;
	if((i=pts)>0){
		c=command|DOUBLE;
		addr=ADDR;
		if(xd>0)addr|=MAXX;
		if(yd>0)addr|=MAXY;
		spew(c);
		delay();
		while(i--){
			spew(addr);
			delay();
		}
	}
	if(xd!=yd){
		if(xd>pts){
			i=xd-pts;
			addr=ADDR|MAXX;
		}
		else{
			i=yd-pts;
			addr=ADDR|MAXY;
		}
		c=command|DOUBLE;
		spew(c);
		delay();
		while(i--){
			spew(addr);
			delay();
		}
	}
	remx=dx-xd*SPACES*2;
	remy=dy-yd*SPACES*2;
	addr=ADDR;
	i = 0;
	if(remx>7){
		i=1;
		addr|=MAXX;
		remx -= 7;
	}
	if(remy>7){
		i=1;
		addr|=MAXY;
		remy -= 7;
	}
	while(i--){
		spew(command);
		delay();
		spew(addr);
		delay();
	}
	if(remx>0||remy>0){
		spew(command);
		delay();
		spew(ADDR|remx<<3|remy);
		delay();
	}
	xnow=ix;
	ynow=iy;
	outplot();
	return;
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

void
delay(void)
{
	int i;

	for (i=0;i<2;i++) {
		ioctl(OUTF,TCSBRK,1);
	}
	return;
}
