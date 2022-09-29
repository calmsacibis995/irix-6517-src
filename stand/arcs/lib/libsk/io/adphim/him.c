/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/io/adphim/RCS/him.c,v 1.3 1997/12/20 02:30:36 philw Exp $ */
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

#ifndef SIMHIM

#include "him_scb.h"
#include "him_equ.h"
#include "seq_off.h"
#include "sys/cmn_err.h"
#include "seq_01.h"
#include "drvr_mac.h"
#include "libsk.h"
#include "libsc.h"

int verbose_nego=0;
extern int adp_verbose;
extern UBYTE adp_curr_scb_num;

#ifdef LE_DISK
extern int soft_swizzle;
#endif

void Ph_EnableNextScbArray(sp_struct *);


/*
 ***************************************************************************
 *
 * Various initialization.
 * Returns 0 if OK, -1 if error during initialization.
 *
 ***************************************************************************
 */
UWORD
PH_init_him (cfp_struct *config_ptr)
{
   AIC_7870 *base;
   int adp_fifo_threshold = 0x2;
   UBYTE sblkctl, sstat1, intstat;
   UBYTE le_index, conv_index;

   base = config_ptr->CFP_Base;

   Ph_init_hsp_struct(config_ptr);

   if (Ph_LoadSequencer(config_ptr))
	   return(-1);

   OUTBYTE(AIC7870[SCSIID], config_ptr->Cf_ScsiId);

   /*
    * high byte termination disable/enable
    * enable/disable high byte termination only if its an optional
    * PCI card.
    */
   if (config_ptr->Cf_AdapterId == 0x8178) {
      OUTBYTE(AIC7870[SEEPROM],SEEMS);                /* process high byte */
      while(INBYTE(AIC7870[SEEPROM]) & EXTARBACK);          /* termination */
      OUTBYTE(AIC7870[SEEPROM],SEEMS|SEECS);
   
      if (config_ptr->CFP_TerminationHigh) {
         OUTBYTE(AIC7870[BRDCTL],BRDDAT6|BRDSTB|BRDCS); 
         OUTBYTE(AIC7870[BRDCTL],BRDDAT6|BRDCS);        
      } else {
         OUTBYTE(AIC7870[BRDCTL],BRDSTB|BRDCS);
         OUTBYTE(AIC7870[BRDCTL],BRDCS);               
      }

      OUTBYTE(AIC7870[BRDCTL],0);
      OUTBYTE(AIC7870[SEEPROM],0);

      /*
       * Turn off LED
       */
      sblkctl = INBYTE(AIC7870[SBLKCTL]);
      sblkctl &= ~(DIAGLEDEN | DIAGLEDON);
      OUTBYTE(AIC7870[SBLKCTL], sblkctl);
   }

   Ph_ResetSCSI(base);
   if (adp_verbose)
	   printf("reset SCSI bus completed\n");

   Ph_ResetChannel(config_ptr);
   if (adp_verbose)
	   printf("reset channel completed\n");

   OUTBYTE(AIC7870[SEQCTL], FAILDIS + FASTMODE + SEQRESET);
   OUTBYTE(AIC7870[BRKADDR1], BRKDIS);
   OUTBYTE(AIC7870[PCISTATUS], adp_fifo_threshold << 6);
   OUTBYTE(AIC7870[SXFRCTL0], DFON);   /* Turn on digital filter 1/7/94 */

   /*
    * Set the allow disconnect maps.
    */
   OUTBYTE(AIC7870[DISCON_OPTION], ~config_ptr->CFP_AllowDscntL);
   le_index = OSM_CONV_BYTEPTR(DISCON_OPTION) + 1;
   conv_index = OSM_CONV_BYTEPTR(le_index);
   OUTBYTE(AIC7870[conv_index], ~config_ptr->CFP_AllowDscntH);

   /*
    * One more check for external reset
    */
   intstat = Ph_ReadIntstat(base);
   if (intstat & ANYPAUSE) {
	   sstat1 = INBYTE(AIC7870[SSTAT1]);
	   if (sstat1 & SCSIRSTI)
		   Ph_ext_scsi_bus_reset(config_ptr);
	   OUTBYTE(AIC7870[CLRINT], CLRSCSINT);
   }

   Ph_UnPause(base);

   return(0);
}


/*********************************************************************
*
* down load sequencer code 
*
*********************************************************************/
int
Ph_LoadSequencer (cfp_struct *config_ptr)
{
   AIC_7870 *base;
   UBYTE *seq_code;
   int seq_size, cnt;
   UBYTE readback;

   base = config_ptr->CFP_Base;
   seq_size = sizeof(Seq_01);
   seq_code = Seq_01;

   OUTBYTE(AIC7870[SEQCTL], PERRORDIS + LOADRAM);
   OUTBYTE(AIC7870[SEQADDR0], 00);
   OUTBYTE(AIC7870[SEQADDR1], 00);
   for (cnt = 0; cnt < seq_size; cnt++)
      OUTBYTE(AIC7870[SEQRAM], seq_code[cnt]);


   OUTBYTE(AIC7870[SEQCTL], PERRORDIS);	   /* clear */
   OUTBYTE(AIC7870[SEQCTL], PERRORDIS + LOADRAM);  /* and set */
   OUTBYTE(AIC7870[SEQADDR0], 00);
   OUTBYTE(AIC7870[SEQADDR1], 00);
   for (cnt = 0; cnt < seq_size; cnt++) {
      if ((readback = INBYTE(AIC7870[SEQRAM])) != seq_code[cnt]) {
        return(-1);
      }
   }

   OUTBYTE(AIC7870[SEQCTL], PERRORDIS);

   return(0);
}


/*********************************************************************
*
*  PH_EnableInt
*
*  Enable AIC-7870 interrupt
*
*  Return Value:  None
*                  
*  Parameters:    config_ptr
*
*  Activation:    PH_IntHandler
*                  
*  Remarks:
*                  
*********************************************************************/

void PH_EnableInt (cfp_struct *config_ptr)
{
   AIC_7870 *base = config_ptr->CFP_Base;

   Ph_WriteHcntrl(base, (UBYTE) (INBYTE(AIC7870[HCNTRL]) | INTEN));
}

/*********************************************************************
*
*  PH_DisableInt
*
*  Disable AIC-7870 interrupt
*
*  Return Value:  None
*                  
*  Parameters:    config_ptr
*
*  Activation:    PH_IntHandler
*                  
*  Remarks:
*                  
*********************************************************************/

void PH_DisableInt (cfp_struct *config_ptr)
{
   AIC_7870 *base = config_ptr->CFP_Base;

   Ph_WriteHcntrl(base, (UBYTE) (INBYTE(AIC7870[HCNTRL]) & ~INTEN));
}

/*********************************************************************
*  Ph_NonInit routine -
*
*  Parse non-initiator command request, activate abort, device reset
*  or read sense routines.
*
*  Return Value:  
*                  
*  Parameters:    
*
*  Activation: scb_send
*
*  Remarks:       
*                 
*********************************************************************/
#ifdef   _FAULT_TOLERANCE

UBYTE Ph_NonInit (sp_struct *scb_pointer)
{
   cfp_struct *config_ptr=scb_pointer->SP_ConfigPtr;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   UBYTE retval = 0;

   switch (scb_pointer->SP_Cmd)
   {

      default:
	 printf("unknown non-initiator mode command\n");

   }
   return (retval);
}
#endif   /* _FAULT_TOLERANCE */

/*********************************************************************
*
*  PH_Special routine -
*
*  Perform command not requiring an SCB: Abort,
*                                        Soft Reset,
*                                        Hard Reset
*
*  Return Value:  0 = Reset completed. (Also returned for abort opcode)
*                 1 = Soft reset failed, Hard reset performed. 
*                 2 = Reset failed, hardware error.
*              0xFF = Busy could not swap sequencers  MDL 
*                  
*  Parameters: UBYTE Opcode: 00 = Abort SCB
*                            01 = Soft Reset Host Adapter
*                            02 = Hard Reset Host Adapter
*
*              sp_struct *: ptr to SCB for Abort, NOT_DEFINED otherwise.
*
*  Activation:    
*              Ph_ScbSend
*  Remarks:       
*                 
*********************************************************************/
#ifdef   _FAULT_TOLERANCE
int PH_Special ( UBYTE spec_opcode,
                   cfp_struct *config_ptr,
                   sp_struct *scb_ptr)
{
   return (0);
}
#endif   /* _FAULT_TOLERANCE */

/*********************************************************************
*  Ph_Abort -
*
*  Determine state of SCB to be aborted and abort if not ACTIVE.
*
*  Return Value:  
*                  
*  Parameters:    
*
*  Activation: Ph_Special
*
*  Remarks:       
*                 
*********************************************************************/
#ifdef   _FAULT_TOLERANCE
void Ph_Abort (sp_struct *scb_ptr)
{
}
#endif   /* _FAULT_TOLERANCE */

