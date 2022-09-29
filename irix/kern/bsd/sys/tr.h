#ifndef __TR_H
#define __TR_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL
#include "net/if.h"
#include "net/raw.h"
#include "netinet/if_ether.h"
#endif  /* _KERNEL */

#define TRMTU_4M		4472	/*  4Mbps MAC info size */
#define TRMTU_16M		17800	/* 16Mbps MAC info size */

/*
 * Source Routing.
 */
#define TR_MAC_RIF_SIZE		18	/* mac routing info field size */
#define	TR_RIF_NON_BROADCAST	0x0000		/* Non-broadcast: follow route*/
#define	TR_RIF_ALL_ROUTES	0x8000		/* All-routes broadcast,
						   Non-broadcast return */
#define	TR_RIF_SRB_ALBR		0xc000		/* Single-route broadcast,
						   All-routes broadcast rtn*/
#define	TR_RIF_SRB_NBR		0xe000		/* Single-route broadcast,
						   Non-broadcast return */
#define TR_RIF_LENGTH		0x1f00		/* length = 30? */
#define TR_RIF_DIRECTION	0x0080		/* direction of frame */
						/* Longest frame flags */
#define	TR_RIF_LF_516		0x0000
#define	TR_RIF_LF_1470		0x0010
#define	TR_RIF_LF_2052		0x0020
#define	TR_RIF_LF_4472		0x0030
#define	TR_RIF_LF_8144		0x0040
#define	TR_RIF_LF_11407		0x0050
#define	TR_RIF_LF_17800		0x0060
#define	TR_RIF_LF_INITIAL	0x0070
#define TR_RIF_LF_MASK		0x0070
typedef struct tr_rii{
	u_short		rii;			/* routing information */
	u_char		sgt_nos[TR_MAC_RIF_SIZE - sizeof(u_short)];
} TR_RII;

/*
 * Access Control
 */
#define TR_AC_PRI		0xe0
#define	TR_AC_PRI_IP			0x60
#define	TR_MAC_AC_PRIORITY		TR_AC_PRI_IP
#define TR_AC_TOK_FRAME		0x10
#define TR_AC_TOK			0x00
#define TR_AC_FRAME			0x10
#define TR_AC_MONITOR		0x08
#define TR_AC_PRIRES		0x07

/*
 * Frame Control
 */
#define TR_FC_TYPE		0xc0
#define TR_FC_TYPE_MAC			0x00
#define TR_FC_TYPE_LLC			0x40
#define	TR_MAC_FC_NON_MAC_CTRL		TR_FC_TYPE_LLC
#define TR_FC_PCF		0x0f
#define TR_FC_PCF_EXP		0x01
#define TR_FC_PCF_REMOV		0x01
#define TR_FC_PCF_DUPA		0x01
#define TR_FC_PCF_BCN		0x02
#define TR_FC_PCF_CLM		0x03
#define TR_FC_PCF_PURGE		0x04
#define TR_FC_PCF_AMP		0x05
#define TR_FC_PCF_SMP		0x06

/*
 * Frame status(FS)
 */
#define	TR_FS_ARI_1		0x0080	/* first address recognized bit */
#define	TR_FS_ARI_2		0x0008	/* second address recognized bit */
#define	TR_FS_FCI_1		0x0040	/* first frame copied bit */
#define	TR_FS_FCI_2		0x0004	/* second frame copied bit */

#define TR_MAC_ADDR_SIZE	6
#define TR_LR_SIZE		2
#define TR_PL_SIZE		(TR_MAC_ADDR_SIZE-TR_LR_SIZE)
#define	TR_GROUP_ADDRESS	0x81
#define TR_MACADR_RII		0x80

#define TR_4Mb_FRAME_SIZE	4519	/* 4 Mbit max MAC frame size */
#define TR_16Mb_FRAME_SIZE	18015	/* 16 Mbit max MAC frame size */

