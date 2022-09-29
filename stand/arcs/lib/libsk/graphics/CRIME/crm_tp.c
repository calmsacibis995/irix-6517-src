/*
 * crm_tp.c - textport graphics routines for Moosehead
 *
 *    Ideally, this should simply replace the routines from Newport and
 *    allow for the rest of the graphics PROM to remain the same, with
 *    the possible exception of the hardcoded parts of the startup screen.
 *
 * ============================================================================
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.9 $"

#include "sys/IP32.h"
#include "sys/sema.h"			/* Needed for gfx.h */
#include "cursor.h"
#include "stand_htport.h"

#include "sys/gfx.h"			/* Needed for crime_gfx.h */
#include "sys/crime_gfx.h"		/* Needed before crime_gbe.h */
#include "sys/crime_gbe.h"
#include "sys/crimechip.h"
#include "crmDefs.h"
#include "crm_stand.h"
#include "gbedefs.h"

/************************************************************************/
/* External Declarations						*/
/************************************************************************/

extern struct crime_info crm_ginfo;
extern Crimechip *crm_CRIME_Base;
extern struct gbechip *crm_GBE_Base;
extern unsigned int normalPtr, blankPtr;
extern int _prom;
extern int usingFlatPanel;

extern int ttyprintf(const char *fmt, ...);
extern int i2cfp_PanelOn(struct gbechip *);
extern int i2cfp_PanelOff(struct gbechip *);


/************************************************************************/
/* Global Declarations							*/
/************************************************************************/


/************************************************************************/
/* Local Declarations							*/
/************************************************************************/

static struct
{
   long	cpos_x, cpos_y;
   int	color;
} tpcontext;

static CrimeFifoMax = CRM_REFIFOWAIT_TRITON180;


/************************************************************************/
/* Functions								*/
/************************************************************************/

/*
 * Function:	CrmTpBlankscreen
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *		mode		Screen blank flag
 *
 * Description:	This functions sets the display control to disable the screen
 *		(if mode is not 0) or enable the screen.  In Newport, this
 *		was controlled by VC2.
 *
 *		For now, we will perform the same process by disabling two
 *		control bits (the cursor planes DMA and the overlay planes
 *		DMA).  The normal planes are ignored since they are not
 *		used by the PROM graphics.
 *
 */

static void
CrmTpBlankscreen(void *hw, int mode)
{
   if (hw == (void *) crm_CRIME_Base)
   {
      if (mode)
      {
	 if (usingFlatPanel)
	 {
	    i2cfp_PanelOff( crm_GBE_Base );
	 }

         gbeSetReg( crm_GBE_Base, crs_ctl, 0x00000000);
	 gbeSetReg( crm_GBE_Base, ctrlstat, 0x000aa000);
      }
      else
      {
	 if (usingFlatPanel)
	 {
	    i2cfp_PanelOn( crm_GBE_Base );
	 }

         gbeSetReg( crm_GBE_Base, crs_ctl, 0x00000001);
	 gbeSetReg( crm_GBE_Base, ctrlstat, 0x300aa000);
      }
   }
   else
   {
      return;
   }
}


/*
 * Function:	CrmTpColor
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *		col		New current drawing color (CI8 value)
 *
 * Description:	This function sets the current drawing color to col.  In
 *		Newport, this was done by setting the colori register in
 *		REX3.  In Moosehead, this is done by changing the foreground
 *		color in both the Shade.fgValue and the MTE.fgValue color.
 *
 */

static void
CrmTpColor(void *hw, int col)
{
   register Crimechip *base = (Crimechip *) hw;
   unsigned long newColor = col;

   /* Set current drawing color to col */

   tpcontext.color = col;

   /* For the foreground color register in CRIME, all bit depths less */
   /* than 32 bits need to be replicated.  Here, we're assuming 8 bit */
   /* depths.							 */

   newColor |= (newColor << 24) | (newColor << 16) | (newColor << 8);

   crmSet( base, Shade.fgColor, newColor);	/* Pixpipe color	*/
   crmSet( base, Mte.fgValue, newColor);	/* MTE color		*/
}


/*
 * Function:	CrmTpMapColor
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *		col		Index to colormap
 *		r		Red value
 *		g		Green value
 *		b		Blue value
 *
 * Description:	This function sets the color map entry indexed by col to the
 *		given R, G, and B values.
 *
 */

