/*
 * crmGBECommands.c
 *
 *    This file contains the functions for initializing GBE registers.
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

#include <sys/crime_gfx.h>
#include <sys/crime_gbe.h>
#include "crmDefs.h"
#include "gbedefs.h"

/************************************************************************/
/* External Declarations						*/
/************************************************************************/

extern struct gbechip *crm_GBE_Base;


/************************************************************************/
/* Functions								*/
/************************************************************************/

unsigned char crmGBEChipID(void);

/****************************/
/* Section xx: IDE Commands */
/****************************/


/**********************************/
/* Section xx: GBE Initialization */
/**********************************/

void
crmInitGBE(void)
{
   int i;
   unsigned int readVal;

   gbeSetReg( crm_GBE_Base, ovr_width_tile, 0x00000000);
   gbeSetReg( crm_GBE_Base, ovr_control,    0x00000000);

   while(1)
   {
      gbeGetReg( crm_GBE_Base, ovr_inhwctrl, readVal);

      if (readVal == 0x00000000)
	 break;
   }

   gbeSetReg( crm_GBE_Base, frm_size_tile,  0x00000000);
   gbeSetReg( crm_GBE_Base, frm_size_pixel, 0x00000000);
   gbeSetReg( crm_GBE_Base, frm_control,    0x00000000);

   while(1)
   {
      gbeGetReg( crm_GBE_Base, frm_inhwctrl, readVal);

      if (readVal == 0x00000000)
	 break;
   }

   gbeSetReg( crm_GBE_Base, did_control, 0x00000000);

   while(1)
   {
      gbeGetReg( crm_GBE_Base, did_inhwctrl, readVal);

      if (readVal == 0x00000000)
	 break;
   }

   for (i = 0; i < GBE_NUM_WIDS; i++)
   {
      gbeSetTable( crm_GBE_Base, mode_regs, i, 0x00000000);
   }

   for (i = 0; i < GBE_NUM_CMAP_ENTRY; i++)
   {
      gbeSetTable( crm_GBE_Base, cmap, i, 0x00000000);
      gbeWaitCmapFifo( crm_GBE_Base, GBE_CMAP_WAIT_LEVEL);
   }

   for (i = 0; i < GBE_NUM_GMAP_ENTRY; i++)
   {
      gbeSetTable( crm_GBE_Base, gmap, i, 0x00000000);
   }

   gbeSetReg( crm_GBE_Base, crs_pos,     0x00000000);
   gbeSetReg( crm_GBE_Base, crs_ctl,     0x00000000);
   gbeSetReg( crm_GBE_Base, crs_cmap[0], 0x00000000);
   gbeSetReg( crm_GBE_Base, crs_cmap[1], 0x00000000);
   gbeSetReg( crm_GBE_Base, crs_cmap[2], 0x00000000);


   for (i = 0; i < GBE_NUM_GMAP_ENTRY; i++)
   {
      gbeSetTable( crm_GBE_Base, crs_glyph, i, 0x00000000);
   }
}


/*
 * Function:	crmGBE_CursorSwitch
 *
 * Parameters:	flag
 *
 * Description:	
 *
 */

void
crmGBE_CursorSwitch(int flag)
{
   if (flag == 0)
      gbeSetReg( crm_GBE_Base, crs_ctl, 0x00000000);
   else if (flag == 2)
      gbeSetReg( crm_GBE_Base, crs_ctl, 0x00000003);
   else
      gbeSetReg( crm_GBE_Base, crs_ctl, 0x00000001);
}


/*********************************/
/* Section xx: GBE Fifo Commands */
/*********************************/

/*
 * Function:	crmGBEWaitCmapFifo
 *
 * Parameters:	(None)
 *
 * Description:
 *
 */

void
crmGBEWaitCmapFifo(void)
{
  unsigned int readVal;
  unsigned char gbeVersion;

  gbeVersion = crmGBEChipID();


  if (gbeVersion >= 2)     /* We have an Arsenic chip */
    {
      while (1)
	{
	  gbeGetReg(crm_GBE_Base, cm_fifo, readVal);
	  readVal &= GBE_CMFIFO_MASK;

	  /*
           * Arsenic correctly handles the cmap fifo indicator register.
           * The FIFO is now limited to 63 entries when full.  Thus,
           * 0x0 means FULL and 0x3F means EMPTY. The test below just makes
           * sure that there are at least GBE_CMAP_WAIT_LEVEL+1 slots 
           * available in the Arsenic cmap FIFO.
           */

	  if (readVal > GBE_CMAP_WAIT_LEVEL)
	    return;
	}
    }

  else                     /* We have a GBE chip */
    {
      while(1)
	{
	  gbeGetReg(crm_GBE_Base, cm_fifo, readVal);
	  readVal &= GBE_CMFIFO_MASK;

	  /* 
           * GBE Hack since the cm_fifo register can overflow; 0x0
           * in cm_fifo on GBE could mean the fifo is full OR
           * empty, there is no way to tell.  So we must also test for the 
           * zero state here, even though that might be incorrect.
           */

	  if ((readVal == GBE_CMAP_FIFO_FULL_OR_EMPTY) ||
	      (readVal > GBE_CMAP_WAIT_LEVEL))
	    return;
	}
    }
}


/*
 * Function:	crmGBEWaitCmapFifoEmpty
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */

void
crmGBEWaitCmapFifoEmpty(void)
{
   unsigned int readVal;
   unsigned char gbeVersion;

   gbeVersion = crmGBEChipID();


   if (gbeVersion >= 2)    /* We have an Arsenic chip */
      {
        while(1)
        {
          gbeGetReg(crm_GBE_Base, cm_fifo, readVal);
          readVal &= GBE_CMFIFO_MASK;
          
          /*
           * Arsenic correctly handles the cmap fifo indicator register.
           * The FIFO is now limited to 63 entries when full.  Thus,
           * 0x0 means FULL and 0x3F means EMPTY. The test below queries the
           * Arsenic chip to see if _all_ slots are available.
           */
          
          if (readVal == ARSENIC_CMAP_FIFO_EMPTY)
            return;
        }
      }
    
    else                       /* We have a GBE chip */
      {
        while(1)
        {
          gbeGetReg( crm_GBE_Base, cm_fifo, readVal);
          readVal &= GBE_CMFIFO_MASK;
          
          /* 
           * GBE Hack since the cm_fifo register can overflow; 0x0
           * in cm_fifo on GBE could mean the fifo is full OR
           * empty, there is no way to tell.  So we must test for the zero
           * state here, even though that might be incorrect.
           */
          
          if (readVal == GBE_CMAP_FIFO_FULL_OR_EMPTY)
            return;
        }
      }
}


/*
 * Function:	crmGBEWaitForBlanking
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */

void
crmGBEWaitForBlanking(crime_timing_info_t *vtp)
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
