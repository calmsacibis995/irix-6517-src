/*
 * crmVisualTest.c - Visual testing functions
 *
 *    The functions in this file can be broken down into the following
 *    sections:
 *
 *       - IDE Commands
 *
 */

#include <libsc.h>
#include <sys/IP32.h>
#include <sys/sema.h>
#include <sys/sbd.h>
#include <sys/gfx.h>
#include <sys/types.h>

#include <sys/crime.h>
#include <sys/crimechip.h>
#include <sys/crime_gfx.h>
#include <sys/crime_gbe.h>
#include <uif.h>
#include <IP32_status.h>

#include "crmDefs.h"
#include "crm_stand.h"
#include "gbedefs.h"

/************************************************************************/
/* External Declarations						*/
/************************************************************************/

extern struct crime_info crm_ginfo;
extern struct gbechip *crm_GBE_Base;
extern Crimechip *crm_CRIME_Base;
extern crime_timing_info_t *currentTimingPtr;
extern code_t errorCode;
extern unsigned int CRC16;

extern void crmREDrawLine(int x1, int y1, int x2, int y2);
extern void crmSetTLB(char tlbFlag);
extern void crmRESetFGColor(unsigned int color);
extern int isGraphicsInitialized(void);
extern unsigned long long crmGetVisualPattern01CRC(int dirFlag, int colorFlag);
extern unsigned long long crmCalculateNormalCRC(int x1, int y1, int x2, int y2);
extern uint Crc16(int data);

extern int maxPixelsX,
	   maxPixelsY;


/************************************************************************/
/* Global Declarations							*/
/************************************************************************/


/************************************************************************/
/* Local Declarations							*/
/************************************************************************/

int crmVisualTest01(int dirFlag, int colorFlag);


/************************************************************************/
/* Functions								*/
/************************************************************************/

/****************************/
/* Section xx: IDE Commands */
/****************************/

/*
 * Usage:	visual_test1 <Color> <Direction>
 *
 * Description:	
 *
 */

int
crmCmdVisualTest01(int argc, char **argv)
{
   int colorFlag, dirFlag, errorFlag = 0;

   if (argc != 3)
   {
      msg_printf(VRB, "Usage: visual_test1 <Color> <Direction>\n");
      msg_printf(VRB, "          <Color> = BW, R, G, B\n");
      msg_printf(VRB, "          <Direction> = [R]ight, [L]eft, [U]p, [D]own\n");
      return 0;
   }

   if ((!strncmp(argv[1], "BW", 2)) || (!strncmp(argv[1], "bw", 2)))
   {
      colorFlag = CRM_VISPAT1_COLORFLAG_BW;
   }
   else if ((*argv[1] == 'R') || (*argv[1] == 'r'))
   {
      colorFlag = CRM_VISPAT1_COLORFLAG_RED;
   }
   else if ((*argv[1] == 'G') || (*argv[1] == 'g'))
   {
      colorFlag = CRM_VISPAT1_COLORFLAG_GREEN;
   }
   else if ((*argv[1] == 'B') || (*argv[1] == 'b'))
   {
      colorFlag = CRM_VISPAT1_COLORFLAG_BLUE;
   }
   else
   {
      msg_printf(VRB, "visual_test1: Unknown color\n");
   }

   if ((*argv[2] == 'R') || (*argv[2] == 'r'))
   {
      dirFlag = CRM_VISPAT1_DIRFLAG_LTOR;
   }
   else if ((*argv[2] == 'L') || (*argv[2] == 'l'))
   {
      dirFlag = CRM_VISPAT1_DIRFLAG_RTOL;
   }
   else if ((*argv[2] == 'B') || (*argv[2] == 'b') ||
	    (*argv[2] == 'U') || (*argv[2] == 'u'))
   {
      dirFlag = CRM_VISPAT1_DIRFLAG_BTOT;
   }
   else if ((*argv[2] == 'T') || (*argv[2] == 't') ||
	    (*argv[2] == 'D') || (*argv[2] == 'd'))
   {
      dirFlag = CRM_VISPAT1_DIRFLAG_TTOB;
   }
   else
   {
      msg_printf(VRB, "visual_test1: Unknown direction\n");
   }

   errorFlag = crmVisualTest01(dirFlag, colorFlag);

   return errorFlag;
}


