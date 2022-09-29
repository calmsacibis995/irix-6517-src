/*
 * crm_init.c - PROM initialization graphics routines for Moosehead
 *
 *    This file contains the basic FINIT (firmware initialization)
 *    routines for Moosehead.
 *
 *    There are two sections, support functions and main functions.
 *    Main functions should be unaware of any underlying hardware or
 *    software mechanisms, while support functions act as the abstraction
 *    layer.
 *
 *    In general, the various sections (Support-FINIT2, Main-FINIT2,
 *    Support-FINIT3, Main-FINIT3) can be used to model startup code 
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

#ident "$Revision: 1.17 $"

#include <sgidefs.h>
#include <sys/IP32.h>
#include <sys/sema.h>			/* Needed for gfx.h */
#include <sys/gfx.h>			/* Needed for crime_gfx.h */
#include <sys/sbd.h>
#include <sys/fpanel.h>
#include <cursor.h>
#include <libsc.h>
#include <stand_htport.h>
#include <tiles.h>
#include <gfxgui.h>

#include "sys/crime_gfx.h"		/* Needed before crime_gbe.h */
#include "sys/crime_gbe.h"
#include "sys/crimechip.h"
#include "crmDefs.h"
#include "crm_stand.h"
#include "crm_gfx.h"
#include "crm_timing.h"
#include "gbedefs.h"

/************************************************************************/
/* External Declarations						*/
/************************************************************************/

extern void cpu_acpanic(char *str);
extern void pongui_setup(char *ponmsg, int checkgui);
extern void _errputs(char*);
extern unsigned int crmCursor[];
extern void _init_htp(void);
extern void us_delay(int);
extern int crime_i2cMonitorProbe( struct gbechip *gbe, crime_ddc_edid_t *edidp );
extern int crime_i2cValidEdid( crime_ddc_edid_t *edidp );
extern int crime_i2cFpProbe(struct gbechip *gbe);
#ifdef CRM_DUAL_CHANNEL
extern int crime_DualChannelProbe(struct gbechip *);
extern int crime_DualChannelCommand(struct gbechip *, unsigned char);
#endif /* CRM_DUAL_CHANNEL */
extern int i2c7of9_PanelOn(struct gbechip *);
extern int i2c7of9_PanelOff(struct gbechip *);
extern int i2cfp_PanelOn(struct gbechip *);
extern int i2cfp_PanelOff(struct gbechip *);

extern int _prom;

extern struct htp_state *htp;

/************************************************************************/
/* Global Declarations							*/
/************************************************************************/

Crimechip *crm_CRIME_Base;			/* CRIME Base Address	*/
CrimeCPUInterface *crm_CPU_Base;		/* CRIME Base Address	*/
struct gbechip *crm_GBE_Base;			/* GBE Base Address	*/
struct crime_info crm_ginfo;			/* Device Indep. Info	*/

extern struct tp_fncs crm_tp_fncs;		/* Textport routines	*/

int usingFlatPanel = 0;
#ifdef CRM_DUAL_CHANNEL
int crm_dc_board_installed = 0;
#endif /* CRM_DUAL_CHANNEL */

int frmWrite1, frmWrite2, frmWrite3a, frmWrite3b;


/************************************************************************/
/* Local Declarations							*/
/************************************************************************/

#define reverseBits(x)   ((unsigned int) (((unsigned char) (x & 0x80) >> 7) | \
                                          ((unsigned char) (x & 0x40) >> 5) | \
                                          ((unsigned char) (x & 0x20) >> 3) | \
                                          ((unsigned char) (x & 0x10) >> 1) | \
                                          ((unsigned char) (x & 0x08) << 1) | \
                                          ((unsigned char) (x & 0x04) << 3) | \
                                          ((unsigned char) (x & 0x02) << 5) | \
                                          ((unsigned char) (x & 0x01) << 7)))

#define crmBoardRev(x)		(((x) & (1<<12)) >> 12 | \
				 ((x) & (1<<14)) >> 13 | \
				 ((x) & (1<<16)) >> 14 | \
				 ((x) & (1<<18)) >> 15);

#define GBE_CURSOR_MAGIC	54

void turnOffGbeDma(void);
void waitForBlanking(crime_timing_info_t *vtp);
void clearTile(void);
int jumper_off(void);

/************************************************************************/
/* Support Functions (finit2)						*/
/************************************************************************/

/*
 * Function:	crmInitGraphicsBase
 *
 * Parameters:	(None)
 *
 * Description:	This function initializes the global data structures for
 *		Moosehead graphics, namely:
 *		1) The base addresses for CRIME and GBE.
 *		2) The name of the graphics system.
 *
 *		IMPORTANT NOTE: Ideally, we have these addresses set
 *		up *before* anything is done with CRIME or GBE, but
 *		since the diagnostics come first (and in post2, which
 *		may not be done before finit2), we also call this
 *		function in finit2.
 *
 */

