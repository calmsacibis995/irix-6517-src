#ifndef __SMTD_H__
#define __SMTD_H__
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.27 $
 */

#include <net/if.h>
#include <netinet/if_ether.h>
#include <net/raw.h>
#include <sys/fddi.h>
#include <sys/smt.h>

/*
 * Value returned by smtd for a FDDI_SNMP_VERSION query.
 * Must be updated whenever changes to structures in this file
 * or <sys/smt.h> might affect SMT clients.
 */
#define SMTD_VERS	2


#define SMTPROG		391005		/* RPC program # */
#define SMTVERS		1
#define SMTPROC_NULL	0
#define SMTPROC_STAUP	1
#define SMTPROC_STADOWN 2
#define SMTPROC_SNMP	3
#define SMTPROC_SENDFRM 4
#define SMTPROC_RECVFRM 5
#define SMT_NPROC	6

#define SMTD_UDS	"/etc/.smtd"	/* limit is 14-1 */

#define SMTD_SERVICE	"smtd"

#define NSMTD		24


/* milliseconds to int byte clocks */
#define TOBCLK(x)	((int)(x * -12500))

/* to floating milliseconds */
#define FROMBCLK(x)	(((signed)x) / -12500.0)

/* convert from official values in fddi.h to byte clocks */
#define FDDITOBCLK(x) (x)

/* add halves of driver counter.
 *	4294967296=0x10000*0x10000 */
#define LCTR(x)	((x.hi * 4294967296.0) + x.lo)



/* PATH */
#define NUM_PATH_CLASSES	2
typedef enum {
	PO_UNSUPPORT,	/* not supported */
	PO_ASCENDING,	/* PHYS in path numbered ascending */
	PO_DESCENDING	/* PHYS in path numbered descending */
} PATH_ORDER;

typedef enum {
	PT_NOTRACE,	/* no current TRACE */
	PT_INIT,	/* TRACE initialized */
	PT_PROP,	/* TRACE propagated */
	PT_TERM		/* TRACE terminated */
} PATH_TRACE;

typedef	struct {
	long	rid;
	long	maxtrace;	/* default = 7sec */
	long	tvx_lobound;	/* same as phy->tvx_lobound */
	long	tmax_lobound;	/* same as phy->tmax_lobound */
} SMT_PATHCLASS;
#define MAX_PATHCLASSES	2	/* one for local, one for non-local */

typedef enum {
	WRAPPED =0,
	    THRU=1
} SMT_PATHSTATUS;

#define PATHTYPE_UNKNOWN	0
#define PATHTYPE_PRIMARY	1
#define PATHTYPE_SECONDARY	2
#define PATHTYPE_LOCAL		4
#define PATHTYPE_ISOLATED	8

/* PATH Config Grp */
typedef struct smt_path {
	u_long		rid;
	SMT_PATHCLASS	*pathclass;
	u_long		type;
	PATH_ORDER	order;
	u_long		latency;
	PATH_TRACE	tracestatus;
	u_long		sba;
	u_long		sbaoverhead;
	u_long		status;
#define PATH_MACTYPE	2
#define PATH_PORTTYPE	4
	u_long		rtype;	/* resource type 2=mac,4=port*/
	u_long		rmode;
} SMT_PATH;

/*******************************************************/
/* PORT */

typedef enum {                          /* FDDI PHY CONNECT STATE */
	PC_DISABLED,
	PC_CONNECTING,
	PC_STANDBY,
	PC_ACTIVE
} PHY_CONN_STATE;

typedef enum {
	PW_NONE,
	PW_MM,
	PW_OTHER
} PHY_WITHHOLD;

typedef enum {
	PF_MULTI,
	PF_SMODE1,
	PF_SMODE2,
	PF_SONET
} PMD_FOTX;

typedef enum {
	ATT_SAS,		/* single attachment */
	ATT_DAS,		/* dual attachment */
	ATT_CONCENTRATOR	/* concentrator */
} FDDI_ATTACH;

