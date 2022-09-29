/*
 * crmRECommands.c
 *
 *    This file contains the functions for initializing RE registers.
 *
 */

/*
 * Copyright 1996, Silicon Graphics, Inc.
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

#ident "$Revision: 1.2 $"

#include <libsc.h>
#include <sys/IP32.h>
#include <sys/sema.h>
#include <sys/types.h>
#include <sys/gfx.h>

#include <sys/crimechip.h>
#include <sys/crime_gfx.h>
#include <sys/crime_gbe.h>
#include <uif.h>
#include "crmDefs.h"
#include "crm_stand.h"
#include "gbedefs.h"

/************************************************************************/
/* External Declarations						*/
/************************************************************************/

extern Crimechip *crm_CRIME_Base;

extern int maxPixelsX,
           maxPixelsY;

extern int maxNormalTilesX,
           maxNormalTilesY,
           maxNormalNumTiles,
           maxNormalExtraPixelsX,
           maxNormalPixelsPerTileX,
           maxNormalPixelsPerTileY;

extern int maxOverlayTilesX,
           maxOverlayTilesY,
           maxOverlayNumTiles,
           maxOverlayExtraPixelsX,
           maxOverlayPixelsPerTileX,
           maxOverlayPixelsPerTileY;

extern int depthBits,
           depthBytes;

extern int refreshRate;


/************************************************************************/
/* Global Declarations							*/
/************************************************************************/

int crmColor;


/************************************************************************/
/* Local Declarations							*/
/************************************************************************/

void crmRESetFGColor(unsigned int color);
void crmREDrawPoint(int x1, int y1);
void crmREDrawLine(int x1, int y1, int x2, int y2);
void crmREDrawRect(int x1, int y1, int x2, int y2);
void crmREDrawNRect(int x1, int y1, int x2, int y2);

/************************************************************************/
/* Functions								*/
/************************************************************************/

/****************************/
/* Section xx: IDE Commands */
/****************************/

/*
 * Usage:	set_fg_color <32-bit hex color>
 *
 * Description:
 *
 */

int
crmCmdRESetFGColor(int argc, char **argv)
{
   int color;

   if (argc != 2)
   {
      msg_printf(VRB, "Usage: set_fg_color <32-bit hex color>\n");
      return 0;
   }

   color = atoi(argv[1]);
   msg_printf(DBG, "Setting foreground color to 0x%0.8x...", color);
   crmRESetFGColor(color);

   msg_printf(DBG, "Done\n");
   return 1;
}


/*
 * Usage:	draw_point <x> <y>
 *
 * Description:
 *
 */

int
crmCmdREDrawPoint(int argc, char **argv)
{
   int x, y;

   if (argc != 3)
   {
      msg_printf(VRB, "Usage: draw_point <x> <y>\n");
      return 0;
   }

   x = atoi(argv[1]);
   y = atoi(argv[2]);

   msg_printf(DBG, "Drawing point at (%d, %d)...", x, y);
   crmREDrawPoint(x, y);

   msg_printf(DBG, "Done\n");
   return 1;
}


/*
 * Usage:	
 *
 * Description:
 *
 */

int
crmCmdREDrawLine(int argc, char **argv)
{
   int x1, y1, x2, y2;

   if (argc != 5)
   {
      msg_printf(VRB, "Usage: draw_line <x1> <y2> <x2> <y2>\n");
      return 0;
   }

   x1 = atoi(argv[1]);
   y1 = atoi(argv[2]);
   x2 = atoi(argv[3]);
   y2 = atoi(argv[4]);

   msg_printf(DBG, "Drawing line from (%d, %d) to (%d, %d)...", x1, y1, x2, y2);
   crmREDrawLine(x1, y1, x2, y2);

   msg_printf(DBG, "Done\n");
   return 1;
}


/*
 * Usage:	
 *
 * Description:
 *
 */

int
crmCmdREDrawRect(int argc, char **argv)
{
   int x1, y1, x2, y2;

   if (argc != 5)
   {
      msg_printf(VRB, "Usage: draw_rect <x1> <y2> <x2> <y2>\n");
      return 0;
   }

   x1 = atoi(argv[1]);
   y1 = atoi(argv[2]);
   x2 = atoi(argv[3]);
   y2 = atoi(argv[4]);

   msg_printf(DBG, "Drawing rectangle from (%d, %d) to (%d, %d)...", x1, y1, x2, y2);
   crmREDrawRect(x1, y1, x2, y2);

   msg_printf(DBG, "Done\n");
   return 1;
}

