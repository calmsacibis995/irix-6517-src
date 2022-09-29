/* Point-to-Point Protocol
 */
#ifndef _SYS_IF_PPP_H
#define _SYS_IF_PPP_H
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.27 $"

/* packet codes */
#define PPP_CODE_CR	1		/* Configure-Request */
#define PPP_CODE_CA	2		/* Configure-Ack */
#define PPP_CODE_CN	3		/* Configure-Nak */
#define PPP_CODE_CONFJ	4		/* Configure-Reject */
#define PPP_CODE_TR	5		/* Terminate-Request */
#define PPP_CODE_TA	6		/* Terminate-Ack */
#define PPP_CODE_CJ	7		/* Code-Reject */
#define PPP_CODE_PJ	8		/* Protocol-Reject */
#define PPP_CODE_EREQ	9		/* Echo-Request */
#define PPP_CODE_EREP	10		/* Echo-Reply */
#define PPP_CODE_DIS	11		/* Discard-Request */
#define PPP_CODE_IDENT	12		/* Identification */
#define PPP_CODE_TIME	13		/* time remaining */
#define PPP_CODE_RREQ	14		/* (compression) reset request */
#define PPP_CODE_RACK	15		/* (compression) reset ACK */


#define MAX_PPP_DEVS	99		/* max total interfaces */
#define MAX_PPP_LINKS	4		/* max devices/interface */

#define MIN_VJ_SLOT 3
#define DEF_VJ_SLOT 16
#define MAX_VJ_SLOT 254

#define PPP_DEF_MRU	1500		/* default MRU */
#define PPP_MAX_MTU	(10*1024)	/* do not try  bigger packets */
#define PPP_MIN_MTU	64		/* require at least this much */
#define PPP_NONDEF_MRU	1529		/* common, non-default value */

#define PPP_MP_MAX_OVHD 5		/* worst case overhead for MP */

#define PPP_FLAG	0x7e		/* frame flag */
#define PPP_ADDR	0xff
#define PPP_CNTL	0x03

#define PPP_ESC		0x7d		/* Control Escape */
#define PPP_ESC_BIT	0x20		/* toggle to escape control chars */

#define PPP_SET_ACCM(accm,c) ((accm)[(u_char)(c)/SFM] |= (1<<((c)%SFM)))

#define PPP_ACCM_RX_MAX	0x20
#define PPP_ASYNC_MAP(c,accm) (0!=(((accm)[(u_char)(c)/SFM]	\
				    >>(((u_char)c)%SFM)) & 1))
#define PPP_ACCM_DEF	0xffffffff

/* protocol numbers */
#define	PPP_IP		0x0021		/* IP */
#define PPP_IPXCP	0x002b		/* IPX */
#define PPP_VJC_COMP	0x002d		/* Van Jacobson Compressed TCP/IP */
#define	PPP_VJC_UNCOMP	0x002f		/* Van Jacobson Uncompressed TCP/IP */
#define PPP_BP		0x0031		/* bridging */
#define PPP_MP		0x003d		/* multilink */
#define PPP_BAP		0x0071		/* BAP Bandwidth Allocation Protocol */
#define PPP_CP		0x00fd		/* compressed packets */
#define PPP_CP_MULTI	0x00fb		/* compression below multi-link */

#define	PPP_IPCP	0x8021		/* IP Control Protocol */
#define PPP_IPXP	0x802b		/* IPX */
#define PPP_BCP		0x8031		/* bridging */
#define PPP_BACP	0x8071		/* BACP Bandwidth Allocation Control */
#define PPP_CCP		0x80fd		/* Compression Control Protocol */
#define PPP_CCP_MULTI	0x80fb		/* compression below multi-link */

#define PPP_LCP		0xc021		/* Link Control Protocol */
#define PPP_PAP		0xc023		/* Password Authentication Protocol */
#define	PPP_LQR		0xc025		/* Link Quality Report */
#define PPP_CHAP	0xc223		/* Challenge Handshake Auth Proto */

#define MIN_BSD_COMP_PROTO  0		/* compress these protocols with */
#define MAX_BSD_COMP_PROTO  0x3fff	/*	BSD Compress */

