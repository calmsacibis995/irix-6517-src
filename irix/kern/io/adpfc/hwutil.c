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

/*
*
* Module Name: hwutil.c
*
* Description:
*		Code to implement utility for hardware management module
*
* Owners:	ECX IC Firmware Team
*    
* Notes:	Utilities defined in this file should be generic and
*		primitive. This is the only module that includes the
*		mainline sequencer code.
*
* Modules:	sh_pausetop
*		sh_unpause_sequencer
*		sh_enable_access
*		sh_load_sequencer
*		sh_access_scratch
*		sh_save_state
*		sh_restore_state
*		sh_disable_irq
*		sh_enable_irq
*		sh_interrupt_pending
*		sh_setup_emerald_software
*		sh_start_mainline_sequencer
*		sh_start_sequencer()
*
*/

#include "hwgeneral.h"
#include "hwcustom.h"
#include "ulmconfig.h"
#include "hwequ.h"
#include "hwref.h"
#include "sequence.h"
#include "seq_off.h"
#include "hwtcb.h"
#include "hwdef.h"
#include "hwproto.h"

/*********************************************************************
*
*  sh_pausetop
*
*     Pause sequencer and wait for hpauseack
*
*  Return Value:  none
*
*  Parameters:	  register base address
*
*  Remarks:	This function only works with mainline sequencer code
*  Warning:
*		If this function is called when the sequencer is
*		already paused, the desired result will not occur.
*		This function cannot release the HPAUSE bit, so
*		if sh_pausetop is called when HPAUSE is on, both
*		HPAUSE and HPAUSETOP will be set. The only way to
*		get the sequencer correctly running again is to call
*		sh_unpause_sequencer, which turns off both bits,
*		then call sh_unpause_sequencer.
*		   
*********************************************************************/
void
sh_pausetop(struct control_blk *c)
{
	register REGISTER base = c->base;
	UINT timeout = 10000;

	if (c->seq_type != SH_NORMAL_SEQ_CODE)
		return;

	OUTBYTE(AICREG(CMC_HCTL),
		(INBYTE(AICREG(CMC_HCTL)) | HPAUSETOP));

	while(!(INBYTE(AICREG(CMC_HCTL)) & HPAUSETOPACK)) {
		if(timeout-- <= 0) {
			sh_pause_sequencer(c);
			break;
		}
	}	
}

/*
*
*	sh_unpause_sequencer
*
*     UnPause sequener chip
*
*  Return Value:  none
*
*  Parameters:	  register base address
*
*  Remarks:
*		   
*/

void
sh_unpause_sequencer(register REGISTER base)
{
	OUTBYTE(AICREG(CMC_HCTL),
		(INBYTE(AICREG(CMC_HCTL)) & ~(HPAUSETOP|HPAUSE)));

}
#if 0
/*
*
*	sh_enable_access
*
*     Enable Access to Emerald device space registers
*
*
*  Return Value:1 - device space access was enabled
*		0 - device space access was disabled
*
*  Parameters:	host task set handle
*		void *ulm_info
*
*  Remarks:	This routine accesses PCI config space on
*		our board only via memory mapping.
*		This should only be done for debug purposes,
*		This should never be done in production code.
*		   
*/
/*ARGSUSED*/
INT
sh_enable_access (register REGISTER base, void* ulm_info)
{
	UBYTE value;

	/* read current command register value */
	value = INPCIBYTE(PCIREG(COMMAND0),ulm_info);

	/* check if memory space enabled */
	if (value & (MASTEREN + MSPACEEN)) {
		return(1);
	}

	/* turn on device memory space access */
	value |= (UBYTE) (MASTEREN | MSPACEEN | 0x40) ;
	OUTPCIBYTE(PCIREG(COMMAND0),value,ulm_info);
  
	return(0);
}
#endif
/*
*
* sh_load_sequencer
*
* Load sequencer code
*
* Return Value: 0 - sequencer code loaded and verified
*		others - sequencer code verification failed
*
* Parameters:	base
*		address of sequencer code to be loaded
*		size of sequencer code 
*
* Remarks:
*		   
*/
INT
sh_load_sequencer (struct control_blk *c, INT type)
{
	register REGISTER base = c->base;
	INT cnt, seq_words;
	USHORT *p, *start;
	USHORT s;
	UBYTE val;

	val = INBYTE(AICREG(CMC_HCTL));
	if (!(val & HPAUSE))
		OUTBYTE(AICREG(CMC_HCTL), (val | HPAUSE));

	switch (type) {

		case SH_NORMAL_SEQ_CODE:
			start = p = (USHORT *) sh_get_vaddr_mainline_code();
			seq_words = (sizeof(sh_mainline_code)) / 
				(sizeof (USHORT));

			if (c->flag & VALID_ALPA)	
				c->lip_state = NO_LIP_IN_PROCESS;
			else
				c->lip_state = NOT_READY_FOR_LIP;

			if (c->flag & BACK_TO_LIP)
				return(0);
			break;
	
		case SH_LIP_SEQ_CODE:
			start = p = (USHORT *) sh_get_vaddr_lip_code();
			seq_words = (sh_get_size_lip_code() / sizeof (USHORT));
			break;

		default:
			return (-1);
	}
	
/*	write out sequencer code					*/
	OUTBYTE(AICREG(SEQCTL), SEQCTL_LOADRAM);

	OUTWORD(AICREG(SEQADDR), 0x00);
	
	for (cnt = 0; cnt < seq_words; cnt++,p++) {
#ifdef BIOS
		SetCurrentDS();
#endif
		OUTWORD(AICREG(SEQRAM), ULM_SWAP_SHORT(*p));
#ifdef BIOS
		RestoreCurrentDS();
#endif
		if (!(cnt & 0x7f) && sh_timer_checking(c))
			sh_check_loopfailure(c);
	}

/*	padding to always write valid data out to 4 byte boundary	*/
	OUTWORD(AICREG(SEQRAM), 0);
	OUTWORD(AICREG(SEQRAM), 0);
	OUTWORD(AICREG(SEQRAM), 0);
	OUTWORD(AICREG(SEQRAM), 0);
	   
/*	and verify							*/

	OUTBYTE(AICREG(SEQCTL), SEQCTL_LOADRAM);

	OUTWORD(AICREG(SEQADDR), 0x00);
	p = start;
	
	for (cnt = 0; cnt < seq_words; cnt++) {
#ifdef BIOS
		RestoreCurrentDS();
#endif
		if (!(cnt & 0x7f) && sh_timer_checking(c))
			sh_check_loopfailure(c);
#ifdef BIOS
		SetCurrentDS();
#endif
		s = INWORD(AICREG(SEQRAM));
		s = ULM_SWAP_SHORT(s);
		if (s != *p) {
#if SHIM_DEBUG
			ulm_printf("read %x expected %x for word %d\n",
				s, *p, cnt);
#endif
#ifdef BIOS
			RestoreCurrentDS();
#endif
			OUTBYTE(AICREG(SEQCTL), SEQCTL_RUN);
			return(-1);
		}
#if SHIM_DEBUG
		if (INBYTE(AICREG(ERROR)) & SQPARERR) {
			ulm_printf("ERROR reg while verifying SEQ code is %x\n",
			INBYTE(AICREG(ERROR)));
			OUTBYTE(AICREG(SEQCTL), SEQCTL_RUN);
			return(-1);
		}
#endif
		p++;
	}

	OUTWORD(AICREG(SEQADDR), 0x00);
	OUTBYTE(AICREG(SEQCTL), SEQCTL_RUN);
#ifdef BIOS
	RestoreCurrentDS();
#endif
	c->seq_type = (UBYTE) type;
	
	return(0);
}

/*
*
* sh_access_scratch()
*
* read/write scratch ram byte
*
* Return Value:
*
* Parameters:	struct access_scratch_ram*
*
* Remarks:	won't check Sram existance; CHIPRST bit could be cleared
*		   
*/
#ifndef BIOS
void
sh_access_scratch (struct access_scratch_ram *c)
{
	register REGISTER base = c->base;
	UBYTE mode, size_select, val;

	/* save bit states for later restore (no CHIPRST) */
	val = (UBYTE) (INBYTE(AICREG(CMC_HCTL)) & (INTEN | HPAUSE | HPAUSETOP));

	while (!(INBYTE(AICREG(CMC_HCTL)) & (HPAUSEACK | HPAUSETOPACK)))
		OUTBYTE(AICREG(CMC_HCTL), (INBYTE(AICREG(CMC_HCTL)) | HPAUSE));

	/* save mode for later restore (no CHIPRST) */
	mode = INBYTE(AICREG(MODE_PTR));
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);

	/* the following four lines should be consitent with sh_get_config() */
#ifdef _ADDRESS_WIDTH32
	/* 4 byte PCI, 4 PVAR pages, 0 EVAR */
	size_select = SGEL_SIZE16 | PVAR_PAGE4;
#else
	/* 8 byte PCI, 4 PVAR pages, 0 EVAR */
	size_select = SGEL_SIZE8 | PVAR_PAGE4;
#endif
	OUTBYTE(AICREG(SIZE_SELECT), size_select);

	OUTBYTE(AICREG(PVARPTR), 0);

	if (c->rw == 0)
		c->rwval = INBYTE(SEQMEM(c->pvar_offset));
	else
		OUTBYTE(SEQMEM(c->pvar_offset), c->rwval);

	OUTBYTE(AICREG(MODE_PTR), mode);

	OUTBYTE(AICREG(CMC_HCTL), val);

	/* validate write */
	INBYTE(AICREG(CMC_HCTL));
	return;
}


/*
*
* sh_restore_sequencer()
*
* Restore sequencer code
*
* Return Value: 0 - sequencer code restored and verified ok
*		others - sequencer code verification failed
*
* Parameters:	shim_config*
*		address of sequencer code to be restored
*
* Remarks:
*		   
*/
INT
sh_restore_sequencer(struct shim_config *s, void *p)
{
	register REGISTER base = s->base_address;
	INT    i, seq_words = 4096 / sizeof(USHORT);
	USHORT st, *pRestore;

/*	write out sequencer code					*/
	OUTBYTE(AICREG(SEQCTL), SEQCTL_LOADRAM);
	OUTWORD(AICREG(SEQADDR), 0x00);
	
	pRestore = p;
	for (i = 0; i < seq_words; i++)
		OUTWORD(AICREG(SEQRAM), *pRestore++);

/*	and verify							*/
	OUTWORD(AICREG(SEQADDR), 0x00);

	pRestore = p;
	for (i = 0; i < seq_words; i++) {
		st = INWORD(AICREG(SEQRAM));
		if (st != *pRestore) {
			OUTBYTE(AICREG(SEQCTL), SEQCTL_RUN);
			return(-1);
		}
		pRestore++;
	}
	
	OUTWORD(AICREG(SEQADDR), 0x00);
	OUTBYTE(AICREG(SEQCTL), SEQCTL_RUN);
	
	return(0);
}

