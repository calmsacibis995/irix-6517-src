/******************************************************************
 *
 *  SpiderX25 Interface Primitives
 *
 *  Copyright (C) 1991  Spider Systems Limited
 *
 *  X25_CONTROL.H
 *
 *  X.25 PLP Streams control primitives for V.3
 *
 ******************************************************************/



/*
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/include/sys/snet/0/s.x25_control.h
 *	@(#)x25_control.h	1.54
 *
 *	Last delta created	17:03:18 4/21/92
 *	This file extracted	14:53:23 11/13/92
 *
 */

/******************************************************************
 *
 * M_IOCTL message type constants for I_STR 
 *
 */

#define	N_snident	(('N'<<8) | 1)
#define	N_snmode	(('N'<<8) | 2)
#define	N_snconfig	(('N'<<8) | 3)
#define	N_snread	(('N'<<8) | 4)
#define	N_getstats	(('N'<<8) | 5)
#define	N_zerostats	(('N'<<8) | 6)
#define	N_putpvcmap	(('N'<<8) | 7)
#define	N_getpvcmap	(('N'<<8) | 8)
#define	N_getvcstates	(('N'<<8) | 9)
#define N_getnliversion (('N'<<8) | 10)

/* Tracing */
#define N_traceon	(('N'<<8) | 11)
#define N_traceoff	(('N'<<8) | 12)


/*  NUI table manipulation */
#define N_nuimsg	(('N'<<8) | 13) /* prim_class for NUI msgs  */
#define N_nuiput	(('N'<<8) | 14) /* put entry into table     */
#define N_nuidel	(('N'<<8) | 15) /* delete entry from table  */
#define N_nuiget	(('N'<<8) | 16) /* return table entry       */
#define N_nuimget	(('N'<<8) | 17) /* return all table entries */
#define N_nuireset	(('N'<<8) | 18) /* reset address table      */



/******************************************************************
 *
 * M_IOCTL message type definitions 
 *
 */

#define NLI_VERSION 3	/* Current NLI version, returned by N_getnliversion */

struct nliformat {
	unsigned char	version;	/* NLI version number */
};

struct xll_reg {			/* For ioctl registration */
	struct ll_reg	lreg;		/* Link registration format */
	int		lmuxid;		/* LINK identifier */
};

struct snoptformat {
	unsigned int	U_SN_ID;
	unsigned short	newSUB_MODES;
	unsigned char	rd_wr;
};

struct pvcconff {
	unsigned int	  sn_id;        /* Subnetwork       */
	unsigned short    lci;          /* Logical channel  */
	unsigned char     locpacket;    /* Loc packet size  */
	unsigned char     rempacket;    /* Rem packet size  */
	unsigned char     locwsize;     /* Loc window size  */
	unsigned char     remwsize;     /* Rem window size  */
};

#define PVC_MAP_SIZE   (MAXIOCBSZ - 8)
#define MAX_PVC_ENTS   (PVC_MAP_SIZE / sizeof(struct pvcconff))

struct pvcmapf {
	struct pvcconff entries[MAX_PVC_ENTS]; /* Data buffer             */
	int             first_ent;             /* Where to start search   */
	unsigned char   num_ent;               /* Number entries returned */
};

struct smallvcinfo {
	unsigned char     xstate;       /* VC state         */
	unsigned char     xtag;         /* VC check record  */
	unsigned int	  xu_ident;     /* subnetwork id    */
	unsigned char	  ampvc;	/* =1 if a PVC	    */
};

#define VC_STATES_SIZE   (MAXIOCBSZ - 8)
#define MAX_VC_ENTS      (VC_STATES_SIZE / sizeof(struct smallvcinfo))

struct vcinfof {
	struct smallvcinfo  vcinfo[MAX_VC_ENTS];   /* Data buffer             */
	int                 first_ent;             /* Where to start search   */
	unsigned char       num_ent;               /* Number entries returned */
};


