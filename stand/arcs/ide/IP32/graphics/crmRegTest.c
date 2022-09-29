/*
 * crmRegTest.c - Register testing functions
 *
 *    The functions in this file can be broken down into the following
 *    sections:
 *
 *       - IDE Commands
 *       - Unique Data Register Tests
 *	 - GBE Unique Address Tests
 *	 - CRIME RE Unique Address Tests
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
#include <stand_htport.h>
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

extern struct htp_state *htp;

extern void crmInitGraphics(int x, int y, int depth, int refresh);
extern void crmDeinitGraphics(void);
extern void crmSetTLB(char tlbFlag);
extern int isGraphicsInitialized(void);


/************************************************************************/
/* Global Declarations							*/
/************************************************************************/

code_t errorCode;


/************************************************************************/
/* Local Declarations							*/
/************************************************************************/

#define GBE_VT_MASK	0x00ffffff

int crmGBE_RegTest(void);
int crmRE_RegTest(void);

/************************************************************************/
/* Functions								*/
/************************************************************************/

/****************************/
/* Section xx: IDE Commands */
/****************************/

/*
 * Usage:	gfxRegTest
 *
 * Description:	
 *
 */

int
crmCmdGfxRegTest(int argc, char **argv)
{
   int i, errorFlag;

   msg_printf(VRB, "Graphics Register Test starting\n");
   crmInitGraphics(1280, 1024, 32, 75);
   for (i = 0; i < (1 << 22); i++);

   errorFlag = crmGBE_RegTest();
   errorFlag |= crmRE_RegTest();

   crmDeinitGraphics();
   crmSetTLB(CRM_SETUP_TLB_A);

   msg_printf(VRB, "Graphics Register Test done\n");
   return errorFlag;
}


/**********************************/
/* Section xx: GBE Register Tests */
/**********************************/

/*
 * Function:	crmGBE_UD_Test
 *
 * Parameters:	(None)
 *
 * Description:	This function performs a "Unique Data" test on the
 *		designated GBE register.  This consists of four steps:
 *		initialize to all 0, walking 1's, initialize to all 1's, 
 *		and walking 0's.  This makes isolation of problems easy.
 *
 */

static int
crmGBE_UD_Test(void)
{
   unsigned int testData, readVal;
   int i, retVal = 0;

   /* Set Error Code */

   errorCode.sw = SW_IDE;
   errorCode.test = TEST_GBEUD32;

   /* Test 1: 0's initialize test */

   testData = 0x00000000;

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetTable( crm_GBE_Base, crs_glyph, 0, testData);
   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeGetTable( crm_GBE_Base, crs_glyph, 0, readVal);

   if (readVal != testData)
   {					/* EC: IP32.xx.xx.xx.xx */
      errorCode.code = CODE_GBEUD32_INIT0;
      errorCode.w1 = readVal;
      errorCode.w2 = 0x00000000;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UD GBE Test: Zero Initialize failed\n");
      msg_printf(VRB, "Read: 0x%0.8x\n", readVal);

      retVal = 1;
   }

   /* Test 2: Walking 1's test */

   testData = 1;

   for (i = 0; i < 32; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, crs_glyph, 0, testData);
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeGetTable( crm_GBE_Base, crs_glyph, 0, readVal);

      if (readVal != testData)
      {					/* EC: IP32.xx.xx.xx.xx */
	 errorCode.code = CODE_GBEUD32_WALK1;
         errorCode.w1 = testData;
         errorCode.w2 = readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UD GBE Test: Walking 1's failed\n");
         msg_printf(VRB, "Expected: 0x%0.8x  Read: 0x%0.8x\n", testData, readVal);

         retVal = 1;
      }

      testData <<= 1;
   }

   /* Test 3: 1's initialize test */

   testData = (unsigned int) 0xffffffff;

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetTable( crm_GBE_Base, crs_glyph, 0, testData);
   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeGetTable( crm_GBE_Base, crs_glyph, 0, readVal);

   if (readVal != testData)
   {					/* EC: IP32.xx.xx.xx.xx */
      errorCode.code = CODE_GBEUD32_INIT1;
      errorCode.w1 = readVal;
      errorCode.w2 = 0x00000000;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UD GBE Test: One Initialize failed\n");
      msg_printf(VRB, "Read: 0x%0.8x\n", readVal);

      retVal = 1;
   }

   /* Test 4: Walking 0's test */

   testData = 0x80000000;

   for (i = 0; i < 32; i++)
   {
      testData = ~testData;
      testData &= 0xffffffff;
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, crs_glyph, 0, testData);
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeGetTable( crm_GBE_Base, crs_glyph, 0, readVal);

      if (readVal != testData)
      {					/* EC: IP32.xx.xx.xx.xx */
         errorCode.code = CODE_GBEUD32_WALK0;
         errorCode.w1 = testData;
         errorCode.w2 = readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UD GBE Test: Walking 0's failed\n");
         msg_printf(VRB, "Expected: 0x%0.8x  Read: 0x%0.8x\n", testData, readVal);

         retVal = 1;
      }

      testData = ~testData;
      testData &= 0xffffffff;
      testData >>= 1;
   }

   return retVal;
}


