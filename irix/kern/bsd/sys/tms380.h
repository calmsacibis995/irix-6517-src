/*
 * TI tms380 chip set definitions.
 *
 *   Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.6 $"
#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------*
 *	Number of data buffers						*
 *----------------------------------------------------------------------*/
#define	TR_BUFM_SIZE		(128*1024)	/* FV1600 memory size   */
#define	TR_SCB_SIZE		8		/*    SCB		*/
#define	TR_SSB_SIZE		8		/*    SSB		*/
#define	TR_OPEN_BLK_SIZE	64		/*    Open parm blk     */
#define TR_ERROR_LOG_SIZE	16		/*    error log		*/
#define	TR_OPEN_SAP_BLK_SIZE 	32		/*    Open SAP parm blk */

#define TR_OPEN_STA_BLK_SIZE	20
#define TR_CONN_STA_BLK_SIZE	8
#define TR_LLC_STAT_BLK_SIZE	12
#define TR_PRODUCTID_SIZE	32		/*    Product ID size   */

#define TR_LIST_SIZE		32	
#define TR_DATA_BUFFER_SIZE	1024		
#define TR_RCV_LISTS		18		/* max = 64 receive lists  */
#define TR_XMIT_LISTS		23		/* max = 64 transmit lists */
#define	TR_DATA_BUFFERS		((TR_BUFM_SIZE - 5*1024)/TR_DATA_BUFFER_SIZE)

#define	TR_DATA_BUF_START	0x400
#define TR_BUFM_RESERVED	(TR_DATA_BUF_START \
				- TR_SCB_SIZE			\
				- TR_SSB_SIZE			\
				- TR_OPEN_BLK_SIZE		\
				- TR_ERROR_LOG_SIZE	  	\
				- TR_OPEN_SAP_BLK_SIZE 		\
				- TR_OPEN_STA_BLK_SIZE 		\
				- TR_CONN_STA_BLK_SIZE		\
				- TR_LLC_STAT_BLK_SIZE		\
				- TR_PRODUCTID_SIZE)

#define	TR_IOB_HEADERS		3	/* max # of iob headers per list */
#define TR_NOT_LAST_IOB_HDR	0x8000	/* not last iob header in xmit list */

#define TR_XMIT_LIST_HD_SIZE	8	/* xmit list header size */
#define TR_XMIT_LLC_SIZE	4	/* xmit list LLC portion size */

#define	TR_SCB_PAD		(TR_SCB_SIZE - 6)
#define TR_OPEN_BLOCK_PAD	(TR_OPEN_BLK_SIZE - 44)
#define TR_OPEN_SAP_BLOCK_PAD	(TR_OPEN_SAP_BLK_SIZE - 24)
#define TR_PRODUCT_ID_PAD	(TR_PRODUCTID_SIZE - 18)
#define TR_ERROR_LOG_PAD	(TR_ERROR_LOG_SIZE - 14)
#define	TR_RECEIVE_LIST_PAD	(TR_LIST_SIZE - 8 - 6*TR_IOB_HEADERS)
#define	TR_TRANSMIT_LIST_PAD	(TR_LIST_SIZE - TR_XMIT_LIST_HD_SIZE \
				- 6*TR_IOB_HEADERS - TR_XMIT_LLC_SIZE)

/*
 * ACTL
 */
#define TR_SWHLDA		0x0800	/* pseudo-DMA s/w hold acknowledge */
#define	TR_SWDDIR_INP		0x0400	/* DMA direction is input */
#define TR_SWHRQ		0x0200	/* TMS380 input DMA is pending */
#define TR_PSDMAEN		0x0100	/* pseudo DMA enable */
#define TR_ARESET		0x0080	/* TMS380 reset */
#define	TR_CPHALT		0x0040	/* TMS380 COMMprocessor halt */
#define TR_RAM_BOOT		0x0020	/* TMS380 firmware is RAM resident */
#define TR_SINTEN		0x0008	/* TMS380 interrupts enabled */
#define TR_PEN			0x0004	/* TMS380 parity enabled */

/*
 * SIFCMD/STATUS register
 */
#define	TR_CMD_INT_ADAPT	0x8000	/* interrupt the TMS380 */
#define	TR_CMD_ADAPT_RESET	0xff00	/* TMS380 adapter reset request */
#define	TR_CMD_SSB_CLEAR	0x2000	/* TMS380 SSB clear request */
#define	TR_CMD_EXECUTE		0x1000	/* TMS380 execute command request */
#define	TR_CMD_SCB_REQUEST	0x0800	/* TMS380 interrupt on SCB clear req */
#define	TR_CMD_RCV_CONTINUE	0x0400	/* TMS380 receive continue request */
#define	TR_CMD_RCV_CANCEL	0x0400	/* TMS380 receive cancel request */
#define	TR_CMD_RCV_VALID	0x0200	/* receive list valid notification */
#define	TR_CMD_XMIT_VALID	0x0100	/* transmit list valid notification */
#define	TR_STAT_INTR		0x0080	/* TMS380 to host interrupt */
#define	TR_STAT_INIT		0x0040	/* TMS380 ready to be initialized */
#define	TR_STAT_TEST		0x0020	/* TMS380 POC diagnostics in progress*/
#define	TR_STAT_ERROR		0x0010	/* TMS380 POC diag or init failed */
#define	TR_STAT_INIT_OK		0x0000	/* TMS380 init completed ok */
#define	TR_STAT_INT_CODE	0x000f	/* TMS380 interrupt code field mask */
#define	TR_STAT_ERROR_CODE	0x000f	/* TMS380 error code field mask */

