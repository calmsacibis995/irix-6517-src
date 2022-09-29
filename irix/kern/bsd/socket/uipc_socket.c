/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)uipc_socket.c	7.10 (Berkeley) 6/29/88
 */

#ident	"$Revision: 4.130 $"

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/domain.h"
#include "sys/errno.h"
#include "ksys/vfile.h"
#include "sys/mbuf.h"
#include "sys/proc.h"
#include "sys/ksignal.h"
#include "sys/protosw.h"
#include "sys/signal.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/strmp.h"
#include "sys/uio.h"
#include "sys/systm.h"
#include "sys/kmem.h"
#include "bstring.h"
#include "sys/sat.h" 
#include "sys/capability.h"
#include "sys/atomic_ops.h"
#include "sys/sesmgr.h"
#include "sys/t6satmp.h"
#include <sys/vsocket.h>
#include <ksys/vproc.h>

static void sorflush(struct socket *so);
extern struct pollhead *phalloc(int);
extern void phfree(struct pollhead *);
extern void sbdroprecord(struct sockbuf *sb);
static void mp_sorflush(struct socket *so);
zone_t *socket_zone;
extern void m_pcbinc(int, int);
extern void m_pcbdec(int, int);

/*
 * Lock a socket buffer sb for socket so.
 * returns 0 if successfully locked
 * if the intr flag is set, wait interruptable by signals
 *	returns EINTR if interrupted, with socket NOT locked
 * else
 *	wait ignoring signals, return 0 with the socket locked
 */
/* ARGSUSED */
int
sblock(struct sockbuf	*sb,
	int		event,
	struct socket	*so,
	int 		intr)
{
	k_sigset_t	oldset;
	int		retval;

	SOCKET_LOCK(so);
	while ((sb)->sb_flags & SB_LOCK) {
                (sb)->sb_flags |= SB_WANT;
		NETPAR(NETSCHED, NETSLEEPTKN,  NETPID_NULL, event,
                       NETCNT_NULL, NETRES_SBLOCK);
		if (intr) {
			if (intr == 2)
				hold_nonfatalsig(&oldset);
			retval = SOCKET_MPUNLOCK_SBWANT(so, sb, PZERO+1);
			if (intr == 2)
				release_nonfatalsig(&oldset);
			if (retval)
				return EINTR;
		} else  {
			SOCKET_MPUNLOCK_SBWANT_NO_INTR(so, sb, PZERO+1);
		}	
		NETPAR(NETSCHED, NETWAKEUPTKN, NETPID_NULL, event,
                       NETCNT_NULL, NETRES_SBLOCK);
                SOCKET_LOCK(so);
        }
        (sb)->sb_flags |= SB_LOCK; 
	return 0;
}

/*
 * Unlock a socket buffer sb for socket so.
 */
/* ARGSUSED */
void
sbunlock(struct sockbuf	*sb,
	int		event,
	struct socket	*so)
{
	ASSERT(SOCKET_ISLOCKED(so));
	(sb)->sb_flags &= ~SB_LOCK;
	if ((sb)->sb_flags & SB_WANT) {
		(sb)->sb_flags &= ~SB_WANT;
		NETPAR(NETSCHED, NETWAKINGTKN, NETPID_NULL, (event),
			NETCNT_NULL, NETRES_SBUNLOCK);
		SOCKET_SBWANT_WAKEALL(sb);
	}
	SOCKET_UNLOCK(so);
}


/*
 * Socket operation routines.
 * These routines are called by the routines in
 * sys_socket.c or from a system process, and
 * implement the semantics of socket operations by
 * switching out to the protocol specific routines.
 *
 * TODO:
 *	test socketpair
 *	clean up async
 *	out-of-band is a kludge
 */
/*ARGSUSED*/
socreate(int dom,
	 struct socket **aso,
	 register int type,
	 int proto)
{
	struct protosw *prp;
	struct socket *so;
	int error = 0;

	if (proto)
		prp = pffindproto(dom, proto, type, &error);
	else
		prp = pffindtype(dom, type, &error);
	if (prp == 0)
		return (error);
	if (prp->pr_type != type)
		return (EPROTOTYPE);
	so = (struct socket *)kmem_zone_zalloc(socket_zone, KM_SLEEP);
	ASSERT(so);
	m_pcbinc(sizeof(*so), MT_SOCKET);
	_SESMGR_SOCKET_INIT_ATTRS(so, get_current_cred());
	so->so_type = type;
	initpollhead(&so->so_ph);
	SOCKET_INITLOCKS(so);
	if (cap_able_cred(get_current_cred(), CAP_NETWORK_MGT))
		so->so_state |= SS_PRIV;
	so->so_proto = prp;
	so->so_input = soinput;
	so->so_data = NULL;
	so->so_refcnt = 1;
	SOCKET_LOCK(so);
	error =
	    (*prp->pr_usrreq)(so, PRU_ATTACH,
		(struct mbuf *)0, (struct mbuf *)(NULL+proto),
			      (struct mbuf *)0);
	if (error) {
		so->so_state |= SS_NOFDREF;
		if (!sofree(so))
			SOCKET_UNLOCK(so);
		return (error);
	}
	SOCKET_UNLOCK(so);
	SO_UTRACE(UTN('socr','eate'), so, __return_address);
	*aso = so;
	return (0);
}

sobind( bhv_desc_t *bdp,
	struct mbuf *nam)
{
	int error;
	register struct socket *so = bhvtos(bdp);

	SOCKET_LOCK(so);
	error =
	    (*so->so_proto->pr_usrreq)(so, PRU_BIND,
		(struct mbuf *)0, nam, (struct mbuf *)0);
	SOCKET_UNLOCK(so);
	return (error);
}

