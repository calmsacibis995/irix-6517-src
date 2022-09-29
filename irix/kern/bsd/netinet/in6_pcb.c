#ifdef INET6
/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/in6_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/if_ether.h>
#include <netinet/if_ndp6.h>

#ifndef BUCKET_INVAL
#define BUCKET_INVAL ((u_short)-1)
#endif

extern u_short in_pcbnextport(struct inpcb *inp);
extern int in_ifaddr_count;
extern int in6_ifaddr_count;
extern u_char inetctlerrmap[];

struct	in6_addr zeroin6_addr = {{{0, 0, 0, 0}}};
extern struct sockaddr_in6 in6_zeroaddr;

extern void in_pcbrehash(struct inpcb *head, struct inpcb *inp);

int
in6_pcbbind(inp, nam)
	register struct inpcb *inp;
	struct mbuf *nam;
{
	register struct socket *so = inp->inp_socket;
	struct inpcb *head = inp->inp_head;
	register struct sockaddr_in6 *sin6;
	register struct sockaddr_in *sin;
	u_int32_t flowinfo = 0;
	u_int16_t lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);
	int error, atype, tos = 0;

	ASSERT(SOCKET_ISLOCKED(so));
	if (inp->inp_lport || inp->inp_latype != IPATYPE_UNBD)
		return (EINVAL);
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
	     (so->so_options & SO_ACCEPTCONN) == 0))
		wild = INPLOOKUP_WILDCARD;
	atype = IPATYPE_UNBD;
	if (nam == 0)
		goto noname;

	/* deal with address in nam mbuf */
	sin6 = mtod(nam, struct sockaddr_in6 *);
	if (nam->m_len != sizeof (*sin6))
		return (EINVAL);
	if (sin6->sin6_family != AF_INET6)
		return (EAFNOSUPPORT);
	lport = sin6->sin6_port;
	/* classify address */
	if (IS_ANYSOCKADDR(sin6)) {
		if ((in_ifaddr_count == 0) && (in6_ifaddr_count == 0))
			return (EADDRNOTAVAIL);
		atype = IPATYPE_UNBD;
	} else if (IS_IPV4SOCKADDR(sin6)) {
		if (in_ifaddr_count == 0)
			return (EADDRNOTAVAIL);
		atype = IPATYPE_IPV4;
	} else {
		if (in6_ifaddr_count == 0)
			return (EADDRNOTAVAIL);
		atype = IPATYPE_IPV6;
	}
	/* deal with flowinfo */
	flowinfo = sin6->sin6_flowinfo;
	if ((flowinfo & ~IPV6_FLOWINFO_PRIFLOW) != 0)
		return (EINVAL);
	if ((atype == IPATYPE_IPV4) &&
	    ((flowinfo & IPV6_FLOWINFO_FLOWLABEL) != 0))
		return (EADDRNOTAVAIL);
	/* try to remap Priority to TOS */
	switch (flowinfo & IPV6_FLOWINFO_PRIORITY) {
	    case IPV6_PRIORITY_INTERACTIVE:
		tos = IPTOS_LOWDELAY;
		break;
	    case IPV6_PRIORITY_BULK:
		tos = IPTOS_THROUGHPUT;
		break;
	}
	if (atype == IPATYPE_IPV6) {
		if (IS_MULTIADDR6(sin6->sin6_addr)) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow complete duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if (so->so_options & SO_REUSEADDR)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
		} else {
			sin6->sin6_flowinfo = 0;	/* yech... */
			sin6->sin6_port = 0;
			if (ifa_ifwithaddr((struct sockaddr *)sin6) == 0)
				return (EADDRNOTAVAIL);
		}
		COPY_ADDR6(sin6->sin6_addr, inp->inp_laddr6);
		if (lport) {
			struct inpcb *t;

			/* GROSS */
			if (ntohs(lport) < IPPORT_RESERVED &&
			    !_CAP_ABLE(CAP_PRIV_PORT))
				return (EACCES);
			mutex_lock(&head->inp_mutex, PZERO);
			t = in_pcblookupx(head, &zeroin6_addr, 0,
				&sin6->sin6_addr, lport, wild, AF_INET6);
			if (t && (reuseport & t->inp_socket->so_options) == 0) {
				mutex_unlock(&head->inp_mutex);
				return (EADDRINUSE);
			}
		} else {
			/* no port given */
			struct inpcb *t;

			t = NULL;
			do {
				if (t) {
					mutex_unlock(&head->inp_mutex);
					if (t != inp) {
					     struct socket *so2 = t->inp_socket;
					     SOCKET_LOCK(so2);
					     if (!INPCB_RELE(t))
						 SOCKET_UNLOCK(so2);
					} else {
					     INPCB_RELE(t);
					}
				}
				lport = in_pcbnextport(inp);
				mutex_lock(&head->inp_mutex, PZERO);
			} while (t = in_pcblookupx(head, &zeroin6_addr, 0,
					&inp->inp_laddr6, lport, wild, AF_INET6));
		}
		INHHEAD_LOCK(inp->inp_hhead);
		inp->inp_flags &= ~INP_COMPATV4;
		inp->inp_latype = atype;
		inp->inp_lport = lport;
		if (tos)
			inp->inp_tos = tos;
		if (flowinfo)
			inp->inp_oflowinfo = flowinfo;
		in_pcbrehash(head, inp);
		INHHEAD_UNLOCK(inp->inp_hhead);
		mutex_unlock(&head->inp_mutex);
		return (0);
	}

	/* backpatch name */
	sin = mtod(nam, struct sockaddr_in *);
