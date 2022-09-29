/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_STRSUBR_H	/* wrapper symbol for kernel use */
#define _SYS_STRSUBR_H	/* subject to change without notice */

/*#ident	"@(#)uts-3b2:io/strsubr.h	1.8"*/
#ident	"$Revision: 1.48 $"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WARNING:
 * Everything in this file is private, belonging to the
 * STREAMS subsystem.  The only guarantee made about the
 * contents of this file is that if you include it, your
 * code will not port to the next release.
 */
#include <sys/types.h>
#include <sys/driver.h>
#include <sys/poll.h>		/* pollhead */
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/kmem.h>

/*
 * Constants used by the common procedure to udpate
 * the per-cpu Stream statistics.
 */
#define STR_STREAM_OPS		0	/* stream use & max cnt */
#define STR_STREAM_FAILOPS	1	/* stream fail cnt */
#define STR_QUEUE_OPS		2	/* queue use & max cnt */
#define STR_QUEUE_FAILOPS	3	/* queue fail cnt */
#define STR_MD_BUF_OPS		4	/* update md plus buffer use & maxcnt*/
#define STR_BUFFER_OPS		5	/* buffer use & max cnt */
#define STR_BUFFER_FAILOPS	6	/* buffer fail cnt */
#define STR_LINK_OPS		7	/* linkblk use & max cnt */
#define STR_LINK_FAILOPS	8	/* linkblk fail cnt */
#define STR_STREVENT_OPS	9	/* strevent use & max cnt */
#define STR_STREVENT_FAILOPS	10	/* strevent fail cnt */
#define STR_QBAND_OPS		11	/* queue band info use & max cnt */
#define STR_QBAND_FAILOPS	12	/* queue band info fail cnt */

/*
 * Flags for b_mopsflag in msgb structure indicating allocation/free status
 * This helps in core dumps of production systems to track down
 * msg/dblk/buffer operations performed on the data structures.
 */
#define STR_MOPS_ALLOCMSG	 0x0001	/* Allocated msg header only */
#define STR_MOPS_ALLOCMD	 0x0002	/* Allocated msg/dblk hdrs together */
#define STR_MOPS_ALLOCMD_ESB	 0x0004	/* Allocated msg/dblk via esballoc */

#define STR_MOPS_FREEMSG	 0x0008		/* Freed msg header only */
#define STR_MOPS_FREEMD		 0x0010		/* Freed both msg/dblk hdrs */
#define STR_MOPS_FREEMD_ESB	 0x0020		/* Freed esballoc msg/dblk */

#define STR_MOPS_MBLK_TO_MBUF      0x0040	/* mblk_to_mbuf op done */
#define STR_MOPS_MBLK_TO_MBUF_DUP  0x0080	/* mblk_to_mbuf_dup op done */
#define STR_MOPS_MBLK_TO_MBUF_FREE 0x0100	/* mblk_to_mbuf_free op done*/

#define STR_MOPS_MBUF_TO_MBLK	   0x0200	/* mbuf_to_mblk op done */
#define STR_MOPS_MBUF_TO_MBLK_FREE 0x0400	/* mbuf_to_mblk_free op done*/

/*
 * Header for a stream: interface to rest of system.
 */
typedef struct stdata {
	struct queue *sd_wrq;		/* write queue */
	struct msgb *sd_iocblk;		/* return block for ioctl */
	struct vnode *sd_vnode;		/* pointer to associated vnode */
	struct streamtab *sd_strtab;	/* pointer to streamtab for stream */
	long sd_flag;			/* state/flags */
	long sd_pflag;			/* spinlock protected state/flags */
	ulong sd_iocid;			/* ioctl id */
	pid_t   sd_pgid;		/* process group, for signals */
	struct vsession *sd_vsession;	/* session for controlling terminal */
	ushort sd_iocwait;		/* ct of procs waiting to do ioctl*/
	ushort sd_wroff;		/* write offset */
	int sd_rerror;			/* read error to set user errorno */
	int sd_werror;			/* write error to set user errorno */
	int sd_pushcnt;			/* number of pushes done on stream */
	int sd_sigflags;		/* logical OR of all siglist events */
	struct strevent *sd_siglist;	/* pid link-list to rcv SIGPOLL sig*/
	struct pollhead sd_pollist;	/* list of all pollers to wake up */
	struct msgb *sd_mark;		/* "marked" message on read queue */
	int sd_closetime;		/* time to wait to drain q in close */
	clock_t sd_rtime;		/* time to release held message */
	struct stdata *sd_hnext;	/* "held" message list linkage */
#ifndef _IRIX_LATER
	short   sd_vtime;		/* polling read parameter */
#endif /* !_IRIX_LATER */
	mon_t	*sd_monp;		/* private monitor for the stream */
	ulong_t	sd_refcnt;		/* stream head reference count */
	struct stdata *sd_assoc_prev;	/* "associated" stream hd list ptrs */
	struct stdata *sd_assoc_next;
} stdata_t;