/* SIFCMD/STATUS - Bring-Up Diagnostic error codes */
#define TR_ERR_INITIAL_TEST	0x0000	/* initial test error */
#define TR_ERR_SOFT_CRC		0x0001	/* TMS380 software CRC error */
#define TR_ERR_RAM		0x0002	/* TMS380 RAM error */
#define TR_ERR_INST_TEST	0x0003	/* TMS380 instruction test error */
#define TR_ERR_CTX_INT_TEST	0x0004	/* TMS380 context/interrupt test err */
#define TR_ERR_PROT_HAND	0x0005	/* TMS380 protocol handler hard err */
#define TR_ERR_SIF_REG		0x0006	/* TMS380 system interface reg. err */

/* SIFCMD/STATUS - interrupt codes */
#define	TR_INT_ADAP_CHECK	0x0	/* TMS380 adapter check interrupt */
#define	TR_INT_RING_STAT	0x4	/* TMS380 ring status interrupt */
#define	TR_INT_LLC_STAT		0x5	/* TMS380 LLC status interrupt */
#define	TR_INT_SCB_CLEAR	0x6	/* TMS380 SCB clear interrupt */
#define	TR_INT_TIMER		0x7	/* TMS380 timer interrupt */
#define	TR_INT_COMMAND_STAT	0x8	/* TMS380 command status interrupt */
#define	TR_INT_RCV_STAT		0xa	/* TMS380 receive status interrupt */
#define	TR_INT_XMIT_STAT	0xc	/* TMS380 transmit status interrupt */
#define	TR_INT_RCV_PENDING	0xe	/* TMS380 receive pending interrupt */

/* SIFCMD/STATUS - init error codes */
#define	TR_IERR_INV_BLOCK	0x1	/* invalid initialization block */
#define	TR_IERR_INV_OPTS	0x2	/* invalid options */
#define	TR_IERR_INV_BSIZE	0x3	/* invalid receive burst size */
#define	TR_IERR_INV_BCNT	0x4	/* invalid transmit burst count */
#define	TR_IERR_INV_DMA_THRES	0x5	/* invalid DMA abort threshold */
#define	TR_IERR_INV_SCB		0x6	/* invalid SCB */
#define	TR_IERR_INV_SSB		0x7	/* invalid SSB */
#define	TR_IERR_DIO_PARITY	0x8	/* DIO parity error */
#define	TR_IERR_DMA_TIMEOUT	0x9	/* DMA timeout */
#define	TR_IERR_DMA_PARITY	0xa	/* DMA parity error */
#define	TR_IERR_DMA_BERR	0xb	/* DMA bus error */
#define	TR_IERR_DMA_DATA_ERR	0xc	/* DMA data error */
#define	TR_IERR_ADAP_CHK	0xd	/* adapter check */

/*
 * initialization block
 */
typedef struct tr_init_block {
	u_short	init_options;		/* initialization option flags */
	u_char	cmd_stat_vec;		/* command status interrupt vector */
	u_char	xmit_vec;		/* transmit interrupt vector */
	u_char	rcv_vec;		/* receive interrupt vector */
	u_char	ring_stat_vec;		/* ring status interrupt vector */
	u_char	scb_clear_vec;		/* SCB clear interrupt vector */
	u_char	adapter_check_vec;	/* adapter check interrupt vector */
	u_short rcv_burst_size;		/* adapter receive DMA burst size */
	u_short xmit_burst_size;	/* adapter transmit DMA burst size */
	u_short dma_abort_thresh;	/* bus and parity errs retry count */
	u_short	scb_addr_hi;		/* upper 16 bits of SCB address */
	u_short scb_addr_lo;		/* lower 16 bits of SCB address */
	u_short	ssb_addr_hi;		/* upper 16 bits of SSB address */
	u_short ssb_addr_lo;		/* lower 16 bits of SSB address */
} TR_INIT_BLOCK;

/* Initialization options */
#define	TR_INIT_RESERVED_BIT	0x8000	/* bit 0 must be set to 1 */
#define	TR_PARITY_ENABLE	0x6000	/* parity enable on DIO and DMA */
#define	TR_BURST_SCB_SSB	0x1000	/* burst DMA from/into SCB and SSB */
#define	TR_BURST_LIST		0x0800	/* burst DMA from xmit/rcv lists */
#define	TR_BURST_LIST_STATUS	0x0400	/* burst DMA into xmit/rcv lists */
#define	TR_BURST_RCV_DATA	0x0200	/* burst DMA into rcv data blocks */
#define	TR_BURST_XMIT_DATA	0x0100	/* burst DMA from xmit data blocks */
#define	TR_LLC_ENABLE		0x0040	/* enable adapter's LLC interface */

/* Initialization block xmit/receive burst sizes */
#define	TR_RCV_BURST_SIZE	0	/* DMA burst size into receive bufs */
#define TR_XMIT_BURST_SIZE	0	/* DMA burst size from xmit bufs */

/* Initialization block DMA abort thrashold */
#define	TR_MAX_BUS_ERRORS	8	/* max retries after bus error */
#define	TR_MAX_PARITY_ERRORS	8	/* max retries after parity error */

/*
 * TMS380 internal addresses.
 */
typedef struct tms_internal_addr {
	u_short	biap;		/* ptr to burnin addr */
	u_short swlp;		/* ptr to sw level */
	u_short addrp;		/* ptr to addr, gaddr, faddr */
	u_short parmp;		/* ptr to adptr parameters */
	u_short macbufp;	/* ptr to MAC buffer */
	u_short llccp;		/* ptr to LLC counter */
	u_short speedp;		/* 0 - 4Mbps, 1 - 16Mbps */
	u_short ramsize;	/* RAM size in K bytes */
} TMS_INTERNAL_ADDR;
#define TR_INT_MEMORY_ADDR	0x0001	/* TMS380 internal memory */
#define TR_INIT_ADDR		0x0a00	/* internal memory initialize */
#define TR_BIA_PTR_ADDR		0x0a00	/* internal memory BIA pointer */
#define TR_TR_SPEED_ADDR	0x0a0c	/* internal memory speed pointer */
#define TR_ICB_ADDR		0x0978	/* internal memory initialize */

