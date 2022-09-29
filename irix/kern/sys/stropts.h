/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_STROPTS_H	/* wrapper symbol for kernel use */
#define _SYS_STROPTS_H	/* subject to change without notice */

/*#ident	"@(#)uts-3b2:io/stropts.h	1.8"*/
#ident	"$Revision: 3.20 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <standards.h>
#include <sys/types.h>
#ifdef _IRIX_LATER
/*
 * For struct ecb (source compatibility).
 */
#include <sys/evecb.h>
#endif /* _IRIX_LATER */
#if _KERNEL
#include <sys/conf.h>
#endif /* _KERNEL */

/* Warning - also defined in conf.h - but this is an XPG header */
#define	FMNAMESZ	8

/*
 * Write options.
 */
#define SNDZERO		0x001		/* send a zero length message */
#define SNDPIPE		0x002		/* send SIGPIPE on write and */
					/* putmsg if sd_werror is set */
#define SNDHOLD		0x004		/* enable strhold feature */

/*
 * Read options
 */

#define RNORM		0x000		/* read msg norm */
#define RMSGD		0x001		/* read msg discard */
#define RMSGN		0x002		/* read msg no discard */

#if _SGIAPI
#define RMODEMASK	0x003		/* all above bits */
#endif  /* _SGIAPI  */

/*
 * These next three read options are added for the sake of
 * user-level transparency.  RPROTDAT will cause the stream head
 * to treat the contents of M_PROTO and M_PCPROTO message blocks
 * as data.  RPROTDIS will prevent the stream head from failing
 * a read with EBADMSG if an M_PROTO or M_PCPROTO message is on
 * the front of the stream head read queue.  Rather, the protocol
 * blocks will be silently discarded and the data associated with
 * the message (in linked M_DATA blocks), if any, will be delivered
 * to the user.  RPROTNORM sets the default behavior, where read
 * will fail with EBADMSG if an M_PROTO or M_PCPROTO are at the
 * stream head.
 */
#define RPROTDAT	0x004		/* read protocol messages as data */
#define RPROTDIS	0x008		/* discard protocol messages, but */
					/* read data portion */
#define RPROTNORM	0x010

#if _SGIAPI
#define RPROTMASK	0x01c		/* all RPROT bits */
#endif  /* _SGIAPI  */

/*
 * Flush options
 */

#define FLUSHR		0x01L		/* flush read queue */
#define FLUSHW		0x02L		/* flush write queue */
#define FLUSHRW		0x03L		/* flush both queues */
#define FLUSHBAND	0x04L		/* flush only band specified */
					/* in next byte */

/*
 * Events for which to be sent SIGPOLL signal and for which events
 * can be posted by the I_SETEV ioctl.
 */
#define S_INPUT		0x0001		/* any msg but hipri on read Q */
#define S_HIPRI		0x0002		/* high priority msg on read Q */
#define S_OUTPUT	0x0004		/* write Q no longer full */
#define S_MSG		0x0008		/* signal msg at front of read Q */
#define	S_ERROR		0x0010		/* error msg arrived at stream head */
#define	S_HANGUP	0x0020		/* hangup msg arrived at stream head */
#define S_RDNORM	0x0040		/* normal msg on read Q */
#define S_WRNORM	S_OUTPUT
#define	S_RDBAND	0x0080		/* out of band msg on read Q */
#define S_WRBAND	0x0100		/* can write out of band */
#define S_BANDURG	0x0200		/* modifier to S_RDBAND, to generate */
					/* SIGURG instead of SIGPOLL */

/*
 * Flags for getmsg() and putmsg() syscall arguments.
 * "RS" stands for recv/send.  The system calls were originally called
 * recv() and send(), but were renamed to avoid confusion with the BSD
 * calls of the same name.  A value of zero will cause getmsg() to return
 * the first message on the stream head read queue and putmsg() to send
 * a normal priority message.
 */
#define RS_HIPRI	0x01		/* send/recv high priority message */

/*
 * Flags for getpmsg() and putpmsg() syscall arguments.
 */

/*
 * These are settable by the user and will be set on return
 * to indicate the priority of message received.
 */