/*********************************************************************
*
* Process case where other device resets scsi bus
* In the unix kernel version, we need to clear all of our internal
* target busy maps and cmds, but in the prom, where we only have
* 1 outstanding cmd at a time, it can be done in Ph_Intfree
*                  
*********************************************************************/
void
Ph_ext_scsi_bus_reset(cfp_struct *config_ptr)
{
   AIC_7870 *base;
   UBYTE HaStatus;

   base = config_ptr->CFP_Base;

   while (INBYTE(AIC7870[SSTAT1]) & SCSIRSTI)
      OUTBYTE(AIC7870[CLRSINT1], CLRSCSIRSTI);

   OUTBYTE(AIC7870[SCSISEQ], 0);        /* Disarm any outstanding selections */
   if (INBYTE(AIC7870[SCSISIG]))       /* If bus still not free...          */
      Ph_ResetSCSI(base);              /* Reset it again!                   */
   Ph_ResetChannel(config_ptr);

   /* Now, Restart Sequencer */
   OUTBYTE(AIC7870[SEQADDR0], (UBYTE) IDLE_LOOP_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], (UBYTE) IDLE_LOOP_ENTRY >> 10);
}


/*********************************************************************
*
*  Reset SCSI bus and clear all associated interrupts.
*  Also clear synchronous / wide mode.
*
*********************************************************************/
void
Ph_ResetChannel (cfp_struct *config_ptr)
{
   AIC_7870 *base;
   UBYTE i, j, xfer_option;

   base = config_ptr->CFP_Base;

   OUTBYTE(AIC7870[SCSISEQ], 00);
   OUTBYTE(AIC7870[CLRSINT0], 0xff);
   OUTBYTE(AIC7870[CLRSINT1], 0xff); /* pvs 5/15/94 */

   OUTBYTE(AIC7870[SXFRCTL0], CLRSTCNT|CLRCHN);
   OUTBYTE(AIC7870[SXFRCTL1],
           ((UBYTE)config_ptr->CFP_ConfigFlags & (STIMESEL + SCSI_PARITY)) |
            (ENSTIMER + ACTNEGEN +    /* low byte termination get set here */
            ((UBYTE)(config_ptr->CFP_TerminationLow) ? STPWEN : 0)));
   OUTBYTE(AIC7870[DFCNTRL], FIFORESET);
   OUTBYTE(AIC7870[SIMODE1], ENSCSIPERR + ENSELTIMO + ENSCSIRST);

   /* Re-Initialize sync/wide negotiation parameters */
   if (verbose_nego)
	   printf("Ph_ResetChannel: setting nego for %d targets\n",
		  config_ptr->Cf_MaxTargets);

   for (i = 0; i < config_ptr->Cf_MaxTargets; i++) {
	   if (verbose_nego)
		   printf("targ %d: config_ptr->Cf_ScsiOption 0x%x\n",
			  i, config_ptr->Cf_ScsiOption[i]);

      if (config_ptr->Cf_ScsiOption[i] & (WIDE_MODE | SYNC_MODE)) {
         Ph_SetNeedNego(i, base);
      } else {
	 xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + i;
	 xfer_option = OSM_CONV_BYTEPTR(xfer_option);
         OUTBYTE(AIC7870[xfer_option], 0);
      }
   }

   /*
    * Re-Initialize internal SCB's
    */
   if (config_ptr->CFP_MultiTaskLun)
      j = 128;
   else
      j = 16;
   for (i = 0; i < j; i++)
      Ph_OptimaIndexClearBusy(config_ptr, i);

   OUTBYTE(AIC7870[SCBPTR], 00);
}

/*********************************************************************
*
*  Check for underrun/overrun conditions following data transfer
*                  
* The host often issues a request sense or mode sense and gives a
* large buffer, and if the target responds with some info that is
* not enough to fill the buffer, the sequencer will signal an
* underrun.  Deal with those without printing scary messages to
* the user.
*********************************************************************/
void
Ph_CheckLength (sp_struct *scb_ptr, AIC_7870 *base)
{
   union gen_union res_lng;
   UBYTE phase;
   int i, set_over_under = 1;

   /*
    * Only complain about "important" commands, or if the user
    * wants us to complain about everything.  Even on the important
    * commands, the host sometimes only wants to see the first few
    * bytes.  Don't complain about those either.
    */
   if ((scb_ptr->Sp_CDB[0] != 0x28) &&
       (scb_ptr->Sp_CDB[0] != 0x2a) &&
       (scb_ptr->Sp_CDB[0] != 0x08) &&
       (scb_ptr->Sp_CDB[0] != 0x0a)) {
	   if (scb_ptr->SP_TargStat != UNIT_CHECK) {
		   scb_ptr->SP_TargStat = UNIT_GOOD;
		   scb_ptr->SP_Stat = SCB_COMP;
	   }
	   set_over_under = 0;
   }

   if ((phase = (INBYTE(AIC7870[SCSISIG]) & BUSPHASE)) == STPHASE) {

      /*
       * We are in STATUS phase.  Target thinks its done.
       * Read SCB16, SCB17, SCB18, and SCB19 into a 32 bit word. 
       * SCB16 is least significant.
       */
      res_lng.ubyte[0] = INBYTE(AIC7870[SCB19]);
      res_lng.ubyte[1] = INBYTE(AIC7870[SCB18]);
      res_lng.ubyte[2] = INBYTE(AIC7870[SCB17]);
      res_lng.ubyte[3] = INBYTE(AIC7870[SCB16]);

      scb_ptr->SP_ResCnt = res_lng.dword;

      if (scb_ptr->SP_TargStat == UNIT_CHECK)
         return;

   } else if ((phase & CDO) == 0) {

      /*
       * We are still in data phase.  Target still wants more 
       * data or still wants to give us more data.
       */
      scb_ptr->SP_ResCnt = -1;

      OUTBYTE(AIC7870[SCSISIG], phase);
      i = INBYTE(AIC7870[SXFRCTL1]);
      OUTBYTE(AIC7870[SXFRCTL1], i | BITBUCKET);
      while ((INBYTE(AIC7870[SSTAT1]) & PHASEMIS) == 0);
      OUTBYTE(AIC7870[SXFRCTL1], i);
   }

   if (set_over_under)
	   scb_ptr->SP_HaStat = HOST_DU_DO;
}


/*********************************************************************
*
*  Ph_CdbAbort routine -
*
*  Send SCSI abort msg to selected target
*
*  Return Value:  None
*                  
*  Parameters:    scb_ptr
*                 base address of AIC-7870
*
*  Activation:    PH_IntHandler
*                  
*  Remarks:       limited implementation, at present
*                  
*********************************************************************/
#ifndef _LESS_CODE
void Ph_CdbAbort (sp_struct *scb_ptr,
                  register AIC_7870 *base)
{
   cfp_struct *config_ptr;
   UBYTE phase;

   config_ptr = scb_ptr->SP_ConfigPtr;
   if ((INBYTE(AIC7870[SCSISIG]) & BUSPHASE) != MIPHASE)
   {
      Ph_BadSeq(config_ptr, base);
      return;
   }
   if (INBYTE(AIC7870[SCSIBUSL]) == MSG03)
   {
      OUTBYTE(AIC7870[SCSISIG], MIPHASE);
      INBYTE(AIC7870[SCSIDATL]);
      OUTBYTE(AIC7870[SEQADDR0], (UBYTE) SIOSTR3_ENTRY >> 2);
      OUTBYTE(AIC7870[SEQADDR1], (UBYTE) SIOSTR3_ENTRY >> 10);
   }
   else
   {
      OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
      do
      {
         INBYTE(AIC7870[SCSIDATL]);
         phase = Ph_Wt4Req(base);
      } while (phase == MIPHASE);
      if (phase != MOPHASE)
      {
         Ph_BadSeq(config_ptr, base);
         return;
      }
      OUTBYTE(AIC7870[SCSISIG], MOPHASE);
      OUTBYTE(AIC7870[CLRSINT1], CLRATNO);
      OUTBYTE(AIC7870[SCSIDATL], MSG07);
      Ph_Wt4Req(base);
      OUTBYTE(AIC7870[SEQADDR0], (UBYTE) SIO204_ENTRY >> 2);
      OUTBYTE((AIC7870[SEQADDR1]), (UBYTE) SIO204_ENTRY >> 10);
   }
}
#endif /* _LESS_CODE */


/*********************************************************************
*
* Perform SCSI bus reset
* Hold the reset signal for 64 ms.
* Then, stall for another 256ms because the SCSI spec says so.
*
*********************************************************************/
void Ph_ResetSCSI (AIC_7870 *base)
{
   UBYTE scb16, scb17;
   UBYTE simode0_value, simode1_value;

   scb16 = INBYTE(AIC7870[SCB16]);         /* save SCB16 and SCB17 */
   scb17 = INBYTE(AIC7870[SCB17]);
   simode0_value = INBYTE(AIC7870[SIMODE0]);   /* Disable all SCSI */
   simode1_value = INBYTE(AIC7870[SIMODE1]); /* to avoid confusing */
   OUTBYTE(AIC7870[SIMODE0],0);
   OUTBYTE(AIC7870[SIMODE1],0);                 /* clear interrupt */
   OUTBYTE(AIC7870[CLRINT], ANYINT);
   OUTBYTE(AIC7870[SCSISEQ], SCSIRSTO);          /* reset SCSI bus */

   us_delay(64000);

   OUTBYTE(AIC7870[SCSISEQ], 00);
   OUTBYTE(AIC7870[CLRSINT1], CLRSCSIRSTI);/* Patch for unexpected */
                                          /*  SCSI reset interrupt */
   OUTBYTE(AIC7870[CLRINT], CLRSEQINT);     /* clear interrupt     */
                                            /* chip is re-paused   */
   OUTBYTE(AIC7870[SIMODE0],simode0_value); /* Restore SIMODE0 and */
   OUTBYTE(AIC7870[SIMODE1],simode1_value);       /* SIMODE1 value */
   OUTBYTE(AIC7870[SCB16], scb16);            /* restore SCB16 and */
   OUTBYTE(AIC7870[SCB17], scb17);                       /* SCB 17 */

   us_delay(256 * 1000);
}