/*
 * Function:	crmGBE_UA_Cmap
 *
 * Parameters:	(None)
 *
 * Description:	UA stands for Unique Address test.
 *
 */

static int
crmGBE_UA_Cmap(int bank)
{
   int i, j, startIndex, stopIndex;
   unsigned int outputVal, retVal = 0;
   volatile unsigned int readVal;

   /* Determine Colormap Bank */

   switch(bank)
   {
      case 0:
	 startIndex = 0;
	 stopIndex = 4095;
	 break;
      case 1:
	 startIndex = 4096;
	 stopIndex = 4351;
	 break;
      default:
	 msg_printf(ERR, "Unknown bank: %d\n", bank);
	 return -1;
   }

   msg_printf(VRB, "Colormap Test, Bank %d...\n", bank);

   /* Set Error Code */

   errorCode.sw = SW_IDE;
   errorCode.func = FUNC_GBE;
   errorCode.test = TEST_CMAP_UA;

   /* Initialize WIDs							 */
   /*    If this is not done, then the colormap does not decode properly */

   for (i = 0; i < 32; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, mode_regs, i, 0x00000017);
   }

   /* Step 1: Clear the Registers */

   for (i = startIndex; i <= stopIndex; i++)
   {
      if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
         busy(1);
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, cmap, i, 0x00000000);
      gbeWaitCmapFifo( crm_GBE_Base, GBE_CMAP_WAIT_LEVEL);
   }
   gbeWaitCmapFifoEmpty( crm_GBE_Base);

   /* Step 2: Verify All Entries Cleared */

   for (i = startIndex; i <= stopIndex; i++)
   {
      if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
         busy(1);
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeGetTable( crm_GBE_Base, cmap, i, readVal);
      readVal &= 0xffffff00;

      if (readVal != 0x00000000)
      {						/* EC: IP32.xx.xx.xx.xx */
	 errorCode.code = CODE_GBE_UA_CMAP_VERIF0;
	 errorCode.w1 = readVal;
	 errorCode.w2 = i;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA CMAP Test: Zero Initialize failed\n");
         msg_printf(VRB, "Index: %d  Read: 0x%0.8x\n", i, readVal);

         retVal = 1;
      }
   }

   /* Step 3: Perform Unique Address Writes */

   readVal = 0;

   for (i = startIndex; i <= stopIndex; i++)
   {
      if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
         busy(1);
      outputVal = (i << 8);
      outputVal &= 0xffffff00;
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, cmap, i, outputVal);
      gbeWaitCmapFifo( crm_GBE_Base, GBE_CMAP_WAIT_LEVEL);
   }
   gbeWaitCmapFifoEmpty( crm_GBE_Base);

   /* Step 4: Perform Unique Address Verify */

   for (i = startIndex; i <= stopIndex; i++)
   {
      if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
         busy(1);
      outputVal = (i << 8);
      outputVal &= 0xffffff00;

      for (j = 0; j < 20; j++)
      {
         gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeGetTable( crm_GBE_Base, cmap, i, readVal);
         readVal &= 0xffffff00;

	 if (readVal == outputVal)
	    break;
      }

      if (readVal != outputVal)
      {						/* EC: IP32.xx.xx.xx.xx */
	 errorCode.code = CODE_GBE_UA_CMAP_VERIFUA;
	 errorCode.w1 = readVal;
	 errorCode.w2 = i;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA CMAP Test: Unique Address Verify failed\n");
         msg_printf(VRB, "Index: %d  Read: 0x%0.8x\n", i, readVal);

         retVal = 1;
      }
   }

   busy(0);
   msg_printf(VRB, "Done\n");
   return retVal;
}


