/*
* Copyright 1998 - PsiTech, Inc.
* THIS DOCUMENT CONTAINS CONFIDENTIAL AND PROPRIETARY
* INFORMATION OF PSITECH.  Its receipt or possession does
* not create any right to reproduce, disclose its contents,
* or to manufacture, use or sell anything it may describe.
* Reproduction, disclosure, or use without specific written
* authorization of Psitech is strictly prohibited.
*
*/
#include "sys/types.h"
#include "sys/systm.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/cpu.h"
#include <sys/ksynch.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/PCI_defs.h>

#include "rad1hw.h"
#include "rad1.h"
#include "rad4pci.h"
#include "rad1sgidrv.h"
#include "rad1tp.h"

#if defined(_STANDALONE)
#include "stand_htport.h"
#define pon_puts(msg) /* ttyprintf(msg) */ /* this prints to the tty always */
#else
#include "sys/htport.h"
#define pon_puts(msg)	/* cmn_err(CE_DEBUG, msg) /* */
extern int * rad1_earlyinit_base;
#endif /* STANDALONE */

static int cpos_x;
static int cpos_y;
extern rad1boards RAD1Boards[];
static int lut_flag;
static int lut_shadow[MAX_LUT_ENTRIES];
static void RAD1TpBlankscreen(void *base_in, int blank);
extern volatile int vol;

static void
RAD1TpInit(void *base_in)
{
	per2_info *p2;
	int idx;

	pon_puts("!Entering RAD1TpInit\n\r");
	if (!(p2 = *((per2_info **)base_in)))
		return;

	ASSERT(p2);
	idx = RAD1_baseindex(p2);
	ASSERT(idx >= 0);

	/* reset and re-initialize board */
	if (!RAD1Boards[idx].boardInitted)
		RAD1InitBoard(p2);

	/* Enable Display */
	RAD1TpBlankscreen(base_in, 0);

	pon_puts("!Leaving RAD1TpInit\n\r");
}

static void 
RAD1TpMapcolor(void *base_in, int index, int r, int g, int b)
{
	lut_shadow[index] = (r << 16) | (g << 8) | b;
	
	pon_puts("!Leaving RAD1TpMapcolor\n\r"); /**/
}

static void 
RAD1TpColor(void *base_in, int index)
{
	lut_flag = lut_shadow[index];

	pon_puts("!Leaving RAD1TpColor\n\r"); /**/
}

static void 
RAD1TpSboxfi(void *base_in, int x1, int y1, int x2, int y2)
{
	per2_info *p2;
	volatile int *registers;
	int x, y, w, h, screen_w, screen_h;

	pon_puts("!Entering RAD1TpSboxfi\n\r"); /**/
	if (!(p2 = *((per2_info **)base_in)))
	{
#if !defined(_STANDALONE)
		int *dest_base, *dest;
		int i;
		if(rad1_earlyinit_base == 0)
		   return;
		screen_w = 1280;
		screen_h = 1024;
		dest_base = rad1_earlyinit_base;
		if (x1 > x2)
		{
			x = x2;
			w = x1 - x2 + 1;
		}
		else
		{
			x = x1;
			w = x2 - x1 + 1;
		}
		if (y1 > y2)
		{
			y = screen_h - y1;
			h = y1 - y2 + 1;
		}
		else
		{
			y = screen_h - y2;
			h = y2 - y1 + 1;
		}
		dest_base += y * screen_w + x;
		while (h--)
		{
			dest = dest_base;
			for(i = 0; i < w; i++)
			{
				*dest = lut_flag;
				dest++;
			}
			dest_base += screen_w;
		}


#else /* !defined(_STANDALONE) */
		return;
#endif /* !defined(_STANDALONE) */
	}
	else
	{
		registers = p2->registers;
	
		while (*(registers + INFIFOSPACE_REG/4) < 6)
			vol = 0;	/* wait for room in register FIFO */
#if !defined(_STANDALONE)
#if 1
		if(SLEEP_TRYLOCK(&p2->hardware_mutex_lock) == FALSE) return;
#else
		SLP_LOCK_LOCK(&p2->hardware_mutex_lock);  /* lock the hardware */
#endif
#endif /* !defined(_STANDALONE) */
		p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
		p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_ENABLE;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
		*(registers + FBWRITEDATA_REG/4) = lut_flag;
		if (x1 > x2)
		{
			x = x2;
			w = x1 - x2 + 1;
		}
		else
		{
			x = x1;
			w = x2 - x1 + 1;
		}
		if (y1 > y2)
		{
			y = p2->v_info.y_size - y1;
			h = y1 - y2 + 1;
		}
		else
		{
			y = p2->v_info.y_size - y2;
			h = y2 - y1 + 1;
		}

		*(registers + RECTANGLEORIGIN_REG/4) = RECTORIGIN_YX(y, x);
		*(registers + RECTANGLESIZE_REG/4) = RECTSIZE_HW(h, w);
		*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_RECTANGLE |
				RENDER_INCREASEX | RENDER_INCREASEY;

		p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_DISABLE;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
#if !defined(_STANDALONE)
		SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);  /* release the hardware */
#endif /*!defined(_STANDALONE) */
	}
	
	pon_puts("!Leaving RAD1TpSboxfi\n\r"); /**/
}

