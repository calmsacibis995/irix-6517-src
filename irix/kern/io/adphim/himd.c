/* $Header: /proj/irix6.5.7m/isms/irix/kern/io/adphim/RCS/himd.c,v 1.10 1998/04/04 02:16:08 gwh Exp $ */
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

#include "him_scb.h"
#include "him_equ.h"
#include "osm.h"
#include "sys/systm.h"
#include "sys/mips_addrspace.h"
#include "sys/kmem.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/ddi.h"
#include "sys/immu.h"

extern int	adp_verbose;
extern int	adp_coredump;
extern void	PH_ScbCompleted(sp_struct *);
extern void	adp_reset(void *, int, int);


/*********************************************************************
*
* Main entry point from driver to HIM (called by adp78command)
*
*********************************************************************/

void PH_ScbSend (sp_struct *scb_ptr)
{
   cfp_struct *config_ptr;
   hsp_struct *ha_ptr;

   config_ptr = scb_ptr->SP_ConfigPtr;
   ha_ptr = config_ptr->CFP_HaDataPtr;
   
   ASSERT((scb_ptr->SP_Cmd == EXEC_SCB) || (scb_ptr->SP_Cmd == NO_OPERATION));

   scb_ptr->SP_Stat = SCB_PENDING;
   scb_ptr->SP_MgrStat = SCB_READY;
   scb_ptr->Sp_dowide = 0;
   scb_ptr->Sp_seq_scb.seq_scb_struct.Aborted = 0;
   scb_ptr->Sp_seq_scb.seq_scb_struct.SpecFunc = 0;
   Ph_SendCommand(ha_ptr, scb_ptr);
}


/*********************************************************************
*
*  Remarks:       More than one interrupt status bit can legally be
*                 set. It is also possible that no bits are set, in
*                 the case of using PH_IntHandler for polling.
*
*********************************************************************/

/*********************************************************************
 *
 * Look in QOUT_PTR_ARRAY for valid scb numbers.
 *
 * Return CMDCMPLT (2) if one was found, otherwise return 0.
 *
*********************************************************************/
static UBYTE
Ph_checkqout(cfp_struct *config_ptr)
{
   hsp_struct *ha_ptr;
   UBYTE i, qout_index;

   ha_ptr = config_ptr->CFP_HaDataPtr;

   qout_index = OSM_CONV_BYTEPTR(ha_ptr->qout_index);
   i = QOUT_PTR_ARRAY[qout_index];

   /*
    * If there is not a valid scb number at the current location,
    * the DMA may be late getting to memory.  Do a register read and
    * try again.
    */
   if (i == INVALID_SCB_INDEX) {
	   char s;
	   AIC_7870 *base;
	   base = config_ptr->CFP_Base;
	   s = INBYTE(AIC7870[SCSIID]);
	   i = QOUT_PTR_ARRAY[qout_index];
   }

   /*
    * If QOUT_PTR_ARRAY contains a valid scb number at the right location,
    * call Ph_OptimaCmdComplete, to process it.
    * Ph_OptimaCmdComplete  will look for multiple scb completions.
    */
   if (i != INVALID_SCB_INDEX) {
	   Ph_OptimaCmdComplete(config_ptr, i);
	   Ph_PostCommand(config_ptr);
	   return (CMDCMPLT);
   } else
	   return 0;
}


/************************************************************************
 * 
 * Assume its a normal cmd complete interrupt and call the abnormal
 * interrupt handler only if no valid scb numbers were found in the
 * qout array.
 *
 ************************************************************************/
UBYTE
PH_IntHandler(cfp_struct *config_ptr)
{
	UBYTE rval;
        UBYTE int_status;
        AIC_7870   *base;


	rval = Ph_checkqout(config_ptr);
        base = config_ptr->CFP_Base;
        int_status = PH_ReadIntstat(base);

	if (rval == CMDCMPLT) {
	  	int_status &= ~CMDCMPLT;
		if (int_status && ANYPAUSE)
		  return (PH_IntHandler_abn(config_ptr));
		else
		  return rval;
	}
	else
		return (PH_IntHandler_abn(config_ptr));

}


