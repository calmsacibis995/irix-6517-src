/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *	@(#)raw_usrreq.c	7.3 (Berkeley) 12/30/87
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sbd.h"
#include "sys/mbuf.h"
#include "sys/domain.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/tcpipstats.h"
#include "sys/cmn_err.h"
#include "sys/errno.h"
#include "sys/systm.h"
#include "sys/systm.h"
#include "sys/debug.h"
#include <bstring.h>

#include "if.h"
#include "route.h"
#include "netisr.h"
#include "raw_cb.h"
#include "netinet/in.h"
#include "raw.h"

mrlock_t rawcblist_lock;

static void raw_protocol_input(struct mbuf *m, struct route *notused);

int routelock_inited;

/*
 * Initialize raw connection block q.
 */
void
raw_init(void)
{
	static int raw_inited;

	if (raw_inited++)
		return;
	rawcb.rcb_next = rawcb.rcb_prev = &rawcb;
	network_input_setup(AF_RAW, raw_protocol_input);
	RAW_INITLOCKS();
	if (routelock_inited++)
		return;
	ROUTE_INITLOCK();
}

/*
 * Raw protocol interface for interrupt-time input.
 */
int
raw_input(struct mbuf *m0,
	struct sockproto *proto,
	struct sockaddr *src, 
	struct sockaddr *dst,
	struct ifnet *ifns)
{
	register struct mbuf *m;
	struct raw_header *rh;

	/*
	 * Rip off an mbuf for a generic header.
	 */
	m = m_get(M_DONTWAIT, MT_HEADER);
	if (m == 0) {
		m_freem(m0);
		return ENOBUFS;
	}
	m->m_next = m0;
	m->m_len = sizeof(struct raw_header);
	rh = mtod(m, struct raw_header *);
	rh->raw_ifh.ifh_ifp = ifns;
#ifdef INET6
	if (((struct sockaddr_new *)dst)->sa_len)
		bcopy(dst, &rh->raw_dst, ((struct sockaddr_new *)dst)->sa_len);
	else
		rh->raw_dst = *dst;
	if (((struct sockaddr_new *)src)->sa_len)
		bcopy(src, &rh->raw_src, ((struct sockaddr_new *)src)->sa_len);
	else
		rh->raw_src = *src;
#else
	rh->raw_dst = *dst;
	rh->raw_src = *src;
#endif
	rh->raw_proto = *proto;
	/*
	 * Header now contains enough info to decide
	 * which socket to place packet in (if any).
	 * Queue it up for the network input procces.
	 */
	return network_input(m, AF_RAW, 0);
}

/*
 * Raw protocol input routine.  Process packets entered
 * into the queue at interrupt time.  Find the socket
 * associated with the packet(s) and move them over.  If
 * nothing exists for this packet, drop it.
 *
 * Lock hierarchy:
 * 	To avoid deadlocking, always acquire socket lock 
 *	before acquiring rawcb lock in nested cases.
 */

/* ARGSUSED1 */
static void
raw_protocol_input(struct mbuf *m, struct route *notused)
{
	struct mbuf *n;
	register struct rawcb *rp, *rpnext;
	register struct raw_header *rh;
	struct socket *rp_so;
	struct sockaddr_in *lsin;
	struct sockaddr *rsa;
	int ok;
    
	rh = mtod(m, struct raw_header *);
	ok = (rh->raw_proto.sp_family == PF_INET) ? 0 : 1;

resync:
	RAWCBLIST_MRRLOCK();

	for (rp = rawcb.rcb_next; rp != &rawcb; rp = rpnext) {

		ASSERT(RAWCBLIST_ISMRLOCKED(MR_ACCESS));
		rpnext = rp->rcb_next;

		if (rp->rcb_proto.sp_family != rh->raw_proto.sp_family)
			continue;

		if (rp->rcb_proto.sp_protocol  &&
		    rp->rcb_proto.sp_protocol != rh->raw_proto.sp_protocol)
			continue;
		lsin = (struct sockaddr_in *)rp->rcb_laddr;
		rsa = rp->rcb_faddr;
		/*
		 * We assume the lower level routines have
		 * placed the address in a canonical format
		 * suitable for a structure comparison.
		 */
#ifdef INET6
#define equal(a1, a2) (bcmp((a1), &(a2), \
 ((struct sockaddr_new *)&(a2))->sa_len ? \
 ((struct sockaddr_new *)&(a2))->sa_len :\
 sizeof (struct sockaddr)) == 0)
#else
#define equal(a1, a2) (bcmp((a1), &(a2), sizeof (struct sockaddr)) == 0)
#endif

		if (lsin != 0) {
			/*
			 * We're not supporting ipv6 with tpisockets so
			 * this if statement has been left alone.
			 */
			if ((rp->rcb_socket->so_state & SS_TPISOCKET) &&
			    (lsin->sin_family == AF_INET) &&
			    (lsin->sin_addr.s_addr == INADDR_ANY) &&
			    (lsin->sin_port == 0))
				goto local_wildcard;

		    	if (!equal(lsin, rh->raw_dst))
				continue;
		}
local_wildcard:
		if (rsa != 0 && !equal(rsa, rh->raw_src))
			continue;
		/*
		 * Exclude routing packets from this socket.
		 * AF_ROUTE packets are prefixed with a pointer
		 * to a rawcb that should not receive the message.
		 */
		if (rh->raw_proto.sp_family == AF_ROUTE &&
		    *mtod(m->m_next, struct rawcb **) == rp)
			continue;
		/*
		 * Found a right rawcb.  Try to deliver a replicated packet.
		 * Watch out for the ordering of locking and holding.
		 */
		ok = 1;
		if (n = m_copy(m->m_next, (rh->raw_proto.sp_family == AF_ROUTE
					   ? sizeof(rp) : 0),
			       M_COPYALL)) {
			rp_so = rp->rcb_socket;
			/* hold rp before freeing rawcb list lock */
			RAWCB_HOLD(rp);
			/* free rawcb list lock before holding socket lock */
			RAWCBLIST_MRUNLOCK();
			/* lock socket before appending to it */
			SOCKET_LOCK(rp_so);
			/*
			 * If the rawcb was not changed between the
			 * time we chose it and we locked it,
			 * then copy the data.
			 */
			/*
			 * XXX6 is this what I want to do to fix the bug?
			 * raw_usrreq zeros out rcb_faddr after calling
			 * pr_output so allow a match if rcb_faddr is now
			 * zero but wasn't before.
			 */
			if (lsin != (struct sockaddr_in *)rp->rcb_laddr
#ifdef INET6
			    || (rp->rcb_faddr && rsa != rp->rcb_faddr)) {
#else
			    || rsa != rp->rcb_faddr) {
#endif
				m_freem(n);
			} else if (sbappendaddr(&rp_so->so_rcv, &rh->raw_src,
					    n, (struct mbuf *)0,
					    ((rp_so->so_options&SO_PASSIFNAME)
					? rh->raw_ifh.ifh_ifp : 0)) == 0) {
				m_freem(n);
				rawsbdrop(rp);
			} else {
				sorwakeup(rp_so, NETEVENT_RAWUP);
			}
			/* free socket lock before holding rawcb list lock */
			SOCKET_UNLOCK(rp_so);
			/* hold rawcb lock before checking if rp is on list */
			RAWCBLIST_MRRLOCK();
			/* remember rp's next field before releasing rp */
			rpnext = rp->rcb_next;
			/* release rp, but without deallocating it */
			if (RAWCB_RELEASE(rp, RAWCB_NOFREE)) {
				/*
				 * This path is not taken in normal cases.
				 *
				 * Free rawcb list lock before locking socket
				 * to avoid deadlocking.  Freeing rawcb list
				 * lock is OK because rp is already off the 
				 * list if we reach here.
				 */
				RAWCBLIST_MRUNLOCK();
				SOCKET_LOCK(rp->rcb_socket);
				/* Deallocate rp explicitly */
				rawcb_free(rp);
				goto resync;
			}
			/*
			 * If rp is off the list and multiple rtnetd's are
			 * working on it, it is possible that we didn't 
			 * release rp in the above RELEASE, but another
			 * rtnetd did it.  Resync if rp is off the list.
			 * Don't use rp; use the saved rpnext instead.
			 */
			if (rpnext == 0) {
				RAWCBLIST_MRUNLOCK();
				goto resync;
			}
		} else {
			rawsbdrop(rp);
		}
	} /* end of rawcb scan loop */

	if (!ok)
		IPSTAT(ips_noproto);
		
	RAWCBLIST_MRUNLOCK();
	m_freem(m);
}

