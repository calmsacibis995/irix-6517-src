/*
 * Copyright (c) 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)rtsock.c	8.7 (Berkeley) 10/12/95
 */

#ident "$Revision: 1.37 $"

#include "sys/errno.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"
#include "sys/atomic_ops.h"

#include "sys/systm.h"
#include "sys/types.h"
#include "sys/debug.h"
#include <ksys/vproc.h>
#include "sys/kmem.h"
#include "sys/ddi.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/domain.h"
#include "sys/protosw.h"
#include "sys/cmn_err.h"

#include "if.h"
#include "netinet/in.h"
#include "netinet/in_var.h"
#include "net/route.h"
#include "net/raw_cb.h"
#include "net/raw.h"

/*
 * External data structures
 */
extern struct ifaddr **ifnet_addrs;

/*
 * External Procedures
 */
extern void ifafree(struct ifaddr *);
extern void in_socktrim(struct sockaddr_in_new *);
extern struct ifaddr *ifa_ifwithnet(struct sockaddr *addr);
extern struct ifaddr *ifa_ifwithaddr(struct sockaddr *addr);
extern struct ifaddr *ifa_ifwithroute(int flags,
	struct sockaddr *dst, struct sockaddr *gateway);

static void m_copyback(struct  mbuf *m0, int off, int len, caddr_t cp);

/*
 * Module global variables
 */
struct	route_cb route_cb;

#ifdef _HAVE_SA_LEN
struct	sockaddr route_dst = { 2, PF_ROUTE, };
struct	sockaddr route_src = { 2, PF_ROUTE, };
struct	sockproto route_proto0 = { sizeof(route_proto0), PF_ROUTE, };
#else
struct	sockaddr route_dst = {{{ PF_ROUTE, }}};
struct	sockaddr route_src = {{{ PF_ROUTE, }}};
struct	sockproto route_proto0 = { PF_ROUTE, };
#endif

struct walkarg {
	int	w_op, w_arg, w_given, w_needed, w_tmemsize;
	caddr_t	w_where, w_tmem;
};

static struct mbuf *rt_msg1 __P((int, struct rt_addrinfo *));
#ifdef sgi
struct  rt_in_msghdr {
	struct rawcb *rp;
	union {
		struct rt_msghdr rtm;
		struct if_msghdr ifm;
		struct ifa_msghdr ifa;
	} u;
};
#endif
static int rt_msg2 __P((int, struct rt_addrinfo *, caddr_t, struct walkarg *));
static void	rt_xaddrs __P((caddr_t, caddr_t, struct rt_addrinfo *));

/*
 * Sleazy use of local variables throughout file, warning!!!!
 */
#define dst	info.rti_info[RTAX_DST]
#define gate	info.rti_info[RTAX_GATEWAY]

#ifdef _HAVE_SA_LEN
#define netmask	info.rti_info[RTAX_NETMASK]
#define genmask	info.rti_info[RTAX_GENMASK]
#else
#define netmask	(*(struct sockaddr_new**)&info.rti_info[RTAX_NETMASK])
#define genmask	(*(struct sockaddr_new**)&info.rti_info[RTAX_GENMASK])
#endif

#define ifpaddr	info.rti_info[RTAX_IFP]
#define ifaaddr	info.rti_info[RTAX_IFA]
#define brdaddr	info.rti_info[RTAX_BRD]

/*ARGSUSED*/
void
route_ctlinput(int cmd, struct sockaddr *arg, void* p)
{
	if (cmd < 0 || cmd > PRC_NCMDS)
		return;
	/* INCOMPLETE */
}


/*ARGSUSED*/
int
route_usrreq(so, req, m, nam, control)
	register struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
{
	register int error = 0;
	register struct rawcb *rp = sotorawcb(so);

	ASSERT(SOCKET_ISLOCKED(so));
	/* keep non-superusers from doing anything interesting */
	if ((so->so_state & SS_PRIV) == 0
	    && req != PRU_ATTACH
	    && req != PRU_DETACH)
		return (EACCES);
#ifndef sgi			/* raw_attach() creates the rawcb in IRIX */
	if (req == PRU_ATTACH) {
		MALLOC(rp, struct rawcb *, sizeof(*rp), M_PCB, M_WAITOK);
		if (so->so_pcb = (caddr_t)rp)
			bzero(so->so_pcb, sizeof(*rp));

	}
#endif
	if (req == PRU_DETACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
#ifdef INET6
		if (af == AF_INET || af == AF_INET6)
#else
		if (af == AF_INET)
#endif
			route_cb.ip_count--;
		else if (af == AF_NS)
			route_cb.ns_count--;
		else if (af == AF_ISO)
			route_cb.iso_count--;
		route_cb.any_count--;
	}
	if (req == PRU_ATTACH) {
		if (sotorawcb(so))
			error = EINVAL;
		else
			error = raw_attach(so, (__psint_t)nam);
	} else {
		error = raw_usrreq(so, req, m, nam, control);
	}
	rp = sotorawcb(so);
	if (req == PRU_ATTACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
		if (error) {
#ifndef sgi			/* raw_attach() creates the rawcb in IRIX */
			free((caddr_t)rp, M_PCB);
#endif
			return (error);
		}
#ifdef INET6
		if (af == AF_INET || af == AF_INET6)
#else
		if (af == AF_INET)
#endif
			route_cb.ip_count++;
#ifndef sgi
		else if (af == AF_NS)
			route_cb.ns_count++;
		else if (af == AF_ISO)
			route_cb.iso_count++;
#endif
		rp->rcb_faddr = &route_src;
		route_cb.any_count++;
		soisconnected(so);
		so->so_options |= SO_USELOOPBACK;
	}
	return (error);
}

