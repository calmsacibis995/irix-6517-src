/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/io/adphim/RCS/himd.c,v 1.3 1997/12/20 02:30:39 philw Exp $ */
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
#include "osm.h"
#include "sys/systm.h"
#include "sys/mips_addrspace.h"
#include "sys/cmn_err.h"
#include "drvr_mac.h"
#include "libsk.h"
#include "libsc.h"

extern int	adp_verbose;
extern void	PH_ScbCompleted(sp_struct *);

extern UBYTE adp_curr_scb_num;


/*********************************************************************
*
* Main entry point from driver to HIM (called by adp78command)
*
*********************************************************************/

void PH_ScbSend (sp_struct *scb_ptr)
{
   register cfp_struct *config_ptr;
   register hsp_struct *ha_ptr;

   config_ptr = scb_ptr->SP_ConfigPtr;
   ha_ptr = config_ptr->CFP_HaDataPtr;
   
   /* Normal SCSI command */
   if ((scb_ptr->SP_Cmd == EXEC_SCB) ||
       (scb_ptr->SP_Cmd == NO_OPERATION)) {

      Ph_ScbPrepare(config_ptr, scb_ptr);
      Ph_SendCommand(ha_ptr, scb_ptr);

   } else {

      /* Non-initator mode command - used for SCSI bus reset */
      Ph_NonInit(scb_ptr);
   }
}


/*********************************************************************
*
* Handle command completed status.
*
*********************************************************************/
UBYTE
PH_IntHandler (cfp_struct *config_ptr)
{
   AIC_7870 *base=config_ptr->CFP_Base;
   hsp_struct *ha_ptr=config_ptr->CFP_HaDataPtr;
   int i, found=0;

   for (i=0; i < 256; i++) {
	   if (QOUT_PTR_ARRAY[i] != NULL_SCB) {
		   found = 1;
		   break;
	   }
   }
   if (found) {
	   if (QOUT_PTR_ARRAY[i] != adp_curr_scb_num)
		   printf("adp_curr_scb_num does not match\n",
			  adp_curr_scb_num, QOUT_PTR_ARRAY);
   } else {
	   printf("no valid QOUT_PTR_ARRAY\n");
   }

   Ph_OptimaCmdComplete(config_ptr, adp_curr_scb_num);
   Ph_PostCommand(config_ptr);

   return (CMDCMPLT);
}


/*
 * Handle abnormal interrupt cases.
 */
