#ifndef __sys_socketvar__
#define __sys_socketvar__
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *	@(#)socketvar.h	7.3 (Berkeley) 12/30/87
 */
#ident	"$Revision: 2.98 $"

#if defined(__sgi) && (defined(_KERNEL) || defined(_KMEMUSER))
#include "sys/par.h"
#include "sys/pda.h"
#include "sys/poll.h"
#include "sys/sema.h"
#include "net/if.h"
#include "ksys/behavior.h"
#ifndef _KMEMUSER
#include "sys/mon.h"
#include "sys/strsubr.h"
#endif
#endif

#ifdef _SO_UTRACE
#include "sys/rtmon.h"
#define SO_UTRACE(name, so, ra) \
	UTRACE(RTMON_ALLOC, (name), (__int64_t)(so), \
		UTPACK((ra), (so) ? (so)->so_state : 0));
#define VSO_UTRACE(name, so, ra) \
	UTRACE(RTMON_ALLOC, (name), (__int64_t)(so), \
		UTPACK((ra), (so) ? (so)->vs_refcnt : 0));
#else
#define SO_UTRACE(name, so, ra)
#define VSO_UTRACE(name, so, ra)
#endif	/* _SO_UTRACE */

#ifdef __SO_LOCK_DEBUG
struct so_trace_rec {
	long	*addr;
	int	op;
	int	cpu;
};

#define	_N_SO_TRACE	128

#define	SO_TRACE(so, opr) { \
	int t = atomicIncWithWrap(&so->so_trace_ptr, _N_SO_TRACE); \
	(so)->sock_trace[t].addr = __return_address; \
	(so)->sock_trace[t].op = (opr); \
	(so)->sock_trace[t].cpu = cpuid(); \
};
#else
#define SO_TRACE(so, op)
#endif

#if (defined(_KERNEL) || defined(_KMEMUSER))
struct vsocket;
struct ipsec;
/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
struct socket {
	short	so_type;		/* generic type, see socket.h */
	short	so_options;		/* from socket call, see socket.h */
	short	so_linger;		/* time to linger while closing */
	pid_t	so_pgid;		/* pgrp for signals */
	caddr_t	so_pcb;			/* protocol control block */
	struct	protosw *so_proto;	/* protocol handle */
	mutex_t	so_sem;			/* protect socket struct and various */ 
					
/*
 * Variables for connection queueing.
 * Socket where accepts occur is so_head in all subsidiary sockets.
 * If so_head is 0, socket is not related to an accept.
 * For head socket so_q0 queues partially completed connections,
 * while so_q is a queue of connections ready to be accepted.
 * If a connection is aborted and it has so_head set, then
 * it has to be pulled out of either so_q0 or so_q.
 * We allow connections to queue up based on current queue lengths
 * and limit on number of queued connections for this socket.
 */
	short	so_timeo;		/* connection timeout */
	sv_t	so_timeosem;		/* sync con/discon wait/wakeup */
	struct	socket *so_head;	/* back pointer to accept socket */
	struct	socket *so_q0;		/* queue of partial connections */
	struct	socket *so_q0prev;	/* pointer to prev on q0 */
	struct	socket *so_q;		/* queue of incoming connections */
	struct	socket *so_qprev;	/* pointer to prev on q */
	short	so_q0len;		/* partials on so_q0 */
	short	so_qlimit;		/* max number queued connections */
/*
 * Reordered the order of these variables to place them at the
 * top of the sockbuf structures.  Helps keep them in the same
 * cache line as the sb_rcv variables which is good for poll()
 */
	u_int	so_oobmark;		/* chars to oob mark */
	short	so_qlen;		/* number of connections on so_q */
	u_short	so_error;		/* error affecting connection */
	int	so_state;		/* internal state flags SS_*, below */
					/* xxx huy check again to see if 
					   can go back to short */
	lock_t	so_qlock;		/* lock for accept queue */
/*
 * Variables for socket buffering.
 */
	struct	sockbuf {
		u_int	sb_cc;		/* actual chars in buffer */
		u_int	sb_hiwat;	/* max actual char count */
		struct	mbuf *sb_mb;	/* the mbuf chain */
		u_char	sb_lowat;	/* low water mark (not used yet) */
		char	sb_timeo;	/* timeout (not used yet) */
		short	sb_flags;	/* flags, see below */
		sv_t	sb_wantsem;	/* used to sync people doing sblock */
		sv_t	sb_waitsem;	/* waiting for buffer status */
	} so_rcv, so_snd;
#define	SB_LOCK		0x01		/* lock on data queue (so_rcv only) */
#define	SB_WANT		0x02		/* someone is waiting to lock */
#define	SB_WAIT		0x04		/* someone is waiting for data/space */
#define	SB_SEL		0x08		/* buffer is selected */
#define	SB_ASYNC	0x10		/* want SIGIO */
#define	SB_NOTIFY	(SB_WAIT|SB_SEL|SB_ASYNC)
#define	SB_INPUT_ACK	0x20		/* call so_input for data-less ACKs */
#define SB_LOCK_RCV	0x40		/* locked in the receive case */