solisten(bhv_desc_t *bdp,
	 int backlog)
{
	int error;
	register struct socket *so = bhvtos(bdp);

	SOCKET_LOCK(so);
	error =
	    (*so->so_proto->pr_usrreq)(so, PRU_LISTEN,
		(struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
	if (error) {
		SOCKET_UNLOCK(so);
		return (error);
	}
	if (so->so_q == 0) {
		so->so_q = so->so_qprev = so;
		so->so_q0 = so->so_q0prev = so;
		so->so_options |= SO_ACCEPTCONN;
	}
	if (backlog < 0)
		backlog = 0;
	so->so_qlimit = MIN(backlog, SOMAXCONN);
	SOCKET_UNLOCK(so);
	return (0);
}

int
sofree(register struct socket *so)
{
	struct socket *head;
#ifdef DEBUGDISCARD
	int save_state;
#endif

	/* 
	 * Decision to dequeue and the dequeue operation must be atomic.
	 * MP protection for soqremque() 
	 */
	ASSERT(SOCKET_ISLOCKED(so));
	if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0) {
		/* soclose can't go here */
		return(0);
	}
	ASSERT(!(so->so_state & SS_SOFREE));
#ifdef DEBUGDISCARD
	save_state = so->so_state;
#endif

	SO_UTRACE(UTN('sofr','ee  '), so, __return_address);

	if (head = so->so_head) {
		register s1;
		SOCKET_LOCK(head);
		if (!soqremque(so, 0)) {
			/* synchronize with accept() */
			SOCKET_QLOCK(head, s1);
			/*
			 * We used to panic if we could not remove the
			 * connection from the list.
			 * In MP systems, it's possible that we could be
			 * removed between the check of head and the
			 * call to soqremque().  So, just proceed.
			 */
			(void) soqremque(so, 1);
			SOCKET_QUNLOCK(head, s1);
		}
		SOCKET_UNLOCK(head);
		so->so_head = 0;
	}
#ifdef DEBUG
	so->so_data = __return_address;
#endif
	return sorele(so);
}

int
sorele(struct socket *so)
{
	ASSERT(so);
	ASSERT(SOCKET_ISLOCKED(so));
#ifdef DEBUG
	so->so_data = __return_address;
#endif
	if (atomicAddInt(&so->so_refcnt, -1)) {
		return 0;
	}

	SO_UTRACE(UTN('sore','le  '), so, __return_address);
	ASSERT(so->so_head == 0);
	ASSERT(!(so->so_state & SS_SOFREE));
#if DEBUG
	so->so_state |= SS_SOFREE;
#endif
#ifdef XTP
	if (!(so->so_state & SS_XTP)) {
		sbrelease(&so->so_snd);
		mp_sorflush(so);
		SOCKET_UNLOCK(so);
	}
#else
	sbrelease(&so->so_snd);
	mp_sorflush(so);
	SOCKET_UNLOCK(so);
#endif

	ASSERT(so->so_holds == 0);
	destroypollhead(&so->so_ph);
	SOCKET_FREELOCKS(so);
/*#ifdef DEBUG*/
#if 0	/* XXX */
	{
		long *p = (long *)so;
		size_t l = sizeof(*so) / sizeof(long);
		size_t i;
		for (i = 0; i < l; p++, i++) {
			*p = (long)__return_address;
		}
	}
#endif
	_SESMGR_SOCKET_FREE_ATTRS(so);
	kmem_zone_free(socket_zone, so);
	m_pcbdec(sizeof(*so), MT_SOCKET);

#ifdef DEBUGDISCARD
	if (save_state & SS_CONN_IND_SENT) {
		printf("sofree: socket had outstanding T_CONN_IND, so_state = 0x%x\n",
			save_state);
	}
#endif
	return(1);
}

/*
 * Close a socket on last file table reference removal.
 * Initiate disconnect if connected.
 * Free socket when disconnect complete.
 */
soclose(register struct socket *so)
{
	NETSPL_DECL(s1)
	int error = 0;


	 _SESMGR_SATMP_SOCLEAR(so);
restart:
	SO_UTRACE(UTN('socl','ose1'), so, __return_address);
	SOCKET_LOCK(so);
	if (so->so_options & SO_ACCEPTCONN) {
		register struct socket *soq;

		SO_UTRACE(UTN('socl','ose3'), so, __return_address);
		so->so_state |= SS_CLOSING;
		while (so->so_q0len > 0) {
			if (((soq = so->so_q0) != so) && (soq != NULL)) {
				if (!SOCKET_TRYLOCK(soq)) {
					SO_HOLD(soq);
					SOCKET_UNLOCK(so);
					SOCKET_LOCK(soq);
					if (SO_RELE(soq) == 0) {
						SOCKET_UNLOCK(soq);
					}
					goto restart;
				}
				(void)soqremque(soq, 0);
				SOCKET_UNLOCK(so);
				error = soabort(soq);
				if (error) {
					ASSERT(error == EINVAL);
					SOCKET_UNLOCK(soq);
				}
				SOCKET_LOCK(so);
			} else {
				panic("soclose: q0 len > 0");
			}
		}
		SOCKET_UNLOCK(so);
		SOCKET_QLOCK(so, s1);
		while (so->so_qlen > 0) {
			if (((soq = so->so_q) != so) && (soq != NULL)) {
				if (!SOCKET_TRYLOCK(soq)) {
					SO_HOLD(soq);
					SOCKET_QUNLOCK(so, s1);
					SOCKET_LOCK(soq);
					if (SO_RELE(soq) == 0) {
						SOCKET_UNLOCK(soq);
					}
					goto restart;
				}
				(void)soqremque(soq, 1);
				SOCKET_QUNLOCK(so, s1);
				error = soabort(soq);
				if (error) {
					ASSERT(error == EINVAL);
					SOCKET_UNLOCK(soq);
				}
				SOCKET_QLOCK(so, s1);
			} else {
				panic("soclose: q len > 0");
			}
		}
		SOCKET_QUNLOCK(so, s1);
		SOCKET_LOCK(so);
        }

	ASSERT(SOCKET_ISLOCKED(so));
	if (so->so_pcb == 0)
		goto discard;
	if (so->so_state & SS_ISCONNECTED) {
		if ((so->so_state & SS_ISDISCONNECTING) == 0) {
			error = sodisconnect(so);
			ASSERT(SOCKET_ISLOCKED(so));
			if (error)
				goto drop;
		}
		if (so->so_options & SO_LINGER) {
			if ((so->so_state & SS_ISDISCONNECTING) &&
			    (so->so_state & SS_NBIO))
				goto drop;
			while (so->so_state & SS_ISCONNECTED) {
			        int rv;
				timespec_t remaining;

				SOCKET_TIMEDSLEEP_SIG(so, PZERO+1, rv, remaining);
				SOCKET_LOCK(so);
				if (rv != 0) {
					error = EINTR;
					break;
				} else if (remaining.tv_sec == 0)
				        break;
				  
			}
		}
	}
drop:
	SO_UTRACE(UTN('socl','ose2'), so, __return_address);
	ASSERT(SOCKET_ISLOCKED(so));
	if (so->so_pcb) {
		int error2 =
		    (*so->so_proto->pr_usrreq)(so, PRU_DETACH,
			(struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
		if (error == 0)
			error = error2;
	}
discard:
	if (so->so_state & SS_NOFDREF)
		panic("soclose: NOFDREF");
	so->so_state |= SS_NOFDREF;
	if (!sofree(so))
		SOCKET_UNLOCK(so);
	return (error);
}

/*
 * Must be called at splnet...
 */
soabort(struct socket *so)
{
	int error;

	ASSERT(SOCKET_ISLOCKED(so));
	SO_UTRACE(UTN('soab','ort '), so, __return_address);
	error =
	    (*so->so_proto->pr_usrreq)(so, PRU_ABORT,
		(struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
	return (error);
}

soaccept(register struct socket *so,
	 struct mbuf *nam)
{
	int error;

	ASSERT(SOCKET_ISLOCKED(so));
	error = (*so->so_proto->pr_usrreq)(so, PRU_ACCEPT,
	    (struct mbuf *)0, nam, (struct mbuf *)0);
	return (error);
}

soconnect(register struct socket *so,
	  struct mbuf *nam)
{
	int error;

	ASSERT(SOCKET_ISLOCKED(so));
	if (so->so_options & SO_ACCEPTCONN)
		return (EOPNOTSUPP);
	/*
	 * If protocol is connection-based, can only connect once,
         * unless a previous connection attempt was interrupted and
         * returned EINTR.  In this case, the SS_CONNINTERRUPTED flag
         * will be set and we allow the request to go down to pr_usrreq().
	 * Otherwise, if connected, try to disconnect first.
	 * This allows user to disconnect by connecting to, e.g.,
	 * a null address.
	 */
	if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING) &&
            !(so->so_state & SS_CONNINTERRUPTED) &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) ||
	    (error = sodisconnect(so))))
		error = EISCONN;
	else
		error = (*so->so_proto->pr_usrreq)(so, PRU_CONNECT,
		    (struct mbuf *)0, nam, (struct mbuf *)0);
	return (error);
}

soconnect2(bhv_desc_t *bdp1,
	bhv_desc_t *bdp2)
{
	register struct socket *so1 = bhvtos(bdp1);
	register struct socket *so2 = bhvtos(bdp2);
	int error;

	SOCKET_PAIR_LOCK(so1, so2);
	error = (*so1->so_proto->pr_usrreq)(so1, PRU_CONNECT2,
	    (struct mbuf *)0, (struct mbuf *)so2, (struct mbuf *)0);
	SOCKET_PAIR_UNLOCK(so1, so2);
	return (error);
}

sodisconnect(register struct socket *so)
{
	int error;

	ASSERT(SOCKET_ISLOCKED(so));
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto bad;
	}
	if (so->so_state & SS_ISDISCONNECTING) {
		error = EALREADY;
		goto bad;
	}
	error = (*so->so_proto->pr_usrreq)(so, PRU_DISCONNECT,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0);
bad:
	return (error);
}