/*********************************************************************
*
*  Ph_BadSeq routine -
*
*  Terminate SCSI command sequence because sequence that is illegal,
*  or if we just can't handle it.
*
*********************************************************************/
void Ph_BadSeq (cfp_struct *config_ptr,
                register AIC_7870 *base)
{
   UBYTE HaStatus;

   /* Don't reset/abort if there is a SCSI bus free interrupt pending */
   if (!((Ph_ReadIntstat(base) & SCSIINT) && (INBYTE(AIC7870[SSTAT1]) & BUSFREE)))
   {
      printf("Resetting SCSI bus\n");
      Ph_ResetSCSI(base);
      Ph_ResetChannel(config_ptr);
   }

   /* Now, Restart Sequencer */
   printf("Restarting sequencer\n");
   OUTBYTE(AIC7870[SEQADDR0], (UBYTE) IDLE_LOOP_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], (UBYTE) IDLE_LOOP_ENTRY >> 10);
}


/*********************************************************************
*
*  Ph_CheckCondition routine -
*
*  handle response to target check condition
*
*  Return Value:  None
*                  
*  Parameters:    scb_ptr
*                 base address of AIC-7870
*
*  Activation:    PH_IntHandler
*                  
*  Remarks:                
*                  
*********************************************************************/
void Ph_CheckCondition (sp_struct *scb_ptr,
                        register AIC_7870 *base)
{
   UBYTE i;
   UBYTE scb;
   UBYTE status;
   UBYTE xfer_option;

   if (scb_ptr->SP_TargStat == UNIT_CHECK)
   {
      scb_ptr->SP_HaStat = HOST_SNS_FAIL;
      status = 0;
   }
   else 
   {
      status = INBYTE(AIC7870[PASS_TO_DRIVER]);
      scb_ptr->SP_TargStat = status;
   }
   scb = INBYTE(AIC7870[ACTIVE_SCB]);
   if ((status == UNIT_CHECK) && (scb_ptr->SP_AutoSense))
   {
      scb_ptr->SP_NegoInProg = 0;
      PHM_REQUESTSENSE(scb_ptr,scb);
   }
   else
   {
      /* clear target busy must be done here also */
      PHM_CLEARTARGETBUSY(scb_ptr->SP_ConfigPtr,scb);
      Ph_TerminateCommand(scb_ptr, scb);
   }

   /* Reset synchronous/wide negotiation */
   /* reset sync/wide as long as configured to do so   */
   /* even if it's negotiated without sync/wide        */
   i = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;
   if (scb_ptr->SP_ConfigPtr->Cf_ScsiOption[i] & (WIDE_MODE | SYNC_MODE))
   {
      xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + i;
      xfer_option = OSM_CONV_BYTEPTR(xfer_option);
      OUTBYTE(AIC7870[xfer_option], NEEDNEGO);
   }

   OUTBYTE(AIC7870[SXFRCTL0], INBYTE(AIC7870[SXFRCTL0]) | CLRCHN);
   OUTBYTE(AIC7870[SIMODE1], INBYTE(AIC7870[SIMODE1]) & ~(ENBUSFREE | ENSCSIRST));
   INBYTE(AIC7870[SCSIDATL]);
   OUTBYTE(AIC7870[SEQADDR0], (UBYTE) IDLE_LOOP_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], (UBYTE) IDLE_LOOP_ENTRY >> 10);
/*   if ((wt4bfree(ha_ptr)) & REQINIT) Ph_BadSeq(ha_ptr, scb_ptr, base); */
}

/*********************************************************************
*
*  Abort current target
*
* This thing may call Ph_BadSeq, which resets the scsi bus and the sequencer.
* So call this only as last resort.
*
*********************************************************************/
UBYTE Ph_TargetAbort (cfp_struct *config_ptr,
                      sp_struct *scb_ptr,
                      register AIC_7870 *base)
{
   UBYTE abt_msg = MSG06, i;
   UBYTE scb_index;

   if ((Ph_ReadIntstat(base) & INTCODE) == NO_ID_MSG) {
      OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
      INBYTE(AIC7870[SCSIDATL]);
   }

   if (Ph_Wt4Req(base) == MOPHASE) {
	   scb_index = INBYTE(AIC7870[ACTIVE_SCB]);

	   if (scb_index != INVALID_SCB_INDEX) {
		   if (scb_ptr == (sp_struct *) 0) {
			   printf("Ph_TargetAbort: null scb even though active_scb index is %d\n",
				  scb_index);
			   return 0;
		   }
	   }

	   if (scb_ptr->SP_MgrStat == SCB_ABORTINPROG) {
		   scb_ptr->SP_MgrStat = SCB_ABORTED;
		   return(0);
	   } else if (scb_ptr->SP_MgrStat == SCB_BDR) {
		   return(0);
	   }

	   if (INBYTE(AIC7870[SCB01]) & TAG_ENABLE)
		   abt_msg = MSG0D;

      if (Ph_SendTrmMsg(scb_ptr->SP_ConfigPtr, abt_msg) & BUSFREE)
      {
         OUTBYTE(AIC7870[CLRSINT1], CLRBUSFREE);
         OUTBYTE(AIC7870[CLRINT], CLRSCSINT);
         i = INBYTE(AIC7870[SCBPTR]);
         if (config_ptr->CFP_HaDataPtr->hsp_active_ptr[i] != NOT_DEFINED)
         {
            OUTBYTE(AIC7870[SCB01], 00);
            scb_ptr->SP_HaStat = HOST_ABT_HA;
            scb_ptr->SP_MgrStat = SCB_ABORTED;
            Ph_TerminateCommand(scb_ptr, i);
            Ph_PostCommand(config_ptr);
            OUTBYTE(AIC7870[SEQADDR0], (UBYTE) IDLE_LOOP_ENTRY >> 2);
            OUTBYTE(AIC7870[SEQADDR1], (UBYTE) IDLE_LOOP_ENTRY >> 10);
         }
         return(0);
      }

   }

   Ph_BadSeq(config_ptr, base);
   return(1);
}

/*********************************************************************
*  Ph_SendTrmMsg routine -
*
*  Send termination message out (Abort or Bus Device Reset) to target.
*
*  Return Value: High 3 bits - Bus Phase from SCSISIG
*                Bit 3 - Bus Free from SSTAT1
*                Bit 0 - Reqinit from SSTAT1
*
*  Parameters: term_msg - Message to send (Bus Device Reset,
*                         Abort or Abort Tag)
*
*              tgtid    - SCSI ID of target to send message to.
*
*              scsi_state - 0C : No SCSI devices connected.
*                           00 : Specified target currently connected.
*                           88 : Specified target selection in progress.
*                           40 : Other device connected.
*
*  Activation: Ph_AbortActive
*
*  Remarks:
*
*********************************************************************/
#ifdef   _FAULT_TOLERANCE
UBYTE Ph_SendTrmMsg ( cfp_struct *config_ptr,
                         UBYTE term_msg)
{
   return(0);
}
#endif   /* _FAULT_TOLERANCE */

/*********************************************************************
*  Ph_Wt4BFree routine -
*
*  Wait for bus free
*
*  Return Value: High 3 bits - Bus Phase from SCSISIG
*                Bit 3 - Bus Free from SSTAT1
*                Bit 0 - Reqinit from SSTAT1
*
*  Parameters: term_msg - Message to send (Bus Device Reset,
*                         Abort or Abort Tag)
*
*              tgtid    - SCSI ID of target to send message to.
*
*              scsi_state - 0C : No SCSI devices connected.
*                           00 : Specified target currently connected.
*                           88 : Specified target selection in progress.
*                           40 : Other device connected.
*
*  Activation: Ph_AbortActive
*
*  Remarks:
*
*********************************************************************/
/*
#ifdef   _FAULT_TOLERANCE
UBYTE Ph_Wt4BFree (cfp_struct *config_ptr)
{
}
#endif
*/
/*********************************************************************
*  Ph_TrmCmplt routine -
*
*  Process terminate and complete
*
*  Return Value:
*
*  Parameters:
*
*  Activation: PH_IntHandler
*              Ph_ProcBkpt
*
*  Remarks:
*
*********************************************************************/
#ifdef   _FAULT_TOLERANCE
UBYTE Ph_TrmCmplt ( cfp_struct *config_ptr,
                        UBYTE busphase,
                        UBYTE term_msg)
{
   return(0);
}
#endif   /* _FAULT_TOLERANCE */

/*********************************************************************
*  Ph_ProcBkpt routine -
*
*  Process break point interrupt
*
*  Return Value:
*
*  Parameters:
*
*  Activation: PH_IntHandler
*
*  Remarks:
*
*********************************************************************/
#ifdef   _FAUL_TOLERANCE
UBYTE Ph_ProcBkpt (cfp_struct *config_ptr)
{
}
#endif   /* _FAULT_TOLERANCE */