	struct ipsec *so_sesmgr_data;	/* Trusted irix session manager */
	int	(*so_callout)();	/* state change function */
	caddr_t	so_callout_arg;		/* argument to function */
	mon_t	**so_monpp;		/* streams monitor lock */
	struct stdata *so_strh;		/* streams header structure */
	struct pollhead	so_ph;		/* poll head */

	int 	so_holds;		/* holds on socket to close cleanly */
	int 	so_refcnt;		/* reference count for deadlock avd. */
	bhv_desc_t	so_bhv;
	void    (*so_input)(struct socket *so, struct mbuf *m);
	void    *so_data;
#ifdef __SO_LOCK_DEBUG
	void    *so_locker;
	void    *so_unlocker;
	void    *so_lockthread;
	void    *so_unlockthread;
	int	so_trace_ptr;
	struct	so_trace_rec	sock_trace[_N_SO_TRACE];
#endif
};
#endif  /* defined(_KERNEL) || defined(_KMEMUSER) */

/*
 * Convenience macros
 */
#define	bhvtos(bhvp)	((struct socket *)(BHV_PDATA(bhvp)))

/*
 * Socket state bits.
 */
#define	SS_NOFDREF		0x001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x008	/* in process of disconnecting */
#define	SS_CANTSENDMORE		0x010	/* can't send more data to peer */
#define	SS_CANTRCVMORE		0x020	/* can't receive more data from peer */
#define	SS_RCVATMARK		0x040	/* at mark on input */
#define SS_ISBOUND		0x080	/* used only by libsocket and sockmod */

#define	SS_PRIV			0x080	/* privileged for broadcast, raw... */
#define	SS_NBIO			0x100	/* non-blocking ops */
#define	SS_ASYNC		0x200	/* async i/o notify */

#ifdef __sgi
#define	SS_CLOSED		0x400	/* PCB has been detached */
#define	SS_MBUFWAIT		0x800	/* waiting for mapped mbuf to vanish */

#define SS_ISCONFIRMING		0x1000	/* OSI only ?? */
#define SS_WANT_CALL	 	0x2000	/* call via so_callout */
#define SS_TPISOCKET	 	0x4000	/* when not on connection queue */
#define SS_CONN_IND_SENT 	0x4000	/* when on connection queue */

#define SS_CLOSING	        0x10000 /* socket is closing */
#define SS_SOFREE	        0x20000 /* socket is being dumped */
#define SS_CONNINTERRUPTED	0x40000	/* connection setup returned EINTR */

#endif /* __sgi */

#ifdef XTP
#define	SS_XTP			0x8000	/* use XTP-specific functions */
#endif

#define SS_STP                  0x80000

/*
 * Socket callout event flags (for TPI-sockets code)
 */
#define SSCO_INPUT_ARRIVED      1       /* new input has arrived */
#define SSCO_OUTPUT_AVAILABLE   2       /* output space has become available */
#define SSCO_OOB_INPUT_ARRIVED  4       /* new oob input has arrived */

/*
 * Macros for sockets and socket buffering.
 */

/* how much space is there in a socket buffer (so->so_snd or so->so_rcv) */
#ifdef __sgi
#define	sbspace(sb)	((int)((sb)->sb_hiwat - (sb)->sb_cc))
#else
#define	sbspace(sb) \
    (MIN((long)((sb)->sb_hiwat - (sb)->sb_cc),\
	 (long)((sb)->sb_mbmax - (sb)->sb_mbcnt)))
#endif /* __sgi */

/* do we have to send all at once on a socket? */
#define	sosendallatonce(so) \
    ((so)->so_proto->pr_flags & PR_ATOMIC)