static int xmitbreak = 1024;

/*
 * Send on a socket.
 * If send must go all at once and message is larger than
 * send buffering, then hard error.
 * Lock against other senders.
 * If must go all at once and not enough room now, then
 * inform user that this would block and do nothing.
 * Otherwise, if nonblocking, send as much as possible.
 *
 * When attaching the user's page, we may do lazy syncing of the tlbs
 * to avoid inter-CPU overhead. Since other CPUs may have pre-attach
 * information in their TLBs, we cannot allow the socket buffer to
 * drain until the TLBs are synced. This we do before we release
 * the lock on the socket buffer. 
 */
sosend( bhv_desc_t *bdp,
	struct mbuf *nam,
	register struct uio *uio,
	int flags,
	struct mbuf *rights)
{
	struct mbuf *top = 0;
	register struct mbuf *m, **mp;
	register int space;
	int len, rlen = 0, error = 0, dontroute, first = 1;
	int mwait;
	int needsync = 0;	/* if m_shget worked, tlbs need to be sync'd */
	register struct socket *so = bhvtos(bdp);
	int intr = 1;		/* if 1, allow sblock to be interrupted */

	NETPAR(NETSCHED,NETEVENTKN, NETPID_NULL,
	       NETEVENT_SODOWN, NETCNT_NULL, NETRES_SYSCALL);
	if (flags & _MSG_NOINTR)
		intr = 2;
	if (sosendallatonce(so) && uio->uio_resid > so->so_snd.sb_hiwat) {
		NETPAR(NETFLOW, NETDROPTKN,  NETPID_NULL,
			 NETEVENT_SODOWN, uio->uio_resid, NETRES_SBFULL);
		return (EMSGSIZE);
	}

#ifdef XTP
	if (so->so_state & SS_XTP) {
		SOCKET_LOCK(so);	/* XXX xtp_sosend needs lock? */
		error = xtp_sosend(so, nam, uio, flags, rights);
		SOCKET_UNLOCK(so);
		return (error);
	}
#endif

	mwait = (so->so_state & SS_NBIO) ? M_DONTWAIT : M_WAIT;
	dontroute =
	    (flags & MSG_DONTROUTE) && (so->so_options & SO_DONTROUTE) == 0 &&
	    (so->so_proto->pr_flags & PR_ATOMIC);
	if (!(so->so_state & SS_WANT_CALL))
		KTOP_UPDATE_CURRENT_MSGSND(1);
	if (rights)
		rlen = rights->m_len;
#define	snderr(errno)	{ error = errno; goto release; }


