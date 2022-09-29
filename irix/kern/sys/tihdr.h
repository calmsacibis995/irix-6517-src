/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TIHDR_H		/* wrapper symbol for kernel use */
#define _SYS_TIHDR_H		/* subject to change without notice */
#ifdef __cplusplus
extern "C" {
#endif

#ident	"@(#)uts-comm:net/ktli/tihdr.h	1.4"

/*
 * The following is all the information
 * needed by the Transport Service Interface.
 */

#ifdef _KERNEL

#include <sys/stream.h>	/* REQUIRED */

#endif /* _KERNEL */


/* 
 * The following are the definitions of the Transport
 * Service Interface primitives.
 */

/* 
 * Primitives that are initiated by the transport user.
 */

#define	T_CONN_REQ	0	/* connection request     */
#define T_CONN_RES	1	/* connection response    */
#define T_DISCON_REQ	2	/* disconnect request     */
#define T_DATA_REQ	3	/* data request	          */
#define T_EXDATA_REQ	4	/* expedited data request */
#define T_INFO_REQ	5	/* information request    */
#define T_BIND_REQ	6	/* bind request		  */
#define T_UNBIND_REQ	7	/* unbind request	  */
#define T_UNITDATA_REQ	8	/* unitdata request       */
#define T_OPTMGMT_REQ   9	/* manage options req     */
#define T_ORDREL_REQ   10       /* orderly release req    */

/* 
 * Primitives that are initiated by the transport provider.
 */

#define T_CONN_IND	11	/* connection indication      */
#define T_CONN_CON	12	/* connection confirmation    */
#define T_DISCON_IND	13	/* disconnect indication      */
#define T_DATA_IND	14	/* data indication	      */
#define T_EXDATA_IND	15	/* expeditied data indication */
#define T_INFO_ACK	16	/* information acknowledgment */
#define T_BIND_ACK	17	/* bind acknowledment	      */
#define T_ERROR_ACK	18	/* error acknowledgment       */
#define T_OK_ACK	19	/* ok acknowledgment          */
#define T_UNITDATA_IND	20	/* unitdata indication	      */
#define T_UDERROR_IND	21	/* unitdata error indication  */
#define T_OPTMGMT_ACK   22      /* manage options ack         */
#define T_ORDREL_IND    23      /* orderly release ind 	      */
#if _XOPEN4 || defined(_BUILDING_LIBXNET)
#define T_ADDR_REQ      24      /* get protocol addr req      */
#define T_ADDR_ACK      25      /* get protocol addr ack      */
#define _XTI_T_OPTMGMT_REQ   26	/* manage XTI options req     */
#endif /* _XOPEN4 || _BUILDING_LIBXNET */

/*
 * The following are the events that drive the state machine
 */
/* Initialization events */
#define TE_BIND_REQ	0	/* bind request		  		*/
#define TE_UNBIND_REQ	1	/* unbind request	  		*/
#define TE_OPTMGMT_REQ  2	/* manage options req     		*/
#define TE_BIND_ACK	3	/* bind acknowledment	      		*/
#define TE_OPTMGMT_ACK  4       /* manage options ack         		*/
#define TE_ERROR_ACK	5	/* error acknowledgment       		*/
#define TE_OK_ACK1	6	/* ok ack  seqcnt == 0 		  	*/
#define TE_OK_ACK2	7	/* ok ack  seqcnt == 1, q == resq      	*/
#define TE_OK_ACK3	8	/* ok ack  seqcnt == 1, q != resq       */
#define TE_OK_ACK4	9	/* ok ack  seqcnt > 1        		*/

/* Connection oriented events */
#define	TE_CONN_REQ	10	/* connection request     		*/
#define TE_CONN_RES	11	/* connection response    		*/
#define TE_DISCON_REQ	12	/* disconnect request     		*/
#define TE_DATA_REQ	13	/* data request	          		*/
#define TE_EXDATA_REQ	14	/* expedited data request 		*/
#define TE_ORDREL_REQ   15      /* orderly release req    		*/
#define TE_CONN_IND	16	/* connection indication      		*/
#define TE_CONN_CON	17	/* connection confirmation    		*/
#define TE_DATA_IND	18	/* data indication	      		*/
#define TE_EXDATA_IND	19	/* expedited data indication 		*/
#define TE_ORDREL_IND   20      /* orderly release ind 	      		*/
#define TE_DISCON_IND1	21	/* disconnect indication seq == 0      	*/
#define TE_DISCON_IND2	22	/* disconnect indication seq == 1   	*/
#define TE_DISCON_IND3	23	/* disconnect indication seq > 1  	*/
#define TE_PASS_CONN	24	/* pass connection 	      		*/

/* Unit data events */
#define TE_UNITDATA_REQ	25	/* unitdata request       		*/
#define TE_UNITDATA_IND	26	/* unitdata indication	      		*/
#define TE_UDERROR_IND	27	/* unitdata error indication  		*/

#define TE_NOEVENTS	28
/*
 * The following are the possible states of the Transport
 * Service Interface
 */

#define TS_UNBND		0	/* unbound	                */
#define	TS_WACK_BREQ		1	/* waiting ack of BIND_REQ      */
#define TS_WACK_UREQ		2	/* waiting ack of UNBIND_REQ    */
#define TS_IDLE			3	/* idle 		        */
#define TS_WACK_OPTREQ		4	/* wait ack options request     */
#define TS_WACK_CREQ		5	/* waiting ack of CONN_REQ      */
#define TS_WCON_CREQ		6	/* waiting confirm of CONN_REQ  */
#define	TS_WRES_CIND		7	/* waiting response of CONN_IND */
#define TS_WACK_CRES		8	/* waiting ack of CONN_RES      */
#define TS_DATA_XFER		9	/* data transfer		*/
#define TS_WIND_ORDREL	 	10	/* releasing rd but not wr      */
#define TS_WREQ_ORDREL		11      /* wait to release wr but not rd*/
#define TS_WACK_DREQ6		12	/* waiting ack of DISCON_REQ    */
#define TS_WACK_DREQ7		13	/* waiting ack of DISCON_REQ    */
#define TS_WACK_DREQ9		14	/* waiting ack of DISCON_REQ    */
#define TS_WACK_DREQ10		15	/* waiting ack of DISCON_REQ    */
#define TS_WACK_DREQ11		16	/* waiting ack of DISCON_REQ    */

#define TS_NOSTATES		17

/*
 * The following are the different flags available to
 * the transport provider to set in the PROVIDER_flag
 * field of the T_info_ack structure.
 */
#define SENDZERO	0x0001	/* provider can handle 0-length TSDU's */
#define EXPINLINE	0x0002	/* provider wants ETSDU's in band 0 */
#define XPG4_1		0x0004	/* provider conforms to XTI in XPG/4 and *
				 * supports T_ADDR_REQ & T_ADDR_ACK */

/* 
 * The following structure definitions define the format of the
 * stream message block of the above primitives.
 * (everything is declared long to ensure proper alignment
 *  across different machines)
 */

/* connection request */

struct T_conn_req {
	int 	PRIM_type;	/* always T_CONN_REQ  */
	int 	DEST_length;	/* dest addr length   */
	int 	DEST_offset;	/* dest addr offset   */
	int 	OPT_length;	/* options length     */
	int 	OPT_offset;	/* options offset     */
};

/* connect response */

/*
 * Note that the size of the QUEUE_ptr field can change when running
 * 32-bit binaries on a 64-bit kernel.  The code that handles I_FDINSERT
 * will adjust the structure accordingly.  This field is not directly
 * capable of being passed from the application to the kernel, but since
 * that was never the intent it should not be a problem.
 */
struct T_conn_res {
	int     PRIM_type;	/* always T_CONN_RES       */
#if _MIPS_SIM == _ABI64
	int	XX_pad;		/* get QUEUE_ptr aligned   */
#endif
	queue_t *QUEUE_ptr;	/* responding queue ptr    */
	int     OPT_length;	/* options length          */
	int 	OPT_offset;	/* options offset          */
	int     SEQ_number;	/* sequence number         */
};

/* disconnect request */

struct T_discon_req {
	int     PRIM_type;	/* always T_DISCON_REQ */
	int     SEQ_number;	/* sequnce number      */
};

/* data request */

struct T_data_req {
	int 	PRIM_type;	/* always T_DATA_REQ */
	int 	MORE_flag;	/* more data	     */
#define MORE_type       MORE_flag       /* 3.2 source compatibility */
};

/* expedited data request */

struct T_exdata_req {
	int 	PRIM_type;	/* always T_EXDATA_REQ */
	int 	MORE_flag;	/* more data	       */
};

/* information request */

struct T_info_req {
	int 	PRIM_type;	/* always T_INFO_REQ */
};

/* bind request */

struct T_bind_req {
	int 		PRIM_type;	/* always T_BIND_REQ            */
	int 		ADDR_length;	/* addr length	                */
	int 		ADDR_offset;	/* addr offset	                */
	unsigned int 	CONIND_number;	/*connect indications requested */
};

/* unbind request */

struct T_unbind_req {
	int 	PRIM_type;	/* always T_UNBIND_REQ */
};

/* unitdata request */

struct T_unitdata_req {
	int 	PRIM_type;	/* always T_UNITDATA_REQ  */
	int 	DEST_length;	/* dest addr length       */
	int 	DEST_offset;	/* dest addr offset       */
	int 	OPT_length;	/* options length         */
	int 	OPT_offset;	/* options offset         */
};

/* manage options request */

struct T_optmgmt_req {
	int 	PRIM_type;	/* T_OPTMGMT_REQ or _XTI_T_OPTMGMT_REQ  */
	int 	OPT_length;	/* options length         */
	int 	OPT_offset;	/* options offset         */
	int     MGMT_flags;	/* options flags          */
};

/* orderly release request */

struct T_ordrel_req {
	int 	PRIM_type;	/* always T_ORDREL_REQ */
};

#if _XOPEN4 || defined(_BUILDING_LIBXNET)
/*
 *  get protocol address request
 */
struct T_addr_req {
	int	PRIM_type;	/* always T_ADDR_REQ    */
};	

/*
 *  get protocol address request  ack
 */
struct T_addr_ack {
	int	PRIM_type;	/* always T_ADDR_ACK    */
	int	LOCADDR_length;	/* length of local addr */
	int	LOCADDR_offset;	/* offset of local addr */
	int	REMADDR_length;	/* length of remote addr*/
	int	REMADDR_offset;	/* offset of remote addr*/
};
#endif /* _XOPEN4 || _BUILDING_LIBXNET */

/* connect indication */

struct T_conn_ind {
	int 	PRIM_type;	/* always T_CONN_IND */
	int 	SRC_length;	/* src addr length   */
	int 	SRC_offset;	/* src addr offset   */
	int 	OPT_length;	/* option length     */
	int     OPT_offset;	/* option offset     */
	int     SEQ_number;	/* sequnce number    */
};

/* connect confirmation */

struct T_conn_con {
	int 	PRIM_type;	/* always T_CONN_CON      */
	int 	RES_length;	/* responding addr length */
	int 	RES_offset;	/* responding addr offset */
	int 	OPT_length;	/* option length          */
	int     OPT_offset;	/* option offset          */
};

/* disconnect indication */

struct T_discon_ind {
	int 	PRIM_type;	/* always T_DISCON_IND 	*/
	int 	DISCON_reason;	/* disconnect reason	*/
	int     SEQ_number;	/* sequnce number       */
};

/* data indication */

struct T_data_ind {
	int  	PRIM_type;	/* always T_DATA_IND */
	int 	MORE_flag;	/* more data 	     */
};

/* expedited data indication */

struct T_exdata_ind {
	int 	PRIM_type;	/* always T_EXDATA_IND */
	int 	MORE_flag;	/* more data           */
};

/* information acknowledgment */

struct T_info_ack {
	int	PRIM_type;	/* always T_INFO_ACK     */
	int	TSDU_size;	/* max TSDU size         */
	int	ETSDU_size;	/* max ETSDU size        */
	int	CDATA_size;	/* max connect data size */
	int	DDATA_size;	/* max discon data size  */
	int	ADDR_size;	/* address size		 */
	int	OPT_size;	/* options size		 */
	int     TIDU_size;	/* max TIDU size         */
	int     SERV_type;	/* provider service type */
	int     CURRENT_state;  /* current state         */
	int     PROVIDER_flag;  /* provider flags        */
};

/* bind acknowledgment */

struct T_bind_ack {
	int 		PRIM_type;	/* always T_BIND_ACK        */
	int 		ADDR_length;	/* addr length              */
	int 		ADDR_offset;	/* addr offset              */
	unsigned int 	CONIND_number;	/* connect ind to be queued */
};

/* error acknowledgment */

struct T_error_ack { 
	int  	PRIM_type;	/* always T_ERROR_ACK  */
	int 	ERROR_prim;	/* primitive in error  */
	int 	TLI_error;	/* TLI error code      */
	int 	UNIX_error;	/* UNIX error code     */
};

/* ok acknowledgment */

struct T_ok_ack {
	int  	PRIM_type;	/* always T_OK_ACK   */
	int 	CORRECT_prim;	/* correct primitive */
};

/* unitdata indication */

struct T_unitdata_ind {
	int 	PRIM_type;	/* always T_UNITDATA_IND  */
	int 	SRC_length;	/* source addr length     */
	int 	SRC_offset;	/* source addr offset     */
	int 	OPT_length;	/* options length         */
	int 	OPT_offset;	/* options offset         */
};

/* unitdata error indication */

struct T_uderror_ind {
	int 	PRIM_type;	/* always T_UDERROR_IND   */
	int 	DEST_length;	/* dest addr length       */
	int 	DEST_offset;	/* dest addr offset       */
	int 	OPT_length;	/* options length         */
	int 	OPT_offset;	/* options offset         */
	int 	ERROR_type;	/* error type	          */
};

/* manage options ack */

struct T_optmgmt_ack {
	int 	PRIM_type;	/* always T_OPTMGMT_ACK   */
	int 	OPT_length;	/* options length         */
	int 	OPT_offset;	/* options offset         */
	int     MGMT_flags;	/* managment flags        */
};

/* orderly release indication */

struct T_ordrel_ind {
	int 	PRIM_type;	/* always T_ORDREL_IND */
};

/*
 * The following is a union of the primitives
 */
union T_primitives {
	int 			type;		/* primitive type     */
	struct T_conn_req	conn_req;	/* connect request    */
	struct T_conn_res	conn_res;	/* connect response   */
	struct T_discon_req	discon_req;	/* disconnect request */
	struct T_data_req	data_req;	/* data request       */
	struct T_exdata_req	exdata_req;	/* expedited data req */
	struct T_info_req	info_req;	/* information req    */
	struct T_bind_req	bind_req;	/* bind request       */
	struct T_unbind_req	unbind_req;	/* unbind request     */
	struct T_unitdata_req	unitdata_req;	/* unitdata requset   */
	struct T_optmgmt_req	optmgmt_req;	/* manage opt req     */
	struct T_ordrel_req	ordrel_req;	/* orderly rel req    */
	struct T_conn_ind	conn_ind;	/* connect indication */
	struct T_conn_con	conn_con;	/* connect corfirm    */
	struct T_discon_ind	discon_ind;	/* discon indication  */
	struct T_data_ind	data_ind;	/* data indication    */
	struct T_exdata_ind	exdata_ind;	/* expedited data ind */
	struct T_info_ack	info_ack;	/* info ack	      */
	struct T_bind_ack	bind_ack;	/* bind ack	      */
	struct T_error_ack	error_ack;	/* error ack	      */
	struct T_ok_ack		ok_ack;		/* ok ack	      */
	struct T_unitdata_ind	unitdata_ind;	/* unitdata ind       */
	struct T_uderror_ind	uderror_ind;	/* unitdata error ind */
	struct T_optmgmt_ack	optmgmt_ack;	/* manage opt ack     */
	struct T_ordrel_ind	ordrel_ind;	/* orderly rel ind    */
#if _XOPEN4 || defined(_BUILDING_LIBXNET)
	struct T_addr_req	addr_req;	/* address req        */
	struct T_addr_ack	addr_ack;	/* address response   */
	struct T_optmgmt_req	xti_optmgmt_req; /* manage XTI opt req */
#endif /* _XOPEN4 || _BUILDING_LIBXNET */
};


#ifdef __cplusplus
}
#endif
#endif	/* _SYS_TIHDR_H */