/*********************************************************************
*  Ph_BusReset routine -
*
*  Perform SCSI Bus Reset and clear SCB queue.
*
*  Return Value:  
*                  
*  Parameters:    
*
*  Activation:    Ph_HardReset
*                 Ph_SendTrmMsg
*                 Ph_TrmComplt
*
*  Remarks:       
*                 
*********************************************************************/
#ifdef   _FAULT_TOLERANCE
void  Ph_BusReset (cfp_struct *config_ptr)
{
}
#endif   /* _FAULT_TOLERANCE */

/*********************************************************************
*  Following functions related to synchronous/wide negotiation
*  may be reorgnized/rewritten in the future:
*     Ph_SendMsgo
*     Ph_Negotiate
*     Ph_SyncSet
*     Ph_SyncNego 
*     Ph_ExtMsgi
*     Ph_ExtMsgo
*     Ph_HandleMsgi
*********************************************************************/

/*********************************************************************
*
*  Ph_SendMsgo routine -
*
*  send message out
*
*  Return Value:  None
* 
*  Parameters:    scb_ptr
*                 base address of AIC-7870
*
*  Activation:    PH_IntHandler
*                 Ph_Negotiate                  
*
*  Remarks:                
*                  
*********************************************************************/
void Ph_SendMsgo (sp_struct *scb_ptr,
                  register AIC_7870 *base)
{
   register UBYTE j;
   register UBYTE scsi_rate;
   UBYTE xfer_option;

   j = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;
   xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + j;
   xfer_option = OSM_CONV_BYTEPTR(xfer_option);

   OUTBYTE(AIC7870[SCSISIG], MOPHASE);
   if (INBYTE(AIC7870[SCSISIG]) & ATNI)
   {
      if (scb_ptr->SP_NegoInProg)
      {
         if (scb_ptr->Sp_ExtMsg[0] == 0xff)
         {
            scb_ptr->Sp_ExtMsg[0] = 0x01;
            if (Ph_ExtMsgo(scb_ptr, base) != MIPHASE)
            {
               return;
            }
            if (INBYTE(AIC7870[SCSIBUSL]) != MSG07)
            {
               return;
            }
            if (scb_ptr->Sp_ExtMsg[2] == MSGSYNC)
            {
               scsi_rate = INBYTE(AIC7870[xfer_option]) & WIDEXFER;
            }
            else
            {
               scsi_rate = 0;
            }
            OUTBYTE(AIC7870[xfer_option], scsi_rate);
            OUTBYTE(AIC7870[SCSIRATE], scsi_rate);
            INBYTE(AIC7870[SCSIDATL]);
            return;
         }
         scb_ptr->SP_NegoInProg = 0;
         if (scb_ptr->Sp_ExtMsg[0] == 0x01)
         {
            Ph_SyncNego(scb_ptr, base);
         }
         else if (INBYTE(AIC7870[xfer_option]) == NEEDNEGO)
         {
            Ph_Negotiate(scb_ptr, base);
         }
         return;
      }
      j = INBYTE(AIC7870[SXFRCTL1]);            /* Turn off parity checking to*/
      OUTBYTE(AIC7870[SXFRCTL1], j & ~ENSPCHK); /* clear any residual error.  */
      OUTBYTE(AIC7870[SXFRCTL1], j | ENSPCHK);  /* Turn it back on explicitly */
                                                /* because it may have been   */
                                                /* cleared in 'Ph_ParityError'. */
                                                /* (It had to been previously */
                                                /* set or we wouldn't have    */
                                                /* gotten here.)              */
      OUTBYTE(AIC7870[CLRSINT1], CLRSCSIPERR | CLRATNO);
      OUTBYTE(AIC7870[CLRINT], CLRSCSINT);
#ifdef   LANCE_A
      Ph_UnPause(base);
#endif   /* LANCE_A */
      OUTBYTE(AIC7870[SCSIDATL], MSG05);
   }
   else
      OUTBYTE(AIC7870[SCSIDATL], MSG08);
   while (INBYTE(AIC7870[SCSISIG]) & ACKI);
   return;
}


/*********************************************************************
*
*  Ph_SetNeedNego routine -
*
*  Setup Sequencer XFER_OPTION with NEEDNEGO (Need to Negotiate)
*
*  Return Value:  None
*                  
*  Parameters: index into XFER_OPTION
*              base address of AIC-7870
*
*  Activation: Ph_ResetChannel
*              Ph_CheckCondition
*              Ph_ExtMsgi
*                  
*  Remarks:                
*                  
*********************************************************************/
void Ph_SetNeedNego (UBYTE index,
                     register AIC_7870 *base)
{
   UBYTE xfer_option;

   if (verbose_nego)
	   printf("Ph_SetNeedNego: for target %d\n", index);

   xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + index;
   xfer_option = OSM_CONV_BYTEPTR(xfer_option);
   OUTBYTE(AIC7870[xfer_option], NEEDNEGO);
}      

/*********************************************************************
*
*  Ph_Negotiate routine -
*
*  Initiate synchronous and/or wide negotiation
*
*  Return Value:  None
*                  
*  Parameters: scb_ptr
*              base address of AIC-7870
*
*  Activation: PH_IntHandler
*              Ph_SendMsgo
*                  
*  Remarks:                
*                  
*********************************************************************/
void Ph_Negotiate (sp_struct *scb_ptr,
                   register AIC_7870 *base)
{
   cfp_struct *config_ptr;
   UBYTE i;
   UBYTE xfer_option;

   config_ptr = scb_ptr->SP_ConfigPtr;

   OUTBYTE(AIC7870[SEQADDR0], (UBYTE) SIOSTR3_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], (UBYTE) SIOSTR3_ENTRY >> 10);

   i = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;

   xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + i;
   xfer_option = OSM_CONV_BYTEPTR(xfer_option);

   if ((INBYTE(AIC7870[SCSISIG]) & BUSPHASE) != MOPHASE)
   {
      OUTBYTE(AIC7870[xfer_option], 00);
      OUTBYTE(AIC7870[SCSIRATE], 00);
      Ph_ClearFast20Reg(config_ptr, scb_ptr);
      return;
   }      


   /*
    * If this is the first time through the negotiation sequence, then
    * NEEDNEGO will be set.  Fill in the ExtMsg array and send it out.
    * In Ph_ExtMsgi, the next message will be put into the ExtMsg array.
    * When this function is called again, just call Ph_SendMsgo to send out
    * the second message.
    */
   if (INBYTE(AIC7870[xfer_option]) == NEEDNEGO) {

      OUTBYTE(AIC7870[xfer_option], 00);
      OUTBYTE(AIC7870[SCSIRATE], 00);
      Ph_ClearFast20Reg(config_ptr, scb_ptr);
      if (verbose_nego)
	      printf("Ph_Negotiate: targ %d Cf_ScsiOption 0x%x negoinprog 0x%x\n",
		     i, config_ptr->Cf_ScsiOption[i], scb_ptr->SP_NegoInProg);


      scb_ptr->Sp_ExtMsg[0] = MSG01;
      switch (config_ptr->Cf_ScsiOption[i] & (WIDE_MODE | SYNC_MODE))
      {
      case (WIDE_MODE | SYNC_MODE):
      case WIDE_MODE:
         scb_ptr->Sp_ExtMsg[1] = 2;	/* wide msg len */
         scb_ptr->Sp_ExtMsg[2] = MSGWIDE;
         scb_ptr->Sp_ExtMsg[3] = WIDE_WIDTH;
	 Ph_WideNego(scb_ptr, base);
	 break;

      case SYNC_MODE:
         scb_ptr->Sp_ExtMsg[1] = 3;
         scb_ptr->Sp_ExtMsg[2] = MSGSYNC;

	 if (config_ptr->CFP_EnableFast20) {
		 scb_ptr->Sp_ExtMsg[3] = (((config_ptr->Cf_ScsiOption[i] & SYNC_RATE) >> 4) * 4) + 12;
	 } else {
		 scb_ptr->Sp_ExtMsg[3] = (((config_ptr->Cf_ScsiOption[i] & SYNC_RATE) >> 4) * 6) + 25;
		 if (config_ptr->Cf_ScsiOption[i] & SXFR2)
			 {
				 ++scb_ptr->Sp_ExtMsg[3];
			 }
	 }

	 /* XXX hmm, will wide devices go to WIDE_OFFSET ? */
	 if (config_ptr->Cf_AllowSyncMask & (1 << i))
		 scb_ptr->Sp_ExtMsg[4] = NARROW_OFFSET;
	 else
		 scb_ptr->Sp_ExtMsg[4] = 0;
         Ph_SyncNego(scb_ptr, base);
	 break;
      }

   } else {

      Ph_SendMsgo(scb_ptr, base);
   }
	   
}

