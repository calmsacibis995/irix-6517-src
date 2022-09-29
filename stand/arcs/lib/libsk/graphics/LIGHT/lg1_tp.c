#ident	"lib/libsk/graphics/LIGHT/lg1_tp.c:  $Revision: 1.14 $"

/* 
 * lg1_tp.c - textport graphics routines for LIGHT
 */

#include <sys/lg1hw.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include <sys/lg1.h>
#include "stand_htport.h"
#include "cursor.h"

#include "macros.h"
#include <libsc.h>
#include <libsk.h>

#define LEFT16(x) ((x)<<16)

extern struct lg1_info lg1_ginfo;

struct {
    long cpos_x, cpos_y;
    int  color;
} context;

static void
Lg1TpBlankscreen(void *hw, int mode)
{
	register struct rexchip *base = (struct rexchip*)hw;
	int state;

	base->p1.go.configsel = VC1_SYS_CTRL;
	VC1GetByte(base, state);
	
	base->p1.go.configsel = VC1_SYS_CTRL;
	if (mode) {
		VC1SetByte(base, state & ~VC1_VC1);	/* screen off */
	}
	else {
		VC1SetByte(base, state | VC1_VC1);	/* screen on */
	}
	
	base->p1.go.configsel = VC1_SYS_CTRL;
	VC1GetByte(base, state);
}

static void
Lg1TpColor(void *hw, int col)
{
	register struct rexchip *base = (struct rexchip*)hw;

	context.color = col;
	RexWait(base);
	base->set.colorredi = col;
}

static void
Lg1TpMapcolor(void *hw, int col, int r, int g, int b)
{
	register struct rexchip *base = (struct rexchip*)hw;
        int sync = lg1_ginfo.monitortype == 6 ? 0 : 1;

	if (lg1_ginfo.boardrev < 2) {			/* LG1 */
		btSet(base, WRITE_ADDR, 0x82);
		btSet(base, CONTROL, 0x0f);
	} else {					/* LG2 */
		btSet(base, CONTROL, sync | 0x2);
	}
	btSet(base, WRITE_ADDR, col);
	base->p1.set.configsel = PALETTE_RAM;
	
	base->p1.set.rwdac = r;
	base->p1.go.rwdac = r;
	base->p1.set.rwdac = g;
	base->p1.go.rwdac = g;
	base->p1.set.rwdac = b;
	base->p1.go.rwdac = b;
	
	if (lg1_ginfo.boardrev < 2) {
		/* pixel read mask */
		btSet(base, PIXEL_READ_MASK, 0xff);
	}
}

static void
Lg1TpSboxfi(void *hw, int x1, int y1, int x2, int y2)
{
	register struct rexchip *base = (struct rexchip*)hw;
	int cmd;

	RexWait(base);
	base->set.yendi = 767 - y1;	/* flip Y's */
	base->set.ystarti =   767 - y2;
	base->set.xstarti = x1;
	base->set.xendi =   x2;
	cmd = REX_LO_SRC | REX_DRAW | QUADMODE | BLOCK | STOPONX | STOPONY;
	base->set.command = cmd;
	x1 = base->go.command;
}

static void
Lg1TpPnt2i(void *hw, int x, int y)
{
	register struct rexchip *base = (struct rexchip*)hw;
	int cmd;
    
	RexWait(base);
	base->set.xstarti = x;
	base->set.ystarti = 767 - y; /* flip Y */
	cmd = REX_LO_SRC | REX_DRAW;
	base->set.command = cmd;
	x = base->go.command;
}

static void
Lg1TpCmov2i(void *hw, int x, int y)
{
    context.cpos_x = x;
    context.cpos_y = y;
}