/*
*
* sh_save_sequencer()
*
* save sequencer code
*
* Return Value: 0 - sequencer code saved and verified ok
*		others - sequencer code verification failed
*
* Parameters:	shim_config*
*		address of sequencer code to be saved
*
* Remarks:
*		   
*/
INT
sh_save_sequencer(struct shim_config *s, void *p)
{
	register REGISTER base = s->base_address;
	INT    i, seq_words = 4096 / sizeof(USHORT);
	USHORT st, *pSave;

/*	write out sequencer code					*/
	OUTBYTE(AICREG(SEQCTL), SEQCTL_LOADRAM);
	OUTWORD(AICREG(SEQADDR), 0x00);
	
	pSave = p;
	for (i = 0; i < seq_words; i++)
		*pSave++ = INWORD(AICREG(SEQRAM));

/*	and verify							*/
	OUTWORD(AICREG(SEQADDR), 0x00);

	pSave = p;
	for (i = 0; i < seq_words; i++) {
		st = INWORD(AICREG(SEQRAM));
		if (st != *pSave) {
			OUTBYTE(AICREG(SEQCTL), SEQCTL_RUN);
			return(-1);
		}
		pSave++;
	}

	OUTWORD(AICREG(SEQADDR), 0x00);
	OUTBYTE(AICREG(SEQCTL), SEQCTL_RUN);
	
	return(0);
}

/*
*
* sh_restore_TCBs()
*
* restore TCBs
*
* Return Value: 0 - TCBs restored ok
*		others - restored TCBs verification failed
*
* Parameters:	shim_config*
*		address for TCBs to be restored
*
* Remarks:
*		   
*/
INT
sh_restore_TCBs(struct shim_config *c, void *p)
{
	register REGISTER base = c->base_address;
	INT i, j;
	UBYTE *pRestoreTCB;
	
	pRestoreTCB = p;		
	for (i = 0; i < SAVE_RESTORE_TCB; i++)		/* restore 3 TCBs */
	{
		OUTWORD(AICREG(TCBPTR), i);
		for (j = 0; j < 128; j++)
			OUTBYTE(TCBMEM(TCB_DAT0 + j), *pRestoreTCB++);

		OUTBYTE(AICREG(SG_CACHEPTR), 0);
		for (j = 0; j < 128; j++) 
			OUTBYTE(AICREG(SGEDAT0), *pRestoreTCB++);
	 }

	/* verify TCBs and S/G RAM			*/
	pRestoreTCB = p;		
	for (i = 0; i < SAVE_RESTORE_TCB; i++)
	{
		OUTWORD(AICREG(TCBPTR), i);
		for (j = 0; j < 128; j++)
			if (INBYTE(TCBMEM(TCB_DAT0 + j)) != *pRestoreTCB++)
					return(-1);
		
		OUTBYTE(AICREG(SG_CACHEPTR), 0);
		for (j = 0; j < 128; j++)
			if (INBYTE(AICREG(SGEDAT0)) != *pRestoreTCB++)
					return(-1);
	}

	return(0);
}

/*
*
* sh_save_TCBs()
*
* save TCBs
*
* Return Value: 0 - TCBs saved ok
*		others - saved TCBs verification failed
*
* Parameters:	shim_config*
*		address for TCBs to be saved
*
* Remarks:
*		   
*/
INT
sh_save_TCBs(struct shim_config *c, void *p)
{
	register REGISTER base = c->base_address;
	INT i, j;
	UBYTE *pSaveTCB;
	
	pSaveTCB = p;		     
	for (i = 0; i < SAVE_RESTORE_TCB; i++)		/* just save 3 TCBs */
	{
		OUTWORD(AICREG(TCBPTR), i);
		for (j = 0; j < 128; j++)
			*pSaveTCB++ = INBYTE(TCBMEM(TCB_DAT0 + j));

		OUTBYTE(AICREG(SG_CACHEPTR), 0);
		for (j = 0; j < 128; j++) 
			*pSaveTCB++ = INBYTE(AICREG(SGEDAT0));
	 }

	/* verify TCBs and S/G RAM			*/
	pSaveTCB = p;		     
	for (i = 0; i < SAVE_RESTORE_TCB; i++)
	{
		OUTWORD(AICREG(TCBPTR), i);
		for (j = 0; j < 128; j++)
			if (INBYTE(TCBMEM(TCB_DAT0 + j)) != *pSaveTCB++)
					return(-1);
		
		OUTBYTE(AICREG(SG_CACHEPTR), 0);
		for (j = 0; j < 128; j++)
			if (INBYTE(AICREG(SGEDAT0)) != *pSaveTCB++)
					return(-1);
	}

	return(0);
}

/*
*
* sh_restore_scratch()
*
* restore scratch
*
* Return Value: 0 - scratch restored OK
*		others - restored scratch verification failed
*
* Parameters:	shim_config*
*		address for scratch to be restored
*
* Remarks:
*		   
*/
INT
sh_restore_scratch(struct shim_config *c, void *p)
{
	register REGISTER base = c->base_address;
	INT i;
	UBYTE *pRestoreScratch;

	OUTBYTE(AICREG(PVARPTR), 0);

	pRestoreScratch = p;
	for (i = 0; i < 128; i++)
		OUTBYTE(SEQMEM(PVAR_DAT0 + i), *pRestoreScratch++);

	pRestoreScratch = p;
	for (i = 0; i < 128; i++)
		if (INBYTE(SEQMEM(PVAR_DAT0 + i)) != *pRestoreScratch++)
				return(-1);
	
	return(0);
}

/*
*
* sh_save_scratch()
*
* save scratch
*
* Return Value: 0 - scratch saved OK
*		others - saved scratch verification failed
*
* Parameters:	shim_config
*		address for scratch to be saved
*
* Remarks:
*		   
*/
INT
sh_save_scratch(struct shim_config *c, void *p)
{
	register REGISTER base = c->base_address;
	INT i;
	UBYTE *pSaveScratch;

	OUTBYTE(AICREG(PVARPTR), 0);

	pSaveScratch = p;
	for (i = 0; i < 128; i++)
		*pSaveScratch++ = INBYTE(SEQMEM(PVAR_DAT0 + i));

	pSaveScratch = p;
	for (i = 0; i < 128; i++)
		if (INBYTE(SEQMEM(PVAR_DAT0 + i)) != *pSaveScratch++)
				return(-1);
	
	return(0);
}

/*
*
* sh_restore_state()
*
*
* Return Value: 0 - states restored OK
*		others - restored states verification failed
*
* Parameters:	shim_config*
*		address for emerald states
*
* Remarks:
*		   
*/
INT
sh_restore_state(struct shim_config *s, void *p)
{
	register REGISTER base = s->base_address;
	struct emerald_states *pStates = (struct emerald_states *) p;

	while (!(INBYTE(AICREG(CMC_HCTL)) & HPAUSEACK))
		OUTBYTE(AICREG(CMC_HCTL), (INBYTE(AICREG(CMC_HCTL)) | HPAUSE));
		
	if (sh_restore_sequencer(s, (void *) pStates->mtpeRam))
		return(-1);
	if (sh_restore_scratch(s, (void *) pStates->scratch))
		return(-1);

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE2);
	if (sh_restore_TCBs(s, (void *) pStates->tcb))
		return(-1);

/*	  OUTBYTE(AICREG(RPB_PLD_SIZE), pStates->rpbPldSize); */ /* mode2 */

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
	OUTWORD(AICREG(SNTCB_QOFF0), pStates->sntcbQoff);   
	OUTWORD(AICREG(SDTCB_QOFF0), pStates->sdtcbQoff);
	OUTWORD(AICREG(HNTCB_QOFF0), pStates->sntcbQoff);
	OUTWORD(AICREG(TILPADDR0),   pStates->tilpAddr);

	OUTBYTE(AICREG(CMC_HST_RTC1), 0);	/* disable before writing address */

	OUTBYTE(AICREG(CIP_DMA_LADR0+0), pStates->cipDmaAddr[0]);
	OUTBYTE(AICREG(CIP_DMA_LADR0+1), pStates->cipDmaAddr[1]);
	OUTBYTE(AICREG(CIP_DMA_LADR0+2), pStates->cipDmaAddr[2]);
	OUTBYTE(AICREG(CIP_DMA_LADR0+3), pStates->cipDmaAddr[3]);
	OUTBYTE(AICREG(CIP_DMA_LADR0+4), pStates->cipDmaAddr[4]);
	OUTBYTE(AICREG(CIP_DMA_LADR0+5), pStates->cipDmaAddr[5]);
	OUTBYTE(AICREG(CIP_DMA_LADR0+6), pStates->cipDmaAddr[6]);
	OUTBYTE(AICREG(CIP_DMA_LADR0+7), pStates->cipDmaAddr[7]);

	OUTBYTE(AICREG(IPEN0), pStates->ipen[0]);
	OUTBYTE(AICREG(IPEN1), pStates->ipen[1]);
	
	OUTBYTE(AICREG(CMC_HST_RTC1), CIP_DMAEN);		/* always enabled */

	OUTBYTE(AICREG(SEQCTL), SEQCTL_RESET);

/*	  if (pStates->tilpAddr == 0)
		OUTBYTE(AICREG(SEQCTL), SEQCTL_RESET);
	else {
		OUTWORD(AICREG(SEQADDR0), (USHORT) (pStates->tilpAddr-1));
	}
	
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE2);
*/

/*	unpause the sequencer and restore INTEN				*/
	OUTBYTE(AICREG(CMC_HCTL), ~(HPAUSE|HPAUSETOP)&pStates->cmcHctlInten);

/*	validate writes							*/
	INBYTE(AICREG(CMC_HCTL));

	return(0);
}

