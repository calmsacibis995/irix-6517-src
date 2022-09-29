/***************************************************************************
*									   *
* Copyright 1997 Adaptec, Inc.,  All Rights Reserved.			   *
*									   *
* This software contains the valuable trade secrets of Adaptec.  The	   *
* software is protected under copyright laws as an unpublished work of	   *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.	The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.							   *
*									   *
***************************************************************************/

/***************************************************************************
*
*  Module Name:   hwintr.c
*
*  Description:
*		  Code to initialize Emerald chip
*
*  Owners:	  ECX Fibre Channel CHIM Team
*    
*  Notes:	  NONE
*
*  Entry Point(s):	sh_frontend_isr
*			sh_backend_isr
*
***************************************************************************/

#include "hwgeneral.h"
#include "hwcustom.h"
#include "ulmconfig.h"
#include "hwtcb.h"
#include "hwequ.h"
#include "hwref.h"
#include "hwdef.h"
#include "hwerror.h"
#include "hwproto.h"
#include "seq_off.h"

/* local function prototypes - static					*/

static void sh_int_error(struct control_blk *r, void *ulm_info);
static void sh_error(struct reference_blk *r, int err, void *ulm_info);
static void sh_pci_errout(struct reference_blk *r, int err, void *ulm_info);

#ifndef BIOS
/* This array must be changed together with codes in hwerror.h	*/
char *sh_event_strings[] = {
/*  0 */ "unknown error",
	 "Fibre Channel LIP",
	 "sequencer firmware reload failed",
	 "loop bypass primitive",
	 "loop enable primitive",
	 "ready for lip tcb",
	 "illegal MTPE op code",
	 "sequencer parity error",
	 "parity error in RPB read by MTPE",
	 "parity error in path from SPB to SFC",
/* 10 */ "parity error from RPB to HST",
	 "CMC RAM parity error",
	 "memory port parity error",
	 "parity error from CMC to HST",
	 "PCI STATUS1 - Data Parity Reported",
	 "host receive DMA data parity reported",
	 "CMC-PCI DMA data parity reported",
	 "CMC intr posting DMA data parity reported",
	 "PCI STATUS1 - Signal Target Abort",
	 "PCI STATUS1 - Reported Target Abort",
/* 20 */ "host send DMA reported target abort",
	 "host receive DMA reported target abort",
	 "CMC-PCI DMA reported target abort",
	 "CMC interrupt posting DMA reported target abort",
	 "PCI STATUS1 - Reported Master Abort",
	 "host send DMA reported master abort",
	 "host receive DMA reported master abort",
	 "CMC-PCI DMA reported master abort",
	 "CMC-PCI Interrupt posting reported master abort",
	 "PCI STATUS1 - #Serr asserted (system error)",
/* 30 */ "PCI STATUS1 - Data Parity Error",
	 "host send DMA data parity error",
	 "CMC-PCI DMA data parity error",
	 "CMC interrupt posting DMA data parity error",
	 "PCI target access data parity error",
	 "LPSM error",
	 "SERDES: controller link not usable",
	 "MIA Fault or Optical Loss of Signal",
	 "RFC: Loss of Signal",
	 "RFC: Loss of Signal LIP",
/* 40 */ "RFC: Loss of Synchronization",
	 "RFC: Loss of Synchronization LIP",
	 "Spindle Sync",
	 "Clock Sync"
};
#endif

/* This function can be entered multiple times before
 * sh_backend_isr() is called
 *
 * Note: when RTC interrupt is used, we will have to
 *	 add to the list of POST_STAT1 cases for
 *	 INTERRUPT_PENDING in hwequ.h
 *
 * return values:
 *	0 - NO_INTERRUPT_PENDING
 *	1 - INTERRUPT_PENDING
 *	2 - LONG_INTERRUPT_PENDING
 */
