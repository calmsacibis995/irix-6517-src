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
 *	@(#)uipc_usrreq.c	7.7 (Berkeley) 6/29/88
 */

/*
 * A lot of this file has been re-written as part of the 6.4 locking changes.
 * This was done in order to avoid the qswtch() calls that were previously
 * used to avoid deadlock conditions.  qswtch() is problematic on SP systems
 * with real-time threads; it is just as likely to end up running the thread
 * that's trying to relinquish as it is the desired thread.
 *
 * To work around this, we have devised a complicated system involving dropping
 * the original lock and using the standard mutex priority inheritance feature
 * to ensure that we will get to run again.  In some cases, this required
 * preventing the underlying socket memory from getting freed.  To this end,
 * if you compare this code with Stevens Vol. 3, you'll notice that:
 * 	- we now have reference counts on unpcb memory
 *	- we now have reference counts on the socket structures
 *	- there's a hack in unp_detach() that prevents datagram sockets from
 *	  being prematurely dropped; it detects the ECONNRESET error on a DG
 *	  socket and avoids releasing the socket resources.
 *
 * The UNIX-domain code is fraught with deadlock problems due to the fact that
 * it has two sockets that are directly connected to each other.  The algorithm
 * used in the locking utility routines is to lock the one with the lowest
 * memory address first.  However, if that won't work, then we attempt to
 * avoid deadlock by using mutex_trylock() and failing if we can't get the
 * second lock.  Because this may involve the first lock being released for
 * some period of time, callers of socket_pair_cmplock() must be prepared for
 * the contents of their underlying data structures to have changed in
 * fundamental ways.  Reference counts should keep the memory valid long enough
 * to detect and recover from errors, usually by failing the operation.
 *
 * -- Steve Alexander, 10/6/96
 */
#include <tcp-param.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/domain.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vsocket.h>

#include <sys/sesmgr.h>
#include <sys/mac_label.h>

extern	mac_label *mac_equal_equal_lp;

/*
 * Forward References.
 */
int unp_attach(struct socket *);
int unp_bind(struct unpcb *, struct mbuf *);
int unp_connect(struct socket *, struct mbuf *);
int unp_connect2(struct socket *, struct socket *);
int unp_detach(struct unpcb *);
void unp_discard(struct vfile *);
void unp_disconnect(struct unpcb *);
static int unp_drop(struct unpcb *, int);
static int unp_freespc(struct unpcb *);
void unp_gc(void);
int unp_internalize(struct mbuf *);
int unp_rele(struct unpcb *);
void unp_usrclosed(struct unpcb *);
void unp_scan(struct mbuf *, void (*)(struct vfile *));
int uipc_three_way_lock(struct socket *, struct socket *);


/*
 * Unix communications domain.
 *
 * TODO:
 *	SEQPACKET, RDM
 *	rethink name space problems
 *	need a proper out-of-band
 */
struct	sockaddr sun_noname = { {{ AF_UNIX }} };
ino_t	unp_ino;			/* prototype for fake inode numbers */

mrlock_t	unp_lock;

#define UNP_MPRDLOCK()		mraccess(&unp_lock)
#define UNP_MPWRLOCK()		mrupdate(&unp_lock)
#define UNP_MPUNLOCK()		mrunlock(&unp_lock)
#define UNP_PROTECT(code)	{  \
	UNP_MPWRLOCK(); code; UNP_MPUNLOCK(); }

struct unpcb unpcb_list;

int
uipc_usrreq(struct socket *so,
	    int req,
	    struct mbuf *m,
	    struct mbuf *nam,
	    struct mbuf *rights)
{
	struct unpcb *unp = sotounpcb(so);
	register struct socket *so2;
	int error = 0;

	ASSERT(SOCKET_ISLOCKED(so));
	if (req == PRU_CONTROL)
		return (EOPNOTSUPP);
	if (req != PRU_SEND && rights && rights->m_len) {
		error = EOPNOTSUPP;
		goto release;
	}

