#ifndef DECNET_NSP_H
#define DECNET_NSP_H
/*
 * Module: nspmsg.h - nsp message formats
 * 
 * This file contains the definitions of all the Network Services Procotol
 * constants, structures, and functions. The message structures sent by the
 * End-to-End Communications layer are also defined.
 */


/*
 * Message flags
 */

/* Data messages */
#define BEG_DATA_MSG	0x20		/* beginning data message */
#define END_DATA_MSG	0x40		/* ending data message */
#define MID_DATA_MSG	0x00		/* middle data message */
#define COM_DATA_MSG	0x60		/* complete data message */
#define INTR_MSG	0x30		/* interrupt message */
#define LINK_SVC_MSG	0x10		/* link service message */
/* Acknowledgement messages */
#define DATA_ACK	0x04		/* data acknowledgement */
#define OTH_ACK		0x14		/* other acknowledgement */
#define CONN_ACK	0x24		/* connect acknowledgement */
/* Control messages */
#define NOP		0x08		/* no operation */
#define CONN_INIT	0x18		/* connect initate */
#define CONN_CONF	0x28		/* connect confirm */
#define DISC_INIT	0x38		/* disconnect initiate */
#define DISC_CONF	0x48		/* disconnect confirm */
#define	RESERVED_1	0x58		/* reserved (Phase II node init) */
#define RECON_INIT	0x68		/* retransmitted connect initiate */
#define	RESERVED_2	0x78		/* reserved */


/*
 * Message header lengths
 */
#define DMLEN   7			/* data message */
#define IMLEN   7			/* interrupt message */
#define LSLEN   9			/* link service message */
#define DALEN   7			/* data acknowledgement */
#define OALEN   7			/* other acknowledgement */
#define CALEN   3			/* connect acknowledgement */
#define NOPLEN  1			/* no operation */
#define CILEN   9			/* connect initate */
#define RCILEN  9			/* retransmitted connect initiate */
#define CCLEN   9			/* connect confirm */
#define DILEN   7			/* disconnect initiate */
#define DCLEN   7			/* disconnect confirm */

#define NSP_DATA_HDR_LEN	11	/* max nsp data header */
#define NSP_INT_HDR_LEN		11	/* max nsp intr header */
#define	NSP_CON_ACK_LEN		 3	/* how big is a connect ack */

#define	NSP_MAXHDRLEN		NSP_DATA_HDR_LEN

#define MSGFLAG_LEN     1
#define DSTADDR_LEN     2
#define SRCADDR_LEN     2
#define COMMON_HDR_LEN          MSGFLAG_LEN + DSTADDR_LEN + SRCADDR_LEN

#define	ACK_LEN	2


/*
 * nsp message codes
 */

/* disconnect (confirm) reasons */
#define NoRes   1       /* no resources */
#define DisCom  42      /* disconnect complete */
#define NoLink  41      /* no link */
/* link service flag values  */
#define DatReq  2       /* data request link service message */
#define DatNop  0
#define DatXoff 1       /* do not send data link service message */
#define DatXon  2
#define IntReq  6       /* interrupt request link service message */
/* flow control options */
#define NoFlow  1       /* no flow control */
#define SegFlow 5       /* segment request flow control */
#define MsgFlow 9       /* message request flow control */
/* routing message codes */
#define DLC_reject              4       /* data link circuit reject */
#define DEST_unreachable        5       /* destination unreachable */
#define BAD_circuit             6       /* bad circuit id */
#define BAD_nexthop             7       /* bad next hop */


/*
 * bit field macro definitions
 */
#define Mod4096( x )            ( ( x ) & 0xFFF )

/* for Link Service */
#define LSINT( x )              ( x >> 2 )
#define LSMOD( x )              ( x & 03 )

/* for Connect Initiate & Connect Confirm */
#define FCOPT( x )              ( x >> 2 )


/*
 * --------------------------------------------------------------------------
 *      D A T A   S T R U C T U R E S   O N   T H E   W I R E
 * --------------------------------------------------------------------------
 */

/*
 * There are three types of NSP messages:
 *	1) Data
 *	2) Acknowledgment
 *	3) Control
 */

/*
 * There are three types of Data messages:
 *	1) Data Segment messages
 *	2) Interrupt messages
 *	3) Link Service messages
 */
