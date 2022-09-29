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

#ifndef _HWTCB_H
#define _HWTCB_H

/***************************************************************************
*
*  file Name:	hwtcb.h
*
*  Description:   Definitions for TCB format
*    
*  Owners:
*    
*  Notes:	  1. Definitions for TCB layout
*		  2. Depending on hardware or mode of operation the layout
*		     of TCB may be different. The layout is defined in
*		     hardware management layer. It is translation management
*		     layer's responsibility to make sure TCB information
*		     get filled properly.
*		  3. Modify CUSTOM.H to enable/disable a particular tcb
*		     format
*
***************************************************************************/

/***************************************************************************
*  Emerald 128 byte TCB format layout
***************************************************************************/


#ifdef PACKA_PUSH
#pragma pack(push)
#endif

#ifdef PACKA_MAC_OS
#pragma options aligned=packed
#endif

#ifdef PACKA__RECOVER /* else 0 */
#pragma pack()
#else /* else 0 */
	#ifdef PACKA_0 /* else 1 */
	#pragma pack(0)
	#else /* else 1 */
		#ifdef PACKA_2 /* else 2 */
		#pragma pack(2)
		#else /* else 2 */
			#ifdef PACKA_4 /* else 3 */
			#pragma pack(4)
			#else /* else 3 */
				#ifdef PACKA_8 /* else 4 */
				#pragma pack(8)
				#else /* else 4 */
					#ifndef PACKA_NONE /* else 5 */
					#pragma pack(1) /* default */
					#endif /* else 5 */
				#endif /* else 4 */
			#endif /* else 3 */
		#endif /* else 2 */
	#endif /* else 1 */
#endif /* else 0 */

typedef struct _frame_head frame_head ;   /* outbound frame header		*/
struct _frame_head
{
  UBYTE our_xid0;	
  UBYTE our_xid1; 
  UBYTE their_xid0;    
  UBYTE their_xid1;

  UBYTE their_aid0;   
  UBYTE their_aid1;
  UBYTE their_aid2;
  UBYTE r_ctl;
  
  UBYTE cs_ctl;
  UBYTE delimiter;    
  UBYTE dat_ep_epctl;
  UBYTE dat_ep_seqid;

  UBYTE dat_ep_seqcnt0;
  UBYTE dat_ep_seqcnt1;
  UBYTE f_ctl2;      
  UBYTE type;
  
  UBYTE f_ctl0;
  UBYTE f_ctl1;
  UBYTE df_ctl;
  UBYTE seq_id;
  
  UBYTE seq_cnt0;
  UBYTE seq_cnt1;
  UBYTE param0;
  UBYTE param1;

  UBYTE param2;
  UBYTE param3;        
  UBYTE their_bb_credit0;
  UBYTE their_bb_credit1;

  UBYTE seq_cfg1;
  UBYTE reserved[3];
};
/* definition for delimiter					*/
#define CLASS_THREE_NORMAL 0x05

/* definition for f_ctl2					*/
#define FCTL2_FIRST_SEQUENCE	0x20
#define FCTL2_LAST_SEQUENCE	0x10
#define FCTL2_END_SEQUENCE	0x08

/* definition for type						*/
#define EXTENDED_LINK_SERVICE_TYPE	1

typedef struct _eframe_head eframe_head ;   /* expected frame header */ 
struct _eframe_head
{
	UBYTE	our_xid0;
	UBYTE	our_xid1;
	UBYTE	their_xid0;
	UBYTE	their_xid1;
	UBYTE	their_aid0;
	UBYTE	their_aid1;
	UBYTE	their_aid2;
	UBYTE	r_ctl;
	UBYTE	cs_ctl;
	UBYTE	delimiter;
	UBYTE	epctl;
	UBYTE	seq_id;
	UBYTE	seq_cnt0;
	UBYTE	seq_cnt1;
	UBYTE	reserved;
	UBYTE	type;
};

typedef struct _uframe_head uframe_head ; /* 22 byte unsolicited frame header */
struct _uframe_head
{
	UBYTE	their_aid0;	
	UBYTE	their_aid1;
	UBYTE	their_aid2;
	UBYTE	reserved; 
	UBYTE	f_ctl0; 
	UBYTE	f_ctl1;
	UBYTE	f_ctl2;
	UBYTE	type;
	UBYTE	df_ctl;
	UBYTE	seq_id;
	UBYTE	seq_cnt0;
	UBYTE	seq_cnt1;
	UBYTE	their_xid0;	
	UBYTE	their_xid1;
	UBYTE	our_xid0;	
	UBYTE	our_xid1;
	UBYTE	param0;
	UBYTE	param1;
	UBYTE	param2;
	UBYTE	param3;
	UBYTE	r_ctl;
	UBYTE	delimiter;
};