	if (unp == 0 && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	switch (req) {

	case PRU_ATTACH:
		if (unp) {
			error = EISCONN;
			break;
		}
		error = unp_attach(so);
		break;

	case PRU_DETACH:
		(void)unp_detach(unp);
		break;

	case PRU_BIND:
		error = unp_bind(unp, nam);
		break;

	case PRU_LISTEN:
	        if (so->so_type == SOCK_DGRAM) {
		        error = EOPNOTSUPP;
			break;
		}
		if (unp->unp_vnode == 0)
			error = EINVAL;
		break;

	case PRU_CONNECT:
		error = unp_connect(so, nam);
		break;

	case PRU_CONNECT2:
		ASSERT(SOCKET_ISLOCKED((struct socket *)nam));
		error = unp_connect2(so, (struct socket *)nam);
		break;

	case PRU_DISCONNECT:
		unp_disconnect(unp);
		break;

	case PRU_ACCEPT:
		/*
		 * Pass back name of connected socket,
		 * if it was bound and we are still connected
		 * (our peer may have closed already!).
		 */
		if (unp->unp_conn && unp->unp_conn->unp_addr) {
			nam->m_len = unp->unp_conn->unp_addr->m_len;
			bcopy(mtod(unp->unp_conn->unp_addr, caddr_t),
			    mtod(nam, caddr_t), (unsigned)nam->m_len);
		} else {
			nam->m_len = sizeof(sun_noname);
			*(mtod(nam, struct sockaddr *)) = sun_noname;
		}
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		unp_usrclosed(unp);
		break;

	case PRU_RCVD:
		switch (so->so_type) {

		case SOCK_DGRAM:
			panic("uipc 1");
			/*NOTREACHED*/

		case SOCK_STREAM:
#define	rcv (&so->so_rcv)
#define snd (&so2->so_snd)
rcv_strloop:
			if (unp->unp_conn == 0) {
				break;
			}
			so2 = unp->unp_conn->unp_socket;
		        if (so2->so_state & SS_CLOSING) {
				break;
			}
			if (SOCKET_PAIR_CMPLOCK(so, so2) == 0) 
				goto rcv_strloop;
			ASSERT(SOCKET_ISLOCKED(so2));		
			/*
			 * Adjust backpressure on sender
			 * and wakeup any waiting to write.
			 */
			snd->sb_hiwat += unp->unp_cc - rcv->sb_cc;
			unp->unp_cc = rcv->sb_cc;
			sowwakeup(so2, NETEVENT_SODOWN);
			if (so2 != so)
				SOCKET_UNLOCK(so2);	
#undef snd
#undef rcv
			break;

		default:
			panic("uipc 2");
		}
		break;

	case PRU_SEND:
		if (rights) {
			error = unp_internalize(rights);
			if (error)
				break;
		}
		switch (so->so_type) {

		case SOCK_DGRAM: {
			struct sockaddr *from;

			if (nam) {
				if (unp->unp_conn) {
					error = EISCONN;
					break;
				}
				error = unp_connect(so, nam);
				if (error)
					break;
			} else {
				if (unp->unp_conn == 0) {
					error = ENOTCONN;
					break;
				}
			}
snd_dloop: 
			so2 = unp->unp_conn->unp_socket;
			if (so2->so_state & SS_CLOSING) {
				/* quick hack */
				error = ENOTCONN;
				break;
			}
			if (SOCKET_PAIR_CMPLOCK(so, so2) == 0) {
				if ((unp->unp_conn == 0) ||
					(so2->so_state & SS_CLOSING)) {
					error = ENOTCONN;
                                        break;
                                }
				goto snd_dloop;
			}
			ASSERT(SOCKET_ISLOCKED(so2));

			if (unp->unp_addr) 
				from = mtod(unp->unp_addr, struct sockaddr *);
			else 
				from = &sun_noname;

			if (sbspace(&so2->so_rcv) > 0 &&
			    sbappendaddr(&so2->so_rcv, from, 
					 m, rights, 0)) {
				sorwakeup(so2, NETEVENT_SOUP);
				m = 0;
			} else
				error = ENOBUFS;
			if (so2 != so)
				SOCKET_UNLOCK(so2);
			if (nam)
				unp_disconnect(unp);
			break;
		}

		case SOCK_STREAM:
#define	rcv (&so2->so_rcv)
#define	snd (&so->so_snd)
			if (so->so_state & SS_CANTSENDMORE) {
				error = EPIPE;
				break;
			}
			if (unp->unp_conn == 0) {
				error = ENOTCONN;
				break;
			}
snd_strloop:
			so2 = unp->unp_conn->unp_socket;
			if (so2->so_state & SS_CLOSING) {
				error = ENOTCONN;
				break;
			}
			if (SOCKET_PAIR_CMPLOCK(so, so2) == 0) {
				if (unp->unp_conn == 0) {
					error = ENOTCONN;
					break;
				}
				goto snd_strloop;
			}
			/*
			 * Send to paired receive port, and then reduce
			 * send buffer hiwater marks to maintain backpressure.
			 * Wake up readers.
			 */
			if (rights)
				(void)sbappendrights(rcv, m, rights);
			else
				sbappend(rcv, m);
			snd->sb_hiwat -= rcv->sb_cc - unp->unp_conn->unp_cc;
			unp->unp_conn->unp_cc = rcv->sb_cc;
			sorwakeup(so2, NETEVENT_SOUP);
			if (so2 != so)
				SOCKET_UNLOCK(so2);
			m = 0;
#undef snd
#undef rcv
			break;

		default:
			panic("uipc 4");
		}
		break;

	case PRU_ABORT:
		if (unp_drop(unp, ECONNABORTED) == 0) {
			/*
			 * soabort() is expected to return with the socket
			 * unlocked if there was no error.
			 * We can't dereference unp past this point, though.
			 */
			SOCKET_UNLOCK(so);
		}
		break;

	case PRU_SENSE:
#define vap ((struct vattr *) m)
		vap->va_blksize = so->so_snd.sb_hiwat;
		if (so->so_type == SOCK_STREAM && unp->unp_conn != 0) {
			so2 = unp->unp_conn->unp_socket;
			vap->va_blksize += so2->so_rcv.sb_cc;
		}
		vap->va_fsid = NODEV;
		if (unp->unp_ino == 0)
			unp->unp_ino = ++unp_ino ? unp_ino : ++unp_ino;
		vap->va_nodeid = unp->unp_ino;
#undef vap
		return (0);

	case PRU_RCVOOB:
		return (EOPNOTSUPP);

	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		if (unp->unp_addr) {
			nam->m_len = unp->unp_addr->m_len;
			bcopy(mtod(unp->unp_addr, caddr_t),
			    mtod(nam, caddr_t), (unsigned)nam->m_len);
		} else
			nam->m_len = 0;
		break;

	case PRU_PEERADDR:
		if (unp->unp_conn && unp->unp_conn->unp_addr) {
			nam->m_len = unp->unp_conn->unp_addr->m_len;
			bcopy(mtod(unp->unp_conn->unp_addr, caddr_t),
			    mtod(nam, caddr_t), (unsigned)nam->m_len);
		} else
			nam->m_len = 0;
		break;

	case PRU_SLOWTIMO:
		break;

	case PRU_SOCKLABEL:
		error = EOPNOTSUPP;
		break;

	default:
		panic("piusrreq");
	}
release:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Both send and receive buffers are allocated PIPSIZ bytes of buffering
 * for stream sockets, although the total for sender and receiver is
 * actually only PIPSIZ.
 * Datagram sockets really use the sendspace as the maximum datagram size,
 * and don't really want to reserve the sendspace.  Their recvspace should
 * be large enough for at least one max-size datagram plus address.
 */
extern u_int	unpst_sendspace;
extern u_int	unpst_recvspace;
extern u_int	unpdg_sendspace;	/* really max datagram size */
extern u_int	unpdg_recvspace;

struct	unpcb *unp_bound;		/* list of bound unpcbs */
int	unp_rights;			/* file descriptors in flight */

int
unp_attach(struct socket *so)
{
	register struct unpcb *unp;

	ASSERT(SOCKET_ISLOCKED(so));	
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		switch (so->so_type) {

		case SOCK_STREAM:
			soreserve(so, unpst_sendspace, unpst_recvspace);
			break;

		case SOCK_DGRAM:
			soreserve(so, unpdg_sendspace, unpdg_recvspace);
			break;
		}
	}
	unp = (struct unpcb *)mcb_get(M_WAIT, sizeof(*unp), MT_PCB);
	if (unp == NULL)
		return (ENOMEM);
	so->so_pcb = (caddr_t)unp;
	unp->unp_socket = so;
	unp->unp_refcnt = 1;
	UNP_MPWRLOCK();
	unp->unp_lnext = unpcb_list.unp_lnext;
	unpcb_list.unp_lnext->unp_lprev = unp;
	unpcb_list.unp_lnext = unp;
	unp->unp_lprev = &unpcb_list;
	UNP_MPUNLOCK();
	return (0);
}

int
unp_detach(register struct unpcb *unp)
{
	struct vnode *vp;
	struct unpcb *unq;
	struct socket *so = unp->unp_socket;

	ASSERT(SOCKET_ISLOCKED(so));

	UNP_MPWRLOCK();
	so->so_state |= SS_CLOSING;
	ASSERT((unp->unp_vnode != NULL) ^ (unp->unp_prevp == NULL));
	if (vp = unp->unp_vnode) {
		unp->unp_vnode = 0;
		if (unq = unp->unp_next)
			unq->unp_prevp = unp->unp_prevp;
		*unp->unp_prevp = unq;
	}
	UNP_MPUNLOCK();
	if (unp->unp_conn || (so->so_type == SOCK_DGRAM)) {
		/*
		 * We want to ensure that we disconnect anybody that might
		 * be pointing to us, and in the case of datagram sockets, the
		 * fact that we have a NULL unp_conn pointer does not mean that
		 * nobody points to us.
		 */
		unp_disconnect(unp);
	}
restart:
	UNP_MPWRLOCK();
	while (unp->unp_refs) {
		struct socket *so = (unp->unp_refs)->unp_socket;
		if (uipc_three_way_lock(unp->unp_socket, so) == 0) {
			goto restart;
		}
		UNP_MPUNLOCK();
		/*
		 * Make sure we use the same socket we just locked above,
		 * and that it was not detached while we blocked.
		 */
		if (so->so_pcb == 0 ||
		    unp_drop((struct unpcb *)so->so_pcb, ECONNRESET) == 0) {
			SOCKET_UNLOCK(so);
		}
		UNP_MPWRLOCK();
	}
	unp->unp_lprev->unp_lnext = unp->unp_lnext;
	unp->unp_lnext->unp_lprev = unp->unp_lprev;
	unp->unp_lnext = unp->unp_lprev = 0;
	UNP_MPUNLOCK();
	soisdisconnected(unp->unp_socket);
	if (vp)
		VN_RELE(vp);
	return UNPCB_RELE(unp);
#ifdef NEVER
	if (unp_rights)
		unp_gc();
#endif
}

struct mbuf *
uipc_copyaddr(struct mbuf *m)
{
	struct mbuf *m2 = m_get(M_WAIT, m->m_type);

	m2->m_len = m->m_len;
	bcopy(mtod(m, caddr_t), mtod(m2, caddr_t), m->m_len);
	return m2;
}

unp_bind(struct unpcb *unp, struct mbuf *nam)
{
	struct sockaddr_un *soun = mtod(nam, struct sockaddr_un *);
	struct vnode *vp;
	int error;
	struct vattr vattr;
	struct unpcb *unq;

	ASSERT(SOCKET_ISLOCKED(unp->unp_socket));
	if (soun->sun_family != AF_UNIX) 
	        return EAFNOSUPPORT;

	if (unp->unp_vnode != NULL)
		return (EINVAL);
	if (nam->m_len == MLEN) {
	        if (*(mtod(nam, caddr_t) + nam->m_len - 1) != 0)
		        return EINVAL;
	} else
	        *(mtod(nam, caddr_t) + nam->m_len) = 0;

	/* Here is a hack so that the following uipc_copyaddr() will 
	 * copy the null character into the address unp_addr.
	 * -- lguo */
	nam->m_len++;

/* SHOULD BE ABLE TO ADOPT EXISTING AND wakeup() ALA FIFO's */
	error = lookupname(soun->sun_path, UIO_SYSSPACE, FOLLOW,
			   NULLVPP, &vp, NULL);
	if (!error) {
		VN_RELE(vp);
		return (EADDRINUSE);
	}
	vattr.va_mask = AT_TYPE|AT_MODE|AT_RDEV;
	vattr.va_type = VSOCK;
	vattr.va_mode = 0777;
	vattr.va_rdev = NODEV;
	error = vn_create(soun->sun_path, UIO_SYSSPACE, &vattr, VEXCL,
			  VREAD|VWRITE, &vp, CRMKNOD, NULL);
	if (error) {
		if (error == EEXIST)
			error = EADDRINUSE;
		return (error);
	}
	unp->unp_addr = uipc_copyaddr(nam);
	unp->unp_addr->m_len--;
	UNP_MPWRLOCK();
	unp->unp_vnode = vp;
	if (unq = unp_bound)
		unq->unp_prevp = &unp->unp_next;
	unp->unp_next = unq;
	unp->unp_prevp = &unp_bound;
	unp_bound = unp;
	UNP_MPUNLOCK();
	return (0);
}

unp_connect(struct socket *so, struct mbuf *nam)
{
	register struct sockaddr_un *soun = mtod(nam, struct sockaddr_un *);
	struct vnode *vp;
	register struct socket *so2, *so3;
	struct unpcb *unp2, *unp3;
	int error;

	ASSERT(SOCKET_ISLOCKED(so));
	if (soun->sun_family != AF_UNIX) 
	        return EAFNOSUPPORT;
	if (nam->m_len + (nam->m_off - MMINOFF) == MLEN)
		return (EMSGSIZE);
	*(mtod(nam, caddr_t) + nam->m_len) = 0;
	error = lookupname(soun->sun_path, UIO_SYSSPACE, FOLLOW,
			   NULLVPP, &vp, NULL);
	if (error)
		return (error);
	VOP_ACCESS(vp, VWRITE, get_current_cred(), error);
	if (error)
		goto out;
	if (vp->v_type != VSOCK) {
		error = ENOTSOCK;
		goto out;
	}
retry:
	so2 = 0;
	UNP_MPRDLOCK();
	for (unp2 = unp_bound; unp2 != 0; unp2 = unp2->unp_next) {
		if ((unp2->unp_vnode == vp) && unp2->unp_socket) {
			so2 = unp2->unp_socket;
			UNPCB_HOLD(unp2);
			break;
		}
	}
	UNP_MPUNLOCK();
	if (so2 == 0) {
		error = ECONNREFUSED;
		goto out;
	}
	if (!SOCKET_PAIR_CMPLOCK(so, so2)) {
		/*
		 * Couldn't get second lock:
		 *	- drop first lock
		 *	- acquire second lock
		 *	- release second PCB
		 *	- reacquire first lock
		 * We don't need to hold so because it is not marked
		 * SS_NOFDREF; there should be no way that can happen while
		 * we are in unp_connect().
		 * (relies on the fact that SOCKET_PAIR_CMPLOCK() will return
		 * 1 if both sockets are the same)
		 */
		SOCKET_UNLOCK(so);
		SOCKET_LOCK(so2);
		if (UNPCB_RELE(unp2) == 0) {
			SOCKET_UNLOCK(so2);
		}
		SOCKET_LOCK(so);
		goto retry;
	}
	if (so2->so_pcb == 0 || (so2->so_state & SS_CLOSING)) {
		error = ECONNREFUSED;
		goto bad;
	}
	if (so->so_type != so2->so_type) {
		error = EPROTOTYPE;
		goto bad;
	}
	
	if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
		struct mbuf *nam2 = 0;

		if ((so2->so_options & SO_ACCEPTCONN) == 0) {
			error = ECONNREFUSED;
			goto bad;
		}
		unp2 = sotounpcb(so2);
		if (unp2->unp_addr)
			nam2 = uipc_copyaddr(unp2->unp_addr); 
		if ((so3 = sonewconn(so2, NULL)) == 0) {
			if (nam2) m_freem(nam2);
			error = ECONNREFUSED;
			/* sonewconn() no longer unlocks head */
			if (UNPCB_RELE(unp2) == 0 && (so != so2)) {
				SOCKET_UNLOCK(so2);
			}
			goto out;
		} else {
			/* sonewconn() no longer unlocks head */
			if (UNPCB_RELE(unp2) == 0 && (so != so2)) {
				SOCKET_UNLOCK(so2);
			}
		}
		unp3 = sotounpcb(so3);
		UNPCB_HOLD(unp3);	/* simplifies exit code below */
		unp3->unp_addr = nam2;
		so2 = so3;
	}
	error = unp_connect2(so, so2);
bad:
	if (so2 != so) {
		if (UNPCB_RELE((struct unpcb *)so2->so_pcb) == 0) {
			SOCKET_UNLOCK(so2);
		}
	} else {
		/* shouldn't be last reference; NOFDREF should be clear */
		ASSERT((so->so_state & SS_NOFDREF) == 0);
		UNPCB_RELE((struct unpcb *)so->so_pcb);
	}
out:
	ASSERT(SOCKET_ISLOCKED(so));
	VN_RELE(vp);
	return (error);
}

