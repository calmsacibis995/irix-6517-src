/*
 * crmGfxState.c
 *
 *    This file contains the core variables and functions for the
 *    IDE to determine what state it is in.
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

#ident "$Revision: 1.3 $"

#include <libsc.h>
#include <arcs/hinv.h>			/* Memory Allocation	*/
#include <sys/IP32.h>			/* Memory Map Info	*/
#include <sys/sema.h>
#include <sys/types.h>
#include <sys/gfx.h>

#include <sys/crimechip.h>		/* RE Map		*/
#include <sys/crime_gfx.h>
#include <sys/crime_gbe.h>		/* GBE Map		*/
#include <uif.h>			/* Error Code		*/
#include <IP32_status.h>
#include "crmDefs.h"			/* RE Register Access	*/
#include "crm_stand.h"
#include "gbedefs.h"
#include "crm_gfx.h"
#include "crm_timing.h"

/************************************************************************/
/* External Declarations						*/
/************************************************************************/

extern Crimechip *crm_CRIME_Base;
extern struct gbechip *crm_GBE_Base;
extern struct crime_timing_info crimeVTimings[ CRM_PROM_VT_SIZE ];

extern code_t errorCode;
extern unsigned int CRC16;

extern void crmInitGraphicsBase(void);
extern void initCrime(void);
extern void crmSetupTLB(unsigned short *descList, int numTilesX, int numTilesY, char tlbFlag);
extern int crime_i2cFpProbe(struct gbechip *gbe);
extern void crmClearFramebuffer(void);
extern void us_delay(int);
extern uint Crc16(int data);
extern void crmGBE_CursorSwitch(int flag);


/************************************************************************/
/* Global Declarations							*/
/************************************************************************/

/*****************/
/* Current State */
/*****************/

unsigned short *crmNormalPtr,		/* Pointer to GBE descriptor */
	       *crmOverlayPtr,		/* list.		     */
	       *crmVCPtr;
unsigned int crmNormalDescList,		/* Values for the various    */
	     crmOverlayDescList,	/* GBE *_control registers   */
	     crmVCDescList;
unsigned int *crmShadowNormalDescList,	/* Shadows of the GBE        */
	     *crmShadowOverlayDescList; /* descriptor list, but in   */
					/* pointer format.           */

unsigned int outputFrm0,		/* Values for the other GBE  */
	     outputFrm1,		/* display registers.        */
	     outputOvr0;

int maxPixelsX,
    maxPixelsY;

int maxNormalTilesX,
    maxNormalTilesY,
    maxNormalNumTiles,
    maxNormalExtraPixelsX,
    maxNormalPixelsPerTileX,
    maxNormalPixelsPerTileY;

int maxOverlayTilesX,
    maxOverlayTilesY,
    maxOverlayNumTiles,
    maxOverlayExtraPixelsX,
    maxOverlayPixelsPerTileX,
    maxOverlayPixelsPerTileY;

int depthBits,
    depthBytes;

int refreshRate;

char timingNotSetFlag = 0,		/* Indicates the timing is not  */
					/* initialized, so GBE doesn't  */
					/* need to be stopped or have   */
					/* its state saved.             */

     framebufferCreatedFlag = 0,	/* Indicates that a framebuffer */
					/* for IDE testing has already  */
					/* been created.                */

     stateSavedFlag = 0,		/* Indicates that the GBE and   */
					/* RE state has been saved and  */
					/* must be restored.            */

     useTimingFlag = 0,			/* Indicates which timing is    */
					/* currently of interest.       */

     graphicsInitializedFlag = 0;	/* Indicates that graphics has  */
					/* been initialized.            */

int fpaneltype;                         /* used to indicate panel ID,   */
                                        /* if appropriate               */

crime_timing_info_t *currentTimingPtr, *previousTimingPtr;

/******************/
/* Previous State */
/******************/

struct gbechip_nopad gbePrevState;
CrmTlbReg crmRETlbShadow;


/************************************************************************/
/* Local Declarations							*/
/************************************************************************/

#define GBE_CURSOR_XOFFSET	55
#define PREVIOUS		0x00000000
#define CURRENT			0x00000001

void crmInitGraphics(int x, int y, int depth, int refresh);
static int crmMatchPreviousTiming(int m, int n, int p);
static void crmSaveState(void);
static void crmSetDisplayParams(int x1, int y1, int x2, int y2);
static int crmMatchTiming(void);
static void crmGbeStop(void);
static void crmCreateFramebuffer(void);
static void crmGbeSetTiming(void);
static void crmGbeStartTiming(void);

void crmDeinitGraphics(void);
static void crmDestroyFramebuffer(void);
static void crmRestoreState(void);

void crmSaveRETLB(void);
void crmRestoreRETLB(void);
unsigned int crmReadNormalPixel(int x, int y);
unsigned char crmReadOverlayPixel(int x, int y);
unsigned long long crmCalculateNormalCRC(int x1, int y1, int x2, int y2);
unsigned short crmCalculateOverlayCRC(int x1, int y1, int x2, int y2);


/************************************************************************/
/* Functions								*/
/************************************************************************/

/****************************/
/* Section xx: IDE Commands */
/****************************/

/*
 * Usage:	init_graphics
 *
 * Description:	
 *
 */

void
crmCmdInitGraphics(int argc, char **argv)
{
   int xmax, ymax, depth, refresh;

   if (argc != 5)
   {
      msg_printf(ERR, "Usage: initGraphics <xmax> <ymax> <depth> <refresh>\n");
      return;
   }

   xmax = atoi(argv[1]);
   ymax = atoi(argv[2]);
   depth = atoi(argv[3]);
   refresh = atoi(argv[4]);

   crmInitGraphics(xmax, ymax, depth, refresh);
}