/*ARGSUSED*/
int
route_output(register struct mbuf *m, struct socket *so, __uint32_t xdst)
{
	register struct rt_msghdr *rtm = 0;
	register struct rtentry *rt = 0;
	struct rtentry *saved_nrt = 0;
	struct radix_node_head *rnh;
	struct rt_addrinfo info;
	int len, error = 0;
	struct ifnet *ifp = 0;
	struct ifaddr *ifa = 0;
	struct sockproto route_proto = route_proto0;

#define senderr(e) { error = e; goto flush;}

#ifdef sgi
	/* only let the superuser change routes */
	if ((so->so_state & SS_PRIV) == 0) {
		m_freem(m);
		return (EACCES);
	}
#endif
	if (m == 0 || ((m->m_len < sizeof(long)) &&
		       (m = m_pullup(m, sizeof(long))) == 0))
		return (ENOBUFS);

	ROUTE_WRLOCK();

#ifndef M_PKTHDR
	if (m->m_len < sizeof(*rtm)
	    || (len = mtod(m, struct rt_msghdr *)->rtm_msglen,
		len > m->m_len)) {
		dst = 0;
		senderr(EINVAL);
	}
#else
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("route_output");
	len = m->m_pkthdr.len;
	if (len < sizeof(*rtm) ||
	    len != mtod(m, struct rt_msghdr *)->rtm_msglen) {
		dst = 0;
		senderr(EINVAL);
	}
#endif
	R_Malloc(rtm, struct rt_msghdr *, len);
	if (rtm == 0) {
		dst = 0;
		senderr(ENOBUFS);
	}
	/*
	 * Copy routing socket message from mbuf into rtm buffer
	 */
	m_datacopy(m, 0, len, (caddr_t)rtm);
	if (rtm->rtm_version != RTM_VERSION) {
		dst = 0;
		senderr(EPROTONOSUPPORT);
	}
	rtm->rtm_pid = current_pid();
	/*
	 * Save rti message header inidicator bits into local 'info' structure
	 * and call the procedure to create the pointer vector in 'info'
	 */
	info.rti_addrs = rtm->rtm_addrs;
	rt_xaddrs((caddr_t)(rtm + 1), len + (caddr_t)rtm, &info);

#ifdef _HAVE_SA_LEN
	if (dst == 0 || (route_proto.sp_protocol = dst->sa_family) >= AF_MAX)
#else
	if (dst == 0
	    || (route_proto.sp_protocol = _FAKE_SA_FAMILY(dst)) >= AF_MAX)
#endif
		senderr(EINVAL);

#ifdef _HAVE_SA_LEN
	if (gate != 0 && gate->sa_family >= AF_MAX)
#else
	if (gate != 0 && _FAKE_SA_FAMILY(gate) >= AF_MAX)
#endif
		senderr(EINVAL);

	if (genmask) {
		struct radix_node *t;
		t = rn_addmask((caddr_t)genmask, 0, 1);
		if (t && Bcmp(genmask, t->rn_key, *(u_char *)genmask) == 0)
#ifdef _HAVE_SA_LEN
			genmask = (struct sockaddr *)(t->rn_key);
#else
			genmask = (struct sockaddr_new *)(t->rn_key);
#endif
		else
			senderr(ENOBUFS);
	}
	switch (rtm->rtm_type) {

	case RTM_ADD:
		if (gate == 0)
			senderr(EINVAL);

		error = rtrequest(RTM_ADD, dst, gate, netmask,
					rtm->rtm_flags, &saved_nrt);

		if (error == 0 && saved_nrt) {
			rt_setmetrics(rtm->rtm_inits,
				&rtm->rtm_rmx, &saved_nrt->rt_rmx);
			RT_RELE(saved_nrt);
			saved_nrt->rt_genmask = genmask;
		}
		break;

	case RTM_DELETE:
		error = rtrequest(RTM_DELETE, dst, gate, netmask,
				rtm->rtm_flags, &saved_nrt);
		if (error == 0) {
			RT_HOLD(saved_nrt);
			rt = saved_nrt;
			goto report;
		}
		break;

	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
		if ((rnh = rt_tables[route_proto.sp_protocol]) == 0) {
			senderr(EAFNOSUPPORT);
		} else if (rt = (struct rtentry *)rnh->rnh_lookup(dst,
				netmask, rnh))
					RT_HOLD(rt);
			else
				senderr(ESRCH);

		switch(rtm->rtm_type) {

		case RTM_GET:
		report:
			dst = rt_key(rt);
			gate = rt->rt_gateway;
			netmask = rt_mask(rt);
			genmask = rt->rt_genmask;
			if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA)) {
				if (ifp = rt->rt_ifp) {
					ifa = ifnet_addrs[ifp->if_index - 1];
					ifpaddr = ifa->ifa_addr;
					ifaaddr = rt->rt_ifa->ifa_addr;

					if (ifp->if_flags & IFF_POINTOPOINT)
					     brdaddr = rt->rt_ifa->ifa_dstaddr;
					else
					     brdaddr = 0;
					rtm->rtm_index = ifp->if_index;
				} else {
					ifpaddr = 0;
					ifaaddr = 0;
			    }
			}
			len = rt_msg2(rtm->rtm_type, &info, (caddr_t)0,
				(struct walkarg *)0);
			if (len > rtm->rtm_msglen) {
				struct rt_msghdr *new_rtm;
				R_Malloc(new_rtm, struct rt_msghdr *, len);
				if (new_rtm == 0)
					senderr(ENOBUFS);
				Bcopy(rtm, new_rtm, rtm->rtm_msglen);
				Free(rtm); rtm = new_rtm;
			}
			(void)rt_msg2(rtm->rtm_type, &info, (caddr_t)rtm,
				(struct walkarg *)0);
			rtm->rtm_flags = rt->rt_flags;
			rtm->rtm_rmx = rt->rt_rmx;
			rtm->rtm_addrs = info.rti_addrs;
			break;

		case RTM_CHANGE:
			if (!gate)
				senderr(EINVAL);

			if (gate && rt_setgate(rt, rt_key(rt), gate))
				senderr(EDQUOT);
			/*
			 * new gateway could require new ifaddr, ifp;
			 * flags may also be different; ifp may be specified by
			 * link-level sockaddr when proto address is ambiguous
			 */

			if (ifpaddr &&
			    (ifa = ifa_ifwithnet(ifpaddr)) &&
			    (ifp = ifa->ifa_ifp)) {

					ifa = ifaof_ifpforaddr(ifaaddr
						? ifaaddr : gate, ifp);
			} else {
				if ((ifaaddr &&
				    (ifa = ifa_ifwithaddr(ifaaddr))) ||
				    (ifa = ifa_ifwithroute(rt->rt_flags,
						rt_key(rt), gate))) {
					ifp = ifa->ifa_ifp;
				}
			}
			if (ifa) {
				register struct ifaddr *oifa = rt->rt_ifa;

				if (oifa != ifa) {
					if (oifa && oifa->ifa_rtrequest) {

						oifa->ifa_rtrequest(RTM_DELETE,
							rt, gate);
					}
					if (rt->rt_llinfo) {
						cmn_err_tag(175,CE_WARN,
							"Memory leak warning: route llinfo non-null "
							"in route_output");
					}
					ifafree(rt->rt_ifa);

					/* bump refcnt on address record */
					atomicAddUint(&(ifa->ifa_refcnt), 1);

					/* reset address & interface pointers*/
					rt->rt_ifa = ifa;
					rt->rt_srcifa = 0;
					rt->rt_ifp = ifp;
				}
			}

			rt_setmetrics(rtm->rtm_inits, &rtm->rtm_rmx,
				      &rt->rt_rmx);

			if (rt->rt_ifa && rt->rt_ifa->ifa_rtrequest) {
			       rt->rt_ifa->ifa_rtrequest(RTM_ADD, rt, gate);
			}
			if (genmask)
				rt->rt_genmask = genmask;
			/*
			 * Fall into
			 */
		case RTM_LOCK:
			rt->rt_rmx.rmx_locks &= ~(rtm->rtm_inits);
			rt->rt_rmx.rmx_locks |=
				(rtm->rtm_inits & rtm->rtm_rmx.rmx_locks);
			break;
		}
		break;

	default:
		senderr(EOPNOTSUPP);
	}