unp_connect2(register struct socket *so, register struct socket *so2)
{
	register struct unpcb *unp = sotounpcb(so);
	register struct unpcb *unp2;

	ASSERT(SOCKET_ISLOCKED(so));
	ASSERT(SOCKET_ISLOCKED(so2));
	if (so2->so_type != so->so_type)
		return (EPROTOTYPE);
	if (so2->so_state & SS_CLOSING) {
		return ECONNREFUSED;
	}
	unp2 = sotounpcb(so2);
	unp->unp_conn = unp2;
	ASSERT(so2->so_head != so);
	ASSERT(so->so_head != so2);
	switch (so->so_type) {

	case SOCK_DGRAM:
		UNP_MPWRLOCK();
		unp->unp_nextref = unp2->unp_refs;
		unp2->unp_refs = unp;
		UNP_MPUNLOCK();
		soisconnected(so);
		break;

	case SOCK_STREAM:
		unp2->unp_conn = unp;
		soisconnected(so2);
		soisconnected(so);
		break;

	default:
		panic("unp_connect2");
	}
	_SESMGR_SAMP_SBCOPY(so, so2);
	return (0);
}

void
unp_disconnect(struct unpcb *unp)
{
	register struct unpcb *unp2 = unp->unp_conn;

	ASSERT(SOCKET_ISLOCKED(unp->unp_socket));
	if (unp2 == 0)
		return;
	switch (unp->unp_socket->so_type) {

	case SOCK_DGRAM:
		unp->unp_conn = 0;
		UNP_MPWRLOCK();
		if (unp2->unp_refs == unp)
			unp2->unp_refs = unp->unp_nextref;
		else {
			unp2 = unp2->unp_refs;
			for (;;) {
				if (unp2 == 0)
					panic("unp_disconnect");
				/*
				 * Avoid panic in PRU_SEND code by ensuring
				 * that anybody who points to us is cleaned up.
				 * We could be shutting down while the lock
				 * is dropped due to pair locking.
				 */
				if (unp2->unp_conn == unp) {
					unp2->unp_conn = (struct unpcb *)0;
				}
				if (unp2->unp_nextref == unp)
					break;
				unp2 = unp2->unp_nextref;
			}
			unp2->unp_nextref = unp->unp_nextref;
		}
		unp->unp_nextref = 0;
		UNP_MPUNLOCK();
		unp->unp_socket->so_state &= ~SS_ISCONNECTED;
		break;

	case SOCK_STREAM:
loop:
		if (unp2->unp_socket == 0)
			return;
		if (SOCKET_PAIR_CMPLOCK(unp->unp_socket,
		    unp2->unp_socket) == 0) {
			if (unp->unp_conn == 0) {
				return;
			} else {
				unp2 = unp->unp_conn;
				goto loop;
			}
		}
		ASSERT(SOCKET_ISLOCKED(unp2->unp_socket));
		unp->unp_conn = 0;
		soisdisconnected(unp->unp_socket);
		unp2->unp_conn = 0;
		soisdisconnected(unp2->unp_socket);
		if (unp->unp_socket != unp2->unp_socket) {
			SOCKET_UNLOCK(unp2->unp_socket);
		}
		ASSERT(SOCKET_ISLOCKED(unp->unp_socket));
		break;
	}
}