/*
*
* sh_save_state()
*
*
* Return Value: 0 - states saved OK
*		others - saved states verification failed
*
* Parameters:	shim_config*
*		address for emerald states
*
* Remarks:
*		   
*/
INT
sh_save_state(struct shim_config *s, void *p)
{
	register REGISTER base = s->base_address;
	struct emerald_states *pStates = (struct emerald_states *) p;
	UINT timeout = 10000;
/*	  USHORT  seqAddr;	*/
	UBYTE cmcHctl, mode;

	cmcHctl = INBYTE(AICREG(CMC_HCTL));
	pStates->cmcHctlInten = (UBYTE) (cmcHctl & INTEN);
	if ((cmcHctl & HPAUSE) == 0) {
		OUTBYTE(AICREG(CMC_HCTL), HPAUSETOP);
		while (!(INBYTE(AICREG(CMC_HCTL)) & (HPAUSETOPACK | HPAUSE))) {
			if(timeout-- <= 0) return(-1);
		}
	}	

	mode = INBYTE(AICREG(MODE_PTR));
/*	  seqAddr  = INWORD(AICREG(SEQADDR0));	*/
		
	if (sh_save_sequencer(s, (void *) pStates->mtpeRam))
		return(-1);
	if (sh_save_scratch(s, (void *) pStates->scratch))
		return(-1);

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE2);
	if (sh_save_TCBs(s, (void *) pStates->tcb))
		return(-1);

/*	  pStates->rpbPldSize = INBYTE(AICREG(RPB_PLD_SIZE)); */	/* mode2 */

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
	pStates->sntcbQoff = INWORD(AICREG(SNTCB_QOFF0));   
	OUTWORD(AICREG(SNTCB_QOFF0), pStates->sntcbQoff);   
	pStates->sdtcbQoff = INWORD(AICREG(SDTCB_QOFF0));
	OUTWORD(AICREG(SDTCB_QOFF0), pStates->sdtcbQoff);   
	pStates->tilpAddr  = INWORD(AICREG(TILPADDR0));

	pStates->ipen[0] = INBYTE(AICREG(IPEN0));
	pStates->ipen[1] = INBYTE(AICREG(IPEN1));
	
	pStates->cipDmaAddr[0] = INBYTE(AICREG(CIP_DMA_LADR0+0));
	pStates->cipDmaAddr[1] = INBYTE(AICREG(CIP_DMA_LADR0+1));
	pStates->cipDmaAddr[2] = INBYTE(AICREG(CIP_DMA_LADR0+2));
	pStates->cipDmaAddr[3] = INBYTE(AICREG(CIP_DMA_LADR0+3));
	pStates->cipDmaAddr[4] = INBYTE(AICREG(CIP_DMA_LADR0+4));
	pStates->cipDmaAddr[5] = INBYTE(AICREG(CIP_DMA_LADR0+5));
	pStates->cipDmaAddr[6] = INBYTE(AICREG(CIP_DMA_LADR0+6));
	pStates->cipDmaAddr[7] = INBYTE(AICREG(CIP_DMA_LADR0+7));

	OUTBYTE(AICREG(MODE_PTR), mode);
	OUTBYTE(AICREG(SEQCTL), SEQCTL_RESET);

/*	  if (seqAddr == 0)
		OUTBYTE(AICREG(SEQCTL), SEQCTL_RESET);
	else {
		OUTWORD(AICREG(SEQADDR0), (USHORT) (seqAddr-1));
	}
*/

	OUTBYTE(AICREG(CMC_HCTL), cmcHctl & ~CHIPRST);

/*	validate writes							*/
	INBYTE(AICREG(CMC_HCTL));

	return(0);
}

/*
*
*	sh_enable_TCB_ChkSum
*
* enable MTPE TCB check sum test
*
* Return Value:  None
*		   
* Parameters:	 *shim_config
*
* Remarks:
*		   
*/
INT
sh_enable_TCB_ChkSum(struct shim_config *c)
{
	register REGISTER base =  c->base_address;
	UINT timeout = 10000;
	UBYTE cmcHctl, val;

	cmcHctl = INBYTE(AICREG(CMC_HCTL));
	if ((cmcHctl & HPAUSE) == 0) {
		OUTBYTE(AICREG(CMC_HCTL), HPAUSETOP);
		while (!(INBYTE(AICREG(CMC_HCTL)) & (HPAUSETOPACK | HPAUSE))) {
			if(timeout-- <= 0) return(-1);
		}
	}	

	val = (SET_BRK_ADDR0 >> 2) & 0xff;
	OUTBYTE(AICREG(BRKADDR0), val);
	val = (SET_BRK_ADDR0 >> 10) & 0xff;
	val |= (INBYTE(AICREG(BRKADDR1)) & ~(BRKDIS|0x03));
	OUTBYTE(AICREG(BRKADDR1), val);
	OUTBYTE(SEQMEM(ENABLE_TCB_CHECKSUM),(UBYTE) 0xff);

	OUTBYTE(AICREG(CMC_HCTL), cmcHctl);

/*	validate writes							*/
	INBYTE(AICREG(CMC_HCTL));

	return(0);
}

/*
*
*	sh_disable_TCB_ChkSum
*
* disable MTPE TCB check sum test
*
* Return Value:  None
*		   
* Parameters:	 *shim_config
*
* Remarks:
*		   
*/
INT
sh_disable_TCB_ChkSum(struct shim_config *c)
{
	register REGISTER base =  c->base_address;
	UINT timeout = 10000;
	UBYTE cmcHctl;

	cmcHctl = INBYTE(AICREG(CMC_HCTL));
	if ((cmcHctl & HPAUSE) == 0) {
		OUTBYTE(AICREG(CMC_HCTL), HPAUSETOP);
		while (!(INBYTE(AICREG(CMC_HCTL)) & (HPAUSETOPACK | HPAUSE))) {
			if(timeout-- <= 0) return(-1);
		}
	}	

	OUTBYTE(SEQMEM(ENABLE_TCB_CHECKSUM),(UBYTE) 0);
	OUTBYTE(AICREG(BRKADDR0), 0);
	OUTBYTE(AICREG(BRKADDR1), 0x80);

	OUTBYTE(AICREG(CMC_HCTL), cmcHctl);

/*	validate writes							*/
	INBYTE(AICREG(CMC_HCTL));

	return(0);
}

#endif
/*
*
*	sh_disable_irq
*
* Disable device hardware interrupt
*
* Return Value:  None
*		   
* Parameters:	 *control_blk
*
* Remarks:
*		   
*/
void
sh_disable_irq (struct control_blk *c)
{
	register REGISTER base = c->base;

	/* program hardware to disable interrupt */
	OUTBYTE(AICREG(CMC_HCTL),INBYTE(AICREG(CMC_HCTL)) & ~INTEN);
}

/*
*
*	sh_enable_irq
*
* Enable device hardware interrupt
*
* Return Value:  None
*		   
* Parameters:	 *control_blk
*
* Remarks:
*		   
*/
void
sh_enable_irq(struct control_blk *c)
{
	register REGISTER base = c->base;

	/* program hardware to enable interrupt */
	OUTBYTE(AICREG(CMC_HCTL),
	INBYTE(AICREG(CMC_HCTL)) | INTEN);
}

/*
*
*	sh_interrupt_pending
*
* See if interrupt pending for associated device hardware
*
* Return Value: 2 bytes of interrupt status from POST_STAT0 and 1
*		   
* Parameters:	*control_blk
*
* Remarks:	return with interrupt status (bottom 2 bytes are valid).
*		   
*/
INT
sh_interrupt_pending(struct control_blk *c)
{
	register REGISTER base = c->base;

	return (INT) (INWORD(AICREG(POST_STAT0)) & 0xff08);
}
#ifndef BIOS
INT
sh_setup_loop_bypass(struct control_blk *c)
{
	register REGISTER base = c->base;
	UBYTE lpsm_cmd;

/*	For now, since there is no bypass hardware			*/
/*	use the ALC to put the chip into bypass mode			*/
	sh_pause_sequencer(c);

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);
	lpsm_cmd = INBYTE(AICREG(ALC_LPSM_CMD));
	lpsm_cmd &= 0xe0;
	lpsm_cmd |= REQ_BYPASS;
	OUTBYTE(AICREG(ALC_LPSM_CMD), lpsm_cmd);
/*	we do not set NON_ACTIVE so that primitives are ignored		*/
/*	this means another initiator cannot send an effective LPE	*/

	sh_unpause_sequencer(base);
	return (0);
}

INT
sh_setup_internal_loopback(struct control_blk *c)
{
	register REGISTER base = c->base;
	UBYTE u;
	sh_pause_sequencer(c);

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE3);
	OUTBYTE(AICREG(RFC_MODE), INTLPBK);

	OUTBYTE(AICREG(LINK_ERRST), 0x20); /* reset */
	OUTBYTE(AICREG(LINK_ERRST), 0x40); /* enable */

	
	u = INBYTE(AICREG(LINK_ERRST));
	sh_unpause_sequencer(base);
	if (u != 0x40) {
#if SHIM_DEBUG
		ulm_printf("RFC internal_loopback reset fail\n");
#endif
		return (-1);
	} else
		return (0);
}

/*
*
*	sh_shutdown
*
* Shutdown all activity on AIC1160
*
* Return Value: 
*		   
* Parameters:	*control_blk
*		option	0   --	Enable internal bypass
*			1   --	Turnoff the transmit clock
*
* Remarks:	
*		   
*/
void
sh_shutdown(struct control_blk *c, UBYTE option)
{
	register REGISTER base = c->base;
	UBYTE val;

	if (c->seq_type == SH_NORMAL_SEQ_CODE)
		sh_pausetop(c);

/*	turn off interrupts and ensure sequencer has stopped		*/
	OUTBYTE(AICREG(CMC_HCTL), HPAUSE);
	
	if (option)
	{	/* Shutdown the transmit clock	*/
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
		val = (UBYTE) (INBYTE(AICREG(CLK_GEN_CTL)) | 0x03);
		OUTBYTE(AICREG(CLK_GEN_CTL), val);

/* NEW */
		/* put in loopback mode */
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE5);
		val = (UBYTE) INBYTE(AICREG(SERDES_CTRL)) & 0xFD;
		OUTBYTE(AICREG(SERDES_CTRL), val | 1);
	}

/*	clear the interrupts so sh_interrupt_pending is valid		*/
	val = INBYTE(AICREG(POST_STAT0));
	OUTBYTE(AICREG(POST_STAT0), val);
	val = INBYTE(AICREG(POST_STAT1));
	OUTBYTE(AICREG(POST_STAT1), val);

/*	use the ALC to put the chip into bypass mode			*/
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);
	val = INBYTE(AICREG(ALC_LPSM_CMD));
	val &= 0xe0;
	val |= REQ_BYPASS;
	OUTBYTE(AICREG(ALC_LPSM_CMD), val);

/*	we do not set NON_ACTIVE so that primitives are ignored */

	c->seq_type = SH_NO_SEQ_CODE;
}
#endif
/*
*
* sh_setup_emerald_software
*
* Reset channel software
*
* Return Value: none
*		   
* Parameters:	host task set handle
*
* Remarks:	This routine can be called either at initialization
*		or exceptional condition
*		   
*/