INT
sh_frontend_isr(struct control_blk *c)
{
	register REGISTER base = c->base;
	struct _doneq *d;
	UBYTE intstat, intstat1;
	INT status = NO_INTERRUPT_PENDING;
	INT count = 0;
	USHORT intstatus;

	intstat = c->dma->stat0;
	intstat1 = c->dma->stat1;

	c->intstat |= intstat;
	c->intstat1 |= intstat1;

	if (intstat & ALC_INT || intstat1 & POST_STAT1_INTERRUPT) {

		c->dma->stat0 &= ~intstat; 
		c->dma->stat1 &= ~intstat1; 

		status = INTERRUPT_PENDING;

		if (intstat1 & HW_ERR_INT) {
/*			save ERROR reg because it's cleared by HW_ERR_INT */
			c->error |= INBYTE(AICREG(ERROR));
		}

		if (intstat1 & SEQ_LIP_INT)
			status = LONG_INTERRUPT_PENDING;
	}

/*	we must look through the doneq to find if any tcbs are done		*/
/*	because we do not want hardware to dma in the SEQ_INT bit		*/

	d = c->d_front;
	while (d->q_pass_count == c->q_done_pass_front) {
		d++;
		count++;
		if (d >= c->qdone_end) {
			d = (struct _doneq *) c->s->doneq;
			c->q_done_pass_front++;
		}
	}

	c->d_front = d;

	if (count != 0) {
		if (status != LONG_INTERRUPT_PENDING)
			status = INTERRUPT_PENDING;
		intstat1 |= SEQ_INT;
	}

	/* 
	 * There is a chance that TCB in the doneq has been processed,
	 * but the interrupt status is not cleared.  So, we will need
	 * to do a PIO to make sure that the interrupt is really not
	 * for us before return.
	 */

	if (status == NO_INTERRUPT_PENDING) {
		intstatus = INWORD(AICREG(POST_STAT0));
		intstat = (UBYTE) intstatus;
		intstat1 = (UBYTE) (intstatus >> 8);
	
		c->intstat |= intstat;
		c->intstat1 |= intstat1;
		if (intstat & ALC_INT || intstat1 & (SEQ_INT|POST_STAT1_INTERRUPT)) {

			status = INTERRUPT_PENDING;

			if (intstat1 & HW_ERR_INT)
				c->error |= INBYTE(AICREG(ERROR));

			if (intstat1 & SEQ_LIP_INT)
				status = LONG_INTERRUPT_PENDING;

			if (c->error & MPARERR) {
				sh_pause_sequencer(c);
				OUTBYTE(AICREG(IPEN1), 
				(INBYTE(AICREG(IPEN1)) & ~HWERR_INTIPEN));
			}
		}
	}

	if (intstat)
		OUTBYTE(AICREG(CLRINT0),intstat);
	if (intstat1)
		OUTBYTE(AICREG(CLRINT1),intstat1);

	return (status);
}

