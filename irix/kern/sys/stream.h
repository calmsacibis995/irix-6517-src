/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/* $Revision: 3.45 $ */


#ifndef _SYS_STREAM_H	/* wrapper symbol for kernel use */
#define _SYS_STREAM_H	/* subject to change without notice */

#ident	"@(#)uts-3b2:io/stream.h	1.4"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/strmdep.h>

#define _EAGER_RUNQUEUES

#ifdef _IRIX_LATER
#include <sys/poll.h>
#endif /* _IRIX_LATER */
#include <sys/cdefs.h>

#ifdef _KERNEL
#define free kern_free
#define malloc kern_malloc
#define calloc kern_calloc
#endif /* _KERNEL */

struct cred;
struct monitor;

/*
 * Data queue
 */
struct	queue {
	struct	qinit	*q_qinfo;	/* procs and limits for queue */
	struct	msgb	*q_first;	/* first data block */
	struct	msgb	*q_last;	/* last data block */
	struct	queue	*q_next;	/* Q of next stream */
	struct	queue	*q_link;	/* to next Q for scheduling */
	void		*q_ptr;		/* to private data structure */
	ulong_t		q_count;	/* number of bytes on Q */
	ulong_t		q_flag;		/* queue state */
	long		q_minpsz;	/* min packet size accepted by */
					/* this module */
	long		q_maxpsz;	/* max packet size accepted by */
					/* this module */
	ulong_t		q_hiwat;	/* queue high water mark */
	ulong_t		q_lowat;	/* queue low water mark */
	struct qband	*q_bandp;	/* separate flow information */
	unsigned char	q_nband;	/* number of priority bands > 0 */
	unsigned char	q_blocked;	/* number of bands flow controlled */
	unsigned char	q_pad1[2];	/* reserved for future use */
	struct monitor	**q_monpp;	/* pointer to the monitor pointer in
					   our controlling stream head */
	ulong_t		q_refcnt;	/* queue reference count */
	struct stdata	*q_strh;	/* pointer to stream head */
	ulong_t		q_pflag;	/* protected queue state */
#ifdef _STRQ_TRACING
	struct	ktrace	*q_trace;	/* trace records */
#endif
};

typedef struct queue queue_t;

/*
 * Queue flags.  Those marked with !P! are in the pflag word.
 */
#define	QENAB	0x001			/* !P! Queue is enabled to run */
#define	QWANTR	0x002			/* Someone wants to read Q */
#define	QWANTW	0x004			/* Someone wants to write Q */
#define	QFULL	0x008			/* Q is considered full */
#define	QREADR	0x010			/* This is the reader (first) Q */
#define	QUSE	0x020			/* This queue in use (allocation) */
#define	QNOENB	0x040			/* Don't enable Q via putq */
#define QBACK	0x100			/* queue has been back-enabled */
#define QHLIST	0x200			/* STRHOLD active on queue */

#ifdef _STRQ_TRACING
#define QTRC	0x800			/* queue being traced */
#endif

/*
 * Structure that describes the separate information
 * for each priority band in the queue.
 */
struct qband {
	struct qband	*qb_next;	/* next band's info */
	ulong_t		qb_count;	/* number of bytes in band */
	struct msgb	*qb_first;	/* beginning of band's data */
	struct msgb	*qb_last;	/* end of band's data */
	ulong_t		qb_hiwat;	/* high water mark for band */
	ulong_t		qb_lowat;	/* low water mark for band */
	ulong_t		qb_flag;	/* qband state (see below) */
	long		qb_pad1;	/* reserved for future use */
};

typedef struct qband qband_t;

/*
 * qband flags
 */
#define QB_FULL		0x01		/* band is considered full */
#define QB_WANTW	0x02		/* Someone wants to write to band */
#define QB_BACK		0x04		/* queue has been back-enabled */

/*
 * Maximum number of bands.
 */
#define NBAND	256

/*
 * Fields that can be manipulated through strqset() and strqget().
 */