void
sh_setup_emerald_software(struct control_blk *c)
{
	register REGISTER base = c->base;
	struct shim_config *s = c->s;
	BUS_ADDRESS busAddress;
#if	QSIZE_WAR
	USHORT doneq_element;
#endif
		
	OUTBYTE(AICREG(PVARPTR), (UBYTE) 0x00); /* select PVAR page 0 */

	OUTBYTE(SEQMEM(ENABLE_TCB_CHECKSUM),(UBYTE) 0);

	/* setup q_done_pass (both scratch and shadow for HIM */
	OUTBYTE(SEQMEM(Q_DONE_PASS),(UBYTE) 0xff);
	INBYTE(SEQMEM(Q_DONE_PASS));

	/* setup q_done_base: this need be done only once	*/
	busAddress = ulm_kvtop((VIRTUAL_ADDRESS) s->doneq, c->ulm_info);
	sh_output_paddr(c, Q_DONE_BASE, busAddress, 1);

/*	setup primitive_tcb PCI address					*/
	busAddress = ulm_kvtop((VIRTUAL_ADDRESS) s->primitive_tcb, c->ulm_info);
	sh_output_paddr(c, OS_TCB_ADDR, busAddress, 1);

/*	setup q_new_pointer						*/
	busAddress = ulm_kvtop((VIRTUAL_ADDRESS) s->first_tcb, c->ulm_info);
	sh_output_paddr(c, Q_NEW_POINTER, busAddress, 1);


	/* setup q_exe_head , all as per Emerald Initialization Doc. */
	OUTBYTE(SEQMEM(Q_EXE_HEAD),(UBYTE)0xff);
	OUTBYTE(SEQMEM(Q_EXE_HEAD+1),(UBYTE)0xff);

	 /* setup q_hdrxfr_head */
	OUTBYTE(SEQMEM(Q_HDRXFR_HEAD),(UBYTE)0xff);
	OUTBYTE(SEQMEM(Q_HDRXFR_HEAD+1),(UBYTE)0xff);

	 /* setup q_sg_req_head */
	OUTBYTE(SEQMEM(Q_SG_REQ_HEAD),(UBYTE)0xff);
	OUTBYTE(SEQMEM(Q_SG_REQ_HEAD+1),(UBYTE)0xff);

	 /* setup q_empty_head */
	OUTBYTE(SEQMEM(Q_EMPTY_HEAD),(UBYTE)0xff);
	OUTBYTE(SEQMEM(Q_EMPTY_HEAD+1),(UBYTE)0xff);

	 /* setup q_send_head */
	OUTBYTE(SEQMEM (Q_SEND_HEAD),(UBYTE)0xff);
	OUTBYTE(SEQMEM (Q_SEND_HEAD+1),(UBYTE)0xff);

	/* setup q_done_head */
	OUTBYTE(SEQMEM(Q_DONE_HEAD), 0xff);
	OUTBYTE(SEQMEM(Q_DONE_HEAD+1), 0xff);

	/* setup start_rcv/snd_TCB */
	OUTBYTE(SEQMEM(START_RCV_HEAD),(UBYTE)0xff);
	OUTBYTE(SEQMEM(START_RCV_HEAD+1),(UBYTE)0xff);
	OUTBYTE(SEQMEM(START_RCV_HEAD+3),(UBYTE)0xff);

	OUTBYTE(SEQMEM(START_SEND_HEAD),(UBYTE)0xff);
	OUTBYTE(SEQMEM(START_SEND_HEAD+1),(UBYTE)0xff);
	OUTBYTE(SEQMEM(START_SEND_HEAD+2),(UBYTE)0xff);
	OUTBYTE(SEQMEM(START_SEND_HEAD+3),(UBYTE)0xff);

	/* setup q_residual_head */
	OUTBYTE(SEQMEM(Q_RESIDUAL_HEAD), 0xff);
	OUTBYTE(SEQMEM(Q_RESIDUAL_HEAD+1), 0xff);

	/* set up primitive_req			*/
	OUTBYTE(SEQMEM(PRIMITIVE_REQ),(UBYTE)0);
	OUTPCIBYTE(PCIREG(HST_LIP_CTL_HI), (UBYTE)0,c->ulm_info);
	c->flag = 0;
	c->primitive_ctrl = HST_REQ_LIP;

	/* Setup the reference_blk within controk_blk	*/
	c->ru.control_blk = c;

	/* initialize correct link error status field	*/
	OUTBYTE(SEQMEM(LINK_ERR_STATUS), 0);

	/* setup q_new_head			*/
	c->tcbs_for_sequencer = 0;

	sh_setup_doneq(c);

#if QSIZE_WAR
	doneq_element = (USHORT) (c->s->done_q_size >> 2);
	doneq_element = (USHORT) (~doneq_element + 1);

	OUTWORD(SEQMEM(QUEUE_SIZE),doneq_element);
#endif

	c->lip_state = NOT_READY_FOR_LIP;
}

void
sh_setup_doneq(struct control_blk *c)
{
	register REGISTER base = c->base;
	register struct _doneq *d;
	struct shim_config *s = c->s;
	INT i;

	/* init pointer to current doneq element  in the control_blk	*/
	c->d_front = c->d = d = (struct _doneq *) c->s->doneq;

	/* setup done TCB queue in host according to instructions */	 
	for (i = 0; i < (INT)(s->number_tcbs); i++, d++) {
		d->q_pass_count = 0xff;
#if SHIM_DEBUG
		d->tcb_number_hi = d->tcb_number_lo = 0xff;
		d->status = 0xff;
#endif
	}
	c->qdone_end = (struct _doneq *) ((UBYTE *) s->doneq + s->done_q_size);

	OUTBYTE(SEQMEM(Q_DONE_PASS),(UBYTE) 0xff);
	INBYTE(SEQMEM(Q_DONE_PASS));
	c->q_done_pass = 0;
	c->q_done_pass_front = 0;
}

void
sh_output_paddr(struct control_blk *c, INT reg, BUS_ADDRESS baddr, UBYTE i)
{
	register REGISTER base = c->base;

#ifdef _ADDRESS_WIDTH32
	QUADLET *quad = (QUADLET *) &baddr;

	if (i == 0) {		/* use base address	*/
		OUTBYTE(AICREG(reg++), (UBYTE) quad->QU_ubyte0);
		OUTBYTE(AICREG(reg++), (UBYTE) quad->QU_ubyte1);
		OUTBYTE(AICREG(reg++), (UBYTE) quad->QU_ubyte2);
		OUTBYTE(AICREG(reg),   (UBYTE) quad->QU_ubyte3);
	}
	else {			/* use SEQMEM address	*/
		OUTBYTE(SEQMEM(reg++), (UBYTE) quad->QU_ubyte0);
		OUTBYTE(SEQMEM(reg++), (UBYTE) quad->QU_ubyte1);
		OUTBYTE(SEQMEM(reg++), (UBYTE) quad->QU_ubyte2);
		OUTBYTE(SEQMEM(reg),   (UBYTE) quad->QU_ubyte3);
	}
#else
	register OCTLET *oct = (OCTLET *) &baddr;

	if (i == 0) {		/* use base address	*/
		OUTBYTE(AICREG(reg++), oct->OU_byte0);
		OUTBYTE(AICREG(reg++), oct->OU_byte1);
		OUTBYTE(AICREG(reg++), oct->OU_byte2);
		OUTBYTE(AICREG(reg++), oct->OU_byte3);
		OUTBYTE(AICREG(reg++), oct->OU_byte4);
		OUTBYTE(AICREG(reg++), oct->OU_byte5);
		OUTBYTE(AICREG(reg++), oct->OU_byte6);
		OUTBYTE(AICREG(reg),   oct->OU_byte7);
	}
	else {			/* use SEQMEM address	*/
		OUTBYTE(SEQMEM(reg++), oct->OU_byte0);
		OUTBYTE(SEQMEM(reg++), oct->OU_byte1);
		OUTBYTE(SEQMEM(reg++), oct->OU_byte2);
		OUTBYTE(SEQMEM(reg++), oct->OU_byte3);
		OUTBYTE(SEQMEM(reg++), oct->OU_byte4);
		OUTBYTE(SEQMEM(reg++), oct->OU_byte5);
		OUTBYTE(SEQMEM(reg++), oct->OU_byte6);
		OUTBYTE(SEQMEM(reg),   oct->OU_byte7);
	}
#endif

}

/*
*
*  sh_get_size_mainline_code
*
*  Return Value:  code size in bytes
*
*/
INT
sh_get_size_mainline_code()
{
	return (sizeof(sh_mainline_code));
}

/*
*
*  sh_get_vaddr_mainline_code
*
*  Return Value:  code virtual address
*
*/
VIRTUAL_ADDRESS
sh_get_vaddr_mainline_code()
{
	return ((VIRTUAL_ADDRESS) sh_mainline_code);
}

void
sh_reload_mainline_firmware(struct control_blk *c)
{
	register REGISTER base = c->base;
	UBYTE i;

	/* make sure the BACK_TO_LIP flag is off so that no need to
	 * go back to LIP code again.  This flag is used to indicate
	 * that during loading of MTPE code, there is loop failure
	 * happened that we have to restart LIP again.
	 */
	c->flag &= ~BACK_TO_LIP;

	/*
	 * Start 10ms timer here so that in case there is a loop failure
	 * happened, we have enough time to catch it before ALC going into
	 * the old_port state.	Before pausing the MTPE code, everything
	 * should be fine because mainline MTPE code is handling this
	 * condition. When MTPE is paused, slim HIM has to take the
	 * responsibility
	 */
	OUTBYTE(AICREG(CMC_HST_RTC0), 0x0a);
	OUTBYTE(AICREG(CMC_HST_RTC1), CRTC_HST_EN+CIP_DMAEN);

/*	pause the sequencer						*/
	sh_pause_sequencer(c);

	sh_setup_doneq(c);

	for (i = 0; i < 8; i++)
		OUTBYTE(SEQMEM(Q_NEW_POINTER+i), c->post_lip_pointer[i]);

	sh_clear_pre_lip_queues(c);

	if (!(c->flag & RESUME_IO_AFTER_LIP))
	{
		OUTBYTE(SEQMEM(Q_EMPTY_HEAD+1), 0xff);
		OUTBYTE(SEQMEM(Q_EMPTY_TAIL+1), 0xff);
		/* Following can potentially move into LIP code */
		sh_invalidate_tcbs(c);	
	}

	if (sh_timer_checking(c))
		sh_check_loopfailure(c);

	/* set the AL_LAT_TIME according to ULM parameters		*/
	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE1);