void
crmInitGraphicsBase(void)
{
   unsigned int readVal;

   /* Initialize Base Addresses */

   crm_CPU_Base = (CrimeCPUInterface *) PHYS_TO_K1(CRM_CPU_INTERFACE_BASE_ADDRESS);
   crm_CRIME_Base = (Crimechip *) PHYS_TO_K1(CRM_RE_BASE_ADDRESS);
   crm_GBE_Base = (struct gbechip *) PHYS_TO_K1(GBECHIP_ADDR);

   /* Set up Graphics Name */

   strncpy(crm_ginfo.gfx_info.name, GFX_NAME_CRIME, GFX_INFO_NAME_SIZE);

   /* Turn on the System Board Revision Pins */

   gbeGetReg( crm_GBE_Base, ctrlstat, readVal);
   readVal &= 0x300aa000;

   if (readVal == 0x00000000)
      gbeSetReg( crm_GBE_Base, ctrlstat, 0x020aa000);

   /* Read Board Revision */

   crm_ginfo.boardrev = crmBoardRev(readVal);
}

 /*
  * Function:  crmGBEChipID
  *
  * Parameters:        (None)
  *
  * Description: Distinguishes between GBE vs. Arsenic
  *              With the use of a static variable, the PIO
  *              is done only once.  Return value is the backend
  *              ASIC's ID (1 for GBE, 2 for Arsenic).
  *
  */
 
 unsigned char
 crmGBEChipID(void)
 {
    static unsigned int gbeVersionNum = 0;
 
    if (gbeVersionNum == 0) 
      {
        gbeGetReg( crm_GBE_Base, ctrlstat, gbeVersionNum);
        gbeVersionNum &= GBE_CHIPID_MASK;
      }
 
    return (unsigned char)gbeVersionNum;
}


/*
 * Function:	matchEDIDTiming
 *
 * Parameters:	(None)
 *
 * Description:	This function attempts to match the video timing with
 *		the information given.  If this information doesn't
 *		match any known timing, we return -1.
 *
 */

static int
matchEDIDTiming(void)
{
   char timingFlag;
   int readVal;
   crime_timing_t i;
   
   crmGetRERev(readVal);
   timingFlag = crm_ginfo.monitor_edid.EstablishedTimings2;

   if (timingFlag & CRM_EDID_ET2_1280_1024_75_MASK)
   {
      if (readVal == 1)
      {
         crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1280_1024_75];
         return 1;
      }
      else
      {
         crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1280_1024_60];
         return 1;
      }
   }
   else if (timingFlag & CRM_EDID_ET2_1024_768_75_MASK)
   {
      if (readVal == 1)
      {
         crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1024_768_75];
         return 1;
      }
      else
      {
         crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1024_768_60];
         return 1;
      }
   }
   else if (timingFlag & CRM_EDID_ET2_1024_768_60_MASK)
   {
      crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1024_768_60];
      return 1;
   }
   else if ((timingFlag & CRM_EDID_ET2_800_600_75_MASK) ||
            (timingFlag & CRM_EDID_ET2_800_600_72_MASK))
   {
      if (readVal == 1)
      {
         crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_800_600_72];
         return 1;
      }
      else
      {
         crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_800_600_60];
         return 1;
      }
   }

   timingFlag = crm_ginfo.monitor_edid.EstablishedTimings1;

   if (timingFlag & CRM_EDID_ET1_800_600_60_MASK)
   {
      crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_800_600_60];
      return 1;
   }
   else if ((timingFlag & CRM_EDID_ET1_640_480_75_MASK) ||
            (timingFlag & CRM_EDID_ET1_640_480_72_MASK) ||
            (timingFlag & CRM_EDID_ET1_640_480_60_MASK))
   {
      crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_640_480_60];
      return 1;
   }
   else
   {
      crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1280_1024_60];
      return 1;
   }
}

/*
 * Function:	initDisplay
 *
 * Parameters:	(None)
 *
 * Description:	This function probes for a monitor and flat panel and
 *		determines a timing table to use for display.
 *
 */

static void
initDisplay(void)
{
   char *monitorVar;
   int i;

   /*
    * Probe for the following displays:
    * 1) Get the monitor display information
    * 2) Check for the flat panel and get the type if it exists
    * 3) Check for dual channel board and get the type if it exists
    */

   crime_i2cMonitorProbe( crm_GBE_Base, &crm_ginfo.monitor_edid);
   if ( !crime_i2cValidEdid( &crm_ginfo.monitor_edid ) )
   {
      monitorVar = getenv("monitor");

      if (monitorVar == NULL)
      {
	 crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1280_1024_60];
      }
      else if ((*monitorVar == 'h') || (*monitorVar == 'H'))
      {
	 crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1280_1024_60];
      }
      else if ((*monitorVar == 'l') || (*monitorVar == 'L'))
      {
	 crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1024_768_60];
      }
      else
      {
	 crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1280_1024_60];
      }
   }
   else
      matchEDIDTiming();

   /*
    * If the flat panel is found, use it first, otherwise, use the
    * monitor information.
    */

   crm_ginfo.fpaneltype = crime_i2cFpProbe( crm_GBE_Base);

   switch(crm_ginfo.fpaneltype)
   {
      case MONITORID_CORONA:		/* Use Corona Flat Panel */
	 crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1024_768_60];
	 usingFlatPanel = 1;
	 break;
      case MONITORID_ICHIBAN:		/* Use Ichiban Flat Panel */
	 crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1280_1024_60];
	 usingFlatPanel = 1;
	 break;
      case MONITORID_7of9:		/* Use 7of9 Flat Panel */
	 crm_ginfo.cur_vtp = &crimeVTimings[CRM_PROM_VT_1600_1024_50];
	 usingFlatPanel = 1;
	 break;
      default:				/* Use Monitor */
	 usingFlatPanel = 0;
	 break;
   }

