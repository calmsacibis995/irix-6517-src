/*
 * htport.c
 *
 * Device independent host graphics textport code.
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

#ident "$Revision: 1.105 $"

#include <setjmp.h>
#include <gfxgui.h>
#include "saioctl.h"

#if	defined(_TP_VANILLA)		/* same look as old tp */
#define _TP_FLAT
#define _TP_OLDFONT
#define _TP_OLDSCROLL
#elif defined(_TP_KERNEL)		/* new textport in kernel */
#define _TP_OLDSCROLL
#else
/* #define _TP_COLOR 			scrolling too slow on non-lg1 */
#define _GFXGUI
#endif

#include <stand_htport.h>		/* needs _TP_COLOR [un]defined */
#include <style.h>

#ifdef _TP_OLDFONT
#include <fonts/iris10i.h>
#define TPFONT_HT	iris10i_ht
#define TPFONT_DC	4
#else
#include <fonts/scrB18.h>
#define TPFONT_HT	SCRB18_HT
#define TPFONT_DC	SCRB18_DC
#endif

#include <fonts/special.h>
#ifdef _GFXGUI
#include <fonts/ncenB18.h>
#include <fonts/helvR10.h>
#include <guicore.h>
#include <gfxgui.h>
#endif
#include <libsc.h>
#include <libsk.h>
#include "gfxgui.h"

static struct htp_font fonts[NUMFONTS];

#ifdef _TP_COLOR
/* pull colors out of array */
static void drawbg(struct htp_state *, int, int, int, int);
static void propbg(struct htp_state *, int, int);
#define foreground(rp,col)	(((rp)->color[(col)] & 0x0f) + \
				 (IS_HL((rp)->data[(col)]) ? HIGHLBASE : 0))
#define background(rp,col)	(((rp)->color[(col)] & 0xf0)>>4)
#else
#define drawbg(tp,x,y,xe,ye)	((*(tp)->fncs->sboxfi)((tp)->hw,x,y,xe,ye))
#define propbg(tp,x,y)
#define foreground(rp,col)	(tp->fgnd)
#define background(rp,col)	(tp->bgnd)
#endif

#define LRT_WIDTH	16	/* width for left, right, and top border */
#define B_WIDTH		50	/* width for bottom border */

#define IN_WIDTH	3
#define OUT_WIDTH	4

#define LRT_BDR		((LRT_WIDTH)-(OUT_WIDTH))
#define B_BDR		((B_WIDTH)-(OUT_WIDTH))

/* function to propegate background color */
#define PBG_LINE	0
#define PBG_EOL		1
#define PBG_BOL		2

/* border sizes for textport. */
#ifdef _GFXGUI
#define TP_YBORDER 90
#else
#define TP_YBORDER 48
#endif
#define TP_XBORDER 96

static struct htp_state hosttextport;
struct htp_state *htp = &hosttextport;
static char *donestr;

static void txShdOutline(struct htp_state *, int, int, int, int);
static void txReset(struct htp_state *, int, int);

/*
 * interpret a two digit hex constant as a eight bit color value.
 */
static int
txhexcolor(unsigned char *cp)
{
	int res;
	if ((*cp>='0') && (*cp<='9')) res = *cp - '0';
	else if ((*cp>='a') && (*cp<='f')) res = *cp - 'a' +10;
	else res = 0;
	cp++;
	res = res<<4;
	if ((*cp>='0') && (*cp<='9')) res += *cp - '0';
	else if ((*cp>='a') && (*cp<='f')) res += *cp - 'a' +10;
	return(res);
}

/*
 * get a color as a six digit hex string in the enviroment
 */
#ifdef _STANDALONE
static int
txGetenv_col(char *cp, int *r, int *g, int *b)
{
	char *envp;

	envp = getenv(cp);
	if (!envp) return 0;

	*r = txhexcolor((unsigned char *)(envp+0));
	*g = txhexcolor((unsigned char *)(envp+2));
	*b = txhexcolor((unsigned char *)(envp+4));
 
	return 1;
}

#else /* !_STANDALONE */

static int
txGetenv_col(char *cp, int *r, int *g, int *b)
{
	static struct {
		char *name;
		char *arg;
	} cstring[] = {
		{"screencolor", arg_screencolor},
		{"pagecolor", arg_pagecolor}
	};
	int i;

	for (i = 0; i < (sizeof(cstring)/sizeof(cstring[0])); i++)
		if (!strcmp(cp, cstring[i].name)
		    && is_specified(cstring[i].arg)) {
			char *a = cstring[i].arg;
			*r = txhexcolor((unsigned char*)a+0);
			*g = txhexcolor((unsigned char*)a+2);
			*b = txhexcolor((unsigned char*)a+4);
			return 1;
		}
	return 0;
}
#endif /* !_STANDALONE */

/*
 * determine whether black or white gives the maximum contrast with a color.  
 * Then return the index of the contrasting color.
 */
static int
txMaxcontrast(int r, int g, int b)
{
	int delta_r, delta_g, delta_b;
	int white_norm, blk_norm;

	blk_norm = r*r + g*g + b*b;

	delta_r = 255 - r;
	delta_g = 255 - g;
	delta_b = 255 - b;

	white_norm = delta_r*delta_r + delta_g*delta_g + delta_b*delta_b;

	if (blk_norm > white_norm) 
		return(BLACK);
	else
		return(WHITE);
}

/*
 * create the color map for the terminal emulator
 */