/*
 * Usage:	deinit_graphics
 *
 * Description:	
 *
 */

void
crmCmdDeinitGraphics(int argc, char **argv)
{
   crmDeinitGraphics();
}


/*
 * Usage:	read_normal_pixel <x> <y>
 *
 * Description:	
 *
 */

int
crmCmdReadNormalPixel(int argc, char **argv)
{
   int x, y, retVal;

   if (argc != 3)
   {
      msg_printf(ERR, "Usage: read_normal_pixel <x> <y>\n");
      return 0;
   }

   x = atoi(argv[1]);
   y = atoi(argv[2]);

   retVal = crmReadNormalPixel(x, y);

   return retVal;
}


/*
 * Usage:	read_overlay_pixel <x> <y>
 *
 * Description:	
 *
 */

char
crmCmdReadOverlayPixel(int argc, char **argv)
{
   int x, y, retVal;

   if (argc != 3)
   {
      msg_printf(ERR, "Usage: read_overlay_pixel <x> <y>\n");
      return 0;
   }

   x = atoi(argv[1]);
   y = atoi(argv[2]);

   retVal = crmReadOverlayPixel(x, y);

   return retVal;
}


/*
 * Usage:	get_normal_crc <x1> <y1> <x2> <y2>
 *
 * Description:	
 *
 */

long long
crmCmdCalculateNormalCRC(int argc, char **argv)
{
   int x1, y1, x2, y2;
   unsigned long long retVal;

   if (argc != 5)
   {
      msg_printf(ERR, "Usage: get_normal_crc <x1> <y1> <x2> <y2>\n");
      return 0;
   }

   x1 = atoi(argv[1]);
   y1 = atoi(argv[2]);
   x2 = atoi(argv[3]);
   y2 = atoi(argv[4]);

   retVal = crmCalculateNormalCRC(x1, y1, x2, y2);

   msg_printf(VRB, "Normal Framebuffer CRC: 0x%0.16llx\n", retVal);
   return retVal;
}


/*
 * Usage:	get_overlay_crc
 *
 * Description:	
 *
 */

int
crmCmdCalculateOverlayCRC(int argc, char **argv)
{
   int retVal;
   int x1, y1, x2, y2;

   if (argc != 5)
   {
      msg_printf(ERR, "Usage: get_overlay_crc <x1> <y1> <x2> <y2>\n");
      return 0;
   }

   x1 = atoi(argv[1]);
   y1 = atoi(argv[2]);
   x2 = atoi(argv[3]);
   y2 = atoi(argv[4]);

   retVal = crmCalculateOverlayCRC(x1, y1, x2, y2);

   msg_printf(VRB, "Overlay Framebuffer CRC: 0x%0.8x\n", retVal);
   return retVal;
}


/***************************************/
/* Section xx: Graphics Initialization */
/***************************************/

/*
 * Function:	crmInitGraphics
 *
 * Parameters:	(None)
 *
 * Description:
 *
 *		Note: The functions here *must* be called in the order they
 *		are listed.  This allows for GBE to be activated without
 *		shifting occurring.
 *
 */

void
crmInitGraphics(int x, int y, int depth, int refresh)
{
   unsigned int readVal;

   if (graphicsInitializedFlag)
      return;

   crmInitGraphicsBase();
   initCrime();
   crmSaveState();
   crmSetDisplayParams(x, y, depth, refresh);
   crmMatchTiming();
   crmGbeStop();
   crmCreateFramebuffer();
   crmClearFramebuffer();
   useTimingFlag = CURRENT;
   crmGbeSetTiming();
   crmGbeStartTiming();
   crmGBE_CursorSwitch(0);

   graphicsInitializedFlag = 1;
}


/*
 * Function:	crmSaveState
 *
 * Parameters:	(None)
 *
 * Description:	Presumably, if GBE is not running, then there is no GBE
 *		state to save.  So, we only save if GBE is running.
 *
 */

static void
crmSaveState(void)
{
   int i, m, n, p, readVal, retVal;

   gbeGetReg( crm_GBE_Base, vt_xy, readVal);
   readVal &= 0x80000000;

   if (readVal == 0x00000000)
   {
      msg_printf(VRB, "Saving Previous Graphics State...\n");

      gbeGetReg( crm_GBE_Base, dotclock, gbePrevState.dotclock);

      m = gbePrevState.dotclock & 0xff;
      n = (gbePrevState.dotclock >> 8) & 0x3f;
      p = (gbePrevState.dotclock >> 14) & 0x03;

      retVal = crmMatchPreviousTiming(m, n, p);

      if (retVal == -1)
	 return;

      gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
      gbeGetReg( crm_GBE_Base, ovr_width_tile, gbePrevState.ovr_width_tile);
      gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
      gbeGetReg( crm_GBE_Base, ovr_inhwctrl,   gbePrevState.ovr_control);

      gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
      gbeGetReg( crm_GBE_Base, frm_size_tile,  gbePrevState.frm_size_tile);
      gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
      gbeGetReg( crm_GBE_Base, frm_size_pixel, gbePrevState.frm_size_pixel);
      gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
      gbeGetReg( crm_GBE_Base, frm_inhwctrl,   gbePrevState.frm_control);

      for (i = 0; i < 32; i++)
      {
         gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
         gbeGetTable( crm_GBE_Base, mode_regs, i, gbePrevState.mode_regs[i]);
      }

      for (i = 0; i < 256; i++)
      {
         gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
         gbeGetTable( crm_GBE_Base, gmap, i, gbePrevState.gmap[i]);
      }

      gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
      gbeGetReg( crm_GBE_Base, crs_pos, gbePrevState.crs_pos);
      gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
      gbeGetReg( crm_GBE_Base, crs_ctl, gbePrevState.crs_ctl);

      for (i = 0; i < 3; i++)
      {
         gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
         gbeGetTable( crm_GBE_Base, crs_cmap, i, gbePrevState.crs_cmap[i]);
      }

      for (i = 0; i < 64; i++)
      {
         gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
         gbeGetTable( crm_GBE_Base, crs_glyph, i, gbePrevState.crs_glyph[i]);
      }

      for (i = 0; i < 32; i++)
      {
         gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
         gbeSetTable( crm_GBE_Base, mode_regs, i, 0x00000017);
      }

      for (i = 0; i < 4352; i++)
      {
         if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
	    busy(1);
         gbeWaitForBlanking( crm_GBE_Base, previousTimingPtr);
         gbeGetTable( crm_GBE_Base, cmap, i, gbePrevState.cmap[i]);
      }

      busy(0);
      stateSavedFlag = 1;
      msg_printf(VRB, "Done\n");
   }
   else
   {
      msg_printf(VRB, "Graphics not running; state not saved...\n");
      stateSavedFlag = 0;
      timingNotSetFlag = 1;
   }
}