typedef enum qfields {
	QHIWAT	= 0,		/* q_hiwat or qb_hiwat */
	QLOWAT	= 1,		/* q_lowat or qb_lowat */
	QMAXPSZ	= 2,		/* q_maxpsz */
	QMINPSZ	= 3,		/* q_minpsz */
	QCOUNT	= 4,		/* q_count or qb_count */
	QFIRST	= 5,		/* q_first or qb_first */
	QLAST	= 6,		/* q_last or qb_last */
	QFLAG	= 7,		/* q_flag or qb_flag */
	QBAD	= 8
} qfields_t;

/*
 * Module information structure
 */
struct module_info {
	ushort_t	mi_idnum;		/* module id number */
	char 		*mi_idname;		/* module name */
	long		mi_minpsz;		/* min packet size accepted */
	long		mi_maxpsz;		/* max packet size accepted */
	ulong_t		mi_hiwat;		/* hi-water mark */
	ulong_t 	mi_lowat;		/* lo-water mark */
	ushort_t	mi_locking;		/* locking type */
};


#define SINGLE_THREADED	0       /* cannot be multi-threaded (default) */
#define MULTI_THREADED	1       /* use private monitor */

struct qinit;

/*
 * Streamtab (used in cdevsw and fmodsw to point to module or driver)
 */

struct streamtab {
	struct qinit *st_rdinit;
	struct qinit *st_wrinit;
	struct qinit *st_muxrinit;
	struct qinit *st_muxwinit;
};

/*
 * Structure sent to mux drivers to indicate a link.
 */
struct linkblk {
	queue_t *l_qtop;	/* lowest level write queue of upper stream */
				/* (set to NULL for persistent links) */
	queue_t *l_qbot;	/* highest level write queue of lower stream */
	int      l_index;	/* index for lower stream. */
	long	 l_pad[5];	/* reserved for future use */
};

/*
 * Class 0 data buffer freeing routine
 */
struct free_rtn {
	void (*free_func)(char *);
	char *free_arg;
};

/*
 * data buffer extended freeing routine with buffer and size parameters
 */
struct freeext_rtn {
	void (*free_func)(char *, int);
	char *free_arg;
	int   size_arg;
};

#ifdef STREAMS_MEM_TRACE
/*
 * Streams memory trace log record
 *
 * NOTEs:
 * Since the Streams trace logging structure is contained in both the
 * msg and dblk records we must make total structure a power of 2 in length
 * so we adjust the number of logging entries accordingly.
 *
 * With STREAMS_MEM_TRACE defined, the total length of the log differs
 * depending on four cases {DEBUG|OPT} and {32-bit|64-bit} kernels.
 * The total length of sizeof(struct mbinfo) + sizeof(struct dbinfo) +
 * 2 * sizeof(struct str_mem_trace_log) MUST also be a power of 2 in length,
 * for all cases.
 */
#ifdef DEBUG

#if _MIPS_SIM == _ABI64
#define MEM_TRACE_LOG_SIZE 11 /* 64-bit DEBUG => Total log length 384 bytes */
#else
#define MEM_TRACE_LOG_SIZE  7 /* 32-bit DEBUG => Total log length 128 bytes */
#endif /* _MIPS_SIM == _ABI64 */

#else

#if _MIPS_SIM == _ABI64
#define MEM_TRACE_LOG_SIZE 13 /* 64-bit OPT => Total log length 448 bytes */
#else
#define MEM_TRACE_LOG_SIZE  7 /* 32-bit OPT => Total log length 128 bytes */
#endif /* _MIPS_SIM == _ABI64 */

#endif /* DEBUG */

struct str_mem_trace_entry {
	long	type;
	void	*arg1;
	void	*arg2;
	long	te_pad;
};

struct str_mem_trace_log {
	long	woff;
	long	tl_pad[3];
	struct str_mem_trace_entry entry[MEM_TRACE_LOG_SIZE];
};
#endif /* STREAMS_MEM_TRACE */

/*
 * Maximum size in bytes of  Streams buffer allocated to any individual datab
 * via the allocb() interface.
 */
#define STR_MAXBSIZE	NBPP

/*
 * Data block descriptor
 * Entire structure MUST be a power of 2 in length for both 32-bit and 64-bit
 * kernels. This includes the Streams tracing structure in debug mode.
 */