typedef struct {unchar b[TR_MAC_ADDR_SIZE];} TR_ADDR;
typedef struct tr_mac {
	u_char		mac_ac;		/* access control */
	u_char		mac_fc;		/* frame control */
	u_char		mac_da[TR_MAC_ADDR_SIZE];
	u_char		mac_sa[TR_MAC_ADDR_SIZE];
} TR_MAC;

/* Group address */
typedef struct {
	u_short a[2];
} TR_FCNADDR;
#define TR_GRPADDR	TR_FCNADDR

/*
 * MAC Major vector
 */
typedef struct tr_macmv {
	u_short len;
#define TR_DST_CLASS	0xf0
#define TR_SRC_CLASS	0x0f
#define TR_CLASS_STA	0	/* station */
#define TR_CLASS_MGR	1	/* lan manager */
#define TR_CLASS_CRS	4	/* Config Report Server */
#define TR_CLASS_RPS	5	/* Ring Param Server */
#define TR_CLASS_REM	6	/* Ring Error Monitor */
	u_char vclass;
#define TR_MVC_RESPONSE	0x00
#define TR_MVC_BEACON	0x02
#define TR_MVC_CLAIM	0x03
#define TR_MVC_PURGE	0x04
#define TR_MVC_AMP	0x05	/* Active Monitor Present */
#define TR_MVC_SMP	0x06	/* Stanby Monitor Present */
#define TR_MVC_DAT	0x07	/* Dup Addr Test */
#define TR_MVC_LOBE	0x08	/* Lobe media Test */
#define TR_MVC_FORWARD	0x09	/* Xmit forward */
#define TR_MVC_REMOVE	0x0b
#define TR_MVC_CHANGE	0x0c
#define TR_MVC_INIT	0x0d
#define TR_MVC_REQADDR	0x0e
#define TR_MVC_REQSTATE	0x0f
#define TR_MVC_REQATACH	0x10
#define TR_MVC_REQINIT	0x20	/* Request init parameters */
#define TR_MVC_RESADDR	0x22
#define TR_MVC_RESSTATE	0x23
#define TR_MVC_RESATACH	0x24
#define TR_MVC_REPNAM	0x25	/* Report New Active Monitor */
#define TR_MVC_REPUNA	0x26
#define TR_MVC_REPNNI	0x27	/* Report Nbr Notify Incomplete */
#define TR_MVC_REPAME	0x28	/* Report Active Monitor Error */
#define TR_MVC_REPSE	0x29	/* Report Soft Error */
#define TR_MVC_REPFRWD	0x2a	/* Report xmit forward */
	u_char cmd;
} TR_MACMV;

typedef struct tr_iec {
	u_char line_ct;
	u_char internal_ct;
	u_char burst_ct;
	u_char ac_ct;
	u_char abort_ct;
	u_char reserved;
} TR_IEC;

typedef struct tr_niec {
	u_char lost_frame_ct;
	u_char rcv_congest_ct;
	u_char frm_copyerr_ct;
	u_char frequency_ct;
	u_char token_ct;
	u_char reserved;
} TR_NIEC;

typedef struct tr_prodid {
#define IBMHW           0x1
#define IBMORNONIBMHW   0x3
#define IBMSW           0x4
#define NONIBMHW        0x9
#define NONIBMSW        0xc
#define IBMORNONIBMSW   0xe
	u_char pclass;

#define PID_MACHINE             0x10
#define PID_MACHINE_MODEL       0x11
#define PID_MACHINE2            0x12
	u_char format;
	u_char mtype[3];
	u_char model[3];
	u_char serial[2];
	u_char sequence[7];
} TR_PRODID;

/* response code */
#define TR_RC_ACK	0x0001	/* positive ack */
#define TR_RC_MV	0x8001	/* missing MV */
#define TR_RC_MVLEN	0x8002	/* bad MV length */
#define TR_RC_MVID	0x8003	/* bad MV CMD */
#define TR_RC_SCLASS	0x8004	/* bad src class */
#define TR_RC_SVLEN	0x8005	/* bad SV length */
#define TR_RC_XF	0x8006	/* bad Transmit forward */
#define TR_RC_SVMISS	0x8007	/* missing required SV */
#define TR_RC_SVID	0x8008	/* unknown SV */
#define TR_RC_BSIZE	0x8009	/* MAC frame exceeds buffer size */
#define TR_RC_DISABLE	0x800a	/* Function disabled */

