/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  Mini GL
 *  $Revision: 1.7 $
 */

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/lg1hw.h>
#include <ide_msg.h>
#include <libsc.h>

#define REX_M_NORM 256.0
#define REX_MCOLOR_NORM 4096.0

int f_dither = 0;
int f_logicop = REX_LO_SRC;
int f_rgb = 0;
int f_planes = PIXELPLANES;
int f_xywin = 0x08000800;

extern Rexchip *REX;

#define RexWait while ((REX->p1.set.configmode) & CHIPBUSY)

int cmode(int argc, char **argv)
{
    if (argc > 1) {
	printf("usage: %s\n", argv[0]);
	return -1;
    }
    f_rgb = 0;
    return 0;
}    
    

int rgbmode(int argc, char **argv)
{
    if (argc > 1) {
	printf("usage: %s\n", argv[0]);
	return -1;
    }
    f_rgb = RGBMODECMD;
    return 0;
}    
    

int logicop(int argc, char **argv)
{
    if (argc < 2) {
	printf("usage: %s mask\n", argv[0]);
	return -1;
    }
    atohex(argv[1], &f_logicop);
    f_logicop <<= 28;
    return 0;
}    
    

int ditherbit(int argc, char **argv)
{
    int flag;

    if (argc < 2) {
	printf("usage: %s 0|1\n", argv[0]);
	return -1;
    }
    atob(argv[1], &flag);
    if (flag == 0)
	f_dither = 0;
    else
	f_dither = DITHER;
    return 0;
}    
    

int writemask(int argc, char **argv)
{
    int mask;

    if (argc < 2) {
	printf("usage: %s mask\n", argv[0]);
	return -1;
    }
    atohex(argv[1], &mask);
    REX->set.rwmask = mask;
    return 0;
}


int _color(int c)
{
    REX->set.colorredi = c;
}

int setcolor(int argc, char **argv)
{
    int color;

    if (argc < 2) {
	printf("usage: %s index\n", argv[0]);
	return -1;
    }
    atohex(argv[1], &color);
    _color(color);
    return 0;
}


int _rgbcolor(int r, int g, int b)
{
    REX->set.colorredi = r;
    REX->set.colorgreeni = g;
    REX->set.colorbluei = b;
}

int rgbcolor(int argc, char **argv)
{
    int r, g, b;

    if (argc < 4) {
	printf("usage: %s r g b\n", argv[0]);
	return -1;
    }
    atohex(argv[1], &r);
    atohex(argv[2], &g);
    atohex(argv[3], &b);
    _rgbcolor(r, g, b);
    return 0;
}


int _planes(int p)
{
    REX->p1.set.aux2 = p;
    REX->p1.set.togglectxt = 0;
    return 0;
}


int planes(int argc, char **argv)
{
    if (argc < 2) {
	printf("usage: %s code\n", argv[0]);
	return -1;
    }
    atob(argv[1], &f_planes);
    f_planes <<= 29;
    _planes(f_planes);
}


int xywinorg(int argc, char **argv)
{
    int x, y;

    if (argc < 3) {
	printf("usage: %s x y\n", argv[0]);
	return -1;
    }
    atob(argv[1], &x);
    atob(argv[2], &y);
    f_xywin = (x << 16) | y;
    REX->p1.set.xywin = f_xywin;
    return 0;
}


/*
 * point() - write a single pixel to VRAM
 *
 *      This function takes x, y coordinates and writes a single pixel into
 *      the framebuffer at that location.
 */
int point(int argc, char **argv)
{
    int x, y;
    unsigned int cmd;

    if (argc < 3) {
	printf("usage: %s x y\n", argv[0]);
	return -1;
    }
    atob(argv[1], &x);
    atob(argv[2], &y);
    cmd = f_logicop | f_rgb | f_dither | REX_DRAW;
    REX->set.xstarti = x;
    REX->set.ystarti = y;
    REX->set.command = cmd;
    x = REX->go.command;
    RexWait;
    return 0;
}