/* TMS380 read adpator parameters */
typedef struct tms_ra_addrs {
	u_char   caddr[TR_MAC_ADDR_SIZE];	/* current mac addr */
	u_short  gaddr_hi;		/* group addr */
	u_short  gaddr_lo;
	u_short  faddr_hi;		/* functional addr */
	u_short  faddr_lo;
} TMS_RA_ADDRS;

typedef struct tms_ra_parm {
	u_short	 phys_drop_hi;		/* physical drop number */
	u_short	 phys_drop_lo;
	u_char   una[TR_MAC_ADDR_SIZE];		/* Upstream neighbor addr */
	u_short	 una_phys_drop_hi;	/* una physical drop number */
	u_short	 una_phys_drop_lo;
	u_char   last_ring_poll[TR_MAC_ADDR_SIZE];/* last ring poll addr */
	u_short  res1;
	u_short  xmit_pri;		/* xmit access priority */
	u_short  src_class;		/* source class authorization */
	u_short  last_attn_code;	/* last attention code */
	u_char   last_src_addr[TR_MAC_ADDR_SIZE];	/* last source addr */
	u_short  last_bcn;		/* last beacon type */
	u_short  last_mvec;		/* last major vector type */
	u_short  ring_status;		/* ring status */
	u_short  soft_err_timer;
	u_short  ring_err_cnt;
	u_short  local_ring_num;	/* local ring number */
	u_short  monitor_err;		/* monitor error code */
	u_short  xmit_bcn_typ;
	u_short  recv_bcn_typ;
	u_short  frame_corel;		/* frame corelator saved */
	u_char   una_bcn[TR_MAC_ADDR_SIZE];	/* beaconing station una */
	u_short  res2[2];
	u_short  bcn_phys_drop_hi;	/* beaconing station phys drop num */
	u_short  bcn_phys_drop_lo;
} TMS_RA_PARMS;

typedef struct tms_ra_llc {
	u_char max_saps;
	u_char open_saps;
	u_char max_stns;
	u_char open_stns;
	u_char avail_stns;
	u_char res;
} TMS_RA_LLC;

/*----------------------------------------------------------------------*
 *	System Command Block (SCB)					*
 *----------------------------------------------------------------------*/

typedef struct {
	u_short	scb_cmd;			/* SCB command */
	u_short	scb_parm1;			/* first parameter */
	u_short scb_parm2;			/* second parameter */
	u_char	scb_pad[TR_SCB_PAD];		/* pad space */
}SCB;

#define	scb_addr	scb_parm1		/* SCB parm block addr */

/*----------------------------------------------------------------------*
 *	 SCB command definitions 					*
 *----------------------------------------------------------------------*/

#define	TR_CMD_OPEN		0x0003		/* connect to ring */
#define	TR_CMD_TRANSMIT		0x0004		/* xmit multiple packets */
#define	TR_CMD_XMIT_HALT	0x0005		/* stop transmisstion */
#define TR_CMD_RECEIVE		0x0006		/* rcv multiple packets */
#define TR_CMD_CLOSE		0x0007		/* disconnect from ring */
#define TR_CMD_SET_GROUP	0x0008		/* set group address */
#define TR_CMD_SET_FUNCT	0x0009		/* set functional address */
#define TR_CMD_READ_ERRLOG	0x000a		/* read error log */
#define TR_CMD_READ_ADAPTER	0x000b		/* read (TMS380) adapter */
#define TR_CMD_MOD_OPEN_PARMS	0x000d		/* modify open parameters */
#define TR_CMD_REST_OPEN_PARMS	0x000e		/* restore open parameters */
#define TR_CMD_SET_FIRST_16	0x000f		/* set first 16 bits of 
						   group address */
#define TR_CMD_SET_BRIDGE_PARMS	0x0010		/* set bridge parameters
						   (unsupported) */
#define TR_CMD_CNF_BRIDGE_PARMS 0x0011		/* config bridge parameters
						   (unsupported) */
#define TR_CMD_LLC_RESET	0x0014		/* LLC reset */
#define TR_CMD_OPEN_SAP		0x0015		/* Open SAP */
#define TR_CMD_CLOSE_SAP	0x0016		/* Close SAP */
#define TR_CMD_OPEN_STATION	0x0019		/* Open Stations */
#define TR_CMD_CLOSE_STATION	0x001a		/* Open Stations */
#define TR_CMD_CONNECT_STATION	0x001b		/* Connect Station */
#define TR_CMD_MOD_LLC_PARMS	0x001c		/* Modify LLC parameters */
#define TR_CMD_FLOW_CONTROL	0x001d		/* Flow Control */
#define TR_CMD_LLC_STATS	0x001e		/* Get LLC Statistics */
#define TR_CMD_DIR_INTERRUPT	0x001f		/* Dir Interrupt */
#define TR_CMD_LLC_REALLOC	0x0021		/* LLC Reallocate */
#define TR_CMD_TIMER_SET	0x0022		/* Timer set */
#define TR_CMD_XMIT_IFRAME	0x0023		/* Transmit I-frame */

/*
 * SSB (Sytem status block)
 */
