/*
 *               Copyright (C) 1997 Silicon Graphics, Inc.                
 *                                                                        
 *  These coded instructions, statements, and computer programs  contain  
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  
 *  are protected by Federal copyright law.  They  may  not be disclosed  
 *  to  third  parties  or copied or duplicated in any form, in whole or  
 *  in part, without the prior written consent of Silicon Graphics, Inc.  
 *                                                                        
 *
 *  Filename:     st.h
 *  Description:  Scheduled Transfer Protocol Headers
 */

#include <sys/endian.h>
#include <sys/types.h>

#ifndef __ST_H__
#define __ST_H__

/* Scheduled Transfer (ST) Header Definitions */

/*
 * ST Bit Flags
 */
#define	ST_FLAG_MASK	0x7ff
#define	ST_REJECT	0x4
#define	ST_LAST		0x8
#define	ST_ORDER	0x10
#define	ST_SENDSTATE	0x20
#define	ST_INTERRUPT	0x40
#define	ST_SILENT	0x80
#define	ST_FetchInc	0x100
#define	ST_FetchDec	0x200
#define	ST_FetchClear	0x300
#define	ST_FetchCmplete	0x700
#define	ST_TOT_FLAGS	11

/*
 * ST parameters
 */
#define ST_MAX_VC 4


/*
 * ST OpCodes
 */
#define	ST_OPCODE_MASK	0xf800
#define	ST_OPCODE_SHIFT	11
#define	ST_RCONNECT	(0x01 << ST_OPCODE_SHIFT)/* request connection */
#define	ST_CANSWER	(0x02 << ST_OPCODE_SHIFT)/* connection answer */
#define	ST_RDISCONNECT	(0x03 << ST_OPCODE_SHIFT)/* request disconnect */
#define	ST_DANSWER	(0x04 << ST_OPCODE_SHIFT)/* disconnect answer */
#define	ST_DCOMPLETE	(0x05 << ST_OPCODE_SHIFT)/* disconnect complete */

#define	ST_NOP		(0x010 << ST_OPCODE_SHIFT)/* nop */
#define	ST_RMR		(0x013 << ST_OPCODE_SHIFT)/* request memory reg */
#define	ST_MRA		(0x014 << ST_OPCODE_SHIFT)/* memory reg available */
#define	ST_GET		(0x015 << ST_OPCODE_SHIFT)/* get,F&O, F&OComp */

#define	ST_RTS		(0x016 << ST_OPCODE_SHIFT)/* request to send */
#define	ST_RANSWER	(0x017 << ST_OPCODE_SHIFT)/* request answer */
#define	ST_RTR		(0x018 << ST_OPCODE_SHIFT)/* request to receive */
#define	ST_CTS		(0x01a << ST_OPCODE_SHIFT)/* clear to send */

#define	ST_DATA		(0x01b << ST_OPCODE_SHIFT)/* data operation */
#define	ST_RS		(0x01c << ST_OPCODE_SHIFT)/* request state */
#define	ST_RSR		(0x01d << ST_OPCODE_SHIFT)/* request state response */
#define	ST_END		(0x01e << ST_OPCODE_SHIFT)/* end operation */
#define	ST_END_ACK	(0x01f << ST_OPCODE_SHIFT)/* end ack */

/* Old names for compatibility, these will go away at some point */

#define ST_RMB	ST_RMR
#define ST_MBA	ST_MRA

/* 
*  the "local" operations (read(), write(), timeout, pin-success/fail...
*  are also modelled as opcodes; the defs. for those are not a part of
*  ST; so, they are defined in st_var.h 
*/



/*
 *  ST Header Structures
 * 
 *  ST Header Structure (based on opcode)     Typedef
 *  -------------------------------------------------------
 *  sth_request_connection_s	              sthdr_rc_t;
 *  sth_connection_answer_s	              sthdr_ca_t;
 *  sth_request_disconnect_s	              sthdr_rd_t;
 *  sth_disconnect_answer_s	              sthdr_da_t;
 *  sth_disconnect_complete_s	              sthdr_dc_t;
 *  sth_end_s                                 sthdr_end_t;
 *  sth_end_ack_s		              sthdr_eack_t;
 *  sth_request_to_send_s	              sthdr_rts_t;
 *  sth_request_answer_s	              sthdr_ra_t;
 *  sth_request_to_receive_s	              sthdr_rtr_t;
 *  sth_clear_to_send_s	                      sthdr_cts_t;
 *  sth_data_s		                      sthdr_data_t;
 *  sth_get_s			              sthdr_get_t;
 *  sth_request_memory_block_s                sthdr_rmb_t;
 *  sth_memory_block_available_s              sthdr_mba_t;
 *  sthdr_request_state_1_s                   sthdr_rs1_t;
 *  sthdr_request_state_response_1_s          sthdr_rsr1_t;
 *  sthdr_request_state_2_s                   sthdr_rs2_t;
 *  sthdr_request_state_response_2_s          sthdr_rsr2_t;
 *  sthdr_request_state_3_s                   sthdr_rs3_t;
 *  sthdr_request_state_response_3_s          sthdr_rsr3_t;
 *
 *
 *  ST Header Union:  sthdr_t
 */