typedef enum {
	ISOLATED = 0,	/* CE0 */
	INSERT_P = 1,	/* CE1 */
	INSERT_S = 2,	/* CE2 */
	INSERT_X = 3,	/* CE3 */
	LOCAL = 4	/* CE4 */
} CE_STATE;

typedef struct smt_phy {
	struct smt_phy *next;	/* what else */
	u_long		rid;	/* resource id */

	/* config */
	int		debug;	/* kernel debug level */
	int		ip_pri; /* llc priotiry */
	enum pcm_tgt	pcm_tgt;/* same as maint_ls */
	u_long		pcm_flags;

	/* flags */
	u_long		uica_flag;	/* undesired connection attempts */

	/* path info */
	SMT_PATH	path;

	/* attachment info */
	enum fddi_st_type type;	/* attatch class. SAS, etc... */
	u_long		obs;	/* optical bypass present */
	long		imax;	/* mand iff obs present */
	u_long		insert;	/* insert status and ditto */
	u_long		ipolicy;/* insert policy and ditto */

	/* PHY Config Grp */
	enum fddi_pc_type	pctype;
	enum fddi_pc_type	pcnbr;
#define PC_MAC_LCT              0x1
#define PC_MAC_LOOP             0x2
#define PC_MAC_PLACEMENT        0x4
	u_long          phy_connp;	/* LCT|PLACEMENT */
	u_long		remotemac;	/* remote mac indicated */
	CE_STATE	ce_state;	/* cur ce state */
	u_long		pathrequested;
	u_long		macplacement;	/* this macrid xmit via this port*/
	u_long		pathavail;
	u_long		loop_time;
	PMD_FOTX	fotx;

	/* PHY Operation Grp */
	enum pcm_ls	maint_ls;
	long		tb_max;
	u_long		bs_flag;	/* break state flag */

	/* PHY Error Ctrs Grp */
	u_long		eberr_delta;	/* eberrs during latest T_NOTIFY */
	u_long		eberr_ct;	/* elasticy buf err ct */
	u_long		lctfail_ct;	/* link confidency test fail */

	/* PHY Ler Grp */
#define MAX_LER	15
#define MIN_LER	4
	u_long		ler_estimate;	/* absolute exponent 4-15 */
	u_long		lemreject_ct;
	u_long		lem_ct;
	u_long		bler_estimate;	/* 4-15 */
	u_long		blemreject_ct;
	u_long		blem_ct;
	struct timeval	bler_ts;	/* base ler time stamp */
	u_long		ler_cutoff;	/* input: 4-15 */
	u_long		ler_alarm;	/* input: threshold 4-15 */

	/* PHY Status Grp */
	PHY_CONN_STATE	conn_state;
	enum pcm_state	pcm_state;
	PHY_WITHHOLD    withhold;
	u_long		pler_cond;

	/* RMT counters */
	enum rt_ls tls;
	u_long phyplacement;		/* indicate if port was ever thru */
	struct smt_lcntr clm;		/* entered MAC T4 */
	struct smt_lcntr bec;		/* entered MAC T5 */
	struct smt_lcntr tvxexp;	/* TVX expired */
	struct smt_lcntr trtexp;	/* TRT expired and late count != 0 */
	struct smt_lcntr myclm;		/* my CLAIM frames received */
	struct smt_lcntr loclm;		/* lower CLAIM frames received */
	struct smt_lcntr hiclm;		/* higher CLAIM frames received */
	struct smt_lcntr mybec;		/* my beacon frames received */
	struct smt_lcntr otrbec;	/* other beacon frames */
	struct smt_lcntr dup_mac;	/* RMT duplicate MAC frames */

	/* back ptr to mac */
	caddr_t		mac;
} SMT_PHY;

/***************************************************/
/* MAC */
typedef enum {
	DUPA_NONE,
	DUPA_PASS,
	DUPA_FAIL
} DUPA_TEST;