	if ((error = sblock(&so->so_snd, NETEVENT_SODOWN, so, intr)) != 0)
		return (error);
	do {
		ASSERT(SOCKET_ISLOCKED(so));
		if (so->so_state & SS_CANTSENDMORE)
			snderr(EPIPE);
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;			/* ??? */
			goto release;
		}
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			if (so->so_proto->pr_flags & PR_CONNREQUIRED)
				snderr(ENOTCONN);
			if (nam == 0 &&
			    (so->so_proto->pr_flags & PR_DESTADDROPT) == 0)
				snderr(EDESTADDRREQ);
		}
		if (flags & MSG_OOB)
			space = 1024;
		else {
			space = sbspace(&so->so_snd);
			if (space <= rlen ||
			   (sosendallatonce(so) &&
				space < uio->uio_resid + rlen) ||
			   (uio->uio_resid >= xmitbreak && space < xmitbreak &&
			   so->so_snd.sb_cc >= xmitbreak &&
			   (so->so_state & SS_NBIO) == 0)) {
				if (so->so_state & SS_NBIO) {
					if (first)
						error = EWOULDBLOCK;
					goto release;
				}
				/*
				 * if we've pattach'd, we can't release the
				 * lock and allow things to drain without first
				 * making sure that all the tlb's have the pages
				 * marked for copy on write. do the tlb syncing.
				 */
				if (needsync) {
					needsync = 0;
					m_sync(NULL);
				}

				NETPAR(NETSCHED, NETSLEEPTKN, (char)&so->so_snd,
					NETEVENT_SODOWN,
                                       	NETCNT_NULL, NETRES_SBFULL);
				error = sbunlock_wait(&so->so_snd, so, intr);
				NETPAR(NETSCHED, NETWAKEUPTKN,
				    (char)&so->so_snd, NETEVENT_SODOWN,
				    NETCNT_NULL, NETRES_SBFULL);
				if (error)
					return (error);
				error = sblock(&so->so_snd,
				       NETEVENT_SODOWN, so, intr);
				if (error)
					return (error);
				continue;
			}
		}

		/*
		 * The hold count keeps a socket from being closed by a
		 * protocol (on MPs).  Below the socket lock is dropped
		 * while doing uiomove and associated bookkeepping.  This
		 * is done because uiomove could fault and could wait a
		 * very long time (nfs); the protocol timeout routines
		 * become hung as they need to grab socket locks to do
		 * timeout function processing.
		 */
		so->so_holds++;
		SOCKET_UNLOCK(so);

		mp = &top;
		space -= rlen;
		while (space > 0) {
			int m_shget_dosync;
			int max_write;
			struct iovec *iov;
			iov = uio->uio_iov;
			
			len = iov->iov_len;
			if (len <= 0) {
				uio->uio_iov++;
				if (--uio->uio_iovcnt == 0) {
					ASSERT(uio->uio_resid == 0);
					break;
				}
				continue;
			}
			len = MIN(len,
				  NBPP - ((__psint_t)iov->iov_base & (NBPP-1)));
			if (flags & MSG_OOB)
				len = MIN(len, space);
			/*
			 * If we are called from a service routine of 
			 * a streams driver, then we are not in KUSEG.
			 * But in this case, do not try the page flipping.
			 * m_shget: only sync tlbs if we can flip at least
			 * three pages on this call.
			 */
			max_write = (space < uio->uio_resid) ?
				space : uio->uio_resid;
			m_shget_dosync = (max_write < NBPP*3 && needsync == 0);
			if ((len >= SOPFLIPMIN) && IS_KUSEG(iov->iov_base) &&
#if CELL_IRIX
			    (curuthread) &&
#endif
			    0 != (m = m_shget(mwait, MT_DATA, iov->iov_base,
					      len, m_shget_dosync))) {
				/* we can page-flip */
				if (!m_shget_dosync)
					needsync = 1;
				uio->uio_resid -= len;
				uio->uio_offset += len;
				iov->iov_base = (char *)iov->iov_base + len;
				if (0 == (iov->iov_len -= len)) {
					uio->uio_iov = iov+1;
					if (--uio->uio_iovcnt == 0) {
						ASSERT(uio->uio_resid == 0);
					}
				}
			} else {
				len = MIN(VCL_MAX,
					  MIN(uio->uio_resid, space));
				/*
				 * Hack for web servers:
				 *	- if we have more than one iov, and
				 *	  the second iov is large enough to
				 *	  do copy-on-write with, just uiomove()
				 *	  the first iov and then hope that
				 *	  COW works.
				 */
				if ((uio->uio_iovcnt > 1) &&
				    ((iov + 1)->iov_len >= SOPFLIPMIN)) {
					len = MIN(VCL_MAX,
						  MIN(iov->iov_len, space));
				}
				if (0 == (m = m_vget(mwait,len,MT_DATA))) {
					if (first)
					    error = EWOULDBLOCK;
					goto holdoff;
				}
				error = uiomove(mtod(m, caddr_t),
						len, UIO_WRITE, uio);
			}
			space -= len;
			*mp = m;
			if (error)
				goto holdoff;
			mp = &m->m_next;
			if (uio->uio_resid <= 0)
				break;
		}

		if (!(flags & MSG_OOB) && _SESMGR_PUT_SAMP(so, &top, nam) != 0)
			goto holdoff;

		SOCKET_LOCK(so);
		so->so_holds--;

		if (so->so_state & SS_CANTSENDMORE)
			snderr(EPIPE);
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;			/* ??? */
			goto release;
		}
		if (dontroute)
			so->so_options |= SO_DONTROUTE;
		NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
			 NETEVENT_SODOWN, len, NETRES_NULL);
		error = (*so->so_proto->pr_usrreq)(so,
		    (flags & MSG_OOB) ? PRU_SENDOOB : PRU_SEND,
		    top, nam, rights);
		if (dontroute)
			so->so_options &= ~SO_DONTROUTE;

		rights = 0;
		rlen = 0;
		top = 0;
		first = 0;
		if (error)
			break;
	} while (uio->uio_resid);

release:
	ASSERT(SOCKET_ISLOCKED(so));

	if (needsync)
		m_sync(NULL);
	sbunlock(&so->so_snd, NETEVENT_SODOWN, so);
	if (top)
		m_freem(top);

	/* Cause SIGPIPE to be sent on return */
	if (error == EPIPE)
		uio->uio_sigpipe = 1;

	NETPAR(error ? NETFLOW : 0,
	       NETDROPTKN, NETPID_NULL, NETEVENT_SODOWN,
	       uio->uio_resid, NETRES_ERROR);
	return (error);

holdoff:
	SOCKET_LOCK(so);
	so->so_holds--;
	goto release;
}

/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf containing access rights if supported
 * by the protocol, and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * When pageflipping, we may do lazy syncing of the tlbs to avoid inter-CPU
 * overhead. Since other CPUs may have the old pre-flip page in their TLBs,
 * we cannot free the old pages until the TLBs are synced. This we do before
 * we exit, or before we block waiting for more data if we're holding several
 * unfreed pages.
 *
 * Note: there are currently two versions of soreceive, soreceive() and
 * soreceive_old().  soreceive_old() is the original function inherited from
 * BSD.  It was replaced by soreceive(), which was a reorganisation of 
 * the code by Ethan Solomita to improve performance by reducing locking
 * contention.  Use of the two functions is controlled by the temporary
 * systuneable soreceive_alt.  soreceive_old() will be removed in a
 * future release.
 */

extern int soreceive_alt;

soreceive_old(bhv_desc_t *bdp,
	      struct mbuf **aname,
	      register struct uio *uio,
	      int *flagsp,
	      struct mbuf **rightsp);

soreceive(bhv_desc_t *bdp,
	  struct mbuf **aname,
	  register struct uio *uio,
	  int *flagsp,
	  struct mbuf **rightsp)
{
	register struct mbuf *m;
	register int len, error = 0;
	register u_int xfer_len, drop_len;
	register struct socket *so = bhvtos(bdp);
	struct protosw *pr = so->so_proto;
	struct mbuf *nextrecord;
	int orig_resid = uio->uio_resid;
	struct mbuf *xfer_start;
	int flags;
	int intr = 1;		/* if 1, allow sblock to be interrupted */
	int need_m_sync = 0;
	struct mbuf *lastm, *extram;

	if(!soreceive_alt) {
		#pragma mips_frequency_hint NEVER
		return soreceive_old(bdp, aname, uio, flagsp, rightsp);
	}
	NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
		 NETEVENT_SOUP, NETCNT_NULL, NETRES_SYSCALL);
	if (rightsp)
		*rightsp = 0;
	if (aname)
		*aname = 0;
	if (flagsp) {
		flags = *flagsp;
	} else {
		flags = 0;
	}
	if (flags & MSG_DONTWAIT)
		flags &= ~MSG_WAITALL;	/* conflicting options */
	if (flags & _MSG_NOINTR)
		intr = 2;
	if (flags & MSG_OOB) {
		#pragma mips_frequency_hint NEVER
		m = m_get(M_WAIT, MT_DATA);
		SOCKET_LOCK(so);
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB,
		    m, (struct mbuf *)(NULL+(flags & MSG_PEEK)),
					 (struct mbuf *)0);
		SOCKET_UNLOCK(so);
		if (error)
			goto bad;
		do {
			len = uio->uio_resid;
			if (len > m->m_len)
				len = m->m_len;
			error =
			    uiomove(mtod(m, caddr_t), (int)len, UIO_READ, uio);
			m = m_free(m);
		} while (uio->uio_resid && error == 0 && m);