/*
 * Function:	crmSetDisplayParams
 *
 * Parameters:	screenX
 *		screenY
 *		depth
 *		refresh
 *
 * Description:	This function sets the display to the given parameters.
 *
 */

static void
crmSetDisplayParams(int screenX, int screenY, int depth, int refresh)
{
   int retVal;

   /* Find out if 7of9 is connected */

   fpaneltype = crime_i2cFpProbe( crm_GBE_Base);

   if (fpaneltype != MONITORID_7of9) 
     msg_printf(VRB, "Initializing Display...");

   else {
     msg_printf(VRB, "Initializing Display for Flat Panel 1600SW...");
     screenX = 1600;
     screenY = 1024;
     depth = 32;
     refresh = 50;
   }

   maxPixelsX = screenX;
   maxPixelsY = screenY;

   depthBits = depth;

   if ((depthBits != 8) && 
       (depthBits != 16) &&
       (depthBits != 32))
   {
      msg_printf(ERR, "%d: Invalid framebuffer depth\n", depthBits);
      return;
   }

   depthBytes = depthBits / 8;

   refreshRate = refresh;

   maxNormalPixelsPerTileX = 512 / depthBytes;
   maxNormalPixelsPerTileY = 128;

   maxOverlayPixelsPerTileX = 512;
   maxOverlayPixelsPerTileY = 128;

   maxNormalTilesX = (maxPixelsX + (maxNormalPixelsPerTileX - 1)) /
		     maxNormalPixelsPerTileX;
   maxNormalTilesY = (maxPixelsY + (maxNormalPixelsPerTileY - 1)) /
		     maxNormalPixelsPerTileY;
   maxNormalNumTiles = maxNormalTilesX * maxNormalTilesY;
   maxNormalExtraPixelsX = maxPixelsX % maxNormalPixelsPerTileX;

   maxOverlayTilesX = (maxPixelsX + (maxOverlayPixelsPerTileX - 1)) /
		      maxOverlayPixelsPerTileX;
   maxOverlayTilesY = (maxPixelsY + (maxOverlayPixelsPerTileY - 1)) /
		      maxOverlayPixelsPerTileY;
   maxOverlayNumTiles = maxOverlayTilesX * maxNormalTilesY;
   maxOverlayExtraPixelsX = maxPixelsX % maxOverlayPixelsPerTileX;

   msg_printf(VRB, "Done\n");
}


/*
 * Function:	crmMatchTiming
 *
 * Parameters:	(None)
 *
 * Description:	This function matches and sets the pointer to the correct
 *		video timing.
 *
 */

static int
crmMatchTiming(void)
{
   crime_timing_t i;

   for (i = CRM_PROM_VT_NULL; i < CRM_PROM_VT_UNKNOWN; i++)
   {
      if ((maxPixelsX == crimeVTimings[i].width) &&
	  (maxPixelsY == crimeVTimings[i].height) &&
	  ((refreshRate == ((int) (crimeVTimings[i].fields_sec / 1000))) ||
	   (refreshRate == ((int) (crimeVTimings[i].fields_sec / 1000)+1)) ||
	   (refreshRate == ((int) (crimeVTimings[i].fields_sec / 1000)-1))))
      {
	 currentTimingPtr = &crimeVTimings[i];
	 return 1;
      }
   }

   msg_printf(ERR, "No timing found\n");
   currentTimingPtr = &crimeVTimings[CRM_PROM_VT_UNKNOWN];
   return -1;
}


/*
 * Function:	crmMatchPreviousTiming
 *
 * Parameters:	m
 *		n
 *		p
 *
 * Description:
 *
 */

static int
crmMatchPreviousTiming(int m, int n, int p)
{
   crime_timing_t i;

   for (i = CRM_PROM_VT_NULL; i < CRM_PROM_VT_UNKNOWN; i++)
   {
      if (((crimeVTimings[i].pll_m - 1) == m) &&
	  ((crimeVTimings[i].pll_n - 1) == n) &&
	  ((crimeVTimings[i].pll_p) == p))
      {
	 previousTimingPtr = &crimeVTimings[i];
	 return 1;
      }
   }

   msg_printf(ERR, "No previous timing found\n");
   previousTimingPtr = &crimeVTimings[CRM_PROM_VT_UNKNOWN];
   return -1;
}


/*
 * Function:	crmGbeStop
 *
 * Parameters:	(None)
 *
 * Description:	Stops the GBE DMA.
 *
 */