#ifdef _HAVE_SIN_LEN 
	sin->sin_len = sizeof(struct sockaddr_in);
#endif
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = sin6->sin6_addr.s6_addr32[3];
	bzero(sin->sin_zero, 8);
	nam->m_len = sizeof(struct sockaddr_in);

    noname:
	if ((error = in_pcbbind(inp, nam)) != 0)
		return (error);
	inp->inp_flags |= INP_COMPATANY;
	if (tos)
		inp->inp_tos = tos;
	if (flowinfo)
		inp->inp_oflowinfo = flowinfo;
	return (0);
}

/*
 * Change the version of local address (GROSS!)
 */
int
in6_pcbrebind(inp, af)
	register struct inpcb *inp;
	int af;
{
	register struct in_ifaddr *ia;
	register struct ifnet *ifp;
	register struct in6_ifaddr *ia6;

	switch (af) {
	case AF_INET:
		/* local address must be IPv6 interface address */
		if (inp->inp_latype != IPATYPE_IPV6)
			return (EADDRNOTAVAIL);
		if (IS_MULTIADDR6(inp->inp_laddr6))
			return (EADDRNOTAVAIL);
		if (in6_ifaddr_count == 0)
			goto unbind;
		if (in_ifaddr_count == 0)
			goto unbind;
		ia6 = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
		  in6addr_match, (caddr_t)&(inp->inp_laddr6),
		  (caddr_t)0, (caddr_t)0);
		if (ia6 == 0)
			goto unbind;
		ifp = ia6->ia_ifp;
		ia = (struct in_ifaddr *)ifp->in_ifaddr;
		if (ia == 0)
			goto unbind;

		inp->inp_flags |= INP_COMPATANY;
		inp->inp_laddr6.s6_addr32[0] = 0;
		inp->inp_laddr6.s6_addr32[1] = 0;
		inp->inp_laddr6.s6_addr32[2] = htonl(0xffff);
		inp->inp_laddr6.s6_addr32[3] = ia->ia_addr.sin_addr.s_addr;
		inp->inp_latype = IPATYPE_IPV4;
		in_pcbrehash(inp->inp_head, inp);
		return (0);

	case AF_INET6:
		/* local address must be IPv4 interface address */
		if (inp->inp_latype != IPATYPE_IPV4)
			return (EADDRNOTAVAIL);
		if (IN_MULTICAST(ntohl(inp->inp_laddr.s_addr)))
			return (EADDRNOTAVAIL);
		if (in_ifaddr_count == 0)
			goto unbind;
		if (in6_ifaddr_count == 0)
			goto unbind;
		ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
		  inaddr_match, (caddr_t)&(inp->inp_laddr),
		  (caddr_t)0, (caddr_t)0);
		if (ia == 0)
			goto unbind;
		ifp = ia->ia_ifp;
		ia6 = (struct in6_ifaddr *)ifp->in6_ifaddr;
		if (ia6 == 0)
			goto unbind;

		inp->inp_flags &= ~INP_COMPATV4;
		COPY_ADDR6(ia6->ia_addr.sin6_addr, inp->inp_laddr6);
		inp->inp_latype = IPATYPE_IPV6;
		in_pcbrehash(inp->inp_head, inp);
		return (0);

	unbind:
		/* TODO: stats ! */
		inp->inp_flags |= INP_COMPATANY;
		CLR_ADDR6(inp->inp_laddr6);
		inp->inp_latype = IPATYPE_UNBD;
		in_pcbrehash(inp->inp_head, inp);
		return (0);

	default:
		return (EAFNOSUPPORT);
	}
}