/*
 * stdata flag field defines (those marked !P! moved to sd_pflag word)
 */
#define	IOCWAIT		0x00000001	/* Someone wants to do ioctl */
#define RSLEEP		0x00000002	/* !P! Someone wants to read/recv msg*/
#define	WSLEEP		0x00000004	/* !P! Someone wants to write */
#define STRPRI		0x00000008	/* An M_PCPROTO is at stream head */
#define	STRHUP		0x00000010	/* Device has vanished */
#define	STWOPEN		0x00000020	/* waiting for 1st open */
#define STPLEX		0x00000040	/* stream is being multiplexed */
#define STRISTTY	0x00000080	/* stream is a terminal */
#define RMSGDIS		0x00000100	/* read msg discard */
#define RMSGNODIS	0x00000200	/* read msg no discard */
#define STRDERR		0x00000400	/* fatal read error from M_ERROR */
#define STRTIME		0x00000800	/* used with timeout strtime */
#define STR2TIME	0x00001000	/* used with timeout str2time */
#define STR3TIME	0x00002000	/* used with timeout str3time */
#define STRCLOSE	0x00004000	/* wait for a close to complete */
#define SNDMREAD	0x00008000	/* used for read notification */
#define OLDNDELAY	0x00010000	/* use old TTY semantics for NDELAY */
					/* reads and writes */
#define RDBUFWAIT	0x00020000	/* used with bufcall in strqbuf() */
#define STRSNDZERO	0x00040000	/* send 0-length msg. down pipe/FIFO */
#define STRTOSTOP	0x00080000	/* block background writes */
#define	RDPROTDAT	0x00100000	/* read M_[PC]PROTO contents as data */
#define RDPROTDIS	0x00200000	/* discard M_[PC]PROTO blocks and */
					/* retain data blocks */
#define STRMOUNT	0x00400000	/* stream is mounted */
#define STRSIGPIPE	0x00800000	/* send SIGPIPE on write errors */
#define STRDELIM	0x01000000	/* generate delimited messages */
#define STWRERR		0x02000000	/* fatal write error from M_ERROR */
#define STRHOLD		0x04000000	/* enable strwrite message coalescing*/

#define STFIONBIO	0x08000000	/* do BSD-style non-blocking errors */
#define STRCTTYALLOC	0x10000000	/* controlling tty alloc in progress */
#define STRCTTYWAIT	0x20000000	/* someone's waiting for the */
					/* controlling tty to be allocated */

/*
 * Structure of list of processes to be sent SIGSEL signal
 * on request, or for processes sleeping on select().  The valid 
 * SIGSEL events are defined in stropts.h, and the valid select()
 * events are defined in select.h.
 */
struct strevent {
	union {
		struct {
			pid_t	procpid;
			/* AT+T had this as a long, but an int matches
			 * its usage in the kernel, and saves some
			 * 32/64 bit conversion.
			 */
			__int32_t	events;
		} e;	/* stream event */
		struct {
			void (*func)(long);
			long arg;
			int size;
			mon_t **monpp;
		} b;	/* bufcall event */
	} x;
	struct strevent *se_next;
};

#define se_pid		x.e.procpid
#define se_events	x.e.events
#define se_func		x.b.func
#define se_arg		x.b.arg
#define se_size		x.b.size
#define se_monpp	x.b.monpp

#define SE_SLEEP	KM_SLEEP	/* ok to sleep in allocation */
#define SE_NOSLP	KM_NOSLEEP	/* don't sleep in allocation */

/*
 * bufcall list
 */
struct bclist {
	struct strevent	*bc_head;
	struct strevent	*bc_tail;
};

