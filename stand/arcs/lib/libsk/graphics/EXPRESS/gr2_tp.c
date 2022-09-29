/*
 * gr2_tport.c
 *
 * GR2 textport functions.
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.22 $"

#include <sys/gr2hw.h>
#include <sys/cpu.h>
#include <sys/gr2_if.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/gr2.h>
#include <sys/sbd.h>
#include <sys/gr2.h>
#include "stand_htport.h"
#include "cursor.h"
#include <libsc.h>
#if defined(IP19) || defined(IP21) || defined(IP25)
#include <libsk.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/dang.h>
extern int tport_window, tport_adap; 
#endif

/*****************************************************************************
 * 
 * All writes to the pipe are protected from interrupts so that interrupt
 * routines can do printf's on the textport.  Otherwise we would hose the
 * pipe when we did a printf from an interrupt routine.
 * 
 * This may not be necessary since this is probably done at a higher level.
 *
 *****************************************************************************/

/*
 * Wait until FIFO no longer full.
 *
 * Wait until vertical retrace.
 *
 * These look at the CPU interrupt not the interrupt status reg. on a
 * particular graphics board (i.e. not checking for multiple heads).
 */
#if defined(IP19) || defined(IP21) || defined(IP25)
extern volatile long long *dang_ptr;
#define GEWAIT()\
{  int reg; int i = 0; \
   while (((load_double((long long *)(&dang_ptr[DANG_WG_WDCNT]))) > 50) && (i++ < 1000000)); \
   if (i >= 999999) { \
	   reg = load_double((long long *)(&dang_ptr[DANG_WG_WDCNT])); \
	ttyprintf("GEWAIT timedout, stop, fifocnt = %d\n",reg);\
	 while (i);/*sit-n-spin*/\
   }\
}
#else
#define GEWAIT() \
    while (*((volatile unsigned char*)PHYS_TO_K1(LIO_0_ISR_ADDR)) & LIO_FIFO)
#endif


#if defined(IP19)
static volatile struct _expPipeEntryRec *wg = (volatile struct _expPipeEntryRec *)EV_WGINPUT_BASE;
#define FIFO(x,y) 	wg[(x)].pipeUnion.l = (y)
#define FLUSHWG		wg[0xff].pipeUnion.l = 0
#elif defined(IP21)
#define EVEN_OFFSET	0
#define FLUSH_OFFSET	4
static volatile union _expPipeEntryRec *wg = (volatile union _expPipeEntryRec *)EV_WGINPUT_BASE;
#define FIFO(x,y) 	{ wg[EVEN_OFFSET].i = (x); wg[EVEN_OFFSET].i = (y); }
#define FLUSHWG		wg[FLUSH_OFFSET].i = 0
#elif defined(IP25)
static volatile union _expPipeEntryRec *wg = (volatile union _expPipeEntryRec *)EV_WGINPUT_BASE;
#define FIFO(x,y) 	{ wg[0].i = (x); wg[0].i = (y); }
#define FLUSHWG		load_double((long long *)&dang_ptr[DANG_UPPER_GIO_ADDR])
#else
#define FIFO(x,y)	base->fifo[(x)] = (y)
#define FLUSHWG
#endif

#if defined (FLAT_PANEL)
#include "sys/fpanel.h"
static char flat_panel;		/* 0 - no flat panel */
				/* MONITORID_CORONA = 1024x768 flat panel */
				/* MONITORID_ICHIBAN = 1280x1024 flat panel */
#endif /* FLAT_PANEL */

/******************************************************************************
 * Turn Screen On/Off
 *****************************************************************************/