bad:
		if (m)
			m_freem(m);
		NETPAR(error ? NETFLOW : 0,
		       NETDROPTKN, NETPID_NULL, NETEVENT_SOUP,
		       NETCNT_NULL, NETRES_ERROR);
		return (error);
	}
#ifdef XTP
	if (so->so_state & SS_XTP) {
		SOCKET_LOCK(so);	/* XXX is this needed? */
		error = xtp_soreceive(so, aname, uio, flags, rightsp);
		SOCKET_UNLOCK(so);
		return (error);
	}
#endif
restart:
	if ((error = sblock(&so->so_rcv, NETEVENT_SOUP, so, intr)) != 0)
		goto out_error;
	so->so_rcv.sb_flags |= SB_LOCK_RCV;

	ASSERT((signed)so->so_rcv.sb_cc >= 0);
	if (so->so_rcv.sb_cc == 0) {
		#pragma mips_frequency_hint NEVER
		if (so->so_error) {
			/* don't give error now if any data has been moved */
			if (orig_resid == uio->uio_resid) {
				error = so->so_error;
				so->so_error = 0;
			}
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE)
			goto release;
		if ((so->so_state & SS_ISCONNECTED) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
			error = EWOULDBLOCK;
			goto release;
		}
		NETPAR(NETSCHED, NETSLEEPTKN, (char)&so->so_rcv,
			 NETEVENT_SOUP, NETCNT_NULL, NETRES_SBEMPTY);
		error = sbunlock_wait(&so->so_rcv, so, intr);
		if (error)
			goto out_error;
		NETPAR(NETSCHED, NETWAKEUPTKN, (char)&so->so_rcv,
			 NETEVENT_SOUP, NETCNT_NULL, NETRES_SBEMPTY);
		goto restart;
	}
	/*
	 * For streams interface, this routine MAY be called from a 	
	 * service routine, which means NO u area, OR the wrong one.
	 */
	if (!(so->so_state & SS_WANT_CALL)) 
		KTOP_UPDATE_CURRENT_MSGRCV(1);
	m = so->so_rcv.sb_mb;
	if (m == 0)
		panic("receive 1");
	GOODMT(m->m_type);
	nextrecord = m->m_act;
	if (pr->pr_flags & PR_ADDR) {
		if (m->m_type != MT_SONAME)
			panic("receive 1a");
		if (flags & MSG_PEEK) {
			#pragma mips_frequency_hint NEVER
			if (aname)
				*aname = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			ASSERT((signed)so->so_rcv.sb_cc >= 0);
			if (aname) {
				*aname = m;
				m = m->m_next;
				(*aname)->m_next = 0;
				so->so_rcv.sb_mb = m;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
			if (m)
				m->m_act = nextrecord;
		}
	}
	if (m && m->m_type == MT_RIGHTS) {
		if ((pr->pr_flags & PR_RIGHTS) == 0)
			panic("receive 2");
		if (flags & MSG_PEEK) {
			#pragma mips_frequency_hint NEVER
			if (rightsp)
				*rightsp = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			ASSERT((signed)so->so_rcv.sb_cc >= 0);
			if (rightsp) {
				*rightsp = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
			if (m)
				m->m_act = nextrecord;
		}
	}

	/*
	 * Work out how many whole mbufs can be pulled of the socket
	 * receive buffer.  The start of this chain is put in xfer_start,
	 * xfer_len is set to the number of bytes being pulled off,
	 * lastm points to the last mbuf in this chain and extram
	 * is a pointer to an mbuf containing a copy of any bytes that 
	 * are to be read, but whose original mbuf cannot yet be
	 * pulled off the queue because other bytes in it have not yet
	 * been read.  extram is NULL if there are no such bytes.
	 */
	xfer_start = m;	/* start of chain to process post-sbunlock */
	xfer_len = drop_len = 0;
	lastm = NULL;
	extram = NULL; 		/* extra mbuf to be copyout'd and */
				/* m_freed post-sbunlock */
	if (m)
		so->so_state &= ~SS_RCVATMARK;
	while (m && uio->uio_resid > xfer_len) {
		len = uio->uio_resid - xfer_len;
		if (so->so_oobmark && len > so->so_oobmark - xfer_len)
			len = so->so_oobmark - xfer_len;

		if (len >= m->m_len) {
			xfer_len += m->m_len;
			lastm = m;
			m = m->m_next;
			
			if (so->so_oobmark == xfer_len && xfer_len)
				break;
			continue;
		} else {
			if (xfer_len == 0 && uio->uio_iovcnt == 1) {
				/* Cheat big-time -- 
				 * uiomove the last mbuf first
				 */
				error = uiomove(mtod(m, caddr_t), len, 
						UIO_READ, uio);
			} else {
				extram = m_vget(M_WAIT, len, MT_DATA);
				extram->m_len = len;
				bcopy(mtod(m, char*), mtod(extram, char*), len);
			}
			if ((flags & MSG_PEEK) == 0) {
				m->m_off += len;
				m->m_len -= len;
				drop_len += len;
			}
			break;
		}
	}
	/*
	 * Extract the above defined list of mbufs from the socket
	 * receive buffer, unless we are only peeking.
	 */
	if ((flags & MSG_PEEK) == 0) {
		drop_len += xfer_len; /* total bytes moved, now to be removed */
		if (m && (pr->pr_flags & PR_ATOMIC)) {
			/*
			 * Doing atomic reads, but there is still some
			 * data left in the rcv buffer, so truncate it.
			 */
			if (flagsp)
				*flagsp = MSG_TRUNC;
			while (m) {
				drop_len += m->m_len;
				lastm = m;
				m = m->m_next;
			}
		}
		if (lastm == NULL)
			xfer_start = NULL; /* nothing to m_freem */
		else
			lastm->m_next = NULL; /* split mbuf chain */
		if (so->so_oobmark) {
			#pragma mips_frequency_hint NEVER
			if (so->so_oobmark > drop_len)
				so->so_oobmark -= drop_len;
			else {
				so->so_oobmark = 0;
				so->so_state |= SS_RCVATMARK;
			}
		}
		so->so_rcv.sb_cc -= drop_len;
		ASSERT((signed)so->so_rcv.sb_cc >= 0);
		if (m == 0)
			so->so_rcv.sb_mb = nextrecord;
		else {
			m->m_act = nextrecord;
			so->so_rcv.sb_mb = m;
		}
		/*
		 * If the underlying protocol wants to be notified of
		 * a socket read, call the user-request function (this
		 * is the case with TCP, for example, to generate ACKs.
		 */
		if ((pr->pr_flags & PR_WANTRCVD) && so->so_pcb)
			(*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
			    (struct mbuf *)0, (struct mbuf *)0);

		if (error == 0 && rightsp && *rightsp &&
		    pr->pr_domain->dom_externalize) {
			error = (*pr->pr_domain->dom_externalize)(*rightsp);
			orig_resid = uio->uio_resid;	/* keep this error */
		}
		sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	} else {
		#pragma mips_frequency_hint NEVER
		so->so_holds++;
		SOCKET_UNLOCK(so);
	}
	NETPAR(error ? NETFLOW : 0,
	       NETDROPTKN, NETPID_NULL,
	       NETEVENT_SOUP, NETCNT_NULL, NETRES_ERROR);
	/*
	 * Traverse the list of mbufs that can be extracted from
	 * the socket buffer, page-flipping the data if possible.
	 */
	m = xfer_start;
	while (xfer_len && error == 0) {
		struct iovec *iov;
		int m_flip_dosync;
		int maxread;
		size_t curlen;

		curlen = m->m_len;
		ASSERT(xfer_len >= curlen);
		iov = uio->uio_iov;

		ASSERT(uio->uio_iovcnt >= 1);
#ifndef XTP				/* allow XTP strange types */
		ASSERT(m->m_type == MT_DATA || m->m_type == MT_HEADER);
#endif /* XTP */
		NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
		       NETEVENT_SOUP, (int)curlen, NETRES_NULL);

		/* page-flip if we can - but only if data in KUSEG. */
		/* m_flip does tlb sync only if we can flip at least
		 * three pages on this call */

		maxread = uio->uio_resid;
		if ((flags & MSG_WAITALL) == 0 && xfer_len < maxread)
			maxread = xfer_len;
		m_flip_dosync = (maxread < NBPP*3 && need_m_sync == 0);
		if (IS_KUSEG(iov->iov_base)
#if CELL_IRIX
		    && (curuthread)
#endif
		    && curlen == NBPP
		    && iov->iov_len >= NBPP
		    && 0 == ((__psint_t)iov->iov_base & (NBPP-1))
		    && !(flags & MSG_PEEK)
		    && m_flip(m, iov->iov_base, m_flip_dosync)) {
			need_m_sync = !m_flip_dosync;
			uio->uio_resid -= curlen;
			uio->uio_offset += curlen;
			iov->iov_base = (char *)iov->iov_base + curlen;
			if (0 == (iov->iov_len -= curlen)) {
				uio->uio_iov = iov+1;
				if (--uio->uio_iovcnt == 0) {
					ASSERT(uio->uio_resid == 0);
				}
			}
		} else {
#ifdef DEBUG
		    if (curlen == 0 ) {
			printf("Zero length mbuf in soreceive()\n");
		    } else
#endif
			error = uiomove(mtod(m, caddr_t), curlen, UIO_READ, uio);
		}
		xfer_len -= curlen;
		m = m->m_next;
	}
	if (extram) {
		if (error == 0)
			error = uiomove(mtod(extram, caddr_t), extram->m_len, 
					UIO_READ, uio);
		m_free(extram);
	}
	if (flags & MSG_PEEK) {
		#pragma mips_frequency_hint NEVER
		SOCKET_LOCK(so);
		so->so_holds--;
		sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	} else {
		if (need_m_sync)
			m_sync(xfer_start);
		else if (xfer_start)
			m_freem(xfer_start);
		if ((flags & MSG_WAITALL) && uio->uio_resid > 0 &&
		    error == 0 && nextrecord == NULL)
			goto restart;
	}
out_error:
	/* if data has been moved, return it, give the error next time */

	if (orig_resid > uio->uio_resid)
		return 0;
	else
		return (error);
release:
	ASSERT(SOCKET_ISLOCKED(so));
	sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	NETPAR(error ? NETFLOW : 0,
	       NETDROPTKN, NETPID_NULL,
	       NETEVENT_SOUP, NETCNT_NULL, NETRES_ERROR);
	goto out_error;
}

