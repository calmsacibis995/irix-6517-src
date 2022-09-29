/*
 * post2_graphics.c - POST2 diagnostic graphics routines for Moosehead
 *
 *    This file contains the basic POST (power on self test) routines
 *    for Moosehead.
 *
 *    There are two sections, support functions and main functions.
 *    Main functions should be unaware of any underlying hardware or
 *    software mechanisms, while support functions act as the abstraction
 *    layer.
 *
 *    In general, the various sections (Support-POST2, Main-POST2,
 *    Support-POST3, Main-POST3) can be used to model startup code 
 *    for later machines.
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

#ident "$Revision: 1.1 $"

#include <sgidefs.h>
#include <sys/IP32.h>
#include <sys/sbd.h>
#include <sys/sema.h>			/* Needed for gfx.h */
#include <sys/gfx.h>			/* Needed for crime_gfx.h */
#include "cursor.h"
#include "libsc.h"
#include "stand_htport.h"
#include "tiles.h"

#include "sys/crime_gfx.h"		/* Needed before crime_gbe.h */
#if 1
#include "sys/crime_gbe.h"
#else
#include "gbeDefs.h"
#endif
#include "sys/crimechip.h"
#include "crmDefs.h"
#include "crm_stand.h"

/************************************************************************/
/* External Declarations						*/
/************************************************************************/

extern void cpu_acpanic(char *str);
extern void pongui_setup(char *ponmsg, int checkgui);
extern void _errputs(char*);
extern unsigned int crmCursor[];
extern void initDescriptorList(void);
extern void _init_htp(void);
extern void initCRIMEGraphicsBase(void);


/************************************************************************/
/* Global Variables							*/
/************************************************************************/

extern Crimechip *crm_CRIME_Base;		/* CRIME Base Address	*/
extern struct gbechip *crm_GBE_Base;		/* GBE Base Address	*/
extern struct crime_info crm_ginfo;		/* Device Indep. Info	*/

extern struct tp_fncs crm_tp_fncs;		/* Textport routines	*/

extern int maxTilesX,
	   maxTilesY,
	   maxNumTiles,
	   maxExtraPixelsX,
	   maxPixelsPerTileX,
	   maxPixelsPerTileY;


/************************************************************************/
/* Support Functions (post2)						*/
/*									*/
/*    The functions here are provided as modular steps in the post2	*/
/*    process.  As a result these functions should be kept static in	*/
/*    order to preserve locality.					*/
/*									*/
/************************************************************************/

/*
 * Function:	testCRIMERegisters
 *
 * Parameters:	(None)
 *
 * Description:	This function actually performs a series of reads and
 *		writes a 64-bit register and checks that the values are
 *		correct.
 *
 *		For now, we simply do Unique Data testing.  If space
 *		permits, we will later add Unique Address testing.
 *
 */

static int
testCRIMERegisters(void)
{
   int i;
   unsigned long long testValue, returnValue;

   /* Perform Unique Data Test on a register */

   testValue = (unsigned long long) 0x0000000000000000;
   crmNSSet64( crm_CRIME_Base, Mte.byteMask, testValue);
   crmGet64( crm_CRIME_Base, ScrMask[0], returnValue);

   if (testValue != returnValue)
      _errputs("(CRM) Failed 0 init\n");	/* EC: IP32.04.01.01 */

   testValue = (unsigned long long) 0x0000000000000001;

   for (i = 0; i < 64; i++)
   {
      crmNSSet64( crm_CRIME_Base, ScrMask[0], testValue);
      crmGet64( crm_CRIME_Base, ScrMask[0], returnValue);

      if (testValue != returnValue)
	 _errputs("(CRM) Failed walking 1's\n"); /* EC: IP32.04.01.02 */

      testValue <<= 1;
   }

   testValue = (unsigned long long) 0xffffffffffffffff;
   crmNSSet64( crm_CRIME_Base, ScrMask[0], testValue);
   crmGet64( crm_CRIME_Base, ScrMask[0], returnValue);

   if (testValue != returnValue)
      _errputs("(CRM) Failed 1 init\n");	/* EC: IP32.04.01.03 */

   testValue = (unsigned long long) 0x8000000000000000;

   for (i = 0; i < 64; i++)
   {
      crmNSSet64( crm_CRIME_Base, ScrMask[0], ~testValue);
      crmGet64( crm_CRIME_Base, ScrMask[0], returnValue);

      if (returnValue != ~testValue)
	 _errputs("(CRM) Failed walking 0's\n"); /* EC: IP32.04.01.04 */

      testValue >>= 1;
   }
}


