/*
 * crm_config.c - configuration graphics routines for Moosehead
 *
 *    This file contains the routines supporting hardware configuration
 *    (generally set up from init_consenv()).  
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

#ident "$Revision: 1.5 $"

#include "arcs/hinv.h"
#include "arcs/folder.h"
#include "sys/IP32.h"
#include "sys/sbd.h"
#include "sys/sema.h"			/* Needed for sys/gfx.h */
#include "cursor.h"
#include "libsc.h"
#include "libsk.h"
#include "stand_htport.h"

#include "sys/gfx.h"			/* Needed for crime_gfx.h */
#include "sys/crime_gfx.h"		/* Needed before crime_gbe.h */
#include "sys/crime_gbe.h"
#include "sys/crimechip.h"
#include "crmDefs.h"
#include "crm_stand.h"

/************************************************************************/
/* External Declarations						*/
/************************************************************************/

extern Crimechip *crm_CRIME_Base;
extern CrimeCPUInterface *crm_CPU_Base;
extern struct gbechip *crm_GBE_Base;
extern struct crime_info crm_ginfo;
extern struct tp_fncs crm_tp_fncs;		/* Textport routines	*/
extern COMPONENT montmpl;			/* Monitor COMPONENT	*/
extern int gfxboard;

extern void cpu_acpanic(char * str);
extern void _errputs(char*);
extern int _gfx_strat(COMPONENT *self, IOBLOCK *iob);
extern void initGraphics(void);
extern void initCRIMEGraphicsBase(void);

extern int _prom;


/************************************************************************/
/* Global Declarations							*/
/************************************************************************/

static COMPONENT crmtmpl =	/* CRM COMPONENT		*/
{
   ControllerClass,		/* Class			*/
   DisplayController,		/* Type				*/
   ConsoleOut|Output,		/* Flags			*/
   SGI_ARCS_VERS,		/* Version			*/
   SGI_ARCS_REV,		/* Revision			*/
   0,				/* Key				*/
   0x01,			/* Affinity			*/
   0,				/* ConfigurationDataSize	*/
   16,				/* IdentifierLength		*/
   ""				/* Identifier			*/
};


/************************************************************************/
/* Local Declarations							*/
/************************************************************************/

static void initGBE(void);
int crmTestCRIMEGraphicsInitialized(void);
int crmTestGraphics(void);

/************************************************************************/
/* Functions								*/
/************************************************************************/

/*
 * Function:	crmConfigInstall
 *
 * Parameters:	root		The root of the hardware inventory tree
 *
 * Description:	This function should perform the same function as
 *		ng1_install for Newport.
 *
 *		For now, we assume that Moosehead graphics are available
 *		and we simply need to identify the components on the
 *		graphics board.
 *
 */

void
crmConfigInstall(COMPONENT *root)
{
   COMPONENT *tmpComponent;
   unsigned int retVal, retVal2;

   crmGetRev( retVal);

   if (retVal == 0x000000a0)
   {
      crm_ginfo.crimerev = 0;
      return;
   }
   else if (retVal == 0x000000a1)
   {
      tmpComponent = AddChild(root, &crmtmpl, (void *) NULL);

      if (tmpComponent == (COMPONENT *) NULL)
         cpu_acpanic("CRM");

      tmpComponent->Key = gfxboard++;

      crmGetRERev( retVal2);

      if (retVal2 == 0)
      {
         strcpy(tmpComponent->Identifier, "SGI-CRM, Rev B");
         tmpComponent->IdentifierLength = 14;
	 crm_ginfo.crimerev = 1;
      }
      else if (retVal2 == 1)
      {
         strcpy(tmpComponent->Identifier, "SGI-CRM, Rev C");
         tmpComponent->IdentifierLength = 14;
	 crm_ginfo.crimerev = 2;
      }
      else
      {
         strcpy(tmpComponent->Identifier, "SGI-CRM, Rev ?");
         tmpComponent->IdentifierLength = 14;
	 crm_ginfo.crimerev = 0;
      }

      RegisterDriverStrategy(tmpComponent, _gfx_strat);

      tmpComponent = AddChild(tmpComponent, &montmpl, (void *) NULL);

      if (tmpComponent == (COMPONENT *) NULL)
         cpu_acpanic("Monitor");
   }
}


/*
 * Function:	crmConfigProbe
 *
 * Parameters:	fncs		Pointer to graphics functions
 *
 * Description:	This function has a "list" of the available graphics
 *		subsystems (e.g. CRIME/GBE, PCI Cards, etc.).  If the
 *		parameter fncs is 0, then we return the address of the
 *		Moosehead board.  On subsequence calls we return 0.
 *
 *		We currently do the following:
 *		1) Return the address of CRIME.
 *
 *		This function should perform the same function as
 *		ng1_probe for Newport.
 *
 */

int
crmConfigProbe(struct tp_fncs **fncs)
{
   unsigned int retVal;
   static int currentIndex;

   if (fncs)
   {
      *fncs = &crm_tp_fncs;
      currentIndex = 0;
      return 0;
   }

   if (currentIndex == 0)
   {
      currentIndex++;

      crmGetType( retVal);
      if ((retVal == 0x000000a1) && (_prom == 0))
         initGraphics();

      return PHYS_TO_K1(CRM_RE_BASE_ADDRESS);
   }
   else
      return 0;
}