typedef struct _fcp_cntl fcp_cntl;
struct _fcp_cntl
{
  UBYTE reserved;
  UBYTE task_attr;	 /* Task Codes bit 0,1,2 */
  UBYTE task_cntrl;
  UBYTE rwrt_data;	 /* bit 0,1, bits 2-7 reserved */
};

/* byte 0 RDWRT_DTAT defines */
#define READ_DATA  0x02
#define WRITE_DATA 0x01

/* byte 1  TASK_CNTRL defines */
#define ABT_TSK_SET   0x02     /* bit 1 */
#define CLR_TSK_SET   0x04     /* bit 2 */
#define TARGET_RESET  0x20     /* bit 5 */
#define CLEAR_ACA     0x40     /* bit 6 */
#define TERMIN_TASK   0x80     /* bit 7 */

/* byte 2  TASK_ATTRIBUTE, defines  */
#define SIMPLE_Q    0x00 ;     /*  0 0 0 */ 
#define HEAD_OF_Q   0x01 ;     /*  0 0 1 */
#define ORDERED_Q   0x02 ;     /*  0 1 0 */
#define ACA_Q	    0x04 ;     /*  1 0 0 */
#define UNTAGGED    0x05 ;     /*  1 0 1 */

typedef struct _fcp_cmnd fcp_cmnd ;   /* SCSI Fibre Channel Protocol (FCP)*/
struct _fcp_cmnd		      /* Command Frame Payload Template   */
{
  UBYTE fcp_lun_0;		      /* Logical Unit Nimber 8 bytes  */
  UBYTE fcp_lun_1;
  UBYTE fcp_lun_2;
  UBYTE fcp_lun_3;
  UBYTE fcp_lun_4;
  UBYTE fcp_lun_5;
  UBYTE fcp_lun_6;
  UBYTE fcp_lun_7;

  fcp_cntl scsi_cntl;	       /* Control Field			 */

  UBYTE  fcp_cdb[16] ;	       /* SCSI COmmand Descriptor Block  */
				    
  UBYTE fcp_dl0;	       /* Data Length  4 bytes		 */
  UBYTE fcp_dl1;
  UBYTE fcp_dl2;
  UBYTE fcp_dl3;		    
};

typedef struct _fcp_status  fcp_status ;
struct _fcp_status
{

 UBYTE	fcp_scsi_stat;	       /* scsi status byte returned by target */
 UBYTE	fcp_rsp_stat;	       /* see below for defines */
 UBYTE	reservd0 ;
 UBYTE	reservd1 ;

};

/* byte 1 defines */
#define FCP_RSP_LEN_VALID 0x00 
#define FCP_SNS_LEN_VALID 0x02 
#define FCP_RESID_OVER	  0x04
#define FCP_RESID_UNDER   0x08;

typedef struct _fcp_rsp fcp_rsp ;
struct _fcp_rsp
{
 UBYTE	  fcp_reserv0;
 UBYTE	  fcp_reserv1;
 UBYTE	  fcp_reserv2;
 UBYTE	  fcp_reserv3;
 UBYTE	  fcp_reserv4;
 UBYTE	  fcp_reserv5;
 UBYTE	  fcp_reserv6;
 UBYTE	  fcp_reserv7;

 fcp_status scsi_rsp;	       /* response from scsi target  */
 
 UBYTE	  fcp_resid0;
 UBYTE	  fcp_resid1;
 UBYTE	  fcp_resid2;
 UBYTE	  fcp_resid3;

 UBYTE	  fcp_sns_len0;
 UBYTE	  fcp_sns_len1;
 UBYTE	  fcp_sns_len2;
 UBYTE	  fcp_sns_len3;

 UBYTE	  fcp_rsp_len0;
 UBYTE	  fcp_rsp_len1;
 UBYTE	  fcp_rsp_len2;
 UBYTE	  fcp_rsp_len3;

 UBYTE	  fcp_rsp_info[8];     /* fcp_rsp_len  0,4 or 8 */	 
 UBYTE	  fcp_sns_info[256];   /* fcp_sns_len,	depending on Sense length */

};

