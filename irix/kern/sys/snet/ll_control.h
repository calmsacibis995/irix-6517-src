#ident "$Revision: 1.2 $"
/******************************************************************
 *
 *  SpiderX25 Protocol Streams Component
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *  LL_CONTROL.H
 *
 *    LAPB Top-level STREAMS interface.
 *
 ******************************************************************/

/*
 *	ll_control.h
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/include/sys/snet/0/s.ll_control.h
 *	@(#)ll_control.h	1.28
 *
 *	Last delta created	18:35:19 10/7/92
 *	This file extracted	14:53:26 11/13/92
 *
 */
#ifndef X25_LL_CONTROL_
#define X25_LL_CONTROL_
/* IOCTL commands */
#define L_SETSNID   ('L'<<8 | 1) /* Set subnetwork identifier (use ll_snioc) */
#define L_GETSNID   ('L'<<8 | 2) /* Get subnetwork identifier (use ll_snioc) */
#define L_SETTUNE   ('L'<<8 | 3) /* Set tuning parameters (use ll_tnioc)     */
#define L_GETTUNE   ('L'<<8 | 4) /* Get tuning parameters (use ll_tnioc)     */
#define L_GETSTATS  ('L'<<8 | 5) /* Get statistics counts (use ll_stioc)     */
#define L_ZEROSTATS ('L'<<8 | 6) /* Zero statistics       (use ll_hdioc)     */
#define L_TRACEON   ('L'<<8 | 7) /* Set message tracing on.                  */
#define L_TRACEOFF  ('L'<<8 | 8) /* Set message tracing off.                 */

/* Values for 'lli_type' (with names of corresponding structures) */
#define LI_PLAIN	0x01        /* Indicates 'struct ll_hdioc'  */
#define LI_SNID		0x02        /* Indicates 'struct ll_snioc'  */
#define LI_STATS	0x04        /* Indicates 'struct ll_stioc'  */

#define LI_LAPBTUNE	0x13        /* Indicates 'struct lapb_tnioc'*/
#define LI_LLC2TUNE	0x23        /* Indicates 'struct llc2_tnioc'*/

/* LAPB tuning structure */
typedef struct lapbtune {
    uint16  N2;		    /* Maximum number of retries		*/
    uint16  T1;		    /* Acknowledgement time	(unit 0.1 sec)  */
    uint16  Tpf;            /* P/F cycle retry time	(unit 0.1 sec)  */
    uint16  Trej;           /* Reject retry time	(unit 0.1 sec)  */
    uint16  Tbusy;          /* Remote busy check time   (unit 0.1 sec)  */
    uint16  Tidle;          /* Idle P/F cycle time	(unit 0.1 sec)  */
    uint16  ack_delay;      /* RR delay time		(unit 0.1 sec)  */
    uint16  notack_max;     /* Maximum number of unack'ed Rx I-frames   */
    uint16  tx_window;      /* Transmit window size			*/
    uint16  tx_probe;       /* P-bit position before end of Tx window   */
    uint16  max_I_len;      /* Maximum I-frame length			*/
    uint16  llconform;      /* LAPB conformance				*/
} lapbtune_t;

/* Values for llconform field in lapbtune_t  */

#define	IGN_UA_ERROR       1	/* UA frames ignored in ERROR state	*/
#define	FRMR_FRMR_ERROR    2	/* FRMR is retransmitted in ERROR state	*/
#define	FRMR_INVRSP_ERROR  4	/* Invalid response frames are FRMR'ed	*/
#define	SFRAME_PBIT        8	/* S frame commands must have P-bit set	*/
#define	NO_DM_ADM         16	/* Dont send DM on entry to ADM state   */

/* LLC2 tuning structure */
typedef struct llc2tune {
    uint16  N2;		    /* Maximum number of retries		*/
    uint16  T1;		    /* Acknowledgement time	(unit 0.1 sec)  */
    uint16  Tpf;            /* P/F cycle retry time	(unit 0.1 sec)  */
    uint16  Trej;           /* Reject retry time	(unit 0.1 sec)  */
    uint16  Tbusy;          /* Remote busy check time   (unit 0.1 sec)  */
    uint16  Tidle;          /* Idle P/F cycle time	(unit 0.1 sec)  */
    uint16  ack_delay;      /* RR delay time		(unit 0.1 sec)  */
    uint16  notack_max;     /* Maximum number of unack'ed Rx I-frames   */
    uint16  tx_window;      /* Transmit window (if no XID received)	*/
    uint16  tx_probe;       /* P-bit position before end of Tx window   */
    uint16  max_I_len;      /* Maximum I-frame length			*/
    uint16  xid_window;     /* XID window size (receive window)		*/
    uint16  xid_Ndup;       /* Duplicate MAC XID count  (0 => no test)  */
    uint16  xid_Tdup;       /* Duplicate MAC XID time   (unit 0.1 sec)  */
} llc2tune_t;

/* Header alone (for decoding and L_ZEROSTATS commands) */
struct ll_hdioc {
    uint8           lli_type;      /* Table type = LI_PLAIN	*/
    uint8           lli_spare[3];  /*   (for alignment)		*/
    uint32          lli_snid;      /* Subnetwork ID character	*/
};