soreceive_old(bhv_desc_t *bdp,
	      struct mbuf **aname,
	      register struct uio *uio,
	      int *flagsp,
	      struct mbuf **rightsp)
{
	register struct mbuf *m;
	register int len, error = 0, offset;
	register struct socket *so = bhvtos(bdp);
	struct protosw *pr = so->so_proto;
	struct mbuf *nextrecord;
	struct mbuf *unfreed_chain = NULL;
	int unfreed_count = 0;
	int mflipped = 0;
	int orig_resid = uio->uio_resid;
	int moff;
	int flags;
	int intr = 1;		/* if 1, allow sblock to be interrupted */

	NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
		 NETEVENT_SOUP, NETCNT_NULL, NETRES_SYSCALL);
	if (rightsp)
		*rightsp = 0;
	if (aname)
		*aname = 0;
	if (flagsp) {
		flags = *flagsp;
	} else {
		flags = 0;
	}
	if (flags & MSG_DONTWAIT)
		flags &= ~MSG_WAITALL;	/* conflicting options */
	if (flags & _MSG_NOINTR)
		intr = 2;
	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		SOCKET_LOCK(so);
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB,
		    m, (struct mbuf *)(NULL+(flags & MSG_PEEK)),
					 (struct mbuf *)0);
		SOCKET_UNLOCK(so);
		if (error)
			goto bad;
		do {
			len = uio->uio_resid;
			if (len > m->m_len)
				len = m->m_len;
			error =
			    uiomove(mtod(m, caddr_t), (int)len, UIO_READ, uio);
			m = m_free(m);
		} while (uio->uio_resid && error == 0 && m);
bad:
		if (m)
			m_freem(m);
		NETPAR(error ? NETFLOW : 0,
		       NETDROPTKN, NETPID_NULL, NETEVENT_SOUP,
		       NETCNT_NULL, NETRES_ERROR);
		return (error);
	}
#ifdef XTP
	if (so->so_state & SS_XTP) {
		SOCKET_LOCK(so);	/* XXX is this needed? */
		error = xtp_soreceive(so, aname, uio, flags, rightsp);
		SOCKET_UNLOCK(so);
		return (error);
	}
#endif

	if ((error = sblock(&so->so_rcv, NETEVENT_SOUP, so, intr)) != 0)
		return (error);