/*
 * Function:	testStippledLines
 *
 * Parameters:	tilePtr			A pointer to the destination tile
 *
 * Description:	We assume that the entire screen is a single tile.  This
 *		test then does the following:
 *		1) Given the tile, the first 33 pixels are initialized
 *		   with zeros.
 *		2) 32 patterns are written to the pixels (1,0) through
 *		   (32,0).  For each pass, one pixel is written with
 *		   the corresponding bit set (e.g. pixel 1 should have
 *		   bit 1 set).
 *		3) After CRIME has finished writing, the patterns are
 *		   checked for.
 *
 */

static int
testStippledLines(void *tilePtr)
{
   int i, j;
   int pattern;
   unsigned int retVal;
   unsigned int *wordPtr = (unsigned int *) tilePtr;

   crmNSSet( crm_CRIME_Base, BufMode.src, CRM32_RGB_MODE_24);
   crmNSSet( crm_CRIME_Base, BufMode.dst, CRM32_RGB_MODE_24);

   pattern = 0x00000001;

   for (i = 0; i < 32; i++)
   {
      for (j = 0; j < 32; j++)
         wordPtr[j] = 0;

      for (j = 0; j < 32; j++)
         if (wordPtr[j] != 0)
	    _errputs("(CRM) Stipple Test Clear Failed\n"); /* EC: IP32.04.02.01 */

      crmNSSetForeground( crm_CRIME_Base, pattern);
      crmNSSet ( crm_CRIME_Base, Stipple.mode, (31 << MAX_INDEX) | (0 << STIPPLE_INDEX));
      crmNSSet( crm_CRIME_Base, Vertex.X[0], (0 << 16) | 0);
      crmNSSet( crm_CRIME_Base, Vertex.X[1], (31 << 16) | 0);
      crmNSSetAndGo( crm_CRIME_Base, Stipple.pattern, pattern);

      /* Wait until RE is flushed */

      while(1)
      {
	 crmGet( crm_CRIME_Base, Status, retVal);

	 if (retVal & CRMSTAT_RE_IDLE)
	    break;
      }

      for (j = 0; j < 32; j++)
      {
	 if (j == i)
	    if (wordPtr[j] != pattern)
	       _errputs("(CRM) Stipple Test Set Failed\n"); /* EC: IP32.04.02.02 */
	 else
	    if (wordPtr[j] != 0)
	       _errputs("(CRM) Stipple Test Skip Failed\n"); /* EC: IP32.04.02.03 */
      }

      pattern <<= 1;
   }
}


/*
 * Function:	testFilledRects
 *
 * Parameters:	(None)
 *
 * Description:	We assume that the entire screen is a single tile.  This
 *		test then does the following:
 *		1) Clears the tile with all zeros.
 *		2) Checks to see if all the bytes are zero.
 *		3) Draws a rectangle.
 *		4) Checks to see if the rectangle is in the proper place.
 *
 */

#define CLEAR_XLEFT	0
#define CLEAR_XRIGHT	127
#define CLEAR_YTOP	0
#define CLEAR_YBOTTOM	127

#define TEST_XLEFT	50
#define TEST_XRIGHT	60
#define TEST_YTOP	50
#define TEST_YBOTTOM	60
#define TEST_COLOR	0x01020408