/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in6_pcbconnect(inp, nam)
	register struct inpcb *inp;
	struct mbuf *nam;
{
	struct sockaddr_in6 *ifaddr6 = 0;
	register struct sockaddr_in6 *sin6 = mtod(nam, struct sockaddr_in6 *);
	register struct sockaddr_in *sin;
	u_int32_t flowinfo;
	int error, tos = 0;
	struct inpcb *head;
	struct in6_ifaddr *ia;
	int atype;
	struct ifnet *ifp;

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	if (nam->m_len != sizeof (*sin6))
		return (EINVAL);
	if (sin6->sin6_family != AF_INET6)
		return (EAFNOSUPPORT);
	if (sin6->sin6_port == 0)
		return (EADDRNOTAVAIL);
	/* classify address */
	if (IS_ANYSOCKADDR(sin6)) {
		/* IPv4 centric! */
		if (inp->inp_latype != IPATYPE_IPV6) {
			if (in_ifaddr_count == 0)
				return (EADDRNOTAVAIL);
			sin6->sin6_addr.s6_addr32[2] = htonl(0xffff);
			sin6->sin6_addr.s6_addr32[3] =
			  ((struct in_ifaddr *)ifnet->in_ifaddr)->ia_addr.sin_addr.s_addr;
			atype = IPATYPE_IPV4;
		} else {
			if (in6_ifaddr_count == 0)
				return (EADDRNOTAVAIL);
			/*
			 * Normally at this point, we would take the primary
			 * addr from the first ifp.  However, not all
			 * ifp's will have an ipv6 address so scan forward and
			 * find the first ifp with an ipv6 addr.  Note,
			 * since in6_ifaddr_count != 0 at this point we will
			 * always find an interface with an ipv6 addr.
			 */
			for (ifp = ifnet; ifp != NULL; ifp = ifp->if_next)
				if (ifp->in6_ifaddr != NULL)
					break;
			COPY_ADDR6(((struct in6_ifaddr *)
			  ifp->in6_ifaddr)->ia_addr.sin6_addr,
			  sin6->sin6_addr);
			atype = IPATYPE_IPV6;
		}
	} else if (IS_IPV4SOCKADDR(sin6)) {
		if (inp->inp_latype == IPATYPE_IPV6) {
			if ((error = in6_pcbrebind(inp, AF_INET)) != 0)
				return (error);
		}
		atype = IPATYPE_IPV4;
	} else {
		if (inp->inp_latype == IPATYPE_IPV4) {
			if ((error = in6_pcbrebind(inp, AF_INET6)) != 0)
				return (error);
		}
		atype = IPATYPE_IPV6;
	}
	/* deal with flowinfo */
	flowinfo = sin6->sin6_flowinfo;
	if ((flowinfo & ~IPV6_FLOWINFO_PRIFLOW) != 0)
		return (EINVAL);
	if ((atype == IPATYPE_IPV4) &&
	    ((flowinfo & IPV6_FLOWINFO_FLOWLABEL) != 0))
		return (EADDRNOTAVAIL);