#if 1
	/* Not to enable the AL Latency timer due to the hardware bug	*/
	OUTBYTE(AICREG(GENERAL_CTRL) , 0);
#else
	OUTBYTE(AICREG(GENERAL_CTRL) , AL_LAT_TIMER_EN);
	if (c->s->r_rdy_timeout != 0)
		OUTBYTE(AICREG(AL_LAT_TIME) , c->s->r_rdy_timeout);
	else
		OUTBYTE(AICREG(AL_LAT_TIME) , AL_LAT_TIME_DEFAULT);
#endif

        if (INBYTE(AICREG(ALC_LPSM_CMD)) & RESET_LPSM)
		c->flag |= BACK_TO_LIP;

	/* workaround for R-RDY bug where we were opened full-duplex	*/
	OUTBYTE(AICREG(SEQ_CFG0), DIS_FULL_DUPLEX);
	OUTBYTE(AICREG(SEQ_CFG2), 0);
	OUTBYTE(AICREG(SPB_BUF_CTL), SPB_CLR_BUFF);

	OUTBYTE(AICREG(SFC_CMD0) , 0);
	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE5);
	OUTBYTE(AICREG(SFC_CMD0) , 0);
	OUTBYTE(AICREG(SEQ_CFG0), DIS_FULL_DUPLEX);

/*	load back the normal Sequencer code				*/
	if (c->seq_type != SH_NORMAL_SEQ_CODE) {
		if (sh_load_sequencer(c, SH_NORMAL_SEQ_CODE)) {
			ulm_event(&c->ru, EVENT_RELOAD_FAILED, c->ulm_info);
#if SHIM_DEBUG
			ulm_printf("Sequencer reload failure\n");
#endif
			return;
		}
	}

	if (c->flag & BACK_TO_LIP)
	{
		c->ru.tcb_number = c->lip_tcb_number;
		c->ru.tcb = (struct _tcb *)c->saved_lip;
		sh_loadnrun_lip(&c->ru,c);
		c->flag &= ~BACK_TO_LIP;
	}
	else
	{
		OUTBYTE(AICREG(MODE_PTR) , CMC_MODE4);
		OUTBYTE(AICREG(IPEN1), INBYTE(AICREG(IPEN1)) | HWERR_INTIPEN);

		c->lip_state = NO_LIP_IN_PROCESS;

		c->flag &= ~(LIP_DURING_LIP + SEQUENCER_SUSPENDED);

		OUTBYTE(AICREG(CMC_HST_RTC1), CIP_DMAEN);

		OUTBYTE(AICREG(MODE_PTR) , CMC_MODE1);
		OUTBYTE(AICREG(SFC_CMD0),
			(INBYTE(AICREG(SFC_CMD0)) & ~DISABLE_ALC_CONTROL));

		if (c->flag & VALID_ALPA) {
			OUTBYTE(AICREG(ALC_LPSM_CMD), 
				PARTICIPATING|REQ_NON_ACTIVE);
			OUTBYTE(AICREG(OUR_AIDA_CFG), OUR_AIDA_VALID);
		}
		sh_start_mainline_sequencer(c);
	}
}
#ifndef BIOS
/* The MTPE should be paused before calling this routine	
and it should be unpaused after calling this and
ready for normal I/Os		*/
void
sh_resume_post_lip_io(struct control_blk *c)
{
	sh_start_mainline_sequencer(c);
}

void
sh_queue_MTPE_queues(struct control_blk *c, UBYTE queue, UBYTE location)
{
	register REGISTER base = c->base;
	UBYTE tcbptr_save_l, tcbptr_save_h;

	tcbptr_save_l = INBYTE(AICREG(TCBPTR));
	tcbptr_save_h = INBYTE(AICREG(TCBPTR + 1));
	if (location == SH_TAIL)
	{	/* queue at tail	*/
		if (queue == Q_SG_REQ_HEAD)
			OUTBYTE(TCBMEM(0xEB), 0xff);
		else
			OUTBYTE(TCBMEM(0xE9), 0xff);

		if (INBYTE(SEQMEM(queue + 1)) == 0xff)
		{
			OUTBYTE(SEQMEM(queue), tcbptr_save_l);
			OUTBYTE(SEQMEM(queue + 1), tcbptr_save_h);
			OUTBYTE(SEQMEM(queue + 2), tcbptr_save_l);
			OUTBYTE(SEQMEM(queue + 3), tcbptr_save_h);
		}
		else
		{
			OUTBYTE(AICREG(TCBPTR), INBYTE(SEQMEM(queue + 2)));
			OUTBYTE(AICREG(TCBPTR + 1),INBYTE(SEQMEM(queue + 3)));
			if (queue == Q_SG_REQ_HEAD)
			{
				OUTBYTE(TCBMEM(0xEA), tcbptr_save_l);
				OUTBYTE(TCBMEM(0xEB), tcbptr_save_h);
			}
			else
			{
				OUTBYTE(TCBMEM(0xE8), tcbptr_save_l);
				OUTBYTE(TCBMEM(0xE9), tcbptr_save_h);
			}
			OUTBYTE(SEQMEM(queue + 2), tcbptr_save_l);
			OUTBYTE(SEQMEM(queue + 3), tcbptr_save_h);
			OUTBYTE(AICREG(TCBPTR), tcbptr_save_l);
			OUTBYTE(AICREG(TCBPTR + 1), tcbptr_save_h);
		}
	}
	else
	{	/* Queue at head -- applies for q_exe_head only */
		OUTBYTE(TCBMEM(0xE8), INBYTE(SEQMEM(queue)));
		OUTBYTE(TCBMEM(0xE9), INBYTE(SEQMEM(queue + 1)));
		if (INBYTE(SEQMEM(queue + 1)) == 0xff)
		{	/* Queue has nothing	*/
			OUTBYTE(SEQMEM(queue + 2), tcbptr_save_l);
			OUTBYTE(SEQMEM(queue + 3), tcbptr_save_h);
		}
		OUTBYTE(SEQMEM(queue + 2), tcbptr_save_l);
		OUTBYTE(SEQMEM(queue + 3), tcbptr_save_h);
	}
}
		
/* This routine is intended to DMA all outstanding I/Os
that MTPE has not get to SRAM and link them to the
appropriate queues like empty queues, execution queues
etc.  This routine should leave MTPE paused and
MTPE won't be unpaused until sh_resume_post_lip_io()
is called.	*/

void
sh_restore_pre_lip_queues(struct control_blk *c)
{
	register REGISTER base = c->base;
	UBYTE PCI_address_size, i, tcbptr_save_l, tcbptr_save_h;

#ifdef _ADDRESS_WIDTH32
	PCI_address_size = 4;
#else
	PCI_address_size = 8;
#endif

	sh_pause_sequencer(c);

	sh_restore_sequencer_queues(c);

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
	/* This while loop may be able to put into MTPE			*/
	while (c->sntcb_qoff != c->tcbs_for_sequencer_save)
	{	/* There are TCBs that MTPE has not DMAed to SRAM yet	*/
		for (i = 0; i < PCI_address_size; i++)
			OUTBYTE(AICREG(CP_DMA_LADR0+i), c->q_new_pointer[i]);
		/* DMA in 128 byte of TCB				*/
		OUTBYTE(AICREG(CP_DMA_CNT0), 0x80);
		OUTBYTE(AICREG(CMC_DMA_CTL), CP_DMAEN+CMC_DIR+CMC_BUFFRST);
		while (!(INBYTE(AICREG(CMC_STATUS0)) & CMC_DONE))
			;
		OUTBYTE(AICREG(CMC_DMA_CTL), CMC_DIR);
		while (INBYTE(AICREG(CMC_DMA_CTL)) & CP_DMAEN)
			;
		OUTBYTE(AICREG(CMC_BADR), 0x68);
		tcbptr_save_l = INBYTE(AICREG(CMC_BDAT0));
		tcbptr_save_h = INBYTE(AICREG(CMC_BDAT0));
		OUTBYTE(AICREG(TCBPTR), tcbptr_save_l);
		OUTBYTE(AICREG(TCBPTR+1), tcbptr_save_h);

		OUTBYTE(AICREG(DMATCBINDX), 0x0);
		OUTBYTE(AICREG(CMC_DMA_CTL), CM_DMAEN+CMC_DIR+CMC_BUFFRST);
		while (!(INBYTE(AICREG(CMC_STATUS0)) & CMC_DONE))
			;
		OUTBYTE(AICREG(CMC_DMA_CTL), CMC_DIR);
		for (i = 0; i < PCI_address_size; i++)
			c->q_new_pointer[i] = INBYTE(TCBMEM(0xC4 + i));
		if (INBYTE(TCBMEM(SG_CACHE_PTR)) == 0x0b)
		{
			for (i = 0; i < 8; i++)
				OUTBYTE(TCBMEM(0xC4+i), 
					INBYTE(TCBMEM(0xCC+i)));
			sh_queue_MTPE_queues(c, Q_SG_REQ_HEAD, SH_TAIL);
		}
		else if (!(INBYTE(TCBMEM(SG_CACHE_PTR)) & 0x04))
		{
			OUTBYTE(AICREG(CMC_BADR), (0x4C+PCI_address_size+3));
			OUTBYTE(AICREG(CMC_BDAT0), 0x01);
			OUTBYTE(AICREG(CMC_BADR), 0x4C);
			OUTBYTE(AICREG(CM_DMA_CNT0), PCI_address_size+4);
			OUTBYTE(AICREG(DMATCBINDX), 0x80);
			OUTBYTE(AICREG(CMC_DMA_CTL), CM_DMAEN+CMC_DIR);
			while (!(INBYTE(AICREG(CMC_STATUS0)) & CMC_DONE))
				;
			OUTBYTE(AICREG(CMC_DMA_CTL), 0);
		}

		if (INBYTE(TCBMEM(0xA1)) == 0x01)
		{		/* empty TCB	*/
			sh_queue_MTPE_queues(c, Q_EMPTY_HEAD, SH_TAIL);
		}
		else if (INBYTE(TCBMEM(0xA1)) & 0x08)
		{		/* link command TCB */
			OUTBYTE(SEQMEM(RUN_STATUS), 0x02);
			sh_queue_MTPE_queues(c, Q_EXE_HEAD, SH_HEAD);
		}
		else
		{
			OUTBYTE(SEQMEM(RUN_STATUS), 0x02);
			sh_queue_MTPE_queues(c, Q_EXE_HEAD, SH_TAIL);
		}
		c->sntcb_qoff++;
	}
}