void
txMap(struct htp_state *tp)
{
	struct htp_fncs *fncs = tp->fncs;
	int tp_r, tp_g, tp_b;
	int i;

	/* Set the default entries for the first 8 colors */
	(*fncs->mapcolor)(tp->hw, BLACK,     0,   0,   0);
	(*fncs->mapcolor)(tp->hw, RED,     255,   0,   0);
	(*fncs->mapcolor)(tp->hw, GREEN,     0, 255,   0);
	(*fncs->mapcolor)(tp->hw, YELLOW,  255, 255,   0);
	(*fncs->mapcolor)(tp->hw, BLUE,      0,   0, 255);
	(*fncs->mapcolor)(tp->hw, MAGENTA, 255,   0, 255);
	(*fncs->mapcolor)(tp->hw, CYAN,      0, 255, 255);
	(*fncs->mapcolor)(tp->hw, WHITE,   240, 240, 240);
	(*fncs->mapcolor)(tp->hw, BLACK2,   85,  85,  85);
	(*fncs->mapcolor)(tp->hw, RED2,    198, 113, 113);
	(*fncs->mapcolor)(tp->hw, GREEN2,  113, 198, 113);
	(*fncs->mapcolor)(tp->hw, YELLOW2, 142, 142,  56);
	(*fncs->mapcolor)(tp->hw, BLUE2,   113, 113, 198);
	(*fncs->mapcolor)(tp->hw, MAGENTA2,142,  56, 142);
	(*fncs->mapcolor)(tp->hw, CYAN2,    56, 131, 131);
	(*fncs->mapcolor)(tp->hw, WHITE2,  170, 170, 170);

#ifdef _TP_COLOR
	/* high intensity colors */
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + BLACK,    62,  62,  62);
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + RED,     255,  62,  62);
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + GREEN,     0, 255, 127);
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + YELLOW,  255, 255, 125);
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + BLUE,      0, 173, 255);
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + MAGENTA, 255, 100, 255);
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + CYAN,    187, 255, 255);
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + WHITE,   255, 255, 255);
#endif

	/* grays for borders, etc */
	(*fncs->mapcolor)(tp->hw,GRAY42,42,42,42);
	(*fncs->mapcolor)(tp->hw,GRAY85,85,85,85);
	(*fncs->mapcolor)(tp->hw,GRAY128,128,128,128);
	(*fncs->mapcolor)(tp->hw,GRAY170,170,170,170);
	(*fncs->mapcolor)(tp->hw,GRAY213,213,213,213);
	
	(*fncs->mapcolor)(tp->hw, SCR_IDX, SCR_DF_R, SCR_DF_G, SCR_DF_B);
#ifdef _TP_COLOR
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + SCR_IDX, scr_r, scr_g, scr_b);
#endif
	(*fncs->mapcolor)(tp->hw, INR_BORDER, 64, 64, 192);
	
	/*
	 * at the current time the login and boot panels are the same
	 * color as the textport
	 */
	if (!txGetenv_col("pagecolor", &tp_r, &tp_g, &tp_b)) {
		tp_r = TP_DF_R;
		tp_g = TP_DF_G;
		tp_b = TP_DF_B;
	}
	
	(*fncs->mapcolor)(tp->hw, TP_IDX, tp_r, tp_g, tp_b);
	(*fncs->mapcolor)(tp->hw, PNL_IDX, tp_r, tp_g, tp_b);
#ifdef _TP_COLOR
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + TP_IDX, tp_r, tp_g, tp_b);
	(*fncs->mapcolor)(tp->hw, HIGHLBASE + PNL_IDX, tp_r, tp_g, tp_b);
#endif
	
	/* XXX Need extra globals, if the RGB contents for PNL_IDX
	 * are different from that of TP_IDX.  These variables are used to fine
	 * out the contrast color for ouputing the text. */
	tp->page_r = tp_r;
	tp->page_g = tp_g;
	tp->page_b = tp_b;
	
#ifdef _GFXGUI
	/*
	 * Extra colors for gui
	 */
	(*fncs->mapcolor)(tp->hw, lightGrayBlue, 85, 85, 213);
	(*fncs->mapcolor)(tp->hw, lightBlue, 128, 128, 255);
	(*fncs->mapcolor)(tp->hw, lightGrayCyan, 85, 213, 213);
	(*fncs->mapcolor)(tp->hw, lightCyan, 128, 255, 255);
	(*fncs->mapcolor)(tp->hw, lightGrayGreen, 85, 213, 85);
	(*fncs->mapcolor)(tp->hw, lightGreen, 128, 255, 128);
	(*fncs->mapcolor)(tp->hw, lightGrayMagenta, 213, 85, 213);
	(*fncs->mapcolor)(tp->hw, lightMagenta, 255, 128, 255);
	(*fncs->mapcolor)(tp->hw, lightGrayRed, 213, 85, 85);
	(*fncs->mapcolor)(tp->hw, lightRed, 255, 128, 128);
	(*fncs->mapcolor)(tp->hw, TerraCotta, 194, 142, 142);
	(*fncs->mapcolor)(tp->hw, DarkTerraCotta, 142, 99, 99);
	(*fncs->mapcolor)(tp->hw, SlateBlue, 113, 113, 198);

#if IP24		/* icon colors to draw Indy */
	(*fncs->mapcolor)(tp->hw, MACHCLR0, 0x09, 0x45, 0x75);
	(*fncs->mapcolor)(tp->hw, MACHCLR1, 0x23, 0x6f, 0xb4);
	(*fncs->mapcolor)(tp->hw, MACHCLR2, 0x01, 0x30, 0x57);
#elif IP22 || IP26	/* icon colors to draw Indigo2 */
	(*fncs->mapcolor)(tp->hw, MACHCLR0, 0x38, 0x92, 0x79);
	(*fncs->mapcolor)(tp->hw, MACHCLR1, 0x09, 0x70, 0x57);
	(*fncs->mapcolor)(tp->hw, MACHCLR2, 0x3f, 0xff, 0xbf);
	(*fncs->mapcolor)(tp->hw, MACHCLR3, 0x3f, 0x91, 0x7f);
#elif IP28  		/* Impact box colors */
	(*fncs->mapcolor)(tp->hw, MACHCLR0, 0x66, 0x09, 0x70);
	(*fncs->mapcolor)(tp->hw, MACHCLR1, 0x24, 0x09, 0x30);
	(*fncs->mapcolor)(tp->hw, MACHCLR2, 0xc7, 0x3f, 0xff);
	(*fncs->mapcolor)(tp->hw, MACHCLR3, 0x6e, 0x01, 0x7a);
#elif IP30
	(*fncs->mapcolor)(tp->hw, MACHCLR0, 0x08, 0x2f, 0x4a);
	(*fncs->mapcolor)(tp->hw, MACHCLR1, 0x00, 0x8b, 0x9b);
	(*fncs->mapcolor)(tp->hw, MACHCLR2, 0x57, 0x47, 0x63);