/************************ 
  NUI Table Manipulation 
 ************************/

#define NUIMAXSIZE	64
#define NUIFACMAXSIZE	32

struct nuiformat {
	unsigned char	nui_len;
	unsigned char	nui_string[NUIMAXSIZE];	   /* Network User Identifier */
};

struct facformat {
	unsigned short SUB_MODES;	/* Mode tuning bits for net */
        unsigned char  LOCDEFPKTSIZE;	/* Local default pkt p */
        unsigned char  REMDEFPKTSIZE;	/* Local default pkt p */
        unsigned char  LOCDEFWSIZE;	/* Local default window size */
        unsigned char  REMDEFWSIZE;	/* Local default window size */
	unsigned char  locdefthclass;   /* Local default value         */
	unsigned char  remdefthclass;   /* Remote default value        */
	unsigned char  CUG_CONTROL;	/* CUG facilities */
};

/*
 * Address table manipulation primitives
 */

struct nui_put {
	char	prim_class;	/* Always NUI_MSG			*/
	char	op;		/* Always NUI_ENT			*/
	struct	nuiformat	nuid;		/* NUI			*/
	struct	facformat	nuifacility;	/* NUI facilities	*/
};

struct nui_del {
	char	prim_class;	/* Always NUI_MSG			*/
	char	op;		/* Always NUI_DEL			*/
	struct	nuiformat	nuid;		/* NUI	to delete	*/
};

struct nui_get {
	char		prim_class;	/* Always NUI_MSG			*/
	char		op;		/* Always NUI_GET			*/
	struct nuiformat	nuid;		/* NUI to get		*/
/* Following field filled in in reply: (as for NUI_PUT)	*/
	struct facformat	nuifacility;	/* NUI facilities	*/
};

struct nui_reset {
	char		prim_class;	/* Always NUI_MSG	*/
	char		op;		/* Always NUI_RESET	*/
};

typedef struct nui_addr {
	struct facformat	nuifacility;	/* NUI facilities	*/
	int     		next_free;	/* Next free entry      */
	struct nuiformat	nuid;		/* NUI			*/
	unsigned char		status;		/* temp/permanent entry */
} NUI_ADDR;

#define NUI_TEMPORARY	1	/* temporary address entry status	*/
#define NUI_PERMANENT	2	/* permanent address entry status	*/

/*
 *  Defines for the Multiple Get IOCTL
 *
 *  NOTE: This size of structure "nui_mget" must not
 *	  exceed MAXIOCBSZ byte (as defined in stream.h).
 *	  Thus if other fields are added to "nui_mget"
 *	  then MGET_NBUFSIZE must decrease to cope
 *	  with any changes made.
 */
 
#define MGET_NBUFSIZE	(MAXIOCBSZ - 20)	/*
						 * Take into account the
						 * other 5 fields by reducing
						 * the buffer size by 20
						 */
#define MGET_NMAXENT	(MGET_NBUFSIZE / sizeof(NUI_ADDR) ) 


struct nui_mget {
	char		prim_class;	   /* Always NUI_MSG			*/
	char		op;		   /* Always NUI_MGET			*/
	unsigned int 	first_ent;	   /* First entry required		*/
	unsigned int 	last_ent; 	   /* Last entry required		*/
	unsigned int 	num_ent;	   /* Number of entries required	*/
	char 		buf[MGET_NBUFSIZE]; /* Data Buffer			*/
};

typedef union nui_adrop {
	char	prim_class;		/* Always NUI_MSG		*/
	struct	nui_put   nui_put;
	struct	nui_del   nui_del;
	struct	nui_get   nui_get;
	struct	nui_mget  nui_mget;
	struct	nui_reset nui_reset;
} S_NUI_ADROP;


/* ----------------------------------------------------------------------
 *
 * Definition for the configuration record structure.  Includes all
 *  parameters.
 * ---------------------------------------------------------------------- 
 */