/*
 * Structure used to track mux links and unlinks.
 */
struct mux_node {
	device_driver_t	 mn_ddt;	/* internal major device ID */
	ushort		 mn_indegree;	/* number of incoming edges */
	struct mux_node *mn_originp;	/* where we came from during search */
	struct mux_edge *mn_startp;	/* where search left off in mn_outp */
	struct mux_edge *mn_outp;	/* list of outgoing edges */
	uint		 mn_flags;	/* see below */
};

/*
 * Flags for mux_nodes.
 */
#define VISITED	1

/*
 * Edge structure - a list of these is hung off the
 * mux_node to represent the outgoing edges.
 */
struct mux_edge {
	struct mux_node	*me_nodep;	/* edge leads to this node */
	struct mux_edge	*me_nextp;	/* next edge */
	int		 me_muxid;	/* id of link */
};

/*
 * Structure to keep track of resources that have been allocated
 * for streams - an array of these are kept, one entry per
 * resource.  This is used by crash to dump the data structures.
 */
struct strinfo {
	void	*sd_head;	/* head of in-use list */
	int	sd_cnt;		/* total # allocated */
};

#define DYN_STREAM	0	/* for stream heads */
#define DYN_QUEUE	1	/* for queues */
#define DYN_MSGBLOCK	2	/* for message blocks */
#define DYN_LINKBLK	3	/* for mux links */
#define DYN_STREVENT	4	/* for stream event cells */
#define DYN_QBAND	5	/* for qband structures */

#define NDYNAMIC	6	/* number data types dynamically allocated */

/*
 * The following structures are mainly used to keep track of
 * the resources that have been allocated so crash can find
 * them (they are stored in a doubly-linked list with the head
 * of it stored in the Strinfo array.  Other data may be stored
 * away here too since this is private to streams.  Pointers
 * to these objects are returned by the allocating procedures,
 * which are later passed to the freeing routine.  The data
 * structure itself must appear first because the pointer is
 * overloaded to refer to both the structure itself or its
 * envelope, depending on context.
 *
 * Stream head info
 */
struct shinfo {
	stdata_t	sh_stdata;	/* must be first */
	struct shinfo	*sh_next;	/* next in list */
	struct shinfo	*sh_prev;	/* previous in list */
#ifdef DEBUG
	void	(*sh_alloc_func)();	/* pc addr where stream head alloc */
	void	(*sh_freeb_func)();	/* pc addr where stream head freed */
#endif /* DEBUG */
};

/*
 * data block info
 * MUST be a power of 2 in length for both 32-bit and 64-bit kernels
 * NOTE: The sizeof(struct mbinfo) plus sizeof(struct dbinfo) ALSO MUST
 * be a power of 2 in length for both 32-bit and 64-bit kernels
 */
struct dbinfo {
	dblk_t	d_dblock;	/* the data block itself */
#ifdef DEBUG
	struct dbinfo *d_next;	/* next data block */
	struct dbinfo *d_prev;	/* previous */
#if _MIPS_SIM == _ABI64
	long   dbpad[6];	/* 64-bit => pad to 64-bytes; power 2 align */
#else
	long   dbpad[14];	/* 32-bit => pad to 64-bytes; power 2 align */
#endif /* _MIPS_SIM == _ABI64 */
#endif /* DEBUG */
};

/*
 * message block info
 * MUST be a power of 2 in length for both 32-bit and 64-bit kernels
 * NOTE: The sizeof(struct mbinfo) plus sizeof(struct dbinfo) ALSO MUST
 * be a power of 2 in length for both 32-bit and 64-bit kernels
 */
struct mbinfo {
	mblk_t	m_mblock;	/* the message block itself */
#ifdef DEBUG
	struct mbinfo *m_next;	/* next message block */
	struct mbinfo *m_prev;	/* previous message block */

	void	(*alloc_func)(); /* pc addr where msgb allocb called */
	void	(*dupb_func)();	 /* pc addr where msgb dupb called */	
	void	(*freeb_func)(); /* pc addr where msgb freeb called */