typedef struct {
	u_short	ssb_command;			/* SSB command */
	u_short	ssb_parm0;			/* first parameter */
	union
	    {
	    struct
		{
	    	u_short	ssb_prm1;		/* second parameter */
	    	u_short ssb_prm2;		/* third parameter */
	    	} ssb_1;
	    u_int      ssb_wd1;		/* view as 32 bits */
	    } xx;
} SSB;
#define ssb_parm1	xx.ssb_1.ssb_prm1
#define ssb_parm2	xx.ssb_1.ssb_prm2
#define ssb_word1	xx.ssb_wd1
#define	ssb_stat	ssb_parm0		/* command status */
#define ssb_addr	ssb_parm1		/* parameter block address */
#define rej_stat	ssb_parm0		/* reject status */
#define rej_cmd		ssb_parm1		/* rejected command */
#define	ssb_stn_id	ssb_parm1		/* station id at open.sap */
#define ssb_parm1_long  xx.ssb_wds.ssb_word1     /* 32 bit access to SSB */

/* SSB command definitions */
#define TR_SSB_RING_STATUS	0x0001		/* ring status SSB */
#define TR_SSB_COMMAND_REJ	0x0002		/* command reject SSB */

/* SSB_stat for RING_STATUS */
#define	TR_SIGNAL_LOSS		0x8000		/* signal loss */
#define TR_HARD_ERROR		0x4000		/* hard error */
#define TR_SOFT_ERROR		0x2000		/* soft error */
#define TR_XMIT_BEACON		0x1000		/* transmitting beacon frames */
#define TR_LOBE_WIRE_FAULT	0x0800		/* open/short circuit */
#define TR_AUTO_REMOVAL_ERR	0x0400		/* internal h/w error */
#define TR_REMOVE_RCV		0x0100		/* recvd Remove Ring Station */
#define TR_COUNTER_OVFL		0x0080		/* error counter overflow */
#define TR_SINGLE_STATION	0x0040		/* only station on ring */
#define TR_RING_RECOVERY	0x0020		/* Claim Token frame on ring */
#define TR_RING_STAT_ERR	0xdde0		/* reported ring stat errs */

/* SSB_stat for COMMAND_REJ */
#define	TR_ILLEGAL_CMD		0x8000		/* illegal command */
#define	TR_ADDRESS_ERR		0x4000		/* odd list forward ptr */
#define TR_ADAPTER_OPEN		0x2000		/* TMS380 is open */
#define	TR_ADAPTER_CLOSED	0x1000		/* TMS380 is closed */
#define	TR_SAME_CMD		0x0800		/* same cmd is executing */

/* SSB_stat for CMD_OPEN */
#define	TR_OPEN_CMD_COMPLETE	0x8000		/* open command complete */
#define TR_OPEN_NODE_ADDR_ERR	0x4000		/* node address error */
#define TR_OPEN_LIST_SIZE_ERR	0x2000		/* list size error */
#define TR_OPEN_BUF_SIZE_ERR	0x1000		/* buffer size error */
#define TR_OPEN_XMIT_BUFCNT_ERR 0x0400		/* xmit buffer count err */
#define TR_OPEN_ERR		0x0200		/* insertion onto ring failed*/
#define TR_LLC_ERR		0x0100		/* LLC processing failed */
#define TR_OPEN_PHASE		0x00f0		/* mask for open phase */
#define TR_OPEN_ERR_CODE	0x000f		/* mast for open err code */

/* SSB_stat for READ_ADAPTER */
#define	TR_READ_ADAPTER_OK	0x8000		/* command completed ok */

/* SSB_stat for CMD_RCV */
#define	TR_RCV_FRAME_COMPLETE	0x8000		/* received a new frame */
#define TR_RCV_SUSPENDED	0x4000		/* receive suspended (odd
						   forward pointer */
/* SSB_stat for CMD_XMIT */
#define	TR_XMIT_CMD_COMPLETE	0x8000		/* xmit command complete */
#define TR_XMIT_FRAME_COMPLETE	0x4000		/* frame transmit complete */
#define TR_XMIT_LIST_ERR	0x2000		/* list error occurred */
#define TR_XMIT_I_FRAME_ACK	0x1000		/* Ack for I-frame */
#define TR_XMIT_I_READY		0x0800		/* Bd. is read for I-frame */
#define TR_XMIT_FRAME_SIZE_ERR	0x0080		/* frame size conflicts */
#define TR_XMIT_THRESHOLD	0x0040		/* frame too large */
#define TR_XMIT_ODD_ADDR	0x0020		/* odd address forward ptr */
#define TR_XMIT_FRAME_ERR	0x0010		/* start frame conflicts */
#define TR_XMIT_ILL_FRAME_FORM	0x0002		/* leftmost FC bit set */

/* SSB_stat for XMIT_IFRAME */
#define	TR_IFRAME_BUILD		0x00		/* board is ready for iframe */
#define TR_IFRAME_MAX_CMDS	0x25		/* max commands exceeded */
#define TR_IFRAME_INV_STN	0x40		/* invalid station id */
#define TR_IFRAME_PROT_ERR	0x41		/* protocol error */
#define	TR_IFRAME_WAIT		0xff		/* bd not ready for iframe */

/* SSB_stat for MOD_OPEN_PARMS */
#define	TR_MOD_PARMS_OK		0x8000		/* modify open parms ok */
#define TR_MOD_PARMS_INV_OPT	0x0100		/* invalid open option */

/*
 * Interface Control Block (ICB)
 */
typedef struct {
	/* Receive pending information */
	u_short	station_id_r;		/* destination station id of frame */
	u_short	lan_header_len;		/* MAC header length of rcv'd frame */
	u_short	llc_header_len;		/* LLC header length of rcv'd frame */
	u_short	frame_len;		/* entire length of rcv'd frame */
	
	/* LLC stat info */
	u_short	station_id_s;		/* sta. id to which stat code belongs */
	u_short	llc_stat_code;		/* LLC status code */
	u_short	frmr_data_hi;		/* high 16 bits of frame reject data */
	u_short frmr_data_mid;		/* mid 16 bits of frame reject data */
	u_char	frmr_data_lo;		/* low 8 bits of frame reject data */
	u_char	ac_priority;		/* access control priority for SAP */
	u_char	remote_address[6];	/* node address of remote station */
	u_char	remote_sap;		/* remote SAP of station_id_s */
	
	/* Receive pending info */
	u_char	frame_type;		/* type of frame received */
} TR_ICB;