#endif

	/*
	 * Allocate color ramp for "root" window.
	 */
	{
		int inc_r = ((TOPCOLOR_R - BOTTOMCOLOR_R) << 16) / RAMPSIZE;
		int inc_g = ((TOPCOLOR_G - BOTTOMCOLOR_G) << 16) / RAMPSIZE;
		int inc_b = ((TOPCOLOR_B - BOTTOMCOLOR_B) << 16) / RAMPSIZE;
		int r = (BOTTOMCOLOR_R<<16);
		int g = (BOTTOMCOLOR_G<<16);
		int b = (BOTTOMCOLOR_B<<16);

		for (i = 0; i < RAMPSIZE; i++) {
			(*fncs->mapcolor)(tp->hw, i + RAMPSTART,
					  r>>16,g>>16,b>>16);
			r += inc_r;
			g += inc_g;
			b += inc_b;
		}
	}
	(*fncs->mapcolor)(tp->hw, COMPANYNAMECOLOR, 0xff, 0xff, 0x00);
	(*fncs->mapcolor)(tp->hw, CLOGOCOLOR, 0x4b, 0x00, 0x83);
	(*fncs->mapcolor)(tp->hw, CLOGOSHADOWCOLOR, 0x5f, 0x5f, 0x92);
#endif /* _GFXGUI */

	/*
	 * since all backgrounds are the same, we can do this here
	 */
	tp->cfgnd = tp->fgnd = txMaxcontrast(tp_r, tp_g, tp_b);
}

static void
txInit(struct htp_state *tp)
{
	struct htp_textport *tx = &(tp->textport);
	struct htp_row *rp;
	int i;

	/*
	 * initialize the textport data structure.
	 */
	tx->attribs = 0;
	tx->state = DIRTY;

	tx->llx = (tp->xscreensize - XSIZE(tx->numcols) - XBORDER * 2) / 2;
#ifdef _GFXGUI
	if (tx->aflags & TP_DONEB)
		tx->lly = (tp->yscreensize - (tp->fontheight * tx->numrows) -
			   (B_WIDTH+LRT_WIDTH)) / 2 + B_WIDTH;
	else
#endif
	tx->lly = (tp->yscreensize - (tp->fontheight * tx->numrows) -
		   (YBORDER * 2)) / 2;
#ifdef _TP_OLDFONT
	/* need to line up with kernel textport on 1024x768 displays */
	if (tp->yscreensize < 1000)
		tx->lly -= 10;
	else 
		tx->lly += (tp->yscreensize>>5);
#endif

	tx->crow = 0;
	tx->ccol = 0;

#ifdef _GFXGUI
	{
		extern unsigned char clogo_data[];
		extern int clogo_w, clogo_h;

		tp->clogow = clogo_w;
		tp->clogoh = clogo_h;
		tp->clogodata = clogo_data;
	}
#endif

	rp = &tx->rowd[0];
	for (i = TP_MAXROWS; --i >= 0; rp++) {
		rp->dirty = 1;
		rp->maxcol = 0;
		tx->row[i] = rp;
		propbg(tp,i,PBG_LINE);
	}
}


void
txConfig(struct htp_state *tp, int mode)
{
	struct htp_textport *tx = &(tp->textport);

	tx->aflags &= ~(TP_DONEB|TP_WANTDONEB);

	switch (mode) {
	    /*
	     * textport
	     */
	    case TP_MODE:
	    default:
		tp->fontheight		= TPFONT_HT;
		tp->fontdescender	= TPFONT_DC;
		tp->boot_panel_mode	= 0;
		tp->bgnd		= TP_IDX;
		tp->fgnd		= txMaxcontrast(tp->page_r, 
						tp->page_g, tp->page_b); 
		tx->numcols		= TP_MAXCOLS;
		tx->numrows		= TP_MAXROWS;
		tx->aflags		&= ~(CURSOFF|TP_AUTOWRAP);
		tx->aflags |= TP_AUTOWRAP;	/* autowrap by default */

#ifdef _GFXGUI
		cleanGfxGui();
#endif
		break;
	    /*
	     * boot panel
	     */
	    case PNL_MODE:
		tp->fontheight		= BOX_HT;
		tp->fontdescender	= BOX_DC;
		tp->boot_panel_mode	= 1;
		tx->numcols		= PNL_MAXCOLS;
		tx->numrows		= PNL_MAXROWS;
		tp->bgnd		= PNL_IDX;
		tp->fgnd		= txMaxcontrast(tp->page_r, 
						tp->page_g, tp->page_b); 
		tx->aflags		|= CURSOFF|TP_AUTOWRAP;

#ifdef _GFXGUI
		cleanGfxGui();
#endif
		break;

#ifdef _GFXGUI
	    case GUI_MODE:
		txConfig(tp,PNL_MODE);		/* does a cleanGfxGui for us */
		tx->aflags |= TP_NOBG|CURSOFF|TP_AUTOWRAP;
		break;
#endif
	}

	/*  Shrink textport so it fits on the resolution selected.  It
	 * looks real cute at 640x480.
	 */
	while ((tp->fontheight*tx->numrows) >= (tp->yscreensize-TP_YBORDER)) {
	    tx->numrows -= 4;
	}
	while ((FONTWIDTH*tx->numcols) >= (tp->xscreensize-TP_XBORDER)) {
	    tx->numcols -= 8;
	}

	tp->cfgnd = tp->fgnd;
	tp->cbgnd = tp->bgnd;
}

#ifdef _GFXGUI
struct Button;
static jmp_buf *donejmp;

/*ARGSUSED*/
static void
txDone(struct Button *b, __scunsigned_t _htp)
{
	struct htp_state *htp = (struct htp_state *)_htp;

	/*  We assume if you turned the done button on, then you have
	 * done the proper setjmp!
	 */
	cleanGfxGui();			/* remove button */
	drawShadedBackground(htp,0,0,htp->xscreensize-1, htp->yscreensize-1);
	if (donejmp) longjmp(*donejmp,1);
}
#endif

