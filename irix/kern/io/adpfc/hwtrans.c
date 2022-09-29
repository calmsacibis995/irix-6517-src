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
*  Module Name:   hwtrans.c
*
*  Description:
*		  Code to manage and deliver tcbs to sequencer
*
*  Owners:	  ECX Fibre Channel CHIM Team
*    
*  Notes:
*
*			sh_deliver_tcb
*			sh_special_tcb
*		  
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
#include "seq_off.h"
#include "hwproto.h"

/*
* sh_deliver_tcb
*
* Transport a tcb to the sequencer
*
* Output Parameters:	none
*		   
* Input Parameters: struct reference_blk
*
* Notes: 
*
*/

void
sh_deliver_tcb(struct reference_blk *r)
{
	register REGISTER base;
	register struct control_blk *c;

/*	Set up adapter addressability				*/
	c = (struct control_blk *) r->control_blk;
	base = c->base;

	OUTWORD(AICREG(HNTCB_QOFF), (USHORT) ++c->tcbs_for_sequencer); 
}

/*
* sh_special_tcb
*
* Transport a special tcb to the sequencer
*
* Input Parameters: struct reference_blk
*
* Output Parameters:	0 on success
*			-1 on failure
*			-2 on illegal request
*		   
* Input Parameters: struct reference_blk
*
* Notes: 
*
*/
INT
sh_special_tcb(struct reference_blk *r)
{

	UBYTE function;
	struct control_blk *c = (struct control_blk *)r->control_blk;
	INT status = SH_SUCCESS;
	register REGISTER base;

	base = c->base;

	function = r->func;

	switch (function) {

	case (SH_PRIMITIVE_REQUEST): {
#if SHIM_DEBUG
		struct shim_config *s = c->s;
		struct primitive_tcb *t;

		t = s->primitive_tcb;
#endif

/*		is this the first of a set of two primitives to deliver ?	*/
		if (r->sub_func & LIP_TYPE_PRIMITIVE) {
			c->flag |= PRIMITIVE_LIP_SENT;
			if ((c->lip_state == NOT_READY_FOR_LIP) &&
				!(c->flag & SEQUENCER_SUSPENDED)) {
				c->lip_state = WAITING_FOR_LIP_QUIESCE;
			}
		}

#if SHIM_DEBUG
		sh_dump_prim_tcb(t, c->flag);
#endif

		
		if ((c->lip_state == NOT_READY_FOR_LIP) &&
			(c->flag & SEQUENCER_SUSPENDED))
		{
			c->flag &= ~SEQUENCER_SUSPENDED;
			c->lip_state = READY_TO_LIP;
			ulm_event(&c->ru, EVENT_READY_FOR_LIP_TCB, c->ulm_info);
		}
		else
		{
			OUTPCIBYTE(PCIREG(HST_LIP_CTL_HI), c->primitive_ctrl,c->ulm_info);
			/* alternate one of HST_REQ_LIP and HST_REQ_CLS bit */
			/* for sending primitive.			    */
			c->primitive_ctrl ^= (HST_REQ_LIP + HST_REQ_CLS);
		}
		break;
	}
#ifndef BIOS
	case (SH_ABORT_REQUEST): 

		if (c->flag & SPECIAL_ABORT)
		{
			if (c->flag & RESUME_IO_AFTER_LIP)
			{
				sh_pausetop(c);
				sh_restore_pre_lip_queues(c);
/*				The MTPE should be paused at this point *
 *				until ULM calls for SH_RESTORE_QUEUES	*/
				c->flag &= ~RESUME_IO_AFTER_LIP;
			}
			sh_special_abort(r);
		}
		else
			sh_deliver_tcb(r);
		break;

	case (SH_SETUP_SERDES_LOOPBACK):  {

                UBYTE old_mode;

		sh_pause_sequencer(c);
		old_mode = INBYTE(AICREG(MODE_PTR));
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);

		switch (r->sub_func) {

			case (0):
			OUTBYTE(AICREG(SERDES_CTRL),
				INBYTE(AICREG(SERDES_CTRL)) 
					& ~LOOP_BACK_EN);
			break;

			case (1):
			OUTBYTE(AICREG(SERDES_CTRL),
				INBYTE(AICREG(SERDES_CTRL)) 
					| LOOP_BACK_EN);
			break;

			default:
			status = SH_ILLEGAL_REQUEST;
			break;
		}

		OUTBYTE(AICREG(MODE_PTR), old_mode);
		sh_unpause_sequencer(c->base);
		break;
	}

	case (SH_SETUP_LRC):  {

                UBYTE old_mode;

		sh_pause_sequencer(c);
		old_mode = INBYTE(AICREG(MODE_PTR));
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);

		switch (r->sub_func) {

			case (0):
			OUTBYTE(AICREG(SERDES_CTRL),
				INBYTE(AICREG(SERDES_CTRL)) 
					& ~LRC_EN);
			break;

			case (1):
			OUTBYTE(AICREG(SERDES_CTRL),
				INBYTE(AICREG(SERDES_CTRL)) 
					| LRC_EN);
			break;

			default:
			status = SH_ILLEGAL_REQUEST;
			break;
		}

		OUTBYTE(AICREG(MODE_PTR), old_mode);
		sh_unpause_sequencer(c->base);
		break;
	}

	case (SH_SETUP_RFC_HOLD):  {

                UBYTE old_mode;

		sh_pause_sequencer(c);
		old_mode = INBYTE(AICREG(MODE_PTR));
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE3);

		switch (r->sub_func) {

			case (0):
			OUTBYTE(AICREG(RFC_MODE),
				INBYTE(AICREG(RFC_MODE)) 
					& ~HOLD);
			break;

			case (1):
			OUTBYTE(AICREG(RFC_MODE),
				INBYTE(AICREG(RFC_MODE)) 
					| HOLD);
			break;

			default:
			status = SH_ILLEGAL_REQUEST;
			break;
		}

		OUTBYTE(AICREG(MODE_PTR), old_mode);
		sh_unpause_sequencer(c->base);
		break;
	}