static int
testFilledRects(void *tilePtr)
{
   int i, j, pixelOffset;
   unsigned int retVal;
   unsigned int *wordPtr = (unsigned int *) tilePtr;

   crmNSSet( crm_CRIME_Base, BufMode.src, CRM32_RGB_MODE_24);
   crmNSSet( crm_CRIME_Base, BufMode.dst, CRM32_RGB_MODE_24);

   /* Clear tile */

   crmNSSet( crm_CRIME_Base, Mte.fgValue, 0x00000000);
   crmNSSet( crm_CRIME_Base, Mte.mode, MTE_CLEAR);
   crmNSSet( crm_CRIME_Base, Mte.byteMask, 0xffffffff);
   crmNSSet( crm_CRIME_Base, Mte.dstYStep, 1);

   crmNSSet( crm_CRIME_Base, Mte.dst0, (TEST_XLEFT << 16) |
	     (TEST_YTOP & 0xffff));

   /* Wait until RE is flushed */

   while(1)
   {
      crmGet( crm_CRIME_Base, Status, retVal);

      if (retVal & CRMSTAT_RE_IDLE)
	 break;
   }

   /* Check if tile is clear */

   for (i = 0; i < 16384; i++)
      if (wordPtr[i] != 0x00000000)
	  _errputs("(CRM) 0's init failed\n");	/* EC: IP32.04.03.01 */

   /* Begin filled rect test */

   crmNSSet( crm_CRIME_Base, Mte.fgValue, TEST_COLOR);
   crmNSSet( crm_CRIME_Base, Mte.mode, MTE_CLEAR);
   crmNSSet( crm_CRIME_Base, Mte.byteMask, 0xffffffff);
   crmNSSet( crm_CRIME_Base, Mte.dstYStep, 1);

   crmNSSet( crm_CRIME_Base, Mte.dst0, (TEST_XLEFT << 16) |
	     (TEST_YTOP & 0xffff));
   crmNSSetAndGo( crm_CRIME_Base, Mte.dst1, (TEST_XRIGHT << 16) |
		  (TEST_YBOTTOM & 0xffff));
   MTEFlush( crm_CRIME_Base);

   /* Wait until RE is flushed */

   while(1)
   {
      crmGet( crm_CRIME_Base, Status, retVal);

      if (retVal & CRMSTAT_RE_IDLE)
	 break;
   }

   /* Check if rectangle is properly drawn */

   pixelOffset = (TEST_YTOP * 128) + TEST_XLEFT;

   for (i = TEST_YTOP; i <= TEST_YBOTTOM; i++)
   {
      for (j = TEST_XLEFT; j <= TEST_XRIGHT; j++)
      {
	 if (wordPtr[pixelOffset] != TEST_COLOR)
	    _errputs("(CRM) Incorrect color\n");  /* EC: IP32.04.03.02 */

         wordPtr[pixelOffset] = 0;

         pixelOffset++;
      }

      pixelOffset += 128 - (TEST_XRIGHT - TEST_XLEFT);
   }

   for (i = 0; i < 16384; i++)
      if (wordPtr[i] != 0x00000000)
	  _errputs("(CRM) Error in drawn rectangle\n");	/* EC: IP32.04.03.03 */
}

/************************************************************************/
/* Main Functions (post2)						*/
/*									*/
/************************************************************************/


/*
 * Function:	testCRIMEGraphics
 *
 * Parameters:	(None)
 *
 * Description:	In Newport, the graphics system is tested by reading
 *		and writing the REX3 registers.
 *
 *		In Moosehead, it's conceivable that more testing should
 *		be done.  The minimal amount of testing should be:
 *		1) The ability to draw stippled lines (for bitmaps).
 *		2) The ability to draw filled rectangles (for buttons and
 *		   the shadowed logo).
 *		3) The various registers are being written fine.  This
 *		   test can only be performed on the readable registers.
 *
 */

void
testCRIMEGraphics(void)
{
   int crmMode;
   unsigned long long crmTlb;
   void *tilePtr;

   initCRIMEGraphicsBase();

   /* Set up CRIME for testing */

   crm_ginfo.gfx_info.xpmax = 512;
   crm_ginfo.gfx_info.ypmax = 128;

   crmMode = BM_FB_A | BM_BUF_DEPTH_8 | BM_PIX_DEPTH_8 | BM_COLOR_INDEX;

   crmNSSet( crm_CRIME_Base, BufMode.src, crmMode);
   crmNSSet( crm_CRIME_Base, BufMode.dst, crmMode);

   tilePtr = getTile();
   crmTlb = (unsigned long long) K1_TO_PHYS(tilePtr);
   crmTlb <<= 32;
   crmTlb |= 0x8000000000000000;

   crmNSSet64( crm_CRIME_Base, Tlb.fbA[0].dw, crmTlb);
   crmFlush( crm_CRIME_Base);

   /* Begin testing */

   testCRIMERegisters();
   testStippledLines(tilePtr);
   testFilledRects(tilePtr);

   /* Done testing */

   crm_ginfo.gfx_info.xpmax = 0;
   crm_ginfo.gfx_info.ypmax = 0;

   freeTile(tilePtr);
   crmNSSet64( crm_CRIME_Base, Tlb.fbA[0].dw, 0x0000000000000000);
}