static void
CrmTpMapColor(void *hw, int col, int r, int g, int b)
{
   int retVal;
   unsigned char gbeVersion;

   gbeVersion = crmGBEChipID();

if (gbeVersion >= 2)    /* We have an Arsenic chip */
      {       
        while (1)
        {
          gbeGetReg( crm_GBE_Base, cm_fifo, retVal);
          retVal &= GBE_CMFIFO_MASK;
 
          /*
           * Arsenic correctly handles the cmap fifo indicator register.
           * The FIFO is now limited to 63 entries when full.  Thus,
           * 0x0 means FULL and 0x3F means EMPTY. The test below queries the
           * Arsenic chip to see if _all_ slots are available.
           */
 
          if (retVal == ARSENIC_CMAP_FIFO_EMPTY)
            break;
        }
      }
 
    else                       /* We have a GBE chip */
      {
        while (1)
        {
          gbeGetReg( crm_GBE_Base, cm_fifo, retVal);
          retVal &= GBE_CMFIFO_MASK;
 
          /* 
           * GBE Hack since the cm_fifo register can overflow; 0x0
           * in cm_fifo on GBE could mean the fifo is full OR
           * empty, there is no way to tell.  So we must test for the zero
           * state here, even though that might be incorrect.
           */
 
          if (retVal == GBE_CMAP_FIFO_FULL_OR_EMPTY)
            break;
        }
      }

   if (hw == (void *) crm_CRIME_Base)
   {
      gbeSetTable( crm_GBE_Base, cmap, col, (r << 24) | (g << 16) | (b << 8));
   }
}


/*
 * Function:	CrmTpSboxfi
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *		x1, y1		First set of coordinates
 *		x2, y2		Second set of coordinates
 *
 * Description:	This function draws a filled rectangle (presumably filled
 *		with the current drawing color) via the rendering engine.
 *		In Newport, this was done via REX3.  In Moosehead, this is
 *		done via CRIME.  For Petty CRIME, the rendering engine is
 *		simulated by this code, and results are written directly
 *		to the framebuffer.
 *
 */

static void
CrmTpSboxfi(void *hw, int x1, int y1, int x2, int y2)
{
   register Crimechip *base = (Crimechip *) hw;
   int drawModeCommand, logicOpCommand, primitiveCommand, temp;

   /* Here, we use the MTE to perform clears (setting all pixels in the */
   /* box to the foreground color) to the framebuffer.		   */

   crmWaitReFifo( crm_CRIME_Base, CrimeFifoMax);

   if (!_prom)
   {
      crmSet( crm_CRIME_Base, BufMode.src, CRM_TP_DRAWING_MODE);
      crmSet( crm_CRIME_Base, BufMode.dst, CRM_TP_DRAWING_MODE);
   }

   y1 = crm_ginfo.gfx_info.ypmax - y1;
   y2 = crm_ginfo.gfx_info.ypmax - y2;

   if ((y1 == 1) && (y2 == 1))
      y1 = 0;

   if (y1 > y2)
      primitiveCommand = PRIM_OPCODE_RECT | PRIM_EDGE_LR_BT;
   else
      primitiveCommand = PRIM_OPCODE_RECT | PRIM_EDGE_LR_TB;

   drawModeCommand = DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK;
   logicOpCommand = CRM_LOGICOP_COPY;

   crmSet( base, DrawMode, drawModeCommand);
   crmSet( base, LogicOp, logicOpCommand);
   crmSet( base, ColorMask, 0xffffffff);
   crmSet( base, Vertex.X[0], (x1 << 16) | (y1 & 0xffff));
   crmSet( base, Vertex.X[1], (x2 << 16) | (y2 & 0xffff));
   crmSet( base, Primitive, primitiveCommand);
   crmSetAndGo( base, PixPipeNull, 0x00000000);
   crmFlush( base);
}


/*
 * Function:	CrmTpPnt2i
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *
 * Description:	This draws a single point.
 *
 */

static void
CrmTpPnt2i(void *hw, int x, int y)
{
   register Crimechip *base = (Crimechip *) hw;
   int drawModeCommand, logicOpCommand, primitiveCommand;

   drawModeCommand = DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK;
   logicOpCommand = CRM_LOGICOP_COPY;
   primitiveCommand = PRIM_OPCODE_POINT;

   crmWaitReFifo( crm_CRIME_Base, CrimeFifoMax);
   if (!_prom)
   {
      crmSet( crm_CRIME_Base, BufMode.src, CRM_TP_DRAWING_MODE);
      crmSet( crm_CRIME_Base, BufMode.dst, CRM_TP_DRAWING_MODE);
   }
   crmSet( base, DrawMode, drawModeCommand);
   crmSet( base, LogicOp, logicOpCommand);
   crmSet( base, ColorMask, 0xffffffff);
   crmSet( base, Primitive, primitiveCommand);
   crmSet( base, Vertex.X[0], (x << 16) |
           ((crm_ginfo.gfx_info.ypmax - y) & 0xffff));
   crmSetAndGo( base, PixPipeNull, 0x00000000);
   crmFlush( crm_CRIME_Base);
}