/*********************************/
/* Section xx: RE Initialization */
/*********************************/

/*
 * Function:	crmInitTLB
 *
 * Parameters:	(None)
 *
 * Description:	This function clears out the RE TLBs so as to avoid
 *		writing to areas of memory we shouldn't be writing
 *		to.
 *
 */

void
crmInitTLB(void)
{
   int i;

   for (i = 0; i < 64; i++)
   {
      crmSet64( crm_CRIME_Base, Tlb.fbA[i].dw, 0x0000000000000000LL);
      crmSet64( crm_CRIME_Base, Tlb.fbB[i].dw, 0x0000000000000000LL);
      crmSet64( crm_CRIME_Base, Tlb.fbC[i].dw, 0x0000000000000000LL);
   }

   for (i = 0; i < 28; i++)
   {
      crmSet64( crm_CRIME_Base, Tlb.tex[i].dw, 0x0000000000000000LL);
   }

   for (i = 0; i < 4; i++)
   {
      crmSet64( crm_CRIME_Base, Tlb.cid[i].dw, 0x0000000000000000LL);
   }

   for (i = 0; i < 16; i++)
   {
      crmSet64( crm_CRIME_Base, Tlb.linearA[i].dw, 0x0000000000000000LL);
      crmSet64( crm_CRIME_Base, Tlb.linearB[i].dw, 0x0000000000000000LL);
   }
}


/*
 * Function:	crmSetupTLB
 *
 * Parameters:	descList
 *		numTilesX
 *		numTilesY
 *		tlbFlag
 *
 * Description:	This function sets up the assigned TLB based on the
 *		tile pointers given in descriptor list format.
 *
 */

void
crmSetupTLB(unsigned short *descList, int numTilesX, int numTilesY, char tlbFlag)
{
   int i, j, k, evenTilesPerRow, spareTilesPerRow, shift, tlbIndex;
   unsigned long long outputVal;

   for (i = 0; i < 64; i++)
   {
      crmWaitReFifo( crm_CRIME_Base, CRM_REFIFOWAIT_TRITON133);
      if (tlbFlag == CRM_SETUP_TLB_A)
      {
	 crmSet64( crm_CRIME_Base, Tlb.fbA[i].dw, 0x0000000000000000LL);
      }
      else if (tlbFlag == CRM_SETUP_TLB_B)
      {
	 crmSet64( crm_CRIME_Base, Tlb.fbB[i].dw, 0x0000000000000000LL);
      }
      else if (tlbFlag == CRM_SETUP_TLB_C)
      {
	 crmSet64( crm_CRIME_Base, Tlb.fbC[i].dw, 0x0000000000000000LL);
      }
   }
   crmWaitReIdle( crm_CRIME_Base);

   k = 0;

   spareTilesPerRow = numTilesX % 4;
   evenTilesPerRow = numTilesX - spareTilesPerRow;

   for (i = 0; i < numTilesY; i++)
   {
      tlbIndex = i * 4;

      for (j = 0; j < evenTilesPerRow; j += 4)
      {
	 outputVal = ((unsigned long long) descList[k] << 48) |
	              ((unsigned long long) descList[k+1] << 32) |
	              ((unsigned long long) descList[k+2] << 16) |
	              ((unsigned long long) descList[k+3]);
	 k += 4;

	 outputVal |= (unsigned long long) 0x8000800080008000LL;

         crmWaitReFifo( crm_CRIME_Base, CRM_REFIFOWAIT_TRITON133);

	 if (tlbFlag == CRM_SETUP_TLB_A)
	 {
	    crmSet64( crm_CRIME_Base, Tlb.fbA[tlbIndex].dw, outputVal);
         }
	 else if (tlbFlag == CRM_SETUP_TLB_B)
	 {
	    crmSet64( crm_CRIME_Base, Tlb.fbB[tlbIndex].dw, outputVal);
         }
	 else if (tlbFlag == CRM_SETUP_TLB_C)
         {
	    crmSet64( crm_CRIME_Base, Tlb.fbC[tlbIndex].dw, outputVal);
         }

            tlbIndex++;
      }

      outputVal = 0;
      shift = 48;

      for (j = 0; j < spareTilesPerRow; j++)
      {
         outputVal |= ((unsigned long long) descList[k] << shift);
         outputVal |= ((unsigned long long) 0x0000000000008000LL << shift);
	 k++;
	 shift -= 16;
      }

      if (tlbFlag == CRM_SETUP_TLB_A)
      {
         crmSet64( crm_CRIME_Base, Tlb.fbA[tlbIndex].dw, outputVal);
      }
      else if (tlbFlag == CRM_SETUP_TLB_B)
      {
         crmSet64( crm_CRIME_Base, Tlb.fbB[tlbIndex].dw, outputVal);
      }
      else if (tlbFlag == CRM_SETUP_TLB_C)
      {
         crmSet64( crm_CRIME_Base, Tlb.fbC[tlbIndex].dw, outputVal);
      }
   }
}