flush:
	if (rtm) {
		if (error)
			rtm->rtm_errno = error;
		else
			rtm->rtm_flags |= RTF_DONE;
	}
	if (rt)
		rtfree(rt);
	ROUTE_UNLOCK();
    {
	    register struct rawcb *rp = 0;
	    struct mbuf *n;
	/*
	 * Check to see if we don't want our own messages.
	 */
	if ((so->so_options & SO_USELOOPBACK) == 0) {
		if (route_cb.any_count <= 1) {
			if (rtm)
				Free(rtm);
			m_freem(m);
			return (error);
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}

#ifdef sgi
	/* Because raw input is handled in a separate stream, the 4.4BSD
	 * kludge of trashing rp->rcb_proto.sp_family cannot work.
	 * raw_input() simply queues the packet for later processing,
	 * long after rp->rcb_proto.sp_family has been restored.
	 *
	 * Copy the mbuf, because it is not nice to reuse mbufs that might
	 * be page flipped.
	 */
	if (rtm) {
		n = m_vget(M_DONTWAIT, rtm->rtm_msglen+sizeof(rp), m->m_type);
		m_copyback(n, sizeof(rp), rtm->rtm_msglen, (caddr_t)rtm);
		Free(rtm);
		m_freem(m);
	} else {
		n = m_get(M_DONTWAIT, m->m_type);
		if (n) {
			n->m_len = sizeof(rp);
			n->m_next = m;
		}
	}
	if (n) {
		*mtod(n, struct rawcb **) = rp;
		raw_input(n, &route_proto, &route_src, &route_dst, 0);
	}
#else
	if (rtm) {
		m_copyback(m, 0, rtm->rtm_msglen, (caddr_t)rtm);
		Free(rtm);
	}
	if (dst)
		route_proto.sp_protocol = dst->sa_family;
	if (rp)
		rp->rcb_proto.sp_family = 0;	/* Avoid us */
	raw_input(m, &route_proto, &route_src, &route_dst);
	if (rp)
		rp->rcb_proto.sp_family = PF_ROUTE;
#endif
    }
	return (error);
}