typedef enum {
	RM_ISOLATED,			/* RM0 */
	RM_NONOP,			/* RM1 */
	RM_RINGOP,			/* RM2 */
	RM_DETECT,			/* RM3 */
	RM_NONOP_DUP,			/* RM4 */
	RM_RINGOP_DUP,			/* RM5 */
	RM_DIRECTED,			/* RM6 */
	RM_TRACE			/* RM7 */
} RMT_STATE;


/* XXX This structure is unfortunately passed to remote clients such as
 * fddivis.  That means the structure cannot change size and elements
 * that remote clients care about cannot move.
 */
typedef struct smt_mac {
	struct smt_mac *next;
	u_long		rid;

	int		maxflops;	/* stop resetting after this */
	int		flops;		/* have reset board this many times */
	int		flips;		/* have reset bhard this many times */
	/* XXX tflop & tflop belong here, but that would move things */

	char		name[10];
	int		smtsock;	/* smt frame port */
	int		statsock;	/* smt stat port */
	enum fddi_st_type st_type;
	SMT_PATH	path;
	u_long		phy_ct;
	SMT_PHY		*primary;
	SMT_PHY		*secondary;

	/* RMT state machine */
#define	RMF_LOOP	0x00000001
#define	RMF_JOIN	0x00000002
#define	RMF_LOOP_AVAIL	0x00000004
#define	RMF_MAC_AVAIL	0x00000008
/* #define RMF_unused	0x00000010 */
#define	RMF_JM_FLAG	0x00000020
#define	RMF_NO_FLAG	0x00000040
#define	RMF_RE_FLAG	0x00000080
#define	RMF_TRT_EXP	0x00000100	/* TRT expired in state T4 or T5 */
#define RMF_DUP_DET	0x00000200	/* board detected duplicate addr */
#define RMF_DUP		0x00000400
#define	RMF_TBID	0x00000800
#define	RMF_MYBCN	0x00001000
#define	RMF_OTRBCN	0x00002000
#define	RMF_MYCLM	0x00004000
#define	RMF_OTRCLM	0x00008000
#define	RMF_BN_FLAG	0x00010000
#define	RMF_CLAIM_FLAG	0x00020000
#define	RMF_T4		0x00040000
#define	RMF_T5		0x00080000
	u_long		rmt_flag;
	u_long		ringop;
	time_t		pcmtime;	/* last time connected */
	time_t		boottime;	/* when the counters were started */
	time_t		tflop;		/* time when last input received */
	time_t		tflip;		/* when last able to send a frame */

	/* nbr notify states */
#define T_NOTIFY_MAX	30		/* seconds */
#define T_NOTIFY_MIN	2
#define T_NOTIFY	T_NOTIFY_MAX
#define T_NN_RETRY	2		/* try twice before dupa detect set */
#define T_NN_OUT	(168 + T_NN_RETRY * T_NOTIFY)
	time_t		tnn;
	time_t		tuv;
	time_t		tdv;
#define NT0	0
#define NT1	1
	u_long		nn_state;
	u_long		nn_xid;		/* NN transaction id */
	u_long		uv_flag;	/* UNA valid */
	u_long		dv_flag;	/* DNA valid */
#define SMT_DUPA_ME	1		/* My station detected dupa */
#define SMT_DUPA_UNA	2		/* UNA detected dupa */
#define da_flag		dupa_flag
#define	unada_flag	unda_flag
	u_long		dupa_flag;
	u_long		unda_flag;	/* UNA detected dupa */


	/* MAC Capabilities Grp */
#define FSC_TYPE0		0x1     /* MAC repeats A/C indicators */
#define FSC_TYPE1               0x2     /* Mac set C but A on forwarding */
#define FSC_TYPE2               0x4     /* Mac invert A/C */
#define FSC_TYPE0_prog          0x100
#define FSC_TYPE1_prog          0x200
#define FSC_TYPE2_prog          0x400
	u_long		fsc;		/* Frame Status Capability */
#define BRIDGE_TYPE0            0x1     /* transparent bridge - 802.1b */
#define BRIDGE_TYPE1            0x2     /* src routing bridge - 802.5  */
	u_long          bridge;
	u_long		tmax_lobound;	/* tmax */
	u_long		tvx_lobound;	/* tvx */

	/* MAC Config Grp */
	u_long		pathavail;
	u_long		curpath;
	LFDDI_ADDR	una;
	LFDDI_ADDR	dna;
	LFDDI_ADDR	old_una;
	LFDDI_ADDR	old_dna;
	u_long		rootconcentrator;
	DUPA_TEST	dupa_test;
	u_long		pathrequested;	/* desired paths */
	u_long		dnaport;	/* same as primary->pc_nbr */

	/* MAC Address Grp */
	LFDDI_ADDR	addr;

	u_char		changed;	/* something changed recently
					 * XXX move this to sampletime */

	u_char		rsrvd_short;	/* XXX reserved space */

	struct smt_st	*st;
	struct smt_st	*st1;

	u_long		rsrvd_int[30];	/* XXX reserved space */

	/* XXX these should be moved when this structure is no longer
	 * sent to other programs over the network.
	 */
	time_t		trm_base;	/* RMT timer reset then */
	time_t		trm_ck;		/* check RTM then */

	u_long		ifflag;
	u_long		pathplacement;

#define SMT_MAX_ALIASES	1
#define SMT_MAX_GRPADRS	1
	LFDDI_ADDR	l_alias[SMT_MAX_ALIASES];
	SFDDI_ADDR	s_alias[SMT_MAX_ALIASES];
	LFDDI_ADDR	l_grpaddr[SMT_MAX_GRPADRS];
	SFDDI_ADDR	s_grpaddr[SMT_MAX_GRPADRS];
	u_long		la_ct;
	u_long		sa_ct;
	u_long		lg_ct;
	u_long		sg_ct;

	long		tneg;
	long		treq;
	long		tmax;
	long		tvx;
	long		tmin;

	/* MAC Operation Grp */
	long		pri0;
	long		pri1;
	long		pri2;
	long		pri3;
	long		pri4;
	long		pri5;
	long		pri6;
	u_long		curfsc;

	/* MAC Counter Grp */
	u_long		frame_ct;
	u_long		recv_ct;
	u_long		xmit_ct;
	u_long		token_ct;

	/* MAC Error Ctrs Grp */
	u_long		err_ct;
	u_long		lost_ct;
	u_long		tvxexpired_ct;
	u_long		notcopied_ct;
	u_long		late_ct;
	u_long		ringop_ct;

	/* MAC Frame Condition Grp */
	u_long		bframe_ct;	/* base frame count */
	u_long		berr_ct;	/* base error count */
	u_long		blost_ct;	/* base lost count */
	struct timeval	basetime;	/* base time mac condition */
	u_long		fr_threshold;	/* bframe report threshold */
	u_long		fr_errratio;	/* frame error ratio */
	struct timeval	sampletime;	/* when MAC condition last checked */

	/* MAC NotCopied Condition Grp */
	u_long		bnc_ct;		/* base notcopied count */
	struct timeval	btncc;		/* base time notcopied condition */
	u_long		fnc_threshold;	/* frame notcpd threshlod */
	u_long		brcv_ct;	/* base recv cnt */
	u_long		nc_ratio;	/* not copied ratio */

	/* MAC Status Grp */
	RMT_STATE	rmtstate;
	u_long		frm_errcond;
	u_long		frm_nccond;
	u_long		llcavail;	/* always true */

	/* MAC Root MAC Status Grp */
#define MSL_UNKNOWN	0
#define MSL_SUSPECTED	1
#define MSL_NOLOOP	2
	u_long		msloop_stat;
	u_long		root_dnaport;
	u_long		root_curpath;

	/* back ptr to smtd */
	int		iid;
} SMT_MAC;