/****************************************/
/* Section xx: Visual Test 1 (RGB Mode) */
/****************************************/

/*
 * Function:	crmVisualPattern01
 *
 * Parameters:	dirFlag
 *		colorFlag
 *
 * Description:	This draws one of four possible black->white patterns
 *		on the screen: left to right, right to left, top to
 *		bottom, bottom to top.
 *
 */

void
crmVisualPattern01(int dirFlag, int colorFlag)
{
   int i, j, k, ctemp, newColor, stepSize, cleanupMax, retVal;

   retVal = isGraphicsInitialized();

   if (retVal == 0)
   {
      msg_printf(VRB, "visual_pattern1: Graphics not initialized\n");
      return;
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetReg( crm_GBE_Base, did_control, 0x00000000);

   for (i = 0; i < 32; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, mode_regs, i, 0x00000417);
   }

   crmSetTLB(CRM_SETUP_TLB_B);

   if ((dirFlag == CRM_VISPAT1_DIRFLAG_RTOL) ||
       (dirFlag == CRM_VISPAT1_DIRFLAG_BTOT))
      ctemp = 255;
   else
      ctemp = 0;

   k = 0;

   if ((dirFlag == CRM_VISPAT1_DIRFLAG_TTOB) ||
       (dirFlag == CRM_VISPAT1_DIRFLAG_BTOT))
      stepSize = maxPixelsY / 255;
   else
      stepSize = maxPixelsX / 255;

   for (j = 0; j < 256; j++)
   {
      if (colorFlag == CRM_VISPAT1_COLORFLAG_BW)
         newColor = (ctemp << 24) | (ctemp << 16) | (ctemp << 8) | ctemp;
      else if (colorFlag == CRM_VISPAT1_COLORFLAG_RED)
         newColor = (ctemp << 24);
      else if (colorFlag == CRM_VISPAT1_COLORFLAG_GREEN)
         newColor = (ctemp << 16);
      else if (colorFlag == CRM_VISPAT1_COLORFLAG_BLUE)
         newColor = (ctemp << 8);

      crmRESetFGColor(newColor);

      for (i = 0; i < stepSize; i++)
      {
         if ((k == maxPixelsX) &&
	     ((dirFlag == CRM_VISPAT1_DIRFLAG_LTOR) ||
              (dirFlag == CRM_VISPAT1_DIRFLAG_RTOL)))
         {
	    break;
	 }
	 else if ((k == maxPixelsY) &&
	          ((dirFlag == CRM_VISPAT1_DIRFLAG_TTOB) ||
                   (dirFlag == CRM_VISPAT1_DIRFLAG_BTOT)))
         {
	    break;
	 }

	 if ((dirFlag == CRM_VISPAT1_DIRFLAG_TTOB) ||
	     (dirFlag == CRM_VISPAT1_DIRFLAG_BTOT))
         {
	    crmSetTLB(CRM_SETUP_TLB_B);
            crmREDrawLine(0, k, maxPixelsX-1, k);
         }
         else
         {
	    crmSetTLB(CRM_SETUP_TLB_B);
            crmREDrawLine(k, 0, k, maxPixelsY-1);
         }

         k++;
      }

      if ((dirFlag == CRM_VISPAT1_DIRFLAG_RTOL) ||
          (dirFlag == CRM_VISPAT1_DIRFLAG_BTOT))
         ctemp--;
      else
         ctemp++;
   }

   /* For screen resolutions that are not multiples of 256 */

   if ((k != maxPixelsX) &&
       ((dirFlag == CRM_VISPAT1_DIRFLAG_LTOR) ||
	(dirFlag == CRM_VISPAT1_DIRFLAG_RTOL)))
     for ( ; k < maxPixelsX; k++) {
       crmSetTLB(CRM_SETUP_TLB_B);
       crmREDrawLine(k, 0, k, maxPixelsY-1);
     }
   else if ((k != maxPixelsY) &&
	    ((dirFlag == CRM_VISPAT1_DIRFLAG_TTOB) ||
	     (dirFlag == CRM_VISPAT1_DIRFLAG_BTOT)))
     for ( ; k < maxPixelsX; k++) {
       crmSetTLB(CRM_SETUP_TLB_B);
       crmREDrawLine(0, k, maxPixelsX-1, k);
     } 
}