UBYTE
PH_IntHandler_abn(cfp_struct *config_ptr, UBYTE int_status)
{
   register AIC_7870   *base;
   register hsp_struct *ha_ptr;
   register sp_struct  *scb_ptr;
   UBYTE i, byte_buf;
   UBYTE ret_status;
   UBYTE scb_index;
   int count = 0;
   int int_cmds = 0;
   base = config_ptr->CFP_Base;
   ha_ptr = config_ptr->CFP_HaDataPtr;

   if (adp_verbose)
	   printf("processing abnormal interrupt\n");

      ret_status = int_status & ANYINT;
      
	    int_cmds++;

            byte_buf = INBYTE(AIC7870[SSTAT1]);

	    if (adp_verbose)
		    printf("loop %d: int_status 0x%x sstat1 0x%x\n", int_cmds,
			   int_status, byte_buf);

            /*
	     * Handle external SCSI bus reset first.
	     */
            if (byte_buf & SCSIRSTI) {
	       printf("SCSI: external device signaled bus reset\n");
               Ph_ext_scsi_bus_reset(config_ptr);
               int_status = SCSIINT;
               i = INVALID_SCB_INDEX;
            } else if (byte_buf & SELTO) {
               i = INBYTE(AIC7870[WAITING_SCB]);
            } else {
               i = INBYTE(AIC7870[ACTIVE_SCB]);
            }

            if (i == INVALID_SCB_INDEX)
               scb_ptr = (sp_struct *) 0;
            else
               scb_ptr = ACTPTR[i];

            if (int_status & SEQINT) {
               OUTBYTE(AIC7870[CLRINT], SEQINT);
               if (scb_ptr == (sp_struct *) 0)
		       int_status = ABORT_TARGET;
	       else
		       int_status &= INTCODE;
        
	       if (adp_verbose)
		       printf("Sequencer int_status 0x%x scb_index 0x%x scb 0x%x\n",
			      int_status, i, scb_ptr);

               if (int_status == DATA_OVERRUN)	        Ph_CheckLength(scb_ptr, base);
               else if (int_status == CDB_XFER_PROBLEM) Ph_CdbAbort(scb_ptr, base);
               else if (int_status == HANDLE_MSG_OUT)   Ph_SendMsgo(scb_ptr, base);
               else if (int_status == SYNC_NEGO_NEEDED) Ph_Negotiate(scb_ptr, base);
               else if (int_status == CHECK_CONDX) {
		       Ph_CheckCondition(scb_ptr, base);
		       ret_status = CMDCMPLT;
	       }
               else if (int_status == PHASE_ERROR)      Ph_BadSeq(config_ptr, base);
               else if (int_status == EXTENDED_MSG)     Ph_ExtMsgi(scb_ptr, base);
               else if (int_status == UNKNOWN_MSG)      Ph_HandleMsgi(scb_ptr, base);
               else if (int_status == ABORT_TARGET)     Ph_TargetAbort(config_ptr, scb_ptr, base);
               else if (int_status == NO_ID_MSG)        Ph_TargetAbort(config_ptr, scb_ptr, base);
            }

	    if (int_status & SCSIINT) {

               /*
		* SCSI Reset is handled before any other interrupts.
                * Select timeout must be checked before bus free since
                * bus free will be set due to a selection timeout.
		*/
	       if (adp_verbose)
		       printf("SCSI interrupt, sstat1 0x%x\n", byte_buf);

               if      (byte_buf & SELTO)    Ph_IntSelto(config_ptr, scb_ptr);
               else if (byte_buf & BUSFREE)  Ph_IntFree(config_ptr, scb_ptr);
               else if (byte_buf & SCSIPERR) Ph_ParityError(base);
               OUTBYTE(AIC7870[CLRINT], CLRSCSINT);
            }
            
	    if (int_status & BRKINT) {
	       printf("BRKINT\n");

	       OUTBYTE(AIC7870[CLRINT], BRKINT);
	       OUTBYTE(AIC7870[BRKADDR1], BRKDIS); /* Disable Breakpoint */
	       byte_buf = INBYTE(AIC7870[SEQCTL]) & ~BRKINTEN;
	       OUTBYTE(AIC7870[SEQCTL], byte_buf); /* Disable BP Interrupt */

            }

         Ph_PostCommand(config_ptr);

   return (ret_status);
}


/*********************************************************************
*
* Set up scb status fields for sending.
*
*********************************************************************/

void Ph_ScbPrepare (cfp_struct *config_ptr,
                     sp_struct *scb_ptr)
{
	scb_ptr->SP_Stat = SCB_PENDING;
	scb_ptr->SP_MgrStat = SCB_READY;
}

/*********************************************************************
*
* Update more variables.  Then enqueue the command by calling
* Ph_OptimaEnqueue.
* Always returns 0.
*
*********************************************************************/
UBYTE Ph_SendCommand (hsp_struct *ha_ptr, sp_struct *scb_ptr)
                      
{
	UBYTE i;

	i = scb_ptr->Sp_scb_num;

	/* Set Current Scb as Active                                */
	scb_ptr->SP_MgrStat = SCB_ACTIVE;

	/* Queue an OPTIMA/STANDARD SCB for the Lance Sequencer    */
	ha_ptr->enque(i,scb_ptr);

	return(0);
}

