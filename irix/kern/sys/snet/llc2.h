/******************************************************************
 *
 *  SpiderX25 - LLC2 STREAMS Multiplexer
 *
 *  Copyright 1991 Spider Systems Limited
 *
 *  LLC2.H
 *
 *    Private header file.
 *
 ******************************************************************/

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/sys/llc2/86/s.llc2.h
 *	@(#)llc2.h	1.27
 *
 *	Last delta created	16:52:35 5/28/93
 *	This file extracted	16:55:18 5/28/93
 *
 */

#ident "@(#)llc2.h	1.27 (Spider) 5/28/93"

/* LLC1 data header size */
#define LLC1_HDR_SIZE	3

/* compile time variables */

#define MAXHWLEN    6
#define DEFHWLEN    6
#define MINRTLEN    2
#define MAXRTLEN    18
#define TR_COMPLEMENT_DBIT(rt)  *(rt+1) ^= 0x80

/* PDU values */
#define SABME       0x6F    /* U format PDU's */
#define DISC        0x43
#define UI          0x03
#define UA          0x63
#define DM          0x0F
#define FRMR        0x87
#define XID         0xAF
#define TEST        0xE3

#define UPF         0x10    /* U format P/F bit */

#define RR          0x01    /* S format PDU's */
#define RNR         0x05
#define REJ         0x09


#define PDU_SIZE    8       /* Longest PDU buffer size (for FRMR) */

/* XID values */
#define XID_NORMAL  0x81
#define XID_BOTH    0x03

/*
 *  State-machine states.
 *  N.B. states OFF and STOP are not seen by the state M/C
 */

#define OFF         0
#define STOP        1
#define ADM         2
#define RST         3
#define DCN         4
#define ERR         5
#define RUN         6
#define RSTCK       7

/* Values of 'dataflag' */
#define NORMAL      0
#define DATALOST    1
#define REJECT      2

/* Values of 'dn_regstage' */
#define REGNONE     0       /* Registration not started     */
#define REGTOMAC    1       /* Registration sent to MAC     */
#define REGFROMMAC  2       /* Registration back from MAC   */
#define REGDUPWAIT  3       /* Duplicate address check      */
#define REGDONEOK   4       /* Registration completed OK    */
#define REGDUPFAIL  5       /* Duplicate address found      */

/* VWXYZ bits for FRMR (error values in 'reason') */
#define FRMR_VBIT  16
#define FRMR_WBIT   1
#define FRMR_XBIT   2
#define FRMR_YBIT   4
#define FRMR_ZBIT   8

/* Special values of 'reason' */
#define FRAMEOK     0
#define IGNORE      0xDDDD
#define INVALID     0xEEEE
#define FRMRIN      0xFFFF

/*  Number of timers */
#define NTIMERS     3

/* Timer IDs (index in timer array) */
#define ACKTIMER    0
#define DLXTIMER    1
#define REJTIMER    2

/* the statistics table definitions */
#define LLC2_STATMAX        8

#define IG_FRAME_SUM        0
#define RE_Q_AK_SUM         1
#define MDC_DELAY_SUM       2
#define REM_BUSY_SUM        3
#define NO_RESOURCE_SUM     4
#define NO_BUFFS_SUM        5
#define EV_FAIL		    6
#define LAST_EVFAIL         7

typedef struct mac_addr {
	unchar		mac_addr_octet[MAXHWLEN];
} mac_addr_t;


/* Individual connection control structure */
typedef struct llc2cn {

    /* Addresses and pointers */
    uint8            dte[MAXHWLEN + 2];	/* addresses (2 SAPS + remote MAC)  */
    uint8            route[MAXRTLEN];     /* source route                   */
    uint8            routelen;  /* length of source route                   */
    struct llc2up   *upp;	/* pointer to UP structure		    */
    struct llc2dn   *dnp;	/* pointer to DOWN structure		    */
    struct llc2cn   *next;	/* next in UP's cn list (or in free chain)  */
    struct llc2cn   *hnext;	/* next in hash chain			    */
    struct llc2cn   *schqnext;  /* next in DOWN's schedule queue            */
    mblk_t          *ctlq;	/* queue of control commands		    */
    mblk_t          *ctlqtail;  /* tail of control command queue            */
    mblk_t          *ackq;	/* queue of unacknowleged I-frames          */
    mblk_t          *datq;	/* start of unsubmitted I-frames            */
    mblk_t          *adqtail;   /* tail of ack/data queue		    */
    caddr_t          nethandle; /* for interface to network layer           */
    uint16           connID;	/* connection identifier		    */
    uint16           tack;	/* current ack timer time		    */

    /* Timers */
    thead_t		thead;	/* timers header structure		    */
    x25_timer_t		tmtab[NTIMERS];	/* Individual timers		    */

    /* Current state of connection */
    uint8            state;	/* state = OFF  (LLC2 Disconnected)         */
				/*    or   STOP (LLC2 Disconnecting)        */
				/*    or   ADM  (LLC2 ADM)		    */
				/*    or   RST  (LLC2 SETUP or RESET)       */
				/*    or   DCN  (LLC2 D_CONN)		    */
				/*    or   ERR  (LLC2 ERROR)		    */
				/*    or   RUN  (LLC2 NORMAL or AWAIT or    */
				/*		      BUSY or AWAIT_BUSY or */
				/*		      REJECT or		    */
				/*		      AWAIT_REJECT)         */
				/*    or   RSTCK (LLC2 RESET_CHECK)         */

    /* Frame sequence numbers */
    uint8            vr;	/* Next N(S) to receive (LLC2 V(R))         */
    uint8            vs;	/* Next N(S) to send	(LLC2 V(S))         */
    uint8            ur;	/* Count of unacked V(R) values		    */
    uint8            us;	/* Count of unacked V(S) values		    */

    /* Flags and counters */
    uint8            txwindow;  /* Current Tx window (default or fm XID)    */
    uint8            txprbpt;   /* Point at which probes are sent           */
    uint8            awaitflag; /* If set, in LLC2 AWAIT.. state            */
    uint8            wclosed;   /* If set, tx window is closed		    */
    uint8            dataflag;  /* Dataflag/REJECT encoding		    */
    uint8            causeflag; /* Identifies entry to RST or DCN state     */
    uint8            pflag;	/* If set, awaiting response with F=1       */
    uint8            retry_count; /* Count of tx retries		    */
    uint8            loc_busy;	/* If set, this end can't accept I-frames   */
    uint8            rem_busy;	/* If set, remote end can't accept I-frames */
    uint8            sflag;	/* Auxiliary state: SABME received          */
    uint8            need_fbit;	/* If set, send frame with F bit set ASAP   */
    uint8            need_ack;	/* If set, send ack (I- or S-frame) ASAP    */
    uint8            discard;	/* If set, LC_BRA received; purge data      */
    uint8            xidflag;	/* Dup MAC XID: 0=>no, 1=>sent, 2=>request  */
    uint8            txstopped;	/* If set, data tx stopped, e.g. by AWAIT   */
    uint8	     loopback;	/* If set, DTE address is own address	    */

    /* FRMR details */
    uint8            frmr[5];	/* Data part of current FRMR command	    */

} llc2cn_t;


/* Max # of multicast addrs that can be enabled for a stream */
#define LLC_MAX_MULTI	10

typedef struct llc2up {
    queue_t         *up_readq;		/* UP (NET) stream READ queue */
    struct llc2dn   *up_dnp;		/* Pointer to DOWN (MAC) structure */
    struct llc2up   *up_dnext;		/* Next in DOWN's list of UP's */
    struct llc2cn   *up_cnlist;		/* List of UP's connect structures */
    struct llc2up   *up_udatqnext;	/* Next in DOWN's LLC1 data queue */
    uint8            up_class;		/* Class of LLC (LLC1 or LLC2) */
    uint8            up_nmlsap;		/* Normal SAP value for UP stream */
    uint8            up_lpbsap;		/* Loopback SAP value for UP stream */
    uint8	     up_interface;	/* Interface being supplied -- DLPI
					    or Spider's LLI */
    uint8            up_dlpi_state;	/* If DLPI, the state of the I/F */
    uint8            up_mode;		/* the mode of the stream */
    uint8            up_dsap[MAXHWLEN + 1];/* remote DLSAP */
    uint16           up_ethsap;		/* SAP for Ethernet mode */
    uint8            up_xidtest_flg;	/* If DLPI, whether or not XID
					    and/or TEST commands are
					    responded to automatically */
    uint8	     up_max_conind;	/* Max # of outstanding DL_CONNECT_IND
					    messages allowed on the stream */
    uint8	     up_num_conind;	/* Current # of outstanding
					    DL_CONNECT_IND messages */
    uint8	     up_conn_mgmt;	/* Is this a "connection management"
					    stream? */
    uint8	     up_waiting_ack;	/* Waiting for ack from LAN driver? */
    mblk_t	    *up_waiting_buf;    /* Waiting for a free buffer */
    uint8	     up_multi_count;	/* # of multicast addrs enabled */
    mac_addr_t	     up_multi_tab[LLC_MAX_MULTI];  /* Table of enabled multi.
							addrs */
    mblk_t	    *up_xid_msg;	/* XID msg to be retransmitted */
    int	             up_bufcall;	/* bufcall Id */
    int	             up_timeout;	/* timeout Id */
} llc2up_t;

/* Values of up_interface */
#define LLC_IF_SPIDER	0
#define LLC_IF_DLPI	1

/* Values of up_mode */
#define	LLC_MODE_NORMAL		0
#define	LLC_MODE_ETHERNET	1
#define	LLC_MODE_NETWARE	2

#ifndef DL_MAXPRIM
#define DL_MAXPRIM DL_GET_STATISTICS_ACK
#endif /* DL_MAXPRIM */


/* Hash definitions */
#define NHASH 32
#define HASH(DTE) ((DTE)[5]+(DTE)[6]+(DTE)[7]&31)

typedef struct llc2dn {
    queue_t         *dn_downq;		/* DOWN (MAC) stream WRITE queue */
    struct llc2up   *dn_uplist;		/* List of DOWN's UP structures */
    struct llc2cn   *dn_schq;		/* Connection schedule queue */
    struct llc2cn   *dn_schqtail;	/* Tail of schedule queue */
    struct llc2cn   *dn_hashtab[NHASH]; /* DTE hash: table of chain heads */
    struct llc2up   *dn_udatq;		/* Head DOWN's LLC1 data queue */
    struct llc2up   *dn_udatqtail;	/* Tail of DOWN's LLC1 data queue */
    mblk_t          *dn_rxqueue;	/* RX frame queue */
    mblk_t          *dn_rxqtail;	/* Tail of RX queue */
    llc2tune_t       dn_tunetab;	/* Table of tuning parameters */
    llc2stats_t      dn_statstab;	/* Statistics counts */
    uint32	     dn_index;		/* I_LINK index for this stream */
    uint8            dn_macaddr[MAXHWLEN];/* MAC address of this stream */
    uint8            dn_maclen;         /* length of MAC address */
    uint32           dn_mactype;        /* type of MAC */
    uint8            dn_rxqcnt;		/* RX queue count */
    uint32           dn_snid;		/* Subnetwork identifier */
    uint8            dn_regstage;	/* Registration stage */
    uint16           dn_frgsz;          /* Max fragment size of lower HW */
    struct llc2cn    dn_station;	/* Station component */
    uint8            dn_trace_active;   /* Tracing active on this subnetwork */
    queue_t         *dn_trace_queue;    /* Queue to send tracing on          */
    mblk_t          *dn_dlpi_ack;	/* Ack msg from LAN driver */
    caddr_t         eth_match;		/* handle for Ethernet matching */ 
    caddr_t         llc1_match;		/* handle for LLC1 matching */ 
    int              dn_bufcall;	/* bufcall id */
} llc2dn_t;

#define yrmac       dte+2
#define yrsap       dte[0]
#define mysap       dte[1]

#define ack_timer   tmtab[ACKTIMER]
#define dlx_timer   tmtab[DLXTIMER]
#define rej_timer   tmtab[REJTIMER]

/* Link level message headers */
union ll_un {
    uint8           ll_type;
    struct ll_reg   reg;
    struct ll_msg   msg;
    struct ll_msgc  msgc;
};


/*
 * Size of DLSAP address, word-aligned and not.
 */
#define DLADDR_SIZE	(MAXHWLEN + 2)
#define DLADDR_ALIGNED_SIZE ((DLADDR_SIZE + sizeof(long) - 1) & ~sizeof(long))

/*
 * Sizes of messages to pass upstream, in normal and connect cases,
 * for the DLPI interface.  (DLPI messages are larger than Spider
 * interface messages, so this is not an optimal use of space for the
 * Spider interface, but is faster than checking for the type of interface
 * each time buffers are allocated.)
 * NORMAL_UPSIZE = sizeof(unitdata_ind) + 2 * sizeof(DLSAP addr) +
 *  one extra byte, so that second addr can be word-aligned as well.
 */
#define NORMAL_UPSIZE   (int)(sizeof(dl_unitdata_ind_t) + \
				DLADDR_ALIGNED_SIZE + DLADDR_SIZE)