struct datab {
	union {
		struct datab	*freep;
		struct free_rtn *frtnp;
		struct freeext_rtn *frtn_extp;
	} db_f;
	unsigned char	*db_base;	/* base address of buffer */
	unsigned char	*db_lim;	/* last address of buffer */

	int		db_ref;		/* dblk ref cnt (MUST be integer) */
	unsigned short	db_size;	/* buffer size in bytes */
	unsigned char	db_type;	/* type of dblk */
	unsigned char	db_index;	/* index into buffer array */

	unsigned short	db_cpuid;	/* cpuid where datab allocated */
	unsigned short	db_buf_cpuid;	/* cpuid where buffer allocated */
	int		db_pad;		/* reserved for future use */

	struct	msgb	*db_msgaddr;	/* msg hdr pointing to datab (owner) */
	struct free_rtn	db_frtn;	/* local storage for free_rtn */

#if _MIPS_SIM != _ABI64
	int		db_pad2[6];	/* 32-bit => pad out to 64-bytes long*/
#endif /* _MIPS_SIM == _ABI64 */

#ifdef STREAMS_MEM_TRACE
	struct str_mem_trace_log tracelog;
#endif /* STREAMS_MEM_TRACE */
};

/*
 * Some defines for the free function union in the data block descriptor
 */
#define db_freep db_f.freep
#define db_frtnp db_f.frtnp
#define db_frtn_extp db_f.frtn_extp

/*
 * Message block descriptor
 * Entire structure MUST be a power of 2 in length for both 32-bit and 64-bit
 * kernels. This includes the Streams tracing structure in debug mode.
 */
struct	msgb {
	struct	msgb	*b_next;	/* next msg on queue chain */
	struct  msgb	*b_prev;	/* previous msg on queu chain */
	struct	msgb	*b_cont;	/* next msg in chain */

	unsigned char	*b_rptr;	/* read pointer into buffer */
	unsigned char	*b_wptr;	/* write pointer into buffer */

	struct datab 	*b_datap;	/* address of dblock */

	unsigned char	b_band;		/* band value */
	unsigned char	b_pad1;		/* reserved for future use */
	unsigned short	b_flag;		/* type of message flag */

	unsigned short	b_cpuid;	/* cpuid where node allocated */
	unsigned short	b_mopsflag;	/* ops done on msg status flag bits */

	int	 	b_mbuf_refct;	/* # mbuf dup op's one on this msg */
#if _MIPS_SIM == _ABI64
	int	 	b_mbuf_pad;	/* 64-bit kernels pad to align */
#else
	long		b_pad3[7];	/* 32-bit=> pad out to 64-bytes long */
#endif /* _MIPS_SIM == _ABI64 */

#ifdef STREAMS_MEM_TRACE
	struct str_mem_trace_log tracelog;
#endif /* STREAMS_MEM_TRACE */
};

typedef struct msgb mblk_t;
typedef struct datab dblk_t;
typedef struct free_rtn frtn_t;

#ifdef _STRQ_TRACING
#include <sys/proc.h>
#include <sys/ktrace.h>

/*
 * Queue tracing code.  Tracks mblks as they are put on and taken off of
 * queues (only if the queue has the QTRC flag set).
 */
#define	QUEUE_TRACE_SIZE	1024	/* XXX should be knob */
#define QUEUE_KTRACE_PUTQ	1
#define QUEUE_KTRACE_GETQ	2
#define QUEUE_KTRACE_ADD1	3
#define QUEUE_KTRACE_ADD2	4
#define QUEUE_KTRACE_SUB1	5
#define QUEUE_KTRACE_SUB2	6
#define QUEUE_KTRACE_ZERO	7
#define QUEUE_KTRACE_PUTBQ	8
#define QUEUE_KTRACE_INSQ	9
#define QUEUE_KTRACE_RMVQ	0xa
extern int __str_msgsize(mblk_t *); /* like msgdsize() but counts non-data */
extern void q_trace_putq(queue_t *, mblk_t *, inst_t *);
extern void q_trace_putbq(queue_t *, mblk_t *, inst_t *);
extern void q_trace_insq(queue_t *, mblk_t *, mblk_t *, inst_t *);
extern void q_trace_getq(queue_t *, mblk_t *, inst_t *);
extern void q_trace_rmvq(queue_t *, mblk_t *, inst_t *);
extern void q_trace_add_1(queue_t *, inst_t *);
extern void q_trace_add_2(queue_t *, inst_t *);
extern void q_trace_sub_1(queue_t *, inst_t *);
extern void q_trace_sub_2(queue_t *, inst_t *);
extern void q_trace_zero(queue_t *, inst_t *);
#endif

