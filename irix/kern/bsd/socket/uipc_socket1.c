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

#ident	"$Revision: 1.49 $"

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/domain.h"
#include "sys/errno.h"
#include "ksys/vfile.h"
#include "sys/kthread.h"
#include "sys/mbuf.h"
#include "sys/proc.h"
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
#include <sys/vsocket.h>
#include <sys/fcntl.h>

/* Trusted IRIX */
#include <sys/mac_label.h>
#include <sys/sesmgr.h>

#include <net/soioctl.h>        /* XXX fold into ioctl.h */

extern struct pollhead *phalloc(int);
extern void phfree(struct pollhead *);
extern void sbdroprecord(struct sockbuf *sb);

extern int soaccept(struct socket *, struct mbuf *);
extern int sockargs(struct mbuf **, void *, int, int);
extern int rtioctl(int, caddr_t);	/* bsd/net/route.c */

int
uipc_accept(bhv_desc_t *bdp,
	caddr_t name,
	sysarg_t *anamelen,
	int namelen,
	rval_t *rvp,
	struct vsocket **nvso)
{
	int error, fd;
	struct vfile *fp, *fp2;
	struct mbuf nam;
	NETSPL_DECL(s)
	struct socket *aso;
	struct vsocket *vso = 0;
	register struct socket *so = bhvtos(bdp);

restart:
	if (!(so->so_proto->pr_flags & PR_CONNREQUIRED)) 
	        return EOPNOTSUPP;

	if ((so->so_options & SO_ACCEPTCONN) == 0) 
		return EINVAL;

	SOCKET_QLOCK(so, s);
	if ((so->so_state & SS_NBIO) && so->so_qlen == 0) {
		SOCKET_QUNLOCK(so, s);
		return EWOULDBLOCK;
	}
	while (so->so_qlen == 0 && so->so_error == 0) {
		int rv;

		if (so->so_state & SS_CANTRCVMORE) {
			so->so_error = ECONNABORTED;
			break;
		}
		rv = SOCKET_WAIT_SIG(&so->so_timeosem, PZERO+1, 
			&so->so_qlock, s);
		SOCKET_QLOCK(so, s);
		if (rv) {
			SOCKET_QUNLOCK(so, s);
			return EINTR;
		}
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		SOCKET_QUNLOCK(so, s);
		return error;
	}

	aso = so->so_q;

	ASSERT(mutex_mine(&aso->so_sem) == 0);
	if (!SOCKET_TRYLOCK(aso)) {
		SO_HOLD(aso);
		SOCKET_QUNLOCK(so, s);
		ASSERT(mutex_mine(&aso->so_sem) == 0);
		/*
		 * This lock should be safe due to hold done above; sofree()
		 * should not free aso out from under us.
		 */
		SOCKET_LOCK(aso);
		if (SO_RELE(aso) == 1) {
			/* freed */
			goto restart;
		}
		if (aso->so_head == 0) {
			/* someone else has a reference, unlock and continue */
			SOCKET_UNLOCK(aso);
			goto restart;
		}
		/*
		 * This should be OK, because:
		 *	- this is the lock order the lower half uses
		 * 	- if we are on a single-CPU system, nobody should
		 *        have the queue lock, because it is a spinlock and
		 *	  they couldn't have been preempted.
		 *	- if this is an MP system, any other thread would
		 *	  have to do the deadlock avoidance we just did, and
		 *	  since we have the lock on aso, they will all be
		 *	  blocked, and will soon relinquish the queue lock
		 *	- soclose() on so can't be running because we are
		 *	  in uipc_accept() on so.
		 */
		SOCKET_QLOCK(so, s);
		ASSERT(aso->so_head);
	}
	/*
	 * Take socket off of accept queue before calling vsowrap(), since
	 * it could sleep in kmem_zone_alloc().
	 */
	if (soqremque(aso, 1) == 0)
		panic("accept");
	SOCKET_QUNLOCK(so, s);
	error = vsowrap(aso, &vso, AF_INET, aso->so_type, 
		aso->so_proto->pr_protocol);
	if (error) {
		/* XXX assume no behavior chain if error */
		goto bail;
	}
	/* 
	 * prevent protocol from dropping this connection while we get a 
	 * vnode. Notice that the following code is not necessary on MP
	 * since we have aso locked already.
	 */
	aso->so_state &= ~SS_NOFDREF;