/* can we read something from so? (note: just OR un-common, save branches) */
#define	soreadable(so) \
    ((so)->so_rcv.sb_cc || \
	(((so)->so_state & SS_CANTRCVMORE) | (so)->so_error))

/* can we write something to so? */
#define	sowriteable(so) \
    (sbspace(&(so)->so_snd) > 0 && \
	(((so)->so_state&SS_ISCONNECTED) || \
	  ((so)->so_proto->pr_flags&PR_CONNREQUIRED)==0) || \
     (((so)->so_state & SS_CANTSENDMORE) | (so)->so_error))

#ifdef __sgi
#define	sballoc(sb, m)	(sb)->sb_cc += (m)->m_len
#define	sbfree(sb, m)	(sb)->sb_cc -= (m)->m_len
#else

/* adjust counters in sb reflecting allocation of m */
#define	sballoc(sb, m) { \
	(sb)->sb_cc += (m)->m_len; \
	(sb)->sb_mbcnt += MSIZE; \
	if ((m)->m_off > MMAXOFF) \
		(sb)->sb_mbcnt += MCLBYTES; \
}

/* adjust counters in sb reflecting freeing of m */
#define	sbfree(sb, m) { \
	(sb)->sb_cc -= (m)->m_len; \
	(sb)->sb_mbcnt -= MSIZE; \
	if ((m)->m_off > MMAXOFF) \
		(sb)->sb_mbcnt -= MCLBYTES; \
}
#endif /* __sgi */

/* set lock on sockbuf sb */
#ifdef __sgi

#define NETSPL_DECL(s)		int s;
#define SOCKBUF_CLEAR(sb)	{ \
			(sb)->sb_cc = 0; 			\
			(sb)->sb_hiwat = 0;		\
			(sb)->sb_mb = 0;			\
			(sb)->sb_lowat = 0;		\
			(sb)->sb_timeo = 0;		\
			(sb)->sb_flags = 0;		\
}

#if defined(__SO_LOCK_DEBUG) || (_SO_UTRACE)
extern int sohold(struct socket *);
#define	SO_HOLD(so)	sohold(so)
#else
#define	SO_HOLD(so)	atomicAddInt(&(so)->so_refcnt, 1)
#endif
#define	SO_RELE(so)	sorele(so)

#define SOCKET_INITLOCKS(so)	{ \
	mutex_init(&((so)->so_sem), MUTEX_DEFAULT, "so_sem"); \
	sv_init(&((so)->so_timeosem), SV_LIFO, "so_timeo"); \
	sv_init(&((so)->so_rcv.sb_wantsem), SV_DEFAULT, "sb_rwant"); \
	sv_init(&((so)->so_rcv.sb_waitsem), SV_DEFAULT, "sb_rwait"); \
	sv_init(&((so)->so_snd.sb_wantsem), SV_DEFAULT, "sb_swant"); \
	sv_init(&((so)->so_snd.sb_waitsem), SV_DEFAULT, "sb_swait"); \
	spinlock_init(&((so)->so_qlock), "soqlock"); \
}
#define SOCKET_FREELOCKS(so)	{ \
	mutex_destroy(&(so)->so_sem); \
	sv_destroy(&(so)->so_timeosem); \
	sv_destroy(&(so)->so_rcv.sb_waitsem); \
	sv_destroy(&(so)->so_rcv.sb_wantsem); \
	sv_destroy(&(so)->so_snd.sb_waitsem); \
	sv_destroy(&(so)->so_snd.sb_wantsem); \
	spinlock_destroy(&(so)->so_qlock); \
}

#ifdef __SO_LOCK_DEBUG
extern int so_mutex_unlock(struct socket *, mutex_t *), 
so_mutex_lock(struct socket *, mutex_t *, int),
so_mutex_trylock(struct socket *, mutex_t *);

#define SOCKET_UNLOCK(so) 	so_mutex_unlock(so, &(so)->so_sem)

#define SOCKET_LOCK(so)	so_mutex_lock(so, &(so)->so_sem, PZERO-1)
#define SOCKET_TRYLOCK(so)	so_mutex_trylock(so, &(so)->so_sem)

#else

#define SOCKET_UNLOCK(so) 	mutex_unlock(&(so)->so_sem)
#define SOCKET_LOCK(so)	mutex_lock(&(so)->so_sem, PZERO-1)
#define SOCKET_TRYLOCK(so)	mutex_trylock(&(so)->so_sem)

#endif