static void
crmGbeStop(void)
{
   int i, readVal;

   if (timingNotSetFlag)
      return;

   gbeGetReg( crm_GBE_Base, frm_control, readVal);
   gbeSetReg( crm_GBE_Base, frm_control, readVal & ~1);
   gbeGetReg( crm_GBE_Base, ovr_control, readVal);
   gbeSetReg( crm_GBE_Base, ovr_control, readVal & ~1);
   gbeGetReg( crm_GBE_Base, did_control, readVal);
   gbeSetReg( crm_GBE_Base, did_control, readVal & ~0x10000);

   for (i = 0; i < 20000; i++)
   {
      gbeGetReg( crm_GBE_Base, frm_inhwctrl, readVal);
      if (readVal & 1)
         us_delay(10);
      else
      {
         gbeGetReg( crm_GBE_Base, ovr_inhwctrl, readVal);
         if (readVal & 1)
            us_delay(10);
         else
	 {
            gbeGetReg( crm_GBE_Base, did_inhwctrl, readVal);
            if (readVal & 0x10000)
               us_delay(10);
            else
	       break;
	 }
      }
   }

   gbeSetReg( crm_GBE_Base, frm_control, (crmNormalDescList & ~1) + 32);

   for (i = 0; i < 20000; i++)
   {
      gbeGetReg( crm_GBE_Base, frm_inhwctrl, readVal);
      
      if (readVal != ((crmNormalDescList & ~1) + 32))
         us_delay(10);
      else
         break;
   }
}


/*
 * Function:	crmCreateFramebuffer
 *
 * Parameters:	(None)
 *
 * Description:	This function preallocates the framebuffers used for
 *		graphics diagnostics.
 *
 */

static void
crmCreateFramebuffer(void)
{
   int i, j, evenTilesPerRow, spareTilesPerRow;
   unsigned int readVal;

   msg_printf(VRB, "Creating Framebuffer...");

   /* Preallocate the Framebuffer */

   if (crmNormalPtr == NULL)
   {
      crmNormalPtr = (unsigned short *) PHYS_TO_K1(IDE_FB_DLIST1_BASE);
      crmNormalDescList = ((unsigned int) K1_TO_PHYS(crmNormalPtr)) | GBE_FRM_DMA_ENABLE_MASK;
      crmShadowNormalDescList = (unsigned int *) PHYS_TO_K1(IDE_SHADOW_FB_DLIST1_BASE);
   }

   if (crmOverlayPtr == NULL)
   {
      crmOverlayPtr = (unsigned short *) PHYS_TO_K1(IDE_FB_DLIST2_BASE);
      crmOverlayDescList = ((unsigned int) K1_TO_PHYS(crmOverlayPtr)) | GBE_OVR_DMA_ENABLE_MASK;
      crmShadowOverlayDescList = (unsigned int *) PHYS_TO_K1(IDE_SHADOW_FB_DLIST2_BASE);
   }

   if (framebufferCreatedFlag == 1)
      return;

   /* Set Up Normal Descriptor List */
   
   for (i = 0; i < maxNormalNumTiles; i++)
   {
      crmNormalPtr[i] = (unsigned short) ((IDE_FB_TILE_BASE + (0x00010000 * i)) >> 16);
      crmShadowNormalDescList[i] = PHYS_TO_K1(IDE_FB_TILE_BASE + (0x00010000 * i));
   }

   outputFrm0 = ((maxNormalExtraPixelsX * depthBytes) / 32) & GBE_OVR_RHS_MASK;

   if (maxNormalExtraPixelsX == 0)
      outputFrm0 |= (maxNormalTilesX << GBE_FRM_WIDTH_TILE_SHIFT) & GBE_FRM_WIDTH_TILE_MASK;
   else
      outputFrm0 |= ((maxNormalTilesX-1) << GBE_FRM_WIDTH_TILE_SHIFT) & GBE_FRM_WIDTH_TILE_MASK;

   if (depthBits == 8)
      outputFrm0 |= (GBE_FRM_DEPTH_8 << GBE_FRM_DEPTH_SHIFT) & GBE_FRM_DEPTH_MASK;
   else if (depthBits == 16)
      outputFrm0 |= (GBE_FRM_DEPTH_16 << GBE_FRM_DEPTH_SHIFT) & GBE_FRM_DEPTH_MASK;
   else
      outputFrm0 |= (GBE_FRM_DEPTH_32 << GBE_FRM_DEPTH_SHIFT) & GBE_FRM_DEPTH_MASK;

   outputFrm1 = (maxPixelsY << GBE_FRM_HEIGHT_PIX_SHIFT) & GBE_FRM_HEIGHT_PIX_MASK;

   /* Set Up Overlay Descriptor List  */

   for (i = 0; i < maxOverlayNumTiles; i++)
   {
      crmOverlayPtr[i] = (unsigned short) ((IDE_FB_TILE_BASE + (0x00010000 * (i + maxNormalNumTiles + 1))) >> 16);
      crmShadowOverlayDescList[i] = PHYS_TO_K1(IDE_FB_TILE_BASE + (0x00010000 * (i + maxNormalNumTiles + 1)));
   }

   outputOvr0 = (maxOverlayExtraPixelsX / 32) & GBE_OVR_RHS_MASK;

   if (maxOverlayExtraPixelsX == 0)
      outputOvr0 |= (maxOverlayTilesX << GBE_OVR_WIDTH_TILE_SHIFT) & GBE_OVR_WIDTH_TILE_MASK;
   else
      outputOvr0 |= ((maxOverlayTilesX-1) << GBE_OVR_WIDTH_TILE_SHIFT) & GBE_OVR_WIDTH_TILE_MASK;

   /* Set Up TLB */

   crmSetupTLB(crmNormalPtr, maxNormalTilesX, maxNormalTilesY, CRM_SETUP_TLB_B);
   crmSetupTLB(crmOverlayPtr, maxOverlayTilesX, maxOverlayTilesY, CRM_SETUP_TLB_C);

   framebufferCreatedFlag = 1;
   msg_printf(VRB, "Done\n");
}