/*
 * Function:	CrmTpCmov2i
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *		x, y		New destination coordinates
 *
 * Description:	This function sets the position of the drawing context as
 *		given by the parameters.
 *
 */

static void
CrmTpCmov2i(void *hw, int x, int y)
{
   register struct Crimechip *base = (struct Crimechip *) hw;

   tpcontext.cpos_x = x;
   tpcontext.cpos_y = y;
}


/*
 * Function:	CrmTpDrawbitmap
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *		bitmap		Pointer to bitmap
 *
 * Description:	This function takes a bitmap (as described the header file
 *		stand_htport.h) and draws it to the framebuffer.  In
 *		Newport, this is done via REX3 and the zbuffer.  In Moosehead,
 *		this is done by using CRIME's Pixel Pipeline stippled line
 *		drawing primitives.
 *
 */

static void
CrmTpDrawbitmap(void *hw, struct htp_bitmap *bitmap)
{
   register Crimechip *base = (Crimechip *) hw;
   int i, xcurrent, xcurrentmax, xsize, ysize, ystart, xleft, xright;
   int shortsPerRow, shortCounter;
   unsigned short *bitmapPtr;
   unsigned long drawModeCommand, logicOpCommand, primitiveCommand;
   unsigned long stippleMode, stippleValue;
   unsigned short bitmapMask;

   xleft = tpcontext.cpos_x - bitmap->xorig;
   xsize = bitmap->xsize;
   xright = xleft + xsize - 1;
   ysize = bitmap->ysize;
   ystart = crm_ginfo.gfx_info.ypmax - tpcontext.cpos_y + bitmap->yorig;
   bitmapPtr = bitmap->buf;

   drawModeCommand = DM_ENLINESTIPPLE | DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK;
   logicOpCommand = CRM_LOGICOP_COPY;
   primitiveCommand = PRIM_OPCODE_LINE | PRIM_ZERO_WIDTH;

   crmWaitReFifo( crm_CRIME_Base, CrimeFifoMax);
   if (!_prom)
   {
      crmSet( crm_CRIME_Base, BufMode.src, CRM_TP_DRAWING_MODE);
      crmSet( crm_CRIME_Base, BufMode.dst, CRM_TP_DRAWING_MODE);
   }
   crmSet( base, DrawMode, drawModeCommand);
   crmSet( base, LogicOp, logicOpCommand);
   crmSet( base, ColorMask, 0xffffffff);
   crmSet( base, Primitive, primitiveCommand);

   /* Here, we have three possibilities for how we draw the bitmap:	*/
   /*								*/
   /* 1) The "width" of the bitmap is <= 16.  In this case, each	*/
   /*    unsigned short in the array represents a single row of the	*/
   /*    bitmap.							*/
   /* 2) The width of the bitmap is between 17 and 32, inclusive. In	*/
   /*    this case, two unsigned shorts represent a row of a bitmap.	*/
   /*    So, we fill can still make one stipple line draw per row.	*/
   /* 3) This width of the bitmap is greater than 32 bits.  In this	*/
   /*    case, we cannot draw an entire row of a bitmap with one	*/
   /*    stippled line, since the stipple register is 32 bits.  So,	*/
   /*    we simply draw as much of each row as we can until we are	*/
   /*    done with that row, and then proceed to the next row.	*/

   if (xsize <= 16)				/* Case 1 */
   {
      while (ysize > 0)
      {
	 stippleValue = *bitmapPtr;
	 bitmapPtr++;

         stippleMode = (31 << MAX_INDEX) | (16 << STIPPLE_INDEX);
         crmSet( base, Stipple.mode, stippleMode);

	 crmSet( base, Vertex.X[0], (xleft << 16) | ystart);
	 crmSet( base, Vertex.X[1], (xright << 16) | ystart);
	 crmSet( base, Stipple.pattern, stippleValue);
         crmSetAndGo( base, PixPipeNull, 0x00000000);
         crmWaitReFifo( crm_CRIME_Base, CrimeFifoMax);

	 ystart--;
	 ysize--;
      }
   }
   else if (xsize <= 32)			/* Case 2 */
   {
      stippleMode = (31 << MAX_INDEX) | (0 << STIPPLE_INDEX);
      crmSet( base, Stipple.mode, stippleMode);

      while (ysize > 0)
      {
	 stippleValue = *bitmapPtr;
	 stippleValue <<= 16;
	 bitmapPtr++;
	 stippleValue |= *bitmapPtr;
	 bitmapPtr++;

	 crmSet( base, Vertex.X[0], (xleft << 16) | ystart);
	 crmSet( base, Vertex.X[1], (xright << 16) | ystart);
	 crmSet( base, Stipple.pattern, stippleValue);
         crmSetAndGo( base, PixPipeNull, 0x00000000);
         crmWaitReFifo( crm_CRIME_Base, CrimeFifoMax);

	 ystart--;
	 ysize--;
      }
   }
   else						/* Case 3 */
   {
      stippleMode = (31 << MAX_INDEX) | (16 << STIPPLE_INDEX);
      shortsPerRow = (xsize + 15) / 16;

      while (ysize > 0)
      {
	 xcurrent = xleft;

	 for (i = 0; i < shortsPerRow; i++)
	 {
	    stippleValue = *bitmapPtr;
	    bitmapPtr++;

	    xcurrentmax = xcurrent + 15;
	    if (xcurrentmax > xright)
	       xcurrentmax = xright;

            crmSet( base, Stipple.mode, stippleMode);
	    crmSet( base, Vertex.X[0], (xcurrent << 16) | ystart);
	    crmSet( base, Vertex.X[1], (xcurrentmax << 16) | ystart);
	    crmSet( base, Stipple.pattern, stippleValue);
            crmSetAndGo( base, PixPipeNull, 0x00000000);
            crmWaitReFifo( crm_CRIME_Base, CrimeFifoMax);

	    xcurrent += 16;
	 }

	 ystart--;
	 ysize--;
      }
   }

   crmFlush( crm_CRIME_Base);

   /* Done drawing the bitmap */

   tpcontext.cpos_x += bitmap->xmove;
   tpcontext.cpos_y += bitmap->ymove;
}