UBYTE
sh_abort_in_MTPE_queue(struct control_blk *c, UBYTE queue, USHORT tcb_number)
{
	register REGISTER base = c->base;
	USHORT start_tcb, next_tcb, tail_tcb;
	UBYTE found = 0, next_ptr;

	if (queue == Q_SG_REQ_HEAD)
		next_ptr = 0xEA;
	else
		next_ptr = 0xE8;

	start_tcb = INWORD(SEQMEM(queue));
	if (start_tcb == tcb_number)
	{	/* Hit the tcb at execution head	*/
		OUTWORD(AICREG(TCBPTR), tcb_number);
		next_tcb = INWORD(TCBMEM(next_ptr));
		OUTWORD(SEQMEM(queue), next_tcb);
		if ((next_tcb && 0xff00) == 0xff00)
			OUTWORD(SEQMEM(queue + 2), next_tcb);
		found = 1;
	}
	else
	{	/* Search for the tcb in the execution queue	*/
		while (((start_tcb && 0xff00) == 0xff00) && !found)
		{
			OUTWORD(AICREG(TCBPTR), start_tcb);
			next_tcb = INWORD(TCBMEM(next_ptr));
			if (next_tcb == tcb_number)
			{	/* Found it! */
				found = 1;
				tail_tcb = INWORD(SEQMEM(queue + 2));
				if (tail_tcb == tcb_number)
				{	/* the tcb is at the end of queue */
					OUTWORD(AICREG(TCBPTR), start_tcb);
					OUTWORD(SEQMEM(queue + 2),
						INWORD(TCBMEM(next_ptr)));
				}
				else
				{	/* the tcb is in the middle */
					OUTWORD(AICREG(TCBPTR), tcb_number);
					next_tcb = INWORD(TCBMEM(next_ptr));
					OUTWORD(AICREG(TCBPTR), start_tcb);
					OUTWORD(SEQMEM(next_ptr), next_tcb);
				}
			}
			else
				start_tcb = next_tcb;
		}
	}

	return(found);
}

/* This routine is intended to abort the TCB numbered by the
reference blk while MTPE is paused.  It should leave MTPE pause
alone.	The TCB to be aborted can be either on execution queue
and could potentially have something in the s/g request queue, 
or not on any queue.  For the case where the TCB being not on 
any queue, it must be a left open exchange before LIP happened.
Currently we are assuming the ULM does not abort anything in 
the empty TCB queue. */

void
sh_special_abort(struct reference_blk *r)
{
	struct control_blk *c = r->control_blk;
	register REGISTER base = c->base;
	USHORT tcb_number = r->tcb_number;

	if (!sh_abort_in_MTPE_queue(c, Q_EXE_HEAD, tcb_number) &&
	    !sh_abort_in_MTPE_queue(c, Q_SEND_HEAD, tcb_number))
	{	/* NOT found in execution queue, nor in the send queue	*/
		;
	}

	/* trying to abort in the S/G request queue no matter what	*/
	sh_abort_in_MTPE_queue(c, Q_SG_REQ_HEAD, tcb_number);

	/* invalidate the TCB					*/
	OUTWORD(AICREG(TCBPTR), tcb_number);
	OUTBYTE(TCBMEM(RUN_STATUS), 0x02);
	OUTBYTE(TCBMEM(SG_CACHE_PTR), 0x04);

	c->ru.tcb_number = tcb_number;
	c->ru.done_status = 0;

	ulm_tcb_complete(&c->ru, c->ulm_info);
}

#endif
void
sh_clear_pre_lip_queues(struct control_blk *c)
{
	register REGISTER base = c->base;

	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE4);

/*	Clear all the queues excluding the empty tcb queue */
	OUTBYTE(SEQMEM(Q_EXE_HEAD+1), 0xff);
	OUTBYTE(SEQMEM(Q_EXE_TAIL+1), 0xff);
	OUTBYTE(SEQMEM(Q_SEND_HEAD+1), 0xff);
	OUTBYTE(SEQMEM(Q_SEND_TAIL+1), 0xff);
	OUTWORD(AICREG(SNTCB_QOFF), 0);
	OUTWORD(AICREG(HNTCB_QOFF), 0);
	OUTWORD(AICREG(SDTCB_QOFF0),0);
}

#ifndef BIOS
	
void
sh_restore_sequencer_queues(struct control_blk *c)
{
	register REGISTER base = c->base;

/*	q_exe_head							*/
	OUTBYTE(SEQMEM(Q_EXE_HEAD), c->q_exe_head[0]);
	OUTBYTE(SEQMEM(Q_EXE_HEAD+1), c->q_exe_head[1]);

/*	q_exe_tail							*/
	OUTBYTE(SEQMEM(Q_EXE_TAIL), c->q_exe_tail[0]);
	OUTBYTE(SEQMEM(Q_EXE_TAIL+1), c->q_exe_tail[1]);

/*	q_send_head							*/
	OUTBYTE(SEQMEM(Q_SEND_HEAD), c->q_send_head[0]);
	OUTBYTE(SEQMEM(Q_SEND_HEAD+1), c->q_send_head[1]);

/*	q_send_tail							*/
	OUTBYTE(SEQMEM(Q_SEND_TAIL), c->q_send_tail[0]);
	OUTBYTE(SEQMEM(Q_SEND_TAIL+1), c->q_send_tail[1]);
}
#endif
#ifndef BIOS
static UBYTE lip_table15[] = {
0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x0f, 0x10, 0x17, 0x18, 0x1b, 0x1d, 0x1e,
0x1f, 0x23, 0x25, 0x26, 0x27, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x31, 0x32,
0x33, 0x34, 0x35, 0x36, 0x39, 0x3a, 0x3c, 0x43, 0x45, 0x46, 0x47, 0x49, 0x4a,
0x4b, 0x4c, 0x4d, 0x4e, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x59, 0x5a, 0x5c,
0x63, 0x65, 0x66, 0x67, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x71, 0x72, 0x73,
0x74, 0x75, 0x76, 0x79, 0x7a, 0x7c, 0x80, 0x81, 0x82, 0x84, 0x88, 0x8f, 0x90,
0x97, 0x98, 0x9b, 0x9d, 0x9e, 0x9f, 0xa3, 0xa5, 0xa6, 0xa7, 0xa9, 0xaa, 0xab,
0xac, 0xad, 0xae, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb9, 0xba, 0xbc, 0xc3,
0xc5, 0xc6, 0xc7, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xd1, 0xd2, 0xd3, 0xd4,
0xd5, 0xd6, 0xd9, 0xda, 0xdc, 0xe0, 0xe1, 0xe2, 0xe4, 0xe8, 0xef
};
#else

UBYTE lip_table15[] = {
0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x0f, 0x10, 0x17, 0x18, 0x1b, 0x1d, 0x1e,
0x1f, 0x23, 0x25, 0x26, 0x27, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x31, 0x32,
0x33, 0x34, 0x35, 0x36, 0x39, 0x3a, 0x3c, 0x43, 0x45, 0x46, 0x47, 0x49, 0x4a,
0x4b, 0x4c, 0x4d, 0x4e, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x59, 0x5a, 0x5c,
0x63, 0x65, 0x66, 0x67, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x71, 0x72, 0x73,
0x74, 0x75, 0x76, 0x79, 0x7a, 0x7c, 0x80, 0x81, 0x82, 0x84, 0x88, 0x8f, 0x90,
0x97, 0x98, 0x9b, 0x9d, 0x9e, 0x9f, 0xa3, 0xa5, 0xa6, 0xa7, 0xa9, 0xaa, 0xab,
0xac, 0xad, 0xae, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb9, 0xba, 0xbc, 0xc3,
0xc5, 0xc6, 0xc7, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xd1, 0xd2, 0xd3, 0xd4,
0xd5, 0xd6, 0xd9, 0xda, 0xdc, 0xe0, 0xe1, 0xe2, 0xe4, 0xe8, 0xef

};

#endif
void 
sh_loadnrun_lip(struct reference_blk *r, struct control_blk *c)
{
	register REGISTER base = c->base;
	int i;
	UBYTE *p;
	struct lip_tcb *t;
	USHORT tcb_number;

	if (c->flag & USE_SAVED_LIP_TCB) {
		t = c->saved_lip;
		tcb_number = c->lip_tcb_number;
	} else {
		t = (struct lip_tcb *)r->tcb;
		tcb_number = r->tcb_number;
		c->lip_tcb_number = tcb_number;
		c->saved_lip = t;
		c->flag |= USE_SAVED_LIP_TCB;
		c->flag &= ~VALID_ALPA;

/*		Check for the flag in lip_tcb to see whether resuming	*
 *		I/O is necessary for this LIP.	Also save the next TCB	*
 *		address with which ULM will send TCB down to slim HIM	*/

		p = (UBYTE *) &t->next_tcb_addr0; 
		for (i = 0; i < 8; i++, p++)
			c->post_lip_pointer[i] = *p;

		ulm_swap8((UBYTE *) t, sizeof(struct lip_tcb), c->ulm_info);
		if (t->flag & SH_RESUME_IO)
			c->flag |= (RESUME_IO_AFTER_LIP + SPECIAL_ABORT);
		else
			c->flag &= ~(RESUME_IO_AFTER_LIP + SPECIAL_ABORT);
		ulm_swap8((UBYTE *) t, sizeof(struct lip_tcb), c->ulm_info);

		/* If we executed a LIP primitive, allow the sequencer 
		   to release	*/
		OUTPCIBYTE(PCIREG(HST_LIP_CTL_HI), (UBYTE)0,c->ulm_info);

		c->loopfail_info = 0;
		c->lip_state = LIP_IN_PROCESS;
	}

	/* 
	 * Start 10ms timer here so that in case there is a loop failure
	 * happened, we have enough time to catch it before ALC going into
	 * the old_port state.	Before pausing the MTPE code, everything
	 * should be fine because mainline MTPE code is handling this
	 * condition. When MTPE is paused, slim HIM has to take the
	 * responsibility 
	 */
	OUTBYTE(AICREG(CMC_HST_RTC0), 0x0a);
	OUTBYTE(AICREG(CMC_HST_RTC1), CRTC_HST_EN+CIP_DMAEN);

	/*	pause the sequencer				*/
	sh_pause_sequencer(c);

	/*	Clear the link error status on scratch here	*/
	OUTBYTE(SEQMEM(LINK_ERR_STATUS), 0);
								 
	if (c->seq_type != SH_LIP_SEQ_CODE)
		sh_save_sequencer_state(c);

	if (sh_timer_checking(c))
		sh_check_loopfailure(c);

	/*	Disable Emerald modules and setup TCBPTRs	*/

	/*	Host						*/
	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE0);
	OUTBYTE(AICREG(HS_DMA_CTL) , 0);
	OUTWORD(AICREG(TCBPTR), (USHORT) tcb_number);

	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE2);
	OUTBYTE(AICREG(HS_DMA_CTL) , 0);
	OUTWORD(AICREG(TCBPTR), (USHORT) tcb_number);

	/*	SFC						*/
	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE1);
	OUTBYTE(AICREG(SFC_CMD0) , 0);
	OUTBYTE(AICREG(OUR_AIDA_CFG) , 0x00);
	OUTWORD(AICREG(TCBPTR), (USHORT) tcb_number);
	OUTBYTE(AICREG(SPB_BUF_CTL), SPB_CLR_BUFF);

	OUTBYTE(AICREG(SEQ_CFG0), 0xff);
	OUTBYTE(AICREG(SEQ_CFG2), 0xff);

	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE5);
	OUTBYTE(AICREG(SFC_CMD0) , 0);
	OUTWORD(AICREG(TCBPTR), (USHORT) tcb_number);

	/*	CMC						*/
	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE4);
	OUTBYTE(AICREG(CMC_DMA_CTL) , 0);
	OUTWORD(AICREG(TCBPTR), (USHORT) tcb_number);
	OUTBYTE(AICREG(IPEN1), INBYTE(AICREG(IPEN1)) | HWERR_INTIPEN);

	/*	RFC						*/
	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE3);
	OUTWORD(AICREG(TCBPTR), tcb_number);

	if (sh_timer_checking(c))
		sh_check_loopfailure(c);

	/*	load the LIP tcb				*/

	/*	Address the first byte of the tcb		*/
	p = (UBYTE *) t;
	/*	move in the tcb					*/
	for (i = 0; i < sizeof(struct _tcb); i++) {
		OUTBYTE(TCBMEM(TCB_DAT0 + i)  , *p++);
	}

	if (sh_timer_checking(c))
		sh_check_loopfailure(c);

	/*	always reload fresh copy of table 15		*/
	OUTBYTE(AICREG(PVARPTR), 1);	/* select PVAR page 1 */