/*********************************************************************
*
*  Ph_SyncSet routine -
*
*  Set synchronous transfer rate based on negotiation
*
*  Return Value:  synchronous transfer rate (unsigned char)
*                  
*  Parameters: scb_ptr
*
*  Activation: Ph_ExtMsgi
*              Ph_ExtMsgo
*                  
*  Remarks:
*                  
*********************************************************************/
UBYTE Ph_SyncSet (sp_struct *scb_ptr)
{
   UBYTE sync_rate;

   if (scb_ptr->Sp_ExtMsg[3] == 12)          /* double speed checking */
      sync_rate = 0x00;
   else if (scb_ptr->Sp_ExtMsg[3] <= 16)
      sync_rate = 0x10;
   else if (scb_ptr->Sp_ExtMsg[3] <= 20)
      sync_rate = 0x20;
   else if (scb_ptr->Sp_ExtMsg[3] <= 25)
      sync_rate = 0x00;
   else if (scb_ptr->Sp_ExtMsg[3] <= 31)
      sync_rate = 0x10;
   else if (scb_ptr->Sp_ExtMsg[3] <= 37)
      sync_rate = 0x20;
   else if (scb_ptr->Sp_ExtMsg[3] <= 43)
      sync_rate = 0x30;
   else if (scb_ptr->Sp_ExtMsg[3] <= 50)
      sync_rate = 0x40;
   else if (scb_ptr->Sp_ExtMsg[3] <= 56)
      sync_rate = 0x50;
   else if (scb_ptr->Sp_ExtMsg[3] <= 62)
      sync_rate = 0x60;
   else
      sync_rate = 0x70;
   return(sync_rate);
}

/*********************************************************************
*
*  Ph_SyncNego routine -
*
*  <brief description>
*
*  Return Value:  None
*                  
*  Parameters: stb_ptr
*              base address of AIC-7870
*
*  Activation: Ph_Negotiate
*              Ph_SendMsgo
*              Ph_WideNego
*                  
*  Remarks:                
*                  
*********************************************************************/
void Ph_SyncNego (sp_struct *scb_ptr,
                  register AIC_7870 *base)
{
   cfp_struct *config_ptr;
   UBYTE j, xfer_option, scsi_rate;

   if (verbose_nego)
	   printf("Ph_SyncNego\n");

   if ((j = Ph_ExtMsgo(scb_ptr, base)) != MIPHASE)
   {
      cmn_err(CE_WARN, "Ph_SyncNego: ph_extmsgo did not end in miphase, phase = 0x%x", j);
      return;
   }

   switch (INBYTE(AIC7870[SCSIBUSL]))
   {
   case MSG01:
      scb_ptr->SP_NegoInProg = 1;
      break;

   case MSG07:
      if (verbose_nego)
	      printf("Ph_SyncNego: rejected; negoInProg 0x%x\n",
		     scb_ptr->SP_NegoInProg);

      while (1)            /* Process any number of Message Rejects */
      {
         OUTBYTE(AIC7870[SCSISIG], MIPHASE);
         INBYTE(AIC7870[SCSIDATL]);
         if (Ph_Wt4Req(base) != MIPHASE)
         {
            break;
         }
         if (INBYTE(AIC7870[SCSIBUSL]) != MSG07)
         {
            break;
         }
      }

      j = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;
      xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + j;
      xfer_option = OSM_CONV_BYTEPTR(xfer_option);
      scsi_rate = INBYTE(AIC7870[xfer_option]);

      /* Make the rate asynchronous */
      scsi_rate = scsi_rate & WIDEXFER;
      OUTBYTE(AIC7870[xfer_option], scsi_rate);
      OUTBYTE(AIC7870[SCSIRATE], scsi_rate);

      config_ptr = scb_ptr->SP_ConfigPtr;
      Ph_ClearFast20Reg(config_ptr, scb_ptr);
      /*
       * the sync message was rejected by the target.  For the purpose
       * of Ph_LogFast20Map, the device is not a fast20 device.
       */
      scb_ptr->Sp_ExtMsg[3] = 70;
      Ph_LogFast20Map(config_ptr, scb_ptr);
      break;
   }
}

/*
 * Send the wide negotiation message out by calling Ph_ExtMsgo.
 *
 * If the target responds to the negotiation, set NegoInProg and let
 * Ph_ExtMsgi handle the response.
 * If the message is rejected, then record that fact; and if sync
 * negotiation is also requested, set up the ExtMsg array with the sync
 * negotiation message and start that.
 *
 * Return Value: None.
 *
 * Activation: Ph_Negotate.
 */
void
Ph_WideNego(sp_struct *scb_ptr, AIC_7870 *base)
{
	cfp_struct *config_ptr;
	UBYTE i, j;

	if (verbose_nego)
		printf("Ph_WideNego: sending out wide message\n");

	if ((j = Ph_ExtMsgo(scb_ptr, base)) != MIPHASE) {
		cmn_err(CE_WARN, "Ph_WideNego: ph_extmsgo did not end in miphase, phase = 0x%x", j);
		return;
	}

	switch (INBYTE(AIC7870[SCSIBUSL])) {
	case MSG01:
		scb_ptr->SP_NegoInProg = 1;
		break;

	case MSG07:
		if (verbose_nego)
			printf("Ph_WideNego: wide negotiation rejected; negoinprog 0x%x\n",
			       scb_ptr->SP_NegoInProg);

		config_ptr = scb_ptr->SP_ConfigPtr;
		i = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;

		if (config_ptr->Cf_ScsiOption[i] & SYNC_MODE) {
			OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
			INBYTE(AIC7870[SCSIDATL]);
			if (Ph_Wt4Req(base) == MOPHASE) {
				OUTBYTE(AIC7870[SCSISIG], MOPHASE | ATNO);
				scb_ptr->Sp_ExtMsg[1] = 3;
				scb_ptr->Sp_ExtMsg[2] = MSGSYNC;
				if (config_ptr->CFP_EnableFast20) {
					scb_ptr->Sp_ExtMsg[3] = (((config_ptr->Cf_ScsiOption[i] & SYNC_RATE) >> 4) * 4) + 12;
				} else {
					scb_ptr->Sp_ExtMsg[3] = (((config_ptr->Cf_ScsiOption[i] & SYNC_RATE) >> 4) * 6) + 25;
					if (config_ptr->Cf_ScsiOption[i] & SXFR2)
						++scb_ptr->Sp_ExtMsg[3];
				}
				scb_ptr->Sp_ExtMsg[4] = NARROW_OFFSET;
				Ph_SyncNego(scb_ptr, base);
			}

		} else {
			OUTBYTE(AIC7870[SCSISIG], MIPHASE);
			INBYTE(AIC7870[SCSIDATL]);
		}

		break;
	}
}