void
rt_setmetrics(__uint32_t which, struct rt_metrics *in, struct rt_metrics *out)
{
#define metric(f, e) if (which & (f)) out->e = in->e;
	metric(RTV_RPIPE, rmx_recvpipe);
	metric(RTV_SPIPE, rmx_sendpipe);
	metric(RTV_SSTHRESH, rmx_ssthresh);
	metric(RTV_RTT, rmx_rtt);
	metric(RTV_RTTVAR, rmx_rttvar);
	metric(RTV_HOPCOUNT, rmx_hopcount);
	metric(RTV_MTU, rmx_mtu);
	metric(RTV_EXPIRE, rmx_expire);
#undef metric
}

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		    : sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif

static void
rt_xaddrs(cp, cplim, rtinfo)
	register caddr_t cp, cplim;
	register struct rt_addrinfo *rtinfo;
{
	register struct sockaddr *sa;
	register int i;
#ifdef NOTYET
	register struct sockaddr_new *sa_new;
#endif /* NOTYET */

	bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));

	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
#ifdef NOTYET
		/*
		 * Ensure that the sa_family field is set properly
		 */
		switch (i) {
			case RTAX_DST: /* destination sockaddr present */
			case RTAX_GATEWAY: /* gateway sockaddr present */
			case RTAX_NETMASK: /* netmask sockaddr present */
			case RTAX_GENMASK: /* cloning mask sockaddr present */
				sa_new = (struct sockaddr_in_new *)sa;
				sa_new->sa_family = AF_INET;
				break;
			default:
				break;
		}
#endif /* NOTYET */

#ifndef _HAVE_SA_LEN
		/*
		 * figure out an sa_len field for the mask cases
		 */
		if ((i == RTAX_NETMASK || i == RTAX_GENMASK)
		    && (sa->sa_family == AF_INET || sa->sa_family == 0)) {
			in_socktrim((struct sockaddr_in_new *)sa);
		}
#endif /* _HAVE_SA_LEN */
	}
}

/*
 * Copy data from a buffer back into the indicated mbuf chain,
 * starting "off" bytes from the beginning, extending the mbuf
 * chain if necessary.
 */
static void
m_copyback(struct  mbuf *m0,
	   register int off,
	   register int len,
	   caddr_t cp)
{
	register int mlen;
	register struct mbuf *m = m0, *n;
	int totlen = 0;

	if (m0 == 0)
		return;
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == 0) {
			n = m_getclr(M_DONTWAIT, m->m_type);
			if (n == 0)
				goto out;
			n->m_len = min(MLEN, len + off);
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		mlen = min (m->m_len - off, len);
		bcopy(cp, off + mtod(m, caddr_t), (unsigned)mlen);
		cp += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->m_next == 0) {
			n = m_get(M_DONTWAIT, m->m_type);
			if (n == 0)
				break;
			n->m_len = min(MLEN, len);
			m->m_next = n;
		}
		m = m->m_next;
	}