static void
txReset(struct htp_state *tp, int button, int keeptext)
{
	struct htp_textport *tx = &(tp->textport);

#ifdef _GFXGUI
	if (!keeptext && (tp->textport.aflags & TP_NOBG) == 0) {
		drawShadedBackground(tp, 0, 0, tp->xscreensize-1,
				     tp->yscreensize-1);
	}

	if (button)
		tx->aflags |= TP_DONEB;

	if (keeptext) {
		txShdOutline(htp,tx->llx-2, tx->lly-2,
		     tx->llx + XSIZE(tx->numcols) + XBORDER * 2 + 1,
		     tx->lly + tx->numrows*tp->fontheight + YBORDER*2 + 1);
	}
	else
		txInit(tp);

	if (button) {
		struct Button *b;
		int x,y,width;
		struct Button *createButton(int, int, int, int);

		/*  SGI style guide: width is greater of 75 or text label
		 * width + 2 * 10.
		 */
		width = textWidth(donestr,BUTTONFONT) + 2 * 10;
		if (width < TEXTBUTW) width = TEXTBUTW;

		x = tx->llx + XSIZE(tx->numcols) + (2*XBORDER) - width + 1;
		y = tx->lly - (B_WIDTH+TEXTBUTH)/2;

		b = (struct Button *)createButton(x,y,width,TEXTBUTH);
		if (donestr)
			addButtonText(b,donestr);
		if (keeptext)
			drawObject((struct gui_obj *)b);
	        addButtonCallBack(b,txDone,(__scunsigned_t)tp);
	}
#else
	(*tp->fncs->color)(tp->hw, SCR_IDX);
	(*tp->fncs->sboxfi)(tp->hw, 0,0, tp->xscreensize-1,tp->yscreensize-1);
	txInit(tp);
#endif
}

#if defined(TP_FLAT) || !defined(TP_COLOR)
/*
 * Outline the given rectangle, using a filled rectangle draw
 */
static void
txOutline(struct htp_state *tp, int x1, int y1, int x2, int y2)
{
	struct htp_fncs *fncs = tp->fncs;
	(*fncs->sboxfi)(tp->hw, x1, y1, x1, y2);	/* left border */
	(*fncs->sboxfi)(tp->hw, x1, y1, x2, y1);	/* bottom border */
	(*fncs->sboxfi)(tp->hw, x2, y1, x2, y2);	/* right border */
	(*fncs->sboxfi)(tp->hw, x1, y2, x2, y2);	/* top border */
}
#endif

#ifndef _TP_FLAT
/*
 * Paints a shaded border 2*NRAMP+1 pixels wide.
 */
static void
txShdOutline(struct htp_state *tp, int x1, int y1, int x2, int y2)
{
	int bottomy = (tp->textport.aflags & TP_DONEB) ?
		y1-B_BDR+1 : y1-LRT_BDR-1;
	struct htp_fncs *fncs = tp->fncs;
	int ramp[4];
	int i;

#define bd(E,X1,Y1,X2,Y2)						\
	for (i = 0; i < E; i++) {					\
		(*fncs->color)(tp->hw, ramp[i]);			\
		(*fncs->sboxfi)(tp->hw, X1 i, Y1 i, X2 i, Y2 i);	\
	}

	/* thick gray border -- right, left, top, bottom*/
	(*fncs->color)(tp->hw, VeryLightGray);
	(*fncs->sboxfi)(tp->hw,
			x2+IN_WIDTH, y1-IN_WIDTH, x2+LRT_BDR-1, y2+IN_WIDTH);
	(*fncs->sboxfi)(tp->hw,
			x1-LRT_BDR+1, y1-IN_WIDTH, x1-IN_WIDTH, y2+IN_WIDTH);
	(*fncs->sboxfi)(tp->hw,
			x1-LRT_BDR+1, y2+IN_WIDTH, x2+LRT_BDR-1, y2+LRT_BDR-1);
	(*fncs->sboxfi)(tp->hw,
			x1-LRT_BDR+1, bottomy, x2+LRT_BDR-1, y1-IN_WIDTH);

	/* outer border */

	/* bottom and right */
	ramp[0] = LightGray; ramp[1] = MediumGray;
	ramp[2] = DarkGray;  ramp[3] = Black;
	bd(OUT_WIDTH, x1-LRT_BDR-, bottomy-, x2+LRT_BDR+, bottomy-);
	bd(OUT_WIDTH, x2+LRT_BDR+, bottomy-, x2+LRT_BDR+, y2+LRT_BDR+);

	/* top and left */
	ramp[0] = White; ramp[1] = White;
	ramp[2] = White; ramp[3] = Black;
	bd(OUT_WIDTH, x1-LRT_BDR-, y2+LRT_BDR+, x2+LRT_BDR+, y2+LRT_BDR+);
	bd(OUT_WIDTH, x1-LRT_BDR-, bottomy-, x1-LRT_BDR-, y2+LRT_BDR+);

	/* inner border */

	/* bottom and right */
	ramp[0] = DarkGray; ramp[1] = VeryLightGray; ramp[2] = White;
	bd(IN_WIDTH, x1-, y1-, x2+, y1-);
	bd(IN_WIDTH, x2+, y1-, x2+, y2+);

	/* top and left */
	ramp[0] = VeryDarkGray; ramp[1] = DarkGray; ramp[2] = MediumGray;
	bd(IN_WIDTH, x1-, y2+, x2+, y2+);
	bd(IN_WIDTH, x1-, y1-, x1-, y2+);
}
#endif

/*
 * Paint a textport.
 */