typedef struct tr_macsv1 {
	u_char len;
#define TR_SVC_BCNTYPE	0x01
#define		TR_BCNTYPE_RMODE	0x0001
#define		TR_BCNTYPE_SIGLOSS	0x0002
#define		TR_BCNTYPE_NOTCLAIM	0x0003
#define		TR_BCNTYPE_CLAIM	0x0004
#define TR_SVC_UNA	0x02
#define TR_SVC_RINGNUM	0x03
#define TR_SVC_ASIGNLOC	0x04	/* Asign physical location */
#define TR_SVC_SETIMO	0x05	/* Soft Error report time out */
#define TR_SVC_EFC	0x06	/* enabled function classes */
#define TR_SVC_AAP	0x07	/* Allowed Access Priority */
#define TR_SVC_COREL	0x09	/* corelator */
#define TR_SVC_LASTUNA	0x0a	/* indicates last NN, before NN fails */
#define TR_SVC_PHYLOC	0x0b	/* physical location */
#define TR_SVC_RESPONSE	0x20
#define TR_SVC_RESERVE	0x21
#define TR_SVC_PID	0x22	/* Product ID */
#define TR_SVC_VERS	0x23	/* micro code level */
#define TR_SVC_WRAPDATA	0x26
#define TR_SVC_FRMFWD	0x27
#define TR_SVC_SID	0x28	/* Station ID */
#define TR_SVC_STATUS	0x29	/* Station Status */
#define TR_SVC_XMITSTAT	0x2a
#define TR_SVC_GRPADDR	0x2b
#define TR_SVC_FCNADDR	0x2c
#define TR_SVC_IERRCNT	0x2d	/* Isolating Error Count */
#define TR_SVC_ERRCNT	0x2e
#define TR_SVC_ERRCODE	0x30
#define		TR_ECODE_MONITOR	0x0001
#define		TR_ECODE_DUPMON		0x0002
#define		TR_ECODE_DUPADDR	0x0003
	u_char svid;
} TR_MACSV1;

typedef struct tr_macsv2 {
	u_char len;
	u_char svid;
	u_short len2;
} TR_MACSV2;