void
unp_usrclosed(struct unpcb *unp)
{
	if (unp->unp_socket->so_type == SOCK_STREAM) {
		struct socket *so;
sync:
		if (unp->unp_conn && (so = unp->unp_conn->unp_socket)) {
			if (SOCKET_PAIR_CMPLOCK(unp->unp_socket, so) == 0)
				goto sync;
			socantrcvmore(so);
			if (unp->unp_socket != so)
				SOCKET_UNLOCK(so);
		}
	}
}

int
unp_rele(struct unpcb *unp)
{
	ASSERT(unp);
	if (atomicAddInt(&unp->unp_refcnt, -1)) {
		return 0;
	}
	return unp_freespc(unp);
}

int
unp_freespc(struct unpcb *unp)
{
	struct socket *so = unp->unp_socket;

	ASSERT(so);
	ASSERT(SOCKET_ISLOCKED(so));
	ASSERT(unp->unp_lnext == 0);
	ASSERT(unp->unp_lprev == 0);
	if (so->so_holds) {
		/*
		 * Someone is sending or receiving, so we can't just blow this
		 * socket away.
		 */
		return 0;
	}
	so->so_pcb = (caddr_t) 0;
	m_freem(unp->unp_addr);
#ifdef DEBUG
	{
		long *p = (long *)unp;
		size_t l = sizeof(*unp) / sizeof(long);
		size_t i;
		for (i = 0; i < l; p++, i++) {
			*p = (long)__return_address;
		}
	}
#endif
	(void) mcb_free(unp, sizeof(*unp), MT_PCB);
	return sofree(so);
}