	if (nvso == NULL) {
		if (error = makevsock(&fp, &fd)) {
			bhv_remove(&(vso->vs_bh), &(aso->so_bhv));
			aso->so_state |= SS_NOFDREF;
bail:
			if (aso->so_pcb == 0) {
				if (sofree(aso) == 0) {
					SOCKET_UNLOCK(aso);
				}
			} else {
				if (soabort(aso)) {
					SOCKET_UNLOCK(aso);
				}
			}
			if (vso) {
				/* might be NULL if vsowrap() failed */
				vsocket_release(vso);
			}
			return error;
		}
	}

	/* XXX might need to initialize more fields here -- SCA */
	nam.m_off = MMINOFF;
	nam.m_len = 0;
	nam.m_next = 0;
	nam.m_type = MT_SONAME;
	if (aso->so_pcb == 0 || (aso->so_state & SS_CLOSING)) {
		/* lost the race: undo allocations and try again */
		bhv_remove(&(vso->vs_bh), &(aso->so_bhv));
		aso->so_state |= SS_NOFDREF;
		if (aso->so_pcb == 0) {
			if (sofree(aso) == 0) {
				SOCKET_UNLOCK(aso);
			}
		} else {
			if (soabort(aso)) {
				SOCKET_UNLOCK(aso);
			}
		}
		if (nvso == NULL) {
			vfile_alloc_undo(fd, fp);
		}
		vsocket_release(vso);
		goto restart;
	}
	if (nvso == NULL) {
		vfile_ready(fp, vso);
	}

#ifdef DEBUG
	error = soaccept(aso, &nam);
	ASSERT(!error);
#else
	(void)soaccept(aso, &nam);
#endif
	SOCKET_UNLOCK(aso);
	if (sesmgr_enabled) {
		if (error == 0 && (error = sesmgr_samp_accept(aso))) {
			SOCKET_LOCK(aso);
			if (soabort(aso))
				SOCKET_UNLOCK(aso);
		}
	}
#ifdef sgi /* Audit */
	_SAT_BSDIPC_CREATE(fd, aso,
			       aso->so_proto->pr_domain->dom_family,
			       aso->so_proto->pr_type, 0);
	_SAT_BSDIPC_ADDR(fd, aso, &nam, 0);
#endif
	if (name) {
	        if (namelen > nam.m_len)
		/* SHOULD COPY OUT A CHAIN HERE */
			error = copyout(mtod(&nam, caddr_t), name, nam.m_len);
		else
		        error = copyout(mtod(&nam, caddr_t), name, namelen);
		if (error) {
			if (nvso == NULL) {
				error = closefd (fd, &fp2);
				ASSERT(error == 0);
				ASSERT(fp == fp2);
				vfile_close(fp);
			}
			return EFAULT;
		}
		namelen = nam.m_len;
		error =  copyout(&namelen, anamelen, sizeof (int));
		if (error) {
			if (nvso == NULL) {
				error = closefd (fd, &fp2);
				ASSERT(error == 0);
				ASSERT(fp == fp2);
				vfile_close(fp);
			}
			return EFAULT;
		}
	}
	if (nvso == NULL) {
		rvp->r_val1 = fd;
	} else {
		rvp->r_val1 = 0;
		*nvso = vso;
	}
	return error;
}