#define SOCKET_QLOCK(so, s)	(s) = mutex_spinlock(&(so)->so_qlock)
#define SOCKET_QUNLOCK(so, s)	mutex_spinunlock(&(so)->so_qlock, (s))
#define	SOCKET_Q_ISLOCKED(so)	spinlock_islocked(&(so)->so_qlock)

#define SOCKET_PAIR_CMPLOCK(so1, so2)		socket_pair_cmplock(so1, so2)
#define SOCKET_PAIR_LOCK(so1, so2)		socket_pair_mplock(so1, so2)
#define SOCKET_PAIR_UNLOCK(so1, so2)		socket_pair_mpunlock(so1, so2)

#define SOCKET_ISLOCKED(so) (mutex_mine(&(so)->so_sem))

#define SOCKET_SOTIMEOWAKE(so) sv_broadcast(&(so)->so_timeosem)
#define SOCKET_SOTIMEOWAKEONE(so) sv_signal(&(so)->so_timeosem)
#define SOCKET_MPUNLOCK_SOTIMEO(so, pri, rv)	{ \
	ASSERT(SOCKET_ISLOCKED(so)); \
	rv = sv_wait_sig(&(so)->so_timeosem, pri, &(so)->so_sem, 0); \
}

#define SOCKET_MPUNLOCK_SBWANT(so, sb, pri)	\
		sv_wait_sig(&(sb)->sb_wantsem, pri, &(so)->so_sem, 0)
#define SOCKET_MPUNLOCK_SBWANT_NO_INTR(so, sb, pri)	\
		sv_wait(&(sb)->sb_wantsem, pri, &(so)->so_sem, 0)
#define SOCKET_MPUNLOCK_SBWAIT(so, sb, pri)	\
		sv_wait_sig(&(sb)->sb_waitsem, pri, &(so)->so_sem, 0)
#define SOCKET_MPUNLOCK_SBWAIT_NO_INTR(so, sb, pri)	\
		sv_wait(&(sb)->sb_waitsem, pri, &(so)->so_sem, 0)

#define SOCKET_WAIT_SIG(sem, pri, lock, ospl) sv_wait_sig(sem,pri,lock,ospl)

#define SOCKET_SBWANT_WAKE(sb)		sv_signal(&(sb)->sb_wantsem)
#define SOCKET_SBWANT_WAKEALL(sb)	sv_broadcast(&(sb)->sb_wantsem)
#define SOCKET_SBWAIT_WAKEALL(sb)	sv_broadcast(&(sb)->sb_waitsem)
#define SOCKET_TIMEDSLEEP_SIG(so, pri, rv, remaining) { \
		  timespec_t ts; \
		  ts.tv_sec = so->so_linger; \
		  ts.tv_nsec = 0; \
		  rv = sv_timedwait_sig(&(so)->so_timeosem, pri, \
					&(so)->so_sem, 0, 0, \
					&ts, &remaining); \
		}
				   
#define	sorwakeup(so, event) { \
	struct sockbuf *sb = &(so)->so_rcv; \
	NETPAR((sb->sb_flags & SB_WAIT) ? NETSCHED : 0, \
	       NETWAKINGTKN, (char)sb, (event), NETCNT_NULL, NETRES_SBFULL); \
	sowakeup((so), sb, POLLIN|POLLPRI|POLLRDNORM|POLLRDBAND); \
}

#define sorwakeupconn(so, event) { \
	struct sockbuf *sb = &(so)->so_rcv; \
        NETPAR((sb->sb_flags & SB_WAIT) ? NETSCHED : 0, \
               NETWAKINGTKN, (char)sb, (event), NETCNT_NULL, NETRES_SBFULL); \
        sowakeup((so), sb, POLLIN|POLLPRI|POLLRDNORM|POLLRDBAND|POLLWAKEUPONE); \
}

#define	sowwakeup(so, event) { \
	struct sockbuf *sb = &(so)->so_snd; \
	NETPAR((sb->sb_flags & SB_WAIT) ? NETSCHED : 0, \
	       NETWAKINGTKN, (char)sb, (event), NETCNT_NULL, NETRES_SBEMPTY);\
	sowakeup((so), sb, POLLOUT); \
}

#else /* __sgi */

#define sblock(sb) { \
	while ((sb)->sb_flags & SB_LOCK) { \
		(sb)->sb_flags |= SB_WANT; \
		sleep((caddr_t)&(sb)->sb_flags, PZERO+1); \
	} \
	(sb)->sb_flags |= SB_LOCK; \
}