/*
 * Message flags.  These are interpreted by the stream head.
 */
#define MSGMARK		0x01		/* last byte of message is "marked" */
#define MSGNOLOOP	0x02		/* don't loop message around to */
					/* write side of stream */
#define MSGDELIM	0x04		/* message is delimited */
#define MSGNOGET	0x08		/* getq does not return message */

/*
 * Streams message types.
 * Data and protocol messages (regular and priority)
 */
#define	M_DATA		0x00		/* regular data */
#define M_PROTO		0x01		/* protocol control */

/*
 * Control messages (regular and priority)
 */
#define	M_BREAK		0x08		/* line break */
#define M_PASSFP	0x09		/* pass file pointer */
#define M_EVENT		0x0a		/* post an event to an event queue */
#define	M_SIG		0x0b		/* generate process signal */
#define	M_DELAY		0x0c		/* real-time xmit delay (1 param) */
#define M_CTL		0x0d		/* device-specific control message */
#define	M_IOCTL		0x0e		/* ioctl; set/get params */
#define M_SETOPTS	0x10		/* set various stream head options */
#define M_RSE		0x11		/* reserved for RSE use only */

/*
 * Control messages (high priority; go to head of queue)
 */
#define	M_IOCACK	0x81		/* acknowledge ioctl */
#define	M_IOCNAK	0x82		/* negative ioctl acknowledge */
#define M_PCPROTO	0x83		/* priority proto message */
#define	M_PCSIG		0x84		/* generate process signal */
#define	M_READ		0x85		/* generate read notification */
#define	M_FLUSH		0x86		/* flush your queues */
#define	M_STOP		0x87		/* stop transmission immediately */
#define	M_START		0x88		/* restart transmission after stop */
#define	M_HANGUP	0x89		/* line disconnect */
#define M_ERROR		0x8a		/* fatal error used to set user err */
#define M_COPYIN	0x8b		/* request to copyin data */
#define M_COPYOUT	0x8c		/* request to copyout data */
#define M_IOCDATA	0x8d		/* response to M_COPYIN and M_COPYOUT*/
#define M_PCRSE		0x8e		/* reserved for RSE use only */
#define	M_STOPI		0x8f		/* stop reception immediately */
#define	M_STARTI	0x90		/* restart reception after stop */
#define M_PCEVENT	0x91		/* post an event to an event queue */

/*
 * Queue message class definitions.  
 */
#define QNORM		0x00		/* normal priority messages */
#define QPCTL		0x80		/* high priority cntrl messages */

/*
 * IOCTL structure - this structure is the format of the M_IOCTL message type
 */
struct iocblk {
	int 		ioc_cmd;	/* ioctl command type */
	struct cred	*ioc_cr;	/* full credentials */
	uint		ioc_id;		/* ioctl id */
	uint		ioc_count;	/* count of bytes in data field */
	int		ioc_error;	/* error code */
	int		ioc_rval;	/* return value  */
#if _MIPS_SIM == _ABI64
	long		ioc_64bit;	/* is process using 64 bit abi? */
	long		ioc_filler[3];	/* reserved for future use */
#else
	long		ioc_filler[4];	/* reserved for future use */
#endif
};

#define ioc_uid ioc_cr->cr_uid
#define ioc_gid ioc_cr->cr_gid

/*
 * structure for the M_COPYIN and M_COPYOUT message types.
 */
struct copyreq {
	int		cq_cmd;		/* ioctl command (from ioc_cmd) */
	struct cred	*cq_cr;		/* full credentials */
	uint		cq_id;		/* ioctl id (from ioc_id) */
	caddr_t		cq_addr;	/* address to copy data to/from */
	uint		cq_size;	/* number of bytes to copy */
	int		cq_flag;	/* see below */
	mblk_t		*cq_private;	/* private state information */
	long		cq_filler[4];	/* reserved for future use */
};