/* ARGSUSED */
int
uipc_connect(bhv_desc_t *bdp, caddr_t name, int namlen)
{
	int error = 0;
	struct mbuf *nam;
	register struct socket *so = bhvtos(bdp);

	SOCKET_LOCK(so);
	if ((so->so_state & SS_NBIO) &&
	    (so->so_state & SS_ISCONNECTING)) {
		SOCKET_UNLOCK(so);
		return EALREADY;
	}
	error = sockargs(&nam, name, namlen, MT_SONAME);
	if (error) {
		SOCKET_UNLOCK(so);
		return error;
	}
	error = _SESMGR_SAMP_INIT_DATA(so, nam);
	if (error)
		goto bad;
	error = soconnect(so, nam);
	if (error)
		goto bad;
	if ((so->so_state & (SS_NBIO|SS_ISCONNECTING)) == 
		(SS_NBIO|SS_ISCONNECTING)) {
		SOCKET_UNLOCK(so);
		m_freem(nam);
		return EINPROGRESS;
	}
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		int rv;

		SOCKET_MPUNLOCK_SOTIMEO(so, PZERO+1, rv);
		SOCKET_LOCK(so);
		if (rv) {
			/*
			 * If the connection call get's interrupted by
			 * a signal, then leave this system call, but leave
			 * the SS_ISCONNECTING flag set and set the 
			 * SS_CONNINTERRUPTED flag so that if we retry this 
			 * connect call, we can resume where we left off.
			 */
			so->so_state |= SS_CONNINTERRUPTED;
			SOCKET_UNLOCK(so);
			m_freem(nam);
			return EINTR;
		}
	}
	error = so->so_error;
	so->so_error = 0;

bad:
	so->so_state &= ~SS_ISCONNECTING;
	SOCKET_UNLOCK(so);
	m_freem(nam);
	return error;
}

/* ARGSUSED */
int
uipc_getsockname(bhv_desc_t *bdp, caddr_t asa, sysarg_t *alen, int len, rval_t *rvp)
{
	int error;
	struct mbuf *m;
	register struct socket *so = bhvtos(bdp);

	m = m_getclr(M_WAIT, MT_SONAME);
	if (m == NULL)
		return ENOBUFS;
	SOCKET_LOCK(so);
	error = (*so->so_proto->pr_usrreq)(so, PRU_SOCKADDR, 0, m, 0);
	SOCKET_UNLOCK(so);
	if (error)
		goto bad;
	if (len > m->m_len)
	        error = copyout(mtod(m, caddr_t), asa, m->m_len);
	else
	        error = copyout(mtod(m, caddr_t), asa, len);
	if (error)
		goto bad;
	len = m->m_len;
	error = copyout(&len, alen, sizeof (len));
bad:
	m_freem(m);
	return error;
}

/* ARGSUSED */
int
uipc_getpeername(bhv_desc_t *bdp, caddr_t asa, sysarg_t *alen, rval_t *rvp)
{
	int error;
	struct mbuf *m;
	int len;
	register struct socket *so = bhvtos(bdp);

	m = m_getclr(M_WAIT, MT_SONAME);
	if (m == NULL)
		return ENOBUFS;
	error = copyin(alen, &len, sizeof (len));
	if (error)
		goto bad;
	SOCKET_LOCK(so);
	/* doing a getpeername() on a shutdown socket returns EINVAL.
	 * X/OPEN conformance. */
	if ((so->so_state & SS_CANTRCVMORE) 
	    && (so->so_state & SS_CANTSENDMORE)) {
	        SOCKET_UNLOCK(so);
		m_freem(m);
	        return EINVAL;
	}
	if ((so->so_state & SS_ISCONNECTED) == 0) {
	        SOCKET_UNLOCK(so);
		m_freem(m);
		return ENOTCONN;
	}
	error = (*so->so_proto->pr_usrreq)(so, PRU_PEERADDR, 0, m, 0);
	SOCKET_UNLOCK(so);
	if (error)
		goto bad;
	if (len > m->m_len)
		error = copyout(mtod(m, caddr_t), asa, m->m_len);
	else
	        error = copyout(mtod(m, caddr_t), asa, len);

	if (error)
		goto bad;
	len = m->m_len;
	error = copyout(&len, alen, sizeof (len));
bad:
	m_freem(m);
	return error;
}