/* release lock on sockbuf sb */
#define	sbunlock(sb) { \
	(sb)->sb_flags &= ~SB_LOCK; \
	if ((sb)->sb_flags & SB_WANT) { \
		(sb)->sb_flags &= ~SB_WANT; \
		wakeup((caddr_t)&(sb)->sb_flags); \
	} \
}

#define	sorwakeup(so)	sowakeup((so), &(so)->so_rcv)
#define	sowwakeup(so)	sowakeup((so), &(so)->so_snd)
#endif /* __sgi */

#ifdef _KERNEL
struct ipsec;
extern int	socket_pair_cmplock(struct socket *, struct socket *);
extern void	socket_pair_mplock(struct socket *, struct socket *);
extern void	socket_pair_mpunlock(struct socket *, struct socket *);
extern struct socket *sonewconn(struct socket *, struct ipsec *);
extern int socreate(int, struct socket **, int, int);
extern int soabort(struct socket *);
extern int sodisconnect(struct socket *);
extern int soconnect(struct socket *, struct mbuf *);
extern int sobind(bhv_desc_t *, struct mbuf *);
extern int solisten(bhv_desc_t *, int);
extern int soshutdown(bhv_desc_t *, int);
extern int soqremque(struct socket *, int);
extern int sosetopt(bhv_desc_t *, int, int, struct mbuf *);
extern int sogetopt(bhv_desc_t *, int, int, struct mbuf **);
extern int sorele(struct socket *);


#ifdef __sgi		/* hack to make SYSV copyin/out look like BSD's */
#define copyin(from,to,len)  bsd_copyin(from,to,len)
#define copyout(from,to,len) bsd_copyout(from,to,len)

extern int	bsd_copyout(void *,void *, int);
extern int	bsd_copyin(void *, void *, int);

struct sockaddr;
struct vfile;
struct uio;
struct vnode;

extern int	makevsock(struct vfile **, int *);
extern int	getsock(int, struct socket **);

extern void	socantsendmore(struct socket *);
extern void	socantrcvmore(struct socket *);
extern int	soclose(struct socket *);
extern int	soconnect2(bhv_desc_t *, bhv_desc_t *);
extern int	sofree(struct socket *);
extern void	sohasoutofband(struct socket *);
extern void	soisconnected(struct socket *);
extern void	soisconnecting(struct socket *);
extern void	soisdisconnected(struct socket *);
extern void	soisdisconnecting(struct socket *);
extern int	soreceive(bhv_desc_t *, struct mbuf **, struct uio *,
			  int *, struct mbuf **);
extern int	soreserve(struct socket *, u_int, u_int);
extern int	sosend(bhv_desc_t *, struct mbuf *, struct uio *,
		       int, struct mbuf *);
extern void	sowakeup(struct socket *, struct sockbuf *, int);

extern void	soinput(struct socket *, struct mbuf *);
extern void	sbappend(struct sockbuf *, struct mbuf *);
extern int	sbappendaddr(struct sockbuf *, struct sockaddr *,
			     struct mbuf *, struct mbuf *, struct ifnet *);
extern int	sbappendrights(struct sockbuf *, struct mbuf *, struct mbuf *);
extern void	sbdrop(struct sockbuf *, int);
extern void	sbflush(struct sockbuf *);
extern void	sbrelease(struct sockbuf *);
extern int	sbreserve(struct sockbuf *, u_int);
extern int	sbunlock_wait(struct sockbuf *, struct socket *, int);
extern int	sbunlock_timedwait(struct sockbuf *, struct socket *,
				   timespec_t *, timespec_t *, int);
extern int	sblock(struct sockbuf *, int, struct socket *, int);
extern void	sbunlock(struct sockbuf	*, int, struct socket *);

/*
 *      test if a socket is under a TPI stream, and if that TPI stream
 *      is not emulating a socket.
 */
#define soisatpistream(so) \
	( ((so)->so_state & (SS_TPISOCKET | SS_WANT_CALL)) == \
		(SS_TPISOCKET | SS_WANT_CALL) && \
	  (so)->so_callout_arg != NULL && \
	  ! (((struct tpisocket *) ((so)->so_callout_arg))->tso_flags & \
		TSOF_SO_IMASOCKET))

/*
 *	Page flip threshold for transmit
 */
#define	SOPFLIPMIN NBPP/2

#endif
#endif

#ifdef __cplusplus
}
#endif
#endif /* !__sys_socketvar__ */