static void
txPaint(struct htp_state *tp)
{
	struct htp_fncs *fncs = tp->fncs;
	struct htp_textport *tx = &(tp->textport);
	struct htp_row *rp;
	int x, y;
	int xend, tmpx;
	int yend, ystart;
	int j, i;
	int rows, rowe;

#ifdef _GFXGUI
	if (tx->state == GUI)
		return;
#endif

	x = tx->llx + XBORDER;
	y = tx->lly + YBORDER + (tp->fontheight * tx->numrows);
	xend = x + XSIZE(tx->numcols) - 1;

	for (i = 0; i < tx->numrows; i ++) {
		rp = tx->row[i];
		y -= tp->fontheight;

		if ((tx->state != CLEAN) || rp->dirty) {
			/* keep mouse tracking ok when scrolling */
			_mspoll();

			ystart = y + tp->fontdescender;
			yend = y + tp->fontheight - 1;

#ifdef _TP_COLOR
			/*  Clear line from maxcol to numcol with background
			 * color stored at maxcol.
			 */
			if ((tmpx=rp->maxcol) == tx->numcols) tmpx--;
			(*fncs->color)(tp->hw, IS_RV(rp->data[tmpx]) ?
						     foreground(rp,tmpx) :
						     background(rp,tmpx));
			drawbg(tp,x+XSIZE(rp->maxcol),y, xend,yend);
#else
			(*fncs->color)(tp->hw,background(rp,x));
			drawbg(tp,x,y,xend,yend);
#endif

			/*
			 * Move cmov2i out of the following loop, since
			 * ucode will update the current char position.
			 */
			(*fncs->cmov2i)(tp->hw, x, ystart);

#ifdef _TP_COLOR
			if (rp->dirty != 2 || tx->state != CLEAN) {
				rows = 0;		/* DIRTYLINE */
				rowe = rp->maxcol;
			}
			else {				/* DIRTYCURSOR */
				rows = tx->ccol;
				rowe = rows + 1;
				if (rows == rp->maxcol)
					rp->data[rows] = ' ';
			}
#else
			rows = 0;
			rowe = rp->maxcol;
#endif

			/* Paint characters */
			for (tmpx = x, j = rows; j < rowe; j++) {
				/* Reverse colors if desired */
				if (IS_RV(rp->data[j])) {
					(*fncs->color)(tp->hw,
						       foreground(rp,j));
					drawbg(tp,tmpx, y,
					       tmpx+FONTWIDTH-1, yend);
					(*fncs->color)(tp->hw,
						       background(rp,j));
				} else {
#ifdef _TP_COLOR
					(*fncs->color)(tp->hw,
						       background(rp,j));
					drawbg(tp,tmpx,y,
					       tmpx+FONTWIDTH-1,yend);
#endif
					(*fncs->color)(tp->hw,
						       foreground(rp,j));
				}
				
				/* draw underline */
				if (IS_UL(rp->data[j]))
					txOutcharmap(tp, U_BAR, Special);
				else if (IS_BX(rp->data[j])) {
					txOutcharmap(tp, U_BAR, Special);
					txOutcharmap(tp, O_BAR, Special);
				}
				txOutcharmap(tp, (char)((rp->data[j])&0xff),
						ScrB18);

				tmpx += FONTWIDTH;
			}
		}
		rp->dirty = 0;
	}
	/*
	 * draw the cursor.  This is typically not done in
	 * information only panels.
	 */
	if (((tx->aflags & CURSOFF) == 0) &&
	    ((tx->aflags & TP_AUTOWRAP) || (tx->ccol < tx->numcols))) {
		x = tx->llx + XBORDER + XSIZE(tx->ccol);
		y = tx->lly + YBORDER
		  + (tp->fontheight*(tx->numrows - tx->crow - 1));
		(*fncs->color)(tp->hw, CURSOR_COLOR);
		(*fncs->sboxfi)(tp->hw, x, y,
				x + FONTWIDTH - 1, y + tp->fontheight - 1);

		/* draw character under cursor */
		rp = tx->row[tx->crow];
		if (rp->maxcol > tx->ccol) {
			(*fncs->color)(tp->hw, foreground(rp,tx->ccol));
			(*fncs->cmov2i)(tp->hw, x, y+tp->fontdescender);
			txOutcharmap(tp, rp->data[tx->ccol]&0xff, ScrB18);
		}
	}

	/* draw borders */
	if (tx->state == DIRTY) {
		/* compute lengths of borders */
		x = XSIZE(tx->numcols) + XBORDER * 2;
		y = tx->numrows * tp->fontheight + YBORDER * 2;
#ifdef _TP_FLAT		
		(*fncs->color)(tp->hw, BORDER_COLOR);
		txOutline(tp, tx->llx-2, tx->lly-2, 
			  tx->llx + x +1, tx->lly + y +1);
		(*fncs->color)(tp->hw, INR_BORDER);
#else
		txShdOutline(tp, tx->llx-2, tx->lly-2,
			     tx->llx + x + 1, tx->lly + y + 1);
		(*fncs->color)(tp->hw, tp->bgnd);
#endif
#ifndef _TP_COLOR	/* color code fills in outlines */
		txOutline(tp, tx->llx-1, tx->lly-1, 
			  tx->llx + x, tx->lly + y);
		(*fncs->color)(tp->hw, tp->bgnd);
		txOutline(tp, tx->llx, tx->lly, 
			  tx->llx + x - 1, tx->lly + y - 1);
		txOutline(tp, tx->llx + 1, tx->lly + 1,
			  tx->llx + x - 2, tx->lly + y - 2);
#endif
#ifdef _GFXGUI
		if (tx->aflags & TP_DONEB)	/* pop done button up */
			guiRefresh();
#endif
	}
	tx->state = CLEAN;
}

/*
 * reverse scroll
 */
static void
txRscroll(struct htp_state *tp, int start, int end)
{
	struct htp_textport *tx = &(tp->textport);
	struct htp_row *rp;
	int i;

	rp = tx->row[end];
	for (i = end; i > start; i--) 
	{
		tx->row[i] = tx->row[i-1];
		tx->row[i]->dirty = 1;
	}
	tx->row[i] = rp;
	propbg(tp,i,PBG_LINE);

	tx->row[i]->dirty  = 1;
	tx->row[i]->maxcol = 0;

}

/*
 * Forward scroll
 */
static void
txFscroll(struct htp_state *tp, int start, int end)
{
	struct htp_textport *tx = &(tp->textport);
	void (*bm)(void*, int, int, int, int, int, int) = (*tp->fncs->blockm);
	struct htp_row *rp;
	int i;

	rp = tx->row[start];

#ifndef _TP_OLDSCROLL
	if (bm) {
		int ybase = tx->lly + YBORDER +
			(tp->fontheight * (tx->numrows-(end+1)));

		(*bm)(tp->hw,
		      tx->llx-1, ybase,
		      tx->llx-1, ybase+tp->fontheight,
		      2*XBORDER + XSIZE(tx->numcols)+1,
		      (end-start)*tp->fontheight-1);

#ifdef _TP_COLOR
		/* redraw first line if it is 0 to update border background.
		 */
		if (start == 0) DIRTYLINE(tx,1);
#endif
		/* redraw the last line scrolled to erase cursor
		 */
		DIRTYLINE(tx,end);
	}
#endif

	for (i = start; i < end; i++) {
		tx->row[i] = tx->row[i+1];
#ifndef _TP_OLDSCROLL
		if (!bm)
#endif
			tx->row[i]->dirty = 1;
	}

	tx->row[i] = rp;
	propbg(tp,i,PBG_LINE);

	tx->row[i]->dirty  = 1;
	tx->row[i]->maxcol = 0;
}


/*
 * Process a newline motion
 */
static void
txNewline(struct htp_state *tp)
{
	struct htp_textport *tx = &(tp->textport);

	if (++tx->crow >= tx->numrows) 
	{
		/* scroll all lines up one row */
		txFscroll(tp,0,tx->numrows-1);
		tx->crow = tx->numrows - 1;
	}
	else if (!(tx->aflags & CURSOFF))
		DIRTYCURSOR(tx, tx->crow-1);
}



 /*
  * Put character "c" into the textport
  */
