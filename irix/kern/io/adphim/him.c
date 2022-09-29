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

/*
 * $Id: him.c,v 1.13 1999/08/19 03:07:45 gwh Exp $
 */

#include "sys/param.h"
#include "sys/sbd.h"
#include "him_scb.h"
#include "him_equ.h"
#include "seq_01.h"
#include "sys/cmn_err.h"
#include "sys/PCI/pciio.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/ddi.h"
#include "sys/scsi.h"
#include "sys/adp78.h"

int verbose_nego=0;

extern int adp_verbose;
extern u_char adp78_print_over_under_run;
extern void adp_reset(void *, int, int);

/***************************************************************************
 *
 * Various initialization.
 * Returns 0 if OK, -1 if error during initialization.
 *
 **************************************************************************/
UWORD
PH_init_him (cfp_struct *config_ptr)
{
   AIC_7870 *base;
   int adp_fifo_threshold = 0x2;
   UBYTE sblkctl, le_index, conv_index;

   base = config_ptr->CFP_Base;

   PH_Pause(base);

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

   Ph_ResetChannel(config_ptr);

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

   PH_WriteHcntrl(base, (UBYTE) (INTEN));

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
*  Process case where other device resets scsi bus
*                  
**********************************************************************/
void
Ph_ext_scsi_bus_reset (cfp_struct *config_ptr)
{
   AIC_7870 *base;
   int count=0;
   UBYTE sstat1;

   cmn_err(CE_ALERT, "SCSI controller %d detected bus reset by external device.",
	   config_ptr->Cf_DeviceNumber - 1);

   base = config_ptr->CFP_Base;

   /*
    * Wait if reset is asserted and bus is not free and we have not
    * been waiting for more than 100ms
    */
   sstat1 = INBYTE(AIC7870[SSTAT1]);
   while ((sstat1 & SCSIRSTI) &&
	  ((sstat1 & BUSFREE) == 0) &&
	  (count < 100)) {
	   OUTBYTE(AIC7870[CLRSINT1], CLRSCSIRSTI);
	   us_delay(1000);
	   count++;
	   sstat1 = INBYTE(AIC7870[SSTAT1]);
   }

   OUTBYTE(AIC7870[SCSISEQ], 0);        /* Disarm any outstanding selections */

   /*
    * Now we have to reset, just to get our bookkeeping right.
    */
   adp_reset(config_ptr->Cf_Adapter, 0, 1);
}


/*********************************************************************
*
*  Reset SCSI bus and clear all associated interrupts.
*  Also clear synchronous / wide mode.
*
*********************************************************************/
void Ph_ResetChannel (cfp_struct *config_ptr)
{
   AIC_7870 *base;
   UBYTE i, j, sxfrctl1;

   base = config_ptr->CFP_Base;

   OUTBYTE(AIC7870[SCSISEQ], 00);
   OUTBYTE(AIC7870[CLRSINT0], 0xff);
   OUTBYTE(AIC7870[CLRSINT1], 0xff); /* pvs 5/15/94 */

   OUTBYTE(AIC7870[SXFRCTL0], CLRSTCNT|CLRCHN);

   sxfrctl1 = (ENSTIMER | ACTNEGEN);
   if (config_ptr->CFP_ScsiParity)
	   sxfrctl1 |= ENSPCHK;
   if (config_ptr->CFP_TerminationLow)
	   sxfrctl1 |= STPWEN;
   OUTBYTE(AIC7870[SXFRCTL1], sxfrctl1);

   OUTBYTE(AIC7870[DFCNTRL], FIFORESET);
   OUTBYTE(AIC7870[SIMODE1], ENSCSIPERR + ENSELTIMO + ENSCSIRST);

   /*
    * Re-Initialize sync/wide negotiation parameters
    */
   for (i = 0; i < config_ptr->Cf_MaxTargets; i++)
     if (config_ptr->Cf_ScsiOption[i].byte & SOME_NEGO)
       PH_SetNeedNego(base, i, config_ptr->Cf_ScsiOption[i].byte);

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
* underrun/overrun condition occured during data transfer
*                  
* The host often issues a request sense or mode sense and gives a
* large buffer, and if the target responds with some info that is
* not enough to fill the buffer, the sequencer will signal an
* underrun.  Deal with those without printing scary messages to
* the user.
*********************************************************************/
void
Ph_CheckLength (sp_struct *scb_ptr, register AIC_7870 *base)
{
   union gen_union res_lng;
   UBYTE phase;
   int i;

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

   scb_ptr->SP_HaStat = HOST_DU_DO;
}


/*********************************************************************
*
* reset the HIM
*                  
*********************************************************************/
void
PH_reset_him(cfp_struct *cfp)
{
	AIC_7870 *base = cfp->Cf_base_addr;

	Ph_ResetChannel(cfp);
	Ph_set_scratchmem(cfp);

	/* Now, Restart Sequencer */
	OUTBYTE(AIC7870[SEQADDR0], IDLE_LOOP_ENTRY >> 2);
	OUTBYTE(AIC7870[SEQADDR1], IDLE_LOOP_ENTRY >> 10);
}


/*********************************************************************
 * 
 * Abort all queued commands in the sequencer.
 * Call this before asserting reset on the SCSI bus.
 * 
 *********************************************************************/
void
PH_abort_seq_cmds(cfp_struct *cfp)
{
	AIC_7870 *base = cfp->Cf_base_addr;
	hsp_struct *ha_ptr = cfp->CFP_HaDataPtr;

	sp_struct *scb_ptr;
	int i;
	UBYTE qincnt, qinfifo, scbsave, scb_num;
	UBYTE scb13;

	/*
	 * Pull out any queued (but not executing) cmds from
	 * the qinfifo.
	 * XXX probably need to abort whatever cmd is next
	 * (see Optimaabortactive).
	 */
	qincnt = INBYTE(AIC7870[QINCNT]);
	if (qincnt != 0) {
		i = qincnt;
		while (i > 0) {
			qinfifo = INBYTE(AIC7870[QINFIFO]);
			i--;
		}
		qincnt = INBYTE(AIC7870[QINCNT]);
	}

#ifdef nowork
	/*
	 * Abort the active SCB by setting various abort
	 * bits and requeuing the command.
	 */
	scb_num = INBYTE(AIC7870[ACTIVE_SCB]);
	if (scb_num != INVALID_SCB_INDEX) {
		scbsave = INBYTE(AIC7870[SCBPTR]);
		OUTBYTE(AIC7870[SCBPTR], scb_num);

		scb13 = INBYTE(AIC7870[SCB13]) | SCB_ABORTED; 
		OUTBYTE(AIC7870[SCB13], scb13);
		OUTBYTE(AIC7870[SCBCNT], 00);
		OUTBYTE(AIC7870[SCBPTR], scbsave);

		scb_ptr = ACTPTR[scb_num];
		if (scb_ptr != (sp_struct *) 0) {
			scb_ptr->SP_MgrStat = SCB_ABORTED;
			scb_ptr->Sp_seq_scb.seq_scb_struct.Aborted = 1;
			scb_ptr->Sp_seq_scb.seq_scb_struct.SpecFunc = SPEC_BIT3SET;
			scb_ptr->SP_SegCnt = SPEC_OPCODE_00;
			if (scb_ptr->Sp_seq_scb.seq_scb_struct.TagEnable)
				scb_ptr->SP_SegPtr = MSG0D;	/* "Abort Tag" for tagged  */
			else
				scb_ptr->SP_SegPtr = MSG06;	/* "Abort" for non-tagged  */

			/* Insert at Head of Queue only if we are not in the process   */      
			/* of selecting the target. If the selection is in progress    */
			/* a interrupt to the HIM is guaranteed. Either the result of  */
			/* command execution or selection time out will hapen */ 
			if (!((INBYTE(AIC7870[SCSISEQ]) & ENSELO) &&
			      (INBYTE(AIC7870[WAITING_SCB]) == scb_num)))
				Ph_OptimaEnqueHead(scb_num, scb_ptr, base);
		}
	}
#endif
}


/*********************************************************************
*
* assert/dessert RST on the SCSI bus
* The SCSI spec says that we must hold the reset signal for at least 64ms.
* So this function is called twice.  Once to assert the signal,
* and 64 (actually 70ms) later to deassert it.
*                  
*********************************************************************/
void
PH_reset_scsi_bus(cfp_struct *cfp, int assertrst)
{
    AIC_7870 *base = cfp->Cf_base_addr;

    if (assertrst) {

	    cfp->Cf_scb16 = INBYTE(AIC7870[SCB16]);	/* save SCB16 and SCB17 */
	    cfp->Cf_scb17 = INBYTE(AIC7870[SCB17]);
	    cfp->Cf_simode0 = INBYTE(AIC7870[SIMODE0]); /* Disable all SCSI */
	    cfp->Cf_simode1 = INBYTE(AIC7870[SIMODE1]); /* to avoid confusing */
	    OUTBYTE(AIC7870[SIMODE0],0);
	    OUTBYTE(AIC7870[SIMODE1],0); /* clear interrupt */
	    OUTBYTE(AIC7870[CLRINT], (CLRSEQINT|CLRCMDINT|CLRSCSINT|CLRBRKINT));
	    OUTBYTE(AIC7870[SCSISEQ], SCSIRSTO); /* assert RST */

    } else {
	    OUTBYTE(AIC7870[SCSISEQ], 00);
	    OUTBYTE(AIC7870[CLRSINT1], CLRSCSIRSTI);
	    OUTBYTE(AIC7870[CLRINT], (CLRSEQINT|CLRCMDINT|CLRSCSINT|CLRBRKINT));
	    OUTBYTE(AIC7870[SIMODE0], cfp->Cf_simode0);
	    OUTBYTE(AIC7870[SIMODE1], cfp->Cf_simode1);
	    OUTBYTE(AIC7870[SCB16], cfp->Cf_scb16);
	    OUTBYTE(AIC7870[SCB17], cfp->Cf_scb17);
    }
}


/*********************************************************************
*
*  Terminate SCSI command sequence because sequence that is illegal,
*  or if we just can't handle it.
*  Return 1 if reset has occured.  0 otherwise.
*
*********************************************************************/
int
Ph_BadSeq (cfp_struct *config_ptr, AIC_7870 *base, int why)
{

   /*
    * Don't reset/abort if there is a SCSI bus free interrupt pending.
    */
   if ((PH_ReadIntstat(base) & SCSIINT) &&
       (INBYTE(AIC7870[SSTAT1]) & BUSFREE)) {
	   cmn_err(CE_ALERT, "SCSI controller %d detected unexpected bus free.",
		   config_ptr->Cf_DeviceNumber - 1);

	   /* Now, Restart Sequencer */
	   OUTBYTE(AIC7870[SEQADDR0], (UBYTE) IDLE_LOOP_ENTRY >> 2);
	   OUTBYTE(AIC7870[SEQADDR1], (UBYTE) IDLE_LOOP_ENTRY >> 10);

	   return 0;

   } else {

	   cmn_err(CE_ALERT, "SCSI controller %d detected internal error. code 0x%x",
		  config_ptr->Cf_DeviceNumber - 1, why);
	   /*
	    * Bus resets must be synchronized with the upper layer.
	    */
	   adp_reset(config_ptr->Cf_Adapter, 0, 1);

	   return 1;
   }
}


/*********************************************************************
*
* Sequencer has detected a non-zero STATUS phase on the SCSI bus
* Sequencer is paused before this is called.  Caller will unpause.
*
*********************************************************************/
void
Ph_CheckCondition (sp_struct *scb_ptr, AIC_7870 *base, UBYTE scb_num)
{
	UBYTE status;

	if (scb_ptr->SP_TargStat == UNIT_CHECK) {
		/*
		 * A check condition on a request sense ?
		 */
		scb_ptr->SP_HaStat = HOST_SNS_FAIL;
		status = 0;
	} else {
		status = INBYTE(AIC7870[PASS_TO_DRIVER]);
		scb_ptr->SP_TargStat = status;
	}

	/*
	 * Hurry and ACK the command complete.
	 * Doing this as the first thing in the function prevents
	 * system from bootup.
	 */

	OUTBYTE(AIC7870[SXFRCTL0], INBYTE(AIC7870[SXFRCTL0]) | CLRCHN);
	OUTBYTE(AIC7870[SIMODE1], INBYTE(AIC7870[SIMODE1]) & ~(ENBUSFREE | ENSCSIRST));
	Ph_RdByteX(base);
	OUTBYTE(AIC7870[SCSISIG], 0); /* no expected phase next */

	if ((status == UNIT_CHECK) && (scb_ptr->SP_AutoSense)) {
		scb_ptr->SP_NegoInProg = 0;
		Ph_OptimaRequestSense(scb_ptr,scb_num);
	} else {
		/* clear target busy must be done here also */
		Ph_OptimaClearTargetBusy(scb_ptr->SP_ConfigPtr,scb_num);
		Ph_TerminateCommand(scb_ptr, scb_num);
	}

	OUTBYTE(AIC7870[SEQADDR0], (UBYTE) IDLE_LOOP_ENTRY >> 2);
	OUTBYTE(AIC7870[SEQADDR1], (UBYTE) IDLE_LOOP_ENTRY >> 10);
}

/*********************************************************************
*
*  Read the scsi bus, but only to advance to next op. Data is don't care
*
*********************************************************************/
void
Ph_RdByteX (AIC_7870 *base)
{
  UBYTE scsisig;
  int i;

  scsisig = INBYTE(AIC7870[SCSISIG]);
  OUTBYTE(AIC7870[SCSISIG], scsisig | ACKO);
  i = lbolt;
  while(INBYTE(AIC7870[SCSISIG]) & REQI)  /* wait for REQ to go away */
    {
      if (lbolt - i > 10) /* if not gone in 100ms don't hang */
	  break;
    }
  OUTBYTE(AIC7870[SCSISIG], scsisig);     /* remove ACK */
}

/*********************************************************************
*
*  Read the scsi bus
*      drive ack
*      wait for Req to go inactive
*      turn off ack
*
*********************************************************************/
UBYTE
Ph_RdByte (AIC_7870 *base)
{
  UBYTE scsisig, scsidatal;
  int i;

  scsisig = INBYTE(AIC7870[SCSISIG]);
  scsidatal = INBYTE(AIC7870[SCSIBUSL]);
  OUTBYTE(AIC7870[SCSISIG], scsisig | ACKO);
  i = lbolt;
  while(INBYTE(AIC7870[SCSISIG]) & REQI)  /* wait for REQ to go away */
    {
      if (lbolt - i > 10) /* if not gone in 100ms don't hang */
	  break;
    }
  OUTBYTE(AIC7870[SCSISIG], scsisig);     /* remove ACK */
  return (scsidatal);
}

/*********************************************************************
*
*  Abort current target
*
* This thing may call Ph_BadSeq, which resets the scsi bus and the sequencer.
* So call this only as last resort.
*
*********************************************************************/
int
Ph_TargetAbort (cfp_struct *config_ptr, sp_struct *scb_ptr, AIC_7870 *base)
{
   UBYTE abt_msg = MSG06, i;
   UBYTE scb_index;

   if ((PH_ReadIntstat(base) & INTCODE) == NO_ID_MSG) {
      OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
      Ph_RdByteX(base);
   }

   if (scb_ptr == NULL)
	   return (Ph_BadSeq(config_ptr, base, BADSEQ_TARGABORT_NULLSCB));

   /*
    * scb_ptr is valid at this point.
    */

   if (scb_ptr->SP_MgrStat == SCB_ABORTED)
	   return 0;

   if (Ph_Wt4Req(scb_ptr, base) == MOPHASE) {
	   scb_index = INBYTE(AIC7870[ACTIVE_SCB]);

	   if (INBYTE(AIC7870[SCB01]) & TAG_ENABLE)
		   abt_msg = MSG0D;

	   if (Ph_SendTrmMsg(scb_ptr->SP_ConfigPtr, abt_msg) & BUSFREE) {
		   OUTBYTE(AIC7870[CLRSINT1], CLRBUSFREE);
		   OUTBYTE(AIC7870[CLRINT], CLRSCSINT);
		   i = INBYTE(AIC7870[SCBPTR]);
		   if (config_ptr->CFP_HaDataPtr->hsp_active_ptr[i] != (sp_struct *) 0) {
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

   /*
    * We are going to reset the bus now, so mark this scb 
    */
   scb_ptr->SP_MgrStat = SCB_ABORTED;
   return (Ph_BadSeq(config_ptr, base, BADSEQ_TARGABORT));
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
UBYTE Ph_SendTrmMsg ( cfp_struct *config_ptr,
                         UBYTE term_msg)
{
   return(0);
}




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
            Ph_RdByteX(base);
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
      if (scb_ptr->Sp_ExtMsg[0] == 0x06 || scb_ptr->Sp_ExtMsg[0] == 0x0d)
	OUTBYTE(AIC7870[SCSIDATL], scb_ptr->Sp_ExtMsg[0]);
      else
	OUTBYTE(AIC7870[SCSIDATL], MSG05);
   }
   else
      OUTBYTE(AIC7870[SCSIDATL], MSG08);
   while (INBYTE(AIC7870[SCSISIG]) & ACKI);
   return;
}


/*********************************************************************
*
* Setup Sequencer XFER_OPTION with NEEDNEGO (Need to Negotiate)
* if the target is enabled to do wide and/or sync transfers.
*
*********************************************************************/
void
PH_SetNeedNego (AIC_7870 *base, UBYTE targ, UBYTE option)
{
   UBYTE xfer_option;

   if (verbose_nego)
	   printf("PH_SetNeedNego: target %d option 0x%x\n",
		  targ, option);

   xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + targ;
   xfer_option = OSM_CONV_BYTEPTR(xfer_option);

   OUTBYTE(AIC7870[xfer_option], NEEDNEGO);
}      


/*********************************************************************
*
*  Initiate synchronous and/or wide negotiation
*
*********************************************************************/
void
Ph_Negotiate (sp_struct *scb_ptr, AIC_7870 *base)
{
   cfp_struct *config_ptr;
   UBYTE i, xfer_option;

   config_ptr = scb_ptr->SP_ConfigPtr;
   i = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;

   if (verbose_nego)
	   printf("\nPh_Negotiate: PCI %d targ %d Cf_ScsiOption 0x%x\n",
		  config_ptr->Cf_DeviceNumber, i,
		  config_ptr->Cf_ScsiOption[i].byte);


   OUTBYTE(AIC7870[SEQADDR0], (UBYTE) SIOSTR3_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], (UBYTE) SIOSTR3_ENTRY >> 10);

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
    * see if we are supposed to send an abort message to the target.
    */
   if (config_ptr->Cf_ScsiOption[i].u.AbortMsg)
     {
       config_ptr->Cf_ScsiOption[i].u.AbortMsg = 0;
       if (scb_ptr->SP_TagEnable)
	 {
	   scb_ptr->Sp_ExtMsg[0] = MSG0D;  /* "Abort Tag" for tagged  */
	 }
       else
	 {
	   scb_ptr->Sp_ExtMsg[0] = MSG06;  /* "Abort" for non-tagged  */
	 }
       Ph_SendMsgo(scb_ptr, base);
     }

   /*
    * If this is the first time through the negotiation sequence, then
    * NEEDNEGO will be set.  Fill in the ExtMsg array and send it out.
    * In Ph_ExtMsgi, the next message will be put into the ExtMsg array.
    * When this function is called again, just call Ph_SendMsgo to send out
    * the second message.
    */
   else if (INBYTE(AIC7870[xfer_option]) == NEEDNEGO) {
     
      OUTBYTE(AIC7870[xfer_option], 00);
      OUTBYTE(AIC7870[SCSIRATE], 00);
      Ph_ClearFast20Reg(config_ptr, scb_ptr);

      scb_ptr->Sp_ExtMsg[0] = MSG01;

      if (config_ptr->Cf_ScsiOption[i].u.WideMode |
	  config_ptr->Cf_ScsiOption[i].u.NarrowFrc) {
         scb_ptr->Sp_ExtMsg[1] = 2;	/* wide msg len */
         scb_ptr->Sp_ExtMsg[2] = MSGWIDE;
         if (config_ptr->Cf_ScsiOption[i].u.NarrowFrc)
	   {
	     scb_ptr->Sp_ExtMsg[3] = 0;
	     config_ptr->Cf_ScsiOption[i].u.NarrowFrc = 0;
	   }
	 else
	   scb_ptr->Sp_ExtMsg[3] = WIDE_WIDTH;
	 Ph_WideNego(scb_ptr, base);
      }

      else {   /* not wide mode so do sync or async negotiation */
	scb_ptr->Sp_ExtMsg[1] = 3;
	scb_ptr->Sp_ExtMsg[2] = MSGSYNC;

	if (config_ptr->CFP_EnableFast20) {
	  scb_ptr->Sp_ExtMsg[3] = (config_ptr->Cf_ScsiOption[i].u.SyncRate * 4) + 12;
	} else {
	  scb_ptr->Sp_ExtMsg[3] = (config_ptr->Cf_ScsiOption[i].u.SyncRate * 6) + 25;
	  if (config_ptr->Cf_ScsiOption[i].u.SyncRate >= 4)
	    {
	      ++scb_ptr->Sp_ExtMsg[3];
	    }
	}

	if (config_ptr->Cf_ScsiOption[i].u.AsyncFrc ||
	    !config_ptr->Cf_ScsiOption[i].u.SyncMode) /* force async */
	  {
	    scb_ptr->Sp_ExtMsg[4] = 0;
	    config_ptr->Cf_ScsiOption[i].u.AsyncFrc = 0;
	  }
	else                                           /* must be SYNC xfer */
	  {
	    if (config_ptr->Cf_ScsiOption[i].u.WideMode)
	      scb_ptr->Sp_ExtMsg[4] = WIDE_OFFSET;     /* sync + wide */
	    else 
	      scb_ptr->Sp_ExtMsg[4] = NARROW_OFFSET;   /* sync + narrow */
	  }
	Ph_SyncNego(scb_ptr, base);
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


/*
 * Make the transfer rate for this device asynchronous.
 */
void
ph_set_async(sp_struct *scb_ptr, AIC_7870 *base)
{
	cfp_struct *config_ptr;
	UBYTE j, xfer_option, scsi_rate;

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

	scb_ptr->Sp_SyncInProg = 0;
}


/************************************************************************
 * Send the sync negotiation message out by calling Ph_ExtMsgo
 *
 ************************************************************************/

void
Ph_SyncNego (sp_struct *scb_ptr, AIC_7870 *base)
{
	UBYTE msg;

	if (verbose_nego)
		printf("Ph_SyncNego: sending out sync message\n");

	scb_ptr->Sp_SyncInProg = 1;

	/*
	 * This target is not handling sync negotiation well.
	 * End it and move on.
	 */
	if (Ph_ExtMsgo(scb_ptr, base) != MIPHASE) {
		if (verbose_nego)
			printf("Ph_SyncNego: ext msg out did not end in MI.  ");

		ph_set_async(scb_ptr, base);
		return;
	}

	msg = INBYTE(AIC7870[SCSIBUSL]);
	switch (msg) {
	case MSG01:
		/* device is responding to our message */
		scb_ptr->SP_NegoInProg = 1;
		break;

	case MSG07:
		if (verbose_nego)
			printf("Ph_SyncNego: rejected; negoInProg 0x%x\n",
			       scb_ptr->SP_NegoInProg);

		while (1) {	/* Process any number of Message Rejects */

			OUTBYTE(AIC7870[SCSISIG], MIPHASE);
			Ph_RdByteX(base);
			if (Ph_Wt4Req(scb_ptr, base) != MIPHASE)
				break;

			if (INBYTE(AIC7870[SCSIBUSL]) != MSG07)
				break;
		}
		ph_set_async(scb_ptr, base);
		break;

	default:
		if (verbose_nego)
			printf("Ph_SyncNego: unknown msg 0x%x\n", msg);

		ph_set_async(scb_ptr, base);
		break;
	}
}


/* 
 * Make the transfer width narrow (actually, don't set WIDEXFER)
 * If the target needs to do sync negotiation, go do that.
 */
void 
ph_set_narrow(sp_struct *scb_ptr, AIC_7870 *base)
{
	cfp_struct *config_ptr;
	UBYTE i;

	scb_ptr->Sp_WideInProg = 0;

	config_ptr = scb_ptr->SP_ConfigPtr;
	i = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;

	if (config_ptr->Cf_ScsiOption[i].u.SyncMode |
	    config_ptr->Cf_ScsiOption[i].u.AsyncFrc) {
		OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
		Ph_RdByteX(base);
		if (Ph_Wt4Req(scb_ptr, base) == MOPHASE) {
			OUTBYTE(AIC7870[SCSISIG], MOPHASE | ATNO);
			scb_ptr->Sp_ExtMsg[1] = 3;
			scb_ptr->Sp_ExtMsg[2] = MSGSYNC;
			if (config_ptr->CFP_EnableFast20) {
				scb_ptr->Sp_ExtMsg[3] = (config_ptr->Cf_ScsiOption[i].u.SyncRate * 4) + 12;
			} else {
				scb_ptr->Sp_ExtMsg[3] = (config_ptr->Cf_ScsiOption[i].u.SyncRate * 6) + 25;
				if (config_ptr->Cf_ScsiOption[i].u.SyncRate >= 4)
					++scb_ptr->Sp_ExtMsg[3];
			}
			if (config_ptr->Cf_ScsiOption[i].u.AsyncFrc) /* force async */
			  {
			    scb_ptr->Sp_ExtMsg[4] = 0;
			    config_ptr->Cf_ScsiOption[i].u.AsyncFrc = 0;
			  }
			else                                           /* must be SYNC xfer */
			  {
			    scb_ptr->Sp_ExtMsg[4] = NARROW_OFFSET;   /* sync + narrow */
			  }
			Ph_SyncNego(scb_ptr, base);
		}

	} else {
		OUTBYTE(AIC7870[SCSISIG], MIPHASE);
		Ph_RdByteX(base);
	}
}


/************************************************************************
 * Send the wide negotiation message out by calling Ph_ExtMsgo.
 *
 * If the target responds to the negotiation, set NegoInProg and let
 * Ph_ExtMsgi handle the response.
 * If the message is rejected, then record that fact; and if sync
 * negotiation is also requested, set up the ExtMsg array with the sync
 * negotiation message and start that.
 *
 ************************************************************************/
void
Ph_WideNego(sp_struct *scb_ptr, AIC_7870 *base)
{
	UBYTE j, msg;

	if (verbose_nego)
		printf("Ph_WideNego: sending out wide message\n");

	scb_ptr->Sp_WideInProg = 1;

	/*
	 * This target is not handling the wide negotiation well.
	 * End it and move on.
	 */
	if ((j = Ph_ExtMsgo(scb_ptr, base)) != MIPHASE) {
		if (verbose_nego)
			printf("Ph_WideNego: ph_extmsgo did not end in miphase, phase = 0x%x\n", j);

		ph_set_narrow(scb_ptr, base);
		return;
	}

	msg = INBYTE(AIC7870[SCSIBUSL]);
	switch(msg) {
	case MSG01:
		/* device is responding to our message */
		scb_ptr->SP_NegoInProg = 1;
		break;

	case MSG07:
		if (verbose_nego)
			printf("Ph_WideNego: wide negotiation rejected; negoinprog 0x%x\n",
			       scb_ptr->SP_NegoInProg);
		ph_set_narrow(scb_ptr, base);
		break;
		
	default:
		if (verbose_nego)
			printf("Ph_WideNego: unknown msg 0x%x\n", msg);
		ph_set_narrow(scb_ptr, base);
		break;
	}
}


/*********************************************************************
*
*  Receive and interpret extended message in
*
*********************************************************************/
void
Ph_ExtMsgi (sp_struct *scb_ptr, AIC_7870 *base)
{
   cfp_struct *config_ptr;
   int sync_tr;
   UBYTE c, i, j, max_rate, phase, scsi_rate, targ;
   UBYTE nego_flag = NONEGO;
   UBYTE max_offset = NARROW_OFFSET;
   UBYTE xfer_option, scsisig, scsidatl;
   int badin=0;
   adp78_adap_t *ad_p;

   config_ptr = scb_ptr->SP_ConfigPtr;

   j = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;
   targ = j;

   c = INBYTE(AIC7870[PASS_TO_DRIVER]);

   /*
    * Read the extented message in from the bus and
    * put it into scb_ptr->Sp_ExtMsg[]
    */
   scb_ptr->Sp_ExtMsg[0] = MSG01;
   scb_ptr->Sp_ExtMsg[1] = c--;
   scb_ptr->Sp_ExtMsg[2] = INBYTE(AIC7870[SCSIBUSL]);

#ifdef DEBUG
   if (scb_ptr->Sp_ExtMsg[2] == MSGWIDE) {
	   if (scb_ptr->Sp_ExtMsg[1] != 2) {
		   printf("bad length for wide msg in %d, resetting\n",
			  scb_ptr->Sp_ExtMsg[1]);
		   scb_ptr->Sp_ExtMsg[1] = 2;
		   c = 1;
		   badin = 1;
	   }
   } else if (scb_ptr->Sp_ExtMsg[2] == MSGSYNC) {
	   if (scb_ptr->Sp_ExtMsg[1] != 3) {
		   printf("bad length for sync msg in %d, resetting\n",
			  scb_ptr->Sp_ExtMsg[1]);
		   scb_ptr->Sp_ExtMsg[1] = 3;
		   c = 2;
		   badin = 1;
	   }
   }
#endif

   /* Read in the rest of the bytes.. "c" is number of bytes left, "i"
    *   is byte # of extended message.
    * if after reading byte and moving to next transfer we are not in
    *   message in report an errror.
    * if we are in message out instead then report error and return
    *   MSG09 as message out (parity error). If Negotiation was in
    *   progress reset to "NEEDNEGO" state.
    */

   for (i = 3; c > 0; --c) 
   {
      scsidatl = Ph_RdByte(base);
      if ((phase = Ph_Wt4Req(scb_ptr, base)) != MIPHASE)
      {
         /*
	  * Yikes, we are not in msg in phase but
	  * we still need to read more msg in.
	  */
	 if (phase == ERR) {
		 /*
		  * wt4req detected a bus free condition.
		  * renegotiate on this target and return.
		  */
		 PH_SetNeedNego(base, j, config_ptr->Cf_ScsiOption[j].byte);
		 scb_ptr->SP_NegoInProg = 0;
		 return;
	 }

	 scsisig = INBYTE(AIC7870[SCSISIG]);
         if ((scsisig & (BUSPHASE | ATNI)) == (MOPHASE | ATNI)) {
            if (scb_ptr->SP_NegoInProg)
            {
               OUTBYTE(AIC7870[SCSISIG], MOPHASE | ATNO);
               PH_SetNeedNego(base, j, config_ptr->Cf_ScsiOption[j].byte);
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

         } else {
            Ph_BadSeq(config_ptr, base, BADSEQ_EMSGI);
         }
         return;
      }

      if (i < 5) {
         scb_ptr->Sp_ExtMsg[i] = INBYTE(AIC7870[SCSIBUSL]);
	 i++;
      }
   }	/* end of for (i = 3; c > 0; --c) */


   scb_ptr->Sp_WideInProg = 0;
   scb_ptr->Sp_SyncInProg = 0;

   /* calculate the maximum transfer rate, either double speed, or non-double speed */
   i = (((UBYTE) scb_ptr->SP_Tarlun) & TARGET_ID) >> 4;
   targ = i;
   ad_p = config_ptr->Cf_Adapter;
   

   if (config_ptr->CFP_EnableFast20)
   {
      max_rate = (config_ptr->Cf_ScsiOption[i].u.SyncRate * 4) + 12;
   }
   else
   {
      max_rate = (config_ptr->Cf_ScsiOption[i].u.SyncRate * 6) + 25;
      if (config_ptr->Cf_ScsiOption[i].u.SyncRate >= 4)
         ++max_rate;
   }

   xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + i;
   xfer_option = OSM_CONV_BYTEPTR(xfer_option);

   if (verbose_nego || badin) {
	   int z;
	   printf("Ph_ExtMsgi: targ %d\n", i);
	   for (z=0; z < (int)scb_ptr->Sp_ExtMsg[1] + 2; z++)
		   printf("[%d] = 0x%x\n", z, scb_ptr->Sp_ExtMsg[z]);
   }


   /*
    * We've read in the message.  Now interpret it.
    */
   switch (scb_ptr->Sp_ExtMsg[2]) {
   case MSGWIDE:
      /*
       * Check the length.  If no good, fall through
       * to the default case.
       */
      if (scb_ptr->Sp_ExtMsg[1] == 2) {

         OUTBYTE(AIC7870[xfer_option], 00);
         OUTBYTE(AIC7870[SCSIRATE], 00);
	 ad_p->ad_negotiate[targ].params.period = 0;
	 
	 /* Harry's code has this, but it's a redundant call, since
	  * Ph_ClearFast20Reg just returns if ExtMsg[2] != MSGSYNC
	  *
	  * Ph_ClearFast20Reg(config_ptr, scb_ptr);
	  */
         if (scb_ptr->Sp_ExtMsg[3] > WIDE_WIDTH) {
            scb_ptr->Sp_ExtMsg[3] = WIDE_WIDTH;
            nego_flag = NEEDNEGO;
         }
	 ad_p->ad_negotiate[targ].params.width = scb_ptr->Sp_ExtMsg[3];

         if (!scb_ptr->SP_NegoInProg)
            break;

         scb_ptr->SP_NegoInProg = 0;

         if (nego_flag == NONEGO) {
	    /*
	     * the message is valid. See if the target can do wide.
	     */
            if (scb_ptr->Sp_ExtMsg[3] == WIDE_WIDTH) {
               OUTBYTE(AIC7870[xfer_option], WIDEXFER);
               OUTBYTE(AIC7870[SCSIRATE], WIDEXFER);
               max_offset = WIDE_OFFSET;
	       scb_ptr->Sp_dowide = 1;
            } else {
	       max_offset = NARROW_OFFSET;
            }

	    /* start to process the synchronous data transfer part */
            if (config_ptr->Cf_ScsiOption[i].u.SyncMode |
		config_ptr->Cf_ScsiOption[i].u.AsyncFrc) {
               scb_ptr->Sp_ExtMsg[1] = 3;
               scb_ptr->Sp_ExtMsg[2] = MSGSYNC;
               scb_ptr->Sp_ExtMsg[3] = max_rate;
	       if (config_ptr->Cf_ScsiOption[i].u.AsyncFrc) {
		 scb_ptr->Sp_ExtMsg[4] = 0;
		 config_ptr->Cf_ScsiOption[i].u.AsyncFrc = 0;
	       }
	       else
		 scb_ptr->Sp_ExtMsg[4] = max_offset;
               OUTBYTE(AIC7870[SCSISIG], ATNO | MIPHASE);
               scb_ptr->SP_NegoInProg = 1;
            }
            return;
         }

      } else {
	      /* do this to make sure the next
	       * case statement passes us through
	       * to the default case.
	       */
	      scb_ptr->Sp_ExtMsg[1] = 2;
      }


   case MSGSYNC:
      /*
       * make sure the length is correct, and if not,
       * fall through??
       */
      if (scb_ptr->Sp_ExtMsg[1] == 3) {
	 /*
	  * Check the results of the wide negotiation, which
	  * happens before the sync nego.
	  */
         scsi_rate = INBYTE(AIC7870[xfer_option]);
	 if (verbose_nego)
		 printf("good sync msg: scsi_rate 0x%x dowide %d\n",
			scsi_rate, scb_ptr->Sp_dowide);
	 if ((scsi_rate == NEEDNEGO) ||
	     ((scsi_rate & WIDEXFER) == 0) ||
	     (scb_ptr->Sp_dowide == 0)) {
		 scsi_rate = 0;
		 max_offset = NARROW_OFFSET;
         } else {
		 scsi_rate = WIDEXFER;
		 max_offset = WIDE_OFFSET;
         }

         OUTBYTE(AIC7870[xfer_option], scsi_rate);
         OUTBYTE(AIC7870[SCSIRATE], scsi_rate);

	 nego_flag = NONEGO;
         if (scb_ptr->Sp_ExtMsg[4]) {
		 /* target wants sync transfers */
		 sync_tr = 1;

                 ad_p->ad_negotiate[targ].params.offset = scb_ptr->Sp_ExtMsg[4];
                 ad_p->ad_negotiate[targ].params.period = scb_ptr->Sp_ExtMsg[3];
		 if (scb_ptr->Sp_ExtMsg[4] > max_offset) {
			 scb_ptr->Sp_ExtMsg[4] = max_offset;
                         ad_p->ad_negotiate[targ].params.offset = max_offset;
			 nego_flag = NEEDNEGO;
		 } else if (scb_ptr->Sp_ExtMsg[3] < max_rate) {
			 scb_ptr->Sp_ExtMsg[3] = max_rate;
                         ad_p->ad_negotiate[targ].params.period = max_rate;
			 nego_flag = NEEDNEGO;
		 } else if (scb_ptr->Sp_ExtMsg[3] > 68) {
			 scb_ptr->Sp_ExtMsg[4] = 0;
                         ad_p->ad_negotiate[targ].params.offset = 0;
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
                 ad_p->ad_negotiate[targ].params.offset = 0;
                 ad_p->ad_negotiate[targ].params.period = 0;
		 scb_ptr->Sp_ExtMsg[3] = FAST20_THRESHOLD;
		 scsi_rate &= WIDEXFER;
	 }

         if (!scb_ptr->SP_NegoInProg)		/* tgt initiated nego */
            break;

         scb_ptr->SP_NegoInProg = 0;
         if (nego_flag == NONEGO) {
		 if (sync_tr)
			 scsi_rate += Ph_SyncSet(scb_ptr) + scb_ptr->Sp_ExtMsg[4];

		 OUTBYTE(AIC7870[xfer_option], scsi_rate);
		 OUTBYTE(AIC7870[SCSIRATE], scsi_rate);
		 Ph_LogFast20Map(config_ptr, scb_ptr);  /* record in Scratch RAM */

		 if (verbose_nego)
			 printf("Ph_ExtMsgi: done with targ %d, scsi_rate 0x%x\n",
				i, scsi_rate);
		 return;
         }
      }

      /* intentional fall through */

   default:
      OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
      Ph_RdByteX(base);
      if ((phase = Ph_Wt4Req(scb_ptr, base)) == MOPHASE)
      {
         OUTBYTE(AIC7870[SCSISIG], MOPHASE);
         OUTBYTE(AIC7870[CLRSINT1], CLRATNO);
         OUTBYTE(AIC7870[SXFRCTL0], INBYTE(AIC7870[SXFRCTL0]) & ~SPIOEN);
         OUTBYTE(AIC7870[SCSIDATL], MSG07);
         OUTBYTE(AIC7870[SXFRCTL0], INBYTE(AIC7870[SXFRCTL0]) | SPIOEN);
      }
      else
      {
         Ph_BadSeq(config_ptr, base, BADSEQ_EMSGI);
      }
      return;
   }

   scb_ptr->Sp_ExtMsg[0] = 0xff;
   OUTBYTE(AIC7870[SCSISIG], ATNO | MIPHASE);
   scb_ptr->SP_NegoInProg = 1;
}


/*********************************************************************
*
*  Send extended message out
*
*  Return Value: current scsi bus phase (unsigned char)
*             
*********************************************************************/
UBYTE
Ph_ExtMsgo (sp_struct *scb_ptr, AIC_7870 *base)
{
   cfp_struct *config_ptr;
   UBYTE c, j, phase, xfer_option, i=0, scsi_rate=0;
   UBYTE savcnt0,savcnt1,savcnt2;   /* save for stcnt just in case of pio */
   int bus_free=0;

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
      phase = Ph_Wt4Req(scb_ptr, base);
      if (phase != MOPHASE) {
	 OUTBYTE(AIC7870[CLRSINT1], CLRATNO);
	 if (phase == ERR)
	     bus_free=1;
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

   if (bus_free)
	   return ERR;
   else
	   return(Ph_Wt4Req(scb_ptr, base));
}


/*********************************************************************
*
* Handle Message In for short messages and target initiated nego?
*
*********************************************************************/
void
Ph_HandleMsgi (sp_struct *scb_ptr, AIC_7870 *base)
{
   cfp_struct *config_ptr = scb_ptr->SP_ConfigPtr;
   UBYTE rejected_msg, phase;

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
            Ph_RdByteX(base);
            Ph_TargetAbort(config_ptr, scb_ptr, base);
            return;
         }
      }
      else
      {
         OUTBYTE(AIC7870[SCSISIG], MIPHASE | ATNO);
         do {
            Ph_RdByteX(base);
            phase = Ph_Wt4Req(scb_ptr, base);
         } while (phase == MIPHASE);
         if (phase != MOPHASE)
         {
            Ph_BadSeq(config_ptr, base, BADSEQ_MSGI);
            return;
         }
         OUTBYTE(AIC7870[SCSISIG], MOPHASE);
         OUTBYTE(AIC7870[CLRSINT1], CLRATNO);
         OUTBYTE(AIC7870[SCSIDATL], MSG07);
         return;
      }
   }
   Ph_RdByteX(base);
}


/*********************************************************************
*
*  Handle SCSI selection timeout Interrupt
*
*********************************************************************/
void Ph_IntSelto (cfp_struct *config_ptr,
                  sp_struct *scb_ptr)
{
   register AIC_7870 *base;
   UBYTE scb_num;

   base = config_ptr->CFP_Base;
   scb_num = INBYTE(AIC7870[WAITING_SCB]);

   OUTBYTE(AIC7870[SCSISEQ], (INBYTE(AIC7870[SCSISEQ]) & ~(ENSELO + ENAUTOATNO)));
   Ph_OptimaClearTargetBusy(config_ptr, scb_num);
   OUTBYTE(AIC7870[CLRSINT1], CLRSELTIMO + CLRBUSFREE);

   if (scb_ptr != (sp_struct *) 0)
   {
   	Ph_EnableNextScbArray(scb_ptr);
   	scb_ptr->SP_HaStat = HOST_SEL_TO;
   	Ph_TerminateCommand(scb_ptr, scb_num);
   }
}


/*
 * Set the selection timeout value.
 * Stimesel contains the 2 bit value, in the correct bit positions,
 * to be programmed into the XSFRCTL1 register.
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
		PH_WriteHcntrl(base, (hcntrl | PAUSE));
		need_unpause=1;
	}

	sxfrctl1 = INBYTE(AIC7870[SXFRCTL1]);
	sxfrctl1 &= ~(STIMESEL1|STIMESEL0);
	sxfrctl1 |= stimesel;
	OUTBYTE(AIC7870[SXFRCTL1], sxfrctl1);

	if (need_unpause)
		PH_WriteHcntrl(base, hcntrl);
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
   UBYTE scb;

   base = config_ptr->CFP_Base;
   scb = INBYTE(AIC7870[ACTIVE_SCB]);

   /* Reset DMA & SCSI transfer logic */
   OUTBYTE(AIC7870[DFCNTRL],FIFORESET);
   OUTBYTE(AIC7870[SXFRCTL0], INBYTE(AIC7870[SXFRCTL0]) | (CLRSTCNT | CLRCHN | SPIOEN));
   OUTBYTE(AIC7870[SIMODE1], INBYTE(AIC7870[SIMODE1]) & ~ENBUSFREE);
   OUTBYTE(AIC7870[SCSIRATE], 0x00);
   OUTBYTE(AIC7870[CLRSINT1], CLRBUSFREE);

   OUTBYTE(AIC7870[SEQADDR0], (UBYTE) IDLE_LOOP_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], (UBYTE) IDLE_LOOP_ENTRY >> 10);

   if (scb_ptr != (sp_struct *) 0)
   {
      adp78_ha_t *ha_p;

      Ph_EnableNextScbArray(scb_ptr);        /* Enable next scb array   */
      Ph_OptimaClearTargetBusy(config_ptr, scb);

      ha_p = scb_ptr->Sp_OSspecific;

      /* if op was abort then bus free is expected so no bad status */
      if (!ha_p->ha_flags & HAF_ABORT)
	scb_ptr->SP_HaStat = HOST_BUS_FREE;
      Ph_TerminateCommand(scb_ptr, scb);
   }
}


/*********************************************************************
*
*  handle SCSI parity errors
*                  
*********************************************************************/
void Ph_ParityError (sp_struct *scb_ptr, AIC_7870 *base)
{
   UBYTE i;

   cmn_err(CE_ALERT, "SCSI controller %d detected parity error.",
	  scb_ptr->SP_ConfigPtr->Cf_DeviceNumber - 1);

   Ph_Wt4Req(scb_ptr, base);

   i = INBYTE(AIC7870[SXFRCTL1]);            /* Turn parity checking off.  */
   OUTBYTE(AIC7870[SXFRCTL1], i & ~ENSPCHK); /* It will be turned back on  */
                                             /* in message out phase       */
}


/*********************************************************************
 * 
 * Handle PCI errors.
 *
 **********************************************************************/
void
Ph_pci_error(cfp_struct *config_ptr, AIC_7870 *base)
{
	int do_reset=0;
	int status;
	UBYTE pcistatus;

	pcistatus = INBYTE(AIC7870[PCISTATUS]);
	if ((pcistatus & 0x3f) == DSRMA) {
/*		printf("received master abort\n"); */
	} else if ((pcistatus & 0x3f) == 0) {
/*		printf("stray pcistatus 0x%x\n", pcistatus); */
	} else {
		cmn_err(CE_ALERT, "SCSI controller %d detected pci errror 0x%x.",
			config_ptr->Cf_DeviceNumber - 1, pcistatus);
		do_reset = 1;
	}
	OUTBYTE(AIC7870[CLRINT], CLRPARERR);
	pciio_config_set(config_ptr->Cf_ConnVhdl, PCI_CFG_STATUS, 2, 0x7980);
	status = pciio_config_get(config_ptr->Cf_ConnVhdl, PCI_CFG_STATUS, 2);
/*	printf("recheck PCI status 0x%x\n", status); */

	if (do_reset)
		adp_reset(config_ptr->Cf_Adapter, 0, 1);
}


/*********************************************************************
*
*  wait for target to assert REQ.
*
*  Return Value:  current SCSI bus phase
*
*********************************************************************/
UBYTE
Ph_Wt4Req (sp_struct *scb_ptr, AIC_7870 *base)
{
   adp78_adap_t *ad_p;
   UBYTE phase, sstat1;
   int tries=0;

      ad_p = scb_ptr->SP_ConfigPtr->Cf_Adapter;
      /* wait for the ack signal to go away */
      while (INBYTE(AIC7870[SCSISIG]) & ACKI);

      /*
       * leading edge of REQ will set REQINIT in sstat1.
       * We want req, but also check for bus free or reset in
       * if when we see reset if we have been trying for >10000 loops
       * this command is probably responsible for a hang and thus
       * the reset, so set the HaStat field appropriately.
       */
      while (((sstat1 = INBYTE(AIC7870[SSTAT1])) & REQINIT) == 0) {
         if ((sstat1 & (BUSFREE | SCSIRSTI)) || (ad_p->ad_flags & ADF_RESET_IN_PROG))
	   {
	     if ((ad_p->ad_flags & ADF_RESET_IN_PROG) && (tries > 10000))
	       scb_ptr->SP_HaStat = HOST_ABT_FAIL;
	     return(ERR);
	   }
	 us_delay(10);
	 tries++;
      }
      OUTBYTE(AIC7870[CLRSINT1], CLRSCSIPERR);

      phase = INBYTE(AIC7870[SCSISIG]) & BUSPHASE;

      /* why do I even need to check here? */
      sstat1 = INBYTE(AIC7870[SSTAT1]);
      if (sstat1 & SCSIPERR) {
	 OUTBYTE(AIC7870[CLRSINT1], CLRSCSIPERR);
#ifdef orig
         OUTBYTE(AIC7870[SCSISIG], phase);
         Ph_RdByteX(base);
         continue;
#endif
      }

      return(phase);
}


/*********************************************************************
*
*  Pause AIC-7870 sequencer
*
*********************************************************************/

void
PH_Pause (register AIC_7870 *base)
{
   PH_WriteHcntrl(base, (UBYTE) (INBYTE(AIC7870[HCNTRL]) | PAUSE));
   while (!(INBYTE(AIC7870[HCNTRL]) & PAUSEACK));
}

/*********************************************************************
*
*  Pause AIC-7870 sequencer, return previous PAUSE state
*
*********************************************************************/

int
PH_Pause_rtn (register AIC_7870 *base)
{
  UBYTE hctrl;

  hctrl = INBYTE(AIC7870[HCNTRL]);
  PH_WriteHcntrl(base, (UBYTE) (INBYTE(AIC7870[HCNTRL]) | PAUSE));
  while (!(INBYTE(AIC7870[HCNTRL]) & PAUSEACK));
  if (hctrl & PAUSE)
    return (1);
  else
    return (0);
}


/*********************************************************************
*
* UnPause AIC-7870 sequencer and try to be sure that its unpaused.
* If still paused, clear INTSTAT
*
*********************************************************************/
void
PH_UnPause_hard(AIC_7870 *base)
{
   UBYTE hcntrl, intstat;

   hcntrl = INBYTE(AIC7870[HCNTRL]);
   if ((hcntrl & PAUSE) == 0)
	   return;

   hcntrl &= ~PAUSE;
   OUTBYTE(AIC7870[HCNTRL], hcntrl);

   hcntrl = INBYTE(AIC7870[HCNTRL]);
   if (hcntrl & PAUSE) {
	   intstat = INBYTE(AIC7870[INTSTAT]);
	   if (intstat & ANYINT)
		   OUTBYTE(AIC7870[CLRINT], 0x1f);

	   hcntrl &= ~PAUSE;
	   OUTBYTE(AIC7870[HCNTRL], hcntrl);

	   hcntrl = INBYTE(AIC7870[HCNTRL]);
#ifdef DEBUG
	   if (hcntrl & PAUSE) {
		   printf("still paused\n");
	   }
#endif
   }
}


/*********************************************************************
*
*  UnPause AIC-7870 sequencer
*
*********************************************************************/

void
PH_UnPause (AIC_7870 *base)
{
   UBYTE hcntrl;

   hcntrl = INBYTE(AIC7870[HCNTRL]);
   if ((hcntrl & PAUSE) == 0)
	   return;

   hcntrl &= ~PAUSE;
   OUTBYTE(AIC7870[HCNTRL], hcntrl);
   hcntrl = INBYTE(AIC7870[HCNTRL]);
}


/*********************************************************************
*
*  Write to HCNTRL
*
*********************************************************************/

void
PH_WriteHcntrl (register AIC_7870 *base, UBYTE value)
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
*  return value of INTSTAT register
*
*  Remarks: This function is designed to test a work-around for the
*           asynchronous pause problem in Lance
*
*********************************************************************/

UBYTE PH_ReadIntstat (register AIC_7870 *base)
{
   UBYTE hcntrl_data;
   UBYTE value;

               
   hcntrl_data = INBYTE(AIC7870[HCNTRL]);               /* If output will  */
   if (!(hcntrl_data & PAUSE))                          /* pause chip, just*/
   {                                                    /* do the output.  */
      OUTBYTE(AIC7870[HCNTRL], hcntrl_data | PAUSE);    /* pause the chip  */
      while (!(INBYTE(AIC7870[HCNTRL]) & PAUSEACK));    /* first.          */
      if ((value = INBYTE(AIC7870[INTSTAT])) & ANYPAUSE)/* paused due to an*/
      {                                                 /* interrupt, make */
         hcntrl_data |= PAUSE;                          /* sure we turn the*/
      }                                              	  /* pause bit on.   */
      OUTBYTE(AIC7870[HCNTRL], hcntrl_data);            /* Restore HCNTRL  */
   }
   else
   {                                                    /* Already paused  */
      value = INBYTE(AIC7870[INTSTAT]);                 /* just read it    */
   }
   return(value);
}


/*********************************************************************
*
*  Enable asssociated valid entry in next scb array
*
*********************************************************************/
void Ph_EnableNextScbArray (sp_struct *scb_ptr)
{
   AIC_7870 *base;
   UBYTE target, value, port_addr;

   base = scb_ptr->SP_ConfigPtr->CFP_Base;
   target = ((UBYTE) scb_ptr->SP_Tarlun) >> 4;

   port_addr = OSM_CONV_BYTEPTR(SCRATCH_NEXT_SCB_ARRAY) + target;
   port_addr = OSM_CONV_BYTEPTR(port_addr);

   value = INBYTE(AIC7870[port_addr]);

   if ((value & 0x80) == 0)
      if (value != 0x7f)
         OUTBYTE(AIC7870[port_addr], value | 0x80);
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