typedef struct sthdr_request_connection_s {
	uint16_t	OpFlags, I_Slots;
	uint16_t	R_Port, I_Port; 
	uint32_t    	unused0;
	uint16_t    	Checksum, EtherType;
	uint32_t	I_Bufsize;
	uint32_t	I_Key;
	uint32_t	I_Max_Stu;
	uint32_t	unused[3];
} sthdr_rc_t;

typedef struct sthdr_connection_answer_s {
	uint16_t	OpFlags, R_Slots;
	uint16_t	I_Port, R_Port; 
	uint32_t	I_Key;
	uint16_t    	Checksum, unused0;
	uint32_t	R_Bufsize;
	uint32_t	R_Key;
	uint32_t	R_Max_Stu;
	uint32_t	unused[3];
} sthdr_ca_t;

typedef struct sthdr_request_disconnect_s {
	uint16_t	OpFlags, unused0;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;
	uint16_t	Checksum, unused1;
	uint32_t	unused2;
	uint32_t	I_Key;
	uint32_t	unused[4];
} sthdr_rd_t;

typedef struct sthdr_disconnect_answer_s {
	uint16_t	OpFlags, unused0;
	uint16_t	I_Port, R_Port; 
	uint32_t	I_Key;
	uint16_t	Checksum, unused1;
	uint32_t	unused2;
	uint32_t	R_Key;
	uint32_t	unused[4];
} sthdr_da_t;

typedef struct sthdr_disconnect_complete_s {
	uint16_t	OpFlags, unused0;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;
	uint16_t	Checksum, unused1;
	uint32_t	unused2;
	uint32_t	I_Key;
	uint32_t	unused[4];
} sthdr_dc_t;

typedef struct	sthdr_end	{
	uint16_t	OpFlags, unused0;
	uint16_t	R_port, I_port; 
	uint32_t	R_key;
	uint16_t	Checksum, unused1;
	uint32_t	unused[4];
	uint32_t	R_id, I_id;
} sthdr_end_t;

typedef struct	sthdr_end_ack_s {
	uint16_t	OpFlags, unused0;
	uint16_t	I_port, R_port; 
	uint32_t	I_key;
	uint16_t	Checksum, unused1;
	uint32_t	unused[4];
	uint32_t	I_id, R_id;
} sthdr_eack_t;

typedef struct  sthdr_request_to_send_s {
	uint16_t	OpFlags, CTS_req;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;
	uint16_t	Checksum, Max_Block;
	uint32_t	unused0, unused1;
	uint64_t	tlen;
	uint32_t	unused2;
	uint32_t	I_id;
} sthdr_rts_t;

typedef struct  sthdr_request_answer_s {
	uint16_t	OpFlags, unused0;
	uint16_t	I_Port, R_Port; 
	uint32_t	I_Key;
	uint16_t	Checksum, unused1;
	uint32_t	unused[4];
	uint32_t	I_id;
	uint32_t	unused2;
} sthdr_ra_t;

typedef struct  sthdr_clear_to_send_s {
	uint16_t	OpFlags, Blocksize;
	uint16_t	I_Port, R_Port; 
	uint32_t	I_Key;
	uint16_t	Checksum, R_Mx;
	uint32_t	R_Bufx, R_Offset;
	uint32_t	F_Offset;
	uint32_t	B_num;
	uint32_t	I_id;
	uint32_t	R_id;
} sthdr_cts_t;

typedef struct  sthdr_data_s {
	uint16_t	OpFlags, STU_num;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;		
	uint16_t	Checksum, R_Mx;
	uint32_t	R_Bufx, R_Offset;
	uint32_t	Sync;
	uint32_t	B_num;
	uint32_t	R_id;
	uint32_t	Opaque;
} sthdr_data_t;

typedef struct  sthdr_request_state_1_s {
	uint16_t	OpFlags, unused0;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;		
	uint16_t	Checksum, unused1;
	uint32_t	unused[2];
	uint32_t	Sync;
	uint32_t	unused2;
	uint32_t	minus_one;
	uint32_t	unused3;
} sthdr_rs1_t;

typedef struct  sthdr_request_state_response_1_s {
	uint16_t	OpFlags, R_Slots;
	uint16_t	I_Port, R_Port; 
	uint32_t	I_Key;		
	uint16_t	Checksum, unused0;
	uint32_t	unused[2];
	uint32_t	Sync;
	uint32_t	unused2;	/* unused for type 1 */
	uint32_t	minus_one;	
	uint32_t	unused1;
} sthdr_rsr1_t;