/* tr_icb.llc_stat_code */
#define	TR_LOST_LINK		0x8000	/* lost link to remote station */
#define	TR_DISC			0x4000	/* disconnect frame/mode */
#define	TR_FRMR_RCVD		0x2000	/* frame reject received */
#define	TR_FRMR_SENT		0x1000	/* frame reject sent */
#define	TR_SABME_RCVD		0x0800	/* SABME frame received */
#define	TR_SABME_OPEN_LINK	0x0400	/* SABME frame received - opened link */
#define	TR_REMOTE_BUSY		0x0200	/* remote station entered busy state */
#define	TR_REMOTE_NOT_BUSY	0x0100	/* remote station exited busy state */
#define	TR_TI_EXPIRED		0x0080	/* inactivity timer expired */
#define	TR_LLC_COUNT_OVFL	0x0040	/* LLC counter reached half of max */
#define	TR_ACCESS_PRIORITY	0x0020	/* SAP access priority reduced */
#define	TR_LOCAL_BUSY		0x0001	/* local station entered busy state */

/* tr_icb.frame_type */
#define TR_MAC_FRAME		0x02	/* MAC frame */
#define TR_I_FRAME		0x04	/* I frame */
#define TR_UI_FRAME		0x06	/* UI frame */
#define TR_XID_CMD_P1		0x08	/* XID command (poll = 1) */
#define TR_XID_CMD_P0		0x0a	/* XID command (poll = 0) */
#define TR_XID_RESP_F1		0x0c	/* XID response (final = 1) */
#define TR_XID_RESP_F0		0x0e	/* XID response (final = 0) */
#define TR_TEST_RESP_F1		0x10	/* TEST response (final = 1) */
#define TR_TEST_RESP_F0		0x12	/* TEST response (final = 0) */
#define TR_NON_MAC_FRAME	0x14	/* other - non-MAC frame */
#define TR_BRIDGE_DATA_FRAME	0x20	/* bridge data frame */

/*
 * Adapter check block
 */
#define	TR_ADAPTER_CHECK_BLOCK_ADDR	0x05e0	/* block address (chap.1 ) */
typedef struct tr_adapter_check_block {
	u_short	adc_status;		/* adapter check status flags */
	u_short	adc_param_0;		/* DMA abort reason */
	u_short adc_param_1;		/* high 16-bit addr of failing DMA */
	u_short	adc_param_2;		/* low 16-bit addr of failing DMA */
} TR_ADAPTER_CHECK_BLOCK;

/* Adapter check block - check status flag */
#define TR_DIO_PARITY		0x8000	/* direct I/O parity detected */
#define TR_DMA_READ_ABORT	0x4000	/* DMA read (output) aborted */
#define TR_DMA_WRITE_ABORT	0x2000	/* DMA write (input) aborted */
#define TR_ILLEGAL_OPCODE	0x1000	/* illegal instruction in firmware */
#define TR_PARITY_ERRORS	0x0800	/* internal bus parity error */
#define TR_RING_UNDERRUN	0x0020	/* internal DMA underrun */
#define TR_INVALID_INT		0x0008	/* internal invalid interrupt */
#define TR_INVALID_ERR_INT	0x0004	/* invalid error interrupt */
#define TR_INVALID_XOP		0x0002	/* invalid xop request */

/* Adapter check block - param_0 */
#define	TR_TIMEOUT_ABORT	0x0000	/* DMA timeout abort */
#define	TR_PARITY_ERR_ABORT	0x0001	/* DMA parity error abort */
#define	TR_BUS_ERR_ABORT	0x0002	/* DMA bus error abort */

/*
 * Open parameter block
 */

typedef struct tr_open_block {
	u_short open_options;		/* open options flags */
	u_char 	node_address[6];	/* 6-byte Token-Ring address */
	TR_GRPADDR group_address;	/* 4-byte group address */
	TR_FCNADDR functional_address;	/* 4-byte functional address */
	u_short	receive_list_size;	/* # of bytes in receive list */
	u_short transmit_list_size;	/* # of bytes in transmit list */
	u_short	buffer_size;		/* adapter internal data buff size */
	u_short	reserved[2];
	u_char	xmit_buf_min;		/* mininum number of adapter 
					   internal buffers */
	u_char	xmit_buf_max;		/* maximum number of adapter
					   internal buffers */
	u_short	product_id_address_hi;	/* high 16-bit productID address */
	u_short	product_id_address_lo;	/* low 16-bit productID address */
	u_char	sap_max;		/* maximum # of open SAPs */
	u_char	station_max;		/* maximum # of open stations */
	u_char	gsap_max;		/* maximum # of open Group SAPs */
	u_char	gsap_member_max;	/* maximum # of SAPs in a group */
	u_char	timer_t1_1;		/* Miscellaneous timers		*/
	u_char	timer_t2_1;
	u_char	timer_ti_1;
	u_char	timer_t1_2;
	u_char	timer_t2_2;
	u_char	timer_ti_2;
	u_short	max_frame_size;		/* maximum # of bytes in a frame */
	u_char	pad[TR_OPEN_BLOCK_PAD];
} TR_OPEN_BLOCK;