/*********************************************************************
*
*  Ph_ExtMsgi routine -
*
*  Receive and interpret extended message in
*
*  Return Value:  None
*                  
*  Parameters: scb_ptr
*              base address of AIC-7870
*
*  Activation: PH_IntHandler
*                  
*  Remarks:
*                  
*********************************************************************/
void Ph_ExtMsgi (sp_struct *scb_ptr,
                 register AIC_7870 *base)
{
   cfp_struct *config_ptr;
   int sync_tr;
   UBYTE c, i, j, max_rate, phase, scsi_rate;
   UBYTE nego_flag = NONEGO;
   UBYTE max_width = 0;
   UBYTE max_offset = NARROW_OFFSET;
   UBYTE xfer_option;

   config_ptr = scb_ptr->SP_ConfigPtr;

   j = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;

   c = INBYTE(AIC7870[PASS_TO_DRIVER]);

   scb_ptr->Sp_ExtMsg[0] = MSG01;
   scb_ptr->Sp_ExtMsg[1] = c--;
   scb_ptr->Sp_ExtMsg[2] = INBYTE(AIC7870[SCSIBUSL]);
   for (i = 3; c > 0; --c)
   {
      INBYTE(AIC7870[SCSIDATL]);
      if ((phase = Ph_Wt4Req(base)) != MIPHASE)
      {
         if ((INBYTE(AIC7870[SCSISIG]) & (BUSPHASE | ATNI)) == (MOPHASE | ATNI))
         {
            if (scb_ptr->SP_NegoInProg)
            {
               OUTBYTE(AIC7870[SCSISIG], MOPHASE | ATNO);

               /*  Setup Sequencer XFER_OPTION with NEEDNEGO        */
               /*     (Need to Negotiate)                           */
               Ph_SetNeedNego(j, base);
            }
            else
            {
               OUTBYTE(AIC7870[CLRSINT1], CLRATNO);
               OUTBYTE(AIC7870[SCSISIG], MOPHASE);
            }
            i = INBYTE(AIC7870[SXFRCTL0]);
            j = INBYTE(AIC7870[SXFRCTL1]);
            OUTBYTE(AIC7870[SXFRCTL1], j & ~ENSPCHK); /* Clear parity error   */
            OUTBYTE(AIC7870[SXFRCTL1], j | ENSPCHK);
            OUTBYTE(AIC7870[CLRINT], CLRSCSINT);
            OUTBYTE(AIC7870[SXFRCTL0], i & ~SPIOEN);  /* Place message parity */
            OUTBYTE(AIC7870[SCSIDATL], MSG09);        /* error on bus without */
            OUTBYTE(AIC7870[SXFRCTL0], i | SPIOEN);   /* an ack.              */
         }
         else
         {
            if (phase == ERR)
               return;

            Ph_BadSeq(config_ptr, base);
         }
         return;
      }
      if (i < 5)
      {
         scb_ptr->Sp_ExtMsg[i++] = INBYTE(AIC7870[SCSIBUSL]);
      }
   }	/* end of for (i = 3; c > 0; --c) */


   /* calculate the maximum transfer rate, either double speed, or non-double speed */
   i = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;

   if (config_ptr->CFP_EnableFast20)
   {
      max_rate = (((config_ptr->Cf_ScsiOption[i] & SYNC_RATE) >> 4) * 4) + 12;
   }
   else
   {
      max_rate = (((config_ptr->Cf_ScsiOption[i] & SYNC_RATE) >> 4) * 6) + 25;
      if (config_ptr->Cf_ScsiOption[i] & SXFR2)
         ++max_rate;
   }

   /* Respond as a 16-bit device only if 2940W AND we are not  */
   /* suppressing negotiation.                                 */
   if (    (config_ptr->CFP_SuppressNego)
       && !(scb_ptr->SP_NegoInProg))
   {
      max_width = 0;                /* 8-bit device   */
      max_offset = 0;
      max_rate = 0;
   }
   else if (INBYTE(AIC7870[SBLKCTL]))
   {
      max_width = WIDE_WIDTH;
   }

   xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + i;
   xfer_option = OSM_CONV_BYTEPTR(xfer_option);

   if (verbose_nego) {
	   int z;
	   printf("Ph_ExtMsgi: targ %d\n", i);
	   for (z=0; z < (int)scb_ptr->Sp_ExtMsg[1] + 2; z++)
		   printf("[%d] = 0x%x\n", z, scb_ptr->Sp_ExtMsg[z]);
   }


   switch (scb_ptr->Sp_ExtMsg[2])
   {
   case MSGWIDE:
      if (scb_ptr->Sp_ExtMsg[1] == 2)
      {

         OUTBYTE(AIC7870[xfer_option], 00);
         OUTBYTE(AIC7870[SCSIRATE], 00);

	 /* Harry's code has this, but it's a redundant call, since
	  * Ph_ClearFast20Reg just returns if ExtMsg[2] != MSGSYNC
	  *
	  * Ph_ClearFast20Reg(config_ptr, scb_ptr);
	  */
	 

         if (scb_ptr->Sp_ExtMsg[3] > max_width)
         {
            scb_ptr->Sp_ExtMsg[3] = max_width;
            nego_flag = NEEDNEGO;
         }

         if (!scb_ptr->SP_NegoInProg)
            break;

         scb_ptr->SP_NegoInProg = 0;

         if (nego_flag == NONEGO)
         {
            if (scb_ptr->Sp_ExtMsg[3])
            {
		   /* Target can do wide */
               OUTBYTE(AIC7870[xfer_option], WIDEXFER);
               OUTBYTE(AIC7870[SCSIRATE], WIDEXFER);
               max_offset = WIDE_OFFSET;
            }

	    /* start to process the synchronous data transfer part */
            if (config_ptr->Cf_ScsiOption[i] & SYNC_MODE)
            {
               scb_ptr->Sp_ExtMsg[1] = 3;
               scb_ptr->Sp_ExtMsg[2] = MSGSYNC;
               scb_ptr->Sp_ExtMsg[3] = max_rate;
               scb_ptr->Sp_ExtMsg[4] = max_offset;
               OUTBYTE(AIC7870[SCSISIG], ATNO | MIPHASE);
               scb_ptr->SP_NegoInProg = 1;
            }
            return;
         }
      }
      scb_ptr->Sp_ExtMsg[1] = 2;

   case MSGSYNC:
      if (scb_ptr->Sp_ExtMsg[1] == 3)	/* make sure the length is correct */
      {
         scsi_rate = INBYTE(AIC7870[xfer_option]);
         if (scsi_rate == NEEDNEGO)
         {
            scsi_rate = 0;
         }
         else
         {
            scsi_rate &= WIDEXFER;
         }
         OUTBYTE(AIC7870[xfer_option], scsi_rate);
         OUTBYTE(AIC7870[SCSIRATE], scsi_rate);

         if (scb_ptr->Sp_ExtMsg[4]) {
		 /* target wants sync transfers */
		 sync_tr = 1;

		 if (scsi_rate)
			 max_offset = WIDE_OFFSET;

		 if (scb_ptr->Sp_ExtMsg[4] > max_offset) {
			 scb_ptr->Sp_ExtMsg[4] = max_offset;
			 nego_flag = NEEDNEGO;
		 }

		 if (scb_ptr->Sp_ExtMsg[3] < max_rate) {
			 scb_ptr->Sp_ExtMsg[3] = max_rate;
			 nego_flag = NEEDNEGO;
		 } else if (scb_ptr->Sp_ExtMsg[3] > 68) {
			 scb_ptr->Sp_ExtMsg[4] = 0;
			 nego_flag = NEEDNEGO;
			 OUTBYTE(AIC7870[SCSISIG],ATNO | MIPHASE);
			 scb_ptr->SP_NegoInProg = 1;
			 return;
		 }
         }  else {
		 /* target wants async transfers.  Clear all bits except for
		  * the WIDEXFER bit.  Screw with the ExtMsg[3] to make
		  * LogFast20Map happy. */

		 sync_tr = 0;
		 scb_ptr->Sp_ExtMsg[3] = FAST20_THRESHOLD;
		 scsi_rate &= WIDEXFER;
	 }

         if (!scb_ptr->SP_NegoInProg)		/* tgt initiated nego */
            break;

         scb_ptr->SP_NegoInProg = 0;
         if (nego_flag == NONEGO) {
		 if (sync_tr)
			 scsi_rate += Ph_SyncSet(scb_ptr) + scb_ptr->Sp_ExtMsg[4];

		 if (verbose_nego)
			 printf("Ph_ExtMsgi: done with targ %d, scsi_rate 0x%x\n",
				i, scsi_rate);

		 OUTBYTE(AIC7870[xfer_option], scsi_rate);
		 OUTBYTE(AIC7870[SCSIRATE], scsi_rate);
		 Ph_LogFast20Map(config_ptr, scb_ptr);  /* record in Scratch RAM */
		 return;
         }
      }

   default:
      OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
      INBYTE(AIC7870[SCSIDATL]);
      if ((phase = Ph_Wt4Req(base)) == MOPHASE)
      {
         OUTBYTE(AIC7870[SCSISIG], MOPHASE);
         OUTBYTE(AIC7870[CLRSINT1], CLRATNO);
         OUTBYTE(AIC7870[SXFRCTL0], INBYTE(AIC7870[SXFRCTL0]) & ~SPIOEN);
         OUTBYTE(AIC7870[SCSIDATL], MSG07);
         OUTBYTE(AIC7870[SXFRCTL0], INBYTE(AIC7870[SXFRCTL0]) | SPIOEN);
      }
      else
      {
         if (phase == ERR)
         {
            return;
         }
         Ph_BadSeq(config_ptr, base);
      }
      return;
   }

   scb_ptr->Sp_ExtMsg[0] = 0xff;
   OUTBYTE(AIC7870[SCSISIG], ATNO | MIPHASE);
   scb_ptr->SP_NegoInProg = 1;
}

/*********************************************************************
*
*  Ph_ExtMsgo routine -
*
*  Send extended message out
*
*  Return Value: current scsi bus phase (unsigned char)
*             
*  Parameters: scb_ptr
*              base address of AIC-7870
*
*  Activation: Ph_Negotiate
*              Ph_SendMsgo
*              Ph_SyncNego
*             
*  Remarks:   
*             
*********************************************************************/
UBYTE Ph_ExtMsgo (sp_struct *scb_ptr,
                  register AIC_7870 *base)
{
   UBYTE c;
   UBYTE i = 0;
   UBYTE j;
   UBYTE scsi_rate = 0;
   UBYTE savcnt0,savcnt1,savcnt2;   /* save for stcnt just in case of pio */
   UBYTE xfer_option;
   cfp_struct *config_ptr;

   savcnt0 = INBYTE(AIC7870[STCNT0]);        /* save STCNT here */
   savcnt1 = INBYTE(AIC7870[STCNT1]);        /* save STCNT here */
   savcnt2 = INBYTE(AIC7870[STCNT2]);        /* save STCNT here */

   config_ptr = scb_ptr->SP_ConfigPtr;

   /* Transfer all but the last byte of the extended message */
   for (c = scb_ptr->Sp_ExtMsg[1] + 1; c > 0; --c)
   {
      OUTBYTE(AIC7870[SCSIDATL], scb_ptr->Sp_ExtMsg[i++]);
      /*
       * Upon seeing the first byte (01 extended message), some devices, such
       * as the EXABYTE, will go to message in phase and reject the message.
       * So this case should be expected.
       */
      if (Ph_Wt4Req(base) != MOPHASE)
      {
	 OUTBYTE(AIC7870[CLRSINT1], CLRATNO);
         break;
      }
   }

   if (c == 0)             /* Removed semi-colon 1/7/94 */
   {
      if (scb_ptr->SP_NegoInProg)
      {
         j = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;
	 xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + j;
	 xfer_option = OSM_CONV_BYTEPTR(xfer_option);
         if (scb_ptr->Sp_ExtMsg[2] == MSGWIDE)
         {
            if (scb_ptr->Sp_ExtMsg[3])
            {
               scsi_rate = WIDEXFER;
            }
         }
         else
         {
            scsi_rate = INBYTE(AIC7870[xfer_option]) + Ph_SyncSet(scb_ptr)
		        + scb_ptr->Sp_ExtMsg[4];
            Ph_LogFast20Map(config_ptr, scb_ptr);  /* record in Scratch RAM */
         }
         OUTBYTE(AIC7870[xfer_option], scsi_rate);
         OUTBYTE(AIC7870[SCSIRATE], scsi_rate);
      }
      scb_ptr->SP_NegoInProg ^= 1;
      OUTBYTE(AIC7870[CLRSINT1], CLRATNO);
      OUTBYTE(AIC7870[SCSIDATL], scb_ptr->Sp_ExtMsg[i]);
   }

   OUTBYTE(AIC7870[STCNT0],savcnt0);            /* restore STCNT */
   OUTBYTE(AIC7870[STCNT1],savcnt1);            /* restore STCNT */
   OUTBYTE(AIC7870[STCNT2],savcnt2);            /* restore STCNT */

   return(Ph_Wt4Req(base));
}