typedef struct  sthdr_request_state_2_s {
	uint16_t	OpFlags, unused0;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;		
	uint16_t	Checksum, unused1;
	uint32_t	unused[2];
	uint32_t	Sync;
	uint32_t	minus_one;
	uint32_t	R_id;
	uint32_t	I_id;
} sthdr_rs2_t;

typedef struct  sthdr_request_state_response_2_s {
	uint16_t	OpFlags, R_Slots;
	uint16_t	I_Port, R_Port; 
	uint32_t	I_Key;		
	uint16_t	Checksum, unused0;
	uint32_t	unused1;
	uint32_t	B_seq;
	uint32_t	Sync;
	uint32_t	minus_one;
	uint32_t	I_id;
	uint32_t	R_id;
} sthdr_rsr2_t;

typedef struct  sthdr_request_state_3_s {
	uint16_t	OpFlags, unused0;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;		
	uint16_t	Checksum, unused1;
	uint32_t	unused[2];
	uint32_t	Sync;
	uint32_t	B_num;
	uint32_t	R_id;
	uint32_t	I_id;
} sthdr_rs3_t;

typedef struct  sthdr_request_state_response_3_s {
	uint16_t	OpFlags, R_Slots;
	uint16_t	I_Port, R_Port; 
	uint32_t	I_Key;		
	uint16_t	Checksum, unused0;
	uint32_t	unused1;
	uint32_t	B_seq;
	uint32_t	Sync;
	uint32_t	B_num;
	uint32_t	I_id;
	uint32_t	R_id;
} sthdr_rsr3_t;

typedef struct  sthdr_get_s {
	uint16_t	OpFlags, T_len;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;
	uint16_t	Checksum, I_Mx;
	uint32_t	R_Bufx;
	uint32_t	R_Offset;
	uint32_t	I_Bufx;
	uint32_t	I_Offset;
	uint32_t	R_id;
	uint32_t	G_id;
} sthdr_get_t;

typedef struct  sthdr_request_to_receive_s {
	uint16_t	OpFlags, unused0;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;
	uint16_t	Checksum, unused1;
	uint32_t	unused[2];
	uint64_t	T_len;
	uint32_t	unused2;
	uint32_t	I_id;
} sthdr_rtr_t;

typedef struct  sthdr_request_memory_region_s {
	uint16_t	OpFlags, unused0;
	uint16_t	R_Port, I_Port; 
	uint32_t	R_Key;
	uint16_t	Checksum, unused1;
	uint32_t	unused[2];
	uint64_t	T_len;
	uint32_t	unused2;
	uint32_t	I_id;
} sthdr_rmr_t;

typedef struct  sthdr_memory_region_available_s {
    	uint16_t	OpFlags, unused0;
    	uint16_t	I_port, R_port; 
    	uint32_t	I_key;
	uint16_t	Checksum, R_Mx;
	uint32_t	R_Bufx;
	uint32_t	R_Offset;
	uint32_t	unused[2];
	uint32_t	I_id;
	uint32_t	R_id;
} sthdr_mra_t;

typedef struct {
	uint16_t	OpFlags;
	uint16_t	Param;
	uint16_t	D_Port;
	uint16_t	S_Port;
	uint32_t	D_Key;
	uint16_t	Cksum;
	uint16_t	B_id;
	uint32_t	Bufx;
	uint32_t	Offset;
	uint32_t	Sync;
	uint32_t	B_num;
	uint32_t	D_id;
	uint32_t	S_id;
} sthdr_general_t;

typedef union {
	sthdr_general_t	sth;
	sthdr_rc_t	sth_rc; 
	sthdr_ca_t	sth_ca; 
	sthdr_rts_t	sth_rts; 
	sthdr_ra_t	sth_ra;
	sthdr_rtr_t	sth_rtr;
	sthdr_cts_t	sth_cts;
	sthdr_data_t	sth_data; 
	sthdr_rs1_t	sth_rs1;
	sthdr_rsr1_t	sth_rsr1;
	sthdr_rs2_t	sth_rs2;
	sthdr_rsr2_t	sth_rsr2;
	sthdr_rs3_t	sth_rs3;
	sthdr_rsr3_t	sth_rsr3;
	sthdr_get_t	sth_get; 
	sthdr_rmr_t	sth_rmr; 
	sthdr_mra_t	sth_mra;
	sthdr_rd_t	sth_rd; 
	sthdr_dc_t	sth_dc; 
	sthdr_da_t	sth_da; 
} sthdr_t;