typedef union _fcp_cmnd_rsp fcp_cmnd_rsp;
union _fcp_cmnd_rsp
{
 fcp_cmnd scsi_cmd ;	       /* 32 byte scsi command block  */
 fcp_rsp  scsi_rsp ;	       /* 8 byte  scsi response block */
};

/* the first tcb array site in SRAM is reserved for the primitive tcbs */
#define PRIM_TCB	0

/* tcb for high priority primitve requests			*/
struct primitive_tcb
{
	UBYTE	reserved[36];

	UBYTE	arb_request;
	UBYTE	al_pd;
	UBYTE	opn_prim;
	UBYTE	ulm_prim;
	UBYTE	cls_prim;
	UBYTE	alc_control;
	UBYTE	prim_control;

	UBYTE	reserved1[85];
};

/* values for alc_control */
#define FIRST_PRIMITIVE_TCB 0xff
#define LAST_PRIMITIVE_TCB 0xf7

/* values for arb_request */
#define PRIM_NO_ARB	0
#define PRIM_ARB_FAIR	1
#define PRIM_ARB_UNFAIR 2

/* values for opn_prim ulm_prim second_prim */
#define RESET_LPSM		0x80
#define PARTICIPATING		0x40
#define LOOP_MASTER		0x20
#define REQ_MONITOR		0x00
#define REQ_ARB_FAIR		0x01
#define REQ_ARB_UNFAIR		0x02
#define REQ_OPNYX		0x03
#define REQ_OPNYY		0x04
#define REQ_OPNFR		0x05
#define REQ_OPNYR		0x06
#define REQ_CLOSE		0x07
#define REQ_BYPASS		0x0b
#define REQ_BYPASSY		0x0c
#define REQ_ENABLE		0x0d
#define REQ_ENABLEY		0x0e
#define REQ_ENABLE_ALL		0x0f
#define REQ_INITIALIZE		0x10
#define REQ_OLD_PORT		0x11
#define REQ_PARTICIPATING	0x12
#define REQ_NON_PARTICIPATING	0x13
#define REQ_INITIALIZE_RESET	0x14
#define REQ_NON_ACTIVE		0x1f

typedef struct _tcb tcb;
struct _tcb /* offsets in hex */
{
/*00*/	frame_head fhead;	/* Frame Header (33 Bytes) defined */
	UBYTE	reserved;
	UBYTE	control;	/* type 0 = Normal (Initiator command+Data) */
	UBYTE	reserved0;
	UBYTE	sg_cache_ptr;	/* s/g management cachePtr	   */

/*24*/	fcp_cmnd  scsi_cmd ;	/* SCSI Cmd 32 Bytes defined	  */

/*44*/	UBYTE	next_tcb_addr0;
	UBYTE	next_tcb_addr1;
	UBYTE	next_tcb_addr2;
	UBYTE	next_tcb_addr3;
	UBYTE	next_tcb_addr4;  
	UBYTE	next_tcb_addr5;
	UBYTE	next_tcb_addr6;
	UBYTE	next_tcb_addr7;

/*4C*/	UBYTE	sg_list_addr0;	
	UBYTE	sg_list_addr1;
	UBYTE	sg_list_addr2;
	UBYTE	sg_list_addr3;
	UBYTE	sg_list_addr4;
	UBYTE	sg_list_addr5;
	UBYTE	sg_list_addr6;
	UBYTE	sg_list_addr7;
/*54*/	UBYTE	sg_list_addr8;	  
	UBYTE	sg_list_addr9;
	UBYTE	sg_list_addra;

/*57*/	UBYTE	reserved1;

/*58*/	UBYTE	info_host_addr0;
	UBYTE	info_host_addr1;
	UBYTE	info_host_addr2;
	UBYTE	info_host_addr3;
	UBYTE	info_host_addr4;
	UBYTE	info_host_addr5;
	UBYTE	info_host_addr6;
	UBYTE	info_host_addr7;

/*60*/	UBYTE	xfer_rdy_count0;	/* transfer_ready returned by target */
	UBYTE	xfer_rdy_count1;	/* space reserved for Sequencer use  */
	UBYTE	xfer_rdy_count2;
	UBYTE	xfer_rdy_count3;

/*64*/	UBYTE	hst_cfg_ctl;		/* Emerald register hst_cfg_ctl */
	UBYTE	reserved2[3];

/*68*/	USHORT	tcb_index;
/*6C*/	UBYTE	reserved3[22];
};