#define MSG_HIPRI	0x01		/* send/recv high priority message */
#define MSG_ANY		0x02		/* recv any messages */
#define MSG_BAND	0x04		/* recv messages from specified band */

/*
 * Flags returned as value of getmsg() and getpmsg() syscall.
 */
#define MORECTL		1		/* more ctl info is left in message */
#define MOREDATA	2		/* more data is left in message */

/*
 * Define to indicate that all multiplexors beneath a stream should
 * be unlinked.
 */
#define MUXID_ALL	(-1)

/*
 * Flag definitions for the I_ATMARK ioctl.
 */
#define ANYMARK		0x01
#define LASTMARK	0x02

/*
 *  Stream Ioctl defines
 */
#define	STR		('S'<<8)
/* (STR|000) in use */
#define I_NREAD		(STR|01)
#define I_PUSH		(STR|02)
#define I_POP		(STR|03)
#define I_LOOK		(STR|04)
#define I_FLUSH		(STR|05)
#define I_SRDOPT	(STR|06)
#define I_GRDOPT	(STR|07)
#define I_STR		(STR|010)
#define I_SETSIG	(STR|011)
#define I_GETSIG	(STR|012)
#define I_FIND		(STR|013)
#define I_LINK		(STR|014)
#define I_UNLINK	(STR|015)
/* (STR|016) in use */
#define I_PEEK		(STR|017)
#define I_FDINSERT	(STR|020)
#define I_SENDFD	(STR|021)
#if defined(_KERNEL)

#define I_RECVFD	(STR|022)
#define I_E_RECVFD	(STR|016)

#else

#define I_RECVFD	(STR|016)	/* maps to kernel I_E_RECVFD */


#endif /* defined(_KERNEL) */

#define I_SWROPT	(STR|023)
#define I_GWROPT	(STR|024)
#define I_LIST		(STR|025)
#define I_PLINK		(STR|026)
#define I_PUNLINK	(STR|027)
#ifdef GEM
#define I_SETEV		(STR|030)
#define I_GETEV		(STR|031)
#define I_STREV		(STR|032)
#define I_UNSTREV	(STR|033)
#endif /* GEM */
#define I_FLUSHBAND	(STR|034)
#define I_CKBAND	(STR|035)
#define I_GETBAND	(STR|036)
#define I_ATMARK	(STR|037)
#define I_SETCLTIME	(STR|040)
#define I_GETCLTIME	(STR|041)
#define I_CANPUT	(STR|042)
#define	I_S_RECVFD	(STR|043)

#ifdef CKPT
#define	I_NREADN	(STR|044)
#define	I_PEEKN		(STR|045)
#define	I_FIFOENDS	(STR|046)
#endif /* CKPT */

/*
 * User level ioctl format for ioctls that go downstream (I_STR)
 */
struct strioctl {
	int 	ic_cmd;			/* command */
	int	ic_timout;		/* timeout value */
	int	ic_len;			/* length of data */
	char	*ic_dp;			/* pointer to data */
};

#if _SGIAPI
/*
 * Value for timeouts (ioctl, select) that denotes infinity
 */
#define INFTIM		-1
#endif  /* _SGIAPI  */

/*
 * Stream buffer structure for putmsg and getmsg system calls
 */
struct strbuf {
	int	maxlen;			/* no. of bytes in buffer */
	int	len;			/* no. of bytes returned */
	char	*buf;			/* pointer to data */
};

/* 
 * Stream I_PEEK ioctl format
 */
struct strpeek {
	struct strbuf ctlbuf;
	struct strbuf databuf;
	long	      flags;
};

/*
 * Stream I_FDINSERT ioctl format
 */
/*
 * Users running 32-bit binaries on 64-bit kernels  need to ensure that the
 * definition of the structure used in conjunction with I_FDINSERT has the
 * pointer field 64-bit aligned and 8 bytes long.
 * For backward compatibility, the kernel will attempt to fix structures
 * in place when processing I_FDINSERT in the stream head.  For an example,
 * look at the definition of the T_conn_res structure in sys/tihdr.h, which
 * has padding inserted to align the queue pointer.  The structure that the
 * user sees in a 32-bit environment will be different than in a 64-bit
 * environment.
 */