/* For H800-FW debug only; 
*  THIS STRUCT WILL BE DELETED REAL SOON NOW!!!
*  Use at your own risk!
*
*  From rev 1.45 of the ST spec. */
typedef struct st_hdr_s {
    	uint16_t	op	: 5,
    			flags	: 11;
    	uint16_t	param;
    	uint16_t	d_port;
    	uint16_t	s_port;
    	uint32_t	d_key;
    	uint16_t	cksum;
    	uint16_t	mx;
    	uint32_t	bufx;
    	uint32_t	offset;
    	uint32_t	sync;
    	uint32_t	b_num;
    	uint32_t	d_id;
    	uint32_t	s_id;
    	uint32_t    unused;
} st_hdr_t;


/* get/set operations that should be in ST */
/* kernel sockets */
#define	ST_CTS_OUTSTD		1
#define	ST_BLKSZ		2
#define	ST_BUFSZ		3
#define	ST_OUT_VCNUM		4
#define	ST_TX_SPRAY_WIDTH	5
#define	ST_RX_SPRAY_WIDTH	6
#define	ST_NUM_SLOTS		7
#define	ST_ROTATE_DATA_VC 	8
#define	ST_UNROTATE_DATA_VC 	9
#define ST_MEMALLOC_POOL        10 
#define ST_STUSZ                11

/* Error values for upper layer */
#define EBADIFP     90
#define EBADBUFSIZE 91   

/* ST ULP MX errors */
#define EBADMX      101  /* An mx outside the st_ifnet, port, or bufx range */
#define EMXPOISON   102  /* A non-zero poison bit was given with the mx */
#define EMXSTUNUM   103  /* A non-zero stu-num was given with the mx */
#define EMXBADKEY   104  /* A key of 0 was given in set_mx */
#define EMXBADPORT  105  /* A port outside the st_ifp range in set_mx */
#define EMXBADBUFX  106  /* A bufx outside the st_ifp range in set_mx */

/* ST ULP PORT errors */
#define EBADPORT    110  /* A port outside of the st_ifp range was received */
#define EPORTBADBUFX 111 /* A bufx outside the st_ifp range in set_bufx */
#define EPORTBADKEY 112  /* A key of 0 was given in set_port */
#define EPORTNOMEM  113  /* Could not create descriptor Q for set_port */
#define EPORTDESSIZE 114 /* Port descriptor size is not a SHAC descriptor integral */

/* ST ULP BUFX errors */
#define EBADBUFX    120  /* A Bufx value outside the defined st_ifp range */
#define EBADSPRAY   121  /* Spray width is not legal for GSN */
#define EBUFXBADFLAGS 122 /* No flags value is supported by GSN bufx calls */

/* ST Output errors */
#define EBADVC      130  /* Invalid VC selected for message type/length */


/* begin OS bypass get/set socket options */

#define ST_BYPASS	1000	/* set/get bypass mode */
#define ST_L_KEY	1001	/* set/get local key */
#define ST_R_KEY	1002	/* ---/get remote key */
#define ST_L_NUMSLOTS	1003	/* set/get local ddq size */
#define ST_R_NUMSLOTS	1004	/* ---/get remote ddq size */
#define ST_V_NUMSLOTS	1005	/* set/get local exported ddq size */
#define ST_L_PORT	1006	/* set/get local physical port */	/* get only, use bind */
#define ST_R_PORT	1007	/* ---/get remote physical port */
#define ST_L_MAXSTU	1008	/* ---/get local max STU size */
#define ST_R_MAXSTU	1009	/* ---/get remote max STU size */
#define ST_L_BUFSIZE	1010	/* ---/get local max buf size */
#define ST_R_BUFSIZE	1011	/* ---/get remote max buf size */

#define ST_BUFX_ALLOC 1020	/* set/--- allocate a bufx range */
#define ST_BUFX_FREE  1021	/* set/--- free a bufx range */

#define ST_BUFX_MAP   1022	/* set/--- pin & map a buffer to a bufx range */
#define ST_BUFX_UNMAP 1023	/* set/--- unmap a buffer */

typedef struct st_bufx_alloc_args {
	uint64_t bufx_range;
	uint64_t bufx_base_addr;
	uint64_t bufx_base;
	uint64_t bufx_cookie_addr;
	uint64_t bufx_cookie;
} st_bufx_alloc_args_t;

typedef struct st_bufx_map_args {
	uint64_t flags;
#define ST_BUFX_MAP_TX (1<<0)
#define ST_BUFX_MAP_RX (1<<1)
	uint64_t buffer;
	uint64_t length;
	uint64_t bufx_base;
	uint64_t bufx_range;
	uint64_t bufx_cookie;
	uint64_t mx_addr;
	uint16_t mx;
} st_bufx_map_args_t;

#endif	/* __ST_H__ */