static void Gr2TpBlankscreen(void *hw, int blank)
{
	register struct gr2_hw *base = (struct gr2_hw *)hw;
	
        if (blank) {
		/* Turn off DAC display*/
		base->reddac.addr = GR2_DAC_READMASK;
        	base->reddac.cmd2 = 0;
        	base->grndac.addr = GR2_DAC_READMASK;
        	base->grndac.cmd2 = 0;
        	base->bluedac.addr = GR2_DAC_READMASK;
        	base->bluedac.cmd2 = 0;
        } else {
		/* Turn on DAC display*/
		base->reddac.addr = GR2_DAC_READMASK;
        	base->reddac.cmd2 = 0xff;
        	base->grndac.addr = GR2_DAC_READMASK;
        	base->grndac.cmd2 = 0xff;
        	base->bluedac.addr = GR2_DAC_READMASK;
        	base->bluedac.cmd2 = 0xff;
#if defined(FLAT_PANEL)
		if (flat_panel)
			i2cPanelOn();		/* ensure panel is on. */
#endif /* FLAT_PANEL */
        }
}

/*****************************************************************************
 * Set the current color.
 *****************************************************************************/
static void Gr2TpColor(void *hw, int color)
{
	register struct gr2_hw *base = (struct gr2_hw *)hw;

	GEWAIT();
	FIFO(PUC_COLOR,color);

#ifdef EVEREST
	FLUSHWG;
#endif
}

/*****************************************************************************
 * Create colormap entries in the XMAP chip.
 *****************************************************************************/
void Gr2TpMapcolor(void *hw, int index,	int red, int green, int blue)
{
	register struct gr2_hw *base = (struct gr2_hw *)hw;

	/* Index to 8-bit color map */
	index += 4096;

	GEWAIT();
	base->xmapall.addrhi = (index & 0x1f00) >> 8;
	base->xmapall.addrlo = index & 0xff;
	base->xmapall.clut = red;
	base->xmapall.clut = green;
	base->xmapall.clut = blue;
}

/*****************************************************************************
 * Draw a filled, screen aligned rectangle.
 *****************************************************************************/
static void Gr2TpSboxfi(void *hw, int x1, int y1, int x2, int y2)
{
	register struct gr2_hw	*base = (struct gr2_hw *)hw;

#if defined (FLAT_PANEL)
	if (flat_panel == MONITORID_CORONA) {
		y1 += 256;	/* adjust for low-res display */
		y2 += 256;
	}
#endif /* FLAT_PANEL */
	GEWAIT();
	FIFO(PUC_RECTI2D,x1);
	FIFO(PUC_DATA,y1);
	FIFO(PUC_DATA,x2);
	FIFO(PUC_DATA,y2);

#ifdef EVEREST
	FLUSHWG;
#endif

}

/*****************************************************************************
 * Draw a pixel at the given screen coordinate.
 *****************************************************************************/
static void Gr2TpPnt2i(void *hw, int x, int y)
{
	register struct gr2_hw	*base = (struct gr2_hw *)hw;

#if defined (FLAT_PANEL)
	if (flat_panel == MONITORID_CORONA)
		y += 256;	/* adjust for low-res display */
#endif /* FLAT_PANEL */
	GEWAIT();
	FIFO(PUC_PNT2I,x);
	FIFO(PUC_DATA,y);

#ifdef EVEREST
	FLUSHWG;
#endif

}

/*****************************************************************************
 * Specify the position at which characters should be drawn.
 *****************************************************************************/
static void Gr2TpCmov2i(void *hw, int x, int y)
{
	register struct gr2_hw	*base = (struct gr2_hw *)hw;

#if defined (FLAT_PANEL)
	if (flat_panel == MONITORID_CORONA)
		y += 256;	/* adjust for low-res display */
#endif /* FLAT_PANEL */
	GEWAIT();
	FIFO(PUC_CMOV2I,x);
	FIFO(PUC_DATA,y);

#ifdef EVEREST
	FLUSHWG;
#endif

}

/*****************************************************************************
 * Output a character bitmap.
 *****************************************************************************/