/*
 * Function:	crmSetTLB
 *
 * Parameters:	tlbFlag
 *
 * Description:	This sets the CRIME RE mode registers to use the appropriate
 *		TLB.
 *
 */

void
crmSetTLB(char tlbFlag)
{
   unsigned int outputVal;

   if (tlbFlag == CRM_SETUP_TLB_A)
   {
      outputVal = BM_FB_A | BM_BUF_DEPTH_8 | BM_PIX_DEPTH_8 | BM_COLOR_INDEX;
   }
   else if (tlbFlag == CRM_SETUP_TLB_B)
   {
      outputVal = BM_FB_B | BM_BUF_DEPTH_32 | BM_PIX_DEPTH_32 | BM_RGBA;
   }
   else if (tlbFlag == CRM_SETUP_TLB_C)
   {
      outputVal = BM_FB_C | BM_BUF_DEPTH_32 | BM_PIX_DEPTH_8 | BM_COLOR_INDEX;
   }
   else
      return;

   crmSet( crm_CRIME_Base, BufMode.src, outputVal);
   crmSet( crm_CRIME_Base, BufMode.dst, outputVal);
}


/*
 * Function:	crmClearFramebuffer
 *
 * Parameters:	(None)
 *
 * Description:	This function clears the predefined diagnostic normal
 *		and overlay planes.
 *
 */

void
crmClearFramebuffer(void)
{
   crmSetTLB(CRM_SETUP_TLB_B);
   crmSet( crm_CRIME_Base, Shade.fgColor, 0x00000000);
   crmSet( crm_CRIME_Base, DrawMode, DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK);
   crmSet( crm_CRIME_Base, Vertex.X[0], (0 << 16) | (0) & 0xffff);
   crmSet( crm_CRIME_Base, Vertex.X[1], ((maxPixelsX - 1) << 16) | (maxPixelsY - 1) & 0xffff);
   crmSet( crm_CRIME_Base, Primitive, PRIM_OPCODE_RECT | PRIM_EDGE_LR_TB);
   crmSetAndGo( crm_CRIME_Base, PixPipeNull, 0x00000000);
   crmFlush( crm_CRIME_Base);

   crmSetTLB(CRM_SETUP_TLB_C);
   crmSet( crm_CRIME_Base, Shade.fgColor, 0x00000000);
   crmSet( crm_CRIME_Base, DrawMode, DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK);
   crmSet( crm_CRIME_Base, Vertex.X[0], (0 << 16) | (0) & 0xffff);
   crmSet( crm_CRIME_Base, Vertex.X[1], ((maxPixelsX - 1) << 16) | (maxPixelsY - 1) & 0xffff);
   crmSet( crm_CRIME_Base, Primitive, PRIM_OPCODE_RECT | PRIM_EDGE_LR_TB);
   crmSetAndGo( crm_CRIME_Base, PixPipeNull, 0x00000000);
   crmFlush( crm_CRIME_Base);
   crmWaitReIdle( crm_CRIME_Base);
}


/**************************************/
/* Section xx: Drawing Mode Functions */
/**************************************/

/*
 * Function:	crmRESetFGColor
 *
 * Parameters:	color
 *
 * Description:
 *
 */

void
crmRESetFGColor(unsigned int color)
{
   crmColor = color;
}


/***********************************/
/* Section xx: X and GL Primitives */
/***********************************/

/*
 * Function:	crmREDrawPoint
 *
 * Parameters:	x1	
 *		y1
 *
 * Description:	
 *
 */

void
crmREDrawPoint(int x1, int y1)
{
   int temp1;
   unsigned int drawModeCommand, logicOpCommand;

   drawModeCommand = DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK;
   logicOpCommand  = CRM_LOGICOP_COPY;

   temp1 = ((x1 << 16) & 0xffff0000) | (y1 & 0x0000ffff);

   crmWaitReFifo( crm_CRIME_Base, CRM_REFIFOWAIT_TRITON133);
   crmSet( crm_CRIME_Base, DrawMode, drawModeCommand);
   crmSet( crm_CRIME_Base, LogicOp, logicOpCommand);
   crmSet( crm_CRIME_Base, Primitive, PRIM_OPCODE_POINT);
   crmSet( crm_CRIME_Base, Shade.fgColor, crmColor);
   crmSetAndGo( crm_CRIME_Base, Vertex.X[0], temp1);
   crmSet( crm_CRIME_Base, PixPipeNull, 0x00000000);
   crmFlush( crm_CRIME_Base);
}