static void 
RAD1TpPnt2i(void *base_in, int x, int y)
{
	per2_info *p2;
	volatile int *registers;
	int screen_w, screen_h;

	pon_puts("!Entering RAD1TpPnt2i\n\r"); /**/
	if (!(p2 = *((per2_info **)base_in)))
	{
#if !defined(_STANDALONE) 
		if(rad1_earlyinit_base == 0)
			return;
		screen_w = 1280;
		screen_h = 1024;
		*(rad1_earlyinit_base +
				(((screen_h - (cpos_y - y)) * screen_w) + cpos_x - x)) = lut_flag;
		return;
#else /* !defined(_STANDALONE) */
		return;
#endif /* !defined(_STANDALONE) */
	}
	else
	{
		registers = p2->registers;
	
		/* this places a point on screen in color set by RAD1TpColor */
		while (*(registers + INFIFOSPACE_REG/4) < 6)
			vol = 0;	/* wait for room in register FIFO */
#if !defined(_STANDALONE) 
#if 1
		if(SLEEP_TRYLOCK(&p2->hardware_mutex_lock) == FALSE) return;
#else
		SLP_LOCK_LOCK(&p2->hardware_mutex_lock);  /* lock the hardware */
#endif
#endif /*!defined(_STANDALONE) */
		p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
		p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_ENABLE;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
		*(registers + FBWRITEDATA_REG/4) = lut_flag;
		*(registers + STARTY_REG/4) = INTtoFIXED16_16(p2->v_info.y_size - y);
		*(registers + STARTXDOM_REG/4) = INTtoFIXED16_16(x);
		*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_POINT;

		p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_DISABLE;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
#if !defined(_STANDALONE) 
		SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);  /* release the hardware */
#endif /*!defined(_STANDALONE) */
	}
	pon_puts("!Leaving RAD1TpPnt2i\n\r"); /**/
}