#ifdef CRM_DUAL_CHANNEL
   crm_dc_board_installed = crime_DualChannelProbe( crm_GBE_Base);
#endif /* CRM_DUAL_CHANNEL */

   crm_ginfo.gfx_info.xpmax = crm_ginfo.cur_vtp->width;
   crm_ginfo.gfx_info.ypmax = crm_ginfo.cur_vtp->height;
}

/*
 * Function:	initCrime
 *
 * Parameters:	(None)
 *
 * Description:	Initializes the CRIME chip for drawing the PROM startup
 *		screen.
 *
 */

void
initCrime(void)
{
   int i;
   unsigned int temp;

   *((unsigned int *) (((unsigned int) crm_CRIME_Base) | 0x408)) = 0x00000000;
   us_delay(10);

   temp = BM_FB_A | BM_BUF_DEPTH_8 | BM_PIX_DEPTH_8 | BM_COLOR_INDEX;

   crmSet( crm_CRIME_Base, BufMode.src, temp);
   crmSet( crm_CRIME_Base, BufMode.dst, temp);

   /* Clear CRIME TLBs */

   if (_prom)
   {
      for (i = 0; i < 64; i++)
      {
         crmSet64( crm_CRIME_Base, Tlb.fbA[i].dw, 0x0000000000000000LL);
         crmSet64( crm_CRIME_Base, Tlb.fbB[i].dw, 0x0000000000000000LL);
         crmSet64( crm_CRIME_Base, Tlb.fbC[i].dw, 0x0000000000000000LL);
      }
   }

   /* Clear all of CRIME's global registers */

   for (i = 0; i < 5; i++)
      crmSet64( crm_CRIME_Base, ScrMask[i], 0x0000000000000000LL);
   
   crmSet64( crm_CRIME_Base, Scissor, 0x0000000000000000LL);
   crmSet64( crm_CRIME_Base, WinOffset.src, 0x0000000000000000LL);
   crmSet64( crm_CRIME_Base, WinOffset.dst, 0x0000000000000000LL);
}


/*
 * Function:	initFramebuffer
 *
 * Parameters:	(None)
 *
 * Description:	This function identifies the on-board Moosehead graphics,
 *		calculates the number tiles to allocate, allocates the
 *		framebuffer space, and sets the appropriate pointers in
 *		CRIME and GBE.
 *
 *		Note that for the PROM, 8 bit graphics are used, so the
 *		framebuffer consists of the overlay plane, and no normal
 *		plane.
 *
 */