static void
Lg1TpDrawbitmap(void *hw, struct htp_bitmap *bitmap)
{
	register struct rexchip *base = (struct rexchip*)hw;
	int xsize, ysize, ystart, xleft, xright;
	int cmd;
	unsigned short *tp;

	xleft = context.cpos_x - bitmap->xorig;
	
	xsize = bitmap->xsize;
	xright= xleft + xsize - 1;
	ysize = bitmap->ysize;
	ystart = 767 - context.cpos_y + bitmap->yorig;
	
	tp = bitmap->buf;
	cmd = REX_LO_SRC | REX_DRAW | ENZPATTERN | QUADMODE | STOPONX | XYCONTINUE;
	
	RexWait(base);
	base->set.xstarti = xleft;
	base->set.ystarti = ystart;
	base->set.xendi = xright; 	/*
					 * Must be set before command,
					 * so REX can determine x direction
					 */
	base->set.yendi = 0;
	base->go.command = REX_NOP;
	
	/* This code is optimized (for time),
	   it can be reduced if space is a problem */
	
	if (xsize <= 16) {
		base->set.command = cmd|BLOCK|LENGTH32;
		base->set.xendi = xright;
		while (ysize > 16) {
			base->go.zpattern = LEFT16(tp[ 0]);
			base->go.zpattern = LEFT16(tp[ 1]);
			base->go.zpattern = LEFT16(tp[ 2]);
			base->go.zpattern = LEFT16(tp[ 3]);
			base->go.zpattern = LEFT16(tp[ 4]);
			base->go.zpattern = LEFT16(tp[ 5]);
			base->go.zpattern = LEFT16(tp[ 6]);
			base->go.zpattern = LEFT16(tp[ 7]);
			base->go.zpattern = LEFT16(tp[ 8]);
			base->go.zpattern = LEFT16(tp[ 9]);
			base->go.zpattern = LEFT16(tp[10]);
			base->go.zpattern = LEFT16(tp[11]);
			base->go.zpattern = LEFT16(tp[12]);
			base->go.zpattern = LEFT16(tp[13]);
			base->go.zpattern = LEFT16(tp[14]);
			base->go.zpattern = LEFT16(tp[15]);
			ysize -= 16;
			tp += 16;
		}
		switch (ysize) {
		case 16:	base->go.zpattern = LEFT16(*tp++);
		case 15:        base->go.zpattern = LEFT16(*tp++);
		case 14:        base->go.zpattern = LEFT16(*tp++);
		case 13:        base->go.zpattern = LEFT16(*tp++);
		case 12:        base->go.zpattern = LEFT16(*tp++);
		case 11:        base->go.zpattern = LEFT16(*tp++);
		case 10:        base->go.zpattern = LEFT16(*tp++);
		case  9:        base->go.zpattern = LEFT16(*tp++);
		case  8:        base->go.zpattern = LEFT16(*tp++);
		case  7:        base->go.zpattern = LEFT16(*tp++);
		case  6:        base->go.zpattern = LEFT16(*tp++);
		case  5:        base->go.zpattern = LEFT16(*tp++);
		case  4:        base->go.zpattern = LEFT16(*tp++);
		case  3:        base->go.zpattern = LEFT16(*tp++);
		case  2:        base->go.zpattern = LEFT16(*tp++);
		case  1:        base->go.zpattern = LEFT16(*tp++);
		}
	} else {  /* multi-shorts per row, slower chars   */
		int xend;
		
		while (ysize-- > 0) {
			/* don't bump y when done in X  */
			base->set.command = cmd;
			xend = xleft + 15;
			while (xend < xright) {
				base->set.xendi = xend;
				base->go.zpattern = LEFT16(*tp++);
				xend += 16;
			}
			/* now bump y when done in X    */
			base->set.command = cmd|BLOCK;
			base->set.xendi = xright;
			base->go.zpattern = LEFT16(*tp++);
		}
	}
	context.cpos_x += bitmap->xmove;
	context.cpos_y += bitmap->ymove;
}

static void
Lg1TpBmove(void *hw, int oldx, int oldy, int newx, int newy, int w, int h)
{
	register struct rexchip *base = (struct rexchip*)hw;
	int cmd, xymove, endy;
	short dx,dy;
	
	cmd = REX_LO_SRC|LOGICSRC|REX_DRAW|QUADMODE|BLOCK|STOPONX|STOPONY;
	
	dx = newx - oldx;
	dy = newy - oldy;
	xymove = ((dx << 16) & 0xffff000) | (dy & 0xffff);
	
	if (dy > 0) {
		endy = newy;
		newy += h;
	}
	else {
		endy = newy + h;
	}
	
	RexWait(base);
	base->set.xstarti = newx;
	base->set.xendi = newx + w;
	base->set.ystarti = 767 - newy;
	base->set.yendi = 767 - endy;
	base->set.command = cmd;
	base->go.xymove = xymove;
}

static void
Lg1TpInit(void *hw, int *x, int *y)
{
	*x = lg1_ginfo.gfx_info.xpmax;
	*y = lg1_ginfo.gfx_info.ypmax;
}

static void
Lg1TpMovec(void *hw, int x, int y)
{
	register struct rexchip *base = (struct rexchip*)hw;

	x += CURSOR_XOFF - tp_ptr_x_hot;
	y += CURSOR_YOFF - tp_ptr_y_hot;

	VC1SetAddr(base, 0x22, 0);
	VC1SetWord(base, x);
	VC1SetWord(base, y);
}

struct htp_fncs lg1_tp_fncs = {
    Lg1TpBlankscreen,
    Lg1TpColor,
    Lg1TpMapcolor,
    Lg1TpSboxfi,
    Lg1TpPnt2i,
    Lg1TpCmov2i,
    Lg1TpDrawbitmap,
    Lg1TpBmove,
    Lg1TpInit,
    Lg1TpMovec
};
