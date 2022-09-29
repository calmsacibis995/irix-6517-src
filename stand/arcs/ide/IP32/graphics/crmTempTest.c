/*
 * crmTempTest.c - Early version of IP32 graphics diagnostic testing
 *
 */

#include <libsc.h>
#include <sys/IP32.h>
#include <sys/sema.h>
#include <sys/sbd.h>
#include <sys/gfx.h>

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

extern code_t errorCode;

extern void crmSetTLB(char tlbFlag);
extern void crmInitGraphics(int x, int y, int depth, int refresh);
extern void crmDeinitGraphics(void);
extern int crmGBE_RegTest(void);
extern int crmRE_RegTest(void);
extern unsigned long long crmCalculateNormalCRC(void);
extern int isGraphicsInitialized(void);
extern int crmVisualTest01(int dirFlag, int colorFlag);


/************************************************************************/
/* Global Declarations							*/
/************************************************************************/



/************************************************************************/
/* Local Declarations							*/
/************************************************************************/


/************************************************************************/
/* Functions								*/
/************************************************************************/

/****************************/
/* Section xx: IDE Commands */
/****************************/

/*
 * Usage:	qndGfxTest
 *
 * Description:	
 *
 */

int
crmCmdQndGfxTest(int argc, char **argv)
{
   int j, k, readVal, errorFlag = 0;
   unsigned long long retVal;

   msg_printf(VRB, "Quick Graphics Test\n");

   run_cached(); 

   crmInitGraphics(1280, 1024, 32, 75);
   for (j = 0; j < (1 << 22); j++);

   errorFlag = crmGBE_RegTest();
   errorFlag |= crmRE_RegTest();

   errorFlag |= crmVisualTest01(CRM_VISPAT1_DIRFLAG_LTOR, CRM_VISPAT1_COLORFLAG_BW);
   errorFlag |= crmVisualTest01(CRM_VISPAT1_DIRFLAG_TTOB, CRM_VISPAT1_COLORFLAG_BW);
   errorFlag |= crmVisualTest01(CRM_VISPAT1_DIRFLAG_RTOL, CRM_VISPAT1_COLORFLAG_RED);
   errorFlag |= crmVisualTest01(CRM_VISPAT1_DIRFLAG_BTOT, CRM_VISPAT1_COLORFLAG_GREEN);
   errorFlag |= crmVisualTest01(CRM_VISPAT1_DIRFLAG_RTOL, CRM_VISPAT1_COLORFLAG_BLUE);

   crmDeinitGraphics();
   crmSetTLB(CRM_SETUP_TLB_A);

   msg_printf(VRB, "Quick Graphics Test done\n");
   return errorFlag;
}