	/* try to remap Priority to TOS */
	switch (flowinfo & IPV6_FLOWINFO_PRIORITY) {
	    case IPV6_PRIORITY_INTERACTIVE:
		tos = IPTOS_LOWDELAY;
		break;
	    case IPV6_PRIORITY_BULK:
		tos = IPTOS_THROUGHPUT;
		break;
	}

	if (atype != IPATYPE_IPV6)
		goto version4;

	if (inp->inp_latype == IPATYPE_UNBD) {
		register struct route *ro;

		ia = (struct in6_ifaddr *)0;
		/* 
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 */
		ROUTE_RDLOCK();
		ro = &inp->inp_route;
		if (ro->ro_rt &&
		    (satosin6(&ro->ro_dst)->sin6_family != AF_INET6 ||
		     !SAME_SOCKADDR(satosin6(&ro->ro_dst), sin6) ||
		     inp->inp_socket->so_options & SO_DONTROUTE)) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)0;
		}
		if ((inp->inp_socket->so_options & SO_DONTROUTE) == 0 && /*XXX*/
		    (ro->ro_rt == (struct rtentry *)0 ||
		    ro->ro_rt->rt_ifp == (struct ifnet *)0)) {
			/* No route yet, so try to acquire one */
			ro->ro_dst.sa_family = AF_INET6;
			((struct sockaddr_new *)&ro->ro_dst)->sa_len =
			  sizeof(struct sockaddr_in6);
			COPY_ADDR6(sin6->sin6_addr,
				   satosin6(&ro->ro_dst)->sin6_addr);
			in6_rtalloc(ro, INP_IFA);
		}
		/*
		 * If we found a route, use the address
		 * corresponding to the outgoing interface
		 * unless it is the loopback (in case a route
		 * to our address on another net goes to loopback).
		 */
		if (ro->ro_rt && !(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))
			ia = ifatoia6(ro->ro_rt->rt_ifa);
		ROUTE_UNLOCK();
		if (ia == 0) {
			u_int16_t fport = sin6->sin6_port;

			sin6->sin6_port = 0;
			ia = ifatoia6(ifa_ifwithdstaddr(sin6tosa(sin6)));
			if (ia == 0)
				ia = ifatoia6(ifa_ifwithnet(sin6tosa(sin6)));
			sin6->sin6_port = fport;
			if (ia == 0)
				ia = (struct in6_ifaddr *)(ifnet->in6_ifaddr);
			if (ia == 0)
				return (EADDRNOTAVAIL);
		}
		/*
		 * If the destination address is multicast and an outgoing
		 * interface has been set as a multicast option, use the
		 * address of that interface as our source address.
		 */
		if (IS_MULTIADDR6(sin6->sin6_addr) &&
		    inp->inp_moptions != NULL) {
			struct ip_moptions *imo;
			struct ifnet *ifp;

			if (inp->inp_moptions != NULL)
				imo = mtod(inp->inp_moptions,
				  struct ip_moptions *);
			else
				imo = NULL;
			if (imo->imo_multicast_ifp != NULL) {
				ifp = imo->imo_multicast_ifp;
				ia = (struct in6_ifaddr *)hash_enum(
				  &hashinfo_in6addr, ifnet_enummatch, HTF_INET,
				  (caddr_t)0, (caddr_t)ifp, (caddr_t)0);
				if (ia == 0)
					return (EADDRNOTAVAIL);
			}
		}
		ifaddr6 = &ia->ia_addr;

	}

	if (inp->inp_latype == IPATYPE_UNBD) {
		if (inp->inp_lport == 0) {
			if (error = in6_pcbbind(inp, (struct mbuf *)0))
				return (error);
		}
		COPY_ADDR6(ifaddr6->sin6_addr, inp->inp_laddr6);
		inp->inp_latype = IPATYPE_IPV6;
	}
	head = inp->inp_head;
	mutex_lock(&head->inp_mutex, PZERO);
	if (in_pcblookupx(inp->inp_head,
		   &sin6->sin6_addr,
		   sin6->sin6_port,
		   inp->inp_latype ? &inp->inp_laddr6 : &ifaddr6->sin6_addr,
		   inp->inp_lport, INPLOOKUP_ALL, AF_INET6) != NULL) {
		mutex_unlock(&head->inp_mutex);
		return (EADDRINUSE);
	}

	if (inp->inp_hashval != BUCKET_INVAL)
		INHHEAD_LOCK(inp->inp_hhead);
	inp->inp_flags &= ~INP_COMPATV4;
	COPY_ADDR6(sin6->sin6_addr, inp->inp_faddr6);
	inp->inp_fatype = IPATYPE_IPV6;
	inp->inp_fport = sin6->sin6_port;
	in_pcbrehash(head, inp); /* returns with INHHEAD_UNLOCK held */
	INHHEAD_UNLOCK(inp->inp_hhead);
	mutex_unlock(&head->inp_mutex);
	if (tos)
		inp->inp_tos = tos;
	if (flowinfo)
		inp->inp_oflowinfo = flowinfo;
	return (0);