/***************************************************************************
 *
 * Handle abnormal interrupt cases.
 *
 ***************************************************************************/
UBYTE
PH_IntHandler_abn(cfp_struct *config_ptr)
{
   AIC_7870   *base;
   hsp_struct *ha_ptr;
   sp_struct  *scb_ptr;
   int reset_occured = 0;
   int badseq_code;
   UBYTE i, sstat1, seqctl;
   UBYTE int_status, ret_status, error;


   base = config_ptr->CFP_Base;
   ha_ptr = config_ptr->CFP_HaDataPtr;
   int_status = PH_ReadIntstat(base);
   ret_status = int_status & ANYINT;

   /*
    * Its possible that 2 completed scb numbers were DMA'd to
    * QOUT_PTR_ARRAY, and we saw both, but only cleared one interrupt.
    * When the next interrupt comes, we might not see any valid
    * entries in QOUT_PTR_ARRAY, but we still must clear the interrupt
    * bit.
    */
   if (int_status & CMDCMPLT) {
	 OUTBYTE(AIC7870[CLRINT], CLRCMDINT);
	 Ph_checkqout(config_ptr);
   }

   if (int_status & ANYPAUSE) {

	 sstat1 = INBYTE(AIC7870[SSTAT1]);
	 /*
	  * Check for external SCSI bus reset
	  * Then selection timeout
	  */
	 if (sstat1 & SCSIRSTI) {
               Ph_ext_scsi_bus_reset(config_ptr);
               int_status = SCSIINT;
	       reset_occured = 1;
               i = INVALID_SCB_INDEX;

	 } else if (sstat1 & SELTO) {
	       i = INBYTE(AIC7870[WAITING_SCB]);

	 } else {
               i = INBYTE(AIC7870[ACTIVE_SCB]);
	       if (i == INVALID_SCB_INDEX) {

		       OUTBYTE(AIC7870[CLRINT], CLRSEQINT|CLRCMDINT|CLRSCSINT);
		       return ret_status;

	       } else {
		       /* i, the scb_num, must be good.  see if
			* the scb_ptr is good.
			*/
		       scb_ptr = ACTPTR[i];
		       if (scb_ptr == (sp_struct *) 0) {
			       UBYTE new_scb_num;
			       new_scb_num = INBYTE(AIC7870[ACTIVE_SCB]);

			       if (new_scb_num < 16)
				       i = new_scb_num;
			       scb_ptr = ACTPTR[i];
		       }

		       if (scb_ptr == (sp_struct *) 0) {
/*			       printf("intstat 0x%x sstat1 0x%x scb_num %d NULL scb_ptr\n",
				      int_status, sstat1, i); 
			       printf("clear all interrupt sources and return\n"); */
			       OUTBYTE(AIC7870[CLRINT], CLRSEQINT|CLRCMDINT|CLRSCSINT);
/*			       for (j=0; j < 16; j++)
				       printf("%d : 0x%x\n", j, ACTPTR[j]); */
			       return ret_status;
		       }
	       }


         }

	 if (i == INVALID_SCB_INDEX)
	       scb_ptr = (sp_struct *) 0;
	 else
               scb_ptr = ACTPTR[i];

	 /*
	  * See if its a sequencer, SCSI, or breakpoint interrupt.
	  */
            if (int_status & SEQINT) {
               OUTBYTE(AIC7870[CLRINT], SEQINT);
               if (scb_ptr == (sp_struct *) 0) {
		   badseq_code = BADSEQ_NULLSCB |
			         (int_status << 8) | sstat1;
		   reset_occured = Ph_BadSeq(config_ptr, base, badseq_code);
	       } else {
		   /*
		    * scb_ptr is guarenteed non-NULL here.
		    * check for special case where a scb is aborted
		    * in Ph_TargetAbort which is immedately followed
		    * by a phase error interrupt.
		    */
                   int_status &= INTCODE;

		   if (scb_ptr->SP_MgrStat == SCB_ABORTED &&
		       int_status == PHASE_ERROR)
			   /* do nothing */
			   ;
		   else if (int_status == DATA_OVERRUN)     Ph_CheckLength(scb_ptr, base);
                   else if (int_status == CDB_XFER_PROBLEM) reset_occured = Ph_BadSeq(config_ptr, base, BADSEQ_CDBXFER);
                   else if (int_status == HANDLE_MSG_OUT)   Ph_SendMsgo(scb_ptr, base);
		   else if (int_status == SYNC_NEGO_NEEDED) Ph_Negotiate(scb_ptr, base);
		   else if (int_status == CHECK_CONDX)      Ph_CheckCondition(scb_ptr, base, i);
		   else if (int_status == PHASE_ERROR)      reset_occured = Ph_BadSeq(config_ptr, base, BADSEQ_PHASE);
		   else if (int_status == EXTENDED_MSG)     Ph_ExtMsgi(scb_ptr, base);
		   else if (int_status == UNKNOWN_MSG)      Ph_HandleMsgi(scb_ptr, base);
		   else if (int_status == ABORT_TARGET)     reset_occured = Ph_TargetAbort(config_ptr, scb_ptr, base);
		   else if (int_status == NO_ID_MSG)        reset_occured = Ph_TargetAbort(config_ptr, scb_ptr, base);
	       }
            }

	    if (int_status & SCSIINT) {
	       /*
                * SCSI Reset is handled before any other interrupts.
                * Select timeout must be checked before bus free since
                * bus free will be set due to a selection timeout.
		*/

               if      (sstat1 & SELTO)    Ph_IntSelto(config_ptr, scb_ptr);
               else if (sstat1 & BUSFREE)  Ph_IntFree(config_ptr, scb_ptr);
               else if (sstat1 & SCSIPERR) Ph_ParityError(scb_ptr, base);
               OUTBYTE(AIC7870[CLRINT], CLRSCSINT);
            }

	    if (int_status & BRKINT) {
               OUTBYTE(AIC7870[CLRINT], BRKINT);
	       OUTBYTE(AIC7870[BRKADDR1], BRKDIS); /* Disable Breakpoint */
	       seqctl = INBYTE(AIC7870[SEQCTL]) & ~BRKINTEN;
	       OUTBYTE(AIC7870[SEQCTL], seqctl); /* Disable BP Interrupt */

	       error = INBYTE(AIC7870[ERROR]);
	       cmn_err(CE_ALERT, "SCSI controller %d detected internal error 0x%x.",
		       config_ptr->Cf_DeviceNumber - 1, error);
	       adp_reset(config_ptr->Cf_Adapter, 0, 1);
	       reset_occured = 1;
            }

	    if (reset_occured)
		    return (CMDCMPLT);

	    PH_UnPause(base);
	    Ph_PostCommand(config_ptr);
   }

   /*
    * Check for PCI and sequencer parity errors.
    */
   error = INBYTE(AIC7870[ERROR]);
   if (error & PCIERRSTAT) {
	   Ph_pci_error(config_ptr, base);
	   ret_status = CMDCMPLT;
   } else if (error & 0x3c) {
	   cmn_err(CE_ALERT, "SCSI controller %d detected internal parity error 0x%x\n",
		   config_ptr->Cf_DeviceNumber -1, error);
	   adp_reset(config_ptr->Cf_Adapter, 0, 1);
	   ret_status = CMDCMPLT;
   }

   return (ret_status);
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

	/* Get a Free scb from the FREE scb Pool                   */
	i = ha_ptr->getfreescb(ha_ptr,scb_ptr);

	/* Set Current Scb as Active                                */
	scb_ptr->SP_MgrStat = SCB_ACTIVE;

	/* Queue an OPTIMA/STANDARD SCB for the Lance Sequencer    */
	ha_ptr->enque(i,scb_ptr);

	return(0);
}