/*********************************************************************
*
*  Remarks:  Caller must guarentee non-reentrancy!
*
*********************************************************************/
void Ph_TerminateCommand (sp_struct *scb_ptr, UBYTE scb)
{
   hsp_struct *ha_ptr;

   if (scb_ptr == NOT_DEFINED) 
      return;

   ha_ptr = scb_ptr->SP_ConfigPtr->CFP_HaDataPtr;

   /* return to done stack only if scb is valid */
   if (scb != INVALID_SCB_INDEX)
      DONE_STACK[(DONE_CMD)++] = scb;

   Ph_SetMgrStat(scb_ptr);                   /* Update status */
}

/*********************************************************************
*
*  Remarks:  Caller must guarentee non-reentrancy!
*
*********************************************************************/

void Ph_PostCommand (cfp_struct *config_ptr)
{
   hsp_struct *ha_ptr;
   sp_struct *scb_ptr;
   UBYTE scb_num;

   ha_ptr = config_ptr->CFP_HaDataPtr;

   while (DONE_CMD)
   {
      scb_num = DONE_STACK[--(DONE_CMD)];
      scb_ptr = ACTPTR[scb_num];
      SCB_PTR_ARRAY[scb_num]  = 0xffffffff;
      ACTPTR[scb_num] = (sp_struct *) 0;
      scb_ptr->SP_Stat = scb_ptr->SP_MgrStat;
      PH_ScbCompleted(scb_ptr);                /* this is in adp78.c */
   }
}

/*********************************************************************
*
*  Ph_RemoveAndPostScb routine -
*
*  Return completed (but non active) SCB to ASPI layer
*
*  Return Value:  None
*
*  Parameters: config_ptr
*
*  Activation: PH_PostCommand
*
*  Remarks:
*
*********************************************************************/
void Ph_RemoveAndPostScb (cfp_struct *config_ptr,
                           sp_struct *scb_ptr)
{
   scb_ptr->SP_Stat = scb_ptr->SP_MgrStat;
   PH_ScbCompleted(scb_ptr);                     /* post */
}

/*********************************************************************
*
*  Ph_PostNonActiveScb routine -
*
*  Return completed (but non active) SCB to ASPI layer
*
*  Return Value:  None
*
*  Parameters: config_ptr
*
*  Activation: PH_PostCommand
*
*  Remarks:
*
*********************************************************************/
void Ph_PostNonActiveScb (cfp_struct *config_ptr,
                           sp_struct *scb_ptr)
{
   Ph_RemoveAndPostScb(config_ptr,scb_ptr);
}

/*********************************************************************
*
*  Ph_TermPostNonActiveScb routine -
*
*  Terminate and return completed (but non active) SCB to ASPI layer
*
*  Return Value:  None
*
*  Parameters: config_ptr
*
*  Activation: PH_AbortChannel
*
*  Remarks:
*
*********************************************************************/
void Ph_TermPostNonActiveScb (sp_struct *scb_ptr)
{
   register cfp_struct *config_ptr = scb_ptr->SP_ConfigPtr;

   Ph_TerminateCommand(scb_ptr, 0xff);
   Ph_RemoveAndPostScb(config_ptr, scb_ptr);
}


/*********************************************************************
*
* Reset everything, scsi bus, host adapter, sequencer
* If the request came from above adp78, then pause the sequencer first.
* If the request came from the HIM itself, then don't pause or unpause
* the sequencer.  The pause already happened on our entry into the HIM,
* and the unpause will happen when we return through the calling stack.

*********************************************************************/
void
Ph_HardReset (cfp_struct *config_ptr, int from_upper)
{
   AIC_7870 *base = config_ptr->CFP_Base;

   if (from_upper) {
	   Ph_Pause(base);
	   OUTBYTE(AIC7870[CLRINT],(CLRCMDINT | SEQINT | BRKINT | CLRSCSINT));
   }

   Ph_ResetSCSI(base);
   Ph_ResetChannel(config_ptr);
   Ph_set_scratchmem(config_ptr);

   /* Now, Restart Sequencer */
   OUTBYTE(AIC7870[SEQADDR0], IDLE_LOOP_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], IDLE_LOOP_ENTRY >> 10);

   if (from_upper)
	   Ph_UnPause(base);
}