struct wlcfg {
	unsigned int   U_SN_ID;		/* Upper level id for net (soft) */
	unsigned char  NET_MODE;	/* Prot/net in use e.g. PinkBook */
	unsigned char  X25_VSN;		/* Non-zero : 1984, otherwise 1980 */
	unsigned char  L3PLPMODE;	/* 1 DTE, 0 DCE, 2 DXE */

        /* X25 PLP virtual circuit ranges */

	short         LPC;               /* Lowest  Permanent VC           */
	short         HPC;               /* Highest Permanent VC           */
        short         LIC;               /* Lowest  Incoming channel       */
        short         HIC;               /* Highest Incoming channel       */
        short         LTC;               /* Lowest  Two-way channel        */
        short         HTC;               /* Highest Two-way channel        */
        short         LOC;               /* Lowest  Outgoing channel       */
        short         HOC;               /* Highest Outgoing channel       */
	short         NPCchannels;	 /* Number PVC channels            */
	short         NICchannels;       /* Number IC channels             */
	short         NTCchannels;       /* Number TC channels             */
	short         NOCchannels;       /* Number OC channels             */
        short         Nochnls;           /* Total number of channels       */

	unsigned char  THISGFI;		/* GFI operating on net */
        unsigned char  LOCMAXPKTSIZE;	/* Max value acceptable for pkt p */
        unsigned char  REMMAXPKTSIZE;	/* Max value acceptable for pkt p */
        unsigned char  LOCDEFPKTSIZE;	/* Local default pkt p */
        unsigned char  REMDEFPKTSIZE;	/* Local default pkt p */
	unsigned char  LOCMAXWSIZE;	/* Max value acceptable for wsize */
	unsigned char  REMMAXWSIZE;	/* Max value acceptable for wsize */
        unsigned char  LOCDEFWSIZE;	/* Local default window size */
        unsigned char  REMDEFWSIZE;	/* Local default window size */

        unsigned short MAXNSDULEN;	/* Max data delivery to N-user */

        /* X25 PLP timer and retransmission values */

        short         ACKDELAY;		 /* Ack suppress and buffs low     */
        short         T20value;          /* Restart request                */
        short         T21value;          /* Call request                   */
        short         T22value;          /* Reset request                  */
        short         T23value;          /* Clear request                  */

        short         Tvalue;            /* Ack and busy timer             */
        short         T25value;          /* Window rotation timer          */
        short         T26value;          /* Interrupt response             */

        short         idlevalue;         /* Idle timeout value for link    */
        short         connectvalue;      /* Link connect timer             */

        unsigned char R20value;          /* Restart request                */
        unsigned char R22value;          /* Reset request                  */
        unsigned char R23value;          /* Clear request                  */

	/* Local values for qos checking */

	unsigned short localdelay;       /* Internal delay locally      */
	unsigned short accessdelay;      /* Line access delay locally   */

	/* Throughput Classes */
	
	unsigned char  locmaxthclass;    /* Local max thruput           */
	unsigned char  remmaxthclass;    /* Remote max thruput          */
	unsigned char  locdefthclass;    /* Local default value         */
	unsigned char  remdefthclass;    /* Remote default value        */
	unsigned char  locminthclass;    /* Local minimum for the PSDN  */
	unsigned char  remminthclass;    /* Remote minimum for the PSDN */

	unsigned char  CUG_CONTROL;		/* CUG facilities subscribed to   */
	unsigned int  SUB_MODES;	/* Mode tuning bits for net */
	unsigned char NUI_override; 

	/* PSDN localisation record */