int _line(int x0, int y0, int x1, int y1)
{
    float slope;
    int dx, dy;
    int adx, ady;
    int cmd;

    REX->set.xstarti = x0;
    REX->set.ystarti = y0;
    dx = x1 - x0;
    dy = y1 - y0;
    if (dx < 0)
	adx = -dx;
    else
	adx = dx;
    if (dy < 0)
	ady = -dy;
    else
	ady = dy;
    cmd = f_logicop | f_rgb | f_dither | INITFRAC | REX_DRAW;
    if (adx > ady) {
	cmd |= XMAJOR | STOPONX;
	REX->set.xendi = x1;
	slope = dy / (float)adx;
    }
    else {
	cmd |= STOPONY;
	REX->set.yendi = y1;
	slope = dx / (float)ady;
    }
    if (slope > 0.0)
	slope += REX_M_NORM;
    else
	slope -= REX_M_NORM;
    REX->set.minorslope.flt = slope;
    REX->set.command = cmd;
    x1 = REX->go.command;
    RexWait;
}

int line(int argc, char **argv)
{
    int x0, y0, x1, y1;

    if (argc < 5) {
	printf("usage: %s x0 y0 x1 y1\n", argv[0]);
	return -1;
    }
    atob(argv[1], &x0);
    atob(argv[2], &y0);
    atob(argv[3], &x1);
    atob(argv[4], &y1);
    _line(x0, y0, x1, y1);
    return 0;
}


int _block(int x0, int y0, int x1, int y1)
{
    REX->set.xstarti = x0;
    REX->set.ystarti = y0;
    REX->set.xendi = x1;
    REX->set.yendi = y1;
    REX->set.command = f_logicop | BLOCK | QUADMODE | STOPONX | STOPONY | f_rgb | f_dither | REX_DRAW;
    x1 = REX->go.command;
    RexWait;
}

int block(int argc, char **argv)
{
    int x0, y0, x1, y1;

    if (argc < 5) {
	printf("usage: %s x0 y0 x1 y1\n", argv[0]);
	return -1;
    }
    atob(argv[1], &x0);
    atob(argv[2], &y0);
    atob(argv[3], &x1);
    atob(argv[4], &y1);
    _block(x0, y0, x1, y1);
    return 0;
}


int _span(int x0, int y, int x1)
{
    REX->set.xstarti = x0;
    REX->set.ystarti = y;
    REX->set.xendi = x1;
    REX->set.command = f_logicop | QUADMODE | STOPONX | f_rgb | f_dither | SHADE | REX_DRAW;
    x1 = REX->go.command;
    RexWait;
}

int span(int argc, char **argv)
{
    int x0, y, x1;

    if (argc < 4) {
	printf("usage: %s x0 y x1\n", argv[0]);
	return -1;
    }
    atob(argv[1], &x0);
    atob(argv[2], &y);
    atob(argv[3], &x1);
    _span(x0, y, x1);
    return 0;
}


int _polygon(int x0, int y0, int x1, int y1, float color[][3])
{
    long y;
    int i;
    float clr[3];
    float alpha;
    float r, g, b;

    for (y = y0; y <= y1; y++) {
	alpha = (y - y0) / (float)(y1 - y0);
	r = REX->set.colorredi = 255.0*((1.0 - alpha)*color[0][0] + alpha*color[2][0]);
	g = REX->set.colorgreeni = 255.0*((1.0 - alpha)*color[0][1] + alpha*color[2][1]);
	b = REX->set.colorbluei = 255.0*((1.0 - alpha)*color[0][2] + alpha*color[2][2]);
	if (r < 0.0 || r >= 256)
	    printf("r = %f\n", r);
	if (g < 0.0 || g >= 256)
	    printf("g = %f\n", g);
	if (b < 0.0 || b >= 256)
	    printf("b = %f\n", b);
	    for (i = 0; i < 3; i++) {
	    clr[i] = 256*((1.0 - alpha)*(color[1][i] - color[0][i])
			+ alpha*(color[3][i] - color[2][i]))/(x1 - x0);
	    if (clr[i] < 0)
		clr[i] -= REX_MCOLOR_NORM;
	    else
		clr[i] += REX_MCOLOR_NORM;
	}
	REX->set.slopered.flt = clr[0];
	REX->set.slopegreen.flt = clr[1];
	REX->set.slopeblue.flt = clr[2];
	_span(x0, y, x1);
    }
}


int white(void)
{
    _color(0xff);
    _block(0, 0, 1023, 767);
}