/* open_options */
#define	TR_WRAP_INTERFACE	0x8000	/* loopback mode */
#define TR_DIS_HARD_ERR		0x4000	/* disable hard err interrupt */
#define TR_DIS_SOFT_ERR		0x2000	/* disable soft err interrupt */
#define TR_PASS_MAC_FRAME	0x1000	/* pass MAC frames to host */
#define TR_PASS_ATTN_FRAME	0x0800	/* pass attention MAC frames to host */
#define TR_PAD_ROUTE_FIELD	0x0400	/* pad routing field to 18 bytes */
#define TR_FRAME_HOLD		0x0200	/* input a whole frame before dma */
#define TR_CONTENDER		0x0100	/* participate in Monitor Contention */
#define TR_PASS_BEACON		0x0080	/* pass beacon MAC frames to host */
#define TR_OPEN_EARLY_RELEASE	0x0010	/* non-early token release (16 Mbit) */
#define TR_HOST_I_FRAME_XMIT	0x0008	/* host-based I-frame transmit */
#define TR_COPY_ALL_MAC_FRAMES	0x0004	/* promisc. mode for MAC frames */
#define TR_COPY_ALL_NON_MAC	0x0002	/* promisc. mode for non-MAC frames */
#define TR_PASS_FIRST_BUFFER	0x0001	/* pass first interal buffer only */

/*
 * Host Product ID
 */
typedef struct tr_product_id {
	u_char	id[18];
	u_char	pad[TR_PRODUCT_ID_PAD];
} TR_PRODUCT_ID;

/*
 * Error Log
 */
typedef struct tr_error_log {
	u_char	line_err;		/* FCS (CRC) error, etc. */
	u_char	reserved_0;
	u_char	burst_err;		/* no transition for 5 half bits */
	u_char	ari_fci_err;		/* upstream neighbor can't set ari */
	u_char	reserved_1;
	u_char	reserved_2;
	u_char	lost_frame_err;		/* strip whole frame from ring 
					   failed */
	u_char	rcv_cong_err;		/* no space to copy in frame */
	u_char	frame_copied_err;	/* line hit or duplicate address */
	u_char	reserved_3;
	u_char	token_err;		/* token error (monitor node only) */
	u_char	reserved_4;
	u_char	dma_bus_errs;		/* DMA bus errors (up to init limit) */
	u_char	dma_parity_errs;	/* DMA parity errs (up to init limit)*/
	u_char	pad[TR_ERROR_LOG_PAD];
} TR_ERROR_LOG;
#define	TR_READ_ERRLOG_OK		0x8000	
#define	TR_CLOSE_OK			0x8000	

/*
 * Receive/Transmit basic data block header structure
 */

typedef struct tr_iob_header {
	u_short	data_count;		/* data count */
	u_short	address_high;		/* data ptr - high 16 bits */
	u_short	address_low;		/* data ptr - low 16 bits */
} TR_IOB_HEADER;


/*
 * Receive list
 */
typedef struct tr_rcv_list {
	u_short	rcv_fwd_high;		/* fwd pointer - high 16 bits */
	u_short	rcv_fwd_low;		/* fwd pointer - low 16 bits */
	u_short	rcv_cstat;		/* receive list status */
	u_short	rcv_frame_size;		/* receive frame size */
					/* data buffer header */
	struct	tr_iob_header	rcv_iobh[TR_IOB_HEADERS];
	u_char	rcv_pad[TR_RECEIVE_LIST_PAD];
} TR_RCV_LIST;

/* Receive list status */
#define	TR_RCVL_VALID		0x8000	/* list valid */
#define	TR_RCVL_FRAME_COMPLETE	0x4000	/* frame complete indicator */
#define TR_RCVL_START_FRAME	0x2000	/* start of frame indicator */
#define	TR_RCVL_END_FRAME	0x1000	/* end of frame indicator */
#define TR_RCVL_FRAME_INT	0x0800	/* interrupt upon frame reception */
#define TR_RCVL_INTER_FRAME_WT	0x0400	/* wait between DMA in frames */
#define	TR_RCVL_PASS_CRC	0x0200	/* pass frame CRC into buffer */
#define TR_RCVL_FS		0x00fc	/* frame status field from frame */
#define TR_RCVL_ADDR_MATCH	0x0003	/* address match field */


/*
 * Transmit list
 */
typedef struct tr_xmit_list {
	u_short	xmit_fwd_high;		/* fwd pointer - high 16 bits */
	u_short	xmit_fwd_low;		/* fwd pointer - low 16 bits */
	u_short	xmit_cstat;		/* transmit list status */
	u_short	xmit_frame_size;	/* transmit frame size */
	struct	tr_iob_header	xmit_iobh[TR_IOB_HEADERS];
	u_short	xmit_station_id;	/* transmit station id */
	u_char 	xmit_remote_sap;	/* transmit remote sap */
	u_char	xmit_header_size;	/* transmit header size */
	u_char	xmit_pad[TR_TRANSMIT_LIST_PAD];
} TR_XMIT_LIST;
#define	xmit_frame_corl	xmit_header_size	/* transmit frame correlator */

/* Transmit list status (xmit_cstat) */
#define	TR_XMITL_VALID		0x8000	/* list valid */
#define	TR_XMITL_FRAME_COMPLETE	0x4000	/* frame complete indicator */
#define TR_XMITL_START_FRAME	0x2000	/* start of frame indicator */
#define	TR_XMITL_END_FRAME	0x1000	/* end of frame indicator */
#define TR_XMITL_FRAME_INT	0x0800	/* interrupt upon frame reception */
#define TR_XMITL_ERR		0x0400	/* transmit error occurred */
#define	TR_XMITL_PASS_CRC	0x0200	/* pass frame CRC from buffer */
#define	TR_XMITL_PASS_SRC_ADDR	0x0100	/* pass source address from buffer */
#define TR_XMITL_FRAME_TYPE	0x00e0	/* frame type */
#define TR_XMITL_STRIP_FS	0x00ff	/* FS of stripped frame */
#define TR_XMITL_XMIT_ERR	0x00ff	/* error code if transmit error */