/*
 * do IOCTLs for sockets
 */
/* ARGSUSED */
int
soioctl(bhv_desc_t *bdp,
	   int cmd,
	   void *arg,
	   int flag,
	   struct cred *cr,
	   rval_t *rvalp)
{
	int error = 0;
	register u_int size;
	long data[(IOCPARM_MASK+1)/sizeof(long)];
	register struct socket *so = bhvtos(bdp);

	/*
	 * Interpret high order word to find
	 * amount of data to be copied to/from the user address space.
	 */
	size = (cmd &~ (IOC_INOUT|IOC_VOID)) >> 16;
	if (size > sizeof (data))
		return EFAULT;
	if (cmd & IOC_IN) {
		if (size != 0) {
			if (error = copyin((caddr_t)arg, data, size)) 
				return error;
		} else
			*(caddr_t *)data = (caddr_t)arg;
	} else if ((cmd & IOC_OUT) && size != 0) {
		/*
		 * Zero the buffer on the stack so the user
		 * always gets back something deterministic.
		 */
		bzero(data, size);
	} else if (cmd & IOC_VOID) {
		*(caddr_t *)data = (caddr_t)arg;
	}

	SOCKET_LOCK(so);
	switch (cmd) {

	case FIONBIO:
		if (*(int *)data)
			so->so_state |= SS_NBIO;
		else
			so->so_state &= ~SS_NBIO;
		break;

	case FIOASYNC:
		if (*(int *)data) {
			so->so_state |= SS_ASYNC;
			so->so_rcv.sb_flags |= SB_ASYNC;
			so->so_snd.sb_flags |= SB_ASYNC;
		} else {
			so->so_state &= ~SS_ASYNC;
			so->so_rcv.sb_flags &= ~SB_ASYNC;
			so->so_snd.sb_flags &= ~SB_ASYNC;
		}
		break;

	case FIONREAD:
		*(int *)data = so->so_rcv.sb_cc;
		break;

	case SIOCNREAD:
		if ((*(int *)data = so->so_rcv.sb_cc) == 0) {
			if (so->so_error) {
				error = so->so_error;
				so->so_error = 0;
			} else if (so->so_state & SS_CANTRCVMORE) {
				/*
				 * For UDP sockets, this isn't really true...
				 */
				error = ECONNRESET;
			}
		}
		break;

	case FIOSETOWN:
	case SIOCSPGRP:
		so->so_pgid = *(int *)data;
		break;

	case FIOGETOWN:
	case SIOCGPGRP:
		*(int *)data = so->so_pgid;
		break;

	case SIOCATMARK:
		*(int *)data = (so->so_state&SS_RCVATMARK) != 0;
		break;

/* Trusted IRIX cases */
        case SIOCGETLABEL:
                error = _SIOCGETLABEL(so, (caddr_t)arg);
                break;

        case SIOCSETLABEL:
                error = _SIOCSETLABEL(so, (caddr_t)arg);
                break;

	default:
		/*
		 * Interface/routing/protocol specific ioctls:
		 * interface and routing ioctls should have a
		 * different entry since a socket is unnecessary
		 */
#define	cmdbyte(x)	(((x) >> 8) & 0xff)
		if (cmdbyte(cmd) == 'i')
			error = (ifioctl(so, cmd, (caddr_t)data));
		else if (cmdbyte(cmd) == 'r')
			error = (rtioctl(cmd, (caddr_t)data));
		else
			error = (*so->so_proto->pr_usrreq)(so,
				PRU_CONTROL, (struct mbuf *)(NULL+cmd),
				(struct mbuf *)data, (struct mbuf *)0);
	}
	SOCKET_UNLOCK(so);

	/*
	 * Copy any data to user, size was
	 * already set and checked above.
	 */
	if (!error && (cmd & IOC_OUT) && size)
		error = copyout(data, (caddr_t)arg, size);
	return error;
}