/*
 * Function:	CrmTpBmove
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *		oldx, oldy	Upper left coordinate of source block
 *		newx, newy	Upper left coordinate of destination block
 *		w, h		Width and height (in pixels) of the copy
 *
 * Description:	This function copies a block of pixels from one framebuffer
 *		to another.  In Moosehead, this will be done by CRIME's
 *		Pixel Pipeline transfers.
 *
 */

static void
CrmTpBmove(void *hw, int oldx, int oldy, int newx, int newy, int w, int h)
{
   register Crimechip *base = (Crimechip *) hw;
   int drawModeCommand, logicOpCommand, primitiveCommand;
   int xs, ys, xe, ye, xoff, yoff, xsrc, ysrc;
   unsigned long long lvertex;

   xsrc = oldx;
   ysrc = crm_ginfo.gfx_info.ypmax - (oldy + h);
   xs = newx;
   ys = crm_ginfo.gfx_info.ypmax - (newy + h);
   xe = newx + w - 1;
   ye = crm_ginfo.gfx_info.ypmax - newy - 1;
   xoff = xs - xsrc;
   yoff = ys - ysrc;

   drawModeCommand = DM_ENPIXXFER | DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK;

   crmWaitReFifo( crm_CRIME_Base, CrimeFifoMax);
   if (!_prom)
   {
      crmSet( crm_CRIME_Base, BufMode.src, CRM_TP_DRAWING_MODE);
      crmSet( crm_CRIME_Base, BufMode.dst, CRM_TP_DRAWING_MODE);
   }
   crmSet( base, DrawMode, drawModeCommand);
   crmSet( base, ColorMask, 0xffffffff);
   crmSet( base, LogicOp, CRM_LOGICOP_COPY);
   crmSet( base, PixelXfer.src.xStep, 1);
   crmSet( base, PixelXfer.src.yStep, 1);

    if (yoff > 0) {
        if (xoff > 0) {
            crmSet( base, Primitive,
                        PRIM_OPCODE_RECT | PRIM_EDGE_RL_BT);
            lvertex = (xe << 16) | (ye & 0xffff);
            lvertex = (lvertex << 32) | (xs << 16) | (ys & 0xffff);
            crmSet64(base, Vertex.X[0], lvertex);
            crmSetAndGo( base, PixelXfer.src.addr,
                        ((xe - xoff) << 16) | ((ye - yoff) & 0xffff));
        } else {
            crmSet( base, Primitive,
                        PRIM_OPCODE_RECT | PRIM_EDGE_LR_BT);
            lvertex = (xs << 16) | (ye & 0xffff);
            lvertex = (lvertex << 32) | (xe << 16) | (ys & 0xffff);
            crmSet64(base, Vertex.X[0], lvertex);
            crmSetAndGo( base, PixelXfer.src.addr,
                        (xsrc << 16) | ((ye - yoff) & 0xffff));
        }
    } else {
        if (xoff > 0) {
            crmSet( base, Primitive,
                        PRIM_OPCODE_RECT | PRIM_EDGE_RL_TB);
            lvertex = (xe << 16) | (ys & 0xffff);
            lvertex = (lvertex << 32) | (xs << 16) | (ye & 0xffff);
            crmSet64(base, Vertex.X[0], lvertex);
            crmSetAndGo( base, PixelXfer.src.addr,
                        ((xe - xoff) << 16) | (ysrc & 0xffff));
        } else {
            crmSet( base, Primitive,
                        PRIM_OPCODE_RECT | PRIM_EDGE_LR_TB);
            lvertex = (xs << 16) | (ys & 0xffff);
            lvertex = (lvertex << 32) | (xe << 16) | (ye & 0xffff);
            crmSet64(base, Vertex.X[0], lvertex);
            crmSetAndGo( base, PixelXfer.src.addr,
                        (xsrc << 16) | (ysrc & 0xffff));
        }
    }
   crmFlush( base);

#if 0
   /* Note that unlike CrmTpSboxfi(), we do not need to check for	 */
   /* xStep and yStep.  The reason for this is because copies will	 */
   /* always be left to right and top to bottom, due to the nature	 */
   /* of the function parameters.					 */
   /*								 */
   /* If CrmTpBmove does not work, then the xStep and yStep is likely */
   /* to be the problem.						 */

   drawModeCommand = DM_ENPIXXFER | DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK;
   primitiveCommand = PRIM_OPCODE_RECT | PRIM_EDGE_LR_TB;

   crmWaitReFifo( crm_CRIME_Base, CrimeFifoMax);
   if (!_prom)
   {
      crmSet( crm_CRIME_Base, BufMode.src, CRM_TP_DRAWING_MODE);
      crmSet( crm_CRIME_Base, BufMode.dst, CRM_TP_DRAWING_MODE);
   }
   crmSet( base, DrawMode, drawModeCommand);
   crmSet( base, ColorMask, 0xffffffff);
   crmSet( base, LogicOp, CRM_LOGICOP_COPY);
   crmSet( base, Primitive, primitiveCommand);
   crmSet( base, PixelXfer.src.xStep, 1);
   crmSet( base, PixelXfer.src.yStep, 1);

   crmSet( base, PixelXfer.src.addr, (oldx << 16) |
           ((crm_ginfo.gfx_info.ypmax - (oldy + h)) & 0xffff));
   crmSet( base, Vertex.X[0], (newx << 16) |
           ((crm_ginfo.gfx_info.ypmax - (newy + h)) & 0xffff));
   crmSet( base, Vertex.X[1], ((newx + w - 1) << 16) |
           ((crm_ginfo.gfx_info.ypmax - newy - 1) & 0xffff));
   crmSetAndGo( base, PixPipeNull, 0x00000000);
   crmFlush( crm_CRIME_Base);
#endif
}