int
unp_drop(struct unpcb *unp, int errno)
{
	struct socket *so = unp->unp_socket;

	ASSERT(SOCKET_ISLOCKED(so));
	so->so_error = errno;
	unp_disconnect(unp);
	if ((errno == ECONNRESET) && (so->so_type == SOCK_DGRAM)) {
		/*
		 * Don't drop datagram socket memory just because the other
		 * side went away.
		 */
		return 0;
	}
	return unp_detach(unp);
}

#ifdef notdef
unp_drain()
{

}
#endif

int
unp_externalize(struct mbuf *rights)
{
	int 		numfps = rights->m_len / sizeof (struct vfile *);
	struct vfile	**fparray = mtod(rights, struct vfile **);
	struct vfile 	*fp;
	int 		i, s, *fdarray, *int_fparray;
	int		error;

	/*
	 * Do not continue if the rights set is zero sized
	 */
	if (numfps == 0)
		return (EMSGSIZE);

	fdarray = kern_malloc(numfps * sizeof(int));

	/* 
	 * Atomically allocate all new fds and set file pointers.	
	 */
	error = fdt_alloc_many(numfps, fparray, fdarray);
	if (error) {
		for (i = 0; i < numfps; i++) {
			fp = fparray[i];
			unp_discard(fp);
			fparray[i] = NULL;
		}
		kern_free(fdarray);
		return (EMSGSIZE);
	}

	/*
	 * Copy the new fd's back to the "rights" mbuf, taking
	 * into account the difference in size between file 
	 * pointers and ints.
	 */
	int_fparray = (int *) fparray;
	for (i = 0; i < numfps; i++) {
		fp = fparray[i];
		int_fparray[i] = fdarray[i];
		s = VFLOCK(fp);
		fp->vf_msgcount--;
		VFUNLOCK(fp, s);
		UNP_PROTECT(unp_rights++);
	}
	/* Adjust mbuf length to reflect type change */
	rights->m_len = numfps * sizeof(int);

	kern_free(fdarray);
	return 0;
}

