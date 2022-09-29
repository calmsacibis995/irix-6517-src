#ident	"lib/libsk/graphics/NEWPORT/ng1_tp.c:  $Revision: 1.10 $"

/* 
 * ng1_tp.c - textport graphics routines for NEWPORT
 */

#include "sys/ng1hw.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/ng1.h"
#include "stand_htport.h"
#include "cursor.h"
#include "sys/vc2.h"
#include "sys/ng1_cmap.h"

#define LEFT16(x) ((x)<<16)

#define rex3Set( hwpage, reg, val ) \
    hwpage->set.reg = (val);

#define rex3SetConfig( hwpage, reg, val ) \
    hwpage->p1.set.reg = (val);

#define rex3SetAndGo( hwpage, reg, val ) \
    hwpage->go.reg = (val);


extern struct ng1_info ng1_ginfo[];

static struct {
    long cpos_x, cpos_y;
    int  color;
} tpcontext;

static void
Ng1TpBlankscreen(void *hw, int mode)
{
	register struct rex3chip *base = (struct rex3chip*)hw;
	int state;

        BFIFOWAIT(base);
	vc2GetReg (base, VC2_DC_CONTROL, state);
	if (mode) {
		vc2SetReg (base, VC2_DC_CONTROL, state & ~VC2_ENA_DISPLAY);
	}
	else {
		vc2SetReg (base, VC2_DC_CONTROL, state | VC2_ENA_DISPLAY);
	}
}

static void
Ng1TpColor(void *hw, int col)
{
	register struct rex3chip *base = (struct rex3chip*)hw;

        tpcontext.color = col;
        REX3WAIT(base);
        rex3Set(base, colori, col);
}

static void
Ng1TpMapcolor(void *hw, int col, int r, int g, int b)
{
	register struct rex3chip *base = (struct rex3chip*)hw;
        BFIFOWAIT(base);
        cmapSetAddr( base, col );
        cmapSetRGB( base, r, g, b );
}

static void
Ng1TpSboxfi(void *hw, int x1, int y1, int x2, int y2)
{
	register struct rex3chip *base = (struct rex3chip*)hw;
        unsigned long int command0;
	int idx = Ng1Index(hw);

        command0 = DM0_DRAW | DM0_BLOCK | DM0_DOSETUP |
                                        DM0_STOPONX | DM0_STOPONY;
        REX3WAIT(base);
        /* Do we have to flip Y's on REX3? */
	rex3Set( base, drawmode0, command0 );
        rex3Set( base, xystarti, ((x1 << 16) |
				(ng1_ginfo[idx].gfx_info.ypmax - y2)) );
        rex3SetAndGo( base, xyendi, ((x2 << 16) |
				(ng1_ginfo[idx].gfx_info.ypmax - y1)) );
}

static void
Ng1TpPnt2i(void *hw, int x, int y)
{
	register struct rex3chip *base = (struct rex3chip*)hw;
        unsigned long int command0;
	int idx = Ng1Index(hw);

        command0 = DM0_DRAW | DM0_BLOCK ;
        REX3WAIT(base);
        rex3Set( base, drawmode0, command0 );
        rex3SetAndGo( base, xystarti, ((x << 16) |
				(ng1_ginfo[idx].gfx_info.ypmax - y)) );
}

static void
Ng1TpCmov2i(void *hw, int x, int y)
{
    tpcontext.cpos_x = x;
    tpcontext.cpos_y = y;
}