/*********************************************************************
*
*  Ph_HandleMsgi routine -
*
*  Handle Message In
*
*  Return Value: none
*
*  Parameters: scb_ptr
*              base address of AIC-7870
*
*  Activation: PH_IntHandler
*
*  Remarks:
*
*********************************************************************/
void Ph_HandleMsgi (sp_struct *scb_ptr,
                    register AIC_7870 *base)
{
   cfp_struct *config_ptr = scb_ptr->SP_ConfigPtr;
   UBYTE rejected_msg;
   UBYTE phase;

   if (verbose_nego) {
	   printf("Ph_HandleMsgi: \n");
   }

   if ((INBYTE(AIC7870[SCSISIG]) & ATNI) == 0)
   {
      if (INBYTE(AIC7870[SCSIBUSL]) == MSG07)
      {
         rejected_msg = INBYTE(AIC7870[PASS_TO_DRIVER]); /* Get rejected msg    */
         if (rejected_msg & (MSGID | MSGTAG))           /* If msg Identify or  */ 
         {                                              /* tag type, abort     */
            OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
            INBYTE(AIC7870[SCSIDATL]);
            PHM_TARGETABORT(config_ptr, scb_ptr, base);
            return;
         }
      }
      else
      {
         OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
         do {
            INBYTE(AIC7870[SCSIDATL]);
            phase = Ph_Wt4Req(base);
         } while (phase == MIPHASE);
         if (phase != MOPHASE)
         {
            Ph_BadSeq(config_ptr, base);
            return;
         }
         OUTBYTE(AIC7870[SCSISIG], MOPHASE);
         OUTBYTE(AIC7870[CLRSINT1], CLRATNO);
         OUTBYTE(AIC7870[SCSIDATL], MSG07);
         return;
      }
   }
   INBYTE(AIC7870[SCSIDATL]);
}

/*********************************************************************
*
* Handle SCSI selection timeout Interrupt
* The scb_ptr and scb must be valid, since some command timed out
* on some target.  If not valid, fix it up.
*
*********************************************************************/
void
Ph_IntSelto (cfp_struct *config_ptr, sp_struct *scb_ptr)
{
   AIC_7870 *base;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   UBYTE scb, scsiseq;

   if (adp_verbose)
	   printf("Ph_IntSelto: config_ptr 0x%x scb_ptr 0x%x\n",
		  config_ptr, scb_ptr);

   base = config_ptr->CFP_Base;
   scb = INBYTE(AIC7870[WAITING_SCB]);

   if (scb != adp_curr_scb_num) {
	   printf("IntSelto: scb %d does not match adp_curr_scb_num %d\n",
		  scb, adp_curr_scb_num);
	   scb = adp_curr_scb_num;
	   scb_ptr = ACTPTR[scb];
	   printf("new scb %d scb_ptr 0x%x\n",
		  scb, scb_ptr);
   }

   scsiseq = INBYTE(AIC7870[SCSISEQ]);
   scsiseq &= ~(ENSELO|ENAUTOATNO);
   OUTBYTE(AIC7870[SCSISEQ], scsiseq);

   Ph_OptimaIndexClearBusy(config_ptr,
			   ((UBYTE) scb_ptr->SP_Tarlun) >> 4);

   OUTBYTE(AIC7870[CLRSINT1], CLRSELTIMO|CLRBUSFREE);

   Ph_EnableNextScbArray(scb_ptr);
   scb_ptr->SP_HaStat = HOST_SEL_TO;
   Ph_TerminateCommand(scb_ptr, scb);
}


/*
 *******************************************************************
 * Set the selection timeout value.
 * Stimesel contains the 2 bit value, in the correct bit positions,
 * to be programmed into the XSFRCTL1 register.
 *******************************************************************
 */
void
PH_Set_Selto(cfp_struct *config_ptr, int stimesel)
{
	AIC_7870 *base;
	int need_unpause=0;
	UBYTE hcntrl, sxfrctl1;

	base = config_ptr->CFP_Base;
	/*
	 * If sequencer is not paused, pause it.
	 */
	hcntrl = INBYTE(AIC7870[HCNTRL]);
	if ((hcntrl & PAUSE) == 0) {
		Ph_WriteHcntrl(base, (hcntrl | PAUSE));
		need_unpause=1;
	}

	sxfrctl1 = INBYTE(AIC7870[SXFRCTL1]);
	sxfrctl1 &= ~(STIMESEL1|STIMESEL0);
	sxfrctl1 |= stimesel;
	OUTBYTE(AIC7870[SXFRCTL1], sxfrctl1);

	if (need_unpause)
		Ph_WriteHcntrl(base, hcntrl);
}
 

/*********************************************************************
*
*  Acknowledge and clear SCSI Bus Free interrupt
*
*********************************************************************/
void
Ph_IntFree (cfp_struct *config_ptr, sp_struct *scb_ptr)
{
   AIC_7870 *base;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   UBYTE scb;

   base = config_ptr->CFP_Base;
   scb = adp_curr_scb_num;
   scb_ptr = ACTPTR[adp_curr_scb_num];

   if (adp_verbose)
	   printf("Ph_IntFree: scb %d scb_ptr 0x%x\n",
		  scb, scb_ptr);

   /* Reset DMA & SCSI transfer logic */
   OUTBYTE(AIC7870[DFCNTRL],FIFORESET);
   OUTBYTE(AIC7870[SXFRCTL0], INBYTE(AIC7870[SXFRCTL0]) | (CLRSTCNT | CLRCHN | SPIOEN));
   OUTBYTE(AIC7870[SIMODE1], INBYTE(AIC7870[SIMODE1]) & ~ENBUSFREE);
   OUTBYTE(AIC7870[SCSIRATE], 0x00);
   OUTBYTE(AIC7870[CLRSINT1], CLRBUSFREE);

   OUTBYTE(AIC7870[SEQADDR0], (UBYTE) IDLE_LOOP_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], (UBYTE) IDLE_LOOP_ENTRY >> 10);

   if (scb_ptr != (sp_struct *) 0) {
      Ph_EnableNextScbArray(scb_ptr);
      Ph_OptimaClearTargetBusy(config_ptr, scb);
      scb_ptr->SP_HaStat = HOST_BUS_FREE;
      Ph_TerminateCommand(scb_ptr, scb);
   }
}


/*********************************************************************
*
*  Ph_ParityError routine -
*
*  handle SCSI parity errors
*
*  Return Value:  none
*                  
*  Parameters: base address of AIC-7870
*
*  Activation: PH_IntHandler
*                  
*  Remarks:                
*                  
*********************************************************************/
void Ph_ParityError (register AIC_7870 *base)
{
   UBYTE i;

   Ph_Wt4Req(base);

   i = INBYTE(AIC7870[SXFRCTL1]);            /* Turn parity checking off.  */
   OUTBYTE(AIC7870[SXFRCTL1], i & ~ENSPCHK); /* It will be turned back on  */
                                             /* in message out phase       */
}

/*********************************************************************
*
*  Ph_Wt4Req routine -
*
*  wait for target to assert REQ.
*
*  Return Value:  current SCSI bus phase
*
*  Parameters     Base address of AIC-7870
*
*  Activation:    most other HIM routines
*
*  Remarks:       bypasses sequencer
*
*********************************************************************/
UBYTE Ph_Wt4Req (register AIC_7870 *base)
{
   UBYTE stat;
   UBYTE phase;

   for (;;)
   {
      while (INBYTE(AIC7870[SCSISIG]) & ACKI);
      while (((stat = INBYTE(AIC7870[SSTAT1])) & REQINIT) == 0)
      {
         if (stat & (BUSFREE | SCSIRSTI))
         {
            return(ERR);
         }
      }
      OUTBYTE(AIC7870[CLRSINT1], CLRSCSIPERR);
      phase = INBYTE(AIC7870[SCSISIG]) & BUSPHASE;
      if ((phase & IOI) &&
          (phase != DIPHASE) &&
          (INBYTE(AIC7870[SSTAT1]) & SCSIPERR))
      {
         OUTBYTE(AIC7870[SCSISIG], phase);
         INBYTE(AIC7870[SCSIDATL]);
         continue;
      }
      return(phase);
   }
}

/*********************************************************************
*
*  Ph_MultOutByte routine -
*
*  Output same value start from the port and for the length specified
*
*  Return Value:  none phase
*
*  Parameters     base - start port address
*                 value - byte value sent to port
*                 length - number of bytes get sent
*
*  Activation:    
*
*  Remarks: This function should be used only when speed is not
*           critical
*
*********************************************************************/
#ifdef   MULT_OUT_BYTE
void Ph_MultOutByte (register AIC_7870 *base,UBYTE value,int length)
{
   int i;

   for (i =0; i<length; i++)
   {
      OUTBYTE(AIC7870[i],value);
   }
}
#endif   /* MULT_OUT_BYTE */