#define CONNECT_UPSIZE  (int)(sizeof(dl_connect_ind_t) + \
				DLADDR_ALIGNED_SIZE + DLADDR_SIZE)

#define	MBLKL(mp)   (mp->b_wptr - mp->b_rptr)
#define	BPSIZE(bp)  ((unsigned int)(bp->b_datap->db_lim - bp->b_datap->db_base))


/* macro for incrementing llc2 stats */
#define LLC2MONITOR(i) { \
if (lp->dnp->dn_statstab.llc2monarray[i]<0XFFFFFFFF) \
	lp->dnp->dn_statstab.llc2monarray[i]++; \
}


#ifdef DEBUG	/* { */
extern int	llc2_debug;

#define LLC2_PRINTF(str, arg)	if (llc2_debug) printf(str, arg);

#else
#define LLC2_PRINTF(str, arg)

#endif /* DEBUG } */

#ifdef SGI	/* { */
#include <sys/systm.h>
extern queue_t *lookup_sap(caddr_t,uint16*,uint8*,uint16,uint8*,uint16);
extern int register_sap(caddr_t, queue_t*, uint8*, uint16);
extern int deregister_sap(caddr_t, queue_t*, uint8*, uint16);
extern int llc2_maccpy(BUFP_T, BUFP_T, uint8);
extern int llc2_maccpy(BUFP_T, BUFP_T, uint8);
extern int llc2_disconnect(register llc2cn_t *);
extern void llc2_schedule(llc2cn_t *);
extern int llc2_dtecpy(register llc2cn_t*, llc2up_t*,
		register struct ll_msgc FAR*, uint8 []);
extern void end_match(caddr_t);
extern caddr_t init_match(void);
extern llc2cn_t *llc2_connectup(llc2up_t*);
extern int llc2_trace(llc2dn_t*, mblk_t*, int);
extern int llc2_rxputq(register llc2dn_t*, mblk_t*);
extern int llc2_settxwind(llc2cn_t*, uint8);
extern int send_Ucmd(register llc2cn_t *,int);
extern int send_Ursp(register llc2cn_t *,int);
extern llc2up_t *llc2_findup(llc2up_t *, uint8, int, int);
extern int closeup(llc2up_t*, int);

#ifdef LLCDBG	/* { */
extern void llc2_hexdump(unsigned char *, int);
#define DLP(a)	printf a
#define HEXDUMP(a,b)	llc2_hexdump(a,b)
#else
#define DLP(a)
#define HEXDUMP(a,b)
#endif		/* } */
#endif		/* } */
