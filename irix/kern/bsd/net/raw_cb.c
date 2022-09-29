/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
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
 *	@(#)raw_cb.c	7.6 (Berkeley) 6/27/88 plus MULTICAST 1.1
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/domain.h"
#include "sys/protosw.h"
#include "sys/errno.h"
#include "sys/systm.h"
#include "sys/atomic_ops.h"

#include "if.h"
#include "route.h"
#include "raw_cb.h"
#include <netinet/in.h>
#include "soioctl.h"
#include "raw.h"

#include "bstring.h"

extern	struct ifaddr *ifa_ifwithaddr(struct sockaddr *);
extern	int ip_mrouter_done(void);
extern	void ip_rsvp_force_done(struct socket *so);
extern	ip_freemoptions(struct mbuf *);

/*
 * Routines to manage the raw protocol control blocks. 
 *
 * TODO:
 *	hash lookups by protocol family/protocol + address family
 *	take care of unique address problems per AF?
 *	redo address binding to allow wildcards
 */

/*
 * Allocate a control block and a nominal amount
 * of buffer space for the socket.
 */
int
raw_attach(register struct socket *so, __psint_t proto)
{
	register struct rawcb *rp;

	rp = (struct rawcb *)mcb_get(M_DONTWAIT, sizeof(*rp), MT_PCB);
	if (rp == 0)
		return (ENOMEM);
	sbreserve(&so->so_snd, (u_long) RAWSNDQ);
	sbreserve(&so->so_rcv, (u_long) RAWRCVQ);
	rp->rcb_socket = so;
	so->so_pcb = (caddr_t)rp;
	rp->rcb_pcb = 0;
	rp->rcb_refcnt = 1;
	rp->rcb_proto.sp_family = so->so_proto->pr_domain->dom_family;
	rp->rcb_proto.sp_protocol = proto;
	RAWCBLIST_MRWLOCK();
	insque(rp, &rawcb);
	RAWCBLIST_MRUNLOCK();
	return (0);
}

/*
 * Detach the raw connection block and discard
 * socket resources.
 */
void
raw_detach(register struct rawcb *rp)
{
	
	RAWCBLIST_MRWLOCK();
	remque(rp);
	rp->rcb_next = rp->rcb_prev = 0;
	RAWCBLIST_MRUNLOCK();

	/* now do the raw cb cleanup and the associated socket cleanup */
	rawcb_rele(rp, RAWCB_FREE);
	return;
}

int
rawcb_hold(struct rawcb *rp)	
{
	return (atomicAddInt(&(rp)->rcb_refcnt, 1));
}

int
rawcb_rele(struct rawcb *rp, int mode)	
{
	int refcnt, rtn = 0;

	refcnt = atomicAddInt(&(rp)->rcb_refcnt, -1);

	/*
	 * Note that checking refcnt and rawcb_free() are not atomic.
	 * Generally, this could cause a race condition:
	 *    1. thread A finds that refcnt is 0
	 *    2. thread B increases refcnt to 1
	 *    3. thread A has already decided that refcnt was 0, and
	 *       frees the rawcb anyway
	 *    4. thread B has a dangling pointer. Ouch!
	 *
	 * However, the above scenario cannot happen because of the way 
	 * we use rawcb. 
	 *
	 * Refcnt can become 0 only when the rawcb has been removed from
	 * the rawcb list via raw_detach().  A rawcb is removed with
	 * the list lock held; refcnt is increased with the list lock held.
	 * It is not possible that refcnt of a rawcb can be increased when
	 * it is already off the list.  Hence, step 2 above can never 
	 * happen. 
	 *
	 */

	if (refcnt == 0) {
		if (mode == RAWCB_FREE)
			rawcb_free(rp);
		rtn = 1;
	}
	return rtn;
}

void
rawcb_free(struct rawcb *rp)
{
	struct socket *so;

	ASSERT(rp->rcb_next == 0 && rp->rcb_prev == 0);
	ASSERT(rp->rcb_refcnt == 0);
	so = rp->rcb_socket;
	if (rp->rcb_laddr != 0) {
		rawclose(rp);
		m_freem(rcbtom(rp->rcb_laddr));
		rp->rcb_laddr = 0;
	}
	if (rp->rcb_route.ro_rt) {
		rtfree_needlock(rp->rcb_route.ro_rt);
	}
	so->so_pcb = 0;
	sofree(so);
	if (rp->rcb_options)
		m_freem(rp->rcb_options);
	/* MULTICAST */
	{
	extern struct socket *ip_mrouter;
	extern struct socket *ip_rsvpd;
	extern int rsvp_on;
	if (so == ip_mrouter)
		ip_mrouter_done();
	if (so == ip_rsvpd)
		ip_rsvpd = NULL;
	if (rsvp_on)
		ip_rsvp_force_done(so);
	if (rp->rcb_proto.sp_family == AF_INET)
		ip_freemoptions(rp->rcb_moptions);
	}
	(void) mcb_free(rp, sizeof(*rp), MT_PCB);
}

/*
 * Disconnect and possibly release resources.
 */
void
raw_disconnect(rp)
	struct rawcb *rp;
{
	extern struct sockaddr route_src;
	struct sockaddr *faddr;

	if (rp->rcb_socket->so_state & SS_NOFDREF)
		raw_detach(rp);
	faddr = rp->rcb_faddr;
	rp->rcb_faddr = 0;
	if (faddr && faddr != &route_src)   /* XXX */
		m_freem(rcbtom(faddr));
}

int
raw_bind(so, nam)
	register struct socket *so;
	struct mbuf *nam;
{
	struct sockaddr *addr = mtod(nam, struct sockaddr *);
	register struct rawcb *rp;

	ASSERT(SOCKET_ISLOCKED(so));
	if (ifnet == 0)
		return (EADDRNOTAVAIL);
	RAWCBLIST_MRWLOCK();
	rp = sotorawcb(so);
	/*
	 * Should we verify address not already in use?
	 * Some say yes, others no.
	 */
#ifdef INET6
	switch (((struct sockaddr_new *)addr)->sa_family) {
#else
	switch (addr->sa_family) {
#endif

	case AF_IMPLINK:
	case AF_INET: {
		if (((struct sockaddr_in *)addr)->sin_addr.s_addr &&
		    ifa_ifwithaddr(addr) == 0) {
			RAWCBLIST_MRUNLOCK();
			return (EADDRNOTAVAIL);
		}
		break;
	}
#ifdef INET6
	case AF_INET6: {
		if (!IS_ANYSOCKADDR(satosin6(addr)) &&
		  ifa_ifwithaddr(sin6tosa(addr)) == 0) {
			RAWCBLIST_MRUNLOCK();
			return (EADDRNOTAVAIL);
		}
		rp->rcb_flowinfo = ((struct sockaddr_in6 *)addr)->sin6_flowinfo;
		break;
	}
#endif
	case AF_RAW: {
		int error = rawopen(rp, (struct sockaddr_raw *) addr);
		if (error) {
			RAWCBLIST_MRUNLOCK();
			return (error);
		}
		break;
	}

	default:
		RAWCBLIST_MRUNLOCK();
		return (EAFNOSUPPORT);
	}
	nam = m_copy(nam, 0, M_COPYALL);
	if (nam != 0)
		rp->rcb_laddr = mtod(nam, struct sockaddr *);
	RAWCBLIST_MRUNLOCK();
	return (0);
}