/* Transmit error/link status codes */
#define TR_XMITERR_OK		0x00	/* no error */
#define TR_XMITERR_UNAUTH_ACC	0x08	/* unauthorized access priority */
#define TR_XMITERR_FXMIT	0x23	/* error in frame transmit or strip */
#define TR_XMITERR_UNAUTH_MAC	0x24	/* unauthorized MAC frame */
#define TR_XMITERR_INV_CORR	0x26	/* invalid frame correlator */
#define TR_XMITERR_INV_LEN	0x28	/* invalid transmit frame length */
#define TR_XMITERR_INV_STA	0x40	/* invalid station id */
#define TR_XMITERR_PROT_ERR	0x41	/* protocol error */
#define TR_XMITERR_INV_ROUT	0x44	/* invalid routing information length */
#define TR_XMITERR_INV_FT	0xf0	/* invalid frame type */

/*
 * Data buffer
 */
#define TR_TMS380_BUFFER_SIZE	1024	/* 1KB internal buffers */
typedef struct tr_data_buf {
	union data_buf {
	   struct  tr_data_buf *dbuf_next;	/* next buffer on free list */
	   u_char  buf[TR_DATA_BUFFER_SIZE];	/* the data buffer itself   */
	} data_buf;
} TR_DATA_BUF;
#define d_next	data_buf.dbuf_next
#define d_buf	data_buf.buf

/*
 * Open SAP parameter block
 */
typedef struct tr_open_sap_block {
	u_short	reserved_0;
	u_short reserved_1;
	u_char	timer_t1;		/* Miscellaneous timers */
	u_char	timer_t2;
	u_char	timer_ti;
	u_char	maxout;			/* I-frame window stuff, not used */
	u_char	maxin;			/* 				  */
	u_char	maxout_incr;		/* 				  */
	u_char	max_retry_count;	/*				  */
	u_char	gsap_max_number;	/* max # of SAPs if this is a GSAP */
	u_short max_i_field;		/* max information field size */
	u_char	sap_value;		/* value of the opened SAP */
	u_char	sap_options;		/* open SAP option flag */
	u_char	station_count;		/* max # of stations on this SAP */
	u_char	reserved_2;		
	u_char	reserved_3;
	u_char	group_member_count;	/* number of GSAPs that this SAP 
					   is a member of*/
	u_short	gsap_list_ptr_hi;	/* upper 16 bits of GSAP list ptr */
	u_short gsap_list_ptr_lo;	/* lower 16 bits of GSAP list ptr */
	u_char	pad[TR_OPEN_SAP_BLOCK_PAD];
} TR_OPEN_SAP_BLOCK;

/* sap_options definitions */
#define	TR_SAP_PRIORITY		0x60	/* packet priority 3, RFC 1042 */
#define TR_XID_HANDLER		0x08	/* host handles XID responses */
#define	TR_INDIVIDUAL_SAP	0x04	/* this is an individual SAP */
#define	TR_GROUP_SAP		0X02	/* this is a group SAP */
#define	TR_GROUP_MEMBER_SAP	0x01	/* this is a group member SAP */
#define	TR_OPEN_IP_SAP_OPTIONS	(TR_SAP_PRIORITY | TR_INDIVIDUAL_SAP)

/* Close SAP return codes */
#define	TR_CLOSE_SAP_OK		0x0000	/* successful */
#define	TR_CLOSE_SAP_INV_STN	0x0040	/* invalid station_id */

/* Open SAP return codes */
#define TR_OPEN_SAP_OK		0x0000	/* successful */
#define TR_OPEN_SAP_INV_OPTS	0x0006	/* invalid options */
#define TR_OPEN_SAP_UNAUTH	0x0008	/* unauthorized access priority */
#define TR_OPEN_SAP_TOOBIG	0x0042	/* a parameter has exceeded max limit*/
#define	TR_OPEN_SAP_INV_SAP	0x0043	/* invalid SAP value */
#define	TR_OPEN_SAP_BAD_GRP	0x0045	/* nonexistent group */
#define TR_OPEN_SAP_RES_NA	0x0046	/* resources not available */
#define	TR_OPEN_SAP_MAX_MEM	0x0049	/* group has too many members */

/* LLC Reset return codes */
#define TR_LLC_RESET_OK		0x0000	/* successful */
#define TR_LLC_RESET_BAD_STN	0x0040  /* bad station */

/*
 * Open station parameter block
 */
typedef struct tr_open_station_block {
	u_short	station_id;
	u_short reserved;
	u_char	timer_t1;		/* Miscellaneous timers */
	u_char	timer_t2;
	u_char	timer_ti;
	u_char	maxout;			/* I-frame window stuff, not used */
	u_char	maxin;			/* 				  */
	u_char	maxout_incr;		/* 				  */
	u_char	max_retry_count;	/*				  */
	u_char	rsap_value;		/* SAP value of remote station */
	u_short max_i_field;		/* max information field size */
	u_short	station_options;	/* open station options flag */
					/* 6-byte ring addr of remote station */
	u_short	rnode_addr_high;	/* ...upper 16 bits */
	u_short rnode_addr_low;		/* ...lower 16 bits */
} TR_OPEN_STATION_BLOCK;

/* Open station return codes */
#define TR_OPEN_STA_OK		0x0000	/* successful */
#define TR_OPEN_STA_UNAUTH	0x0008	/* unauthorized access priority */
#define TR_OPEN_STA_INV_STA	0x0040	/* Invalid station id */
#define	TR_OPEN_STA_TOOBIG	0x0042	/* Parameter exceeds maximum */
#define	TR_OPEN_STA_INV_SAP	0x0043	/* Invalid remote SAP or SAP in use */
#define TR_OPEN_STA_RES_NA	0x0046	/* Resources not available */
#define	TR_OPEN_STA_INV_NODE	0x004f	/* Invalid remote node address */