unp_internalize(struct mbuf *rights)
{
	register struct vfile	**rp;
	int 			oldfds = rights->m_len / sizeof (int);
	register int 		i, *ip;
	struct vfile 		*fp, **fpp, **fparray;
	int 			error, s;

	/* malloc array for file pointers */
	fparray = kern_malloc(oldfds * sizeof(struct vfile *));

	/* 
	 * Make sure fds are OK, and get file pointers.
	 * This will mark each fd as inuse so that any sprocs
	 * that share fd's can't close any of them.
	 */
	rp = mtod(rights, struct vfile **);
	ip = (int *)rp;
	for (i = 0; i < oldfds; i++)
		if (error = getf(*ip++, &fparray[i])) {
			kern_free(fparray);
			return (error);
		}

	/* 
	 * Convert fds to file pointers.  Walk backward because a pointer
	 * is bigger than a fd in 64-bit mode. 
	 */
	rp = mtod(rights, struct vfile **) + oldfds - 1;
	fpp = &fparray[oldfds - 1];
	for (i = 0; i < oldfds; i++, fpp--, rp--) {
		fp = *fpp;
		*rp = fp;;
		s = VFLOCK(fp);
		fp->vf_count++;
		fp->vf_msgcount++;
		VFUNLOCK(fp, s);
		UNP_PROTECT(unp_rights++);
	}

	/* adjust mbuf length to reflect type change */
	rights->m_len = oldfds * sizeof(struct vfile *);

	kern_free(fparray);
	return (0);
}