/***********************************************************/
/* SMT */

typedef enum {
	SMT_STATION = 0,
	SMT_CONCENTRATOR
} SMT_NODECLASS_TYPE;

typedef enum {
	EC_OUT,                 /* EC0 */
	EC_IN,                  /* EC1 */
	EC_TRACE,               /* EC2 */
	EC_LEAVE,               /* EC3 */
	EC_PATHTEST,            /* EC4 */
	EC_INSERT,              /* EC5 */
	EC_CHECK                /* EC6 */
} ECM_STATE;

typedef enum {
	SC_ISOLATED,            /* SC0: isolated DAS,SAS */
	SC_WRAP_A,              /* SC1: Wrap_A DAS */
	SC_WRAP_B,              /* SC2: Wrap_B DAS */
	SC_WRAP_AB,             /* SC3: Wrap_AB DAS */
	SC_THRU_A,              /* SC4: Thru A DAS */
	SC_THRU_B,              /* SC5: Thru_B DAS */
	SC_THRU_AB,             /* SC6: Thru_AB DAS */
	SC_WRAP_S               /* SC7: Wrap_S SAS */
} CFM_STATE;

typedef struct {
	u_long  sc_cnt;         /* count */
	struct timeval sc_ts;	/* time stamp */
} setcount_t;