/* Endpoint Discriminator types */
#define LCP_CO_ENDPOINT_NULL	0
#define LCP_CO_ENDPOINT_LOC	1
#define LCP_CO_ENDPOINT_IP	2
#define LCP_CO_ENDPOINT_MAC	3
#define LCP_CO_ENDPOINT_MAGIC	4
#define LCP_CO_ENDPOINT_PHONE	5
#define LCP_MAX_CO_ENDPOINT LCP_CO_ENDPOINT_PHONE

#define PPP_IDENT_LEN 128		/* RFC 1570 identification  length */
struct ppp_ident {
    __uint32_t  magic;
    u_char	msg[PPP_IDENT_LEN-sizeof(__uint32_t)];
};


#define PPP_FM  __uint32_t		/* type of filter bit mask */
#define SFM (sizeof(PPP_FM)*8)		/* size of filter bit mask in bits */

/* port activity filter
 * 1=>do not worry about this packet
 */
struct afilter {
    PPP_FM	port[0x10000/sizeof(PPP_FM)];
    PPP_FM	icmp[256/sizeof(PPP_FM)];
};
#define IGNORING_PORT(f,p) ((f) != 0 && (((f)->port[(p)/SFM]>>((p)%SFM)) & 1))
#define IGNORING_ICMP(f,t) ((f) != 0 && (((f)->icmp[(t)/SFM]>>((t)%SFM)) & 1))
#define SET_PORT(f,p)	    ((f)->port[(p)/SFM] |= ((PPP_FM)1<<((p)%SFM)))
#define SET_ICMP(f,t)	    ((f)->icmp[(t)/SFM] |= ((PPP_FM)1<<((t)%SFM)))


/* PPP "Predictor" compression */
struct pred_db {
    struct pred_db_info {
	u_short	    hash;
	u_short	    mru;
	char	    debug;
	u_char	    unit;
	__uint32_t  uncomp;		/* decompressed bytes */
	__uint32_t  comp;		/* compressed bytes */
	__uint32_t  incomp;		/* incompressible packets */
	__uint32_t  padding;		/* bytes of padding added by peer */
    } db_i;
    u_char	tbl[65536];
};
#define PRED_OVHD (4+2)			/* Predictor 1 overhead/packet */
#ifdef _KERNEL
extern struct pred_db *pf_pred1_init(struct pred_db*,int,int);
extern int pf_pred1_comp(struct pred_db*, u_char*,int,struct mbuf*,int,int);
extern mblk_t* pf_pred1_decomp(struct pred_db*, mblk_t*);
#endif


/* PPP "BSD Compress" */
struct bsd_db {
    __uint32_t	totlen;			/* length of this structure */
    __uint32_t	hsize;			/* size of the hash table */
    u_char	hshift;			/* used in hash function */
    u_char	n_bits;			/* current bits/code */
    u_char	debug;
    u_char	unit;
    u_short	mru;
    u_short	seqno;			/* # of last byte of packet */
    __uint32_t	maxmaxcode;		/* largest valid code */
    __uint32_t	max_ent;		/* largest code in use */
    __uint32_t	in_count;		/* uncompressed bytes */
    __uint32_t	bytes_out;		/* compressed bytes */
    __uint32_t	prev_in_count;
    __uint32_t	prev_bytes_out;
    __uint32_t	ratio;			/* recent compression ratio */
    __uint32_t	checkpoint;		/* when to next check the ratio */
    __int32_t	clear_count;		/* times dictionary cleared */
    __int32_t	incomp_count;		/* incompressible packets */
    __int32_t	decomp_count;		/* packets decompressed */
    __int32_t	overshoot;		/* excess decompression buf */
    __int32_t	undershoot;		/* insufficient decomp. buf */
    u_short	*lens;			/* array of lengths of codes */
    struct bsd_dict {
	union {				/* hash value */
	    __uint32_t	fcode;
	    struct {
#ifdef BSD_LITTLE_ENDIAN
		u_short prefix;		/* preceding code */
		u_char  suffix;		/* last character of new code */
		u_char	pad;
#else
		u_char	pad;
		u_char  suffix;		/* last character of new code */
		u_short prefix;		/* preceding code */
#endif
	    } hs;
	} f;
	u_short codem1;			/* output of hash table -1 */
	u_short cptr;			/* map code to hash table entry */
    } dict[1];
};
#define BSD_COMP_RATIO_SCALE_LOG    8
#define BSD_COMP_RATIO_SCALE	(1<<BSD_COMP_RATIO_SCALE_LOG)
#define BSD_COMP_RATIO_MAX	(0x7fffffff>>BSD_COMP_RATIO_SCALE_LOG)
#define BSD_OVHD (2+2)			/* BSD compress overhead/packet */
#define MIN_BSD_BITS	9
#define MAX_BSD_BITS	15		/* implementation limit */
#define BSD_VERS	1		/* when shifted */
#ifdef _KERNEL
extern struct bsd_db *pf_bsd_init(struct bsd_db*, int, int, int);
extern int pf_bsd_comp(struct bsd_db*,u_char*,int,struct mbuf*,int);
extern mblk_t* pf_bsd_decomp(struct bsd_db*, mblk_t*);
extern void pf_bsd_incomp(struct bsd_db*, mblk_t*, u_int);
#endif