#ifdef BIOS
	SetCurrentDS();
#endif
		p = lip_table15;
	for (i = 0; i < LIP_TABLE15_SIZE; i++) {
		OUTBYTE(SEQMEM(PVAR_DAT0 + i), *p++);
	}
#ifdef BIOS
	RestoreCurrentDS();
#endif
	OUTBYTE(AICREG(PVARPTR), 0);	/* select PVAR page 0 */

	/* swap in LIP sequencer code if necessary		*/
	if (c->seq_type != SH_LIP_SEQ_CODE) {
		if (sh_load_sequencer(c, SH_LIP_SEQ_CODE)) {
#if SHIM_DEBUG
			ulm_printf("Sequencer load failure\n");
#endif
			return;
		}
	}

	if (sh_timer_checking(c))
		sh_check_loopfailure(c);

	OUTBYTE(AICREG(CMC_HST_RTC1), CIP_DMAEN);

	sh_timer_checking(c);

	sh_setup_doneq(c);

	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE1);

	/* We cannot be loop master until we get an AL_PA	*/
	OUTBYTE(AICREG(ALC_LPSM_CMD),
		(INBYTE(AICREG(ALC_LPSM_CMD)) & ~LOOP_MASTER));

	if ((c->flag & PRIMITIVE_LIP_SENT) && !(c->flag & LIP_DURING_LIP))
		sh_send_lip_primitive_in_lip(c);

	sh_start_sequencer(c);
}

void
sh_save_sequencer_state(struct control_blk *c)
{
	register REGISTER base = c->base;
	int i;

/*	save sequencer registers					*/
	OUTBYTE(AICREG(PVARPTR), 0);	/* select PVAR page 0 */

/*	q_new_pointer							*/
	for (i = 0; i < sizeof(c->q_new_pointer); i++) {
		c->q_new_pointer[i] = INBYTE(SEQMEM(Q_NEW_POINTER+i));
	}

/*	q_exe_head							*/
	c->q_exe_head[0] = INBYTE(SEQMEM(Q_EXE_HEAD));
	c->q_exe_head[1] = INBYTE(SEQMEM(Q_EXE_HEAD+1));

/*	q_exe_tail							*/
	c->q_exe_tail[0] = INBYTE(SEQMEM(Q_EXE_TAIL));
	c->q_exe_tail[1] = INBYTE(SEQMEM(Q_EXE_TAIL+1));

/*	q_send_head							*/
	c->q_send_head[0] = INBYTE(SEQMEM(Q_SEND_HEAD));
	c->q_send_head[1] = INBYTE(SEQMEM(Q_SEND_HEAD+1));

/*	q_send_tail							*/
	c->q_send_tail[0] = INBYTE(SEQMEM(Q_SEND_TAIL));
	c->q_send_tail[1] = INBYTE(SEQMEM(Q_SEND_TAIL+1));

/*	sntcb_qoff							*/
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
	c->sntcb_qoff = INWORD(AICREG(SNTCB_QOFF));

	c->tcbs_for_sequencer_save = c->tcbs_for_sequencer;

	c->q_empty_head[0] = INBYTE(SEQMEM(Q_EMPTY_HEAD));
	c->q_empty_head[1] = INBYTE(SEQMEM(Q_EMPTY_HEAD+1));

	c->q_empty_tail[0] = INBYTE(SEQMEM(Q_EMPTY_HEAD+2));
	c->q_empty_tail[1] = INBYTE(SEQMEM(Q_EMPTY_HEAD+3));

	c->tcbs_for_sequencer = 0;
}

/*
*
* sh_start_sequencer
*
* Start up the sequencer
*
* Return Value: none
*
* Parameters:	control_blk pointer
*
* Note:		startup depends on version of sequencer code
*		   
*/
void
sh_start_sequencer(struct control_blk *c)
{
	register REGISTER base = c->base;

	/* sh_pause_sequencer(c); */
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);


	switch (c->seq_type) {
		case (SH_NORMAL_SEQ_CODE):
			sh_start_mainline_sequencer(c);
			break;
		case (SH_LIP_SEQ_CODE):
			sh_start_lip_sequencer(c);
			break;

		default:
#if SHIM_DEBUG
			ulm_printf("sh_start_sequencer: No sequencer code loaded\n");
#endif
			break;
	}
}

/*
*
* sh_start_mainline_sequencer
*
* Start to run mainline sequencer
*
* Return Value: none
*		   
* Parameters:	control_blk pointer
*
* Remarks:	This routine starts the sequencer at address 0. 
*
*/
void
sh_start_mainline_sequencer(struct control_blk *c)
{
	register REGISTER base = c->base;


/*	Alway start at address 0					*/
	OUTBYTE(AICREG(SEQCTL), SEQCTL_RESET);

#ifdef NOT_YET
/*	set the sequencer to location					*/
	OUTWORD(AICREG(SEQADDR0), (USHORT)(IDLE_LOOP_ENTRY >> 2));
#endif

/*	unpause the sequencer						*/
	OUTBYTE(AICREG(CMC_HCTL), 
		(INBYTE(AICREG(CMC_HCTL)) & ~(HPAUSE|HPAUSETOP)));

/*	validate writes							*/
	INBYTE(AICREG(CMC_HCTL));
}

/*
*
* sh_pause_sequencer
*
* Pause the sequencer
*
* Return Value: none
*		   
* Parameters:	control_blk pointer
*
* Remarks:	This pauses the sequencer, dependent on type of
*		sequencer code is loaded, if any.
*
*/
void
sh_pause_sequencer(struct control_blk *c)
{
	register REGISTER base = c->base;
	UBYTE	val;

	val = INBYTE(AICREG(CMC_HCTL));
	if (!(val & HPAUSE))
	{
		OUTBYTE(AICREG(CMC_HCTL), val | HPAUSE);
		val = 0;
		while ((INBYTE(AICREG(CMC_HCTL)) & HPAUSEACK) != HPAUSEACK)
#if SHIM_DEBUG
			val++;
		if (val > 0)
			ulm_printf("%d times through pause loop\n");
#else
			;
#endif
	}
}

#if SHIM_DEBUG
void
sh_dump_prim_tcb(struct primitive_tcb *t, UBYTE flag)
{
	ulm_swap8((UBYTE *) t, sizeof(struct primitive_tcb), c->ulm_info);
	ulm_printf("Primitive tcb: arb %x pd %x 1st %x 2nd %x alc %x c->flag %x\n",
		t->arb_request, t->al_pd, t->opn_prim,
		t->ulm_prim, t->alc_control, flag);
	ulm_swap8((UBYTE *) t, sizeof(struct primitive_tcb), c->ulm_info);
}
#endif

void
sh_reset_chip(register REGISTER base)
{
	UBYTE	val;

	val = INBYTE(AICREG(CMC_HCTL));
	while ((INBYTE(AICREG(CMC_HCTL)) & HPAUSEACK) != HPAUSEACK)
		OUTBYTE(AICREG(CMC_HCTL), val | HPAUSE);

	OUTBYTE(AICREG(CMC_HCTL), CMC_HCTL_RESET_MASK);
}
#ifndef BIOS
/*
*
* sh_read_seeprom
*
* Read from seeprom
*
* Return Value: none
*
* Parameters:	control block pointer
*		buffer to be filled
*		seeprom address
*		number of word to be read
*
* Remarks:	the sequencer must be pasued when this routine get
*		referenced
*
*/

void
sh_read_seeprom(register REGISTER base,UBYTE *buffer,INT seeprom_address,INT no_of_word)
{
	INT i;
	UBYTE cmc_hctl, old_mode;

	/* keep cmc_hctl value for restore */
	cmc_hctl = INBYTE(AICREG(CMC_HCTL));

	/* pause sequencer if it is not paused yet */
	if (!(cmc_hctl & HPAUSEACK))
	{
		while ((INBYTE(AICREG(CMC_HCTL)) & HPAUSEACK) != HPAUSEACK)
			OUTBYTE(AICREG(CMC_HCTL), cmc_hctl | HPAUSE);
	}
	
	old_mode = INBYTE(AICREG(MODE_PTR));
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE0);

	for (i = 0; i < no_of_word; i++) {
		OUTBYTE(AICREG(SEEADR),seeprom_address+i);
		OUTBYTE(AICREG(SEECTL),OP2|OP1|START);

		/* wair for read busy */
		while(INBYTE(AICREG(SEECTL)) & BUSY)
			;

		/* load data into buffer */
		*((USHORT *)(buffer+i*2)) = (USHORT) INBYTE(AICREG(SEEDAT0));
		*((USHORT *)(buffer+i*2)) |= (USHORT) (INBYTE(AICREG(SEEDAT1)) << 8);
	}

	OUTBYTE(AICREG(MODE_PTR), old_mode);

	/* restore CMC_HCTL register only if PAUSE bit is not true */
	if (!(cmc_hctl & HPAUSEACK)) 
		OUTBYTE(AICREG(CMC_HCTL),cmc_hctl);
}