version4:
	/* backpatch name */
	sin = mtod(nam, struct sockaddr_in *);
#ifdef _HAVE_SIN_LEN 
	sin->sin_len = sizeof(struct sockaddr_in);
#endif
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = sin6->sin6_addr.s6_addr32[3];
	bzero(sin->sin_zero, 8);
	nam->m_len = sizeof(struct sockaddr_in);
	if (error = in_pcbconnect(inp, nam))
		return (error);
	if (tos)
		inp->inp_tos = tos;
	if (flowinfo)
		inp->inp_oflowinfo = flowinfo;
	return (0);
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 *
 * Must be called at splnet.
 */
#define MAX_PER_HASH	512
/*ARGSUSED*/
void
in6_pcbnotify(
	struct inpcb *head,
	struct sockaddr *dst,
	u_int fport_arg,
	struct in6_addr *laddr,
	u_int lport_arg,
	int cmd,
	void (*notify)(struct inpcb *, int, void *),
	void *data)
{
	register struct inpcb *inp;
	struct sockaddr_in6 *dst6 = (struct sockaddr_in6 *)dst;
	u_int16_t fport = fport_arg, lport = lport_arg;
	int i;
	int hash, cnt;
	struct in_pcbhead *hinp;
	int start = 1, end = head->inp_tablesz;
	struct inpcb **notify_list;
	int errno;
	extern void in_rtchange(struct inpcb *, int, void *);

	if (((unsigned)cmd > PRC_NCMDS) ||
	  (dst == NULL) || (_FAKE_SA_FAMILY(dst) != AF_INET6))
		return;
	if (IS_ANYSOCKADDR(dst6) || IS_IPV4SOCKADDR(dst6))
		return;

	/*
	 * Redirects go to all references to the destination,
	 * and use in_rtchange to invalidate the route cache.
	 * Dead host indications: notify all references to the destination.
	 * Otherwise, if we have knowledge of the local port and address,
	 * deliver only to that socket.
	 */
	if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) {
		fport = 0;
		lport = 0;
		laddr = NULL;
		if (cmd != PRC_HOSTDEAD)
			notify = in_rtchange;
	}
	if (head->inp_hashflags & INPFLAGS_CLTS) {
		if (lport) {
			start = (*head->inp_hashfun)(head, &dst6->sin6_addr,
			  fport, &laddr, lport, AF_INET6);
			end = start + 1;
		}
	} else {
		/* This ignores TIME-WAITers for TCP, but who cares? */
		if (!IS_ANYSOCKADDR(dst6) && laddr != NULL &&
		  !IS_ANYADDR6(*laddr) && lport && fport) {
			start = (*head->inp_hashfun)(head, &dst6->sin6_addr,
			  fport, &laddr, lport, AF_INET6);
			end = start + 1;
		}
	}
	errno = inetctlerrmap[cmd];
	notify_list = (struct inpcb **)kmem_alloc(
	  sizeof(struct inpcb *) * (end - start) * MAX_PER_HASH, KM_SLEEP);
	cnt = 0;
	for (hash = start; hash < end; hash++) {
		hinp = &head->inp_table[hash];
		INHHEAD_LOCK(hinp);
		for (inp = hinp->hinp_next; inp != (struct inpcb *)hinp;
		  inp = inp->inp_next) {
			ASSERT(inp->inp_socket != 0);
			if ((inp->inp_flags & INP_COMPATV6) == 0 ||
			    (inp->inp_fatype & IPATYPE_IPV6) == 0 ||
			    !SAME_ADDR6(inp->inp_faddr6, dst6->sin6_addr) ||
			    inp->inp_socket == 0 ||
			    (lport && inp->inp_lport != lport) ||
			    (laddr &&
			    ((inp->inp_latype & IPATYPE_IPV6) == 0 ||
			    !SAME_ADDR6(inp->inp_laddr6, *laddr))) ||
			    (fport && inp->inp_fport != fport)) 
				continue;
			INPCB_HOLD(inp);
			notify_list[cnt++] = inp;
			ASSERT(cnt <= MAX_PER_HASH * (end - start));
		}
		INHHEAD_UNLOCK(hinp);
	}
	for (i = 0; i < cnt; i++) {
	        struct socket *so2;
		inp = notify_list[i];
		so2 = inp->inp_socket;
		SOCKET_LOCK(so2);
		(*notify)(inp, errno, data);
		if (!INPCB_RELE(inp))
			SOCKET_UNLOCK(so2);
	}
}