/* All int's in the kernel-user interface are 32-bits long to avoid
 * translation problems, so that a 32-bit daemon can be used on
 * all systems.
 */
#define IF_PPP_VERSION 7		/* change when the interface does */
struct ppp_arg {
    __uint32_t	dev_index;
    __uint32_t	version;		/* version of kernel modules */
    union {
	u_char	ok;
	struct {
	    __uint32_t	ip_mtu;		/* what to tell IP */
	    __uint32_t	mtru;		/* gross MTU for PPP */
	    __uint32_t	qmax;
	    __uint32_t  bigxmit;
	    u_char	telnettos;
	} mtu;
	struct ppp_arg_lcp {
#	    define  PPP_ACCM_LEN (256/SFM)
	    PPP_FM  tx_accm[PPP_ACCM_LEN];
	    PPP_FM  rx_accm;
	    u_char  bits;
#		define SIOC_PPP_LCP_ACOMP   0x01    /* compress A field */
#		define SIOC_PPP_LCP_PCOMP   0x02    /* compress protocol */
#		define SIOC_PPP_LCP_SYNC    0x04    /* SYNC link */
	} lcp;
	struct {
	    u_char  tx_comp;
	    u_char  tx_compslot;
	    u_char  tx_slots;
	    u_char  rx_slots;
	    u_char  link_num;
	} vj;
	struct {
	    u_char  proto;
#		define	SIOC_PPP_CCP_PRED1  0x01    /* Predictor type 1 */
#		define	SIOC_PPP_CCP_BSD    0x02    /* BSD compress */
#		define	SIOC_PPP_CCP_PROTOS (SIOC_PPP_CCP_PRED1	\
					     | SIOC_PPP_CCP_BSD)
	    u_char  debug;
	    u_char  bits;
	    u_short mru;
	    u_char  unit;
	} ccp;
	struct {
	    u_char  id;
	} ccp_arm;
	struct ppp_arg_mp {
	    __uint32_t	max_frag;	/* per-link MTU */
	    __uint32_t	min_frag;	/* 0 or minimum frag size for bundle */
	    __uint32_t	reasm_window;	/* limit on unassembled fragments */
	    u_char	on;		/* 1=turn on MP */
	    u_char	send_ssn;	/* 1=send short sequence numbers */
	    u_char	recv_ssn;	/* 1=receive short sequence numbers */
	    u_char	must_use_mp;	/* 1=must use MP headers */
	    u_char	mp_nulls;	/* 1=ok to use MP null fragmentss */
	    u_char	debug;
	} mp;
    } v;
};



#define BEEP		(HZ*5)		/* poke daemon this often */