#ifndef M_PKTHDR
out:	;
#else
out:	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;
#endif
}

static struct mbuf *
rt_msg1(type, rtinfo)
	int type;
	register struct rt_addrinfo *rtinfo;
{
	register struct rt_msghdr *rtm;
	register struct mbuf *m;
	register int i;
	register struct sockaddr *sa;
	int len, dlen;

	ASSERT(ROUTE_ISLOCKED());
#ifndef M_PKTHDR
	m = m_get(M_DONTWAIT, MT_DATA);
#else
	m = m_gethdr(M_DONTWAIT, MT_DATA);
#endif
	if (m == 0)
		return (m);
	switch (type) {

	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
#ifndef M_PKTHDR
	len += sizeof(struct rawcb *);
	if (len > MLEN)
		panic("rt_msg1");
	m->m_len = len;
#else
	asdf;				/* deal with raw_input() ptr */
	if (len > MHLEN)
		panic("rt_msg1");
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = 0;
#endif
#ifdef sgi
	rtm = &mtod(m, struct rt_in_msghdr *)->u.rtm;
	bzero(mtod(m, void*), len);
#else
	rtm = mtod(m, struct rt_msghdr *);
	bzero((caddr_t)rtm, len);
#endif
	for (i = 0; i < RTAX_MAX; i++) {
		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
#ifdef _HAVE_SA_LEN
		dlen = ROUNDUP(sa->sa_len);
#else
		dlen = ROUNDUP(_FAKE_SA_LEN_DST(sa));
#endif
		m_copyback(m, len, dlen, (caddr_t)sa);
		len += dlen;
	}
#ifdef M_PKTHDR
	if (m->m_pkthdr.len != len) {
		m_freem(m);
		return (NULL);
	}
	rtm->rtm_msglen = len;
#else
	rtm->rtm_msglen = len-sizeof(struct rawcp *);
#endif
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	return (m);
}

static int
rt_msg2(type, rtinfo, cp, w)
	int type;
	register struct rt_addrinfo *rtinfo;
	caddr_t cp;
	struct walkarg *w;
{
	register int i;
	int len, dlen, second_time = 0;
	caddr_t cp0;

	rtinfo->rti_addrs = 0;
again:
	switch (type) {

	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
	if (cp0 = cp)
		cp += len;
	for (i = 0; i < RTAX_MAX; i++) {
		register struct sockaddr *sa;

		if ((sa = rtinfo->rti_info[i]) == 0)
			continue;
		rtinfo->rti_addrs |= (1 << i);
#ifdef _HAVE_SA_LEN
		dlen = ROUNDUP(sa->sa_len);
#else
		dlen = ROUNDUP(_FAKE_SA_LEN_DST(sa));
#endif
		if (cp) {
			bcopy((caddr_t)sa, cp, (unsigned)dlen);
			cp += dlen;
		}
		len += dlen;
	}
	if (cp == 0 && w != NULL && !second_time) {
		register struct walkarg *rw = w;

		rw->w_needed += len;
		if (rw->w_needed <= 0 && rw->w_where) {
			if (rw->w_tmemsize < len) {
#ifdef sgi
				if (rw->w_tmem)
					kmem_free(rw->w_tmem, rw->w_tmemsize);
				rw->w_tmem = (caddr_t)kmem_alloc(len,
								 KM_NOSLEEP);
				if (rw->w_tmem != 0)
					rw->w_tmemsize = len;
#else
				if (rw->w_tmem)
					free(rw->w_tmem, M_RTABLE);
				if (rw->w_tmem = (caddr_t)
						malloc(len, M_RTABLE, M_NOWAIT))
					rw->w_tmemsize = len;
#endif
			}
			if (rw->w_tmem) {
				cp = rw->w_tmem;
				second_time = 1;
				goto again;
			} else
				rw->w_where = 0;
		}
	}
	if (cp) {
		register struct rt_msghdr *rtm = (struct rt_msghdr *)cp0;

		rtm->rtm_version = RTM_VERSION;
		rtm->rtm_type = type;
		rtm->rtm_msglen = len;
	}
	return (len);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that a redirect has occured, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination.
 */
void
rt_missmsg(int type, struct rt_addrinfo *rtinfo, int flags, int error)
{
	register struct rt_msghdr *rtm;
	register struct mbuf *m;
	struct sockaddr *sa = rtinfo->rti_info[RTAX_DST];
	struct sockproto route_proto = route_proto0;

	ASSERT(ROUTE_ISLOCKED());
	if (route_cb.any_count == 0)
		return;
	m = rt_msg1(type, rtinfo);
	if (m == 0)
		return;
#ifdef sgi
	rtm = &mtod(m, struct rt_in_msghdr *)->u.rtm;
#else
	rtm = mtod(m, struct rt_msghdr *);
#endif
	rtm->rtm_flags = RTF_DONE | flags;
	rtm->rtm_errno = error;
	rtm->rtm_addrs = rtinfo->rti_addrs;
	route_proto.sp_protocol = sa ? sa->sa_family : 0;
	raw_input(m, &route_proto, &route_src, &route_dst, 0);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that the status of a network interface has changed.
 */
void
rt_ifmsg(register struct ifnet *ifp)
{
	register struct if_msghdr *ifm;
	struct mbuf *m;
	struct rt_addrinfo info;
	struct sockproto route_proto = route_proto0;

	if (route_cb.any_count == 0)
		return;
	bzero((caddr_t)&info, sizeof(info));
	ROUTE_RDLOCK();
	m = rt_msg1(RTM_IFINFO, &info);
	ROUTE_UNLOCK();
	if (m == 0)
		return;
#ifdef sgi
	ifm = &mtod(m, struct rt_in_msghdr *)->u.ifm;
#else
	ifm = mtod(m, struct if_msghdr *);
#endif
	ifm->ifm_index = ifp->if_index;
	ifm->ifm_flags = ifp->if_flags;
	ifm->ifm_data = ifp->if_data;
	ifm->ifm_addrs = 0;
	route_proto.sp_protocol = 0;
	raw_input(m, &route_proto, &route_src, &route_dst, 0);
}

/*
 * This is called to generate messages from the routing socket
 * indicating a network interface has had addresses associated with it.
 * if we ever reverse the logic and replace messages TO the routing
 * socket indicate a request to configure interfaces, then it will
 * be unnecessary as the routing socket will automatically generate
 * copies of it.
 */
void
rt_newaddrmsg(cmd, ifa, error, rt)
	int cmd, error;
	register struct ifaddr *ifa;
	register struct rtentry *rt;
{
	struct rt_addrinfo info;
	struct sockaddr *sa;
	int pass;
	struct mbuf *m;
#ifdef INET6
	struct ifaddr *ia;
#else
	struct in_ifaddr *ia;
#endif
	struct ifnet *ifp = ifa->ifa_ifp;
	struct sockproto route_proto = route_proto0;

	if (route_cb.any_count == 0)
		return;
	for (pass = 1; pass < 3; pass++) {
		bzero((caddr_t)&info, sizeof(info));
		if ((cmd == RTM_ADD && pass == 1) ||
		    (cmd == RTM_DELETE && pass == 2)) {
			register struct ifa_msghdr *ifam;
			int ncmd = cmd == RTM_ADD ? RTM_NEWADDR : RTM_DELADDR;

			ifaaddr = sa = ifa->ifa_addr;
#ifdef INET6
			ia = (struct ifaddr *)(ifp->in_ifaddr);
/* XXX6 Is this what we want to do when there is no v4 addr? */
			if (ia == NULL) /* no v4 addr */
				ia = (struct ifaddr *)(ifp->in6_ifaddr);
			ifpaddr = ia->ifa_addr;
#else
			ia = (struct in_ifaddr *)(ifp->in_ifaddr);
			ifpaddr = ia->ia_ifa.ifa_addr;
#endif

			netmask = ifa->ifa_netmask;
			brdaddr = ifa->ifa_dstaddr;
			if ((m = rt_msg1(ncmd, &info)) == NULL)
				continue;
#ifdef sgi
			ifam = &mtod(m, struct rt_in_msghdr *)->u.ifa;
#else
			ifam = mtod(m, struct ifa_msghdr *);
#endif
			ifam->ifam_index = ifp->if_index;
			ifam->ifam_metric = ifa->ifa_metric;
			ifam->ifam_flags = ifa->ifa_flags;
			ifam->ifam_addrs = info.rti_addrs;
		}
		if ((cmd == RTM_ADD && pass == 2) ||
		    (cmd == RTM_DELETE && pass == 1)) {
			register struct rt_msghdr *rtm;

			if (rt == 0)
				continue;
			netmask = rt_mask(rt);
			dst = sa = rt_key(rt);
			gate = rt->rt_gateway;
			if ((m = rt_msg1(cmd, &info)) == NULL)
				continue;
#ifdef sgi
			rtm = &mtod(m, struct rt_in_msghdr *)->u.rtm;
#else
			rtm = mtod(m, struct rt_msghdr *);
#endif
			rtm->rtm_index = ifp->if_index;
			rtm->rtm_flags |= rt->rt_flags;
			rtm->rtm_errno = error;
			rtm->rtm_addrs = info.rti_addrs;
		}
		route_proto.sp_protocol = sa ? sa->sa_family : 0;
		raw_input(m, &route_proto, &route_src, &route_dst, 0);
	}
}

/*
 * This is used in dumping the kernel table via sysctl().
 */
int
sysctl_dumpentry(struct radix_node *rn, struct walkarg *w)
{
	register struct rtentry *rt = (struct rtentry *)rn;
	int error = 0, size;
	struct rt_addrinfo info;

	if (w->w_op == NET_RT_FLAGS && !(rt->rt_flags & w->w_arg))
		return 0;
	bzero((caddr_t)&info, sizeof(info));
	dst = rt_key(rt);
	gate = rt->rt_gateway;
	netmask = rt_mask(rt);
	genmask = rt->rt_genmask;
	if (rt->rt_ifp) {
		struct ifaddr *ia = (struct ifaddr *)(rt->rt_ifp->in_ifaddr);
#ifdef INET6
		/*
		 * To maintain binary backwards compatibility we never return
		 * an IPv6 addr even if the interface only has IPv4
		 * addresses.  The user program may not be able to handle
		 * an IPv6 addr.
		 */
#endif
		ifpaddr = ia ? ((struct in_ifaddr *)ia)->ia_ifa.ifa_addr : 0;

		ifaaddr = rt->rt_ifa->ifa_addr;
		if (rt->rt_ifp->if_flags & IFF_POINTOPOINT)
			brdaddr = rt->rt_ifa->ifa_dstaddr;
	}
	size = rt_msg2(RTM_GET, &info, 0, w);
	if (w->w_where && w->w_tmem) {
		register struct rt_msghdr *rtm = (struct rt_msghdr *)w->w_tmem;

		rtm->rtm_flags = rt->rt_flags;
		rtm->rtm_use = rt->rt_use;
		rtm->rtm_rmx = rt->rt_rmx;
		rtm->rtm_index = rt->rt_ifp->if_index;
		rtm->rtm_errno = rtm->rtm_pid = rtm->rtm_seq = 0;
		rtm->rtm_addrs = info.rti_addrs;
#ifdef sgi
		if (w->w_given >= size) {
			bcopy(rtm, w->w_where, size);
			w->w_where += size;
			w->w_given -= size;
		} else {
			w->w_where = NULL;
			error = EFAULT;
		}
#else
		if (error = copyout((caddr_t)rtm, w->w_where, size))
			w->w_where = NULL;
		else
			w->w_where += size;
#endif
	}
	return (error);
}

/*
 * We return zero for everything unless there is an error.  That way the
 * enumeration continues.
 */
int
iflist_enum(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	struct walkarg *w = (struct walkarg *)arg1;
	int af = (int)(__psunsigned_t)arg2;	/* gag */
	struct ifnet *ifp = (struct ifnet *)key;
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct ifaddr *ifa;
	struct	rt_addrinfo info;
	int len;

	/* skip ones that aren't on this interface */
	if (ia->ia_ifp != ifp) {
		return 0;
	}
	/* skip primary; it's been done */
	if (ia == (struct in_ifaddr *)(ifp->in_ifaddr)) {
		return 0;
	}
	ifa = &(ia->ia_ifa);

	/* Not the right family?  Forget it. */
	if (af && af != ifa->ifa_addr->sa_family) {
		return 0;
	}
	bzero((caddr_t)&info, sizeof(info));
	ifaaddr = ifa->ifa_addr;
	netmask = ifa->ifa_netmask;
	brdaddr = ifa->ifa_dstaddr;
	len = rt_msg2(RTM_NEWADDR, &info, 0, w);

	if (w->w_where && w->w_tmem) {
		register struct ifa_msghdr *ifam;

		ifam = (struct ifa_msghdr *)w->w_tmem;
		ifam->ifam_index = ifa->ifa_ifp->if_index;
		ifam->ifam_flags = ifa->ifa_flags;
		ifam->ifam_metric = ifa->ifa_metric;
		ifam->ifam_addrs = info.rti_addrs;

		if (w->w_given >= len) {
			bcopy(w->w_tmem, w->w_where, len);
			w->w_given -= len;
		} else {
			return ENOMEM;
		}
		w->w_where += len;
	}
	return 0;
}

int
sysctl_iflist(int af, struct walkarg *w)
{
	register struct ifnet *ifp;
	register struct ifaddr *ifa;
	struct	rt_addrinfo info;
	int	len;
	int error;
	struct in_ifaddr *ia;

	bzero((caddr_t)&info, sizeof(info));

	for (ifp = ifnet; ifp; ifp = ifp->if_next) {
		if (w->w_arg && (w->w_arg != ifp->if_index)) {
			continue;
		}
		/*
		 * First message returned must contain the AF_LINK address
		 * for the interface, followed by 'n' new address records
		 */
		ifa = ifnet_addrs[ifp->if_index - 1];
		ifpaddr = ifa->ifa_addr;
		len = rt_msg2(RTM_IFINFO, &info, (caddr_t)0, w);
		ifpaddr = 0;

		if (w->w_where && w->w_tmem) {
			register struct if_msghdr *ifm;

			ifm = (struct if_msghdr *)w->w_tmem;
			ifm->ifm_index = ifp->if_index;
			ifm->ifm_flags = ifp->if_flags;
			ifm->ifm_data = ifp->if_data;
			ifm->ifm_addrs = info.rti_addrs;

			if (w->w_given >= len) {
				bcopy(ifm, w->w_where, len);
				w->w_given -= len;
			} else {
				return ENOMEM;
			}
			w->w_where += len;
		}
		if (ifp->in_ifaddr == 0) {
			continue;
		}
		/*
		 * Subsequent msgs return addresses associated with interface.
		 * Hit primary first, then aliases.
		 */
		ia = (struct in_ifaddr *)(ifp->in_ifaddr);
		ifa = &(ia->ia_ifa);

		if (af && af != ifa->ifa_addr->sa_family) {
			continue;
		}
		ifaaddr = ifa->ifa_addr;
		netmask = ifa->ifa_netmask;
		brdaddr = ifa->ifa_dstaddr;
		len = rt_msg2(RTM_NEWADDR, &info, 0, w);

		if (w->w_where && w->w_tmem) {
			register struct ifa_msghdr *ifam;

			ifam = (struct ifa_msghdr *)w->w_tmem;
			ifam->ifam_index = ifa->ifa_ifp->if_index;
			ifam->ifam_flags = ifa->ifa_flags;
			ifam->ifam_metric = ifa->ifa_metric;
			ifam->ifam_addrs = info.rti_addrs;

			if (w->w_given >= len) {
				bcopy(w->w_tmem, w->w_where, len);
				w->w_given -= len;
			} else {
				return ENOMEM;
			}
			w->w_where += len;
		}
		/* We are in an oasis of cheese. */
		error = (int)(__psunsigned_t)
			hash_enum(&hashinfo_inaddr, iflist_enum,
				  HTF_INET, (caddr_t)ifp, (caddr_t)w,
				  (caddr_t)(long)af);
		if (error) {
			return error;
		}
#ifdef _HAVE_SA_LEN
		ifaaddr = netmask = brdaddr = 0;
#else
		ifaaddr = brdaddr = 0;
		netmask = 0;
#endif
	}
	return (0);
}

/* ARGSUSED */
int
sysctl_rtable(int	*name,
	      int	namelen,
	      caddr_t	where,
	      size_t	*given,
	      caddr_t	*new,
	      size_t	newlen)
{
	register struct radix_node_head *rnh;
	int	i, error = EINVAL;
	u_char  af;
	struct	walkarg w;

	if (new)
		return (EPERM);
	if (namelen != 3)
		return (EINVAL);
	af = name[0];
	Bzero(&w, sizeof(w));
	w.w_where = where;
	w.w_given = *given;
	w.w_needed = 0 - w.w_given;
	w.w_op = name[1];
	w.w_arg = name[2];

	switch (w.w_op) {

	case NET_RT_DUMP:
	case NET_RT_FLAGS:

		ROUTE_RDLOCK();
		for (i = 1; i <= AF_MAX; i++)
			if ((rnh = rt_tables[i]) && (af == 0 || af == i) &&
			    (error = rnh->rnh_walktree(rnh,
							sysctl_dumpentry, &w)))
				break;
		ROUTE_UNLOCK();
		break;

	case NET_RT_IFLIST:
		error = sysctl_iflist(af, &w);
	}

#ifdef sgi
	if (w.w_tmem)
		kmem_free(w.w_tmem, w.w_tmemsize);
	w.w_needed += *given;		/* w.w_given is changed in IRIX */
#else
	if (w.w_tmem)
		free(w.w_tmem, M_RTABLE);
	w.w_needed += w.w_given;
#endif
	if (where) {
		*given = w.w_where - where;
		if (*given < w.w_needed)
			return (ENOMEM);
	} else {
		*given = (11 * w.w_needed + 10) / 10;
	}
	return (error);
}

/*
 * Definitions of protocols supported in the ROUTE domain.
 */
extern struct domain routedomain;		/* or at least forward ref */

struct protosw routesw[] = {
{ SOCK_RAW,	&routedomain,	0,		PR_ATOMIC|PR_ADDR,
	/* do not use raw_input() because it has the wrong # of args */
  0,		route_output,	route_ctlinput,	0,
  route_usrreq,
  raw_init,	0,		0,		0,
}
};

struct domain routedomain =
    { PF_ROUTE, "route", route_init, 0, 0,
      routesw, &routesw[sizeof(routesw)/sizeof(routesw[0])] };
