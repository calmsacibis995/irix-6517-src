/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN0_IF_CL_H__
#define	__SYS_SN0L_IF_CL_H__

#define	IFCL_VERSION		1

#define	IFCL_XP_MSG_SZ		256			/* xp msg sz */
#define	IFCL_XP_MAX_MSG_NUM	(NBPP / IFCL_XP_MSG_SZ)	/* xp msg nm */

#define	CL_MAX_OUTQ		IFCL_XP_MAX_MSG_NUM	/* max o/p Q length */

#define	IF_CL_NAME		"cl"
#define	IFCL_DFLT_MTU		(56 << 10)

#define	IFCL_OMBUF_ALIGN	3		/* mbuf tail addr align mask */

/* IFCL_DEBUG -- for debugging purposes */

/* never want these defined for non debug kernel !!! */
#if defined(DEBUG) 
#define	IFCL_DEBUG		0

#if IFCL_DEBUG /* debug must be set before these should be set */

/* IFCL_DEBUG_HDR must be set for all partitions that are in a system for 
 * this debug flag to work.  If not, the headers sent will not be recognized
 * by other partitions and problems will occur.  So then the code will crash
 * or do other unpredictable things.  However, IFCL_DEBUG_HDR is broken
 * and always prints out a mismatch of checksums.  So only use this if you
 * are willing to look into fixing it first :) */
#define	IFCL_DEBUG_HDR		0		/* do checksums on headers */
#define	IFCL_LOG		0		/* log sends and receives */
#endif /* IFCL_DEBUG */
#endif /* DEBUG */

#define	IFCL_ADDR_SZ		6

/* ifcl header */
typedef struct ifcl_hdr_s {
    char		ich_fpttn[IFCL_ADDR_SZ];	/* from partition */
    char		ich_tpttn[IFCL_ADDR_SZ];	/* to partition */
    u_short		ich_type;			/* ETHERTYPE_IP or 
							 * ETHERTYPE_SG_RESV 
							 * for ctrl packet */
#if IFCL_DEBUG_HDR
    u_long		ich_cksum;			/* checksum */
    __psunsigned_t	ich_cookie;			/* slist index */
#endif /* IFCL_DEBUG_HDR */
} ifcl_hdr_t;

#define	IFCL_HDR_SZ	sizeof(ifcl_hdr_t)

#define	IFCLBUF_PAD	RAW_HDRPAD(sizeof(ifcl_hdr_t))
#define	IFCLBUF_HEAD	(sizeof(ifclbufhead_t) - sizeof(ifcl_hdr_t))


/* actually ifcl header that goes in the mbuf */
typedef struct ifclbufhead_s {
    struct ifnet	icb_ifnet;	       	/* interface */
    struct rawif	icb_rawif;		/* raw interface */
#if !(IFCL_DEBUG_HDR) /* IFCL_DEBUG_HDR makes pad length 0 */
    char		pad[IFCLBUF_PAD];	/* unused */
#endif /* !(IFCL_DEBUG_HDR) */
    ifcl_hdr_t		icb_hdr;		/* ifcl header info */
} ifclbufhead_t;


/* send list for acknowledging every send */
typedef struct ifcl_slist_s {
    int			first;			/* head of list */
    int			last;			/* tail of list */
    u_int      		first_wrap;		/* how many head wraps */
    u_int      		last_wrap;		/* how many tail wraps */
#define	ISL_NOTIFY	0x1			/* the send has been acked */
    char		nflag[CL_MAX_OUTQ];	/* for ISL_NOTIFY flag */
    struct mbuf		*slist[CL_MAX_OUTQ];	/* mbufs queued */
    partid_t		pttn[CL_MAX_OUTQ];	/* associated parts */
} ifcl_slist_t;


/* each partition information */
typedef struct ifcl_xpttn_s {
    xpchan_t		icxp_xpchan;	/* xpr pointer for xpc_* calls */
    u_int		icxp_sent;	/* number of sends */
    u_int		icxp_ackd;	/* number of acknowledgements */
#define IFCL_XP_INIT	0x1		/* icxp_thread_sv been initted */
#define	IFCL_XP_UP	0x2		/* this pttn is up */
#define IFCL_XP_DISPEND 0x4     	/* disconnect pending */
    int			icxp_flags;	/* partition flags */
    u_int               icxp_nthreads;	/* number of threads done xpc_allocs
					 * without xpc_dones yet */
    sv_t                icxp_thread_sv;	/* sv to wait on before killing
					 * thread to wait for xpc_dones */
} ifcl_xpttn_t;


/* general ifcl structure saved for all info */
typedef struct ifcl_s {
    struct ifnet	cl_if;				/* struct ifnet */
    struct rawif	cl_rawif;			/* struct rawif */
#define	IFCL_INIT	0x1
#define	IFCL_READY	0x2
#define	IFCL_IF_UP	0x4
    int			cl_flags;			/* interface flags */
    char		cl_addr[IFCL_ADDR_SZ];		/* if_cl addr */
    struct in_addr	cl_inaddr;			/* interface addr */
    dev_t		cl_dev;				/* hwgraph entry */
    ifcl_xpttn_t	cl_xpttn[MAX_PARTITIONS];	/* xpttn struct */
    ifcl_slist_t	cl_slist;  			/* send list */
    lock_t		cl_mutex;			/* mutex */
} ifcl_t;