restart:
	ASSERT((signed)so->so_rcv.sb_cc >= 0);
	if (so->so_rcv.sb_cc == 0) {
		if (so->so_error) {
			/* don't give error now if any data has been moved */
			if (orig_resid == uio->uio_resid) {
				error = so->so_error;
				so->so_error = 0;
			}
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE)
			goto release;
		if ((so->so_state & SS_ISCONNECTED) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
			error = EWOULDBLOCK;
			goto release;
		}
		/* don't hold a lot of memory if we're blocking */
		if (unfreed_count >= 16) {	/* ethan: arbitrary value */
			m_sync(unfreed_chain);
			unfreed_chain = NULL;
			unfreed_count = 0;
		}
		NETPAR(NETSCHED, NETSLEEPTKN, (char)&so->so_rcv,
			 NETEVENT_SOUP, NETCNT_NULL, NETRES_SBEMPTY);
		error = sbunlock_wait(&so->so_rcv, so, intr);
		if (error)
			goto out_error;
		NETPAR(NETSCHED, NETWAKEUPTKN, (char)&so->so_rcv,
			 NETEVENT_SOUP, NETCNT_NULL, NETRES_SBEMPTY);
		error = sblock(&so->so_rcv, NETEVENT_SOUP, so, intr);
		if (error)
			goto out_error;
		goto restart;
	}
	/*
	 * For streams interface, this routine MAY be called from a 	
	 * service routine, which means NO u area, OR the wrong one.
	 */
	if (!(so->so_state & SS_WANT_CALL)) 
		KTOP_UPDATE_CURRENT_MSGRCV(1);
	m = so->so_rcv.sb_mb;
	if (m == 0)
		panic("receive 1");
	GOODMT(m->m_type);
	nextrecord = m->m_act;
	if (pr->pr_flags & PR_ADDR) {
		if (m->m_type != MT_SONAME)
			panic("receive 1a");
		if (flags & MSG_PEEK) {
			if (aname)
				*aname = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			ASSERT((signed)so->so_rcv.sb_cc >= 0);
			if (aname) {
				*aname = m;
				m = m->m_next;
				(*aname)->m_next = 0;
				so->so_rcv.sb_mb = m;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
			if (m)
				m->m_act = nextrecord;
		}
	}
	if (m && m->m_type == MT_RIGHTS) {
		if ((pr->pr_flags & PR_RIGHTS) == 0)
			panic("receive 2");
		if (flags & MSG_PEEK) {
			if (rightsp)
				*rightsp = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			ASSERT((signed)so->so_rcv.sb_cc >= 0);
			if (rightsp) {
				*rightsp = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
			if (m)
				m->m_act = nextrecord;
		}
	}
	moff = 0;
	offset = 0;
	while (m && uio->uio_resid > 0 && error == 0) {
		struct iovec *iov;
		int m_flip_dosync;
		int maxread;

		len = uio->uio_resid;
		iov = uio->uio_iov;

		ASSERT(uio->uio_iovcnt >= 1);
#ifndef XTP				/* allow XTP strange types */
		ASSERT(m->m_type == MT_DATA || m->m_type == MT_HEADER);
#endif /* XTP */
		so->so_state &= ~SS_RCVATMARK;
		if (so->so_oobmark && len > so->so_oobmark - offset)
			len = so->so_oobmark - offset;
		/*
		 * the holdcount keeps a socket from being closed by a protocol
		 * while executing the loop below
		 */
		so->so_holds++;
		SOCKET_UNLOCK(so);

		if (len > m->m_len - moff)
			len = m->m_len - moff;
		NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
		       NETEVENT_SOUP, (int)len, NETRES_NULL);

		/* page-flip if we can - but only if data in KUSEG. */
		/* m_flip does tlb sync only if we can flip at least
		 * three pages on this call */

		maxread = uio->uio_resid;
		if ((flags & MSG_WAITALL) == 0 && so->so_rcv.sb_cc < maxread)
			maxread = so->so_rcv.sb_cc;
		m_flip_dosync = (maxread < NBPP*3 && unfreed_chain == NULL);
		if (IS_KUSEG(iov->iov_base)
#if CELL_IRIX
		    && (curuthread)
#endif
		    && len == NBPP
		    && iov->iov_len >= NBPP
		    && 0 == ((__psint_t)iov->iov_base & (NBPP-1))
		    && moff == 0
		    && !(flags & MSG_PEEK)
		    && m_flip(m, iov->iov_base, m_flip_dosync)) {
			if (!m_flip_dosync)
				mflipped = 1;
			uio->uio_resid -= len;
			uio->uio_offset += len;
			iov->iov_base = (char *)iov->iov_base + len;
			if (0 == (iov->iov_len -= len)) {
				uio->uio_iov = iov+1;
				if (--uio->uio_iovcnt == 0) {
					ASSERT(uio->uio_resid == 0);
				}
			}
		} else {
#ifdef DEBUG
		    if (len == 0 ) {
			printf("Zero length mbuf in soreceive()\n");
		    } else
#endif
			error = uiomove(mtod(m, caddr_t) + moff,
					len, UIO_READ, uio);
		}
		ASSERT(len <= m->m_len - moff); /* m_len must not shrink */

		SOCKET_LOCK(so);
		so->so_holds--;

		if (len == m->m_len - moff) {
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_act;
				sbfree(&so->so_rcv, m);
				ASSERT((signed)so->so_rcv.sb_cc >= 0);
				if (mflipped) {
					so->so_rcv.sb_mb = m->m_next;
					m->m_next = unfreed_chain;
					unfreed_chain = m;
					unfreed_count++;
					mflipped = 0;
				} else {
					MFREE(m, so->so_rcv.sb_mb);
				}
				m = so->so_rcv.sb_mb;
				if (m)
					m->m_act = nextrecord;
			}
		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				m->m_off += len;
				m->m_len -= len;
				so->so_rcv.sb_cc -= len;
				ASSERT((signed)so->so_rcv.sb_cc >= 0);
			}
		}
		ASSERT(mflipped == 0);
		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
					so->so_state |= SS_RCVATMARK;
					break;
				}
			} else
				offset += len;
		}
	}
	if (m && (pr->pr_flags & PR_ATOMIC) && flagsp) {
		*flagsp = MSG_TRUNC;
	}
	if ((flags & MSG_PEEK) == 0) {
		if (m == 0)
			so->so_rcv.sb_mb = nextrecord;
		else if (pr->pr_flags & PR_ATOMIC)
			(void) sbdroprecord(&so->so_rcv);
		if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
			(*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
			    (struct mbuf *)0, (struct mbuf *)0);
		if (error == 0 && rightsp && *rightsp &&
		    pr->pr_domain->dom_externalize) {
			error = (*pr->pr_domain->dom_externalize)(*rightsp);
			orig_resid = uio->uio_resid;	/* keep this error */
		}
	}

	if ((flags & MSG_WAITALL) && uio->uio_resid > 0 && error == 0)
		goto restart;
release:
	ASSERT(SOCKET_ISLOCKED(so));
	sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	NETPAR(error ? NETFLOW : 0,
	       NETDROPTKN, NETPID_NULL,
	       NETEVENT_SOUP, NETCNT_NULL, NETRES_ERROR);
out_error:
	if (unfreed_chain)
		m_sync(unfreed_chain);

	/* if data has been moved, return it, give the error next time */

	if (orig_resid > uio->uio_resid)
		return 0;
	else
		return (error);
}

int
soshutdown(bhv_desc_t *bdp,
	   register int how)
{
	register struct socket *so = bhvtos(bdp);
	register struct protosw *pr = so->so_proto;

	if (!(so->so_state & SS_ISCONNECTED))
	  return ENOTCONN;
	if (how < 0 || (++how & ~(0x3)) != 0)
	  return EINVAL;
	if (how & FREAD)
#ifdef XTP
		if (!(so->so_state & SS_XTP))
			sorflush(so);
#else
		sorflush(so);
#endif
	if (how & FWRITE) {
		int rv;

		SOCKET_LOCK(so);
		rv =  ((*pr->pr_usrreq)(so, PRU_SHUTDOWN,
		    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0));
		SOCKET_UNLOCK(so);
		return(rv);
	}

	return (0);
}