	void	(*putq_q)(); /* addr struct queue in last putnext op */
	void	(*putnext_q)(); /* addr struct queue in last putnext op */
	void	(*putnext_qi_putp)(); /* addr qi_putp in last putnext */

#if _MIPS_SIM != _ABI64
	long   mbpad[8];	/* 32-bit => pad out to 64-bytes long */
#endif /* _MIPS_SIM != _ABI64 */
#endif /* DEBUG */
};

/*
 * Stream procedure call entry/exit statistics
 * NOTE: All statistics related macro's are NO-OP's for production kernels,
 * statistics counts are maintained ONLY in DEBUG kernels.
 */
#ifdef DEBUG
#define SAVE_ALLOCB_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->alloc_func = (void (*)())(addr)
#define SAVE_DUPB_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->dupb_func = (void (*)())(addr)
#define SAVE_FREEB_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->freeb_func = (void (*)())(addr)

#define SAVE_PUTQ_QADDRESS(mp, qaddr) \
	((struct mbinfo *)(mp))->putq_q = (void (*)())(qaddr)
#define SAVE_PUTNEXT_Q_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->putnext_q = (void (*)())(addr)
#define SAVE_PUTNEXT_QIPUTP_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->putnext_qi_putp = (void (*)())(addr)

#define SAVE_MBUF_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->putq_q = (void (*)())(addr)
#define SAVE_MBUFDUP_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->putnext_q = (void (*)())(addr)

#define SAVE_LASTPROC_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->freeb_func = (void (*)())(addr)
#else
#define SAVE_ALLOCB_ADDRESS(mp, addr)
#define SAVE_DUPB_ADDRESS(mp, addr)
#define SAVE_FREEB_ADDRESS(mp, addr)

#define SAVE_PUTQ_QADDRESS(mp, qaddr)
#define SAVE_PUTNEXT_Q_ADDRESS(mp, addr)
#define SAVE_PUTNEXT_QIPUTP_ADDRESS(mp, addr)
#define SAVE_MBUF_ADDRESS(mp, addr)
#define SAVE_MBUFDUP_ADDRESS(mp, addr)
#define SAVE_LASTPROC_ADDRESS(mp, addr)

#endif /* DEBUG */

/*
 * Stream event info
 */
struct seinfo {
	struct strevent	s_strevent;	/* must be first */
	struct seinfo	*s_next;	/* next in list */
	struct seinfo	*s_prev;	/* previous in list */
	long   se_pad[5];		/* pad for power 2 alignment */
};

/*
 * Queue info
 */
struct queinfo {
	struct queue	qu_rqueue;	/* read queue - must be first */
	struct queue	qu_wqueue;	/* write queue - must be second */
	struct queinfo	*qu_next;	/* next in list */
	struct queinfo	*qu_prev;	/* previous in list */
};

/*
 * Multiplexed streams info
 */
struct linkinfo {
	struct linkblk	li_lblk;	/* must be first */
	struct vfile	*li_fpdown;	/* file pointer for lower stream */
	struct linkinfo	*li_next;	/* next in list */
	struct linkinfo *li_prev;	/* previous in list */
};

/*
 * Qband info
 */
struct qbinfo {
	struct qband	qbi_qband;	/* must be first */
	struct qbinfo	*qbi_next;	/* next in list */
	struct qbinfo	*qbi_prev;	/* previous in list */
};

/*
 * Lock Free (lf) element structure definition
 */
struct lf_elt {
	struct	lf_elt *next;
};

/*
 * list vector structure definition
 */
struct listvec {
	struct lf_elt 	*head;	/* head of free node list */
	struct lf_elt 	*tail;	/* tail of free node list */
	u_int	nodecnt;	/* number elts on free node list */
	u_int	avg_nodecnt;	/* average node count on free node list */

	char	*reclaim_page;	/* base address of page we're scavenging */
	__psint_t node_mask;	/* address mask for debugging free ops */

	short	nelts;		/* number of elts of reclaimed page so far */
	short	e_per_page;	/* number of elts per page */
	short	size;		/* node size in bytes (Power of 2) */
	short	threshold;	/* threshold for local list length */

#ifdef DEBUG
	struct lf_elt *reclaim_page_cur; /* current node on reclaim list */

	u_int	get_calls;	/* # lf_get ops on this listvec */
	u_int   get_local;	/* # lf_get ops got node from listvec */
	u_int   get_addpage;	/* # lf_get ops got added page of nodes */
	u_int   get_fail;	/* # lf_get ops got which totally failed */