/* shape of a PPP packet */
struct ppp_buf {
    __uint32_t	type;
    short	dev_index;
    u_char	bits;
    u_char	frame;			/* last of PPP_BUF_HEAD bytes */
    u_char	addr;
    u_char	cntl;
    u_short	proto;
    union {
	u_char	    info[PPP_MAX_MTU];
	__uint32_t  l_info[(PPP_MAX_MTU+3)/4];
	struct	{			/* ordinary packet */
	    u_char	code;
	    u_char	id;
	    u_short	len;
	} pkt;
	struct mp_l {			/* MP header with long sequence # */
	    u_int	mp_bits:2;
	    int		rsvd:6;
	    u_int	sn:24;
	} mp_l;
	struct mp_s {			/* MP header with short sequence # */
	    u_int	mp_bits:2;
	    int		rsvd:2;
	    u_int	sn:12;
	} mp_s;
#	define MP_B_BIT	0x02		/* 1st MP fragment */
#	define MP_E_BIT 0x01		/* last MP fragment */
#	define MP_BIT_SHIFT 6
#	define MP_LSN_MASK 0xffffff
#	define MP_SSN_MASK 0xfff
#	define MP_MAX_SN (0x40000000)	/* wrap seq # at this */
#	define MP_TRIM_SN (MP_MAX_SN/2)	/* trim by this at wrap */
	struct beep {			/* message from the driver */
	    __uint32_t  st;
#	     define BEEP_ST_RX_ACT  1
#	     define BEEP_ST_RX_BUSY 2
#	     define BEEP_ST_TX_ACT  4
#	     define BEEP_ST_TX_BUSY 8
	    time_t	tstamp;
	    __uint32_t	raw_ibytes;
	    __uint32_t	raw_obytes;
	} beep;
    } un;
};
#define PPP_BUF_HEAD 8			/* bytes before link-layer packet */
#define PPP_BUF_HEAD_FRAME (PPP_BUF_HEAD-1) /* bytes before framing byte */
#define PPP_BUF_HEAD_PROTO (PPP_BUF_HEAD+2) /* bytes before before protocol */
#define PPP_BUF_HEAD_INFO (PPP_BUF_HEAD+4)  /* bytes before good stuff */
#define PPP_BUF_TAIL 3			/* bytes after info, FCS and frame */
#define PPP_BUF_MAX sizeof(struct ppp_buf)
#define PPP_BUF_MIN (PPP_BUF_MAX-PPP_MAX_MTU)
#define PPP_BUF_BEEP (PPP_BUF_MIN+sizeof(pb->un.beep))

#define PPP_BUF_ALIGN(p) (0 == ((__psunsigned_t)(p) % sizeof(__uint32_t)))

/* bytes before protocol in MP packet */
#define MB_BUF_HEAD_L  (PPP_BUF_HEAD_INFO+4)
#define MB_BUF_HEAD_S  (PPP_BUF_HEAD_INFO+2)
#define MP_BUF_HEAD(ssn) ((ssn) ? MB_BUF_HEAD_S : MB_BUF_HEAD_L)

/* values for ppp_buf.type */
enum ppp_buf_type {
    BEEP_ACTIVE	=1,			/* looks like high activity */
    BEEP_WEAK	=2,			/* the streams module says "Hi" */
    BEEP_DEAD	=3,			/* the modem is dead */
    BEEP_CP_BAD	=4,			/* compression has gone off */
    BEEP_FRAME	=5,			/* a PPP frame */
    BEEP_FCS	=6,			/* complaint about bad FCS */
    BEEP_ABORT	=7,			/* complaint about aborted frame */
    BEEP_TINY	=8,			/* complaint about truncated frame */
    BEEP_BABBLE	=9,			/* complaint about bloated frame */
    BEEP_MP	=10,			/* MP sequence number */
    BEEP_MP_ERROR=11			/* bad MP header */
};


#define PPP_FCS_INIT	0xffff		/* Initial FCS value */
#define PPP_FCS_GOOD	0xf0b8		/* Good final FCS value */

/* add to the FCS */
#define PPP_FCS(fcs,c) ((((u_short)(fcs))>>8)	\
			^ ppp_fcstab[((fcs) ^ (c)) & 0xff])
extern u_short ppp_fcstab[256];