static void Gr2TpDrawBitmap(void *hw, struct htp_bitmap *b)
{
#define GR2MAXW		32
	register struct gr2_hw *base = (struct gr2_hw *)hw;
	register unsigned short *buf;
	register int tmp1;
	int xsize, ysize, yorig;
	int sper = b->sper;
	int xoffset = 0;
	int bufset = 0;
	int xleft = b->xsize;

nextcolumn:
	buf = b->buf + bufset;
	ysize = b->ysize;
	xsize = (xleft > 32) ? 32 : xleft;
	yorig = b->yorig;

	while (ysize > 18) {
		GEWAIT();
		FIFO(PUC_DRAWCHAR,xsize);
		FIFO(PUC_DATA,18);	/* 18 is max height in ucode */
		FIFO(PUC_DATA,(sper > 1) ? 2 : 1);
		FIFO(PUC_DATA,b->xorig - xoffset);
		FIFO(PUC_DATA,yorig);
		FIFO(PUC_DATA,0);	/* xmove and ymove are zero */
		FIFO(PUC_DATA,0);
		
		if (sper > 1) {
			for (tmp1 = 0; tmp1 < 18; tmp1++) {
				FIFO(PUC_DATA,((*buf)<<16)|(*(buf+1)));
				buf += b->sper;
			}
		} else if (sper == 1) {
			for (tmp1 = 0; tmp1 < 18; tmp1++) {
				FIFO(PUC_DATA,*buf);
				buf += b->sper;
			}
		}
		ysize -= 18;
		yorig -= 18;
	}
	FLUSHWG;
	GEWAIT();
	FIFO(PUC_DRAWCHAR,xsize);
	FIFO(PUC_DATA,ysize);
	FIFO(PUC_DATA,(sper > 1) ? 2 : 1);
	FIFO(PUC_DATA,b->xorig - xoffset);
	FIFO(PUC_DATA,yorig);
	FIFO(PUC_DATA,b->xmove);
	FIFO(PUC_DATA,0);		/* ymove is zero */

	tmp1 = 0;
	if (sper > 1) {
		for ( ; tmp1 < 18; tmp1++) {
			FIFO(PUC_DATA,((*buf)<<16)|(*(buf+1)));
			buf += b->sper;
		}
	} else {
		for ( ; tmp1 < ysize; tmp1++) {
			FIFO(PUC_DATA,*buf);
			buf += b->sper;
		}
	}

	while (tmp1++ < 18) {
		FIFO(PUC_DATA,0);
	}

	if (sper > 2) {
		sper -= 2;
		xoffset += 32;
		xleft -= 32;
		bufset += 2;
		goto nextcolumn;
	}
#if defined(IP19) || defined(IP21) || defined(IP25)
	FLUSHWG;
#endif

}

/*****************************************************************************
 * Initialize the GR2 textport.
 *****************************************************************************/
static void Gr2TpInit(void *hw, int *x, int *y)
{
	extern struct gr2_info *getGr2Info(void *hw);
	struct gr2_info *gr2 = getGr2Info(hw);

	if (gr2) {
		*x = gr2->gfx_info.xpmax;
		*y = gr2->gfx_info.ypmax;

#if defined (FLAT_PANEL)
		if (gr2->MonitorType == MONITORID_CORONA ||
		    gr2->MonitorType == MONITORID_ICHIBAN)
			flat_panel = gr2->MonitorType;
		else
			flat_panel = 0;

		if (flat_panel) {
			Gr2TpMapcolor(hw,0,0,0,0);
			Gr2TpColor(hw,0);
			i2cPanelOn();
			Gr2TpSboxfi(hw,0,0,(*x)-1,(*y)-1);
		}
#endif /* FLAT_PANEL */

	}
#if defined (FLAT_PANEL)
	else
		flat_panel = 0;
#endif /* FLAT_PANEL */
}

/*****************************************************************************
 * Move cursor.
 *****************************************************************************/