typedef struct tr_status {
	/* From RING STATUS interrupts */
	u_int	signal_loss;		/* # of signal losses */
	u_int	hard_error;		/* # of hard errors */
	u_int	soft_error;		/* # of soft errors */
	u_int	lobe_wire_fault;	/* # of lobe wire fault errors */
	u_int	auto_removal_err;	/* # of auto removal errors */
	u_int	remove_rcv;		/* # of remove receive errors */
	u_int	ring_recovery;		/* # of ring recovery errors */
	u_int	beacons;		/* # of beacon frames received */

	/* from error log */
	u_int	line_err;		/* FCS (CRC) error, etc. */
	u_int	burst_err;		/* no transition for 5 half bits */
	u_int	ari_fci_err;		/* upstream neighbor can't set ari */
	u_int	lost_frame_err;		/* strip whole frame from ring failed */
	u_int	rcv_cong_err;		/* no space to copy in frame */
	u_int	frame_copied_err;	/* line hit or duplicate address */
	u_int	token_err;		/* token error (monitor node only) */
	u_int	dma_bus_errs;		/* DMA bus errors (up to init limit) */
	u_int	dma_parity_errs;	/* DMA parity errs (up to init limit)*/

	/* operational */
	u_int	single_station;		/* # of single station errors */
	u_int	cmd_rejects;		/* # of cmds rejected by tms380 */

	/*  frame xmit err */
	u_int	xmit_incmplt;		/* incomplete xmit frame */
	u_int  xmit_suspend;		/* suspended xmit commands */
	u_int  xmit_list;		/* xmit list error */
	u_int  xmit_listcnt;		/* data cnt list error */
	u_int  xmit_frmsiz;		/* frame size error */
	u_int  xmit_oddaddr;		/* odd fwd ptr */
	u_int  xmit_framgment;		/* bad start bit */
	u_int  xmit_priority;		/* bad access pri */
	u_int  xmit_class;		/* bad MAC class */
	u_int  xmit_format;		/* bad FC error */

	/*  frame recv err */
	u_int	rcv_incmplt;		/* incomplete receives */
	u_int	rcv_suspend;		/* suspended rcv commands */

/*
 * Firmware stat counts
 */
	u_int	misfrm;			/* R frm dropped due to no input buf */
	u_int	xmtabt;			/* X frm aborted due to no xmitlist */
	u_int	frame_ct;		/* R total frame count */
	u_int	byte_ct;		/* R bytes seen by board */

	u_int	e;			/* received E bit */
	u_int	a;			/* received A bit */
	u_int	c;			/* received C bit */

	u_int	bd_hiwat;		/* R BUSD hiwat */
	u_int	rq_hiwat;		/* R max lists in use */
	u_int	xq_hiwat;		/* X max lists in use */

	u_int	xmit_frm_ct;		/* X frame count */
	u_int	frm_cpy_ct;		/* R copied frame count */

	/* new */
	u_int	x_callouts;		/* R callouts */
	u_int	r_callouts;		/* X callouts */

	u_int	x_cpy_ct;		/* X copied frame count */
	u_int	x_byte_ct;		/* X bytes sent by board */

	u_int	x_bd_hiwat;		/* X BUSD hiwat */
	u_int	x_maxsize;		/* X max xmit frame size */
	u_int	r_maxsize;		/* R max recv frame size */
	u_int	r_maxlists;		/* R max lists per frame */
	u_int	x_maxlists;		/* X max lists per frame */
	u_int	x_maxfpc;		/* X max frames per callout */
	u_int	r_maxfpc;		/* R max frames per callout */

	u_int	isema;			/* Iframe semaphore */
	u_int	bad_xseq;		/* bad xmit sequence number */

	u_int	rsv_2;
	u_int	rsv_3;
	u_int	rsv_4;
	u_int	rsv_5;
} TR_STATUS;

typedef struct tr_config {
	u_char   caddr[TR_MAC_ADDR_SIZE];	/* current mac addr */
	u_char   bia[TR_MAC_ADDR_SIZE];
	u_char   gaddr[4];		/* group addr */
	u_char   faddr[4];		/* functional addr */
	u_int	 speed;
	u_int	 vers;

	/* TMS_RA_PARMS -- SHOULD NOT BE ALTERED */
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

	/* new config -- SHOULD NOT BE ALTERED */
	u_char	 mcl[10];		/* Micro Code Level */
	u_char	 pid[18];		/* Product ID */
	u_char   sid[4];		/* Station ID */
} TR_CONFIG;

/*
 * pointer to user buffers for counters or configuration
 */
typedef struct tr_sioc {
	union	{
		TR_CONFIG *conf;
		TR_STATUS *st;
		void *ptr;
		u_int alarm;
	} tr_ptr_u;
	uint	len;
} TR_SIOC;


/* used for passing firmware images */
struct mtr_download {
	unsigned short      sizeof_fmplus_array;
	unsigned short      recorded_size_fmplus_array;
	unsigned char       fmplus_checksum;
	__uint64_t fmplus_image;
};