	struct {
	unsigned short SNMODES;			/* Mode tuning for PSDN */
	unsigned char intl_addr_recogn;		/* Recognise intnatl  */
	unsigned char intl_prioritised;		/* Prioritise intnatl */
	unsigned char dnic1;			/* 4 BCD digits DNIC  */
	unsigned char dnic2;			/* used when required */
	unsigned char prty_encode_control;	/* Encode priority    */
	unsigned char prty_pkt_forced_value;    /* Force pkt size     */
	unsigned char src_addr_control;		/* Calling addr fixes */
	unsigned char dbit_control;		/* Action on D-bit    */
	unsigned char thclass_neg_to_def;       /* TELENET negn type  */
	unsigned char thclass_type;		/* tclass map handle  */
	unsigned char thclass_wmap[16];		/* tclass -> wsize    */
	unsigned char thclass_pmap[16];		/* tclass -> psize    */
	} psdn_local;

	/* Link level local address or local DTE address */

	struct lsapformat local_address;

        };

/* ---- SUBNET TUNING MODE BITS ---- */

#define    SUB_EXTENDED           1  /* Subscribe extended fac               */
#define    BAR_EXTENDED           2  /* Bar extended incoming                */
#define    SUB_FSELECT            4  /* Subscribe fselect fac                */
#define    SUB_FSRRESP            8  /* Subscribe fs restrict                */
#define    SUB_REVCHARGE         16  /* Subscribe rev charge                 */
#define    SUB_LOC_CHG_PREV      32  /* Subscribe local charging prevention  */
#define    BAR_INCALL            64  /* Bar incoming calls                   */
#define    BAR_OUTCALL          128  /* Bar ougoing calls                    */
#define    SUB_TOA_NPI_FMT      256  /* Subscribe TOA/NPI Address Format     */
#define    BAR_TOA_NPI_FMT      512  /* Bar TOA/NPI Address Format incoming  */
#define    SUB_NUI_OVERRIDE    1024  /* Subscribe NUI Override  */
#define    AND_80                 0  /* BIT RESERVED 84/80                   */

/* ---- PSDN LOCALISATION TUNING MODE BITS ---- */

#define    ACC_NODIAG             1  /* Short clr etc is OK                  */
#define    USE_DIAG               2  /* Use diagnostic packet                */
#define    CCITT_CLEAR_LEN        4  /* Restrict Clear lengths               */
#define    BAR_DIAG               8  /* Bar diagnostic packets               */
#define    DISC_NZ_DIAG          16  /* Discard diag packets on non-zero LCN */
#define    ACC_HEX_ADD           32  /* Allow hex digits in dte addresses    */
#define    BAR_NONPRIV_LISTEN    64  /* Bar non privileged users from   
                                        listening for incoming calls.        */
/* ---- CUG SUBSCRIPTION BITS ------- */

#define		SUB_CUG			1
#define 	SUB_PREF		2
#define		SUB_CUGOA		4
#define		SUB_CUGIA		8
#define		BASIC			16
/* inna - avoid redefine in sys/dvh.h... Change from EXTENDED */
#define		X25_EXTENDED		32
#define 	BAR_CUG_IN		64

/* ----- D-BIT CONTROL ACTIONS ------- */

#define 	CC_IN_CLR		1
#define 	CC_IN_ZERO		2
#define		CC_OUT_CLR		4
#define 	CC_OUT_ZERO		8
#define 	DT_IN_RST		16
#define  	DT_IN_ZERO		32
#define		DT_OUT_RST		64
#define		DT_OUT_ZERO		128


/* ------ NETWORK MODE TYPES --------- */

/* (Only X25_LLC and DATAPAC currently plumbed into code) */