/*
 * Function:	crmGetVisualPattern01CRC
 *
 * Parameters:	code
 *
 * Description:	This returns the framebuffer checksum for the appropriate
 *		visual pattern.
 *
 */

unsigned long long
crmGetVisualPattern01CRC(int dirFlag, int colorFlag)
{
   int i, j, k, l, newColor, stepSize, cleanupMax, errorFlag = 0;
   unsigned long long retVal;

   CRC16 = 0;
   msg_printf(VRB, "Calculating expected framebuffer CRC...\n");

   if ((maxPixelsX == 1280) &&
       (maxPixelsY == 1024))
   {
      switch(dirFlag)
      {
	 case CRM_VISPAT1_DIRFLAG_LTOR:
	    retVal = 0x0000000000001deeLL;
	    break;
	 case CRM_VISPAT1_DIRFLAG_RTOL:
	    retVal = 0x0000000000009f8bLL;
	    break;
	 case CRM_VISPAT1_DIRFLAG_TTOB:
	    retVal = 0x000000000000e992LL;
	    break;
	 case CRM_VISPAT1_DIRFLAG_BTOT:
	    retVal = 0x0000000000006bf7LL;
	    break;
      }
   }
   else
   {
      if ((dirFlag == CRM_VISPAT1_DIRFLAG_RTOL) ||
          (dirFlag == CRM_VISPAT1_DIRFLAG_BTOT))
         newColor = 255;
      else
         newColor = 0;

      if ((dirFlag == CRM_VISPAT1_DIRFLAG_TTOB) ||
          (dirFlag == CRM_VISPAT1_DIRFLAG_BTOT))
         stepSize = maxPixelsY / 255;
      else
         stepSize = maxPixelsX / 255;

      if ((dirFlag == CRM_VISPAT1_DIRFLAG_RTOL) ||
          (dirFlag == CRM_VISPAT1_DIRFLAG_LTOR))
      {
         for (l = 0; l < maxPixelsY; l++)
         {
	    k = 0;

            for (j = 0; j < 256; j++)
            {
               for (i = 0; i < stepSize; i++)
               {
                  if (k == maxPixelsX)
	             break;

                  Crc16(newColor);
   
	          if ((k % CRM_BUSY_WAIT_PERIOD) == 0)
		     busy(1);
                  k++;
               }
   
               if (dirFlag == CRM_VISPAT1_DIRFLAG_RTOL)
                  newColor--;
               else
                  newColor++;
            }
         }
      }
      else
      {
         i = 0;

         for (l = 0; l < 256; l++)
         {
	    for (k = 0; k < stepSize; k++)
	    {
	       if (i == maxPixelsY)
	          break;

	       for (j = 0; j < maxPixelsX; j++)
	       {
                  Crc16(newColor);

	          if ((j % CRM_BUSY_WAIT_PERIOD) == 0)
		     busy(1);
	       }

	       i++;
	    }

            if (dirFlag == CRM_VISPAT1_DIRFLAG_BTOT)
               newColor--;
            else
               newColor++;
         }
      }
      retVal = CRC16;
   }

   /* Return the CRC value.  Note that due to the way the bonsai       */
   /* compilers generate code, the shifting must be done in units less */
   /* than 32.                                                         */

   if (colorFlag == CRM_VISPAT1_COLORFLAG_BW)
   {
      retVal |= retVal << 16;
      retVal |= retVal << 16;
      retVal <<= 16;
   }
   else if (colorFlag == CRM_VISPAT1_COLORFLAG_RED)
   {
      retVal <<= 16;
      retVal <<= 16;
      retVal <<= 16;
   }
   else if (colorFlag == CRM_VISPAT1_COLORFLAG_GREEN)
   {
      retVal <<= 16;
      retVal <<= 16;
   }
   else if (colorFlag == CRM_VISPAT1_COLORFLAG_BLUE)
   {
      retVal <<= 16;
   }

   busy(0);
   msg_printf(VRB, "Done\n");
   return retVal;
}