#define PPP_MAX_SBUF MIN(PPP_BUF_MAX, STR_MAXBSIZE)

/*
 * everything there is to know about each PPP framer
 */
struct ppp_pf {
    struct	ppp_pf *pf_next;
    queue_t	*pf_rq;			/* our queues */
    queue_t	*pf_wq;
    int		pf_index;		/* STREAMS mux index */

    PPP_FM	pf_accm;		/* Async Control Character Map */

    mblk_t	*pf_head;		/* growing packet */
    mblk_t	*pf_tail;
    int		pf_type;

    int		pf_len;
    u_short	pf_fcs;

    u_short	pf_state;		/* miscellaneous state bits */
#    define PF_ST_SYNC	    0x0001	/* sync framing */
#    define PF_ST_HEAD_RDY  0x0002	/* pf_head is complete */
#    define PF_ST_OFF_RX    0x0004	/* stop passing frames upstream */
#    define PF_ST_STUTTER   0x0008	/* one frame at a time */
#    define PF_ST_ESC	    0x0010	/* waiting for 2nd byte	*/
#    define PF_ST_GAP	    0x0020	/* in inter-frame gap */
#    define PF_ST_RX_PRED   0x0040	/* Predictor 1 decompression */
#    define PF_ST_RX_BSD    0x0080	/* BSD (de)Compress(ion) */
#    define PF_ST_MP	    0x0100	/* MP on */

#    define PF_ST_CP	(PF_ST_RX_BSD  | PF_ST_RX_PRED)

    u_char	pf_ccp_id;		/* expected CCP ID */

    struct bsd_db *pf_bsd;		/* "BSD compress" compression */
    struct pred_db *pf_pred;		/* "Predictor" compression */

    struct pf_meters {
	__uint32_t  raw_ibytes;
	__uint32_t  nobuf;		/* no input buffer available */
	__uint32_t  null;		/* null frames */
	__uint32_t  tiny;		/* invalidly small frames */
	__uint32_t  abort;		/* aborted frames */
	__uint32_t  long_frame;
	__uint32_t  babble;
	__uint32_t  bad_fcs;
	__uint32_t  cp_rx;		/* total compressed packets */
	__uint32_t  cp_rx_bad;		/* bad compressed packets */
    } pf_meters;
};


/* per-link PPP driver info known in user-space */
struct ppp_link {
    __int32_t	pl_index;		/* STREAMS mux index */
    __int32_t	pl_link_num;		/* multilink number */

    __int32_t	pl_mp_sn;		/* max sequence # seen on this link */
    __uint32_t	pl_max_frag;		/* max MP data bytes/frag for link */

    __uint32_t	pl_state;
#    define PL_ST_TX_PRED   0x000001    /* Predictor Type 1 compression */
#    define PL_ST_TX_BSD    0x000002    /* BSD compress compression */
#    define PL_ST_TX_IP	    0x000004	/* IP output ok */
#    define PL_ST_TX_LINK   0x000008    /* output on link ok */
#    define PL_ST_BB_TX	    0x000010	/* delay TX */
#    define PL_ST_SYNC	    0x000020	/* sync. framing */
#    define PL_ST_ACOMP	    0x000040	/* compress LCP address & control */
#    define PL_ST_PCOMP	    0x000080	/* compress LCP protocol */
#    define PL_ST_NULL_NEED 0x000100	/* need an MP null fragment */

    __uint32_t	pl_rx_pings;		/* ISDN driver said it is busy */
#    define PPP_RX_PING_TICKS 100	/* ISDN driver fires this often */

    __uint32_t  pl_raw_obytes;
};


/* per-bundle PPP driver info known in user-space */
struct ppp_bundle {
    __int32_t	pb_pl_num;		/* number of devices under MUX */

    struct {
	__int32_t   out_head;		/* !=0 to use MP headers */
	__uint32_t  min_frag;		/* target fragment size */
	__int32_t   sn_out;		/* next output sequence number */
	__int32_t   num_frags;		/* input fragments waiting */
	__int32_t   reasm_window;
	__int32_t   sn_in;		/* reassemble through this */
#	 define	MP_FLUSH_TIME (10*HZ)
#	 define	MP_FLUSH_LEN (MP_FLUSH_TIME/BEEP + 1)
	__int32_t   sn_flush[MP_FLUSH_LEN];
	__uint32_t  debug;
    } pb_mp;