/*ARGSUSED*/
void
raw_ctlinput(int cmd, struct sockaddr *arg, void* p)
{
	if (cmd < 0 || cmd > PRC_NCMDS)
		return;
	/* INCOMPLETE */
}

/*ARGSUSED*/
raw_usrreq(so, req, m, nam, rights)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *rights;
{
	register struct rawcb *rp = sotorawcb(so);
	register int error = 0, len;

	ASSERT(SOCKET_ISLOCKED(so));
	if (req == PRU_CONTROL) {
		return(rawioctl(so, (__psint_t)m, (caddr_t)nam)); 
	}

	if (rights && rights->m_len) {
		error = EOPNOTSUPP;
		goto release;
	}
	if (rp == 0 && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	switch (req) {

	/*
	 * Allocate a raw control block and fill in the
	 * necessary info to allow packets to be routed to
	 * the appropriate raw interface routine.
	 */

	case PRU_ATTACH:
		if ((so->so_state & SS_PRIV) == 0) {
			error = EACCES;
			break;
		}
		if (rp) {
			error = EINVAL;
			break;
		}
		error = raw_attach(so, (__psint_t)nam);
		break;

	/*
	 * Destroy state just before socket deallocation.
	 * Flush data or not depending on the options.
	 */
	case PRU_DETACH:
		if (rp == 0) {
			error = ENOTCONN;
			break;
		}
		raw_detach(rp);
		break;

	/*
	 * If a socket isn't bound to a single address,
	 * the raw input routine will hand it anything
	 * within that protocol family (assuming there's
	 * nothing else around it should go to). 
	 */
	case PRU_CONNECT:
		if (rp->rcb_faddr != 0) {
			error = EISCONN;
			break;
		}
		nam = m_copy(nam, 0, M_COPYALL);
		rp->rcb_faddr = mtod(nam, struct sockaddr *);
		soisconnected(so);
		break;

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;		/* not go to release */

	case PRU_BIND:
		if (rp->rcb_laddr != 0) {
			error = EINVAL;			/* XXX */
			break;
		}
		error = raw_bind(so, nam);
		break;

	case PRU_DISCONNECT:
		if (rp->rcb_faddr == 0) {
			error = ENOTCONN;
			break;
		}
		raw_disconnect(rp);
		soisdisconnected(so);
		break;

	/*
	 * Mark the connection as being incapable of further input.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	/*
	 * Ship a packet out.  The appropriate raw output
	 * routine handles any massaging necessary.
	 */
	case PRU_SEND:
		if (nam) {
			if (rp->rcb_faddr != 0) {
				error = EISCONN;
				break;
			}
			rp->rcb_faddr = mtod(nam, struct sockaddr *);
		} else if (so->so_proto->pr_flags & PR_DESTADDROPT) {

		} else if (rp->rcb_faddr == 0) {
			error = ENOTCONN;
			break;
		}
		error = (*so->so_proto->pr_output)(m, so, 0);
		m = NULL;
		if (nam)
			rp->rcb_faddr = 0;
		goto release;	

	case PRU_ABORT:
		raw_disconnect(rp);
		sofree(so);
		soisdisconnected(so);
		break;

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	/*
	 * Not supported.
	 */
	case PRU_RCVOOB:
	case PRU_RCVD:
		return(EOPNOTSUPP);

	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		if (rp->rcb_laddr != 0) {
#ifdef INET6
			len = ((struct sockaddr_new *)&rp->rcb_laddr)->sa_len;
			if (len == 0)
#endif
				len = sizeof(struct sockaddr);
			bcopy(rp->rcb_laddr, mtod(nam, caddr_t), len);
		} else {
			len = sizeof(struct sockaddr);
			bzero(mtod(nam, caddr_t), sizeof(struct sockaddr));
		}
		nam->m_len = len;
		break;

	case PRU_PEERADDR:
		if (rp->rcb_faddr != 0) {
#ifdef INET6
			len = ((struct sockaddr_new *)&rp->rcb_faddr)->sa_len;
			if (len == 0)
#endif
				len = sizeof(struct sockaddr);
			bcopy(rp->rcb_faddr, mtod(nam, caddr_t), len);
		} else {
			len = sizeof(struct sockaddr);
			bzero(mtod(nam, caddr_t), sizeof(struct sockaddr));
		}
		nam->m_len = len;
		break;

	case PRU_SOCKLABEL:
		error = EOPNOTSUPP;
		break;

	default:
		panic("raw_usrreq");
	}
release:
	if (m != NULL)
		m_freem(m);
	return (error);
}