/* Close station return codes */
#define	TR_CLOSE_STA_OK		0x0000	/* successful */
#define	TR_CLOSE_DUP_CMD	0x0002	/* Duplicate command outstanding  */
#define	TR_CLOSE_INV_STA	0x0040	/* Invalid station id  */
#define	TR_CLOSE_STA_CLOSED	0x004b	/* Station closed w/o remote ack  */
#define	TR_CLOSE_SEQ_ERR	0x004c	/* Sequence error */

/*
 * Connect Station parameter block
 */
typedef struct tr_connect_station_block {
	u_short	station_id;
	u_short reserved;
						/* routing information addr */ 
	u_short	routing_info_high;		/* ...upper 16 bits */
	u_short	routing_info_low;		/* ...lower 16 bits */
} TR_CONNECT_STN_BLK;

/* Connect station return codes */
#define	TR_CONNECT_STA_OK	0x0000	/* successful */
#define	TR_CONNECT_DUP_CMD	0x0002	/* Duplicate command outstanding  */
#define	TR_CONNECT_INV_STA	0x0040	/* Invalid station id  */
#define	TR_CONNECT_PROT_ERR	0x0041	/* Protocol error  */
#define	TR_CONNECT_INV_ROUTE	0x0044	/* Invalid routing info length  */
#define	TR_CONNECT_SEQ_ERR	0x004a	/* Sequence error */
#define	TR_CONNECT_NOT_OK	0x004d	/* Unsuccessful connection attempt  */

/*
 * LLC statistics parameter block
 */
typedef struct tr_llc_statistics_block {
	u_short	station_id;
	u_short buffer_length;			/* length of *buffer_addr */
						/* Statistics buffer addr */
	u_short	buffer_addr_high;		/* ...upper 16 bits */
	u_short	buffer_addr_low;		/* ...lower 16 bits */
	u_short	actual_buff_length;		/* actual length of stats */
	u_short	options;			/* statistics options */
} TR_LLC_STATISTICS_BLOCK;

/* LLC statistics options */
#define TR_LLC_STATS_RESET	0x8000		/* reset stats after call */

/* LLC statistics return codes */
#define TR_LLC_STATS_OK		0x0000		/* successful */
#define TR_LLC_STATS_NOSPACE	0x0015		/* not enough space for stats */
#define TR_LLC_STATS_INV_STA	0x0040		/* invalid station id */

typedef struct tr_llc_stat_data {
	u_short	nxifrm;			/* num of iframes xmit */
	u_short nrifrm;			/* num of iframes rcvd */
	u_char	nrerr;			/* num of iframes rcvd with error */
	u_char	nxerr;			/* num of iframes xmit with error */
	u_short	t1_exp;			/* num of t1 expire */
	u_char	rcmd;			/* last cmd rcvd */
	u_char	xcmd;			/* last cmd sent */
	u_char	pstate;			/* primary state */
	u_char	sstate;			/* secondary state */
	u_char	vs;
	u_char	vr;
	u_char	nr;
	u_char	hdrlen;			/* MAC hdr len used */
	u_char	machdr[32];		/* MAC hdr from AC to RII */
} TR_LLC_STAT_DATA;

/*
 * Transmit I-frame Request return codes
 */
#define TR_XMIT_IFR_OK		0x0000		/* successful */
#define TR_XMIT_IFR_MAX_CMDS	0x0000		/* Maximum cmds exceeded */
#define TR_XMIT_IFR_INV_STA	0x0000		/* Invalid station id */
#define TR_XMIT_IFR_PROT_ERR	0x0000		/* Protocol error */
#define TR_XMIT_IFR_OK_FC	0x0000		/* Good completion; the
						   adapter has assinged a
						   frame correlator */

/*
 * Frame types
 */
#define TR_FTYPE_DIRECT		0x0000	/* direct frame type */
#define TR_FTYPE_UI		0x0020	/* unnumberd information frame type */
#define TR_FTYPE_XID_COM	0x0040	/* XID command frame type */
#define TR_FTYPE_XID_RESP_F	0x0060	/* XID response final frame type */
#define TR_FTYPE_XID_RESP_NF	0x0080	/* XID response not final frame type */
#define TR_FTYPE_TEST_COM	0x00a0	/* TEST command frame type */
#define TR_FTYPE_I_FRAME	0x00c0	/* I frame type */
#define TR_FTYPE_INVALID	0x00e0	/* invalid frame type */

/*
 * I-Transmit error codes
 */
#define	TR_XMITL_UNAUTH_ACC_PRI	0x08	/* unauthorized access priority */
#define	TR_XMITL_STRIP_ERR	0x23	/* error in frame xmit or strip */
#define	TR_XMITL_UNAUTH_MAC_FRM	0x24	/* unauthorized MAC frame */
#define	TR_XMITL_NO_I_FRM_XMIT	0x27	/* not transmitting I frames */
#define	TR_XMITL_ILL_FRAME_LEN	0x28	/* invalid transmit frame length */
#define	TR_XMITL_ILL_STATION_ID	0x40	/* invalid station id */
#define	TR_XMITL_PROTOCOL_ERR	0x41	/* protocol error */
#define	TR_XMITL_ILL_ROUTE_LEN	0x44	/* invalid routing info length */
#define	TR_XMITL_ILL_FRAME_TYPE	0xf0	/* invalid frame type (111) */
#define	TR_XMITL_I_FRAME_NOTRDY	0xfe	/* I frame - TMS380 not ready */
#define	TR_XMITL_I_FRAME_READY	0xff	/* I frame - TMS380 ready */

#ifdef __cplusplus
}
#endif