/*********************************************************************
 * 
 * First part of command done processing.
 * Push the scb_number of the command into the DONE_STACK
 * Set the Mgr_stat field.
 *
 *********************************************************************/
void
Ph_TerminateCommand (sp_struct *scb_ptr, UBYTE scb_num)
{
   hsp_struct *ha_ptr;

   if ((scb_ptr == (sp_struct *) 0) || (scb_num == INVALID_SCB_INDEX))
      return;

   ha_ptr = scb_ptr->SP_ConfigPtr->CFP_HaDataPtr;

   DONE_STACK[(DONE_CMD)++] = scb_num;

   /*
    * MgrStat gets SCB_DONE unless there is a ha or scsi status error or
    * if the command was aborted.
    */
   if (scb_ptr->SP_MgrStat == SCB_ABORTED)
       return;

   scb_ptr->SP_MgrStat = SCB_DONE;

   if (scb_ptr->SP_HaStat)
      scb_ptr->SP_MgrStat = SCB_DONE_ERR;

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
*
* Final processing in the HIM for a command that has completed.
* See also terminatecommand and PH_ScbCompleted
*
*********************************************************************/
void
Ph_PostCommand (cfp_struct *config_ptr)
{
   hsp_struct *ha_ptr;
   sp_struct *scb_ptr;
   UBYTE scb_num;

   ha_ptr = config_ptr->CFP_HaDataPtr;

   while (DONE_CMD)
   {
      scb_num = DONE_STACK[--(DONE_CMD)];
      scb_ptr = ACTPTR[scb_num];
      if (scb_ptr == (sp_struct *) 0) {
	      ASSERT(scb_ptr);
	      continue;
      }
      ACTPTR[scb_num] = (sp_struct *) 0;
      SCB_PTR_ARRAY[scb_num] = 0xffffffff;

      scb_ptr->SP_Stat = scb_ptr->SP_MgrStat;

      PH_ScbCompleted(scb_ptr);                /* this is in adp78.c */
   }
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
   int addr;

   ha_ptr->Hsp_MaxNonTagScbs  = (UBYTE) config_ptr->Cf_MaxNonTagScbs;
   ha_ptr->Hsp_MaxTagScbs     = (UBYTE) config_ptr->Cf_MaxTagScbs;

   ha_ptr->enque = Ph_OptimaEnque;
   ha_ptr->enquehead = NULL;
   ha_ptr->cmdcomplete = NULL;
   ha_ptr->getfreescb = Ph_OptimaGetFreeScb;
   ha_ptr->returnfreescb = NULL;
   ha_ptr->abortactive = NULL;
   ha_ptr->indexclearbusy = NULL;
   ha_ptr->cleartargetbusy = NULL;
   ha_ptr->requestsense = NULL;

   /*
    * Initialize SCB_PTR_ARRAY
    */
   ha_ptr->scb_ptr_array = (DWORD *) kmem_alloc(ADP78_NUMSCBS * sizeof(DWORD),
#ifdef MH_R10000_SPECULATION_WAR
						VM_UNCACHED | VM_DIRECT |
#endif /* MH_R10000_SPECULATION_WAR */
						KM_PHYSCONTIG|KM_CACHEALIGN);

   /*
    * Initialize QOUT_PTR_ARRAY and the index pointer into it.
    * The array has to be 256 byte aligned.
    */
   ha_ptr->qout_ptr_all = (int *) kmem_alloc(1024, 
#ifdef MH_R10000_SPECULATION_WAR
					     VM_UNCACHED | VM_DIRECT |
#endif /* MH_R10000_SPECULATION_WAR */
					     KM_PHYSCONTIG|KM_CACHEALIGN);
   addr = (int) ha_ptr->qout_ptr_all;
   addr += 256;
   addr = addr & 0xffffff00;
   QOUT_PTR_ARRAY= (UBYTE *) addr;

#ifndef MH_R10000_SPECULATION_WAR
   /* Change SCB_PTR_ARRAY to K1 to save cache flushing calls */
   SCB_PTR_ARRAY = (DWORD *) K0_TO_K1(SCB_PTR_ARRAY);
   QOUT_PTR_ARRAY = (UBYTE *) K0_TO_K1(QOUT_PTR_ARRAY);
#endif /* MH_R10000_SPECULATION_WAR */

   Ph_set_scratchmem(config_ptr);
}






