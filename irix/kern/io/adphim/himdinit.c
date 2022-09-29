/* $Header: /proj/irix6.5.7m/isms/irix/kern/io/adphim/RCS/himdinit.c,v 1.3 1996/01/16 03:54:51 ack Exp $ */
/***************************************************************************
*                                                                          *
* Copyright 1993 Adaptec, Inc.,  All Rights Reserved.                      *
*                                                                          *
* This software contains the valuable trade secrets of Adaptec.  The       *
* software is protected under copyright laws as an unpublished work of     *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.  The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.                                                    *
*                                                                          *
***************************************************************************/

/***************************************************************************
*
*  Module Name:   HIMDINIT.C
*
*  Description:
*                 Codes specific to initialize HIM configured for driver
*                 are defined here. It should only be included if HIM is
*                 configured for driver and it can be thrown away after
*                 HIM get initialized.
*
*  Programmers:   Paul von Stamwitz
*                 Chuck Fannin
*                 Jeff Mendiola
*                 Harry Yang
*    
*  Notes:         NONE
*
*  Entry Point(s):
*     PH_CalcDataSize  - Returns size of HA data structure according to the
*                        number of SCBs (Driver only)
*  Revisions -
*
***************************************************************************/

#include "him_scb.h"
#include "him_equ.h"
#include "seq_off.h"
#include "sys/cmn_err.h"
#include "drvr_mac.h"


/*********************************************************************
*
*   Ph_GetDrvrConfig routine -
*
*   This routine initializes the members of the ha_Config and ha_struct
*   structures which are specific to driver only.
*
*  Return Value:  None
*                  
*  Parameters:    config_ptr
*
*  Activation:    PH_GetConfig
*                  
*  Remarks:                
*                  
*********************************************************************/

void Ph_GetDrvrConfig (cfp_struct *config_ptr)
{
   register AIC_7870 *base;
   UBYTE hcntrl_data;

   base = config_ptr->CFP_Base;
   config_ptr->CFP_DriverIdle = 1;
   config_ptr->CFP_BiosActive = 0;
   config_ptr->Cf_AccessMode = 2;
   Ph_GetOptimaConfig(config_ptr);

}

/*********************************************************************
*
*   Ph_InitDrvrHA routine -
*
*   This routine initializes the host adapter.
*
*  Return Value:  0x00      - Initialization successful
*                 <nonzero> - Initialization failed
*                  
*  Parameters:    config_ptr
*
*  Activation:    PH_InitHA
*                  
*  Remarks:                
*
*********************************************************************/

void Ph_InitDrvrHA (cfp_struct *config_ptr)
{
   register AIC_7870 *base;
   hsp_struct *ha_ptr;
   UBYTE cnt;
   UBYTE updateflag;

   base = config_ptr->CFP_Base;
   ha_ptr = config_ptr->CFP_HaDataPtr;

   ha_ptr->zero.dword = (DWORD) 0;        /* Initialize zero field   */

   /* set Initialize Entire HA data structure */
   ha_ptr->HaFlags |= HAFL_INITALL;

   PHM_SEMSET(ha_ptr,SEM_RELS);

   /* SAVE Entire 64 bytes of PCI SCRATCH RAM into Hsp_SaveScr      */
   /*  MEMORY Area -AND- Don't Update SCRATCH RAM from MEMORY       */
   updateflag = 0;
   SWAPCurrScratchRam(config_ptr, updateflag);

   /* Set STATE = SCRATCH RAM moved into MEMORY State. NO SWAP done.*/
   ha_ptr->Hsp_SaveState = SCR_PROTMODE;

   /* set HA data structure */
   Ph_SetHaData(config_ptr);

   /* Calculate device reset breakpoint */
   OUTBYTE(AIC7870[SEQCTL], PERRORDIS);
   OUTBYTE(AIC7870[SEQCTL], PERRORDIS + LOADRAM);
   OUTBYTE(AIC7870[SEQADDR0], START_LINK_CMD_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], 00);        /* Entry points always low page  */
   INBYTE(AIC7870[SEQRAM]);
   INBYTE(AIC7870[SEQRAM]);
   cnt  = (INBYTE(AIC7870[SEQRAM]) & 0xFF);
   cnt |= ((INBYTE(AIC7870[SEQRAM]) & 0x01) << 8);
   ha_ptr->sel_cmp_brkpt = cnt + 1;

   OUTBYTE(AIC7870[SEQCTL], PERRORDIS);

   Ph_OptimaLoadFuncPtrs(config_ptr);

}

/*********************************************************************
*
*   PH_CalcDataSize routine -
*
*   This routine will calculate the size of the HIM data structure
*   according to the number of SCBs
*
*  Return Value:  size of HIM data structure
*
*  Parameters:    mode: same as AccessMode
*                       1 - STANDARD mode
*                       2 - OPTIMA mode
*                       mode will be referenced only if both STANDARD and
*                       and OPTIMA mode are enabled at compile time.
*                 unsigned int number_scbs
*
*  Activation:    Driver, initialization
*
*  Remarks:
*
*********************************************************************/

UWORD PH_CalcDataSize (UBYTE mode,UWORD number_scbs)
{
      return(Ph_CalcOptimaSize(number_scbs));
}