typedef struct smt_fs_info {
	struct smt_fs_info *next;
	int lsock;
	int timo;
	int fc;		/* SMT frame class */
	int ft;		/* SMT frame type: if 0, return any type */
	int svcnum;	/* number of frames svced to the port */
	union {
		struct sockaddr_in to;
		LFDDI_ADDR da;
		int anum[2];
	} u;
} SMT_FS_INFO;
#define fsi_to	u.to
#define fsi_da	u.da

#define fss_fc		lsock
#define fss_ireq	timo
#define fss_ires	fc
#define fss_ianoun	ft
#define fss_oreq	svcnum
#define fss_ores	u.anum[0]
#define fss_oanoun	u.anum[1]

#define MIN_FS_REGTIMO	(10*60+10)	/* seconds fddivis can be registered
					 * must be > value used by fddivis */
#define MAX_FS_REGTIMO	(MIN_FS_REGTIMO*2)

#define SMT_MAXFSENTRY	20
typedef struct smt_fsstat {
	int num;
	SMT_FS_INFO entry[SMT_MAXFSENTRY];
} SMT_FSSTAT;

#define HS_DISABLED	0
#define HS_PRIMARY	1
#define HS_SECONDARY	2
typedef enum {
	not_holding = HS_DISABLED,
	holding_prm = HS_PRIMARY,
	holding_sec = HS_SECONDARY
} HOLD_STATE;