	u_int	free_calls;	/* number lf_free ops on this listvec */
	u_int	free_reclaimpage_set; /* number times set reclaim_page field */
	u_int   free_reclaimpage_clear;
	u_int   free_reclaimpage_incr;
	u_int   free_pagesize_release;
	u_int   free_pagesize_norelease;
	u_int   free_localadd;
#endif
};

#define LFVEC_MAX_INDEX		7 /* NOTE: Also in file stream.c */
/*
 * Lock Free (lf) list vector structure definition
 */
struct lf_listvec {
	lock_t	lock;		/* lock (Used in MP machines) */
	struct listvec buffer[LFVEC_MAX_INDEX];	/* various size buffers */
};


/*
 * Miscellaneous parameters and flags.
 *
 * Default timeout in seconds for ioctls and close
 */
#define STRTIMOUT 15

/*
 * Flag values for stream io waiting procedure (strwaitq)
 */
#define WRITEWAIT	0x1	/* waiting for write event */
#define READWAIT	0x2	/* waiting for read event */
#define NOINTR		0x4	/* error is not to be set for signal */
#define GETWAIT		0x8	/* waiting for getmsg event */

/*
 * Copy modes for tty and I_STR ioctls
 */
#define	U_TO_K 	01			/* User to Kernel */
#define	K_TO_K  02			/* Kernel to Kernel */

/*
 * canonical structure definitions
 */
#define STRLINK		"lli"
#define STRIOCTL	"iiil"
#define STRPEEK		"iiliill"
#ifdef CKPT
#define	STRPEEKN	"iiliilli"
#define	STRNREADN	"isii"
#endif
#define STRFDINSERT	"iiliillii"
#define O_STRRECVFD	"lssc8"
#define STRRECVFD	"lllc8"
#define S_STRRECVFD	"lllsslllllllllll"
#define STRNAME		"c0"
#define STRINT		"i"
#define STRTERMIO	"ssssc12"
#define STRTERMCB	"c6"
#define STRSGTTYB	"c4i"
#define STRTERMIOS	"llllc20"
#define STRLIST		"il"
#define STRSEV		"issllc1"
#define STRGEV		"ili"
#define STREVENT	"lssllliil"
#define STRLONG		"l"
#define STRBANDINFO	"ci"

#define STRPIDT		"l"

/*
 * Tables we reference during open(2) processing.
 */
#define CDEVSW	0
#define FMODSW	1

/*
 * Mux definitions
 */
#define LINKNORMAL	0x01		/* normal mux link */
#define LINKPERSIST	0x02		/* persistent mux link */
#define LINKTYPEMASK	0x03		/* bitmask of all link types */
#define LINKCLOSE	0x04		/* unlink from strclose */
#define LINKIOCTL	0x08		/* unlink from strioctl */
#define LINKNOEDGE	0x10		/* no edge to remove from graph */

/*
 * Definitions of Streams macros and function interfaces.
 *
 * Queue scheduling macros.
 */
extern toid_t itimeout(void (*)(), void *, long, pl_t, ...);
extern char qrunflag;			/* set iff queues are enabled */

#define qready()	qrunflag	/* test if queues are ready to run */

/*
 * This doesn't need to be at plstr. Allow the timeout to start at
 * splbase and let the different queuerun() versions (UP vs. MP)
 * determine if it needs to be at splstr or not.
 */
extern pl_t plbase;
#define setqsched() { \
	if (!qrunflag) { \
		qrunflag = 1; \
		itimeout(queuerun, NULL, TIMEPOKE_NOW, plbase); \
	} \
}

/*
 * STRHOLD support macros
 */
#define STRHOLDTIME (((strholdtime*HZ)+999)/1000)

/*
 * Macros dealing with mux_nodes.
 */
#define MUX_VISIT(X)	((X)->mn_flags |= VISITED)
#define MUX_CLEAR(X)	((X)->mn_flags &= (~VISITED)); \
			((X)->mn_originp = NULL)
#define MUX_DIDVISIT(X)	((X)->mn_flags & VISITED)

/*
 * qinit structures for stream head read and write queues.
 */
extern struct qinit	strdata;
extern struct qinit	stwdata;

/*
 * Declarations of private variables.
 * Not to be referenced outside the Streams code!
 */