/*
*
* sh_backend_isr
*
* Front end processing of ISR
*
* Return Value:  none
*		   
* Input Parameters: control_blk pointer
*/
void
sh_backend_isr(struct control_blk *c)
{
	register REGISTER base = c->base;
	register struct _doneq *d;
	UBYTE intstat, intstat1;
	void *ulm_info = c->ulm_info;

	intstat = c->intstat;
	intstat1 = c->intstat1;

	if (intstat1 & POST_STAT1_HW_BAD)
		sh_int_error(c, ulm_info);

	if (intstat & ALC_INT) {
		UBYTE mode;
		UBYTE alc_stat1, alc_lpsm_stat2;

		sh_pause_sequencer(c);

		mode = INBYTE(AICREG(MODE_PTR)) ;
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);

		alc_lpsm_stat2 = INBYTE(AICREG(ALC_LPSM_STATUS_2));
		if (alc_lpsm_stat2 & SYNC_SPIN)
			ulm_event(&c->ru, EVENT_SYNC_SPINDLE, ulm_info);
		if (alc_lpsm_stat2 & SYNC_CLK)
			ulm_event(&c->ru, EVENT_SYNC_CLOCK, ulm_info);

/*		read from the ALC register to see what is happening	*/
		alc_stat1 = INBYTE(AICREG(ALC_LPSM_STATUS_1));

		if (alc_stat1 & LPB_RCV) {
			c->ru.done_status = INBYTE(AICREG(ALC_SOURCE_PA));
			OUTBYTE(AICREG(ALC_LPSM_STATUS_1), LPB_RCV);
			ulm_event(&c->ru, EVENT_BYPASS, ulm_info);
		}

		if (alc_stat1 & LPE_RCV) {
			c->ru.done_status = INBYTE(AICREG(ALC_SOURCE_PA));
			OUTBYTE(AICREG(ALC_LPSM_STATUS_1), LPE_RCV);
			ulm_event(&c->ru, EVENT_ON_LOOP, ulm_info);
		}

		if (alc_stat1 & LIP_RCV) {

/*			the done_status will tell what kind of LIP received */
/*			and if we are not participating, we will give 0xff  */

			if ((alc_stat1 & LPSM_STATE_MSK) == OPEN_INIT)
				c->ru.done_status = (UBYTE) (INBYTE(AICREG(ALC_LPSM_STATUS_3)) 
							& ALC_LPSM_STATE);
			else
				c->ru.done_status = 0xff;

			if ((c->ru.done_status == ALC_LPSM_F7ALPS) ||
			    (c->ru.done_status == ALC_LPSM_F8ALPS) ||	   
			    (c->ru.done_status == ALC_LPSM_ALPDALPS)) 
				c->ru.func = INBYTE(AICREG(ALC_SOURCE_PA));

			if (c->lip_state == LIP_IN_PROCESS)
			{
				if ((alc_stat1 & LPSM_STATE_MSK) == OPEN_INIT)
				{	/* LIP received after LILP phase	*/
					c->flag |= LIP_DURING_LIP;
					sh_loadnrun_lip(&c->ru, c);
				}
			}
			else
				ulm_event(&c->ru, EVENT_FCAL_LIP, ulm_info);
		}

		if (!(c->flag & LIP_DURING_LIP)) {
			OUTBYTE(AICREG(MODE_PTR), mode);
			sh_unpause_sequencer(base);
		}
	}

	d = c->d;
	while (d->q_pass_count == c->q_done_pass) {

		c->ru.tcb_number = (USHORT) ((d->tcb_number_hi << 8) + d->tcb_number_lo);
		c->ru.done_status  = d->status;

#if SHIM_DEBUG
		if (c->lip_state == LIP_IN_PROCESS) {
				if (intstat1 != SEQ_INT) {
					ulm_printf("sh_isr: ");
				ulm_printf("d->tcb_number %d\n",c->ru.tcb_number);
				ulm_printf("During LIP: POST_STAT0 %x POST_STAT1 %x\n",
					intstat, intstat1);
				}
		}
#endif

		if (c->ru.tcb_number == PRIM_TCB) {
			OUTPCIBYTE(PCIREG(HST_LIP_CTL_HI), 0,ulm_info);
			if (c->flag & PRIMITIVE_LIP_SENT) {
				c->flag &= ~PRIMITIVE_LIP_SENT;
			}

			ulm_primitive_complete(&c->ru, ulm_info);

		} else if (c->lip_state == LIP_IN_PROCESS) {
			if (c->flag & PRIMITIVE_LIP_SENT) {
				c->flag &= ~PRIMITIVE_LIP_SENT;
				c->ru.tcb_number = 0;
				c->ru.done_status = 0;
				ulm_primitive_complete(&c->ru, ulm_info);
				c->ru.tcb_number = (USHORT) ((d->tcb_number_hi << 8) 
					+ d->tcb_number_lo);
				c->ru.done_status  = d->status;
			}
			switch (d->status) {

				case (LIP_TIMEOUT):
				case (LIP_NON_PARTICIPATING):
				case (LIP_HARDWARE_ERROR):
				case (LIP_OLD_PORT_STATE):
				case (LIP_UNEXPECTED_CLS):
					break;
				default:
					c->flag |= VALID_ALPA;
					c->our_aida0 = d->status;
#if SHIM_DEBUG
					ulm_printf("Got AL_PA %x\n",d->status);
#endif
					break;
			}
			sh_reload_mainline_firmware(c);
			d = c->d;
			ulm_tcb_complete(&c->ru, ulm_info);
			break;
		}else
			ulm_tcb_complete(&c->ru, ulm_info);

		d++;
		if (d >= c->qdone_end) {
			d = (struct _doneq *) c->s->doneq;
			c->q_done_pass++;
		}
	}

	c->d = d;

	if (intstat1 & SEQ_LIP_INT) {
		/* 
		 * When we came in SEQ_LIP_INT, the MTPE is ready for LIP 
		 * sequencer code. However, we should keep the MTPE code
		 * running as much as possible to handle the loop failure
		 * error condition.
		 */
		c->flag |= SEQUENCER_SUSPENDED;

		if ((c->lip_state == NO_LIP_IN_PROCESS) ||
		    (c->lip_state == WAITING_FOR_LIP_QUIESCE))
		{
			c->lip_state = READY_TO_LIP;
			ulm_event(&c->ru, EVENT_READY_FOR_LIP_TCB, ulm_info);
		}
	}

	if (intstat1 & CRTC_HST_INT) {
		/* 
		 * Handle possible real-time clock interrupt 
		 * Right now, the only time we use this timer is
		 * when the slim HIM is trying to avoid hardware
		 * ever going into the old port
		 */
#if SHIM_DEBUG
		ulm_printf("RTC interrupt\n");
#endif
	}

	c->intstat = c->intstat1 = 0;
}