/*
 * Function:	crmGBE_UA_Gmap
 *
 * Parameters:	(None)
 *
 * Description:	UA stands for Unique Address test.  Specifically, this
 *		tests each color component separately.
 *
 */

static int
crmGBE_UA_Gmap(void)
{
   int i, j, outputVal, readVal, retVal = 0;

   /* Set Error Code */

   errorCode.sw = SW_IDE;
   errorCode.func = FUNC_GBE;
   errorCode.test = TEST_GMAP_UA;

   /* Begin Testing */

   for (j = 1; j <= 3; j++)
   {
      /* Step 1: Clear the Registers */

      for (i = 0; i < GBE_NUM_GMAP_ENTRY; i++)
      {
	 gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeSetTable( crm_GBE_Base, gmap, i, 0x00000000);
      }

      /* Step 2: Verify All Entries Cleared */

      for (i = 0; i < GBE_NUM_GMAP_ENTRY; i++)
      {
	 if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
	    busy(1);
         gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeGetTable( crm_GBE_Base, gmap, i, readVal);

	 if (readVal != 0x00000000)
	 {					/* EC: IP32.xx.xx.xx.xx */
	    errorCode.code = CODE_GBE_UA_GMAP_VERIF0;
	    errorCode.w1 = (j << 16) | i;
	    errorCode.w2 = readVal;

            set_test_code(&errorCode);
	    report_error(&errorCode);
            msg_printf(VRB, "UA GMAP Test: Zero Initialize failed\n");
            msg_printf(VRB, "Index: %d  Read: 0x%0.8x\n", i, readVal);

            retVal = 1;
	 }
      }

      /* Step 3: Perform Unique Address Writes */

      for (i = 0; i < GBE_NUM_GMAP_ENTRY; i++)
      {
	 if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
	    busy(1);
	 outputVal = (i << (j * 8)) & ((0x000000ff) << (j * 8));
	 gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeSetTable( crm_GBE_Base, gmap, i, outputVal);
      }

      /* Step 4: Perform Unique Address Verify */

      for (i = 0; i < GBE_NUM_GMAP_ENTRY; i++)
      {
	 if ((i % CRM_BUSY_WAIT_PERIOD) == 0)
	    busy(1);
	 outputVal = (i << (j * 8)) & ((0x000000ff) << (j * 8));
         gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
         gbeGetTable( crm_GBE_Base, gmap, i, readVal);

	 if (readVal != outputVal)
	 {					/* EC: IP32.xx.xx.xx.xx */
	    errorCode.code = CODE_GBE_UA_GMAP_VERIFUA;
	    errorCode.w1 = (j << 16) | i;
	    errorCode.w2 = readVal;

            set_test_code(&errorCode);
	    report_error(&errorCode);
            msg_printf(VRB, "UA GMAP Test: Unique Address Verify failed\n");
            msg_printf(VRB, "Index: %d  Expected: 0x%0.8x  Read: 0x%0.8x\n", i, outputVal, readVal);

            retVal = 1;
	 }
      }
   }

   busy(0);
   return retVal;
}


/*
 * Function:	crmGBE_UA_MiscRegs
 *
 * Parameters:	(None)
 *
 * Description:	UA stands for Unique Address test.
 *
 */