static void
sorflush(struct socket *so)
{
	register struct sockbuf *sb = &so->so_rcv;
	register struct protosw *pr = so->so_proto;
	struct sockbuf asb;

	sblock(sb, NETEVENT_SOUP, so, 0);
	socantrcvmore(so);
	sbunlock(sb, NETEVENT_SOUP, so);
	asb = *sb;
	SOCKBUF_CLEAR(sb);
	if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
		(*pr->pr_domain->dom_dispose)(asb.sb_mb);
	sbrelease(&asb);
}

static void
mp_sorflush(struct socket *so)
{
	register struct sockbuf *sb = &so->so_rcv;
        register struct protosw *pr = so->so_proto;
        struct sockbuf asb;

	ASSERT(SOCKET_ISLOCKED(so));
        socantrcvmore(so);
        asb = *sb;
        SOCKBUF_CLEAR(sb);
        if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
                (*pr->pr_domain->dom_dispose)(asb.sb_mb);
        sbrelease(&asb);
}


sosetopt(bhv_desc_t *bdp,
	 int level,
	 int optname,
	 struct mbuf *m0)
{
	int error = 0;
	register struct mbuf *m = m0;
	register struct socket *so = bhvtos(bdp);

	SOCKET_LOCK(so);
	if ((so->so_state & SS_CANTRCVMORE) &&
	    (so->so_state & SS_CANTSENDMORE)) {
		error = EINVAL;
		goto bad;
	}
	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput) {
			int rv;

			rv = ((*so->so_proto->pr_ctloutput)
				  (PRCO_SETOPT, so, level, optname, &m0));
			SOCKET_UNLOCK(so);
			return(rv);
		}
		error = ENOPROTOOPT;
	} else {
		switch (optname) {

		case SO_LINGER:
			if (m == NULL || m->m_len != sizeof (struct linger)) {
				error = EINVAL;
				goto bad;
			}
			so->so_linger = mtod(m, struct linger *)->l_linger;
			/* fall thru... */

		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_USELOOPBACK:
		case SO_BROADCAST:
		case SO_REUSEADDR:
		case SO_OOBINLINE:
		case SO_REUSEPORT:
		case SO_PASSIFNAME:
		case SO_XOPEN:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				goto bad;
			}
			if (*mtod(m, int *))
				so->so_options |= optname;
			else
				so->so_options &= ~optname;
			break;

		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				goto bad;
			}
			switch (optname) {

			case SO_SNDBUF:
				if ((int)*mtod(m,int *) == 0) {
					error = EINVAL;
					goto bad;
				}
			case SO_RCVBUF:
				sbreserve(optname == SO_SNDBUF ?
				    &so->so_snd : &so->so_rcv,
				    (__psunsigned_t) *mtod(m, int *));
				break;

			case SO_SNDLOWAT:
				so->so_snd.sb_lowat = *mtod(m, int *);
				break;
			case SO_RCVLOWAT:
				so->so_rcv.sb_lowat = *mtod(m, int *);
				break;
			case SO_SNDTIMEO:
				so->so_snd.sb_timeo = *mtod(m, int *);
				break;
			case SO_RCVTIMEO:
				so->so_rcv.sb_timeo = *mtod(m, int *);
				break;
			}
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
	}
bad:
	SOCKET_UNLOCK(so);
	if (m)
		(void) m_free(m);
	return (error);
}

sogetopt(bhv_desc_t *bdp,
	 int level,
	 int optname,
	 struct mbuf **mp)
{
	register struct mbuf *m;
	register struct socket *so = bhvtos(bdp);
	int error = 0;

	SOCKET_LOCK(so);
	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput) {
			error = ((*so->so_proto->pr_ctloutput)
				  (PRCO_GETOPT, so, level, optname, mp));
		} else 
			error = ENOPROTOOPT;
	} else {
	    if (!(so->so_state & SS_TPISOCKET) || (m = *mp) == NULL)
		m = m_get(M_WAIT, MT_SOOPTS);
		m->m_len = sizeof (int);

		switch (optname) {

		case SO_LINGER:
			m->m_len = sizeof (struct linger);
			mtod(m, struct linger *)->l_onoff =
				so->so_options & SO_LINGER;
			mtod(m, struct linger *)->l_linger = 
				(ushort)so->so_linger;
			break;

		case SO_USELOOPBACK:
		case SO_DONTROUTE:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_REUSEADDR:
		case SO_BROADCAST:
		case SO_OOBINLINE:
		case SO_REUSEPORT:
		case SO_PASSIFNAME:
		case SO_ACCEPTCONN:
			*mtod(m, int *) = so->so_options & optname;
			break;

		case SO_TYPE:
			*mtod(m, int *) = so->so_type;
			break;

		case SO_ERROR:
			*mtod(m, int *) = so->so_error;
			so->so_error = 0;
			break;

		case SO_SNDBUF:
			*mtod(m, int *) = so->so_snd.sb_hiwat;
			break;

		case SO_RCVBUF:
			*mtod(m, int *) = so->so_rcv.sb_hiwat;
			break;

		case SO_SNDLOWAT:
			*mtod(m, int *) = so->so_snd.sb_lowat;
			break;

		case SO_RCVLOWAT:
			*mtod(m, int *) = so->so_rcv.sb_lowat;
			break;

		case SO_SNDTIMEO:
			*mtod(m, int *) = so->so_snd.sb_timeo;
			break;

		case SO_RCVTIMEO:
			*mtod(m, int *) = so->so_rcv.sb_timeo;
			break;

		case SO_PROTOCOL:
			if (so->so_proto)
				*mtod(m, int *) = so->so_proto->pr_protocol;
			else
				*mtod(m, int *) = -1;
			break;

		case SO_BACKLOG:
			*mtod(m, int *) = so->so_qlimit;
			break;

		default:
		    if (!(so->so_state & SS_TPISOCKET) || *mp == NULL)
			(void)m_free(m);
			error = ENOPROTOOPT;
			goto out;
		}
	    if (!(so->so_state & SS_TPISOCKET) || *mp == NULL)
		*mp = m;
	}
out:
	SOCKET_UNLOCK(so);
	return (error);
}

void
sohasoutofband(register struct socket *so)
{
	ASSERT(SOCKET_ISLOCKED(so));
	if (so->so_state & SS_WANT_CALL) {
		if (so->so_callout) {
			ASSERT(so->so_monpp);
			mp_streams_interrupt(so->so_monpp,
				  0,
				  (void(*)())so->so_callout, so,
				  (void *)(NULL+SSCO_OOB_INPUT_ARRIVED),
				  so->so_callout_arg);
		}
	}
	else if (so->so_pgid < 0)
		signal(-so->so_pgid, SIGURG);
	else if (so->so_pgid > 0)
		sigtopid(so->so_pgid, SIGURG, SIG_ISKERN, 0, 0, 0);
	pollwakeup(&so->so_ph, POLLIN|POLLPRI|POLLRDNORM|POLLRDBAND);
}