/*
 * Clone an host route.
 */
struct rtentry *
in6_rthost(dst)
	struct in6_addr *dst;
{
	struct rtentry *rt, *newrt;
	struct sockaddr_new *genmask;
	extern int arpt_keep;
	struct sockaddr_in6 sindst = in6_zeroaddr;

	ASSERT(ROUTE_ISLOCKED());
	COPY_ADDR6(*dst, sindst.sin6_addr);
	rt = rtalloc1(sin6tosa(&sindst), 1);
	if (rt == 0)
		return rt;
	RT_RELE(rt);
	if (rt->rt_rmx.rmx_mtu == 0)
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
	if (rt->rt_flags & RTF_HOST)
		return rt;
	genmask = rt->rt_genmask;
	rt->rt_genmask = (struct sockaddr_new *)0;
	newrt = rt;
	if (rtrequest(RTM_RESOLVE, sin6tosa(&sindst),
		      (struct sockaddr *)0, (struct sockaddr_new *)0,
		      0, &newrt)) {
		rt->rt_genmask = genmask;
		return rt;
	}
	rt->rt_genmask = genmask;
	rt = newrt;
	RT_RELE(rt);
	if ((rt->rt_flags & RTF_HOST) == 0)
		return rt;
	rt->rt_flags |= RTF_DYNAMIC;
	/* TODO: use a dedicated delay */
	if (rt->rt_rmx.rmx_expire == 0)
		rt->rt_rmx.rmx_expire = time + arpt_keep;
	return rt;
}

/*
 * Apply strong multi-homed model to link-local stuff.
 */
void
in6_rtalloc(ro, ifa)
	struct route *ro;
	struct ifaddr *ifa;
{
	struct rtentry *rt;
	struct sockaddr_in6 *sin = satosin6(&ro->ro_dst);
	struct ifnet *ifp;
	struct ifaddr *oifa;