/*
 * Garbage collecting - its possible that there are fd's caught in transit
 * they have been passed into messages but not ever received.
 * XXXjwag - seems like all messages are cleaned vi soclose->sorflush->
 * the unp_dispose function which frees all descriptors.
 *
 * The old filescan code is very bogus:
 * 1) filescan holds the spinlock while calling its functions - one of these
 *	functions calls vfile_close! this immediately double trips on the file lock
 * 2) The entire model - marking ALL file descriptors, then scanning
 *	them for unmarked descriptors doesn't work in an MP environment -
 *	between the marking and the final scan, other file descriptors could
 *	have been added - these will be closed! by mistake.
 * 3) holding the file lock for a scan is way too long to hold off any
 *	new file opens
 * Its all been removed ...
 */

int
unp_dispose(struct mbuf *m)
{
	if (m)
		unp_scan(m, unp_discard);
	return (0);
}

void
unp_scan(register struct mbuf *m0,
	void (*op)(struct vfile *))
{
	register struct mbuf *m;
	register struct vfile **rp;
	register int i;
	int qfds;

	while (m0) {
		for (m = m0; m; m = m->m_next)
			if (m->m_type == MT_RIGHTS && m->m_len) {
				qfds = m->m_len / sizeof (struct vfile *);
				rp = mtod(m, struct vfile **);
				for (i = 0; i < qfds; i++, rp++)
					(*op)(*rp);
				break;		/* XXX, but saves time */
			}
		m0 = m0->m_act;
	}
}