/*
 * Function:	CrmTpInit
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *		x, y		Pointer to write current textport limits
 *
 * Description:	This function is used by htp_gfx_init() to get the size of
 *		the screen.
 *
 */

static void
CrmTpInit(void *hw, int *x, int *y)
{
   register struct Crimechip *base = (struct Crimechip *) hw;

   *x = crm_ginfo.gfx_info.xpmax;
   *y = crm_ginfo.gfx_info.ypmax;
}


/*
 * Function:	CrmTpMovec
 *
 * Parameters:	hw		Pointer to CRIME hardware
 *		x, y		Destination coordinates of the cursor
 *
 * Description:	This routine moves the cursor location to the specified
 *		x and y.
 *
 */

static void
CrmTpMovec(void *hw, int x, int y)
{
   unsigned long val;

   x += CURSOR_XOFF - tp_ptr_x_hot;
   y += CURSOR_YOFF - tp_ptr_y_hot;

   val = (((y << GBE_CRS_POSY_SHIFT) & GBE_CRS_POSY_MASK) |
	  (x & GBE_CRS_POSX_MASK));

   if (hw == (void *) crm_CRIME_Base)
   {
      gbeSetReg( crm_GBE_Base, crs_pos, val);
   }
}


/* Set up Moosehead graphics function pointers */

struct htp_fncs crm_tp_fncs = {
   CrmTpBlankscreen,
   CrmTpColor,
   CrmTpMapColor,
   CrmTpSboxfi,
   CrmTpPnt2i,
   CrmTpCmov2i,
   CrmTpDrawbitmap,
   CrmTpBmove,
   CrmTpInit,
   CrmTpMovec
};