/*********************************************************************
*
* Initialize the hsp struct.  Most of the hsp struct will be eventually
* moved into the cfp struct.
*
*********************************************************************/
void
Ph_init_hsp_struct(cfp_struct *config_ptr)
{
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   int i, addr;
   __psunsigned_t qout_ptr_array_addr;

   ha_ptr->Head_Of_Queue = ha_ptr->End_Of_Queue = NOT_DEFINED;
   ha_ptr->Hsp_MaxNonTagScbs  = (UBYTE) config_ptr->Cf_MaxNonTagScbs;
   ha_ptr->Hsp_MaxTagScbs     = (UBYTE) config_ptr->Cf_MaxTagScbs;

   ha_ptr->enque = Ph_OptimaEnque;
   ha_ptr->enquehead = Ph_OptimaEnqueHead;
   ha_ptr->cmdcomplete = NULL;
   ha_ptr->getfreescb = Ph_OptimaGetFreeScb;
   ha_ptr->returnfreescb = NULL;
   ha_ptr->abortactive = Ph_OptimaAbortActive; 
   ha_ptr->indexclearbusy = Ph_OptimaIndexClearBusy;
   ha_ptr->cleartargetbusy = Ph_OptimaClearTargetBusy;
   ha_ptr->requestsense = Ph_OptimaRequestSense;

   /*
    * Initialize SCB_PTR_ARRAY
    */
   ha_ptr->scb_ptr_array = (DWORD *) malloc(ADP78_NUMSCBS * sizeof(DWORD));

   /*
    * Initialize QOUT_PTR_ARRAY and the index pointer into it.
    * The array has to be 256 byte aligned.
    */
   ha_ptr->qout_ptr_all = (int *) malloc(1024);
   addr = (int) ha_ptr->qout_ptr_all;
   addr += 256;
   addr = addr & 0xffffff00;
   QOUT_PTR_ARRAY= (UBYTE *) addr;
   for (i = 0; i < 256 ; i++)
      QOUT_PTR_ARRAY[i]= (UBYTE) NOT_DEFINED;

   qout_ptr_array_addr = (__psunsigned_t)QOUT_PTR_ARRAY; /* compiler gets error on next line */
   DCACHE_WBINVAL(qout_ptr_array_addr, 256);

   /* Change SCB_PTR_ARRAY to K1 to save cache flushing calls */
   SCB_PTR_ARRAY = (DWORD *) K0_TO_K1(SCB_PTR_ARRAY);
   QOUT_PTR_ARRAY = (UBYTE *) K0_TO_K1(QOUT_PTR_ARRAY);

   Ph_set_scratchmem(config_ptr);
}


/*********************************************************************
*
*  Ph_ScbRenego -
*
*  Reset scratch RAM to initiate or suppress sync/wide negotiation.
*
*  Return Value:  void
*
*  Parameters: config_ptr
*              tarlun      - Target SCSI ID / Channel / LUN,
*                            same format as in SCB.
*
*  Activation: scb_special
*
*  Remarks:
*
*********************************************************************/
void Ph_ScbRenego (cfp_struct *config_ptr, UBYTE tarlun)
{
   AIC_7870 *base = config_ptr->CFP_Base;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   UBYTE option_index, scratch_index;
#ifdef MIPS_BE
   UBYTE le_index, conv_index;
#endif /* MIPS_BE */

   /* Extract SCSI ID */
   option_index = scratch_index = (tarlun & TARGET_ID) >> 4;

   /* Write scratch RAM, renegotiate */

   if (config_ptr->Cf_ScsiOption[option_index] & (WIDE_MODE + SYNC_MODE))
   {
      /*  Setup Sequencer XFER_OPTION with NEEDNEGO (Need to Negotiate) */
      Ph_SetNeedNego(scratch_index, base);
   }
   else
   {
      /* Write scratch RAM, asynch. */
#ifdef MIPS_BE
      le_index = osm_conv_byteptr(XFER_OPTION) + scratch_index;
      conv_index = osm_conv_byteptr(le_index);
      OUTBYTE(AIC7870[conv_index], 00);
#else /* MIPS_BE */
      OUTBYTE(AIC7870[XFER_OPTION + scratch_index], 00);
#endif /* MIPS_BE */
   }
   return;
}




