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
*  Module Name:   hwlip.c
*
*  Description:
*		Code to run Log In Process (LIP)
*
*  Owners:	ECX Fibre Channel CHIM Team
*    
*  Notes:	This is the only module that includes the lip
*		sequencer code. The loading of all sequencer dode
*		is done in hwutil.c.
*
*  Entry Point(s):
*			sh_start_lip_sequencer
*			sh_get_vaddr_lip_code
*			sh_get_size_lip_code
*/

#include "hwgeneral.h"
#include "hwcustom.h"
#include "ulmconfig.h"
#include "hwtcb.h"
#include "hwequ.h"
#include "hwref.h"
#include "hwdef.h"
#include "hwerror.h"
#include "lip.h"
#include "lseq_off.h"
#include "hwproto.h"

/*
*
*  sh_start_lip_sequencer
*
*     Start lip sequencer
*
* Return Value:  none
*
* Parameters:	 control_blk pointer
*
* Remarks: This code is placed here because of possible #define
* collisions between differing versions of sequencer code.
*		   
*/
void
sh_start_lip_sequencer(struct control_blk *c)
{
	register REGISTER base = c->base;
	USHORT start_address;
#if LIP_DEBUG
	UBYTE debug;
#endif

/*	This makes the unpause work					*/
	OUTBYTE(AICREG(SEQCTL), SEQCTL_RUN);

	if ((c->flag & PRIMITIVE_LIP_SENT) && !(c->flag & LIP_DURING_LIP))
		start_address = START_LIP_INIT >> 2;
	else
		start_address = START_LIP >> 2;

#if LIP_DEBUG
	debug = INBYTE(AICREG(ALC_LPSM_CMD));
	ulm_printf("The ALC_LPSM_CMD is %d, ",debug);
	debug = INBYTE(AICREG(ALC_LPSM_STATUS_1));
	ulm_printf("ALC_LPSM_STATUS1 is %d, ",debug);
	debug = INBYTE(AICREG(ALC_LPSM_STATUS_2));
	ulm_printf("ALC_LPSM_STATUS2 is %d\n",debug);
#endif

	if (start_address == 0)
		OUTBYTE(AICREG(SEQCTL), SEQCTL_RESET);
	else {
		OUTWORD(AICREG(SEQADDR0), (USHORT) (start_address-1));
		OUTWORD(AICREG(SEQADDR0), (USHORT) start_address);
	}

/*	unpause the sequencer						*/
	OUTBYTE(AICREG(CMC_HCTL), (INBYTE(AICREG(CMC_HCTL))
			& ~(HPAUSE|HPAUSETOP)));

/*	validate writes							*/
	INBYTE(AICREG(CMC_HCTL));
}
void
sh_send_lip_primitive_in_lip(struct control_blk *c)
{
	register REGISTER base = c->base;
	struct primitive_tcb *t = (struct primitive_tcb *) (c->s)->primitive_tcb;

	ulm_swap8((UBYTE *) t, sizeof(struct primitive_tcb), c->ulm_info);
	OUTBYTE(SEQMEM(ARBI_REQ), PARTICIPATING+REQ_ARB_UNFAIR);
	OUTBYTE(SEQMEM(LIP_PRIM), t->ulm_prim);
	OUTBYTE(SEQMEM(DEST_PRIM), t->al_pd);
	ulm_swap8((UBYTE *) t, sizeof(struct primitive_tcb), c->ulm_info);
}

/*
*
*  sh_get_vaddr_lip_code
*
*  Return Value:  virtual address of lip code
*
*/
VIRTUAL_ADDRESS
sh_get_vaddr_lip_code()
{
	VIRTUAL_ADDRESS vaddr;

	vaddr = (VIRTUAL_ADDRESS) sh_lip_code;

	return (vaddr);
}

/*
*
*  sh_get_size_lip_code
*
*  Return Value:  code size in bytes
*
*/

INT
sh_get_size_lip_code()
{
	return (sizeof(sh_lip_code));
}

/*
*
*  sh_timer_checking
*
*     Check to see whether the timer is expired or not
*
* Return Value:  0   Timer not expired
*		 1   Timer expired
*
* Parameters:	 control_blk pointer
*
* Remarks: This internal routine is used when swapping between mainline
*	   code and LIP code.
*		   
*/
UBYTE
sh_timer_checking(struct control_blk *c)
{
	register REGISTER base = c->base;

	if (INBYTE(AICREG(POST_STAT1)) & CRTC_HST_INT)
	{
		/* 
		 * By clearing the CRTC_HST_INT, the clock will start
		 * ticking again.  We do not need to disable the timer
		 * and re-enable the timer again for it.
		 */
		OUTBYTE(AICREG(POST_STAT1), CRTC_HST_INT);
		return(1);
	}
	return(0);
}

/*
*
*  sh_check_loopfailure
*
*     Check to see whether there is any loop failure and if there is,
*     try to handle it, if there is not, just return.
*
* Return Value:  none
*
* Parameters:	 control_blk pointer
*
* Remarks: This internal routine is used when swapping between mainline
*	   code and LIP code.
*		   
*/
void
sh_check_loopfailure(struct control_blk *c)
{
	register REGISTER base = c->base;
        UBYTE mode_sav, lpsm_cmd, order_ctrl;
        UBYTE recover_counter = 0xff;

	mode_sav = INBYTE(AICREG(MODE_PTR));
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE3);
	if (INBYTE(AICREG(RFC_STATUSA)) & RFC_LINKERR)
	{
                OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);
                lpsm_cmd = INBYTE(AICREG(ALC_LPSM_CMD));
                OUTBYTE(AICREG(ALC_LPSM_CMD), lpsm_cmd | RESET_LPSM);

                if (lpsm_cmd & PARTICIPATING)
                        order_ctrl = SEND_OS_CONT + LIPf7ALPA;
                else
                        order_ctrl = SEND_OS_CONT + LIPf7f7;

                OUTBYTE(AICREG(ORDR_CTRL), order_ctrl);
                OUTBYTE(AICREG(ALC_LPSM_STATUS_1), LIP_RCV);

                /* 
                 * We may or may not be able to recover from loop failure
                 * in a reasonable short time and we do not want to hang
                 * the system resource for this recovery.  So we put in a
                 * counter to avoid staying in this infinite recover loop
                 */
                do {
                        OUTBYTE(AICREG(MODE_PTR), CMC_MODE3);
                        OUTBYTE(AICREG(LINK_ERRST), LINKEN+LINKRST);
                OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);
                } while (!(INBYTE(AICREG(ALC_LPSM_STATUS_1)) & LIP_RCV) &&
                         !(recover_counter--));

                /* clear the linkerr status in RFC */
                OUTBYTE(AICREG(MODE_PTR), CMC_MODE3);
                OUTBYTE(AICREG(RFC_STATUSA), RFC_LINKERR);

		c->flag |= BACK_TO_LIP;
	}
	OUTBYTE(AICREG(MODE_PTR), mode_sav);
}