    __int32_t	pb_rx_ping;		/* ISDN driver said it is busy */

    __int32_t	pb_bigxmit;		/* bigger packets gets slow service */
    __int32_t	pb_bigpersmall;		/* ratio of big to small packets */
    __int32_t	pb_starve;		/* small packets leapfrogged */
    u_char	pb_telnettos;

    u_char	pb_ccp_id;		/* recent CCP packet ID */

    __int32_t	pb_mtru;		/* PPP MTU of entire bundle */

    __uint32_t  pb_state;		/* misc. state bits */
#    define PB_ST_TX_PRED	0x000001    /* Predictor Type 1 compression */
#    define PB_ST_TX_BSD	0x000002    /* BSD compress compression */
#    define PB_ST_RX_PRED	0x000004    /* input compression */
#    define PB_ST_RX_BSD	0x000008
#    define PB_ST_ON_RX_CCP	0x000010    /* pass all input frames */
#    define PB_ST_BEEP		0x000020    /* tell driver things are awake */
#    define PB_ST_MP_TIME	0x000040    /* MP needs timer */
#    define PB_ST_BEEP_TIME	0x000080    /* timeout pending */
#    define PB_ST_TX_BUSY	0x000100    /* have been busy */
#    define PB_ST_TX_DONE	0x000200    /* caught up */
#    define PB_ST_VJ		0x000400    /* send VJ compressed packets */
#    define PB_ST_VJ_SLOT	0x000800    /* send VJ slot compressed */
#    define PB_ST_MP		0x001000    /* MP on */
#    define PB_ST_MP_OPT_HEAD	0x002000    /* no MP headers if unneeded */
#    define PB_ST_MP_SEND_SSN	0x004000    /* send short MP sequence #s */
#    define PB_ST_MP_RECV_SSN	0x008000    /* receive short sequence #s */
#    define PB_ST_MP_NULLS	0x010000    /* use MP null fragments */
#    define PB_ST_NEED_NULLS	0x020000    /* have sent a fragment */
#    define PB_ST_NOICMP	0x040000    /* suppress ICMP */
#    define PB_ST_RX_CP (PB_ST_RX_PRED | PB_ST_RX_BSD)
#if PL_ST_TX_PRED != PB_ST_TX_PRED || PL_ST_TX_BSD != PB_ST_TX_BSD
    ? "PL_ST_TX_PRED, PB_ST_TX_PRED, PL_ST_TX_BSD,  PB_ST_TX_BSD differ";
#endif
#    define ST_TX_PRED PB_ST_TX_PRED
#    define ST_TX_BSD  PB_ST_TX_BSD
#    define ST_TX_CP (ST_TX_PRED | ST_TX_BSD)

    struct pb_meters {
	__uint32_t  raw_ibytes;
	__uint32_t  ibytes;
	__uint32_t  raw_obytes;
	__uint32_t  obytes;
	__uint32_t  move2f;		/* small packets moved ahead of big */
	__uint32_t  move2f_leak;	/* big packets leaked */
	__uint32_t  mp_frags;		/* total fragments seen */
	__uint32_t  mp_null;		/* null MP packets */
	__uint32_t  mp_no_end;		/* missing end fragment */
	__uint32_t  mp_stale;		/* arrived too late */
	__uint32_t  mp_no_bits;		/* missing MP fragments */
	__uint32_t  mp_stray;
	__uint32_t  mp_alloc_fails;	/* failed to get needed STREAMS buf */
	__uint32_t  mp_nested;		/* MP packet inside MP */
	__uint32_t  cp_rx;		/* input decompressions */
	__uint32_t  cp_rx_bad;		/* input decompressions that failed */
	__uint32_t  mp_pullup;
	__uint32_t  rput_pullup;
    } pb_meters;
};