static void
Ng1TpDrawbitmap(void *hw, struct htp_bitmap *bitmap)
{
	struct rex3chip *base = (struct rex3chip *)hw;
	unsigned long int command0;
	int idx = Ng1Index(hw);

	int xsize, ysize, ystart, xleft, xright;
	unsigned short *tp;
    
	xleft = tpcontext.cpos_x - bitmap->xorig;
	xsize = bitmap->xsize;
	xright= xleft + xsize - 1;
	ysize = bitmap->ysize;
	ystart = ng1_ginfo[idx].gfx_info.ypmax -
			tpcontext.cpos_y + bitmap->yorig;

	tp = bitmap->buf;
	command0 = DM0_DRAW | DM0_BLOCK | DM0_STOPONX |
					DM0_ENZPATTERN | DM0_LENGTH32;
	REX3WAIT(base);
	rex3Set(base, drawmode0, command0);
	rex3Set(base, bresoctinc1, 0x1000000);

	/* This code is optimized for time */
    
	if (xsize <= 16) 
	{
		rex3Set( base, xystarti, ((xleft << 16) | ystart) );
		rex3Set( base, xyendi, (xright << 16) );

		while (ysize > 16) 
		{
			REX3WAIT(base);
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 0]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 1]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 2]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 3]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 4]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 5]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 6]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 7]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 8]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[ 9]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[10]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[11]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[12]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[13]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[14]));
			rex3SetAndGo( base, zpattern, LEFT16(tp[15]));
			ysize -= 16;
			tp += 16;
		}
		REX3WAIT(base);
		switch (ysize) 
		{
			case 16 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case 15 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case 14 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case 13 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case 12 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case 11 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case 10 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case  9 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case  8 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case  7 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case  6 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case  5 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case  4 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case  3 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case  2 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			case  1 : rex3SetAndGo(base, zpattern, LEFT16(*tp++));
			REX3WAIT(base);
	       }
	}
	else 
	{	/* multi-shorts per row, slower chars   */
		int xstart, xend, stride, numspans, i;
		unsigned short *tmp_tp = tp;

		stride = (xsize >> 4) + (xsize & 0xf ? 1 : 0);
		xstart = xleft;
		xend = xleft + 15;
		
		REX3WAIT(base);
		while (xend < xright) 
		{
		    rex3Set(base, xystarti, ((xstart << 16) | ystart));
		    rex3Set(base, xyendi, (xend << 16) );
		    numspans = ysize;
			
		    while (numspans > 16){
			for(i=0; i<16; i++){
			    rex3SetAndGo(base, zpattern, LEFT16(*tmp_tp));
			    tmp_tp += stride;
			}
			numspans -= 16;
			REX3WAIT(base);
		    }
		    if(numspans){
		        while (numspans-- > 0) {
			    rex3SetAndGo(base, zpattern, LEFT16(*tmp_tp));
			    tmp_tp += stride;
			}
		        REX3WAIT(base);
		    }
		    xstart = xend + 1;
		    xend += 16;
		    tmp_tp = ++tp;
		}

		rex3Set(base, xystarti, ((xstart << 16) | ystart));
		rex3Set(base, xyendi, (xright << 16));
		numspans = ysize;
			
		while (numspans > 16){
		    for(i=0; i<16; i++){
			rex3SetAndGo(base, zpattern, LEFT16(*tmp_tp));
			tmp_tp += stride;
		    }
		    numspans -= 16;
		    REX3WAIT(base);
		}
		if(numspans){
		    while (numspans-- > 0) {
			rex3SetAndGo(base, zpattern, LEFT16(*tmp_tp));
			tmp_tp += stride;
		    }	
		    REX3WAIT(base);
		}
	}
	tpcontext.cpos_x += bitmap->xmove;
	tpcontext.cpos_y += bitmap->ymove;
}

static void
Ng1TpBmove(void *hw, int oldx, int oldy, int newx, int newy, int w, int h)
{
	register struct rex3chip *base = (struct rex3chip*)hw;
	int idx = Ng1Index(hw);
	int xymove, startx, endx, starty, endy;
	short dx,dy;
	unsigned long int command0;
	
	command0 = DM0_SCR2SCR | DM0_STOPONX | DM0_STOPONY |
						DM0_BLOCK | DM0_DOSETUP;

	oldy = ng1_ginfo[idx].gfx_info.ypmax - oldy;
	newy = ng1_ginfo[idx].gfx_info.ypmax - newy;
	dx = newx - oldx;
	dy = newy - oldy;
	xymove = ((dx << 16) & 0xffff000) | (dy & 0xffff);
	
	if (dy > 0) {
		starty = oldy;
		endy = oldy - h;
	}
	else {
		starty = oldy - h;
		endy = oldy;
	}
	if (dx > 0) {
		startx = oldx + w;
		endx = oldx;
	}
	else {
		startx = oldx;
		endx = oldx + w;
	}
	
	REX3WAIT(base);
	rex3Set( base, xystarti, ( ( startx << 16) | starty ) );
	rex3Set( base, xyendi, ( ( endx << 16) | endy ) );
	rex3Set( base, drawmode0, command0);
	rex3SetAndGo( base, xymove, xymove);
}

static void
Ng1TpInit(void *hw, int *x, int *y)
{
    int idx = Ng1Index(hw);

    *x = ng1_ginfo[idx].gfx_info.xpmax;
    *y = ng1_ginfo[idx].gfx_info.ypmax;
}

static void
Ng1TpMovec(void *hw, int x, int y)
{
	register struct rex3chip *base = (struct rex3chip*)hw;

	x += CURSOR_XOFF - tp_ptr_x_hot;
	y += CURSOR_YOFF - tp_ptr_y_hot;

        BFIFOWAIT(base);
	vc2SetReg( base, VC2_CURS_X_LOC, x );
        vc2SetReg( base, VC2_CURS_Y_LOC, y );
}

struct htp_fncs ng1_tp_fncs = {
    Ng1TpBlankscreen,
    Ng1TpColor,
    Ng1TpMapcolor,
    Ng1TpSboxfi,
    Ng1TpPnt2i,
    Ng1TpCmov2i,
    Ng1TpDrawbitmap,
    Ng1TpBmove,
    Ng1TpInit,
    Ng1TpMovec
};