extern struct queue	*qhead;		/* head of queues to run */
extern struct queue	**qtailp;	/* last queue */

extern struct stdata	*strholdhead;	/* list of streams with held messages*/
extern struct stdata	**strholdtailp;	/*  tail of above list */

extern struct bclist	strbcalls;	/* list of waiting bufcalls */
extern int		strholdflag;	/* strhold timeout pending */

extern struct strinfo	Strinfo[];	/* dynamic resource info */

/*
 * Declarations of private routines.
 */
struct cred;
struct vfile;
struct uio;
union rval;

extern void strinit(void);
extern void strsendsig(struct strevent *, int, long);
extern void strevpost(struct stdata *, int, long);
extern void strdrpost(mblk_t *);
extern int qattach(queue_t *, dev_t *, int, int, void *, struct cred *);
extern void qdetach(queue_t *, int, int, struct cred *);
extern void strrelease(void);
extern void strtime(struct stdata *);
extern void str2time(struct stdata *);
extern void str3time(struct stdata *);
extern int putiocd(mblk_t *, mblk_t *, caddr_t, int, int, char *);
extern int getiocd(mblk_t *, caddr_t, int, char *);
extern struct linkinfo *alloclink(queue_t *, queue_t *, struct vfile *);
extern void lbfree(struct linkinfo *);
extern int linkcycle(stdata_t *, stdata_t *);
extern struct linkinfo *findlinks(stdata_t *, int, int);
extern queue_t *getendq(queue_t *);
extern int munlink(struct stdata **, struct linkinfo *, int,
			     struct cred *, int *);
extern int munlinkall(struct stdata **, int, struct cred *, int *);
extern int mux_addedge(stdata_t *, stdata_t *, int);
extern void setq(queue_t *, struct qinit *, struct qinit *);
extern int strmakemsg(struct strbuf *, int, struct uio *,
			     struct stdata *, long, mblk_t **);
extern int strwaitbuf(int, int);
#ifdef _IRIX_LATER
extern int strwaitq(struct stdata *, int, off_t, int, int *);
#else
extern int strwaitq(struct stdata *, int, off_t, int, int, int *);
#endif /* _IRIX_LATER */
extern int xmsgsize(mblk_t *, int);
extern struct stdata *shalloc(queue_t *);
extern void shfree(struct stdata *);
extern mblk_t *xmsgalloc(void);
extern struct mdbblock *xmdballoc(void);
extern queue_t *allocq(void);
extern void freeq(queue_t *);
extern qband_t *allocband(void);
extern struct strevent *sealloc(int);
extern void sefree(struct strevent *);
extern void queuerun(void);
extern void runqueues(void);
extern int findmod(char *);
extern void setqback(queue_t *, unsigned char);
extern int strcopyin(caddr_t, caddr_t, unsigned int, char *, int);
extern int strcopyout(caddr_t, caddr_t, unsigned int, char *, int);
extern void strsignal(struct stdata *, int, long);
extern void strhup(struct stdata *);
extern void stralloctty(struct vnode *, struct stdata *);
extern void strpunlink(struct cred *);
extern void strclean(struct stdata *);
extern void strctltty(queue_t *);

extern int stropen(struct vnode *, dev_t *, struct vnode **,
			int, struct cred *);
extern void stropen_clone_finish(struct vnode *, struct stdata *, int);
extern int strclose(struct vnode *, int, struct cred *);
extern int strread(struct vnode *, struct uio *, struct cred *);
extern int strwrite(struct vnode *, struct uio *, struct cred *);
extern int strioctl(struct vnode *, int, void *, int, int,struct cred *,int *);
extern int strgetmsg(struct vnode *, struct strbuf *, struct strbuf *,
		unsigned char *, int *, int, union rval *);
extern int strputmsg(struct vnode *, struct strbuf *, struct strbuf *,
		unsigned char, int, int);
extern int strpoll(struct stdata *, short, int, short *,
		struct pollhead **, unsigned int *);

extern int strdoioctl(struct stdata *, struct strioctl *, mblk_t *,
		int, char *, struct cred *, int *);

struct mux_node *str_find_mux_node(device_driver_t, int);
extern void str_free_mux_node(device_driver_t);

extern int fmfind(char *);
extern void _queuerun(void);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_STRSUBR_H */
