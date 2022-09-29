/***************************************************************************
*									   *
* Copyright 1998 Adaptec, Inc.,  All Rights Reserved.			   *
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
*  Module Name:   ulmconfi.h
*
*  Description:   Slim him definitions for ULM
*    
*  Owners:	  ECX IC Firmware Team
*    
*  Notes:	  
*
*/

#ifndef _ULMCONFI_H
#define _ULMCONFI_H

/* shim_config structure				*/

struct shim_config {
	REGISTER base_address;		/* Base memory map address	     */
	REGISTER seqmembase;		/* Base I/O  address		     */
/* NEW */
	REGISTER pcicfgbase;	        /* base address of PCI config space  */
	struct control_blk  *control_blk; /* pointer to control_blk structure */
	void *doneq;			/* doneq array in host memory	     */
	void *first_tcb;		/* priming tcb in host memory	     */
	void *primitive_tcb;		/* tcb in host memory for primitives */
	void *dma;			/* pointer to status dma area	     */
	void *ulm_info;			/* for convenience of ULM	*/
	USHORT control_blk_size;	/* size of SHIM private structure    */
	USHORT done_q_size;		/* size of doneq in host memory      */
	USHORT number_tcbs ;		/* total of tcb SRAM locations	     */
	USHORT num_unsol_tcbs;		/* tcbs resv. for unsolicited frames */
	USHORT slimhim_version;		/* Version of SLIM HIM		     */
	USHORT recv_payload_size;	/* 128, 256, 512, 1024 or 2048	     */
	USHORT emerald_states_size;	/* size of memory for emerald states */
	UBYTE al_pa[3];		
	UBYTE flag;
	UBYTE doneq_elem_size;		/* Size of one doneq element	     */
	UBYTE link_speed ;		/* one(1G), half(.5G), quarter(.25G) */
	UBYTE lip_timeout;		/* unit = 154 us (default = 0x60)    */
	UBYTE r_rdy_timeout;		/* unit = 2.4 ms (default = 0xff)    */
	UBYTE chip_version;	
	UBYTE hcommand1;		/* work arround for some emeralds    */
					/* used if SH_USE_HCOMMAND1 is set   */     
}; 

/* Definitions for flag field of shim_config */
#define SH_SET_ALPA			0x01
#define SH_POWERUP_INITIALIZATION	0x02
#define CURRENT_SENSING			0x04
#define SH_ENABLE_SYNC_C_S		0x08
#define SH_USE_HCOMMAND1		0x10

struct reference_blk {
	void *control_blk;	/* pointer to control_blk  structure	*/
	struct _tcb *tcb;
	USHORT tcb_number;
	UBYTE func;
	UBYTE sub_func;
	UBYTE done_status;	/* status on one tcb request		*/
};

struct link_error_status_blk {
	UBYTE link_fail_lsb;
	UBYTE link_fail_2;
	UBYTE link_fail_3;
	UBYTE link_fail_msb;
	UBYTE loss_of_synch_msb;
	UBYTE loss_of_synch_2;
	UBYTE loss_of_synch_3;
	UBYTE loss_of_synch_lsb;
	UBYTE los_of_signal_lsb;
	UBYTE los_of_signal_2;
	UBYTE los_of_signal_3;
	UBYTE los_of_signal_msb;
	UBYTE prim_seq_proto_err_msb;
	UBYTE prim_seq_proto_err_2;
	UBYTE prim_seq_proto_err_3;
	UBYTE prim_seq_proto_err_lsb;
	UBYTE bad_trans_word_msb;
	UBYTE bad_trans_word_2;
	UBYTE bad_trans_word_3;
	UBYTE bad_trans_word_lsb;
	UBYTE invalid_crc_msb;
	UBYTE invalid_crc_2;
	UBYTE invalid_crc_3;
	UBYTE invalid_crc_lsb;
};

/* The ref_blk which requests link errors should point
* tcb to a struct of type link_error_status_blk.
* func is SH_READ_LINK_ERRORS.
* If the structure is filled in, done_status will be 0,
* else non-zero.
* Errors begin at 0 after chip reset, then counters 
* increment until they roll over to 0 again a 2^32
*/

