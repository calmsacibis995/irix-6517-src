/*
 *   tr_user.h
 *
 *   Formation FV1600 Token-Ring
 *	Interface to LLI software
 *
 *   Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *   Defines interface structures for passing commands and
 *   status between the token ring driver and the LLI.
 */
#ident "$Revision: 1.13 $"
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Debugging levels
 */
#define DBG_LVL_DRV	1	/* enable most printfs if driver */
#define DBG_LVL_LLI	2	/* enable lli monitor printfs */

#ifndef DBG_LVL
#define DBG_LVL 0
#endif

#if DBG_LVL >= DBG_LVL_DRV
# define DP(a)           printf a
# define EDP(a)          if (error) printf a
#else
# define DP(a)
# define EDP(a)
#endif

#if DBG_LVL >= DBG_LVL_LLI
# define LP(a)           printf a
# define ELP(a)          if (error) printf a
#else
# define LP(a)
# define ELP(a)
#endif

/* This part has to move to llc.h eventually */
typedef struct llc1 {
#define LLC_DSAP_GROUP	1
	u_char		llc1_dsap;
#define LLC_SSAP_RESP	1
	u_char		llc1_ssap;
#define LLC_FORMAT_MASK		0x3
#define		LLC_SFORMAT		0x1
#define 	LLC_UFORMAT		0x3
#define LLC_SEQ_MASK		0xfe
#define LLC_PF_MASK		0x1
#define LLC_SCMD_MASK		0xc
#define 	LLC_CMD_RR		0x0
#define 	LLC_CMD_REJ		0x8
#define 	LLC_CMD_RNR		0x4
#define LLC_UPF_MASK		0x10
#define 	LLC_CMD_UI		0x3
#define 	LLC_CMD_XID		0xaf
#define 	LLC_CMD_TEST		0xe3
#define 	LLC_CMD_SABME		0x6f
#define 	LLC_CMD_DISC		0x43
#define 	LLC_CMD_UARESP		0x63
#define 	LLC_CMD_DMRESP		0xf
#define 	LLC_CMD_FRMRRESP	0x87
	u_char		llc1_cont;
} LLC1;

typedef struct llc2 {
	LLC1		ch;
	u_char		llc2_cont2;
} LLC2;
#define llc2_dsap ch.llc1_dsap
#define llc2_ssap ch.llc1_ssap
#define llc2_cont ch.llc1_cont

typedef struct snap {
	u_char snap_org[3];
	u_char snap_etype[2];
} SNAP;

#define	TR_ARP_HW_TYPE		6	/* ARP hardware type for Token-Ring */
/* endof llc.h */
/*----------------------------------------------------------------------*
 * Interface Command/Response Block 					*
 * 									*
 * LLI/TRD use this structure to pass commands/responses across 	*
 * their interface.  The structure must be an M_PROTO block containing  *
 * the command or response in LLI_CB format in the control info part 	*
 * of the message, and any associated data in the data part of the 	*
 * message (see STREAMS Programmers Guide on M_PROTO messages).		*
 * 									*
 *----------------------------------------------------------------------*/

typedef struct {
	unsigned int		port;		/* interface # (0 - 3) */

#define LLI_CMD_RSP		0x00	/* CB contains an SSB_CMD response */
#define LLI_RCV_PENDING		0x01	/* CB contains Rcv ICB status */
#define LLI_LLC_STATUS		0x02	/* CB contains LLC ICB status */
	unsigned int		command;	/* LLI command/response code */
	void			*param_blk;	/* ptr to cmd/rsp blk */
} LLI_CB;

typedef struct tr_xmit_parms {
	mblk_t		      *xmit_data;
	u_short		      frame_size;
	u_short		      station_id;
	u_char		      remote_sap;
	u_char		      frame_type;
} TR_XMIT_PARMS;

typedef struct iiq {
	u_int	seq;
	mblk_t	*frm;
} IIQ;

/*
 * iframes structure
 */