static void Gr2TpMovec(void *hw, int x, int y)
{
	register struct gr2_hw	*base = (struct gr2_hw *)hw;
	extern struct htp_state *htp;
        int screenx, screeny;

	/* 
	 * Find x origin of cursor bitmap
	 */
	screenx = x - tp_ptr_x_hot;

	/*
	 * If the cursor x position crosses the border from right to left,
	 * or left to right, respectively, then change timing table.
	 * Only needed on high-res displays.  We can assume in any
	 * case that the vc1 VID_EP pointer is initialized elsewhere.
	 */

#if defined (FLAT_PANEL)
	if (flat_panel != MONITORID_CORONA) {
#endif /* FLAT_PANEL */
	if (screenx < 32) {
		/* Cross from right to left*/
		/* Add horzontal offset */
               	screenx +=  GR2_CURS_XOFF_1280;

		base->vc1.addrhi = (GR2_VID_EP >> 8) & 0xff;
       		base->vc1.addrlo = GR2_VID_EP & 0xff;
       		base->vc1.cmd0 = GR2_VIDTIM_FRMT_BASE;
       	} else {
		/* Cross from left to right*/
		base->vc1.addrhi = (GR2_VID_EP >> 8) & 0xff;
       		base->vc1.addrlo = GR2_VID_EP & 0xff;
       		base->vc1.cmd0 = GR2_VIDTIM_CURSFRMT_BASE;
	}
#if defined (FLAT_PANEL)
	}
#endif /* FLAT_PANEL */

	/* Y origin of bitmap */
	screeny = y - tp_ptr_y_hot;

#if defined (FLAT_PANEL)
	if (flat_panel == MONITORID_CORONA) {
		screenx += GR2_CURS_XOFF_CORONA;
		screeny += GR2_CURS_YOFF_CORONA;
	}
	else
#endif /* FLAT_PANEL */
	screeny += GR2_CURS_YOFF_1280;
	
	/* Write x, y values */
	base->vc1.addrhi = (GR2_CUR_XL >> 8) & 0xff;
	base->vc1.addrlo = GR2_CUR_XL & 0xff;
	base->vc1.cmd0 = screenx;
	base->vc1.cmd0 = screeny;
}

static void
Gr2TpBmove(void *hw, int oldx, int oldy, int newx, int newy, int w, int h)
{
	register struct gr2_hw	*base = (struct gr2_hw *)hw;
	int word_len = ((w+3)>>2);
	int max_lines_stored = 4864 / word_len;	/* reserved shram area size */
#if defined (FLAT_PANEL)
	int screenheight;

	if (flat_panel == MONITORID_CORONA)
		screenheight = 768;
	else
		screenheight = 1024;
#else  /* FLAT_PANEL */
#define screenheight 1024
#endif /* FLAT_PANEL */

	/* gr2 rectcopy ucode uses Xish coordinates */
	oldy = screenheight - oldy - h;
	newy = screenheight - newy - h;

	if ((newy <= oldy) || (h <= max_lines_stored)) {
		GEWAIT();
		FIFO(PUC_RECTCOPY,word_len);
		FIFO(PUC_DATA,max_lines_stored);
		FIFO(PUC_DATA,oldx);
		FIFO(PUC_DATA,oldy);
		FIFO(PUC_DATA,w);
		FIFO(PUC_DATA,h);
		FIFO(PUC_DATA,newx);
		FIFO(PUC_DATA,newy);
	}
	else {
		int srcy, dsty, draw_height;

		srcy = oldy + h - max_lines_stored;
		dsty = newy + h - max_lines_stored;
		draw_height = _min(h,max_lines_stored);
		while (draw_height) {
			GEWAIT();
			FIFO(PUC_RECTCOPY,word_len);
			FIFO(PUC_DATA,max_lines_stored);
			FIFO(PUC_DATA,oldx);
			FIFO(PUC_DATA,srcy);
			FIFO(PUC_DATA,w);
			FIFO(PUC_DATA,draw_height);
			FIFO(PUC_DATA,newx);
			FIFO(PUC_DATA,dsty);
			h -= draw_height;
			draw_height = _min(h,max_lines_stored);
			srcy -= draw_height;
			dsty -= draw_height;
		}
	}

#ifdef EVEREST
	FLUSHWG;
#endif

	return;
}

/*****************************************************************************
 * Host Textport routines.  This is the interface with htport.
 *****************************************************************************/
struct htp_fncs gr2_tp_fncs = {
	Gr2TpBlankscreen,
	Gr2TpColor,
	Gr2TpMapcolor,
	Gr2TpSboxfi,
	Gr2TpPnt2i,
	Gr2TpCmov2i,
	Gr2TpDrawBitmap,
	Gr2TpBmove,
	Gr2TpInit,
	Gr2TpMovec,
};
