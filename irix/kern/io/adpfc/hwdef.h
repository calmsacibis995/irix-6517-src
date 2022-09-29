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

#ifndef _HWDEF_H
#define _HWDEF_H

/*
*
*  Module Name:   hwdef.h
*
*  Description:   Internal Software/Sequencer Control Structures
*    
*  Owners:	  ECX IC Firmware Team
*    
*  Notes:	  
*
*/


/*
* Information available at tcb completion time
*/
typedef struct _doneq
{
#if PCI_BIGENDIAN
	UBYTE q_pass_count;		 /* queue pass count		  */
	UBYTE status;			 /* status associated with CMDCMP */
	UBYTE tcb_number_hi;		 /* tcb number			  */
	UBYTE tcb_number_lo;		 /* tcb number			  */
#else
	UBYTE tcb_number_lo;		 /* tcb number			  */
	UBYTE tcb_number_hi;		 /* tcb number			  */
	UBYTE status;			 /* status associated with CMDCMP */
	UBYTE q_pass_count;		 /* queue pass count		  */
#endif
} doneq;

#define FCADP_DMA_SIZE	8

#define SAVE_RESTORE_TCB 3

typedef struct _dma_stat
{
#if PCI_BIGENDIAN
#if OSM_DMA_SWAP_WIDTH32
	UBYTE stat3;
	UBYTE stat2;
	UBYTE stat1;
	UBYTE stat0;

	UBYTE resv0;
	UBYTE resv1;
	UBYTE resv2;
	UBYTE resv3;
#endif
#if OSM_DMA_SWAP_WIDTH64
	UBYTE resv3;
	UBYTE resv2;
	UBYTE resv1;
	UBYTE resv0;

	UBYTE stat3;
	UBYTE stat2;
	UBYTE stat1;
	UBYTE stat0;
#endif

#else /* little endian */

	UBYTE stat0;
	UBYTE stat1;
	UBYTE stat2;
	UBYTE stat3;

	UBYTE resv0;
	UBYTE resv1;
	UBYTE resv2;
	UBYTE resv3;
#endif /* little endian */
} dma_stat;

struct control_blk
{
	REGISTER base;		/* base address				*/
	REGISTER seqmembase;	/* base sequencer I/O memory address	*/
/* NEW */
	REGISTER pcicfgbase;	/* base address of PCI config space     */

	struct lip_tcb *saved_lip;
	struct reference_blk *r;
	struct shim_config *s;		/* under control of ULM		*/
	void *ulm_info;			/* for convenience of ULM	*/
	struct _tcb *t;			/* working tcb in memory	*/
	doneq *d;			/* current doneq element	*/
	doneq *d_front;			/* current doneq element front	*/
	doneq *qdone_end;		/* saved for optimization	*/

	dma_stat *dma;		/* pointer to area to dma int status	*/

	struct reference_blk ru;

	UBYTE	chip_version;	/* hardware revision			*/
	UBYTE	intstat;	/* interrupt state saved		*/
	UBYTE	intstat1;
	UBYTE	seq_type;	/* what version of sequencer code is up */
	UBYTE	primitive_ctrl; /* value to HST_LIP_CTL for primitive	*/
	USHORT number_tcbs;	/* number of tcbs configured		*/
	USHORT flag;

	USHORT	tcbs_for_sequencer;	/* HIM count (HNTCB_QOFF)	*/
	UBYTE	q_done_pass;
	UBYTE	q_done_pass_front;

	USHORT	lip_state;
	UBYTE	miatype_cs;	/* Mia type if current sensing		*/
	UBYTE	miatype_vs;	/* Mia type if voltage sensing		*/
/*	bit 0, 1, 2 of loop_fail_info are used to signal whether LOSync, 
 *	LOSig, MIA Fault been reported to ULM yet or not
 */
	UBYTE	loopfail_info;	
		
/*	save area during lip						*/
	UBYTE	q_new_pointer[8];
	UBYTE	q_exe_head[2];
	UBYTE	q_exe_tail[2];
	UBYTE	q_send_head[2];
	UBYTE	q_send_tail[2];
	UBYTE	q_empty_head[2];
	UBYTE	q_empty_tail[2];
	UBYTE	post_lip_pointer[8];
	USHORT	tcbs_for_sequencer_save;
	USHORT	sntcb_qoff; /* change back to USHORT when hardware fixed */
	UBYTE	pci_device_id; /* for DOS PCI access (temporarily)	*/

	UBYTE	our_aida0;
	UBYTE	error;			  /* copy of ERROR register	  */
	USHORT	lip_tcb_number;
};

/*
* pass to sh_access_scratch() to access SRAM
*/
struct access_scratch_ram
{
	REGISTER base;		/* base address				*/
	REGISTER seqmembase;	/* base sequencer I/O memory address	*/
	UBYTE rw;		/* =0, for read; otherwise write	*/
	UBYTE rwval;		/* read/write value			*/
	int pvar_offset;	/* byte offset to be accessed		*/
};

/*
* for saved/restored Emerald states
*/
struct emerald_states
{
	UBYTE	tcb[SAVE_RESTORE_TCB*256];	/* just save 3 TCBs */
	UBYTE	scratch[128];
	UBYTE	mtpeRam[4096];
	UBYTE	cipDmaAddr[8];
	UBYTE	ipen[2];
	UBYTE	cmcHctlInten;
	UBYTE	rpbPldSize;
	UBYTE	cipDmaEn;
	USHORT	tilpAddr;
	USHORT	sntcbQoff;
	USHORT	sdtcbQoff;
};

/* residual information dmaed from MTPE to host */
typedef struct _residual_info residual_info;
struct _residual_info
{
	BUS_ADDRESS next_sg_page;
	UBYTE current_residual0;
	UBYTE current_residual1;
	UBYTE current_residual2;
	UBYTE reserved;
	UBYTE current_sg_no;
};

/* these are used in flags field of control_blk		*/
#define PRIMITIVE_CHAINED	0x01
#define VALID_ALPA		0x02
#define LIP_DURING_LIP		0x08
#define USE_SAVED_LIP_TCB	0x10
#define PRIMITIVE_LIP_SENT	0x20
#define SEQUENCER_SUSPENDED	0x40
#define RESUME_IO_AFTER_LIP	0x80
#define SPECIAL_ABORT		0x100
#define BACK_TO_LIP		0x200

/* loopfail_info for reporting loop failure status	*/
#define LOSync_reported		0x01
#define LOSignal_reported	0x02
#define MIA_FAULT_reported	0x04

/* flags for lip_state							*/
#define NOT_READY_FOR_LIP		0
#define NO_LIP_IN_PROCESS		1
#define WAITING_FOR_LIP_QUIESCE		2
#define READY_TO_LIP			3
#define LIP_IN_PROCESS			4

/* Types of sequencer code (c->seq_type)				*/
#define SH_NO_SEQ_CODE		0
#define SH_NORMAL_SEQ_CODE	1
#define SH_LIP_SEQ_CODE		2
#define SH_DIAG_SEQ_CODE	3

#define SH_TAIL		0
#define SH_HEAD		1
#endif /* _HWDEF_H */