static void
initFramebuffer(void)
{
   unsigned short *frmDescList;
   int i, j, k, shift, tlbIndex = 0;
   int evenTilesPerRow, spareTilesPerRow;
   int maxTilesX, maxTilesY, maxNumTiles;
   int maxExtraPixelsX, maxPixelsPerTileX, maxPixelsPerTileY;
   unsigned int outputVal, temp, tempLAddr;
   unsigned long long finalAddr, tempLLAddr;

   maxTilesX = (crm_ginfo.gfx_info.xpmax + 511)/ 512;
   maxTilesY = (crm_ginfo.gfx_info.ypmax + 127)/ 128;
   maxNumTiles = maxTilesX * maxTilesY;

   /* Clear Colormap */

   gbeSetTable( crm_GBE_Base, cmap, 0, 0x00000000); 

   /* Set up Framebuffer */

   frmDescList = (unsigned short *) PHYS_TO_K1(PROM_FB_DLIST1_BASE);

   k = 0;

   spareTilesPerRow = maxTilesX % 4;
   evenTilesPerRow = maxTilesX - spareTilesPerRow;

   for (i = 0; i < maxTilesY; i++)
   {
      tlbIndex = i * 4;

      if (evenTilesPerRow > 0)
      {
	 for (j = 0; j < evenTilesPerRow; j += 4)
	 {
	    tempLAddr = (unsigned int) PROM_FB_TILE_BASE + (k * 65536);
	    tempLLAddr = (unsigned long long) tempLAddr;
	    tempLAddr >>= 16;
	    frmDescList[k++] = (unsigned short) tempLAddr;
	    finalAddr = tempLLAddr << 32;

	    tempLAddr = (unsigned int) PROM_FB_TILE_BASE + (k * 65536);
	    tempLLAddr = (unsigned long long) tempLAddr;
	    tempLAddr >>= 16;
	    frmDescList[k++] = (unsigned short) tempLAddr;
	    finalAddr |= tempLLAddr << 16;

	    tempLAddr = (unsigned int) PROM_FB_TILE_BASE + (k * 65536);
	    tempLLAddr = (unsigned long long) tempLAddr;
	    tempLAddr >>= 16;
	    frmDescList[k++] = (unsigned short) tempLAddr;
	    finalAddr |= tempLLAddr;

	    tempLAddr = (unsigned int) PROM_FB_TILE_BASE + (k * 65536);
	    tempLLAddr = (unsigned long long) tempLAddr;
	    tempLAddr >>= 16;
	    frmDescList[k++] = (unsigned short) tempLAddr;
	    finalAddr |= tempLLAddr >> 16;

	    finalAddr |= (unsigned long long) 0x8000800080008000LL;

	    if (_prom)
               crmSet64( crm_CRIME_Base, Tlb.fbA[tlbIndex].dw, finalAddr);

	    tlbIndex++;
	 }
      }

      finalAddr = 0;
      shift = 48;

      for (j = 0; j < spareTilesPerRow; j++)
      {
	 tempLAddr = (unsigned int) PROM_FB_TILE_BASE + (k * 65536);
	 tempLLAddr = (unsigned long long) tempLAddr;
	 tempLAddr >>= 16;
	 tempLLAddr >>= 16;
	 frmDescList[k++] = (unsigned short) tempLAddr;

	 finalAddr |= tempLLAddr << shift;

         if (shift == 48)
	    finalAddr |= (unsigned long long) 0x8000000000000000LL;
         else if (shift == 32)
	    finalAddr |= (unsigned long long) 0x0000800000000000LL;
         else if (shift == 16)
	    finalAddr |= (unsigned long long) 0x0000000080000000LL;
         else if (shift == 0)
	    finalAddr |= (unsigned long long) 0x0000000000008000LL;

	 shift -= 16;
      }

      tempLLAddr = 0;

      for (j = 0; j < (4-spareTilesPerRow); j++)
      {
	 tempLLAddr <<= 16;
	 tempLLAddr |= 0x0000000000008000LL | (PROM_TEST_TILE >> 16);
      }

      finalAddr |= tempLLAddr;

      if (_prom)
         crmSet64( crm_CRIME_Base, Tlb.fbA[tlbIndex].dw, finalAddr);
   }

   /* Set the GBE registers used for display */

   maxExtraPixelsX = crm_ginfo.gfx_info.xpmax % 512;
   maxPixelsPerTileX = 512;
   maxPixelsPerTileY = 128;

   temp = maxExtraPixelsX / 32;
   frmWrite1 = temp & GBE_FRM_RHS_MASK;

   if (temp == 0)
      frmWrite1 |= (maxTilesX << GBE_OVR_WIDTH_TILE_SHIFT) & GBE_OVR_WIDTH_TILE_MASK;
   else
      frmWrite1 |= ((maxTilesX-1) << GBE_OVR_WIDTH_TILE_SHIFT) & GBE_OVR_WIDTH_TILE_MASK;

   frmWrite1 |= (GBE_FRM_DEPTH_8 << GBE_FRM_DEPTH_SHIFT) & GBE_FRM_DEPTH_MASK;

   frmWrite2 = (crm_ginfo.gfx_info.ypmax << GBE_FRM_HEIGHT_PIX_SHIFT) & GBE_FRM_HEIGHT_PIX_MASK;

   frmWrite3a = (unsigned int) K1_TO_PHYS(frmDescList);
   frmWrite3b = (unsigned int) K1_TO_PHYS(frmDescList) | GBE_FRM_DMA_ENABLE_MASK;

   /* Clear Framebuffer */

   crmSet( crm_CRIME_Base, Shade.fgColor, 0x00000000);
   crmSet( crm_CRIME_Base, DrawMode, DM_ENCOLORMASK | DM_ENCOLORBYTEMASK);
   crmSet( crm_CRIME_Base, ColorMask, 0xffffffff);
   crmSet( crm_CRIME_Base, Vertex.X[0], (0 << 16) | (0) & 0xffff);
   crmSet( crm_CRIME_Base, Vertex.X[1], ((crm_ginfo.gfx_info.xpmax - 1) << 16) | (crm_ginfo.gfx_info.ypmax - 1) & 0xffff);
   crmSet( crm_CRIME_Base, Primitive, PRIM_OPCODE_RECT | PRIM_EDGE_LR_TB);
   crmSetAndGo( crm_CRIME_Base, PixPipeNull, 0x00000000);
   crmFlush( crm_CRIME_Base);

   /* Initialize DIDs */

   if (_prom)
   {
      outputVal = (GBE_CMODE_I8 << GBE_WID_TYPE_SHIFT) & GBE_WID_TYPE_MASK;
      outputVal |= GBE_BMODE_BOTH & GBE_WID_BUF_MASK;
      outputVal |= (0x0 << GBE_WID_CM_SHIFT) & GBE_WID_CM_MASK;
      outputVal |= (0x0 << GBE_WID_GM_SHIFT) & GBE_WID_GM_MASK;

      for (i = 0; i < 32; i++)
      {
         gbeSetTable( crm_GBE_Base, mode_regs, i, outputVal); 
      }
   }
}