/* Ioctl block for L_SETSNID and L_GETSNID commands */
struct ll_snioc {
    uint8           lli_type;      /* Table type = LI_SNID		*/
    uint8           lli_spare[3];  /*   (for alignment)			*/
    uint32          lli_snid;      /* Subnetwork ID character		*/
    uint32          lli_index;     /* Link index			*/
};


/* Ioctl block for LAPB L_SETTUNE and L_GETTUNE commands */
struct lapb_tnioc {
    uint8           lli_type;      /* Table type = LI_LAPBTUNE		  */
    uint8           lli_spare[3];  /*   (for alignment)			  */
    uint32          lli_snid;      /* Subnetwork ID character ('*' => 'all') */

    lapbtune_t      lapb_tune;     /* Table of tuning values		  */
};

/* Ioctl block for LLC2 L_SETTUNE and L_GETTUNE commands */
struct llc2_tnioc {
    uint8           lli_type;      /* Table type = LI_LLC2TUNE		  */
    uint8           lli_spare[3];  /*   (for alignment)			  */
    uint32          lli_snid;      /* Subnetwork ID character ('*' => 'all') */

    llc2tune_t      llc2_tune;     /* Table of tuning values		  */
};


/* Statistics table definitions 	*/
/* Both LAPB and LLC2 			*/

#define RR_rx_cmd           0   /* RR = Receive Ready           */
#define RR_rx_rsp           1   /* tx = transmitted             */
#define RR_tx_cmd           2   /* rx = received                */
#define RR_tx_rsp           3   /* cmd/rsp = command/response   */
#define RR_tx_cmd_p         4   /* p = p-bit set                */

#define RNR_rx_cmd          5   /* RNR = Receive Not Ready      */
#define RNR_rx_rsp          6
#define RNR_tx_cmd          7
#define RNR_tx_rsp          8
#define RNR_tx_cmd_p        9

#define REJ_rx_cmd         10   /* REJ = Reject                 */
#define REJ_rx_rsp         11
#define REJ_tx_cmd         12
#define REJ_tx_rsp         13 
#define REJ_tx_cmd_p       14

#define SABME_rx_cmd       15   /* SABME = Set Asynchronous     */
#define SABME_tx_cmd       16   /*       Balanced Mode Extended */

#define DISC_rx_cmd        17   /* DISC = Disconnect            */
#define DISC_tx_cmd        18

#define UA_rx_rsp          19   /* UA = Unnumbered              */
#define UA_tx_rsp          20   /*      Acknowledement          */

#define DM_rx_rsp          21
#define DM_tx_rsp          22

#define I_rx_cmd           23   /* I = Information              */
#define I_tx_cmd           24

#define FRMR_rx_rsp        25   /* FRMR = Frame Reject          */
#define FRMR_tx_rsp        26

/* LAPB only */
#define SABM_rx_cmd        27   /* SABM = Set Asynchronous      */
#define SABM_tx_cmd        28   /*        Balanced Mode         */
#define SARM_rx_cmd        29   /* SARM = Set Asynchronous      */
#define SARM_tx_cmd        30   /*        Response Mode         */

#define lapbstatmax        31

/* LLC2 only */
/* values mutually exclusive with LAPB */

#define I_rx_rsp	   27
#define I_tx_rsp           28

#define UI_rx_cmd          29
#define UI_tx_cmd          30

#define XID_rx_cmd         31
#define XID_rx_rsp         32
#define XID_tx_cmd         33
#define XID_tx_rsp         34

#define TEST_rx_cmd        35
#define TEST_rx_rsp        36
#define TEST_tx_cmd        37
#define TEST_tx_rsp        38

#define llc2statmax        39


typedef struct llc2_stats {
	uint32	llc2monarray[llc2statmax]; /* array of LLC2 stats */
} llc2stats_t;

typedef struct lapb_stats {
	uint32	lapbmonarray[lapbstatmax]; /* array of LAPB stats */
} lapbstats_t;

/* Ioctl block for L_GETSTATS command */
struct llc2_stioc {
    uint8           lli_type;	   /* Table type = LI_STATS		*/
    uint8           lli_spare[3];  /*   (for alignment)			*/
    uint32          lli_snid;      /* Subnetwork ID character		*/
    llc2stats_t     lli_stats;     /* Table of stats values		*/
};

struct lapb_stioc {
    uint8           lli_type;   /* Table type = LI_STATS		*/
    uint8	    state;      /* connection state			*/
    uint16          lli_spare;  /*   (for alignment)			*/
    uint32          lli_snid;   /* Subnetwork ID character		*/

    lapbstats_t     lli_stats;  /* Table of stats values		*/
};


/* Union of ioctl blocks */
typedef union lli_union {
    struct ll_hdioc	ll_hd;      /* Parameter-less command       	*/
    struct ll_snioc	ll_sn;      /* Set/get subnetwork identifier    */
    struct lapb_tnioc   lapb_tn;    /* Set/get LAPB tuning          	*/
    struct llc2_tnioc   llc2_tn;    /* Set/get LLC2 tuning          	*/
    struct llc2_stioc   llc2_st;    /* Get llc2 statistics          	*/
    struct lapb_stioc   lapb_st;    /* Get lapb statistics          	*/
} lliun_t;
#endif /* X25_LL_CONTROL_ */