typedef struct data_seg_msg {	/* data segment message */
	char	flag;		/* message flag */
	u_char	dstaddr[2];	/* logical link destination address */
	u_char	srcaddr[2];	/* logical link source address */
	u_char	acknum[2];	/* optional: last data seg received */
	u_char	ackoth[2];	/* optional: last other data received */
	u_char	segnum[2];	/* data segment number */
	} NSP_data_seg;

typedef struct intr_msg {	/* interrupt message */
	char	flag;		/* message flag */
	u_char	dstaddr[2];	/* logical link destination address */
	u_char	srcaddr[2];	/* logical link source address */
	u_char	acknum[2];	/* optional: last intr/link svc received */
	u_char	ackdat[2];	/* optional: last data seg received */
	u_char	segnum[2];	/* data segment number */
	} NSP_intr_seg;

typedef struct link_svc_msg {	/* link service message */
	char	flag;		/* message flag */
	u_char	dstaddr[2];	/* logical link destination address */
	u_char	srcaddr[2];	/* logical link source address */
	u_char	acknum[2];	/* optional: last intr/link svc received */
	u_char	ackdat[2];	/* optional: last data seg received */
	u_char	segnum[2];	/* data segment number */
	char	lsflags;	/* link service flags */
	char	fcval;		/* remaining window size */
	} NSP_link_svc;

/*
 * There are three types of acknowledgment messages:
 *      1) Data Acknowledgement messages
 *      2) Other-Data Acknowledgment messages
 *      3) Connect Acknowledgment messages
 */
typedef struct data_ack_msg {   /* data ack message */
	char    flag;           /* message flag */
	u_char  dstaddr[2];     /* logical link destination address */
	u_char  srcaddr[2];     /* logical link source address */
	u_char  acknum[2];      /* last data segment received */
	u_char	ackoth[2];	/* optional: last other data received */
	} NSP_data_ack;

typedef struct oth_ack_msg {   /* other data ack message */
	char    flag;           /* message flag */
	u_char  dstaddr[2];     /* logical link destination address */
	u_char  srcaddr[2];     /* logical link source address */
	u_char  acknum[2];      /* last intr/link svc received */
	u_char	ackdat[2];	/* optional: last data segment received */
	} NSP_oth_ack;

typedef struct conn_ack_msg {   /* connect ack message */
	char    flag;           /* message flag */
	u_char  dstaddr[2];     /* logical link destination address */
	} NSP_conn_ack;

/*
 * There are five types of control messages:
 *	1) No Operation messages
 *	2) Connect Initiate messages
 *	3) Connect Confirm messages
 *	4) Disconnect Initiate messages
 *	5) Disconnect Confirm messages
 */
typedef struct nop_msg {	/* no operation message */
	char    flag;           /* message flag */
	} NSP_nop;

typedef struct conn_init_msg {	/* (retransmitted) connect initiate message */
	char    flag;           /* message flag */
	u_char  dstaddr[2];     /* logical link destination address */
	u_char  srcaddr[2];     /* logical link source address */
	char	services;	/* requested services: flow control option */
	char	info;		/* information: version */
	u_char	segsize[2];	/* max data segment size */
	/* data to follow */
	} NSP_conn_init;

typedef struct conn_conf_msg {	/* connect confirm message */
	char    flag;           /* message flag */
	u_char  dstaddr[2];     /* logical link destination address */
	u_char  srcaddr[2];     /* logical link source address */
	char	services;	/* requested services: flow control option */
	char	info;		/* information: version */
	u_char	segsize[2];	/* max data segment size */
	/* user supplied data to follow; maximum of 16 bytes */
	u_char	cf_ctlsize;	/* size of control data */
	} NSP_conn_conf;

typedef struct disc_init_msg {	/* disconnect initiate message */
	char    flag;           /* message flag */
	u_char  dstaddr[2];     /* logical link destination address */
	u_char  srcaddr[2];     /* logical link source address */
	u_char	reason[2];	/* first 2 bytes of Session Control disc data */
	/* remaining bytes of Session Control disc data; max of 16 bytes */
	u_char	di_ctlsize;	/* size of control data */
	} NSP_disc_init;

typedef struct disc_conf_msg {	/* disconnect confirm message */
	char    flag;           /* message flag */
	u_char  dstaddr[2];     /* logical link destination address */
	u_char  srcaddr[2];     /* logical link source address */
	u_char	reason[2];	/* disconnect reason */
	} NSP_disc_conf;

#endif /* DECNET_NSP_H */