/*
 * Function:	initGammaMap
 *
 * Parameters:	index		The index corresponding to the graphics board
 *
 * Description:	This function initializes a linear gamma map in the
 *		GBE chip specified.
 *
 */

void
initGammaMap(void)
{
   int i, j, readVal, boardRev = 0;

   us_delay(10);
   gbeGetReg( crm_GBE_Base, ctrlstat, readVal);
   boardRev = crmBoardRev(readVal);

   for (i = 0; i < 256; i++)
   {
      if ((crm_ginfo.fpaneltype == MONITORID_CORONA) ||
          (crm_ginfo.fpaneltype == MONITORID_ICHIBAN))
         j = i;
      else if (boardRev == 0x00000000)
         j = reverseBits(i);
      else
         j = i;

      gbeSetTable( crm_GBE_Base, gmap, i, (j << 24) | (j << 16) | (j << 8));
   }
}


/*
 * Function:	initCursor
 *
 * Parameters:	(None)
 *
 * Description:	This function initializes the cursor color map and the
 *		cursor glyph registers in GBE.  The cursor glyph is
 *		defined in crm_cursor.c.  Note that the format for
 *		this cursor is *not* the same as the cursor glyph
 *		format in X (and previous PROM code).
 *
 */

static void
initCursor(void)
{
   int i;

   gbeSetTable( crm_GBE_Base, crs_cmap, 0, 0xffffff00);
   gbeSetTable( crm_GBE_Base, crs_cmap, 1, 0xff000000);

   gbeSetReg( crm_GBE_Base, crs_pos, 0x00000000);

   for (i = 0; i < 32; i++)
      gbeSetTable( crm_GBE_Base, crs_glyph, i, crmCursor[i]);

   for (i = 32; i < 64; i++)
      gbeSetTable( crm_GBE_Base, crs_glyph, i, 0x00000000);

   gbeSetReg( crm_GBE_Base, crs_ctl, 0x00000000);
}


/*
 * Function:	initTiming
 *
 * Parameters:	(None)
 *
 * Description:	This function should initialize the display.
 *
 */