/* special IOCTLs */
#define SIOC_TR_GETCONF		_IOW('S', 1, TR_SIOC)
#define SIOC_TR_SETCONF		_IOW('S', 2, TR_SIOC)
#define SIOC_TR_GETST		_IOW('S', 3, TR_SIOC)
#define SIOC_TR_SETST		_IOW('S', 4, TR_SIOC)
#define SIOC_TR_ARM		_IOWR('S',5, TR_SIOC)
#define SIOC_TR_GETMULTI	_IOR('S', 6, TR_GRPADDR)
#define SIOC_TR_ADDMULTI	_IOW('S', 7, TR_GRPADDR)
#define SIOC_TR_DELMULTI	_IOW('S', 8, TR_GRPADDR)
#define SIOC_TR_GETFUNC		_IOR('S', 9, TR_FCNADDR)
#define SIOC_TR_ADDFUNC		_IOW('S', 10, TR_FCNADDR)
#define SIOC_TR_DELFUNC		_IOW('S', 11, TR_FCNADDR)
#define SIOC_TR_GETLLCSTAT	_IOW('S', 12, TR_SIOC)
#define SIOC_TR_GETSPEED        _IOR('S', 13, struct ifreq)
#define SIOC_TR_SETSPEED        _IOW('S', 14, struct ifreq)
#define SIOC_TR_GETIFSTATS      _IOWR('S', 16, struct ifreq)
#define SIOC_MTR_GETFUNC        _IOWR('S', 17, struct ifreq)
#define SIOC_MTR_ADDFUNC        _IOWR('S', 18, struct ifreq)
#define SIOC_MTR_DELFUNC        _IOWR('S', 19, struct ifreq)

/*	Used for mtrconfig to pass down the paramters 
 *	to restart the adapter.
 */
typedef struct mtr_cfg_req_s {
        __uint32_t              cmd;
        union {
                __uint32_t      ring_speed;
                __uint32_t      mtu;
                uchar_t         macaddr[TR_MAC_ADDR_SIZE];
        } param;
} mtr_cfg_req_t;

/*      Values for mtr_cfg_req_t.cmd.   */
#define MTR_CFG_CMD_NULL                0
#define MTR_CFG_CMD_SET_SPEED           SIOC_TR_SETSPEED
#define MTR_CFG_CMD_SET_MTU             SIOCSIFMTU

#define MAX_CFG_PARAMS_PER_REQ          4
typedef struct mtr_restart_req_s {
        mtr_cfg_req_t           reqs[MAX_CFG_PARAMS_PER_REQ];
        struct mtr_download     dlrec;
} mtr_restart_req_t;
#define	SIOC_TR_RESTART		_IOWR('S', 15, mtr_restart_req_t)

/* alarm bits to awaken demon */
#define	TR_ALARM_RNGOP	0x00000001	/* ring died or recovered */
#define TR_ALARM_LEM	0x00000002	/* many link errors seen */
#define TR_ALARM_BS	0x00000004	/* stuck in BREAK */
#define TR_ALARM_CON_X	0x00000008	/* illegal connection topology */
#define TR_ALARM_CON_U	0x00000010	/* undesirable connection topology */
#define TR_ALARM_CON_W  0x00000020	/* connection withheld */
#define TR_ALARM_TFIN	0x00000040	/* PC-Trace finished */
#define TR_ALARM_TBEG	0x00000080	/* PC-Trace started by neighbor */
#define TR_ALARM_SICK	0x00000100	/* the hardware is sick */

/* lenght of mbuf used to awaken smt deamon */
#define TR_LEN_ALARM	(sizeof(TOKENBUFHEAD)+2+sizeof(TR_MAC)+4)

/* "Drain port" numbers to select TR frames
 */
#define TR_PORT_SMT	1
#define TR_PORT_MAC	2
#define TR_PORT_IMP	3
#define TR_PORT_LLC	4		/* this will change with full DSAPs */
#define TR_PORT_TMP	99

#define TR_ISBROAD(addr)	ETHER_ISBROAD(addr)
#define TR_ISMULTI(addr) 	ETHER_ISMULTI(addr)

#ifdef _KERNEL
/* Common token ring info structure
 */
struct trif {
	struct arpcom	tif_arpcom;	/* ifnet and current address */
	char		tif_bia[TR_MAC_ADDR_SIZE]; /* Burn-In Address */
	struct rawif	tif_rawif;	/* raw domain interface */
};

#define ifptotif(ifp)	((struct trif *)(ifp))
#define tiftoifp(tif)	(&(tif)->tif_arpcom.ac_if)
#endif  /* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif /* __TR_H */