/* definitions for control byte */

/* bits 0,1 */
#define CLASS1_ESTCON  0x00	  /* establish class_1 connection */
#define CLASS1_DATA    0x01
#define CLASS2_DATA    0x02
#define CLASS3_DATA    0x03

/* bit 2 */
#define DATA_DIRECTION 0x04

/* bit 3,4,5 */

/* bit 7 */
#define XMIT_HOLD      0x40

/* definitions for control field of link tcb */
#define SEND_LINK_CMD_RCV_LINK_REPLY	0x0a 

/* defines for link_cmd_opcode (at address scsi_cmd in link tcb) */
#define XFER_LINK_CMD			0
#define ABORT_VICTIM_TCB		1

/* Definition for sg_cache_ptr					*/
#define EMBEDDED_SG_LIST 0x08

/* For the lip tcb, this must be in sync with tab.inc of sequencer */
#define LIP_TABLE_PAGE_OFFSET	0x41

/* value to indicate end of scatter gather list for external sg list	*/
#define SG_LIST_END	0xed
/* value to indicate there are more sg elements to follow		*/
#define SG_LIST_MORE	0x0
#define LIP_TABLE15_SIZE	128
struct lip_tcb
{
/* 0 */ UBYTE xfer_rdy_count0;	    /* initialize to 132 for LIRP,LILP */
	UBYTE xfer_rdy_count1;
	UBYTE xfer_rdy_count2;
	UBYTE xfer_rdy_count3;
	UBYTE hst_cfg_ctl;
	UBYTE reserved;
	UBYTE flag;
	UBYTE table_page;	    /* external ram page number in PVAR */

/*08*/	frame_head fhead;	    /* 32 bytes */

	UBYTE reserved1;
	UBYTE done_status;	    /* for non_participating == 0x06 */
	UBYTE fabric_ALPA0;
	UBYTE fabric_ALPA1;
	UBYTE prev_ALPA0;
	UBYTE prev_ALPA1;
	UBYTE hard_ALPA0;
	UBYTE hard_ALPA1;

/*30*/	UBYTE portname0;
	UBYTE portname1;
	UBYTE portname2;
	UBYTE portname3;
	UBYTE portname4;
	UBYTE portname5;
	UBYTE portname6;
	UBYTE portname7;
	
/*38*/	UBYTE	 info_host_addr0;	   
	UBYTE	 info_host_addr1;
	UBYTE	 info_host_addr2;
	UBYTE	 info_host_addr3;
	UBYTE	 info_host_addr4;
	UBYTE	 info_host_addr5;
	UBYTE	 info_host_addr6;
	UBYTE	 info_host_addr7;

/*40*/	UBYTE reserved2[4];

/*44*/	UBYTE	next_tcb_addr0;
	UBYTE	next_tcb_addr1;
	UBYTE	next_tcb_addr2;
	UBYTE	next_tcb_addr3;
	UBYTE	next_tcb_addr4;  
	UBYTE	next_tcb_addr5;
	UBYTE	next_tcb_addr6;
	UBYTE	next_tcb_addr7;

	UBYTE reserved3[52];
};

/* definitions for flag field */
#define SH_RESUME_IO		1
#define SH_BACKWARD_LISA	2	/*checked by MTPE to do backward LISA*/

#ifdef PACKB_RECOVER /* else 0 */
#pragma pack()
#else /* else 0 */
	#ifdef PACKB_1 /* else 1 */
	#pragma pack(1)
	#else /* else 1 */
		#ifdef PACKB_2 /* else 2 */
		#pragma pack(2)
		#else /* else 2 */
			#ifdef PACKB_4 /* else 3 */
			#pragma pack(4)
			#else /* else 3 */
				#ifdef PACKB_8 /* else 4 */
				#pragma pack(8)
				#else /* else 4 */
					#ifndef PACKB_NONE /* else 5 */
					#pragma pack(0) /* default */
					#endif /* else 5 */
				#endif /* else 4 */
			#endif /* else 3 */
		#endif /* else 2 */
	#endif /* else 1 */
#endif /* else 0 */

#ifdef PACKB_MAC_OS
#pragma options aligned=reset
#endif

#ifdef PACKB_POP
#pragma pack(pop)
#endif

#endif /* _HWTCB.H */