/*********************************************************************
*
*  Ph_GetScbStatus routine -
*
* Upper layer guarentees to never send more commands than its supposed
* to.
*
*********************************************************************/
UBYTE Ph_GetScbStatus (hsp_struct *ha_ptr,
                      sp_struct *scb_ptr)
{
   return(SCB_READY);
}

/*********************************************************************
*
*  Ph_SetMgrStat routine -
*
*  Set status value to MgrStat
*
*  Return Value:  None
*
*  Parameters: scb_ptr
*
*  Activation: PH_TerminateCommand
*
*  Remarks:
*
*********************************************************************/
void Ph_SetMgrStat (sp_struct *scb_ptr )
{
   scb_ptr->SP_MgrStat = SCB_DONE;           /* Update status */
   if (scb_ptr->SP_HaStat)
   {
      scb_ptr->SP_MgrStat = SCB_DONE_ERR;
   }
   switch (scb_ptr->SP_TargStat)
   {
      case UNIT_GOOD:
      case UNIT_MET:
      case UNIT_INTERMED:
      case UNIT_INTMED_GD:
         break;
      default:
         scb_ptr->SP_MgrStat = SCB_DONE_ERR;
   }
}

/*********************************************************************
*  Ph_ScbAbort -
*
*  - Aborts inactive SCBs
*  - Calls ha_ptr->abortactive() for active SCBs
*
*  Return Value:
*
*  Parameters:
*
*  Activation: PH_Special()
*
*  Remarks:
*
*********************************************************************/
#ifdef   _FAULT_TOLERANCE
void Ph_ScbAbort ( sp_struct *scb_ptr)
{
   register cfp_struct *config_ptr = scb_ptr->SP_ConfigPtr;
   register AIC_7870 *base = config_ptr->CFP_Base;
   register hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;

   /* Take action based on current SCB state */
   switch(scb_ptr->SP_MgrStat)
   {
      case  SCB_ACTIVE:       /* Active SCB, call mode specific routine */
         scb_ptr->SP_MgrStat = SCB_ABORTED;
         scb_ptr->SP_HaStat  = HOST_ABT_HOST;
         if (ha_ptr->abortactive( scb_ptr) == ABTACT_CMP)
         {
            Ph_TermPostNonActiveScb( scb_ptr);     /* Terminate and Post */
         }
         break;

      case  SCB_WAITING:
         scb_ptr->SP_HaStat  = HOST_ABT_HOST;
         scb_ptr->SP_MgrStat = SCB_ABORTED;
         Ph_PostNonActiveScb(config_ptr, scb_ptr); /* Terminate and Post */
         break;

      case  SCB_READY:
         scb_ptr->SP_HaStat  = HOST_ABT_HOST;
         scb_ptr->SP_MgrStat = SCB_ABORTED;
         Ph_TermPostNonActiveScb(scb_ptr);         /* Terminate and Post */
         break;

      case  SCB_PROCESS:      /* SCB not active, mark and post */
      case  SCB_DONE:         /* SCB already completed, just return */
      case  SCB_DONE_ABT:
      case  SCB_DONE_ERR:
      case  SCB_DONE_ILL:
      default:
         break;
   }
   return;
}
#endif

#endif /* SIMHIM */