/*********************************************************************
*
*  Ph_MemorySet routine -
*
*  Set memory buffer with fixed value
*
*  Return Value:  none phase
*
*  Parameters     memptr - memory buffer pointer
*                 value - byte value set to buffer
*                 length - length of buffer to be set
*
*  Activation:    
*
*  Remarks: This function should be used only when speed is not
*           critical
*
*********************************************************************/

void Ph_MemorySet(UBYTE *memptr,UBYTE value,int length)
{
   WORD i;

#ifdef DEBUG
   if (((int) memptr) >= 0xbf500000)
	cmn_err(CE_WARN, "Ph_MemorySet: addr 0x%x", memptr);
#endif

   for (i=0; i<length; i++ )
      *memptr++ = value;
}

/*********************************************************************
*
* Pause the sequencer, and verify that it is paused by looking for
* the pause ACK.
*
*********************************************************************/

void
Ph_Pause (AIC_7870 *base)
{
	UBYTE hcntrl;
	int count=0;
	
	hcntrl = INBYTE(AIC7870[HCNTRL]);
	if (hcntrl & PAUSEACK)
		return;

	hcntrl |= PAUSE;

	OUTBYTE(AIC7870[HCNTRL], hcntrl);

	/*
	 * Now delay for a while to let the sequencer pause
	 */
	hcntrl = 0;
	while ((hcntrl & PAUSEACK) == 0) {
		us_delay(HCNTRL_PAUSE_DELAY);
		hcntrl = INBYTE(AIC7870[HCNTRL]);
		if (count++ > HCNTRL_PAUSE_LOOPS) {
			printf("SCSI chip malfunction (cannot pause) hcntrl=0x%x\n",
			       hcntrl);
			break;
		}
	}
}


/*********************************************************************
*
* UnPause the sequencer
* Sometimes the sequencer cannot be unpaused because there is an
* outstanding interrupt which needs to be serviced.
*
*********************************************************************/

void
Ph_UnPause (AIC_7870 *base)
{
   UBYTE hcntrl;
   int count=0;

   hcntrl = INBYTE(AIC7870[HCNTRL]);
   if ((hcntrl & PAUSE) == 0)
	   return;

   hcntrl &= ~PAUSE;
   OUTBYTE(AIC7870[HCNTRL], hcntrl);

   us_delay(HCNTRL_PAUSE_DELAY);
}


/*********************************************************************
*
*  Ph_WriteHcntrl routine -
*
*  Write to HCNTRL
*
*  Return Value:  none
*
*  Parameters     base
*                 output value
*
*  Activation:    
*
*  Remarks: This function is designed to test a work-around for the
*           asynchronous pause problem in Lance
*
*********************************************************************/

void Ph_WriteHcntrl (register AIC_7870 *base, UBYTE value)
{
   UBYTE hcntrl_data;
                                                        /* If output will  */
   if (!(value & PAUSE))                                /* pause chip, just*/
   {                                                    /* do the output.  */
      hcntrl_data = INBYTE(AIC7870[HCNTRL]);            
      if (!(hcntrl_data & PAUSEACK))                    /* If chip is not  */
      {                                                 /* paused, pause   */
         OUTBYTE(AIC7870[HCNTRL], hcntrl_data | PAUSE); /* the chip first. */
         while (!(INBYTE(AIC7870[HCNTRL]) & PAUSEACK));
      }                                                 /* If the chip is  */
      if (INBYTE(AIC7870[INTSTAT]) & ANYPAUSE)          /* paused due to an*/
      {                                                 /* interrupt, make */
         value |= PAUSE;                                /* sure we turn the*/
      }                                              	  /* pause bit on.   */
   }
   OUTBYTE(AIC7870[HCNTRL], value);
}


/*********************************************************************
*
* Read INTSTAT register.
* The sequencer must be paused before calling this.
*
*********************************************************************/

UBYTE
Ph_ReadIntstat (AIC_7870 *base)
{
   UBYTE hcntrl;
   UBYTE value;
               
   hcntrl = INBYTE(AIC7870[HCNTRL]);
   if (hcntrl & PAUSEACK) {
      us_delay(HCNTRL_PAUSE_DELAY);
      value = INBYTE(AIC7870[INTSTAT]);
   } else {
      printf("Ph_ReadIntstat: sequencer not paused hcntrl=0x%x\n",
	     hcntrl);
      value = 0;
   }

   return(value);
}


/*********************************************************************
*  Ph_EnableNextScbArray -
*
*  Enable asssociated valid entry in next scb array
*
*  Return Value:  
*                  
*  Parameters:    
*
*  Activation: Ph_IntSelto
*              Ph_IntFree
*
*  Remarks:       
*                 
*********************************************************************/
void Ph_EnableNextScbArray (sp_struct *scb_ptr)
{
   register AIC_7870 *base;
   UBYTE target;
   UBYTE value;

   base = scb_ptr->SP_ConfigPtr->CFP_Base;
   target = ((UBYTE) scb_ptr->SP_Tarlun) >> 4;
   if (!((value = INBYTE(AIC7870[SCRATCH_NEXT_SCB_ARRAY+target])) & 0x80))
      if (value != 0x7f)
         OUTBYTE(AIC7870[SCRATCH_NEXT_SCB_ARRAY+target], value | 0x80);
}

/*********************************************************************
*  
*  Ph_ClearFast20Reg -
*  
*  Initializing control register that related to Fast20 to non-fast20
*  mode.
*
*  Return Value:  void
*             
*  Parameters: config_ptr
*              scb_ptr     - scb of the target ID will be extracted
*
*  Activation: Ph_Negotiate, Ph_ExtMsgi
*
*  Remarks:
*                 
*********************************************************************/
void Ph_ClearFast20Reg (cfp_struct *config_ptr, sp_struct *scb_ptr)
{
   register AIC_7870 *base;
   UBYTE target_id, id_mask, fast20map, reg_value;

   if (scb_ptr->Sp_ExtMsg[2] != MSGSYNC)
      return;

   base = config_ptr->CFP_Base;
   target_id = (UBYTE)(((UBYTE)scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;

   if (target_id <= 7)
   {
      id_mask = (0x01) << target_id;
      fast20map = INBYTE(AIC7870[FAST20_LOW]);
      fast20map &= ~id_mask;     /* clear the fast20 device map */
      OUTBYTE(AIC7870[FAST20_LOW], fast20map);
   }
   else
   {
      id_mask = (0x01) << (target_id-8);
      fast20map = INBYTE(AIC7870[FAST20_HIGH]);
      fast20map &= ~id_mask;     /* clear the fast20 device map */
      OUTBYTE(AIC7870[FAST20_HIGH], fast20map);
   }
   reg_value = INBYTE(AIC7870[SXFRCTL0]) & ~FAST20;
   OUTBYTE(AIC7870[SXFRCTL0], reg_value);
}

/*********************************************************************
*  
*  Ph_LogFast20Map -
*  
*  Log into scratch RAM locations the fast20 device map
*
*  Return Value:  void
*             
*  Parameters: config_ptr
*              scb_ptr     - scb of the target ID will be extracted
*
*  Activation: Ph_ExtMsgi
*
*  Remarks:    This routine assumes the two internal registers of the
*              sequencer were set to zero upon power on/reset.
*                 
*********************************************************************/
void Ph_LogFast20Map (cfp_struct *config_ptr, sp_struct *scb_ptr)
{
   register AIC_7870 *base;
   UBYTE fast20map, target_id, id_mask, reg_value;

   if (scb_ptr->Sp_ExtMsg[2] != MSGSYNC)
      return;

   base = config_ptr->CFP_Base;
   target_id = (UBYTE)(((UBYTE)scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;

   if (target_id <= 7)
   {
      id_mask = (0x01) << target_id;
      fast20map = INBYTE(AIC7870[FAST20_LOW]);

      /* @YANG1 */
      if (scb_ptr->Sp_ExtMsg[3] < FAST20_THRESHOLD)
      {
         fast20map |= id_mask;      /* set the fast20 device map */
         reg_value = INBYTE(AIC7870[SXFRCTL0]) | FAST20;
      }
      else
      {
         fast20map &= ~id_mask;     /* clear the fast20 device map */
         reg_value = INBYTE(AIC7870[SXFRCTL0]) & ~FAST20;
      }
      OUTBYTE(AIC7870[FAST20_LOW], fast20map);
   }
   else
   {
      id_mask = (0x01) << (target_id-8);
      fast20map = INBYTE(AIC7870[FAST20_HIGH]);

      /* @YANG1 */
      if (scb_ptr->Sp_ExtMsg[3] < FAST20_THRESHOLD)
      {
         fast20map |= id_mask;      /* set the fast20 device map */
         reg_value = INBYTE(AIC7870[SXFRCTL0]) | FAST20;
      }
      else
      {
         fast20map &= ~id_mask;     /* clear the fast20 device map */
         reg_value = INBYTE(AIC7870[SXFRCTL0]) & ~FAST20;
      }
      OUTBYTE(AIC7870[FAST20_HIGH], fast20map);
   }

   OUTBYTE(AIC7870[SXFRCTL0], reg_value);

   if (verbose_nego)
	   printf("Ph_LogFast20Map: map 0x%x reg_value 0x%x\n", fast20map,
		  reg_value);
   return;
}

#endif /* SIMHIM */