static int
crmGBE_UA_MiscRegs(void)
{
   int i, outputVal, readVal, retVal = 0;

   /* Set Error Code */

   errorCode.sw = SW_IDE;
   errorCode.func = FUNC_GBE;
   errorCode.test = TEST_MISC_UA;

   /* Step 1: Clear the Registers */

   for (i = 0; i < 32; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, mode_regs, i, 0x00000000);
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetReg( crm_GBE_Base, crs_pos, 0x00000000);

   for (i = 0; i < 3; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, crs_cmap, i, 0x00000000);
   }

   for (i = 0; i < 64; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, crs_glyph, i, 0x00000000);
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetReg( crm_GBE_Base, vc_lr, 0x00000000);
   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetReg( crm_GBE_Base, vc_tb, 0x00000000);

   /* Step 2: Verify All Entries Cleared */

   for (i = 0; i < 32; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeGetTable( crm_GBE_Base, mode_regs, i, readVal);
      readVal &= 0x00001fff;
      if (readVal != 0x00000000)
      {
         errorCode.code = CODE_GBE_UA_MISC_VERIF0;
         errorCode.w1 = W1_GBE_MODE_REGS | i;
         errorCode.w2 = readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA Misc Test: Zero Initialize failed\n");
         msg_printf(VRB, "Index: %d  Read: 0x%0.8x\n", i, readVal);

         retVal = 1;
      }
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeGetReg( crm_GBE_Base, crs_pos, readVal);
   if (readVal != 0x00000000)
   {
      errorCode.code = CODE_GBE_UA_MISC_VERIF0;
      errorCode.w1 = W1_GBE_CRS_POS;
      errorCode.w2 = readVal;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UA Misc Test: Zero Initialize failed\n");
      msg_printf(VRB, "Read: 0x%0.8x\n", readVal);

      retVal = 1;
   }


   for (i = 0; i < 3; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeGetTable( crm_GBE_Base, crs_cmap, i, readVal);
      if (readVal != 0x00000000)
      {
         errorCode.code = CODE_GBE_UA_MISC_VERIF0;
         errorCode.w1 = W1_GBE_CRS_CMAP | i;
         errorCode.w2 = readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA Misc Test: Zero Initialize failed\n");
         msg_printf(VRB, "Index: %d  Read: 0x%0.8x\n", i, readVal);

         retVal = 1;
      }
   }

   for (i = 0; i < 64; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeGetTable( crm_GBE_Base, crs_glyph, i, readVal);
      if (readVal != 0x00000000)
      {
         errorCode.code = CODE_GBE_UA_MISC_VERIF0;
         errorCode.w1 = W1_GBE_CRS_GLYPH | i;
         errorCode.w2 = readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA Misc Test: Zero Initialize failed\n");
         msg_printf(VRB, "Index: %d  Read: 0x%0.8x\n", i, readVal);

         retVal = 1;
      }
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeGetReg( crm_GBE_Base, vc_lr, readVal);
   readVal &= 0x00ffffff;
   if (readVal != 0x00000000)
   {
      errorCode.code = CODE_GBE_UA_MISC_VERIF0;
      errorCode.w1 = W1_GBE_VC_LR;
      errorCode.w2 = readVal;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UA Misc Test: Zero Initialize failed\n");
      msg_printf(VRB, "Read: 0x%0.8x\n", readVal);

      retVal = 1;
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeGetReg( crm_GBE_Base, vc_tb, readVal);
   readVal &= 0x00ffffff;
   if (readVal != 0x00000000)
   {
      errorCode.code = CODE_GBE_UA_MISC_VERIF0;
      errorCode.w1 = W1_GBE_VC_TB;
      errorCode.w2 = readVal;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UA Misc Test: Zero Initialize failed\n");
      msg_printf(VRB, "Read: 0x%0.8x\n", readVal);

      retVal = 1;
   }


   /* Step 3: Perform Unique Address Writes */

   outputVal = 0;

   for (i = 0; i < 32; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, mode_regs, i, outputVal);
      outputVal++;
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetReg( crm_GBE_Base, crs_pos, outputVal++);

   for (i = 0; i < 3; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, crs_cmap, i, (outputVal++) << 8);
   }

   for (i = 0; i < 64; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeSetTable( crm_GBE_Base, crs_glyph, i, outputVal++);
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetReg( crm_GBE_Base, vc_lr, outputVal++);
   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeSetReg( crm_GBE_Base, vc_tb, outputVal++);

   /* Step 4: Perform Unique Address Verify */

   outputVal = 0;

   for (i = 0; i < 32; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeGetTable( crm_GBE_Base, mode_regs, i, readVal);
      readVal &= 0x00001fff;
      if (readVal != outputVal++)
      {
         errorCode.code = CODE_GBE_UA_MISC_VERIFUA;
         errorCode.w1 = W1_GBE_MODE_REGS | i;
         errorCode.w2 = readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA Misc Test: Unique Address Verify failed\n");
         msg_printf(VRB, "Index: %d  Read: 0x%0.8x\n", i, readVal);

         retVal = 1;
      }
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeGetReg( crm_GBE_Base, crs_pos, readVal);
   if (readVal != outputVal++)
   {
      errorCode.code = CODE_GBE_UA_MISC_VERIFUA;
      errorCode.w1 = W1_GBE_CRS_POS;
      errorCode.w2 = readVal;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UA Misc Test: Unique Address Verify failed\n");
      msg_printf(VRB, "Read: 0x%0.8x\n", readVal);

      retVal = 1;
   }


   for (i = 0; i < 3; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeGetTable( crm_GBE_Base, crs_cmap, i, readVal);
      if (readVal != ((outputVal++) << 8))
      {
         errorCode.code = CODE_GBE_UA_MISC_VERIFUA;
         errorCode.w1 = W1_GBE_CRS_CMAP | i;
         errorCode.w2 = readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA Misc Test: Unique Address Verify failed\n");
         msg_printf(VRB, "Index: %d  Read: 0x%0.8x\n", i, readVal);

         retVal = 1;
      }
   }

   for (i = 0; i < 64; i++)
   {
      gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
      gbeGetTable( crm_GBE_Base, crs_glyph, i, readVal);
      if (readVal != outputVal++)
      {
         errorCode.code = CODE_GBE_UA_MISC_VERIFUA;
         errorCode.w1 = W1_GBE_CRS_GLYPH | i;
         errorCode.w2 = readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA Misc Test: Unique Address Verify failed\n");
         msg_printf(VRB, "Index: %d  Read: 0x%0.8x\n", i, readVal);

         retVal = 1;
      }
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeGetReg( crm_GBE_Base, vc_lr, readVal);
   readVal &= 0x00ffffff;
   if (readVal != outputVal++)
   {
      errorCode.code = CODE_GBE_UA_MISC_VERIFUA;
      errorCode.w1 = W1_GBE_VC_LR;
      errorCode.w2 = readVal;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UA Misc Test: Unique Address Verify failed\n");
      msg_printf(VRB, "Read: 0x%0.8x\n", readVal);

      retVal = 1;
   }

   gbeWaitForBlanking( crm_GBE_Base, currentTimingPtr);
   gbeGetReg( crm_GBE_Base, vc_tb, readVal);
   readVal &= 0x00ffffff;
   if (readVal != outputVal++)
   {
      errorCode.code = CODE_GBE_UA_MISC_VERIFUA;
      errorCode.w1 = W1_GBE_VC_TB;
      errorCode.w2 = readVal;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UA Misc Test: Unique Address Verify failed\n");
      msg_printf(VRB, "Read: 0x%0.8x\n", readVal);

      retVal = 1;
   }

   return retVal;
}

/*
 * Function:	crmGBE_RegTest
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */

int
crmGBE_RegTest(void)
{
   int errorFlag = 0;

   msg_printf(VRB, "GBE Register Test started\n");

   errorCode.func = FUNC_GBE;
   errorFlag = crmGBE_UD_Test();
   errorFlag |= crmGBE_UA_MiscRegs();
   errorFlag |= crmGBE_UA_Gmap();
   errorFlag |= crmGBE_UA_Cmap(0);
   errorFlag |= crmGBE_UA_Cmap(1);

   if (errorFlag)
      sum_error("GBE Register Test");
   else
   {
      msg_printf(VRB, "GBE Register ");
      okydoky();
   }

   return errorFlag;
}


/********************************/
/* Section xx: RE Register Test */
/********************************/

/*
 * Function:	crmRE_UD_TLBTest
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */

int
crmRE_UD_TLBTest(void)
{
   int i, retVal = 0;
   unsigned long long testData, readVal;

   /* Set Error Code */

   errorCode.sw = SW_IDE;
   errorCode.test = TEST_REUD64;

   /* Test 1: 0's initialize test */

   testData = 0x0000000000000000LL;

   crmSet64( crm_CRIME_Base, Tlb.fbA[0].dw, testData);
   crmGet64( crm_CRIME_Base, Tlb.fbA[0].dw, readVal);

   if (readVal != testData)
   {					/* EC: IP32.xx.xx.xx.xx */
      errorCode.code = CODE_REUD64_INIT0;
      errorCode.w1 = (unsigned int) (readVal >> 32);
      errorCode.w2 = (unsigned int) readVal;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UD RE TLB Test: Zero Initialize failed\n");
      msg_printf(VRB, "Expected: 0x%0.16llx  Read: 0x%0.16llx\n", testData, readVal);

      retVal = 1;
   }

   /* Test 2: Walking 1's test */

   testData = 1;

   for (i = 0; i < 64; i++)
   {
      crmSet64( crm_CRIME_Base, Tlb.fbA[0].dw, testData);
      crmGet64( crm_CRIME_Base, Tlb.fbA[0].dw, readVal);

      if (readVal != testData)
      {					/* EC: IP32.xx.xx.xx.xx */
	 errorCode.code = CODE_REUD64_WALK1;
         errorCode.w1 = (unsigned int) (readVal >> 32);
         errorCode.w2 = (unsigned int) readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UD RE TLB Test: Walking 1's failed\n");
	 msg_printf(VRB, "Expected: 0x%0.16llx  Read: 0x%0.16llx\n", testData, readVal);

         retVal = 1;
      }

      testData <<= 1;
   }

   /* Test 3: 1's initialize test */

   testData = (unsigned int) 0xffffffffffffffffLL;

   crmSet64( crm_CRIME_Base, Tlb.fbA[0].dw, testData);
   crmGet64( crm_CRIME_Base, Tlb.fbA[0].dw, readVal);

   if (readVal != testData)
   {					/* EC: IP32.xx.xx.xx.xx */
      errorCode.code = CODE_REUD64_INIT1;
      errorCode.w1 = (unsigned int) (readVal >> 32);
      errorCode.w2 = (unsigned int) readVal;

      set_test_code(&errorCode);
      report_error(&errorCode);
      msg_printf(VRB, "UD RE TLB Test: Ones Initialize failed\n");
      msg_printf(VRB, "Expected: 0x%0.16llx  Read: 0x%0.16llx\n", testData, readVal);

      retVal = 1;
   }

   /* Test 4: Walking 0's test */

   testData = 0x8000000000000000LL;

   for (i = 0; i < 64; i++)
   {
      testData = ~testData;
      crmSet64( crm_CRIME_Base, Tlb.fbA[0].dw, testData);
      crmGet64( crm_CRIME_Base, Tlb.fbA[0].dw, readVal);

      if (readVal != testData)
      {					/* EC: IP32.xx.xx.xx.xx */
         errorCode.code = CODE_REUD64_WALK0;
         errorCode.w1 = (unsigned int) (readVal >> 32);
         errorCode.w2 = (unsigned int) readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UD RE TLB Test: Walking 0's failed\n");
         msg_printf(VRB, "Expected: 0x%0.16llx  Read: 0x%0.16llx\n", testData, readVal);

         retVal = 1;
      }

      testData = ~testData;
      testData >>= 1;
   }

   return retVal;
}


/*
 * Function:	crmRE_UA_TLBTest
 *
 * Parameters:	(None)
 *
 * Description:	This performs Unique Address tests on the RE TLB's.
 *
 */

int
crmRE_UA_TLBTest(unsigned long long *tempPtr, int maxIndex)
{
   int i, j, retVal = 0;
   unsigned long long outputVal, readVal;
   volatile unsigned long long *reg64Ptr = tempPtr;

   outputVal = 0;

   /* Set Error Code */

   errorCode.sw = SW_IDE;
   errorCode.func = FUNC_CRIME;
   errorCode.test = TEST_RE_UA_TLB;

   /* Step 1: Clear the Registers */

   for (i = 0; i < maxIndex; i++)
   {
      reg64Ptr[i] = 0x0000000000000000LL;
   }

   /* Step 2: Verify All Entries Cleared */

   for (i = 0; i < maxIndex; i++)
   {
      readVal = reg64Ptr[i];

      if (readVal != 0x0000000000000000LL)
      {						/* EC: IP32.xx.xx.xx.xx */
         errorCode.code = ((unsigned int) reg64Ptr << 16) | CODE_RE_UA_TLB_VERIF0;
         errorCode.w1 = (unsigned int) (readVal >> 32);
         errorCode.w2 = (unsigned int) readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA RE TLB Test: Zero Initialize failed\n");
         msg_printf(VRB, "Read: 0x%0.16llx\n", readVal);

         retVal = 1;
      }
   }

   /* Step 3: Perform Unique Address Writes */

   for (i = 0; i < maxIndex; i++)
   {
      reg64Ptr[i] = outputVal;
      outputVal++;
   }

   /* Step 4: Perform Unique Address Verify */

   outputVal = 0;

   for (i = 0; i < maxIndex; i++)
   {
      readVal = reg64Ptr[i];

      if (readVal != outputVal)
      {
         errorCode.code = ((unsigned int) reg64Ptr << 16) | CODE_RE_UA_TLB_VERIFUA;
         errorCode.w1 = (unsigned int) (readVal >> 32);
         errorCode.w2 = (unsigned int) readVal;

         set_test_code(&errorCode);
         report_error(&errorCode);
         msg_printf(VRB, "UA RE TLB Test: Unique Address Verify failed\n");
         msg_printf(VRB, "Expected: 0x%0.16llx  Read: 0x%0.16llx\n", outputVal, readVal);

	 retVal = 1;
      }

      outputVal++;
   }

   return retVal;
}


/*
 * Function:	crmRE_RegTest
 *
 * Parameters:	(None)
 *
 * Description:	
 *
 */


int
crmRE_RegTest(void)
{
   int errorFlag = 0;
   unsigned long long retVal;

   msg_printf(VRB, "RE Register Test started\n");
   errorCode.func = FUNC_CRIME;

   crmSaveRETLB();

   errorFlag = crmRE_UD_TLBTest();

   msg_printf(VRB, "RE TLB Test...");
   errorFlag |= crmRE_UA_TLBTest(&(crm_CRIME_Base->Tlb.fbA[0].dw), 64);
   errorFlag |= crmRE_UA_TLBTest(&(crm_CRIME_Base->Tlb.fbB[0].dw), 64);
   errorFlag |= crmRE_UA_TLBTest(&(crm_CRIME_Base->Tlb.fbC[0].dw), 64);
   errorFlag |= crmRE_UA_TLBTest(&(crm_CRIME_Base->Tlb.tex[0].dw), 28);
   errorFlag |= crmRE_UA_TLBTest(&(crm_CRIME_Base->Tlb.cid[0].dw), 4);
   errorFlag |= crmRE_UA_TLBTest(&(crm_CRIME_Base->Tlb.linearA[0].dw), 16);
   errorFlag |= crmRE_UA_TLBTest(&(crm_CRIME_Base->Tlb.linearB[0].dw), 16);
   msg_printf(VRB, "Done\n");
   
   if (errorFlag)
      sum_error("RE Register Test");
   else
   {
      msg_printf(VRB, "RE Register ");
      okydoky();
   }

   crmRestoreRETLB();
   return errorFlag;
}