#define cq_uid cq_cr->cr_uid
#define cq_gid cq_cr->cr_gid

/* cq_flag values */

#define STRCANON	0x01		/* b_cont data block contains */
					/* canonical format specifier */
#define RECOPY		0x02		/* perform I_STR copyin again, */
					/* this time using canonical */
					/* format specifier */
#define STRCOPYTRANS	0x10000000	/* M_COPYOUT data to address
					   specified by driver (cq_addr) */

/*
 * structure for the M_IOCDATA message type.
 */
struct copyresp {
	int		cp_cmd;		/* ioctl command (from ioc_cmd) */
	struct cred	*cp_cr;		/* full credentials */
	uint		cp_id;		/* ioctl id (from ioc_id) */
	caddr_t		cp_rval;	/* status of request: 0 -> success */
					/*             non-zero -> failure */
	uint		cp_pad1;	/* reserved */
	int		cp_pad2;	/* reserved */
	mblk_t 		*cp_private;	/* private state information */
#if _MIPS_SIM == _ABI64
	long		cp_64bit;	/* is process using 64 bit abi? */
	long		cp_filler[3];	/* reserved for future use */
#else
	long		cp_filler[4];	/* reserved for future use */
#endif
};

#define cp_uid cp_cr->cr_uid
#define cp_gid cp_cr->cr_gid

/*
 * Options structure for M_SETOPTS message.  This is sent upstream
 * by a module or driver to set stream head options.
 */
struct stroptions {
	ulong_t		so_flags;		/* options to set */
	short		so_readopt;		/* read option */
	ushort_t	so_wroff;		/* write offset */
	long		so_minpsz;		/* minimum read packet size */
	long		so_maxpsz;		/* maximum read packet size */
	ulong_t		so_hiwat;		/* read queue high water mark*/
	ulong_t		so_lowat;		/* read queue low water mark */
	unsigned char 	so_band;		/* band for water marks */
#ifndef _IRIX_LATER
	short		so_vtime;		/* polling read parameter */
        short		so_tostop;		/* setting of TOSTOP flag */
#endif /* !_IRIX_LATER */
};

/*
 * flags for stream options set message
 */
#define SO_ALL		0x003f	/* set all old options */
#define SO_READOPT	0x0001	/* set read option */
#define SO_WROFF	0x0002	/* set write offset */
#define SO_MINPSZ	0x0004	/* set min packet size */
#define SO_MAXPSZ	0x0008	/* set max packet size */
#define SO_HIWAT	0x0010	/* set high water mark */
#define SO_LOWAT	0x0020	/* set low water mark */
#define SO_MREADON      0x0040	/* set read notification ON */
#define SO_MREADOFF     0x0080	/* set read notification OFF */
#define SO_NDELON	0x0100	/* old TTY semantics for NDELAY reads/writes */
#define SO_NDELOFF      0x0200	/* STREAMS semantics for NDELAY reads/writes */
#define SO_ISTTY	0x0400	/* the stream is acting as a terminal */
#define SO_ISNTTY	0x0800	/* the stream is not acting as a terminal */
#define SO_TOSTOP	0x1000	/* stop on background writes to this stream */
#define SO_TONSTOP	0x2000	/* do not stop on background writes to stream */
#define SO_BAND		0x4000	/* water marks affect band */
#define SO_DELIM	0x8000	/* messages are delimited */
#define SO_NODELIM	0x010000	/* turn off delimiters */
#define SO_STRHOLD	0x020000	/* enable strwrite message coalescing */
#define SO_NOSTRHOLD	0x080000	/* disable strhold */
#ifndef _IRIX_LATER
#define SO_VTIME	0x040000 /* set vtime */
#endif /* !_IRIX_LATER */

/*
 * Structure for M_EVENT and M_PCEVENT messages.  This is sent upstream
 * by a module or driver to have the stream head generate a call to the
 * General Events subsystem.  It is also contained in the first M_DATA
 * block of an M_IOCTL message for the I_STREV and I_UNSTREV ioctls.
 */