/* returned by SIOC_PPP_INFO */
struct ppp_info {
    __int32_t	    pi_version;
#    define	PI_VERSION 2
    __int32_t	    pi_len;		/* sizeof(struct ppp_info) */
    struct ppp_bundle pi_bundle;
    struct {
	__int32_t   tx_states;		/* # of TX SLOTS */
	__int32_t   actconn;		/* apparent active TCP connections */
	__int32_t   last;		/* last sent slot id */
	__int32_t   cnt_packets;	/* outbound packets */
	__int32_t   cnt_compressed;	/* outbound compressed packets */
	__int32_t   cnt_searches;	/* searches for connection state */
	__int32_t   cnt_misses;		/* could not find state */
    } pi_vj_tx;
    struct {
	struct ppp_link	dev_link;
	u_short	    dev_pf_state;
	struct pf_meters dev_meters;
	/* only when in use */
	struct {
	    __int32_t	rx_states;
	    __int32_t	last;		/* last rcvd slot ID */
	    __int32_t	cnt_uncompressed;   /* uncompressed packets */
	    __int32_t	cnt_compressed; /* compressed packets */
	    __int32_t	cnt_tossed;	/* tossed for error */
	    __int32_t	cnt_error;	/* unknown type	packets */
	} dev_vj_rx;
	union {
	    struct bsd_db bsd;
	    struct pred_db_info pred;
	} dev_tx_db;
	union {
	    struct bsd_db bsd;
	    struct pred_db_info pred;
	} dev_rx_db;
    } pi_devs[MAX_PPP_LINKS];
    __int32_t	pad[128];		/* in case of changes */
};


/* These IOCTLs do not currently work via sockets.  They can be used only
 * via the STREAMS side.
 */
#ifdef _KERNEL
#define SIOC_PPP_AFILTER _IOWR('P',1, __uint32_t)   /* address filter */
#else
#if _MIPS_SIM == _ABI64
/* this interface is only for 32-bit programs */
#else
#define SIOC_PPP_AFILTER _IOWR('P',1, struct afilter*)
#endif
#endif /* KERNEL */
#define SIOC_PPP_NOICMP	_IOWR('P',2, struct ppp_arg)	/* suppress ICMP */
#define SIOC_PPP_MTU	_IOWR('P',3, struct ppp_arg)	/* set MTU */
#define SIOC_PPP_LCP	_IOWR('P',4, struct ppp_arg)	/* LCP parameters */
#define SIOC_PPP_VJ	_IOWR('P',5, struct ppp_arg)	/* set VJ comp */
#define SIOC_PPP_RX_OK	_IOWR('P',6, struct ppp_arg)	/* allow link IP rx */
#define SIOC_PPP_TX_OK	_IOWR('P',7, struct ppp_arg)	/* allow link IP tx */
#define SIOC_PPP_1_RX	_IOWR('P',8, struct ppp_arg)	/* allow 1 rx frame */
#define SIOC_PPP_CCP_RX	_IOWR('P',9, struct ppp_arg)	/* set rx CCP */
#define SIOC_PPP_CCP_TX	_IOWR('P',10,struct ppp_arg)	/* set tx CCP */
#define SIOC_PPP_CCP_ARM_CA _IOWR('P',12,struct ppp_arg)
#   define PPP_CCP_DISARM_ID 0
#define SIOC_PPP_IP_TX_OK _IOWR('P',13,struct ppp_arg)	/* enable IP TX */
#define SIOC_PPP_MP	_IOWR('P',14,struct ppp_arg)	/* IETF MP on or off */
#ifdef _KERNEL
#define SIOC_PPP_INFO	_IOWR('P',15,__uint32_t)    /* what's happening */
#else
#if _MIPS_SIM == _ABI64
/* this interface is only for 32-bit programs */
#else
#define SIOC_PPP_INFO	_IOWR('P',15,struct ppp_info*)
#endif
#endif /* KERNEL */

#ifdef _KERNEL
extern int pf_sioc_rx_init(struct iocblk*, struct ppp_arg*,
			   struct pred_db**, struct bsd_db**, int, int);
#endif


#ifdef __cplusplus
}
#endif
#endif