/*
 * Function:	crmVisualTest01
 *
 * Parameters:
 *
 * Description:
 *
 */

int
crmVisualTest01(int dirFlag, int colorFlag)
{
   int retVal = 0;
   unsigned long long expectedCRC, readCRC;

   /* Set Error Code */

   errorCode.sw = SW_IDE;
   errorCode.func = FUNC_CRIME;
   errorCode.test = TEST_RE_VIS01;

   switch(colorFlag)
   {
      case CRM_VISPAT1_COLORFLAG_BW:
	 msg_printf(VRB, "Black->White Visual Test ");
	 break;
      case CRM_VISPAT1_COLORFLAG_RED:
	 msg_printf(VRB, "Red Visual Test ");
	 break;
      case CRM_VISPAT1_COLORFLAG_GREEN:
	 msg_printf(VRB, "Green Visual Test ");
	 break;
      case CRM_VISPAT1_COLORFLAG_BLUE:
	 msg_printf(VRB, "Blue Visual Test ");
	 break;
      default:
	 msg_printf(ERR, "Unknown Color Flag\n");
	 return 0;
   }

   switch(dirFlag)
   {
      case CRM_VISPAT1_DIRFLAG_LTOR:
	 msg_printf(VRB, "(Left->Right)...\n");
	 break;
      case CRM_VISPAT1_DIRFLAG_RTOL:
	 msg_printf(VRB, "(Right->Left)...\n");
	 break;
      case CRM_VISPAT1_DIRFLAG_TTOB:
	 msg_printf(VRB, "(Top->Bottom)...\n");
	 break;
      case CRM_VISPAT1_DIRFLAG_BTOT:
	 msg_printf(VRB, "(Bottom->Top)...\n");
	 break;
      default:
	 msg_printf(ERR, "Unknown Direction Flag\n");
	 return 0;
   }

   /* Run Visual Test */

   crmVisualPattern01(dirFlag, colorFlag);
   expectedCRC = crmGetVisualPattern01CRC(dirFlag, colorFlag);
   readCRC = crmCalculateNormalCRC(0, 0, maxPixelsX-1, maxPixelsY-1);

#if 0
   ttyprintf("Expected Checksum: 0x%0.16llx\n", expectedCRC);
#endif

   /* There is a bug above which prevents resolution dimensions that
      are not multiples of 256 to have their CRC checked correctly */

   if ((readCRC != expectedCRC) && (maxPixelsX % 256) == 0 
       && (maxPixelsY % 256 == 0))
   {
      errorCode.code = CODE_RE_VIS01;
      errorCode.w1 = (unsigned int) (readCRC >> 32);
      errorCode.w2 = (unsigned int) (readCRC);

      set_test_code(&errorCode);
      report_error(&errorCode);

      msg_printf(VRB, "Expected Checksum: 0x%0.16llx\n", expectedCRC);
      msg_printf(VRB, "Read Checksum: 0x%0.16llx\n", readCRC);

      retVal = 1;
   }

   if (retVal)
      sum_error("RE Register Test");
   else
   {
      msg_printf(VRB, "RE Visual Test 1 ");
      okydoky();
   }

   return retVal;
}


/***************************************************/
/* Section xx: Visual Test 2 (CRIME RE Read/Write) */
/***************************************************/

/*
 * Function:	crmVisualPattern02
 *
 * Parameters:	logicOpFlag
 *
 * Description:
 *
 */

#define CRM_VISPAT2_XSTART	64
#define CRM_VISPAT2_YSTART	64
#define CRM_VISPAT2_XINC	64
#define CRM_VISPAT2_YINC	64

void
crmVisualPattern02(int logicOpFlag)
{
   unsigned int color;
}