typedef struct iframes {
	struct tr_xmit_parms  xmit_parms;

#define	TR_IFRM_NULL		0x00	/* NULL */
#define	TR_IFRM_PENDING		0x01	/* wait to be requested */
#define	TR_IFRM_OOB		TR_IFRM_PENDING
#define	TR_IFRM_WAIT_COREL	0x02	/* waiting for READY with no HOLD */
#define	TR_IFRM_WAIT_READY	0x03	/* waiting for READY but HOLD */
#define	TR_IFRM_WAIT_ACK	0x04	/* waiting for xmit ACK */
	char		      state;
	char		      correlator;
} IFRAMES;

/*
 * Link Control Block (LCB) - 1 per STREAM
 */
typedef struct {
	  queue_t           *qptr;    	    /* back pointer to WR queue */
	  u_int		    ppa; 	    /* interface # */
	  u_int		    ssap_id;  	    /* SSAP from OPEN.SAP */
	  u_int		    dsap_id;  	    /* DSAP from OPEN.STATION */
	  u_int		    stn_id;	    /* 15-8 = SAP_ID, 7-0 = STN_ID */
	  u_int		    mgt_sap_flag;   /* = 1 if MGT SAP, 0 if not */
	  u_int		    max_conind;	    /* max o/s connections */
	  u_int		    sap_stnid;	    /* stn_id of associated SAP */
	  u_int		    correln;  	    /* dev for incoming CN from DLPI */
	  mblk_t	    *ip_msg;	    /* input msg being processed */
	  u_int		    errno;	    /* error for bad response */
	  u_int		    syserr;	    /* system err if applicable */
	  u_int		    err_prim;	    /* primitive causing error */
	  u_int		    token;	    /* non-zero if DL_TOKEN issued */
	  u_int	    	    fsm_lock;	    /* 1 = FSM is in use */
	  u_short           cstate;   	    /* current state */
	  u_short           pstate;   	    /* previous state */
	  u_short           ctrig;    	    /* current trigger */
	  u_short           ptrig;    	    /* previous trigger */
	  u_short           mdev;	    /* minor device number */
	  u_short	    rif_len;	    /* Routing Infor Field length */
	  u_char	    mac_addr[6];    /* link station MAC address */
	  u_char	    src_ri[18];     /* source routing information */
#define NIFRAMES	32
	  IFRAMES	    iframes[NIFRAMES]; /* to non-acked I-frames */
	  u_short 	    ifr_first;
	  u_short 	    ifr_last;
	  u_int	    	    ifr_cnt;	    /* total num of ifrm in q */
#define IFR_STATE_OK	0	/* O.K. to request */
#define IFR_STATE_WAIT	1	/* wait for ready bit */
	  u_int		    ifr_state;
	  u_short	    ifr_pend;	    /* Iframe Req pending */
	  u_short 	    ifr_hold;	    /* Iframe hold */
	  u_short	    ifr_corel;	    /* expected iframe corel */
	  u_short	    ifr_pad;

	/* Iframe Input queue */
	u_int		iiq_noob;
	u_int		iiq_seq;
#define	NIIQ		16
	IIQ		iiq[NIIQ];
} LCB;

#define	MAX_CONIND		4	    /* max o/s connection indicators */
#define MAX_ADAPTORS		4
#define MAX_SAPS		4	    /* maximum SAPs per interface */
#define MAX_STNS		16	    /* maximum connections per SAP */
#define MAX_LCB			(MAX_ADAPTORS * MAX_STNS)

/*-------------------------------------------------------------------------
 * Interface Table - translate STATION_ID to ptr to WR q for this Stream
 *------------------------------------------------------------------------*/

typedef struct {
	queue_t			*wq_ptr;
	u_char			*pmacaddr;
} STN_ENTRY;

typedef struct {
	STN_ENTRY   		stn_slot[MAX_STNS * MAX_SAPS];
} STN_TBL;

#define WR_PTR(x)		stn_slot[x].wq_ptr
#define MACPTR(x)		stn_slot[x].pmacaddr
extern STN_TBL	stn_tbl[];	/* 1 STN TBL per i/f */
extern u_int 	sapid_2_sap[MAX_ADAPTORS][MAX_SAPS];

/*----------------------------------------------------------------------*
 *	Frame formats							*
 *----------------------------------------------------------------------*/

#define MAX_I_FRAME		516