static void
txStuff(struct htp_state *tp, unsigned char c)
{
	struct htp_fncs *fncs = tp->fncs;
	struct htp_textport *tx = &(tp->textport);
	struct htp_row *rp;

	rp = tx->row[tx->crow];
	if (tx->ccol > rp->maxcol) {
		register unsigned short *cp;

		/*
		 * Character is past the known end of line.  Fill in
		 * from known end to character column with spaces.
		 */
		cp = &rp->data[rp->maxcol];
		while (cp < &rp->data[tx->ccol])
			*cp++ = ' ';
	}

	if (((tx->aflags & TP_AUTOWRAP) == 0) && (tx->ccol >= tx->numcols))
		return;

	rp->data[tx->ccol] = c | tx->attribs;
#ifdef _TP_COLOR
	rp->color[tx->ccol] = tp->cfgnd | (tp->cbgnd<<4);
#endif

	/*
	 * If everything else is up to date, then just draw the
	 * character.	Otherwise the character will be drawn when
	 * txPaint redraws the whole line.
	 */

	if (!rp->dirty && tx->state == CLEAN) {
		int x,y;
		x = tx->llx + XBORDER + XSIZE(tx->ccol);
		y = tx->lly + YBORDER + 
			(tp->fontheight*(tx->numrows - tx->crow - 1));

		if (IS_RV(rp->data[tx->ccol])) {
		    (*fncs->color)(tp->hw, foreground(rp,tx->ccol));
		    drawbg(tp, x, y, x+FONTWIDTH-1, y+tp->fontheight-1);

		    (*fncs->cmov2i)(tp->hw, x, y + tp->fontdescender);
		    (*fncs->color)(tp->hw, background(rp,tx->ccol));
		}
		else {
		    /* erase cursor */
		    (*fncs->color)(tp->hw, background(rp,tx->ccol));
		    drawbg(tp, x, y, x+FONTWIDTH-1, y + tp->fontheight-1);

		   (*fncs->cmov2i)(tp->hw, x, y + tp->fontdescender);
		   (*fncs->color)(tp->hw, foreground(rp,tx->ccol));
		}

		if (IS_UL(rp->data[tx->ccol]))
			txOutcharmap(tp, U_BAR, Special);
		else if (IS_BX(rp->data[tx->ccol])) {
			txOutcharmap(tp, U_BAR, Special);
			txOutcharmap(tp, O_BAR, Special);
		}

		txOutcharmap(tp, (char)((rp->data[tx->ccol])&0xff), ScrB18);
	}

	/* update line length limiter */
	if (++tx->ccol > rp->maxcol)
		rp->maxcol = tx->ccol;

	if ((tx->aflags & TP_AUTOWRAP) && (tx->ccol >= tx->numcols)) {
		tx->ccol = 0;
		txNewline(tp);
	}
}

/*
 * Get ith parameter value
 */
static int
txGetAparamv(struct htp_state *tp, int i)
{
	struct htp_textport *tx = &(tp->textport);

	if (i > tx->aparamc)
		return 0;
	else
		return tx->aparams[i];
}

/*
 * Cursor position - one based (1,1) is upper left hand corner
 */
static void
txAcup(struct htp_state *tp, int row, int col)
{
	struct htp_textport *tx = &(tp->textport);

	if (!(tx->aflags & CURSOFF))
		DIRTYLINE(tx,tx->crow);

	if (row <= 0)
		row = 1;
	if (row > tx->numrows)
		row = tx->numrows;
	if (col <= 0)
		col = 1;
	if (col > tx->numcols)
		col = tx->numcols;
	
	tx->crow = row-1;
	tx->ccol = col-1;
}

/*
 * Handle a Control Sequence - Esc [ <param> ; ... <char>
 */