#define	IFCL_ARP_ALL	0x1	/* send arp request to everyone */
#define	IFCL_ARP_REQ	0x2	/* send arp request to someone */
#define	IFCL_ARP_REP	0x3	/* send arp reply to someone */


/* part of a control message (only type is arp right now). */
typedef struct ifcl_arp_s {
    u_short		ica_type;			/* ARP type */
    char		ica_src_addr[IFCL_ADDR_SZ];	/* src LLP addr (pid) */
    struct in_addr	ica_src_inaddr;			/* src ipaddr */
    struct in_addr	ica_arp_inaddr;			/* ARP ipaddr */
} ifcl_arp_t;

/*
 * We always look at the first field to figure out the ctrl pkt type. 
 * Don't change ict_type's type or location! Ctrl pkts are always sent 
 * as a immediate pkt. Hence it should fit into a single xpm
 */

#define	IFCL_CTRL_ARP	0x1

/* the message we send in a control (arp) message */
typedef struct ifcl_ctrl_s {
    u_int		ict_type;			/* ctrl pkt type */
    u_int		ict_version;			/* ifcl version */
    union {
        ifcl_arp_t	ict_arp_u;			/* arp info */
    } ict_u;
} ifcl_ctrl_t;

#define	ict_arp		ict_u.ict_arp_u

/* element snarp table -- saves the address of remote partitions over cl */
typedef struct snarptab_s {
    struct in_addr	snarp_inaddr;			/* address */
    char		snarp_addr[IFCL_ADDR_SZ];	/* address */
#define	SNARP_VALID	0x1
    u_short		snarp_flags;			/* valid ? */
#define	SNARP_PADSZ	(16 - sizeof(struct in_addr) - IFCL_ADDR_SZ - \
			sizeof(u_short))
    char		snarp_pad[SNARP_PADSZ];		/* unused */
} snarptab_t;

/* external functions */
extern void ifcl_init(void);
extern void ifcl_attach_xpttn(partid_t);
extern void ifcl_shutdown(partid_t);
extern void ifcl_xpc_async(xpc_t, xpchan_t, partid_t, int, void *);
extern int ifcl_output(struct ifnet *, struct mbuf *, struct sockaddr *,
			 struct rtentry *);
extern void ifcl_output_odone(xpc_t, xpchan_t, partid_t, int, void *);
extern int ifcl_ifioctl(struct ifnet *, int , void *);

#if IFCL_LOG

#define IMD     1	/* immediate type sent */
#define PTR     2	/* pointer type sent */

#define MAX_ADDRS_IN_MESG       5

/* entry in the receive log */
typedef struct re_s {
    int                 chan;		      		/* logical xpc chan
							 * received on */
    int                 cpu;				/* cpu doing receive */
    int                 type[MAX_ADDRS_IN_MESG];	/* IMD or PTR */
    kthread_t           *kthread;			/* kthread pointer for
							 * receive */
    size_t              scnt[MAX_ADDRS_IN_MESG];	/* sent xpm bytes */
    __psunsigned_t      saddr[MAX_ADDRS_IN_MESG];	/* sent xpm addr ptr */
    size_t              rcnt[MAX_ADDRS_IN_MESG];	/* recv alenl bytes */
    __psunsigned_t      raddr[MAX_ADDRS_IN_MESG];	/* recv alenl ptr */
} re_t;

#define LOG_ENTRIES     25000

/* receive log overall struct */
typedef struct ifcl_recv_log_s {
    int         ptr;		/* index into re array */
    int         wraps;		/* number of wraps of ptr */
    int         num_exc;	/* number of time info in xpm recorded
				 * was too large for re_t struct */
    lock_t      mutex;		/* lock for receive log */
    re_t        *re;		/* array of receive entries */
} ifcl_recv_log_t;

/* send log entry */
typedef struct se_s {
    int                 cpu;			/* cpu doing send */
    kthread_t           *kthread;		/* kthread pointer of 4 send */
    u_int               outsd;			/* # of messages that we 
						 * not successfully sent yet */
    size_t              cnt[MAX_ADDRS_IN_MESG];	/* received xpm bytes */
    __psunsigned_t      addr[MAX_ADDRS_IN_MESG];/* received xpm addr ptr */
} se_t;

/* send log overall struct */
typedef struct ifcl_send_log_s {
    int         ptr;		/* index into se array */
    int         wraps;		/* number of wraps of ptr */
    int         num_exc;	/* number of times info in xpm recorded
				 * was too long for se_t struct */
    lock_t      mutex;		/* lock for send log */
    se_t        *se;		/* array of sent entries */
} ifcl_send_log_t;

#endif /* IFCL_LOG */

#endif /* __SYS_SN0_IF_CL_H__ */