void
sh_int_error(struct control_blk *c, void *ulm_info)
{
	register UBYTE intstat1 = c->intstat1;
	register REGISTER base = c->base;
	UBYTE err;
	UBYTE dma_error0, dma_error1, dma_error2;
	UBYTE old_mode;

	if (intstat1 & PCI_ERR_INT) {
		err = INPCIBYTE(PCIREG(STATUS1),ulm_info);

		dma_error0 = INPCIBYTE(PCIREG(DMA_ERROR0),ulm_info);
		dma_error1 = INPCIBYTE(PCIREG(DMA_ERROR1),ulm_info);
		dma_error2 = INPCIBYTE(PCIREG(DMA_ERROR2),ulm_info);

/*		Bit 0 - Data Parity Reported (DPR)			*/
		if (err & STATUS1_DPR) {
			if (((dma_error0 & DMA_ERROR0_DPR_MASK) == 0) &&
				((dma_error1 & DMA_ERROR1_DPR_MASK) == 0))
				sh_pci_errout(&c->ru, EVENT_PCI_STATUS1_DPR, ulm_info);
			else {
				if (dma_error0 & HR_DMA_DPR)
					sh_error(&c->ru, EVENT_PCI_ERROR0_HR_DMA_DPR, ulm_info);

				if (dma_error1 & CP_DMA_DPR)
					sh_error(&c->ru, EVENT_PCI_ERROR1_CP_DMA_DPR, ulm_info);
				if (dma_error1 & CIP_DMA_DPR)
					sh_error(&c->ru, EVENT_PCI_ERROR1_CIP_DMA_DPR, ulm_info);
			}
		}

/*		Signal Target Abort					*/
/*		STA: programmer asked us for something illegal		*/
/*		should only see during debug				*/
		if (err & STATUS1_STA)
			sh_error(&c->ru, EVENT_PCI_STATUS1_STA, ulm_info);

/*		RTA: We asked them to do st illegal			*/
/*		Again a programming error				*/
/*		Bit 4 - Reported Target Abort (RTA)			*/
		if (err & STATUS1_RTA) {
			if (((dma_error0 & DMA_ERROR0_RTA_MASK) == 0) &&
				((dma_error1 & DMA_ERROR1_RTA_MASK) == 0))
				sh_pci_errout(&c->ru, EVENT_PCI_STATUS1_RTA, ulm_info);
			else {
				if (dma_error0 & HS_DMA_RTA)
					sh_error(&c->ru, EVENT_PCI_ERROR0_HS_DMA_RTA, ulm_info);
				
				if (dma_error0 & HR_DMA_RTA)
					sh_error(&c->ru, EVENT_PCI_ERROR0_HR_DMA_RTA, ulm_info);

				if (dma_error1 & CP_DMA_RTA)
					sh_error(&c->ru, EVENT_PCI_ERROR1_CP_DMA_RTA, ulm_info);

				if (dma_error1 & CIP_DMA_RTA)
					sh_error(&c->ru, EVENT_PCI_ERROR1_CIP_DMA_RTA, ulm_info);
			}
		}

/*		Bit 5 - Reported Master Abort (RMA)			*/
/*		RMA: Bad address, or someone cannot accept our data.	*/
/*		tcb generation or SG list. Just report the error.	*/
		if (err & STATUS1_RMA) {
			if ((dma_error0 & DMA_ERROR0_RMA_MASK) == 0 &&
				(dma_error1 & DMA_ERROR1_RMA_MASK) == 0)
				sh_pci_errout(&c->ru, EVENT_PCI_STATUS1_RMA, ulm_info);
			else {
				if (dma_error0 & HS_DMA_RMA)
					sh_error(&c->ru, EVENT_PCI_ERROR0_HS_DMA_RMA, ulm_info);

				if (dma_error0 & HR_DMA_RMA)
					sh_error(&c->ru, EVENT_PCI_ERROR0_HR_DMA_RMA, ulm_info);

				if (dma_error1 & CP_DMA_RMA)
					sh_error(&c->ru, EVENT_PCI_ERROR1_CP_DMA_RMA, ulm_info);

				if (dma_error1 & CIP_DMA_RMA)
					sh_error(&c->ru, EVENT_PCI_ERROR1_CIP_DMA_RMA, ulm_info);
			}
		}

/*		#Serr asserted (system error)				*/
		if (err & STATUS1_SERR)
			sh_error(&c->ru, EVENT_PCI_STATUS1_SERR, ulm_info);


		if (err & STATUS1_DPE) {
/*			Bit 7 - Data Parity Error (DPE)			*/
			if (((dma_error0 & DMA_ERROR0_DPE_MASK) == 0) &&
				((dma_error1 & DMA_ERROR1_DPE_MASK) == 0) &&
				((dma_error2 & DMA_ERROR2_DPE_MASK) == 0))
				sh_pci_errout(&c->ru, EVENT_PCI_STATUS1_DPE, ulm_info);
			else {
				if (dma_error0 & HS_DMA_DPE)
					sh_error(&c->ru, EVENT_PCI_ERROR0_HS_DMA_DPE, ulm_info);

				if (dma_error1 & CP_DMA_DPE)
					sh_error(&c->ru, EVENT_PCI_ERROR1_CP_DMA_DPE, ulm_info);

				if (dma_error1 & CIP_DMA_DPE)
					sh_error(&c->ru, EVENT_PCI_ERROR1_CIP_DMA_DPE, ulm_info);

				if (dma_error2 & T_DPE)
					sh_error(&c->ru, EVENT_PCI_ERROR2_T_DPE, ulm_info);
			}
		}
	}

	if (intstat1 & HW_ERR_INT) {
		static int unpause_sequencer = 1;

		sh_pause_sequencer(c);
		old_mode = INBYTE(AICREG(MODE_PTR));

/*		Handle possible internal Emerald hardware errors	*/

		err = c->error;
		c->error = 0;

		if (err & ILLOPCODE) {
			sh_error(&c->ru, EVENT_SEQ_OP_CODE_ERR, ulm_info);
			unpause_sequencer = 0;
		}

		if (err & SQPARERR) {
			sh_error(&c->ru, EVENT_SEQ_PAR_ERR, ulm_info);
			unpause_sequencer = 0;
		}

		if (err & DRCPARERR)
			sh_error(&c->ru, EVENT_RPB_PAR_ERR, ulm_info);

		if (err & DSSPARERR)
			sh_error(&c->ru, EVENT_SPB_TO_SFC_PAR_ERR, ulm_info);

		if (err & DRHPARERR)
			sh_error(&c->ru, EVENT_RPB_TO_HST_PAR_ERR, ulm_info);

		if (err & CMC_RAM_PERR)
			sh_error(&c->ru, EVENT_CMC_RAM_PAR_ERR, ulm_info);

		if (err & MPARERR) {
/*			disable HW_ERR_INT				     */
				OUTBYTE(AICREG(IPEN1), 
				   (INBYTE(AICREG(IPEN1)) & ~HWERR_INTIPEN));
			OUTBYTE(AICREG(CMC_HCTL),
				INBYTE(AICREG(CMC_HCTL)) | HPAUSEACK);
			sh_error(&c->ru, EVENT_MEM_PORT_PAR_ERR, ulm_info);
			unpause_sequencer = 0;
		}

		if (err & CRPARERR)
			sh_error(&c->ru, EVENT_CMC_TO_HST_PAR_ERR, ulm_info);

/*		Look in the ALC to see how the LPSM is doing			*/
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);

		err = INBYTE(AICREG(ALC_LPSM_STATUS_3));

/*		MTPE will recover loop state, just warn the user		*/
		if (err & LPSM_ERR) {
			c->flag &= ~VALID_ALPA;
			sh_error(&c->ru, EVENT_LPSM_ERROR, ulm_info);
		}

/*		Look at SFC - also mode 1					*/
		err = INBYTE(AICREG(SERDES_STATUS));

/*		Only report LKNUSE error when the interrupt is enabled		*/
		if ((INBYTE(AICREG(SERDES_CTRL)) & LKNUSE_INT_EN) && (err & SERDES_LKNUSE))
			sh_error(&c->ru, EVENT_SERDES_LKNUSE, ulm_info);

		/* When MIA fault is detected, do not report the SERDES
		 * FAULT event to ULM unless hardware error interrupt is 
		 * enabled or MIA fault has not reported to ULM yet.
		 * Similiar approach is done for both Loss of Sync and
		 * Loss of Signal. loopfail_info in the control blk is used
		 * to determine whether those three events been reported
		 * to ULM or not.  It is set here when it is reported to ULM
		 * and it is reset when the loop becomes good.
		 */
		if (err & SERDES_FAULT) 
		{
			OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
			if (INBYTE(AICREG(IPEN1)) & HWERR_INTIPEN)
			{
				OUTBYTE(AICREG(IPEN1), 
				   (INBYTE(AICREG(IPEN1)) & ~HWERR_INTIPEN));
				c->loopfail_info |= MIA_FAULT_reported;
				sh_error(&c->ru, EVENT_SERDES_FAULT, ulm_info);
			}
			else if (!(c->loopfail_info & MIA_FAULT_reported))
			{
				c->loopfail_info |= MIA_FAULT_reported;
				sh_error(&c->ru, EVENT_SERDES_FAULT, ulm_info);
			}
		}

		/* Goto scratch where MTPE has provided info about loop 
		 * status.  If there is no error status provided by MTPE,
		 * Look at RFC for more information.  It is done this way
		 * because there could be some time that MTPE can not go
		 * to the tight loop and try to recover the loop.  Good
		 * example is during LIP.
		 */

		err = INBYTE(SEQMEM(LINK_ERR_STATUS));
		if (!(err & (LOSIG|LOSYNTOT)))
		{
			OUTBYTE(AICREG(MODE_PTR), CMC_MODE2);
			err |= INBYTE(AICREG(LINK_ERRST));
		}
		else
			OUTBYTE(SEQMEM(LINK_ERR_STATUS), 0);

		if (err & LOSIG)
		{
			OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
			if (INBYTE(AICREG(IPEN1)) & HWERR_INTIPEN)
			{
				OUTBYTE(AICREG(IPEN1), 
				   (INBYTE(AICREG(IPEN1)) & ~HWERR_INTIPEN));
				c->loopfail_info |= LOSignal_reported;
				if (c->seq_type == SH_LIP_SEQ_CODE)
					sh_error(&c->ru,
						EVENT_LINK_ERRST_LOSIG_LIP,
						ulm_info);
				else
					sh_error(&c->ru,
						EVENT_LINK_ERRST_LOSIG,
						ulm_info);
			}
			else if (!(c->loopfail_info & LOSignal_reported))
			{
				c->loopfail_info |= LOSignal_reported;
				if (c->seq_type == SH_LIP_SEQ_CODE)
					sh_error(&c->ru,
						EVENT_LINK_ERRST_LOSIG_LIP,
						ulm_info);
				else
					sh_error(&c->ru,
						EVENT_LINK_ERRST_LOSIG,
						ulm_info);
			}
		}
		
		if (err & LOSYNTOT)
		{
			OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
			if (INBYTE(AICREG(IPEN1)) & HWERR_INTIPEN)
			{
				OUTBYTE(AICREG(IPEN1), 
				   (INBYTE(AICREG(IPEN1)) & ~HWERR_INTIPEN));
				c->loopfail_info |= LOSync_reported;
				if (c->seq_type == SH_LIP_SEQ_CODE)
					sh_error(&c->ru,
						EVENT_LINK_ERRST_LOSYNTOT_LIP,
						ulm_info);
				else
					sh_error(&c->ru, EVENT_LINK_ERRST_LOSYNTOT,
						ulm_info);
			}
			else if (!(c->loopfail_info & LOSync_reported))
			{
				c->loopfail_info |= LOSync_reported;
				if (c->seq_type == SH_LIP_SEQ_CODE)
					sh_error(&c->ru,
						EVENT_LINK_ERRST_LOSYNTOT_LIP,
						ulm_info);
				else
					sh_error(&c->ru, EVENT_LINK_ERRST_LOSYNTOT,
						ulm_info);
			}

		}
/*		Restore the former mode					*/
		OUTBYTE(AICREG(MODE_PTR), old_mode);
		if (unpause_sequencer)
			sh_unpause_sequencer(base);
	}

	if (intstat1 & SW_ERR_INT)
/*	Handle possible software errors					*/
#if SHIM_DEBUG
		ulm_printf("Software error\n");
#endif


	if (intstat1 & EXT_INT) {
/*	Handle possible unknown error from EXT_INT pin			*/
#if SHIM_DEBUG
		ulm_printf("EXT_INT error\n");
#endif
	}
}

static void
sh_pci_errout(struct reference_blk *r, INT err, void *ulm_info)
{
#if SHIM_DEBUG
	ulm_printf("Inconsistent PCI error reporting for type %s\n",
		sh_event_strings[err]);
#endif
	ulm_event(r, err, ulm_info);
}

static void
sh_error(struct reference_blk *r, INT err, void *ulm_info)
{
#if SHIM_DEBUG
	ulm_printf("%d %s\n", err, sh_event_strings[err]);
#endif
	ulm_event(r, err, ulm_info);
}