/*
*
* sh_write_non_protected_seeprom
*
* Write to non protected area of seeprom
*
* Return Value: 0 - write to seeprom successfully
*		1 - can't write to protected area
*
* Parameters:	control block pointer
*		seeprom address
*		number of word to be written
*		buffer for image to be wriietn to seeprom
*
* Remarks:	this routine can only write to non protected area.
*		for Emerald A1 hardware the seeprom is protected
*		every other 32 word
*
*/

INT
sh_write_non_protected_seeprom(register REGISTER base,INT seeprom_address,INT no_of_word,UBYTE *buffer)
{
	INT i, j;
	INT offset;
	UBYTE cmc_hctl, old_mode;

	/* keep cmc_hctl value for restore */
	cmc_hctl = INBYTE(AICREG(CMC_HCTL));

	/* pause sequencer if it is not paused yet */
	if (!(cmc_hctl & HPAUSEACK))
	{
		while ((INBYTE(AICREG(CMC_HCTL)) & HPAUSEACK) != HPAUSEACK)
			OUTBYTE(AICREG(CMC_HCTL), cmc_hctl | HPAUSE);
	}
	
	old_mode = INBYTE(AICREG(MODE_PTR));
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE0);

	/* make sure it's not protected area to be written */
	offset = seeprom_address & 0x3f;
	if ((offset < 0x20) || (((offset & 0x1F) +  no_of_word) > 0x20)) {
		return(1);
	}

	/* enable write to seeprom */
	OUTBYTE(AICREG(SEEADR),OP4|OP3);
	OUTBYTE(AICREG(SEECTL),OP2|START);

	/* wait for enable busy */
	while(INBYTE(AICREG(SEECTL)) & BUSY)
		;

	/* write to the seeprom */
	for (i = 0; i < no_of_word; i++) {
		/* write one word to seeprom */
		OUTBYTE(AICREG(SEEADR),seeprom_address+i);
		OUTBYTE(AICREG(SEEDAT0),(UBYTE)*((USHORT *)(buffer+i*2)));
		OUTBYTE(AICREG(SEEDAT1),(UBYTE)(*((USHORT *)(buffer+i*2)) >> 8));
		OUTBYTE(AICREG(SEECTL),OP2|OP0|START);

		/* wait for write busy */
		while(INBYTE(AICREG(SEECTL)) & BUSY)
			;

/*		BUSY comes on prematurely, wait a bit			*/
/*		Put in a counter for now, wait for hardware team's input*/

		j = 0x7fff;
		while (j--)
			;
	}

	/* disable write to seeprom */
	OUTBYTE(AICREG(SEEADR),0);
	OUTBYTE(AICREG(SEECTL),OP2|START);

	/* wait for disable busy */
	while(INBYTE(AICREG(SEECTL)) & BUSY)
		;

	OUTBYTE(AICREG(MODE_PTR), old_mode);

	/* restore CMC_HCTL register only if PAUSE bit is not true */
	if (!(cmc_hctl & HPAUSEACK)) 
		OUTBYTE(AICREG(CMC_HCTL),cmc_hctl);

	return(0);
}

/*
*
* sh_write_seeprom
*
* Write to seeprom
*
* Return Value: 0 - write to seeprom successfully
*		1 - illegal address
*
* Parameters:	control block pointer
*		seeprom address
*		number of word to be written
*		buffer for image to be wriietn to seeprom
*
* Remarks:	this routine write to area.
*
*/

INT
sh_write_seeprom(register REGISTER base,INT seeprom_address,INT no_of_word,UBYTE *buffer)
{
	INT i, j;
	UBYTE cmc_hctl, old_mode;

	/* keep cmc_hctl value for restore */
	cmc_hctl = INBYTE(AICREG(CMC_HCTL));

	/* pause sequencer if it is not paused yet */
	if (!(cmc_hctl & HPAUSEACK))
	{
		while ((INBYTE(AICREG(CMC_HCTL)) & HPAUSEACK) != HPAUSEACK)
			OUTBYTE(AICREG(CMC_HCTL), cmc_hctl | HPAUSE);
	}
	
	old_mode = INBYTE(AICREG(MODE_PTR));
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE0);

	/* valid address? */
	if (seeprom_address > 0x3f) {
		return(1);
	}

	/* enable write to seeprom */
	OUTBYTE(AICREG(SEEADR),OP4|OP3);
	OUTBYTE(AICREG(SEECTL),OP2|START);

	/* wait for enable busy */
	while(INBYTE(AICREG(SEECTL)) & BUSY);

	/* write to the seeprom */
	for (i = 0; i < no_of_word; i++) {
		/* write one word to seeprom */
		OUTBYTE(AICREG(SEEADR),seeprom_address+i);
		OUTBYTE(AICREG(SEEDAT0),(UBYTE)*((USHORT *)(buffer+i*2)));
		OUTBYTE(AICREG(SEEDAT1),(UBYTE)(*((USHORT *)(buffer+i*2)) >> 8));
		OUTBYTE(AICREG(SEECTL),OP2|OP0|START);

		/* wait for write busy */
		while(INBYTE(AICREG(SEECTL)) & BUSY);

/*		BUSY comes on prematurely, wait a bit			*/
/*		Put in a counter for now, wait for hardware team's input*/

		j = 0x7fff;
		while (j--);
	}

	/* disable write to seeprom */
	OUTBYTE(AICREG(SEEADR),0);
	OUTBYTE(AICREG(SEECTL),OP2|START);

	/* wait for disable busy */
	while(INBYTE(AICREG(SEECTL)) & BUSY);

	OUTBYTE(AICREG(MODE_PTR), old_mode);

	/* restore CMC_HCTL register only if PAUSE bit is not true */
	if (!(cmc_hctl & HPAUSEACK)) 
		OUTBYTE(AICREG(CMC_HCTL),cmc_hctl);

	return(0);
}

/*
*
* sh_read_mia_type
*
* Get the MIA type
*
* Return Value: 0 - No MIA is installed
*		1 - A multiple mode MIA is installed
*		2 - A single mode MIA is installed
*		3 - Unknown MIA type
*
* Parameters:	control block pointer
*
* Remarks:	Bit 2 of flag field in shim_config struct must be
*		initialized to tell SHIM whether it is voltage sensing
*		or current sensing before this call
*/

UBYTE
sh_read_mia_type(struct control_blk *c)
{
	if (c->s->flag & CURRENT_SENSING)
		return(c->miatype_cs);
	else
		return(c->miatype_vs);
}

/*
*
* sh_convert
*
* Convert address into cpu endian
*
* Parameters:	virtual address point to 32 bits address
*
* Return value: address format matched with cpu endian
*
* Remarks:	none
*
*/
#ifdef _ADDRESS_WIDTH32
BUS_ADDRESS
sh_convert(UBYTE *addr)
{
	BUS_ADDRESS busAddress = 0, temp_bus;
	UBYTE i;

	for (i = 0; i < 32; i+= 8)
	{
		temp_bus = addr[i/8];
		busAddress += (temp_bus << i);
	}
	return(busAddress);
}
#endif

/*
 *
 * sh_convert
 *
 * Convert address into cpu endian
 *
 * Parameters:	 virtual address point to 64 bits address
 *
 * Return value: address format matched with cpu endian
 *
 * Remarks:	 none
 *
 */
#ifndef _ADDRESS_WIDTH32
BUS_ADDRESS
sh_convert(UBYTE *addr)
{
	BUS_ADDRESS busAddress = 0, temp_bus;
	UBYTE i;

	for (i = 0; i < 64; i+= 8)
	{
		temp_bus = addr[i/8];
		busAddress += (temp_bus << i);
	}
	return(busAddress);
}
#endif

/*
*
* sh_get_residual_length
*
* Calculate residual length based on the information stored in memory
* pointed by info_host_address of tcb.
*
* Return Value: residual length
*
* Parameters:	virtual address to the memory pointed by info_host_addr
*		virtual address pointing to beginning of sg list
*		physical (bus) address pointing to beginning of sg list
*
* Remarks:	this routine should only be called when done_status is
*		underrun or the response code indicating error
*
*/

UBYTE4
sh_get_residual_length(struct control_blk *c, void *info_host_vadr, 
		       void *sg_list_vadr, BUS_ADDRESS sg_list_padr)
{
	UBYTE4 residual;
	UBYTE4 offset;
	residual_info *rp = (residual_info *) ((UBYTE *)info_host_vadr + 128);
	BUS_ADDRESS next_page_padr;
	SH_SG_DESCRIPTOR *sp = (SH_SG_DESCRIPTOR *)sg_list_vadr;
	UBYTE sg_element_size;

	/* reverse the host-to-pci bridge effect */
	ulm_swap8((UBYTE *)rp,16, c->ulm_info);

	/* get residual length for the current segment */
	residual = rp->current_residual0 + (rp->current_residual1 << 8) +
	       (rp->current_residual2 << 16);

	/* only the high 4 bits represent the current SG number */
	rp->current_sg_no >>= 4;
	sg_element_size = sizeof(SH_SG_DESCRIPTOR);

	if (*((BUS_ADDRESS *)rp)) {
		next_page_padr = sh_convert((UBYTE *) rp);
		offset = next_page_padr - 128 - sg_list_padr;
		offset += rp->current_sg_no * sg_element_size;
		sp += offset / sg_element_size - (intptr_t) 1;

		ulm_swap8((UBYTE *) sp, sg_element_size, c->ulm_info);
		if (sp->segmentLength[3] == 0)
		{
			ulm_swap8((UBYTE *) sp, sg_element_size, c->ulm_info);
			while (1)
			{
				++sp;
				ulm_swap8((UBYTE *) sp, sg_element_size, 
					c->ulm_info);

				residual += sp->segmentLength[0];
				residual += sp->segmentLength[1] << 8;
				residual += sp->segmentLength[2] << 16;

				if (sp->segmentLength[3])
					break;

				ulm_swap8((UBYTE *) sp, sg_element_size, 
					c->ulm_info);
			}
		}
		ulm_swap8((UBYTE *) sp, sg_element_size, c->ulm_info);
	}

	/* clear residual information */
	bzero((UBYTE *)rp, 16);

	return(residual);
}
#endif