/* ARGSUSED */
int
uipc_setfl1(bhv_desc_t *bdp,
	   int oflags,
	   int nflags,
	   struct cred *cr)
{
	register struct socket *so = bhvtos(bdp);

	SOCKET_LOCK(so);
	if (nflags & (FNONBLK|FNDELAY))
		so->so_state |= SS_NBIO;
	else
		so->so_state &= ~SS_NBIO;
	if (nflags & FASYNC) {
		so->so_state |= SS_ASYNC;
		so->so_rcv.sb_flags |= SB_ASYNC;
		so->so_snd.sb_flags |= SB_ASYNC;
	} else {
		so->so_state &= ~SS_ASYNC;
		so->so_rcv.sb_flags &= ~SB_ASYNC;
		so->so_snd.sb_flags &= ~SB_ASYNC;
	}
	SOCKET_UNLOCK(so);
	return 0;
}

/*ARGSUSED*/
int
uipc_getattr1(bhv_desc_t *bdp,
	     struct vattr *vap,
	     int flags,
	     struct cred *cr)
{
	register struct socket *so = bhvtos(bdp);
	int error;

	bzero(vap, sizeof *vap);
	vap->va_type = VSOCK;
	SOCKET_LOCK(so);
	error = (*so->so_proto->pr_usrreq)(so, PRU_SENSE, (struct mbuf *)vap,
					   (struct mbuf *)0, (struct mbuf *)0);
	SOCKET_UNLOCK(so);
	return(error);
}

/* ARGSUSED */
int
uipc_fcntl1(
	bhv_desc_t *bdp,
	int cmd,
	void *arg,
	rval_t *rvp)
{
	register struct socket *so = bhvtos(bdp);

	switch (cmd) {
	case F_GETOWN:
		rvp->r_val1 = so->so_pgid;
		break;

	case F_SETOWN:
		so->so_pgid = (__psint_t)arg;
		break;

	default:
		return EINVAL;
	}
	return 0;
}

/* ARGSUSED */
int
sopoll(bhv_desc_t *bdp,
	  short events,
	  int anyyet,
	  short *reventsp,
	  struct pollhead **phpp,
	  unsigned int *genp)
{
	register unsigned revents;
	register struct socket *so = bhvtos(bdp);
	NETSPL_DECL(s1)

#define sbselqueue(sb) (sb)->sb_flags |= SB_SEL

	/* we can set phpp outside lock, saves two arg restores */
	if (!anyyet) {
		*phpp = &so->so_ph;
		/*
		 * Snapshotting the pollhead's generation number here before
		 * we grab the socket locks is fine since taking it early
		 * simply provides a more conservative estimate of what
		 * ``generation'' of the pollhead our event state report
		 * indicates.
		 */
		*genp = POLLGEN(&so->so_ph);
	}
	
	revents = events;
	SOCKET_LOCK(so);
	SOCKET_QLOCK(so, s1);
	if ((revents & (POLLIN|POLLRDNORM)) && !soreadable(so) && so->so_qlen == 0) {
		revents &= ~(POLLIN|POLLRDNORM);
		sbselqueue(&so->so_rcv);
	}
	if ((revents & (POLLPRI|POLLRDBAND))
	    && !(so->so_oobmark | (so->so_state & SS_RCVATMARK))) {
		revents &= ~(POLLPRI|POLLRDBAND);
		sbselqueue(&so->so_rcv);
	}
	if ((revents & POLLOUT) && !sowriteable(so)) {
		revents &= ~POLLOUT;
		sbselqueue(&so->so_snd);
	}
	SOCKET_QUNLOCK(so, s1);
	SOCKET_UNLOCK(so);
	*reventsp = revents;
	return 0;
}