struct str_evmsg {
	long		 sv_event;	/* the event (module-specific) */
	struct vnode	*sv_vp;		/* vnode pointer of event queue */
	long		 sv_eid;	/* same as ev_eid */
	long		 sv_evpri;	/* same as ev_pri */
	long		 sv_flags;	/* same as ev_flags */
	uid_t		 sv_uid;	/* user id of posting process */
	pid_t		 sv_pid;	/* process id of posting process */
	hostid_t	 sv_hostid;	/* host id of posting process */
	long		 sv_pad[4];	/* reserved for future use */
};

/*
 * queue information structure
 */
struct	qinit {
	int	(*qi_putp)(queue_t *, mblk_t *); /* put procedure */
	int	(*qi_srvp)(queue_t *);		/* service procedure */
	int	(*qi_qopen)(queue_t *, dev_t *, int, int,
				     struct cred *);	/* called on startup */
	int	(*qi_qclose)(queue_t *, int, struct cred *);
							/* called on finish */
	int	(*qi_qadmin)(void);		/* for future use */
	struct module_info *qi_minfo;	/* module information structure */
	struct module_stat *qi_mstat;	/* module statistics structure */
};

/*
 * Miscellaneous parameters and flags.
 *
 * New code for two-byte M_ERROR message.
 */
#define NOERROR	((unsigned char)-1)

/*
 * Values for stream flag in open to indicate module open, clone open;
 */
#define MODOPEN 	0x1	/* open as a module */
#define CLONEOPEN	0x2	/* open for clone, pick own minor device */

/*
 * Priority definitions for block allocation.
 */
#define BPRI_LO		1
#define BPRI_MED	2
#define BPRI_HI		3

/*
 * Value for packet size that denotes infinity
 */
#define INFPSZ		-1

/*
 * Flags for flushq()
 */
#define FLUSHALL	1	/* flush all messages */
#define FLUSHDATA	0	/* don't flush control messages */

/*
 * Flag for transparent ioctls
 */
#define TRANSPARENT	(unsigned int)(-1)

/*
 * Sleep priorities for stream io
 */
#define	STIPRI	PZERO+3
#define	STOPRI	PZERO+3

/*
 * Stream head default high/low water marks 
 */
#define STRHIGH 5120
#define STRLOW	1024

/*
 * Block allocation parameters
 */
#define MAXIOCBSZ	1024		/* max ioctl data block size */

extern int strholdtime;

/*
 * Definitions of Streams macros and function interfaces.
 *
 * Definition of spl function needed to provide critical region protection
 * for streams drivers and modules.  See ddi.c.
 */
extern int splstr(void);

/*
 * canenable - check if queue can be enabled by putq().
 */
#define canenable(q)	!((q)->q_flag & QNOENB)

/*
 * Finding related queues
 */
#define	OTHERQ(q)	((q)->q_flag&QREADR? (q)+1: (q)-1)
#define	WR(q)		((q)+1)
#define	RD(q)		((q)-1)
#define SAMESTR(q)	(((q)->q_next) && (((q)->q_flag&QREADR) == ((q)->q_next->q_flag&QREADR)))

#ifndef SAVE_PUTNEXT_Q_ADDRESS
#ifdef DEBUG
#define SAVE_PUTNEXT_Q_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->putnext_q = (void (*)())(addr)
#else
#define SAVE_PUTNEXT_Q_ADDRESS(mp, addr)
#endif /* DEBUG */
#endif /* SAVE_PUTNEXT_Q_ADDRESS */

#ifndef SAVE_PUTNEXT_QIPUTP_ADDRESS
#ifdef DEBUG
#define SAVE_PUTNEXT_QIPUTP_ADDRESS(mp, addr) \
	((struct mbinfo *)(mp))->putnext_qi_putp = (void (*)())(addr)
#else
#define SAVE_PUTNEXT_QIPUTP_ADDRESS(mp, addr)
#endif /* DEBUG */
#endif /* SAVE_PUTNEXT_QIPUTP_ADDRESS */

/*
 * Put a message of the next queue of the given queue.
 */