typedef struct {
	/* Local mgmt */
	SMT_FS_INFO *fs;		/* frame service registered */
	enum fddi_st_type	st_type;
	char		trunk[10];
#define SMTXID	(++(smtp->xid))
	u_long		xid;
	u_long		phy_ct;
	SMT_MAC		*mac;
	SMT_PHY		*phy;
	SMT_PATHCLASS	pathclass[MAX_PATHCLASSES];

#define	SMT_WRAPPED	0x1	/* wraped */
#define	SMT_UNROOT	0x2	/* Unrooted Concentrator */
#define SMT_AA_TWIST	0x4
#define SMT_BB_TWIST	0x8
#define SMT_ROOTSTA	0x10	/* rooted station */
#define SMT_DO_SRF	0x20	/* do SRF */
	u_long	topology;	/* flag */

	SMT_NODECLASS_TYPE	nodeclass;
	u_long		pmf_on;
	u_long		trace_on;	/* RMT tracing */

	/* Stationid Group */
	SMT_STATIONID		sid;
#define SMT_VER         1       /* SMT-rev 6.2 version */
#define SMT_VER_LO      1
#define SMT_VER_HI      1
	/* 0 < version < 256 : implied */
	u_long		vers_op;
	u_long		vers_hi;
	u_long		vers_lo;
	SMT_MANUF_DATA  manuf;
	USER_DATA       userdata;

	/* Station Config Grp */
	u_long          mac_ct;		/* tatal mac cnt */
	u_long          nonmaster_ct;	/* non master mac cnt */
	u_long          master_ct;	/* master mac cnt */
	u_long		pathavail;
#define SMT_HOLDAVAIL	1
#define SMT_WRAPAB	2
	u_long		conf_cap;
	u_long		conf_policy;	/* same as conf_cap */
#define SMT_REJECT_AA	0x0001
#define SMT_REJECT_AB	0x0002
#define SMT_REJECT_AS	0x0004
#define SMT_REJECT_AM	0x0008
#define SMT_REJECT_BA	0x0010
#define SMT_REJECT_BB	0x0020
#define SMT_REJECT_BS	0x0040
#define SMT_REJECT_BM	0x0080
#define SMT_REJECT_SA	0x0100
#define SMT_REJECT_SB	0x0200
#define SMT_REJECT_SS	0x0400
#define SMT_REJECT_SM	0x0800
#define SMT_REJECT_MA	0x1000
#define SMT_REJECT_MB	0x2000
#define SMT_REJECT_MS	0x4000
#define SMT_REJECT_MM	0x8000
	u_long          conn_policy;
#define SMT_DEFAULT_RPORTLIMIT	5
	u_long		reportlimit;
	u_long		t_notify;
	u_long		srf_on;

	/* SMTStatusGrp */
	ECM_STATE	ecmstate;
	CFM_STATE	cf_state;
	HOLD_STATE	hold_state;
	u_long		rd_flag;	/* Remote disconnect flag = 0 */

	/*
	 * SMT MIB Operation Grp
	 */
	struct timeval	msg_ts;		/* msg time stamp */
	struct timeval	trans_ts;	/* transition time stamp */
	setcount_t	setcount;	/* setcount */
	SMT_STATIONID	last_sid;	/* last set station id */
	int		manufset;

	LFDDI_ADDR	sr_mid;

	SMT_FS_INFO	fss[13];

	u_long	cusion[40];
} SMTD;

extern int	nsmtd;
extern int	valid_nsmtd;
extern SMTD	*smtp;
extern int	smtd_debug;
extern char	*smtd_cfile;		/* config file name */
extern char	*smtd_mfile;		/* MIB file name */
extern char	*smtd_traphost;		/* traphost name */
extern struct	timeval curtime;	/* now, more or less */
extern LFDDI_ADDR	smtd_broad_mid;	/* broadcast addr */
extern LFDDI_ADDR	smtd_null_mid;	/* all-zero addr */

/* SNMP-daemon GLOBALS */
extern struct tree	*smtd_mib;	/* mib tree */
extern struct tree	*smtd_fdditree;	/* fddi subtree */
extern fd_set		smtd_fdset;	/* fdset */
extern int		smtd_cmd;	/* Cmd that is being work on */
extern int		smtd_rvalue;	/* Return val for INTEGER */
extern SMT_MAC		*smtd_config;	/* configuration */

#define mactosmt(m)	(&smtd[m->iid])
#define phytomac(p)	((SMT_MAC*)(p->mac))
#define phytosmt(p)	(&smtd[((SMT_MAC*)(p->mac))->iid])

extern void smtd_quit(void);
extern void smtd_setcx(int);
extern int smtd_setsid(SMT_MAC *);
extern void sr_timo(void);

#define SMT_NEVER	0x7fffffff	/* timeout that will never happen */

#define MAC_MID(x)	((x)-smtp->smt_phy_ct)
#define MAC_RID(x)	(smtp->smt_phy_ct+(x))
#define PHY_MID(x)	((x))
#define PHY_RID(x)	((x))

#endif /* __SMTD_H__ */