typedef struct tr_mac_hdr {
	TR_MAC		mac;
	TR_RII		ri;		/* routing info */
} TR_MAC_HDR;
#define t_mac_ac	mac.mac_ac
#define t_mac_fc	mac.mac_fc
#define t_mac_da	mac.mac_da
#define t_mac_sa	mac.mac_sa
#define t_mac_ri	ri
#define t_mac_rii	ri.rii
#define t_seg_nos	ri.sgt_nos

typedef struct fvohdr {
	TR_MAC          mac;
	SNAP		snap;
} FVOHDR;
#define t_snap_org	snap.snap_org
#define t_snap_etype	snap.snap_etype

typedef struct fvihdr {
	TR_MAC_HDR	machdr;
	LLC2		llc2;
} FVIHDR;

#define TOKEN_ISGROUP(addr)	(*(u_char*)(addr) & 0x80)
#define TOKEN_ISBROAD(addr)	(TOKEN_ISGROUP(addr) && \
				((u_char*)addr)[2] == 0xff && \
				((u_char*)addr)[3] == 0xff && \
				((u_char*)addr)[4] == 0xff && \
				 ((u_char*)addr)[5] == 0xff)
#define TOKEN_ISMULTI(addr) 	(TOKEN_ISGROUP(addr) && !TOKEN_ISBROAD(addr))

/*----------------------------------------------------------------------*
 *	SAP structure							*
 *----------------------------------------------------------------------*/

#define	TR_NSTATION	16	/* number of supported stations */
#define	TR_NSAP		4	/* number of supported SAPs */

typedef struct tr_sap {
#define	TR_NULL_SAP		0x00	/* null SAP */
#define	TR_BROADCAST_SAP	0xff	/* broadcast SAP */
#define	TR_IP_SAP		0xaa	/* IP SAP */
	u_char	code;		/* SAP code */

#define	TR_SAP_OPENING		0x01	/* trying to open SAP */
#define	TR_SAP_OPEN		0x02	/* SAP open successful */
#define	TR_OPEN_SAP_FAILED	0x03	/* SAP open failed */
	u_char	flags;

	u_short	failure_code;
	u_short	station_id;
} TR_SAP;

/*
 * LLI BANDAGE:
 */
struct trlliops {
	int	(*lli_valid)(int);		/* check valid unit */
	char*	(*lli_get_sa)(int);	/* get MAC addr */
	int	(*lli_output)(u_short, SCB *);	/* output */
};

/*
 * Token Ring stats.
 */
struct tr_qs {
	u_int sndq;
	u_int smlq;
	u_int medq;
	u_int bigq;
	u_int sndq_freed;
	u_int smlq_freed;
	u_int medq_freed;
	u_int bigq_freed;
	u_int sndq_hiwat;
	u_int smlq_hiwat;
	u_int medq_hiwat;
	u_int bigq_hiwat;
	u_int cmdq;
	u_int cmdq_freed;
	u_int cmdq_hiwat;
};

struct tr_ints {
	u_int tot;
	u_int recv;
	u_int xmit;
	u_int buf;
	u_int stray;
};

struct tr_ifx {
	u_long lcb;		/* link control blk */
	u_int stat;
	u_int tot;		/* total request from SNA */
	u_int reject;		/* request failed */
	u_int pend;		/* hold */
	u_int rtry;		/* successful retry of held ifr */
	u_int rfail;		/* failed retry of held ifr */
	u_int wcorel;		/* total IFR requested and waiting corel */
	u_int wack;		/* total IFR xmited */
	u_int rxmit;		/* total IFR retransmited released */
	u_int freed;		/* total IFR ACKED and released */
	/* recv iframe stat */
	u_int iiq_tot;		/* total iframes received */
	u_int iiq_oobs;		/* total OOBs */
	u_int iiq_rcvrd;	/* tatal OOBs recovered */
};

extern int tr_lli_register(struct trlliops*);
extern void tr_lli_input(mblk_t*);
extern LCB* tr_find_WR_q(struct ifnet*, u_short);
extern struct iframes* tr_find_iframe_q(struct ifnet*, u_short, u_short);
extern void tr_free_iframes(LCB*);
extern int tr_if_valid(int);
extern char* tr_get_sa(int);
extern int tr_lli_output(u_short, SCB*);
extern u_short tr_addr2sid(struct ifnet*, char*, u_char);
#ifdef __cplusplus
}
#endif