/* definitions for done_status in reference_blk set by sequencer	*/
#define TCB_NO_ERROR		0x00 /* no error			    */
#define TCB_OVERRUN		0x01 /* data overrun			    */
#define TCB_UNDERRUN		0x02 /* data underrun			    */
#define TCB_PARITY_ERROR	0x03 /* parity error in the TCB		    */
#define TCB_ERROR_RESPONSE	0x04 /* Error Response received from target */
#define TCB_SEQ_ERROR		0x05 /* frame received out of order	    */
#define TCB_FRAME_ERROR		0x06 /* frame format error		    */
#define TCB_MAX_CNT_ERROR	0x07 /* Overflow: SEQ_ID SEQ_CNT or rel_off */ 
#define TCB_UNDELIVERED		0x08 /* frame we sent came back undelivered */
#define TCB_SOF_ERROR		0x09 /* SOF is not normal		    */
#define TCB_EOF_ERROR		0x0a /* EOF is not normal		    */
#define TCB_ABORTED_TCB		0x0b /* Execution has been aborted	    */
#define TCB_PRIMITIVE_TIMEOUT	0x0f /* The primitive timed out		    */

/* definitions for done_status for LIP tcb only				*/
#define LIP_TIMEOUT		5
#define LIP_NON_PARTICIPATING	6
#define LIP_HARDWARE_ERROR	7
#define LIP_OLD_PORT_STATE	9
#define LIP_UNEXPECTED_CLS	10


/* definitions of the reference_blk func				*/
#define SH_PRIMITIVE_REQUEST		0
#define SH_ABORT_REQUEST		1
#define SH_SETUP_SERDES_LOOPBACK	2
#define SH_SETUP_LRC			3
#define SH_SETUP_RFC_HOLD		4
#define SH_DELIVER_LIP_TCB		5
#define SH_RESTORE_QUEUES		6
#define SH_READ_LINK_ERRORS		9
#define SH_READ_LOOP_STATUS	       10
#define SH_ENABLE_INT_SWACT	       11
#define SH_SETUP_INTERNAL_LOOPBACK     12

/* sub_func definitions for primitive reference_blk	*/
#define LIP_TYPE_PRIMITIVE	1

/* Error codes returned by ulm_event()					*/
/* These codes must be changed together with strings in hwintr.c	*/
#define EVENT_UNK_ERR				0
#define EVENT_FCAL_LIP				1
#define EVENT_RELOAD_FAILED			2
#define EVENT_BYPASS				3
#define EVENT_ON_LOOP				4
#define EVENT_READY_FOR_LIP_TCB			5
#define EVENT_SEQ_OP_CODE_ERR			6
#define EVENT_SEQ_PAR_ERR			7
#define EVENT_RPB_PAR_ERR			8
#define EVENT_SPB_TO_SFC_PAR_ERR		9
#define EVENT_RPB_TO_HST_PAR_ERR		10
#define EVENT_CMC_RAM_PAR_ERR			11
#define EVENT_MEM_PORT_PAR_ERR			12
#define EVENT_CMC_TO_HST_PAR_ERR		13
#define EVENT_PCI_STATUS1_DPR			14
#define EVENT_PCI_ERROR0_HR_DMA_DPR		15
#define EVENT_PCI_ERROR1_CP_DMA_DPR		16
#define EVENT_PCI_ERROR1_CIP_DMA_DPR		17
#define EVENT_PCI_STATUS1_STA			18
#define EVENT_PCI_STATUS1_RTA			19
#define EVENT_PCI_ERROR0_HS_DMA_RTA		20
#define EVENT_PCI_ERROR0_HR_DMA_RTA		21
#define EVENT_PCI_ERROR1_CP_DMA_RTA		22
#define EVENT_PCI_ERROR1_CIP_DMA_RTA		23
#define EVENT_PCI_STATUS1_RMA			24
#define EVENT_PCI_ERROR0_HS_DMA_RMA		25
#define EVENT_PCI_ERROR0_HR_DMA_RMA		26
#define EVENT_PCI_ERROR1_CP_DMA_RMA		27
#define EVENT_PCI_ERROR1_CIP_DMA_RMA		28
#define EVENT_PCI_STATUS1_SERR			29
#define EVENT_PCI_STATUS1_DPE			30
#define EVENT_PCI_ERROR0_HS_DMA_DPE		31
#define EVENT_PCI_ERROR1_CP_DMA_DPE		32
#define EVENT_PCI_ERROR1_CIP_DMA_DPE		33
#define EVENT_PCI_ERROR2_T_DPE			34
#define EVENT_LPSM_ERROR			35
#define EVENT_SERDES_LKNUSE			36
#define EVENT_SERDES_FAULT			37
#define EVENT_LINK_ERRST_LOSIG			38
#define EVENT_LINK_ERRST_LOSIG_LIP		39
#define EVENT_LINK_ERRST_LOSYNTOT		40
#define EVENT_LINK_ERRST_LOSYNTOT_LIP		41
#define EVENT_SYNC_SPINDLE			42
#define EVENT_SYNC_CLOCK			43


#define SLIM_HIM_VERSION                       101
#endif /* _ULMCONFI_H */