static void 
RAD1TpDrawbitmap(void *base_in, struct htp_bitmap *bitmap)
{
	per2_info *p2;
	volatile int *registers;
	int xsize, ysize, shortsRemaining;
	unsigned short *bufptr = bitmap->buf;
	unsigned short pattern, mask;
	int y, x;

	pon_puts("!Entering RAD1TpDrawbitmap\n\r"); /**/
	if (!(p2 = *((per2_info **)base_in)))
	{
#if !defined(_STANDALONE) 
		int *dest_base, *dest;
		int screen_w, screen_h;
		if(rad1_earlyinit_base == 0)
			return;
		screen_w = 1280;
		screen_h = 1024;
		dest_base = rad1_earlyinit_base;
		dest_base +=
		(((screen_h - (cpos_y - bitmap->yorig)) * screen_w)
			+ cpos_x - bitmap->xorig);
		ysize = bitmap->ysize;
		while (ysize--)
		{
			dest = dest_base;
			xsize = bitmap->xsize;
			shortsRemaining = bitmap->sper;
			while (shortsRemaining-- && xsize)
			{
				mask = 0x8000;
				pattern = *(bufptr++);
				while (mask && xsize)
				{
					if (mask & pattern)
					{
						*dest = lut_flag;
					}
					mask >>= 1;
					xsize--;
					dest++;
				}
			}
			dest_base -= screen_w;  /* back up one line */

		}
#else /* !defined(_STANDALONE) */
		return;
#endif /* !defined(_STANDALONE) */
	}
	else
	{
		registers = p2->registers;

		y = p2->v_info.y_size - (cpos_y - bitmap->yorig);
		ysize = bitmap->ysize;
		
		while (*(registers + INFIFOSPACE_REG/4) < 3)
			vol = 0;	/* wait for room in register FIFO */
#if !defined(_STANDALONE)
#if 1
		if(SLEEP_TRYLOCK(&p2->hardware_mutex_lock) == FALSE) return;
#else
		SLP_LOCK_LOCK(&p2->hardware_mutex_lock);  /* lock the hardware */
#endif
#endif /* !defined(_STANDALONE)  */
		p2->p2_logicalopmode.bits.logicalOpEnable = RAD1_DISABLE;
		p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_ENABLE;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
		*(registers + FBWRITEDATA_REG/4) = lut_flag;

		while (ysize--)
		{
			*(registers + STARTY_REG/4) = INTtoFIXED16_16(y--);
			x = cpos_x - bitmap->xorig;
			xsize = bitmap->xsize;
			shortsRemaining = bitmap->sper;
			while (shortsRemaining-- && xsize)
			{
				mask = 0x8000;
				pattern = *(bufptr++);
				while (mask && xsize)
				{
					if (mask & pattern)
					{
						while (*(registers + INFIFOSPACE_REG/4) < 3)
							vol = 0;	/* wait for room in register FIFO */
						*(registers + STARTXDOM_REG/4) = INTtoFIXED16_16(x);
						*(registers + RENDER_REG/4) = RENDER_PRIMITIVE_POINT;
					}
					mask >>= 1;
					xsize--;
					x++;
				}
			}
		}

		p2->p2_logicalopmode.bits.useConstantFBWriteData = RAD1_DISABLE;
		*(registers + LOGICALOPMODE_REG/4) = p2->p2_logicalopmode.all;
#if !defined(_STANDALONE)
		SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);  /* release the hardware */
#endif /* !defined(_STANDALONE)  */
	}
	cpos_x += bitmap->xmove;
	cpos_y += bitmap->ymove;

	pon_puts("!Leaving RAD1TpDrawbitmap\n\r"); /**/
}