static void
initTiming(void)
{
   char *refresh, *screenX, *screenY;
   int htmp, vtmp, temp, fp_wid, fp_hgt, fp_vbs, fp_vbe;
   unsigned int pll_pnm, outputVal, readVal;

   gbeGetReg( crm_GBE_Base, dotclock, readVal);
   gbeSetReg( crm_GBE_Base, dotclock, readVal & 0xffff);

   gbeSetReg( crm_GBE_Base, vt_xymax, 0x00000000);
   gbeSetReg( crm_GBE_Base, vt_vsync,
              ((crm_ginfo.cur_vtp->vsync_start & 0xFFF) << 12) |
              ((crm_ginfo.cur_vtp->vsync_end & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_hsync,
              ((crm_ginfo.cur_vtp->hsync_start & 0xFFF) << 12) |
              ((crm_ginfo.cur_vtp->hsync_end & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_vblank,
              (((crm_ginfo.cur_vtp->vblank_start) & 0xFFF) << 12) |
              (((crm_ginfo.cur_vtp->vblank_end) & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_hblank,
              (((((crm_ginfo.cur_vtp->hblank_start-5) + crm_ginfo.cur_vtp->htotal) %
		   crm_ginfo.cur_vtp->htotal) & 0xFFF) << 12) |
              (((crm_ginfo.cur_vtp->hblank_end-3) & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_hcmap,
              (((crm_ginfo.cur_vtp->hblank_start) & 0xFFF) << 12) |
              (((crm_ginfo.cur_vtp->hblank_end) & 0xFFF) << 0));
   gbeSetReg( crm_GBE_Base, vt_vcmap,
              (((crm_ginfo.cur_vtp->vblank_start) & 0xFFF) << 12) |
              (((crm_ginfo.cur_vtp->vblank_end) & 0xFFF) << 0));

   /* Turn on the flat panel */

   if (usingFlatPanel == 1)
   {
      switch(crm_ginfo.fpaneltype)
      {
         case MONITORID_CORONA:
            fp_wid=1024; fp_hgt=768; fp_vbs=771; fp_vbe=806;
	    break;
         case MONITORID_ICHIBAN:
	    fp_wid=1280; fp_hgt=1024; fp_vbs=1024; fp_vbe=1065;
	    break;
         case MONITORID_7of9:
	    fp_wid=0x640; fp_hgt=0x401;
	    break;
         default:
	    fp_wid=0xfff; fp_hgt=0xfff; fp_vbs=0xfff; fp_vbe=0xfff;
	    break;
      }
   }

#ifdef CRM_DUAL_CHANNEL
   /*
    * If Dual Channel board is installed, generate blanking
    * signals for the board.
    */
   if (crm_dc_board_installed) {
	int tmp;
	int h_offset = 0;

	/* Set PLL clock divide rate based on pixel clock rate. */
	if (crm_ginfo.cur_vtp->cfreq > 100000) {
		tmp = 0xff;	/* High */
	}
	else {
		tmp = 0x7f;	/* Low */
	}
	crime_DualChannelCommand( crm_GBE_Base, tmp);

	/*
	 * Board type		horiz. offset
	 * ----------		-------------
	 *    1 - 8			2
	 *    9 - 16			0
	 */
	if (crm_dc_board_installed <= 8)
		h_offset = 2;

	gbeSetReg( crm_GBE_Base, fp_de,	/* Set Device Enable to select head 0 */
	    ((0xfff) << 12)) | 0;	/* never turn on, turn off at line 0 */

	gbeSetReg(crm_GBE_Base, fp_hdrv,
	    (((crm_ginfo.cur_vtp->width + h_offset) & 0xfff) << 12) | h_offset);
	gbeSetReg(crm_GBE_Base, fp_vdrv,
		(((crm_ginfo.cur_vtp->height + 1) & 0xfff) << 12) | 1);
   } else {
#endif /* CRM_DUAL_CHANNEL */
     if (crm_ginfo.fpaneltype == MONITORID_7of9) {
       gbeSetReg( crm_GBE_Base, fp_hdrv,
		  (fp_wid & 0xfff) |
		  ((0x0 & 0xfff) << 12));
       gbeSetReg( crm_GBE_Base, fp_vdrv,
		  (fp_hgt & 0xfff) |
		  ((0x1  & 0xfff) << 12));
       
       /* We have the option of powersaving the CRT during 7of9 operation in
	* the PROM. After some thought on the subject, we decided 
	* to make sure that the main monitor works alongside the 7of9 for
	* use in making software installation and field support easier.
	* The code below just makes sure that the CRT is not powersaved
	* in general operation */
	
	gbeGetReg( crm_GBE_Base, vt_flags, readVal);
	
	readVal &= GBE_POWERSAVE_MASK;
	readVal |= GBE_POWERSAVE_OFF;
	
	gbeSetReg( crm_GBE_Base, vt_flags, readVal); 

       /* XXX Note: vt_hblank and vt_vblank MUST be left at their
	* proper settings or GBE/Arsenic will hang the PROM.
	* Unfortunately, these two signals drive just about all of GBE's
	* logic and removing their settings will cause GBE to bail out
	* on PIOs.  Use VT6 (vt_flags) to powersave the monitor.
	*/

     } else {
	gbeSetReg( crm_GBE_Base, fp_de,
              (fp_vbe & 0xfff) |
              ((fp_vbs & 0xfff) << 12));
	gbeSetReg( crm_GBE_Base, fp_hdrv,
              (fp_wid & 0xfff) |
              ((0x0 & 0xfff) << 12));
	gbeSetReg( crm_GBE_Base, fp_vdrv,
              (fp_hgt+1 & 0xfff) |
              ((0x1  & 0xfff) << 12));
     }
#ifdef CRM_DUAL_CHANNEL
   }
#endif /* CRM_DUAL_CHANNEL */

   temp = crm_ginfo.cur_vtp->vblank_start - crm_ginfo.cur_vtp->vblank_end - 1;
   if (temp > 0)
      temp = -temp;
   temp &= 0xfff;
   temp <<= 12;
   if (crm_ginfo.cur_vtp->hblank_end >= 20)
      temp |= crm_ginfo.cur_vtp->hblank_end - 20;
   else
      temp |= crm_ginfo.cur_vtp->htotal - (20 - crm_ginfo.cur_vtp->hblank_end);

   gbeSetReg( crm_GBE_Base, did_start_xy, temp);

   temp &= 0xfff000;
   if (crm_ginfo.cur_vtp->hblank_end >= GBE_CURSOR_MAGIC)
      temp |= crm_ginfo.cur_vtp->hblank_end - GBE_CURSOR_MAGIC;
   else
      temp |= crm_ginfo.cur_vtp->htotal - (GBE_CURSOR_MAGIC - crm_ginfo.cur_vtp->hblank_end);

   gbeSetReg( crm_GBE_Base, crs_start_xy, temp + 0x1000);

   temp &= 0x00fff000;
   temp |= crm_ginfo.cur_vtp->hblank_end - 4;

   gbeSetReg( crm_GBE_Base, vc_start_xy, temp);

   gbeSetReg( crm_GBE_Base, frm_size_tile, frmWrite1);
   gbeSetReg( crm_GBE_Base, frm_size_pixel, frmWrite2);

   pll_pnm = ((crm_ginfo.cur_vtp->pll_n-1) & 0x3f) << 8;
   pll_pnm |= (((crm_ginfo.cur_vtp->pll_p) & 0x3) << 14);
   pll_pnm |= 0x100000 | ((crm_ginfo.cur_vtp->pll_m-1) & 0xff);
   gbeSetReg( crm_GBE_Base, dotclock, pll_pnm);

   us_delay(11*1000);

   gbeSetReg( crm_GBE_Base, vt_vpixen, 0xffffff);
   gbeSetReg( crm_GBE_Base, vt_hpixen, 0xffffff);

   gbeSetReg( crm_GBE_Base, vt_xymax,
              ((crm_ginfo.cur_vtp->vtotal & 0xFFF) << 12) |
              ((crm_ginfo.cur_vtp->htotal & 0xFFF) << 0));

   gbeSetReg( crm_GBE_Base, frm_size_tile, frmWrite1 | (1 << 15));
   gbeSetReg( crm_GBE_Base, frm_size_tile, frmWrite1);

   gbeSetReg( crm_GBE_Base, ovr_width_tile, 0x00000000 | (1 << 13));
   gbeSetReg( crm_GBE_Base, ovr_width_tile, 0x00000000);

   gbeSetReg( crm_GBE_Base, frm_control, frmWrite3b);
   gbeSetReg( crm_GBE_Base, did_control, 0x00000000);

   htmp = crm_ginfo.cur_vtp->hblank_end - 19;
   if (htmp < 0)
      htmp += crm_ginfo.cur_vtp->htotal;    /* allow blank to wrap around */
   htmp = (htmp << 12) |
          ((htmp + crm_ginfo.cur_vtp->width - 2) % crm_ginfo.cur_vtp->htotal);

   vtmp = (crm_ginfo.cur_vtp->vblank_start & 0xfff) |
          ((crm_ginfo.cur_vtp->vblank_end & 0xfff)<<12);

   while(1)
   {
      gbeGetReg( crm_GBE_Base, frm_inhwctrl, readVal);
      if ((readVal & 1) != 0)
         break;
      else
         us_delay(1);
   }

   waitForBlanking( crm_ginfo.cur_vtp);

   gbeSetReg( crm_GBE_Base, vt_hpixen, htmp);
   gbeSetReg( crm_GBE_Base, vt_vpixen, vtmp);

   gbeGetReg( crm_GBE_Base, ctrlstat, readVal);
   readVal &= 0x020aa000;

   if (readVal == 0x020aa000)
   {
      if (usingFlatPanel)
      {
	if (crm_ginfo.fpaneltype != MONITORID_7of9) {
	    i2cfp_PanelOn( crm_GBE_Base );
	} else {
	    /* 7of9-specific function call */
	    i2c7of9_PanelOn( crm_GBE_Base );	  
	}
      }

      gbeSetReg( crm_GBE_Base, ctrlstat, 0x300aa000);
   }
}

/*
 * Function:	turnOnGbe
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */

void
turnOnGbe(void)
{
   int i;
   unsigned int readVal;

   gbeGetReg( crm_GBE_Base, vt_xy, readVal);
   readVal &= 0x80000000;

   if (readVal != 0x00000000)
   {
      for (i = 0; i < 100; i++)
      {
         gbeSetReg( crm_GBE_Base, vt_xy, 0x00000000);
	 us_delay(1);
         gbeGetReg( crm_GBE_Base, vt_xy, readVal);
         readVal &= 0x80000000;

	 if (readVal != 0x80000000) 
	    break;
      }
   }
   else
      turnOffGbeDma();
}

/*
 * Function:	turnOffGbeDma
 *
 * Parameters:	(None)
 *
 * Description:	This should turn off the monitor and GBE.  This is used
 *		when switching between the serial console and the graphics
 *		console.
 *
 */

void
turnOffGbeDma(void)
{
   int i;
   unsigned int readVal;

   /* Check to see if things are turned off.  If so, then we can just */
   /* return.							      */

   gbeGetReg( crm_GBE_Base, ctrlstat, readVal);
   readVal &= 0x300aa000;

   if ((readVal != 0x300aa000) && (readVal != 0x000aa000))
      return;

   gbeGetReg( crm_GBE_Base, vt_xy, readVal);
   readVal &= 0x80000000;

   if (readVal != 0x00000000)
      return;

   /* Otherwise, turn off GBE */

   gbeGetReg( crm_GBE_Base, ovr_control, readVal);
   gbeSetReg( crm_GBE_Base, ovr_control, readVal & ~1);
   us_delay(1000);
   gbeGetReg( crm_GBE_Base, frm_control, readVal);
   gbeSetReg( crm_GBE_Base, frm_control, readVal & ~1);
   us_delay(1000);
   gbeGetReg( crm_GBE_Base, did_control, readVal);
   gbeSetReg( crm_GBE_Base, did_control, readVal & ~0x10000);
   us_delay(1000);

   for (i = 0; i < 10000; i++)
   {
      gbeGetReg( crm_GBE_Base, frm_inhwctrl, readVal);
      readVal &= 0x00000001;

      if (readVal == 0x00000000)
         us_delay(10);
      else
      {
         gbeGetReg( crm_GBE_Base, ovr_inhwctrl, readVal);
         readVal &= 0x00000001;

         if (readVal == 0x00000000)
            us_delay(10);
         else
	 {
            gbeGetReg( crm_GBE_Base, did_inhwctrl, readVal);
            readVal &= 0x00010000;

            if (readVal == 0x00000000)
               us_delay(10);
            else
	       break;
	 }
      }
   }

   gbeSetReg( crm_GBE_Base, frm_control, (frmWrite3b & ~1) + 32);

   for (i = 0; i < 20000; i++)
   {
      gbeGetReg( crm_GBE_Base, frm_inhwctrl, readVal);

      if (readVal != ((frmWrite3b & ~1) + 32))
         us_delay(10);
      else
         break;
   }
}

/*
 * Function:    waitForBlanking
 *
 * Parameters:  (None)
 *
 * Description:
 *
 */

void
waitForBlanking(crime_timing_info_t *vtp)
{
   int maxX, maxY;
   int readVal, tempX, tempY;

   maxX = vtp->hblank_end - vtp->hblank_start;
   maxX /= 2;
   maxX += vtp->hblank_start;

   maxY = vtp->vblank_end - vtp->vblank_start;
   maxY /= 2;
   maxY += vtp->vblank_start;

   while(1)
   {
      gbeGetReg( crm_GBE_Base, vt_xy, readVal);
      tempX = readVal & 0x00000fff;
      tempY = (readVal & 0x00fff000) >> 12;

      if (((tempX >= (vtp->hblank_start)) && (tempX <= maxX)) ||
          ((tempY >= (vtp->vblank_start)) && (tempY <= maxY)))
      {
         break;
      }
   }
}

/************************************************************************/
/* Main Functions (finit2)						*/
/*									*/
/*    These functions should be called by the firmware initialization	*/
/*    routines to set up the graphics hardware and bring up the		*/
/*    graphics console.							*/
/*									*/
/*    The following needs to be done (in proper order):			*/
/*									*/
/*    1) Initialize the global variables (whereas Newport did this	*/
/*	 during the declaration of the variable).			*/
/*    2) Get the monitor information (for flat panel and monitor) via	*/
/*       the I2C bus.							*/
/*    3) Initialize CRIME's registers.					*/
/*    4) Initialize GBE's registers.					*/
/*    5) Initialize the gamma map.					*/
/*    6) Allocate the tiles for the normal and overlay planes, while	*/
/*       setting up the appropriate pointers.				*/
/*									*/
/*    At this point, only the graphics software (e.g. PROM data		*/
/*    structures for the startup screen and GUI) need to be		*/
/*    initialized.							*/
/************************************************************************/

/*
 * Function:	initGraphics
 *
 * Parameters:	(None)
 *
 * Description:	Performs graphics initialization
 *
 */

void
initGraphics(int functionCode)
{
   char *cp;
   int graphicsConsoleFlag = 0;
   unsigned int readVal;

   crmInitGraphicsBase();

   cp = getenv("console");
   if (cp != NULL)
   {
      if (*cp == 'g' || *cp == 'G' || jumper_off())
         graphicsConsoleFlag = 1;
   }

   crmGetRev(readVal);

   if (readVal == 0x000000a1)
   {
      turnOffGbeDma();

      if (graphicsConsoleFlag)
      {
         initDisplay();
         initCrime();
         initGammaMap();
	 turnOnGbe();
         initFramebuffer();
         initTiming();
         initCursor();
      }
      else
      {
	 if (usingFlatPanel)
	 {
	   if (crm_ginfo.fpaneltype != MONITORID_7of9) {
		i2cfp_PanelOff( crm_GBE_Base );
	   } else {
		/* 7of9-specific function call */
		i2c7of9_PanelOff( crm_GBE_Base );
	   }
	 }

         gbeSetReg( crm_GBE_Base, vt_xy, 0x80000000);
         gbeSetReg( crm_GBE_Base, ctrlstat, 0x020aa000);
      }
   }
}


/*
 * Function:	initConsole
 *
 * Parameters:	(None)
 *
 * Description:	This function performs the same functionality as
 *		pon_morediags() in the IP22 PROM (ide/pon/pon_more.c).
 *
 *		By the end of this function, we should enter POST2
 *		to perform the remaining diagnostics.  Then, we can
 *		call pongui_cleanup() at the beginning of FINIT3.
 *
 */

void
initConsole(void)
{
   static char *ponMsg = "Running power-on diagnostics...";
   char *cp;
   int serialConsoleFlag = 1;
   void miscTest(void);

   /* Here, we output the power-on message to the serial port if	*/
   /* the console environment variable is set to the serial port.	*/

   cp = getenv("console");
   if (cp != NULL)
   {
      if (*cp == 'g' || *cp == 'G')
         serialConsoleFlag = 0;
   }

   if (serialConsoleFlag)
   {
      pongui_setup(ponMsg, 0);
   }
   else
   {
      pongui_setup(ponMsg, 1);
   }
}


/************************************************************************/
/* Notes on Remaining Graphics Stuff					*/
/*									*/
/*    At this point, everything for the PROM graphics, namely the	*/
/*    host textport routines should be ready to go.			*/
/*									*/
/************************************************************************/

void
clearTile(void)
{
   int i;
   unsigned int *tilePtr = (unsigned int *) PHYS_TO_K1(PROM_TEST_TILE);

   for (i = 0; i < 16384; i++)
   {
      tilePtr[i] = 0x00000000;
   }
}