struct strfdinsert {
	struct strbuf ctlbuf;
	struct strbuf databuf;
	long	      flags;
	int	      fildes;
	int	      offset;
};

#ifdef CKPT
/*
 * Stream I_PEEKN ioctl format
 */
struct strpeekn {
	struct strbuf ctlbuf;
	struct strbuf databuf;
	long	      flags;
	unsigned int  msgnum;
};

/*
 * Stream I_NREADN ioctl format
 */
struct strreadn {
	unsigned int	msgnum;
	unsigned short	flags;
	int		datasz;
	int		ctlsz;
};
#endif /* CKPT */
/*
 * Receive file descriptor structure
 */

#if defined(_KERNEL)

struct o_strrecvfd {	/* SVR3 syscall structure */
	union {
		struct vfile *fp;
		int fd;
	} f;
	o_uid_t uid;		/* always ushort */
	o_gid_t gid;
	char fill[8];
};

/* Although EFT is enabled in the kernel we kept the following definition
** to support an EFT application on a 4.0 non-EFT system.
*/

struct e_strrecvfd {	/* SVR4 expanded syscall interface structure */
	union {
		struct vfile *fp;
		int fd;
	} f;
	uid_t uid;		/* always long */
	gid_t gid;
	char fill[8];
};

struct strrecvfd {	/* Kernel structure dependent on EFT definition */
	union {
		struct vfile *fp;
		int fd;
	} f;
	uid_t uid;
	gid_t gid;
	char fill[8];
};

#else /* !_KERNEL */

struct strrecvfd {
	int fd;
	uid_t uid;
	gid_t gid;
	char __fill[8];
};

#endif	/* defined(_KERNEL) */

#if _SGIAPI
/*
 * REMIND - I moved this here since secsys.h was purged.
 */
struct sub_attr {
	char kernel_info[1];
};

struct s_strrecvfd {
	int fd;
	uid_t uid;
	gid_t gid;
	struct sub_attr s_attrs;
};
#endif /* _SGIAPI */

/*
 * For I_LIST ioctl.
 */
struct str_mlist {
	char l_name[FMNAMESZ+1];
};

struct str_list {
	int sl_nmods;
	struct str_mlist *sl_modlist;
};

/*
 * For I_STREV and I_UNSTREV ioctls: requesting
 * ET_DRIVER-type events from STREAMS modules and
 * drivers.
 */
#ifdef GEM
struct str_event {
	long		ste_event;	/* the event (module-specific) */
	ecb_t		ste_ecb;	/* event control block */
	long		ste_evflags;	/* same as ev_flags */
	struct strbuf	ste_buf;	/* module-specific data */
};

/*
 * For I_SETEV ioctls: requesting ET_STREAM-type
 * events from the stream head.
 */
typedef struct str_sev {
	int	se_mask;	/* generate events for above S_XXX conditions */
	ecb_t	se_ecb;		/* the event control block */
	unchar	se_band;	/* band requested (ignored if does not apply) */
} str_sev_t;

/*
 * For I_GETEV ioctls: retrieve the events for
 * which stream head is generating events.
 */
typedef struct str_gev {
	int		ge_flags;	/* see below */
	str_sev_t	*ge_sep;	/* ptr to array of str_sev structs */
	int		ge_ses;		/* number of elements in ge_sep array */
} str_gev_t;


/*
 * Flags for ge_flags field.
 */
#define	GEF_PROC	0x0001	/* Get information only about	*/
				/* this process' requests.	*/
				/* Otherwise, all I_SETEV's for	*/
				/* the stream head will be	*/
				/* returned.			*/
#define	GEF_MORE	0x0002	/* Set on return if there were	*/
				/* more I_SETEVs to return than	*/
				/* would fit into the ge_sep	*/
				/* array the caller provided.	*/
#endif /* GEM */
/*
 * For I_FLUSHBAND ioctl.  Describes the priority
 * band for which the operation applies.
 */
struct bandinfo {
	unsigned char	bi_pri;
	int		bi_flag;
};

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_STROPTS_H */