/*
 * Function:	crmGbeSetTiming
 *
 * Parameters:	(None)
 *
 * Description:	This sets the video timing based on the current screen
 *		settings.
 *
 */

static void
crmGbeSetTiming(void)
{
   int temp;
   unsigned int pll_pnm, readVal;
   crime_timing_info_t *vtp;

   if (useTimingFlag == CURRENT)
      vtp = currentTimingPtr;
   else if (useTimingFlag == PREVIOUS)
      vtp = previousTimingPtr;

   /* Try to turn on timing if it's not on */

   if (timingNotSetFlag)
   {
      gbeSetReg( crm_GBE_Base, vt_xy, 0x00000000);
   }

   /* Find out if 7of9 is connected */

   if (fpaneltype != MONITORID_7of9) {
     msg_printf(VRB, "Initializing Timing...");
   } else {
     msg_printf(VRB, "Initializing Timing for Flat Panel 1600SW...");
   }

   /* Set timing */

   gbeGetReg( crm_GBE_Base, dotclock, readVal);
   gbeSetReg( crm_GBE_Base, dotclock, readVal & 0x0000ffff);

   gbeSetReg( crm_GBE_Base, vt_xymax, 0x00000000);
   gbeSetReg( crm_GBE_Base, vt_vsync,
              ((vtp->vsync_start & 0xFFF) << 12) |
              ((vtp->vsync_end & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_hsync,
              ((vtp->hsync_start & 0xFFF) << 12) |
              ((vtp->hsync_end & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_vblank,
              (((vtp->vblank_start) & 0xFFF) << 12) |
              (((vtp->vblank_end) & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_hblank,
              (((vtp->hblank_start) & 0xFFF) << 12) |
              (((vtp->hblank_end) & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_hcmap,
              (((vtp->hblank_start) & 0xFFF) << 12) |
              (((vtp->hblank_end) & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_vcmap,
              (((vtp->vblank_start) & 0xFFF) << 12) |
              (((vtp->vblank_end) & 0xFFF) << 0));

   if (fpaneltype == MONITORID_7of9) {
     msg_printf(VRB, "\nSetting hdrv/vdrv for Flat Panel 1600SW...");
     gbeSetReg( crm_GBE_Base, fp_hdrv,
		(((0x0) & 0xFFF) << 12) |
		(((0x640) & 0xFFF) << 0));
     gbeSetReg( crm_GBE_Base, fp_vdrv,
		(((0x1) & 0xFFF) << 12) |
		(((0x401) & 0xFFF) << 0));
   }

   if ((vtp->flags & CRIME_VOF_FS_STEREO) == 0)
      gbeSetReg( crm_GBE_Base, vt_f2rf_lock,
                 ((vtp->vblank_start & 0xfff)<<12) | 0xfff);
   else
      gbeSetReg( crm_GBE_Base, vt_f2rf_lock,
                 ((vtp->vblank_start & 0xfff)<<12) |
                 (vtp->vblank_start & 0xfff));

   temp = vtp->vblank_start - vtp->vblank_end - 1;
   if (temp > 0)
      temp = -temp;
   temp &= 0x00000fff;
   temp <<= 12;
   if (vtp->hblank_end >= 20)
      temp |= vtp->hblank_end - 20;
   else
      temp |= vtp->htotal - (20 - vtp->hblank_end);

   gbeSetReg( crm_GBE_Base, did_start_xy, temp);

   temp &= 0x00fff000;
   if (vtp->hblank_end >= GBE_CURSOR_XOFFSET)
      temp |= vtp->hblank_end - GBE_CURSOR_XOFFSET;
   else
      temp |= vtp->htotal - (GBE_CURSOR_XOFFSET - vtp->hblank_end);

   gbeSetReg( crm_GBE_Base, crs_start_xy, temp+0x1000);

   temp &= 0x00fff000;
   temp |= vtp->hblank_end - 4;

   gbeSetReg( crm_GBE_Base, vc_start_xy, temp);

   if (useTimingFlag == CURRENT)
   {
      gbeSetReg( crm_GBE_Base, frm_size_tile, outputFrm0);
      gbeSetReg( crm_GBE_Base, frm_size_pixel, outputFrm1);
      gbeSetReg( crm_GBE_Base, ovr_width_tile, outputOvr0);
   }
   else if (useTimingFlag == PREVIOUS)
   {
      gbeSetReg( crm_GBE_Base, frm_size_tile, gbePrevState.frm_size_tile);
      gbeSetReg( crm_GBE_Base, frm_size_pixel, gbePrevState.frm_size_pixel);
      gbeSetReg( crm_GBE_Base, ovr_width_tile, gbePrevState.ovr_width_tile);
   }

   pll_pnm = ((vtp->pll_n-1) & 0x3f) << 8;
   pll_pnm |= (((vtp->pll_p) & 0x3) << 14);
   pll_pnm |= 0x100000 | ((vtp->pll_m-1) & 0xff);
   gbeSetReg( crm_GBE_Base, dotclock, pll_pnm);

   us_delay(11*1000);
   msg_printf(VRB, "Done\n");
}


/*
 * Function:	crmGbeStartTiming
 *
 * Parameters:	(None)
 *
 * Description:	This function completes the initialization of GBE by setting
 *		the last few GBE registers for proper display.
 *
 */

void
static crmGbeStartTiming(void)
{
   int htmp, vtmp, readVal, desc1, desc2;
   crime_timing_info_t *vtp;

   if (useTimingFlag == CURRENT)
   {
      desc1 = crmNormalDescList;
      desc2 = crmOverlayDescList;
      vtp = currentTimingPtr;
   }
   else if (useTimingFlag == PREVIOUS)
   {
      desc1 = gbePrevState.frm_control;
      desc2 = gbePrevState.ovr_control;
      vtp = previousTimingPtr;
   }

   gbeSetReg( crm_GBE_Base, vt_vpixen, 0xffffff);
   gbeSetReg( crm_GBE_Base, vt_hpixen, 0xffffff);

   gbeSetReg( crm_GBE_Base, vt_xymax,
              ((vtp->vtotal & 0xFFF) << 12) |
              ((vtp->htotal & 0xFFF) << 0));

   if (useTimingFlag == CURRENT)
   {
      gbeSetReg( crm_GBE_Base, frm_size_tile, outputFrm0 | (1 << 15));
      gbeSetReg( crm_GBE_Base, frm_size_tile, outputFrm0);

      gbeSetReg( crm_GBE_Base, ovr_width_tile, outputOvr0 | (1 << 13));
      gbeSetReg( crm_GBE_Base, ovr_width_tile, outputOvr0);

      gbeSetReg( crm_GBE_Base, frm_control, crmNormalDescList);
      gbeSetReg( crm_GBE_Base, ovr_control, crmOverlayDescList);
      gbeSetReg( crm_GBE_Base, did_control, 0x00000000);
   }
   else if (useTimingFlag == PREVIOUS)
   {
      gbeSetReg( crm_GBE_Base, frm_size_tile, gbePrevState.frm_size_tile | (1 << 15));
      gbeSetReg( crm_GBE_Base, frm_size_tile, gbePrevState.frm_size_tile);

      gbeSetReg( crm_GBE_Base, ovr_width_tile, gbePrevState.ovr_width_tile | (1 << 13));
      gbeSetReg( crm_GBE_Base, ovr_width_tile, gbePrevState.ovr_width_tile);

      gbeSetReg( crm_GBE_Base, frm_control, gbePrevState.frm_control);
      gbeSetReg( crm_GBE_Base, ovr_control, gbePrevState.ovr_control);
      gbeSetReg( crm_GBE_Base, did_control, gbePrevState.did_inhwctrl);
   }

   htmp = vtp->hblank_end - 19;
   if (htmp < 0)
      htmp += vtp->htotal;    /* allow blank to wrap around */
   htmp = (htmp << 12) | ((htmp + vtp->width - 2) % vtp->htotal);

   vtmp = (vtp->vblank_start & 0xfff) | ((vtp->vblank_end & 0xfff)<<12);

   if (desc1 & 1)
   {
      while(1)
      {
         gbeGetReg( crm_GBE_Base, frm_inhwctrl, readVal);
         if ((readVal & 1) != 0)
	    break;
         else
	    us_delay(1);
      }
   }
   else if (desc2 & 1)
   {
      while(1)
      {
         gbeGetReg( crm_GBE_Base, ovr_inhwctrl, readVal);
         if ((readVal & 1) != 0)
	    break;
         else
	    us_delay(1);
      }
   }
   else
   {
      gbeSetReg( crm_GBE_Base, vt_hpixen, htmp);
      gbeSetReg( crm_GBE_Base, vt_vpixen, vtmp);
      return;
   }

   gbeWaitForBlanking( crm_GBE_Base, vtp);

   gbeSetReg( crm_GBE_Base, vt_hpixen, htmp);
   gbeSetReg( crm_GBE_Base, vt_vpixen, vtmp);

   if (timingNotSetFlag) {
     if (fpaneltype == MONITORID_7of9) { 

       /* Unless this bit is set, we cannot draw to the panel properly
	* for our diagnostic test patterns */

       msg_printf(VRB, "Altered the half-phase bit for Flat Panel 1600SW.\n");
       gbeSetReg( crm_GBE_Base, ctrlstat, 0x340aa000); 

       /* We're now ready to turn on the panel */

       i2c7of9_PanelOn( crm_GBE_Base );
     }
     else
       gbeSetReg( crm_GBE_Base, ctrlstat, 0x300aa000);
   }
}


/*****************************************/
/* Section xx: Graphics Deinitialization */
/*****************************************/

/*
 * Function:	crmDeinitGraphics
 *
 * Parameters:	(None)
 *
 * Description:	This function performs all the functions necessary for
 *		stopping graphics (and turning off GBE and the monitor
 *		if necessary).
 *
 */

void
crmDeinitGraphics(void)
{
   int i;

   if (graphicsInitializedFlag == 0)
      return;

   /* DO NOT call i2c7of9_PanelOff() 
      here, or the field diags will
      not show up on the panel at the
      end of the qnd_gfx_test! */

   crmClearFramebuffer();
   for (i = 0; i < (1 << 10); i++);
   crmGbeStop();
   crmDestroyFramebuffer();
   crmRestoreState();
   graphicsInitializedFlag = 0;
}


/*
 * Function:	crmDestroyFramebuffer
 *
 * Parameters:	(None)
 *
 * Description:	This function clears and deallocates the framebuffers used for
 *		graphics diagnostics.
 *
 */

static void
crmDestroyFramebuffer(void)
{
   unsigned int readVal;

   if (framebufferCreatedFlag == 0)
      return;

   msg_printf(VRB, "Destroying Framebuffer...");

   /* Free Allocated Memory */

   framebufferCreatedFlag = 0;
   msg_printf(VRB, "Done\n");
}


/*
 * Function:	crmRestoreState
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */

static void
crmRestoreState(void)
{
   char *c;
   int i, serialConsoleFlag = 1;
   unsigned int readVal;

   if (stateSavedFlag = 1)
   {
      msg_printf(VRB, "Restoring graphics state...");

      for (i = 0; i < 4352; i++)
      {
         gbeWaitCmapFifo( crm_GBE_Base, GBE_CMAP_WAIT_LEVEL);
         gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeSetTable( crm_GBE_Base, cmap, i, gbePrevState.cmap[i]);
      }

      for (i = 0; i < 32; i++)
      {
         gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeSetTable( crm_GBE_Base, mode_regs, i, gbePrevState.mode_regs[i]);
      }

      for (i = 0; i < 256; i++)
      {
         gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeSetTable( crm_GBE_Base, gmap, i, gbePrevState.gmap[i]);
      }

      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetReg( crm_GBE_Base, crs_pos, gbePrevState.crs_pos);
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetReg( crm_GBE_Base, crs_ctl, gbePrevState.crs_ctl);

      for (i = 0; i < 3; i++)
      {
         gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeSetTable( crm_GBE_Base, crs_cmap, i, gbePrevState.crs_cmap[i]);
      }

      for (i = 0; i < 64; i++)
      {
         gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeSetTable( crm_GBE_Base, crs_glyph, i, gbePrevState.crs_glyph[i]);
      }

      msg_printf(VRB, "Done\n");
   }
   else
   {
      msg_printf(VRB, "Graphics state not saved--nothing to restore...");
   }

   c = getenv("console");
   if (c != NULL)
   {
      if (*c == 'g')
	 serialConsoleFlag = 0;
   }

   if (serialConsoleFlag)
   {
      gbeSetReg( crm_GBE_Base, ctrlstat, 0x000aa000);
      gbeSetReg( crm_GBE_Base, vt_xy, 0x80000000);
   }
   else
   {
      useTimingFlag = PREVIOUS;
      crmGbeSetTiming();
      crmGbeStartTiming();
   }
}


/****************************************/
/* Section xx: Graphics State Functions */
/****************************************/

/*
 * Function:	isGraphicsInitialized
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */

int
isGraphicsInitialized(void)
{
   return graphicsInitializedFlag;
}


/*
 * Function:	crmSaveRETLB
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */

void
crmSaveRETLB(void)
{
   int i;

   for (i = 0; i < 64; i++)
   {
      crmGet64( crm_CRIME_Base, Tlb.fbA[i].dw, crmRETlbShadow.fbA[i].dw);
      crmGet64( crm_CRIME_Base, Tlb.fbB[i].dw, crmRETlbShadow.fbB[i].dw);
      crmGet64( crm_CRIME_Base, Tlb.fbC[i].dw, crmRETlbShadow.fbC[i].dw);
   }

   for (i = 0; i < 28; i++)
   {
      crmGet64( crm_CRIME_Base, Tlb.tex[i].dw, crmRETlbShadow.tex[i].dw);
   }

   for (i = 0; i < 4; i++)
   {
      crmGet64( crm_CRIME_Base, Tlb.cid[i].dw, crmRETlbShadow.cid[i].dw);
   }

   for (i = 0; i < 16; i++)
   {
      crmGet64( crm_CRIME_Base, Tlb.linearA[i].dw, crmRETlbShadow.linearA[i].dw);
      crmGet64( crm_CRIME_Base, Tlb.linearB[i].dw, crmRETlbShadow.linearB[i].dw);
   }
}


/*
 * Function:	crmRestoreRETLB
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */

void
crmRestoreRETLB(void)
{
   int i;

   for (i = 0; i < 64; i++)
   {
      crmSet64( crm_CRIME_Base, Tlb.fbA[i].dw, crmRETlbShadow.fbA[i].dw);
      crmSet64( crm_CRIME_Base, Tlb.fbB[i].dw, crmRETlbShadow.fbB[i].dw);
      crmSet64( crm_CRIME_Base, Tlb.fbC[i].dw, crmRETlbShadow.fbC[i].dw);
   }

   for (i = 0; i < 28; i++)
   {
      crmSet64( crm_CRIME_Base, Tlb.tex[i].dw, crmRETlbShadow.tex[i].dw);
   }

   for (i = 0; i < 4; i++)
   {
      crmSet64( crm_CRIME_Base, Tlb.cid[i].dw, crmRETlbShadow.cid[i].dw);
   }

   for (i = 0; i < 16; i++)
   {
      crmSet64( crm_CRIME_Base, Tlb.linearA[i].dw, crmRETlbShadow.linearA[i].dw);
      crmSet64( crm_CRIME_Base, Tlb.linearB[i].dw, crmRETlbShadow.linearB[i].dw);
   }
}


/************************************/
/* Section xx: Framebuffer Snooping */
/************************************/

/*
 * Function:	crmReadNormalPixel
 *
 * Parameters:	x
 *		y
 *
 * Description:	
 *
 */

unsigned int
crmReadNormalPixel(int x, int y)
{
   unsigned char *pix8TilePtr;
   unsigned short *pix16TilePtr;
   unsigned int *pix32TilePtr;

   int localX, localY, localIndex, tileX, tileY, tileIndex;
   unsigned int retVal;

   tileX     = x / maxNormalPixelsPerTileX;
   tileY     = y / maxNormalPixelsPerTileY;
   tileIndex = (tileY * maxNormalTilesX) + tileX;

   localX     = x % maxNormalPixelsPerTileX;
   localY     = y % maxNormalPixelsPerTileY;
   localIndex = (localY * maxNormalPixelsPerTileX) + localX;

   pix8TilePtr  = (unsigned char *)  crmShadowNormalDescList[tileIndex];
   pix16TilePtr = (unsigned short *) crmShadowNormalDescList[tileIndex];
   pix32TilePtr = (unsigned int *)   crmShadowNormalDescList[tileIndex];

   if (depthBits == 8)
   {
      retVal = (unsigned int) pix8TilePtr[localIndex];
   }
   else if (depthBits == 16)
   {
      retVal = (unsigned int) pix16TilePtr[localIndex];
   }
   else if (depthBits == 32)
   {
      retVal = (unsigned int) pix32TilePtr[localIndex];
   }
   else
      retVal = 0;

   return retVal;
}


/*
 * Function:	crmReadOverlayPixel
 *
 * Parameters:	x
 *		y
 *
 * Description:	
 *
 */

unsigned char
crmReadOverlayPixel(int x, int y)
{
   unsigned char *pix8TilePtr;

   int localX, localY, localIndex, tileX, tileY, tileIndex;
   unsigned int retVal;

   tileX     = x / maxOverlayPixelsPerTileX;
   tileY     = y / maxOverlayPixelsPerTileY;
   tileIndex = (tileY * maxOverlayTilesX) + tileX;

   localX     = x % maxOverlayPixelsPerTileX;
   localY     = y % maxOverlayPixelsPerTileY;
   localIndex = (localY * maxOverlayPixelsPerTileX) + localX;

   pix8TilePtr  = (unsigned char *) crmShadowOverlayDescList[tileIndex];

   retVal = (unsigned int) pix8TilePtr[localIndex];

   return retVal;
}


/*
 * Function:	crmCalculateNormalCRC
 *
 * Parameters:	x1
 *		y1
 *		x2
 *		y2
 *
 * Description:	This function calculates a CRC for the normal planes.  If
 *		the framebuffer is 8-bit or 16-bit, we just calculate
 *		the CRC for the entire framebuffer.  For a 32-bit
 *		framebuffer, we calculate the CRC for each of the four
 *		(R, G, B, and A) channels.
 *
 */

unsigned long long
crmCalculateNormalCRC(int x1, int y1, int x2, int y2)
{
   int i, j, k, l, readVal;
   int startX, startY, endX, endY;
   unsigned long long retVal = 0, temp;

   msg_printf(VRB, "Calculating framebuffer CRC...\n");

   /* Calculate "edges" of CRC */

   if (x1 < 0)
      x1 = 0;
   else if (x1 >= maxPixelsX)
      x1 = maxPixelsX-1;

   if (x2 < 0)
      x2 = 0;
   else if (x2 >= maxPixelsX)
      x2 = maxPixelsX-1;

   if (y1 < 0)
      y1 = 0;
   else if (y1 >= maxPixelsY)
      y1 = maxPixelsY-1;

   if (y2 < 0)
      y2 = 0;
   else if (y2 >= maxPixelsY)
      y2 = maxPixelsY-1;

   startX = x1;
   endX   = x2;
   startY = y1;
   endY   = y2;

   /* Calculate CRCs */

   if (depthBits == 32)
   {
      for (k = 3; k > 0; k--)
      {
         CRC16 = 0;

         for (j = startY; j <= endY; j++)
         {
            for (i = startX; i <= endX; i++)
            {
	       readVal = crmReadNormalPixel(i, j);
	       readVal &= 0x000000ff << (k * 8);
	       readVal >>= (k * 8);
	       Crc16(readVal);

	       if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
	          busy(1);
            }
         }

	 temp = CRC16;

	 for (l = 0; l < k; l++)
	    temp <<= 16;

         retVal |= temp;
      }
   }
   else
   {
      for (j = 0; j < maxPixelsY; j++)
      {
         for (i = 0; i < maxPixelsX; i++)
         {
	    readVal = crmReadNormalPixel(i, j);
	    Crc16(readVal);

	    if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
	       busy(1);
         }
      }

      retVal = CRC16;
   }

   busy(0);
   msg_printf(VRB, "Done\n");
   return retVal;
}


/*
 * Function:	crmCalculateOverlayCRC
 *
 * Parameters:	x1
 *		y1
 *		x2
 *		y2
 *
 * Description:	This function calculates a CRC for the overlay planes.
 *
 */

unsigned short
crmCalculateOverlayCRC(int x1, int y1, int x2, int y2)
{
   int i, j, k, l, readVal;
   int startX, startY, endX, endY;
   unsigned short retVal = 0;

   msg_printf(VRB, "Calculating framebuffer CRC...\n");

   CRC16 = 0;

   /* Calculate "edges" of CRC */

   if (x1 < 0)
      x1 = 0;
   else if (x1 >= maxPixelsX)
      x1 = maxPixelsX-1;

   if (x2 < 0)
      x2 = 0;
   else if (x2 >= maxPixelsX)
      x2 = maxPixelsX-1;

   if (y1 < 0)
      y1 = 0;
   else if (y1 >= maxPixelsY)
      y1 = maxPixelsY-1;

   if (y2 < 0)
      y2 = 0;
   else if (y2 >= maxPixelsY)
      y2 = maxPixelsY-1;

   startX = x1;
   endX   = x2;
   startY = y1;
   endY   = y2;

   /* Calculate CRCs */

   for (j = startY; j <= endY; j++)
   {
      for (i = startX; i <= endX; i++)
      {
	 readVal = crmReadOverlayPixel(i, j);
	 Crc16(readVal);

	 if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
	    busy(1);
      }
   }

   retVal = CRC16;

   busy(0);
   msg_printf(VRB, "Done\n");
   return retVal;
}