void
unp_discard(struct vfile *fp)
{
	int s;

	s = VFLOCK(fp);
	fp->vf_msgcount--;
	VFUNLOCK(fp, s);
	UNP_PROTECT(unp_rights--);
	vfile_close(fp);
}

/*
 * This routine is a hacked up variant of socket_pair_cmp_lock(), which
 * knows about the unp_list lock.  It exists only to avoid livelock between
 * unp_detach() and the PRU_SEND code.
 */
/* ARGSUSED */
int
uipc_three_way_lock(struct socket *so1, struct socket *so2)
{
	timespec_t backoff = {0, 1L};

	if (so1 == so2)
		return(1);
	ASSERT(SOCKET_ISLOCKED(so1));
	if (SOCKET_TRYLOCK(so2) == 0) {
		/*
		 * Couldn't get lock.  Hold so2 so that it won't go away
		 * while we drop the lock on so1.
		 */
		SO_HOLD(so2);
		UNP_MPUNLOCK();
		SOCKET_UNLOCK(so1);
		/*
		 * This really sticks in my craw, but we need to avoid
		 * livelock.
		 */
		nano_delay(&backoff);
		SOCKET_LOCK(so2);
		if (SO_RELE(so2) == 0) {
			SOCKET_UNLOCK(so2);
		}
		SOCKET_LOCK(so1);
		return(0);
	}
	return(1);
}

int
uipc_vsock_from_addr(vnode_t *vp, vsock_t **vsop)
{
	vsock_t	*vso = NULL;
	struct unpcb *unp;
	extern void vsocket_hold(struct vsocket *);
	struct socket *so;

	UNP_MPRDLOCK();
	for (unp = unp_bound; unp != 0; unp = unp->unp_next) {
		if ((unp->unp_vnode == vp) && unp->unp_socket) {
			so = unp->unp_socket;
			UNPCB_HOLD(unp);
			break;
		}
	}
	UNP_MPUNLOCK();
	*vsop = NULL;
	if (unp) {
		ASSERT(so);
		SO_UTRACE(UTN('uivs','add1'), so, unp);
		SOCKET_LOCK(so);
		if (!(so->so_state & SS_NOFDREF)) {
			vso = BHV_TO_VSOCK_TRY(&(so->so_bhv));
			if (vso && !(vso->vs_flags & VS_LASTREFWAIT)) {
				VSO_UTRACE(UTN('uivs','add2'), vso, unp);
				vsocket_hold(vso);
				*vsop = vso;
			}
		}
		if (!UNPCB_RELE(unp))
			SOCKET_UNLOCK(so);
		return *vsop ? 0 : ENOENT;
	}
	return ENOENT;
}