	ASSERT(ROUTE_ISLOCKED());
	rtalloc(ro);
	if ((ifa == 0) ||
	    ((ifp = ifa->ifa_ifp) == 0) ||
	    ((ifp->if_ndtype & IFND6_INLL) == 0) ||
	    ((ifp->if_ndtype & IFND6_LLSET) == 0) ||
	    ((rt = ro->ro_rt) == 0) ||
	    (rt->rt_ifp == ifp) ||
	    (rt->rt_flags & RTF_GATEWAY) ||
	    ((oifa = rt->rt_ifa) == 0) ||
	    !IS_LOCALADDR6(sin->sin6_addr) ||
	    (!IFND6_IS_LLINK(rt->rt_ifp) &&
	     (rt->rt_ifp->if_ndtype & IFND6_INLL) == 0))
		return;
	ro->ro_rt = rt = in6_rthost(&sin->sin6_addr);
#define	MASK	(RTF_UP|RTF_GATEWAY|RTF_HOST|RTF_DYNAMIC|RTF_LLINFO|RTF_STATIC)
#define NEEDED	(RTF_UP|RTF_HOST|RTF_DYNAMIC|RTF_LLINFO)
	if ((rt == 0) || ((rt->rt_flags & MASK) != NEEDED)) {
		log(LOG_ERR, "in6_rtalloc: in6_rthost failed\n");
		return;
	}
#undef MASK
#undef NEEDED
	if ((rt->rt_ifp == ifp) ||
	    ((oifa = rt->rt_ifa) == 0) ||
	    (!IFND6_IS_LLINK(rt->rt_ifp) &&
	     (rt->rt_ifp->if_ndtype & IFND6_INLL) == 0)) {
		log(LOG_WARNING, "in6_rtalloc: clobber?\n");
		return;
	}
	if (oifa->ifa_ifp != rt->rt_ifp)
		log(LOG_ERR, "in6_rtalloc: ifa/ifp mismatch\n");
	if (oifa->ifa_rtrequest)
		oifa->ifa_rtrequest(RTM_DELETE, rt, (struct sockaddr *)0);
	ifafree(oifa);
	rt->rt_ifa = ifa;
	rt->rt_ifp = ifp;
	atomicAddUint(&ifa->ifa_refcnt, 1);
	if (ifa->ifa_rtrequest)
		ifa->ifa_rtrequest(RTM_ADD, rt, (struct sockaddr *)0);
	rt->rt_flags |= RTF_MODIFIED;
}

/*ARGSUSED*/
int
in6_setifa(inp, ifa)
	struct inpcb *inp;
	struct ifaddr *ifa;
{
#ifdef INTERFACES_ARE_SELECTABLE
	register struct route *ro;
	register struct sockaddr_in6 *dst;
	int s = splnet();

	if (inp->inp_ifa)
		ifafree(inp->inp_ifa);
	inp->inp_ifa = ifa;
	atomicAddUint(&ifa->ifa_refcnt, 1);
	splx(s);

	if ((inp->inp_fatype == IPATYPE_UNBD) ||
	    (inp->inp_socket->so_options & SO_DONTROUTE))
		return (0);

	ro = &inp->inp_route;
	ROUTE_RDLOCK();
	dst = satosin6(&ro->ro_dst);
	if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
			  !SAME_ADDR6(dst->sin6_addr, inp->inp_faddr6))) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = (struct rtentry *)0;
	}
	if (ro->ro_rt == 0) {
		dst->sin6_family = AF_INET6;
		dst->sin6_len = sizeof(struct sockaddr_in6);
		COPY_ADDR6(inp->inp_faddr6, dst->sin6_addr);
		in6_rtalloc(ro, ifa);
	}
	if (ro->ro_rt == 0)
		return (EHOSTUNREACH);
	ro->ro_rt->rt_use++;
	ROUTE_UNLOCK();
#endif
	return (0);
}
#endif /* INET6 */