#ifdef STREAMS_DEBUG
extern void putnext_debug(queue_t *, mblk_t *);

#define putnext(q, mp) putnext_debug(q, mp)
#else
#define putnext(q, mp) ((*(q)->q_next->q_qinfo->qi_putp)((q)->q_next, (mp)))
#endif /* STREAMS_DEBUG */

/*
 * Test if data block type is one of the data messages (i.e. not a control
 * message).
 */
#define datamsg(type) ((type) == M_DATA || (type) == M_PROTO || (type) == M_PCPROTO || (type) == M_DELAY)

/*
 * Extract queue class of message block.
 */
#define queclass(bp) (((bp)->b_datap->db_type >= QPCTL) ? QPCTL : QNORM)

/*
 * Align address on next lower word boundary.
 */
#define straln(a)	(caddr_t)((__psint_t)(a) & ~(sizeof(__psint_t)-1))

/*
 * Find the max size of data block.
 */
#define bpsize(bp) ((unsigned int)(bp->b_datap->db_lim - bp->b_datap->db_base))

/*
 * declarations of common routines
 */
extern mblk_t *allocb(int, uint);
extern mblk_t *esballoc(unsigned char *, int, int, frtn_t *);
extern toid_t esbbcall(int, void (*)(), long);
extern int testb(int, uint);
extern toid_t bufcall(uint, int, void (*)(), long);
extern void unbufcall(toid_t);
extern void freeb(mblk_t *);
extern void freemsg(mblk_t *);
extern mblk_t *dupb(mblk_t *);
extern mblk_t *dupmsg(mblk_t *);
extern mblk_t *copyb(mblk_t *);
extern mblk_t *copymsg(mblk_t *);
extern void linkb(mblk_t *, mblk_t *);
extern mblk_t *unlinkb(mblk_t *);
extern mblk_t *rmvb(mblk_t *, mblk_t *);
extern int pullupmsg(mblk_t *, int);
extern mblk_t *msgpullup(mblk_t *, int);
extern int adjmsg(mblk_t *, int);
extern int msgdsize(mblk_t *);
extern mblk_t *getq(queue_t *);
extern void rmvq(queue_t *, mblk_t *);
extern void flushq(queue_t *, int);
extern void flushband(queue_t *, unsigned char, int);
extern int canput(queue_t *);
extern int canputnext(queue_t *);
extern int bcanput(queue_t *, unsigned char);
extern int bcanputnext(queue_t *, unsigned char);
extern void put(queue_t *, mblk_t *);
extern int putq(queue_t *, mblk_t *);
extern int putbq(queue_t *, mblk_t *);
extern int insq(queue_t *, mblk_t *, mblk_t *);
extern int putctl(queue_t *, int);
extern int putnextctl(queue_t *, int);
extern int putctl1(queue_t *, int, int);
extern int putnextctl1(queue_t *, int, int);
extern queue_t *backq(queue_t *);
extern void qreply(queue_t *, mblk_t *);
extern void qenable(queue_t *);
extern int qsize(queue_t *);
extern void noenable(queue_t *);
extern void enableok(queue_t *);
extern int strqset(queue_t *, qfields_t, unsigned char, long);
extern int strqget(queue_t *, qfields_t, unsigned char, long *);
extern int freezestr(queue_t *);		/* XXX pl_t */
extern void unfreezestr(queue_t *, int);	/* XXX pl_t */
extern void qprocson(queue_t *);
extern void qprocsoff(queue_t *);
extern int pcmsg(unsigned char);

#ifndef _IRIX_LATER
extern int strdrv_push(queue_t *, char *, dev_t *, struct cred *);
extern void sdrv_error(queue_t *, mblk_t *);
extern void sdrv_flush(queue_t *, mblk_t *);
extern mblk_t *str_allocb(int, queue_t *, uint);
extern void str_unbcall(queue_t *);
extern void str_conmsg(mblk_t **, mblk_t **, mblk_t *);
#endif /* !_IRIX_LATER */

/*
 * shared or externally configured data structures
 */
extern int nstrpush;			/* maxmimum number of pushes allowed */
extern struct strstat strst;		/* STREAMS statistics structure */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_STREAM_H */