#ifdef CAN_BLOCK_MOVE
static void 
RAD1TpBlockM(void *base_in, int, int, int, int, int, int)
{
	per2_info *p2;
	volatile int *registers;
	unsigned int render_cmd = RENDER_PRIMITIVE_RECTANGLE | RENDER_INCREASEX | RENDER_INCREASEY;

	pon_puts("!Entering RAD1TpBlockM\n\r"); /**/
	if (!(p2 = *((per2_info **)base_in)))
		return;
	registers = p2->registers;

	while (*(registers + INFIFOSPACE_REG/4) < 5)
		vol = 0;	/* wait for room in register FIFO */
	p2->p2_fbreadmode.bits.readSourceEnable = RAD1_ENABLE;
	p2->p2_fbreadmode.bits.readDestinationEnable = RAD1_DISABLE;
	if (rw->offsetx & 1)
	{
		if (rw->offsetx > 0)
			p2->p2_fbreadmode.bits.relativeOffset = 1;
		else
			p2->p2_fbreadmode.bits.relativeOffset = -1;
	}
	if ((rw->offsety > 0) && (rw->offsetx < rw->sizex) && (rw->offsety < rw->sizey))
	{
		render_cmd = RENDER_PRIMITIVE_RECTANGLE | RENDER_INCREASEX;
	}
	else if ((rw->offsety == 0) && (rw->offsetx > 0) && (rw->offsetx < rw->sizex))
	{
		render_cmd = RENDER_PRIMITIVE_RECTANGLE | RENDER_INCREASEY;
	}
	
	*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
	*(registers + FBSOURCEDELTA_REG/4) = SOURCEDELTA_YX(TTY_FONT_HEIGHT, 0);
	/* tell GC to copy rectangle */
	*(registers + RECTANGLEORIGIN_REG/4) = RECTORIGIN_YX(y1, x1);
		/* destination rectangle */
	*(registers + RECTANGLESIZE_REG/4) =
		RECTSIZE_HW((max_tty_row - 1) * TTY_FONT_HEIGHT, max_tty_column * TTY_FONT_WIDTH);
	*(registers + RENDER_REG/4) = render_cmd;

	p2->p2_fbreadmode.bits.readSourceEnable = RAD1_DISABLE;
	*(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;

	pon_puts("!Leaving RAD1TpBlockM\n\r"); /**/
}
#endif /* CAN_BLOCK_MOVE */

/*ARGSUSED3*/
static void
RAD1TpCmov2i (void *base_in, int x, int y)
{
	pon_puts("!Entering RAD1TpCmov2i\n\r"); /**/
	
	cpos_x = x;
	cpos_y = y;

	pon_puts("!Leaving RAD1TpCmov2i\n\r"); /**/
}

static void
RAD1TpBlankscreen(void *base_in, int blank)
{
	per2_info *p2;
	volatile int *registers;

	pon_puts("!Entering RAD1TpBlankscreen\n\r"); /* */
	if (!(p2 = *((per2_info **)base_in)))
		return;
	registers = p2->registers;

	/* Enable/Disable Display */
	if (blank)
		p2->p2_rddaccontrol_2v.bits.DACPowerCtl = LOWPOWER_2V;
	else
		p2->p2_rddaccontrol_2v.bits.DACPowerCtl = NORMAL_OPERATION_2V;
	WR_INDEXED_REGISTER_2V(registers,
		RDDACCONTROL_2V_REG, p2->p2_rddaccontrol_2v.all);

	pon_puts("!Leaving RAD1TpBlankscreen\n\r"); /**/
}


#ifdef _STANDALONE
static void
RAD1TpMovec(void *base_in, int x, int y)
{
	per2_info *p2;
	volatile int *registers;

	pon_puts("!Entering RAD1TpMovec\n\r"); /**/
	if (!(p2 = *((per2_info **)base_in)))
		return;
	registers = p2->registers;

	WR_INDEXED_REGISTER_2V(registers, RDCURSORXLOW_2V_REG, x & 0xFF);
	WR_INDEXED_REGISTER_2V(registers, RDCURSORXHIGH_2V_REG, x >> 8);
	WR_INDEXED_REGISTER_2V(registers, RDCURSORYLOW_2V_REG, y & 0xFF);
	WR_INDEXED_REGISTER_2V(registers, RDCURSORYHIGH_2V_REG, y >> 8);

	pon_puts("!Leaving RAD1TpMovec\n\r"); /**/
}

static void
RAD1PROMTpInit(void *base_in, int *x, int *y)
{
	per2_info *p2;

	pon_puts("!Entering RAD1PROMTpInit\n\r"); /**/
	if (!(p2 = *((per2_info **)base_in)))
		return;

	RAD1TpInit(base_in);

	*x = p2->v_info.x_size;
	*y = p2->v_info.y_size;
	RAD1InitCursor(p2);

	pon_puts("!Leaving RAD1PROMTpInit\n\r"); /**/
}

struct htp_fncs RAD1_htp_fncs = {
	RAD1TpBlankscreen,
	RAD1TpColor,
	RAD1TpMapcolor,
	RAD1TpSboxfi,
	RAD1TpPnt2i,
	RAD1TpCmov2i,
	RAD1TpDrawbitmap,
#ifdef CAN_BLOCK_MOVE
	RAD1TpBlockM,	/* block move??? */
#else /* CAN_BLOCK_MOVE */
	0,
#endif /* CAN_BLOCK_MOVE */
	RAD1PROMTpInit,
	RAD1TpMovec
};

#else  /* if !defined(_STANDALONE) */
static void
RAD1TpUnblank(void *base_in)
{
	RAD1TpBlankscreen(base_in, 0);
}

static int
RAD1TpGetdesc(void *base_in, int code)
{
	/* Any other bit maps ? */
	switch (code ) {
		case HTP_GD_BIGMAPS: /* handles large bit maps */
			return 1;
	}

	return 0;
}

struct htp_fncs RAD1_htp_fncs = {
	RAD1TpInit,
	RAD1TpMapcolor,
	RAD1TpColor,
	RAD1TpSboxfi,
	RAD1TpPnt2i,
	RAD1TpDrawbitmap,
	RAD1TpCmov2i,
	RAD1TpUnblank,
	RAD1TpGetdesc,
	0
};
#endif /*!_STANDALONE */