#define		X25_LLC		1	/*  X.25(84)/LLC-2	*/
#define		X25_88		2	/*  X.25(88)		*/
#define		X25_84		3	/*  X.25(84)		*/
#define		X25_80		4	/*  X.25(80)		*/
#define		PSS		5	/*  UK			*/
#define		AUSTPAC		6	/*  Australia		*/
#define		DATAPAC		7	/*  Canada		*/
#define		DDN		8	/*  USA			*/
#define		TELENET		9	/*  USA			*/
#define		TRANSPAC	10	/*  France		*/
#define		TYMNET		11	/*  USA			*/
#define		DATEX_P		12	/*  West Germany	*/
#define		DDX_P		13	/*  Japan		*/
#define		VENUS_P		14	/*  Japan		*/
#define		ACCUNET		15	/*  USA			*/
#define		ITAPAC		16	/*  Italy		*/
#define		DATAPAK		17	/*  Sweden		*/
#define		DATANET		18	/*  Holland		*/
#define		DCS		19	/*  Belgium		*/
#define		TELEPAC		20	/*  Switzerland		*/
#define		F_DATAPAC	21	/*  Finland		*/
#define		FINPAC		22	/*  Finland		*/
#define		PACNET		23	/*  New Zealand		*/

#define		G_8		0X10	/* GFI - not extended sequencing   */
#define		G_128		0X20	/* GFI - extended sequencing       */

/* Statistical Information */

#define		cll_coll	44	/* Call collision count (not rjc)  */
#define		cll_in		45	/* Calls rcvd and indicated        */
#define		cll_out		46	/* Calls sent                      */
#define		cll_uabort	47	/* Calls aborted by user b4 sent   */
#define		rjc_buflow	48	/* Calls rejd no buffs b4 sent     */
#define		rjc_coll	49	/* Calls rejd - collision DCE mode */
#define		rjc_failNRS	50	/* Calls rejd negative NRS resp    */
#define		rjc_lstate	51	/* Calls rejd link disconnecting   */
#define		rjc_nochnl	52	/* Calls rejd no lcns left         */
#define		rjc_nouser	53	/* In call but no user on NSAP     */
#define		rjc_remote	54	/* Call rejd by remote responder   */
#define		rjc_u		55	/* Call rejd by NS user            */
#define		caa_in		56	/* Call established for outgoing   */
#define		caa_out		57	/* Ditto - in call                 */
#define		dg_in		58	/* DIAG packets in                 */
#define		dg_out		59	/* DIAG packets out                */
#define		dt_in		60	/* Data packets rcvd               */
#define		dt_out		61	/* Data packets sent               */
#define		ed_in		62	/* Interrupts rcvd                 */
#define		ed_out		63	/* Interrupts sent                 */
#define		p4_ferr		64	/* Format errors in P4             */
#define		rem_perr	65	/* Remote protocol errors          */
#define		res_ferr	66	/* Restart format errors           */
#define		res_in		67	/* Restarts received (inc DTE/DXE) */
#define		res_out		68	/* Restarts sent (inc DTE/DXE)     */
#define		rnr_in		69	/* Receiver not ready rcvd         */
#define		rnr_out		70	/* Receiver not ready sent         */
#define		rr_in		71	/* Receiver ready rvcd             */
#define		rr_out		72	/* Receiver ready sent             */
#define		rst_in		73	/* Resets rcvd                     */
#define		rst_out		74	/* Resets sent                     */
#define		vcs_labort	75	/* Circuits aborted via link event */
#define		r23exp		76	/* Circuits hung by r23 expiry     */
#define		l2conin		77	/* Link level connect established  */
#define		l2conok		78	/* LLC connections accepted        */
#define		l2conrej	79	/* LLC connections rejd            */
#define		l2refusal	80	/* LLC connnect requests refused   */
#define		l2lzap		81	/* Oper requests to kill link      */
#define		l2r20exp	82	/* R20 retransmission expiry       */
#define		l2dxeexp	83	/* DXE/connect expiry              */
#define		l2dxebuf	84	/* DXE resolv abort - no buffers   */
#define		l2noconfig	85	/* No config base - error          */
#define		xiffnerror	86	/* Upper i/f bad M_PROTO type	   */
#define		xifuserror	87	/* Upper user fn/state error	   */
#define		xintdisc	88	/* Internal disconnect events	   */
#define		xifaborts	89	/* Interface abort_vc called	   */
#define		PVCusergone	90	/* Count of non-user interactions  */
#define		mon_size	91	/* 1 over last, for length         */