#endif
	case (SH_DELIVER_LIP_TCB): {
		c->flag &= ~USE_SAVED_LIP_TCB;
		sh_loadnrun_lip(r, c);

		break;
	}
#ifndef BIOS
	case (SH_RESTORE_QUEUES): {
		sh_resume_post_lip_io(c);
		break;
	}

	case (SH_READ_LINK_ERRORS): {

                UBYTE   old_mode;
		struct	link_error_status_blk *lesb = 
			(struct link_error_status_blk *) r->tcb;

		sh_pausetop(c);
		sh_pause_sequencer(c);
		old_mode = INBYTE(AICREG(MODE_PTR));
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);

		lesb->link_fail_lsb = INBYTE(AICREG(LESB_WD0B0));
		lesb->link_fail_2 = INBYTE(AICREG(LESB_WD0B1));
		lesb->link_fail_3 = INBYTE(AICREG(LESB_WD0B2));
		lesb->link_fail_msb = INBYTE(AICREG(LESB_WD0B3));
		lesb->loss_of_synch_lsb = INBYTE(AICREG(LESB_WD1B0));
		lesb->loss_of_synch_2 = INBYTE(AICREG(LESB_WD1B1));
		lesb->loss_of_synch_3 = INBYTE(AICREG(LESB_WD1B2));
		lesb->loss_of_synch_msb = INBYTE(AICREG(LESB_WD1B3));
		lesb->los_of_signal_lsb = INBYTE(AICREG(LESB_WD2B0));
		lesb->los_of_signal_2 = INBYTE(AICREG(LESB_WD2B1));
		lesb->los_of_signal_3 = INBYTE(AICREG(LESB_WD2B2));
		lesb->los_of_signal_msb = INBYTE(AICREG(LESB_WD2B3));
		lesb->prim_seq_proto_err_lsb = INBYTE(AICREG(LESB_WD3B0));
		lesb->prim_seq_proto_err_2 = INBYTE(AICREG(LESB_WD3B1));
		lesb->prim_seq_proto_err_3 = INBYTE(AICREG(LESB_WD3B2));
		lesb->prim_seq_proto_err_msb = INBYTE(AICREG(LESB_WD3B3));
		lesb->bad_trans_word_lsb = INBYTE(AICREG(LESB_WD4B0));
		lesb->bad_trans_word_2 = INBYTE(AICREG(LESB_WD4B1));
		lesb->bad_trans_word_3 = INBYTE(AICREG(LESB_WD4B2));
		lesb->bad_trans_word_msb = INBYTE(AICREG(LESB_WD4B3));
		lesb->invalid_crc_lsb = INBYTE(AICREG(LESB_WD5B0));
		lesb->invalid_crc_2 = INBYTE(AICREG(LESB_WD5B1));
		lesb->invalid_crc_3 = INBYTE(AICREG(LESB_WD5B2));
		lesb->invalid_crc_msb = INBYTE(AICREG(LESB_WD5B3));

		OUTBYTE(AICREG(MODE_PTR), old_mode);
		sh_unpause_sequencer(c->base);
		break;
	}

	case (SH_READ_LOOP_STATUS): {
                UBYTE   old_mode;
		sh_pause_sequencer(c);
		old_mode = INBYTE(AICREG(MODE_PTR));
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);
		r->sub_func = (UBYTE) (INBYTE(AICREG(ALC_LPSM_STATUS_1)) & LPSM_STATE_MSK);
		OUTBYTE(AICREG(MODE_PTR), old_mode);
		sh_unpause_sequencer(c->base);
		break;
	}

	case (SH_ENABLE_INT_SWACT): {
                UBYTE   old_mode;
		sh_pause_sequencer(c);
		old_mode = INBYTE(AICREG(MODE_PTR));
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
		if (r->sub_func) {
					OUTBYTE(AICREG(CMC_SEQ_RTC1), INBYTE(AICREG(CMC_SEQ_RTC1)) | SW_ACT_INT);
		}
		else {
					OUTBYTE(AICREG(CMC_SEQ_RTC1), INBYTE(AICREG(CMC_SEQ_RTC1)) & ~SW_ACT_INT);
		}
		OUTBYTE(AICREG(MODE_PTR), old_mode);
		sh_unpause_sequencer(c->base);
		break;
	}

	case (SH_SETUP_INTERNAL_LOOPBACK): {
                UBYTE   old_mode;
		sh_pause_sequencer(c);
		old_mode = INBYTE(AICREG(MODE_PTR));
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE3);
		if (r->sub_func) {
					OUTBYTE(AICREG(RFC_MODE), INBYTE(AICREG(RFC_MODE)) | INTLPBK);
		}
		else {
					OUTBYTE(AICREG(RFC_MODE), INBYTE(AICREG(RFC_MODE)) & ~INTLPBK);
		}
			OUTBYTE(AICREG(LINK_ERRST), LINKRST);
			OUTBYTE(AICREG(LINK_ERRST), LINKEN);
		OUTBYTE(AICREG(MODE_PTR), old_mode);
		sh_unpause_sequencer(c->base);
		break;
	}
#endif
	default:
		status = SH_ILLEGAL_REQUEST;
		break;

	}


	return (status);
}