/*
 * Function:	crmREDrawLine
 *
 * Parameters:	x1	
 *		y1
 *		x2
 *		y2
 *
 * Description:	
 *
 */

void
crmREDrawLine(int x1, int y1, int x2, int y2)
{
   int temp1, temp2;
   unsigned int drawModeCommand, logicOpCommand;

   drawModeCommand = DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK;
   logicOpCommand  = CRM_LOGICOP_COPY;

   temp1 = ((x1 << 16) & 0xffff0000) | (y1 & 0x0000ffff);
   temp2 = ((x2 << 16) & 0xffff0000) | (y2 & 0x0000ffff);

   crmWaitReFifo( crm_CRIME_Base, CRM_REFIFOWAIT_TRITON133);
   crmSet( crm_CRIME_Base, DrawMode, drawModeCommand);
   crmSet( crm_CRIME_Base, LogicOp, logicOpCommand);
   crmSet( crm_CRIME_Base, ColorMask, 0xffffffff);
   crmSet( crm_CRIME_Base, Primitive, PRIM_OPCODE_LINE | PRIM_ZERO_WIDTH);
   crmSet( crm_CRIME_Base, Shade.fgColor, crmColor);
   crmSet( crm_CRIME_Base, Vertex.X[0], temp1);
   crmSetAndGo( crm_CRIME_Base, Vertex.X[1], temp2);
   crmSet( crm_CRIME_Base, PixPipeNull, 0x00000000);
   crmFlush( crm_CRIME_Base);
}


/*
 * Function:	crmREDrawTriangle
 *
 * Parameters:	x1	
 *		y1
 *		x2
 *		y2
 *		x3
 *		y3
 *
 * Description:	
 *
 */

void
crmREDrawTriangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
   int temp1, temp2, temp3;
}

void
crmREDrawNRect (int x1, int y1, int x2, int y2)
{
  int delta1, delta2, i;
 
  delta1 = x2-x1;
  delta2 = y2-y1;

  if (delta1 >= delta2) {
    for (i = x1; i <= x2; i++) {
      crmREDrawLine(i, y1, i, y2);
    }
  }
  else {
    for (i = y1; i <= y2; i++) {
      crmREDrawLine(x1, i, x2, i);
    }
  }
}

/*
 * Function:	crmREDrawRect
 *
 * Parameters:	x1	
 *		y1
 *		x2
 *		y2
 *
 * Description:	
 *
 */
void
crmREDrawRect(int x1, int y1, int x2, int y2)
{
   int temp1, temp2;
   unsigned int drawModeCommand, logicOpCommand, primitiveCommand;

   drawModeCommand = DM_ENCOLORMASK | DM_ENLOGICOP | DM_ENCOLORBYTEMASK;
   logicOpCommand  = CRM_LOGICOP_COPY;

   if (x1 < x2)
   {
      if (y1 < y2)
         primitiveCommand = PRIM_EDGE_LR_TB;
      else
         primitiveCommand = PRIM_EDGE_LR_BT;
   }
   else
   {
      if (y1 < y2)
         primitiveCommand = PRIM_EDGE_RL_TB;
      else
         primitiveCommand = PRIM_EDGE_RL_BT;
   }

   temp1 = ((x1 << 16) & 0xffff0000) | (y1 & 0x0000ffff);
   temp2 = ((x2 << 16) & 0xffff0000) | (y2 & 0x0000ffff);

   crmWaitReFifo( crm_CRIME_Base, CRM_REFIFOWAIT_TRITON180);
   crmSet( crm_CRIME_Base, DrawMode, drawModeCommand);
   crmSet( crm_CRIME_Base, LogicOp, logicOpCommand);
   crmSet( crm_CRIME_Base, ColorMask, 0xffffffff);
   crmSet( crm_CRIME_Base, Primitive, primitiveCommand | PRIM_OPCODE_RECT);
   crmSet( crm_CRIME_Base, Shade.fgColor, crmColor);
   crmSet( crm_CRIME_Base, Vertex.X[0], temp1);
   crmSetAndGo( crm_CRIME_Base, Vertex.X[1], temp2);
   crmSet( crm_CRIME_Base, PixPipeNull, 0x00000000);
   crmFlush( crm_CRIME_Base);
}