static void
txCsistuff(struct htp_state *tp, unsigned char c)
{
	struct htp_textport *tx;
	int i;
#ifdef _TP_COLOR
	int arg;
#endif

	tx = &(tp->textport);

	switch (c) {
		case ' ':
			/* Font selection: "CSIn;m D".  Postpone action
			 * till D arrives by bumping state.
			 */
			tx->astate = CSICONT;
			return;
		case 'm': /* sgr - set graphics rendition */
			for (i = 0; i <= tx->aparamc; i++) {
#ifdef _TP_COLOR
				switch(arg=txGetAparamv(tp, i)) {
#else
				switch(txGetAparamv(tp, i)) {
#endif
					case 0:
						tx->attribs = 0;
						tp->cbgnd = tp->bgnd;
						tp->cfgnd = tp->fgnd;
						break;
					case 1:
						tx->attribs |= AT_HL;
						break;
					case 4:
						tx->attribs |= AT_UL;
						break;
					case 7:
						tx->attribs |= AT_RV;
						break;
					}
#ifdef _TP_COLOR
				/* Change fgnd/bgnd color.
				 */
				if ((arg>=30)&&(arg<=37)) {	/* fgnd */
					tp->cfgnd = arg-30;
				}
				else if ((arg>=40)&&(arg<=47)) {/* bgnd */
					tp->cbgnd = arg - 40;
				}
#endif
			}
			break;
		case 'A': /* cursor up */
			if ((i=txGetAparamv(tp,0)) <= 0) i = 1;
			DIRTYLINE(tx,tx->crow);
			if ((tx->crow -= i) < 0)
				tx->crow = 0;
			break;
		case 'B': /* cursor down */
			if ((i=txGetAparamv(tp,0)) <= 0) i = 1;
			DIRTYLINE(tx,tx->crow);
			if ((tx->crow += i) >= tx->numrows)
				tx->crow = tx->numrows-1;
			break;
		case 'C': /* cursor forward */
			if ((i=txGetAparamv(tp,0)) <= 0) i = 1;
			if ((tx->ccol += i) >= tx->numcols)
				tx->ccol = tx->numcols-1;
			DIRTYLINE(tx,tx->crow);
			break;
		case 'D': /* cursor back and font selection */
			/* Ignore font selection */
			if (tx->astate == CSICONT) break;

			if ((i=txGetAparamv(tp,0)) <= 0) i = 1;
			if ((tx->ccol -= i) < 0)
				tx->ccol = 0;
			DIRTYLINE(tx,tx->crow);
			break;
		case 'H': /* cup - cursor position */
			txAcup(tp, txGetAparamv(tp, 0), 
					txGetAparamv(tp, 1));
			break;
		case 'M': /* dl - delete line */
			txFscroll(tp,tx->crow,tx->numrows-1);
			break;
		case 'L': /* il - insert line */
			txRscroll(tp,tx->crow,tx->numrows-1);
			break;
		case 'l':
			for (i = 0; i <= tx->aparamc; i++)
				switch (txGetAparamv(tp, i)) {
					case 25:
						tx->aflags |=  CURSOFF;
						DIRTYLINE(tx,tx->crow);
						break;
					/* autowrap off */
					case 37:
						tx->aflags &= TP_AUTOWRAP;
						break;

					}
			break;
		case 'h':
			for (i = 0; i <= tx->aparamc; i++) {
				int button = (tx->aflags & TP_WANTDONEB);
				switch (txGetAparamv(tp, i)) {
					case 25:
						tx->aflags &= ~CURSOFF;
						break;
					case 32:
						txConfig(tp, TP_MODE);
						txReset(tp,button,0);
						break;
					case 33:
					case 34:
						txConfig(tp, PNL_MODE);
						txReset(tp,button,0);
						break;
#ifdef _GFXGUI
					case 35:
						txConfig(tp, GUI_MODE);
						txReset(tp,0,0);
						tx->state = GUI;
						break;
#endif
					/* autowrap on */
					case 37:
						tx->aflags |= TP_AUTOWRAP;
						break;
#ifdef _GFXGUI
					/* want button on at mode change */
					case 39:
						tx->aflags |= TP_WANTDONEB;
						break;
					/*  want button now (does not recenter
					 * the textport).
					 */
					case 40:
						txReset(tp,1,1);
						break;
#endif
					}
			}
			break;
		case 'J': /* eis - clear screen */
			switch (txGetAparamv(tp, 0)) {
				case 0:	/* clear end of screen */
					tx->row[tx->crow]->maxcol = tx->ccol;
					propbg(tp,tx->crow,PBG_EOL);
					DIRTYLINE(tx,tx->crow);
					for (i=tx->crow+1;i<tx->numrows; i++) {
						tx->row[i]->maxcol = 0;
						propbg(tp,i,PBG_LINE);
						DIRTYLINE(tx,i);
					}
					break;

				case 1:		/* clear to home */
					for (i=0; i <= tx->ccol; i++)
						tx->row[tx->crow]->data[i] =
							' ';
					propbg(tp,tx->crow,PBG_BOL);
					DIRTYLINE(tx,tx->crow);
					for (i=0; i < tx->crow; i++) {
						tx->row[i]->maxcol = 0;
						propbg(tp,i,PBG_LINE);
						DIRTYLINE(tx,i);
					}
					break;

				case 2:		/* clear whole screen */
					for (i = 0; i < TP_MAXROWS; i++) {
						tx->rowd[i].maxcol = 0;
						propbg(tp,i,PBG_LINE);
					}
					tx->state = DIRTY;
					tx->crow=tx->ccol=0; /* move to home */
				}
			break;
		case 'K': /* eil - erase in line */
			switch (txGetAparamv(tp, 0)) {
			case 1:		/* erase to beginning of line */
				for (i=0; i<=tx->ccol; i++)
					tx->row[tx->crow]->data[i] = ' ';
				propbg(tp,tx->crow,PBG_BOL);
				DIRTYLINE(tx,tx->crow);
				break;

			case 2:		/* whole line, move to column 0 */
				tx->ccol = 0;
				/*FALLSTHROUGH*/
			case 0:	/* erase to end of line */
				if (tx->ccol < tx->row[tx->crow]->maxcol) {
					tx->row[tx->crow]->maxcol = tx->ccol;
					propbg(tp,tx->crow,PBG_EOL);
					DIRTYLINE(tx,tx->crow);
					}
				break;
			}
			break;
		}

	for (i = 0; i < MAXAPARAMS; i++)
		tx->aparams[i] = 0;
	tx->aparamc = 0;

	tx->astate = NORM;

	return;
}

/*
 * reverse linefeed
 */
static void
txRindex(struct htp_state *tp)
{
	struct htp_textport *tx;

	tx = &(tp->textport);
	DIRTYLINE(tx,tx->crow);
	if (tx->crow > 0)
		tx->crow--;
	else
		txRscroll(tp,0,tx->numrows-1);
}


/*
 * Handle a simple escape sequence: Esc <char>
 */
static void
txEscstuff(struct htp_state *tp, unsigned char c)
{
	struct htp_textport *tx;

	tx = &(tp->textport);
	switch (c) {
		case 'D': /* ind - forward index */
			txNewline(tp);
			break;
		case 'M': /* rind - reverse index */
			txRindex(tp);
			break;
		}
	
	tx->astate = NORM;

	return;
}

/*
 * Handle a control character
 */
static void
txCtrlstuff(struct htp_state *tp, unsigned char c)
{
	struct htp_textport *tx;

	tx = &(tp->textport);
	switch (c) {
	  case '\b':				/* backspace */
		if (tx->ccol) {
			tx->ccol -= 1;
			tx->row[tx->crow]->maxcol = tx->ccol;
#ifdef _TP_COLOR
			if ((tx->ccol+1) < tx->numcols)
				tx->row[tx->crow]->color[tx->ccol] =
					tx->row[tx->crow]->color[tx->ccol+1];
#endif
			DIRTYLINE(tx, tx->crow);
		}
		break;
	  case '\007':
		bell();
		break;
	  case '\013':			/* ARCS: VT/FF -> LF */
	  case '\014':
	  case '\n':				/* new line */
		txNewline(tp);
		/* FALL THROUGH--  this is our sleazy hack to add CR */
	  case '\r':				/* carriage return */
		tx->ccol = 0;
		break;
	  case '\t':				/* horizontal tab */
		tx->ccol += (8 - (tx->ccol & 7));
		if (tx->ccol >= tx->numcols) {
			if (tx->aflags & TP_AUTOWRAP) {
				tx->ccol = 0;
				txNewline(tp);
			}
			else {
				tx->ccol = tx->numcols;
				DIRTYLINE(tx,tx->crow);
			}
		} else
			DIRTYLINE(tx, tx->crow);
		break;
	  case L_BRAK:
		if (tp->boot_panel_mode) {
			txStuff(tp, c);
			tx->attribs |= AT_BX;
		}
		break;
	  case R_BRAK:
		if (tp->boot_panel_mode) {
			tx->attribs &= ~AT_BX;
			txStuff(tp, c);
		}
		break;
	  default:
		break;
	}
}

void
initFonts(void)
{
	htp->fonts = fonts;

#ifdef _TP_OLDFONT
	fonts[ScrB18].data = iris10iData;
	fonts[ScrB18].info = iris10iInfo;
	fonts[ScrB18].height = iris10i_ht;
	fonts[ScrB18].descender = iris10i_ds;
#else
	fonts[ScrB18].data = ScrB18Data;
	fonts[ScrB18].info = ScrB18Info;
	fonts[ScrB18].height = ScrB18_ht;
	fonts[ScrB18].descender = ScrB18_ds;
#endif

	fonts[Special].data = tpsdata;
	fonts[Special].info = tpspecial;
	fonts[Special].height = special_ht;
	fonts[Special].descender = special_ds;

#ifdef _GFXGUI
	fonts[ncenB18].data = ncenB18Data;
	fonts[ncenB18].info = ncenB18Info;
	fonts[ncenB18].height = ncenB18_ht;
	fonts[ncenB18].descender = ncenB18_ds;

	fonts[helvR10].data = helvR10Data;
	fonts[helvR10].info = helvR10Info;
	fonts[helvR10].height = helvR10_ht;
	fonts[helvR10].descender = helvR10_ds;
#endif
}

void
htp_init(void)
{
	extern int _prom;

#ifdef _GFXGUI
	if (_prom) {
		initGfxGui(htp);
		txConfig(htp,GUI_MODE);
	}
#else
	if (_prom)
		txConfig(htp, PNL_MODE);
#endif
	else
		txConfig(htp, TP_MODE);

	initFonts();

	txReset(htp,0,0);
#ifdef _GFXGUI
	if (_prom) {
		htp->textport.state = GUI;
		drawShadedBackground(htp,0,0,htp->xscreensize-1,
				     htp->yscreensize-1);
	}
#endif
	txPaint(htp);

	htp->textport.state = VOFF;		/* video is still off! */
}

int gfx_testing;				/* used by ide */
int
htp_write(char *buf, int len)
{
	struct htp_textport *tx;
	struct htp_fncs *fncs = htp->fncs;
	unsigned char c;
	unsigned char *cp = (unsigned char *)buf;

	/*  If gfx_testing is set, we should be doing all terminal i/o
	 * with buffered msg_printf(), but we can trip on a _errputs()
	 * or printf() in one libraries (like re-plugging the keyboard)
	 * which can hang graphics.
	 */
	if (fncs && !gfx_testing) {
		tx = &(htp->textport);

		/* if the video is off, turn it on */
		if (tx->state == VOFF) {
			(*fncs->blankscreen)(htp->hw,0);
			htp->textport.state = DIRTY;
		}
#ifdef _GFXGUI
		else if (tx->state == GUI && *buf != '\033') {
			/*  if textport is dirty on a write, then textport
			 * is popping up.  Get rid of graphics stuff.
			 */
			cleanGfxGui();
			txReset(htp,(tx->aflags & TP_WANTDONEB),0);
			tx->aflags &= ~TP_WANTDONEB;
		}
#endif

		while (--len >= 0) {
			if (!(c = *cp++))	/* skip nulls */
				continue;
			switch(tx->astate) {
			case NORM:
				if ((c >= 0x20 && c <= 0x7f) || c >= 0xa0)
					txStuff(htp, c);
				else if (c == 0x1b)
					tx->astate = ESC;
				else if (c == 0x9b)
					tx->astate = CSI;
				else
					txCtrlstuff(htp, c);
				break;
			case ESC:
				if (c == '[')
					tx->astate = CSI;
				else
					txEscstuff(htp, c);
				break;
			case CSI:
			case CSICONT:
				if (c >= '0' && c <= '9')
					tx->aparams[tx->aparamc] =
					   tx->aparams[tx->aparamc]*10+(c-'0');
				else
					if (c == ';')
						tx->aparamc++;
					else
						txCsistuff(htp, c);
				break;
			}
		}
		txPaint(htp);
		return 1;
	}
	return 0;
}

#ifdef _TP_COLOR
static void bset(char *p, char c, int i)
{
	while (i--) *p++ = c;
}
#endif

#ifdef _TP_COLOR
/* propegate current background color.  Used when clearing regions.
 */
static void
propbg(struct htp_state *tp, int row, int type)
{
	register struct htp_textport *tx = &tp->textport;
	unsigned char *p = tx->row[row]->color;
	int n = 0;

	switch (type) {
	case PBG_LINE:
		n = tx->numcols;
		break;
	case PBG_EOL:
		p = &p[tx->row[row]->maxcol];
		n = tx->numrows - tx->ccol;
		break;
	case PBG_BOL:
		n = tx->ccol+1;
		break;
	}

	if (n) bset((char *)p, tp->cbgnd<<4|tp->cbgnd,n);
}

/* Check for propegation to [XY]BORDER area before sboxfi().
 */
static void
drawbg(struct htp_state *tp, int x, int y, int xend, int yend)
{
	struct htp_textport *tx = &(tp->textport);
	int rux = tx->llx + XSIZE(tx->numcols) + XBORDER - 1;
	int ruy = tx->lly + (tx->numrows * tp->fontheight) + YBORDER - 1;

	if (x <= (tx->llx + XBORDER)) x = tx->llx-1;
	if (y <= (tx->lly + YBORDER)) y = tx->lly-1;
	if (xend >= rux) xend = rux+XBORDER+1;
	if (yend >= ruy) yend = ruy+YBORDER+1;

	(*tp->fncs->sboxfi)(tp->hw,x,y,xend,yend);
}
#endif

int
getHtpPs(void)
{
	return(htp->textport.numrows);
}

/*ARGSUSED*/
void
txSetJumpbuf(jmp_buf *newjmp)
{
#ifdef _GFXGUI
	donejmp = newjmp;
#endif
}

/*ARGSUSED*/
void
txSetString(char *newstr)
{
#ifdef _GFXGUI
	donestr = newstr;
#endif
}

struct htp_state *
gethtp(void)
{
	return(htp);
}

/* tnDBG() replaced by ttyprintf(fmt,...) */
