/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)if_ether.c	8.1 (Berkeley) 6/10/93
 */

/*
 * IPv6 neighbor discovery protocol.
 */

#ifdef INET6

#include <sys/tcp-param.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/hashing.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in6_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/ip6_icmp.h>
#include <netinet/ip6_opts.h>
#include <netinet/if_ether.h>
#include <netinet/if_ether6.h>
#include <netinet/if_ndp6.h>
#include <netinet/icmp6_var.h>
#include <sys/tcpipstats.h>

#define satosdl(s) ((struct sockaddr_dl *)s)

/*
 * Timer values
 */
u_int	ndpt_keep = 10*60;	/* route default lifetime (10 minutes) */
u_int	ndpt_reachable = 120;	/* reachable time (4 * 30 seconds) */
u_int	ndpt_retrans = 40;	/* retrans timer (4 * 10 seconds) */
u_int	ndpt_probe = 20;	/* delay first probe (4 * 5 seconds) */
u_int	ndpt_down = 10;		/* hold down timer (10 seconds) */
u_int	ndp_umaxtries = 3;	/* max unicast solicit (3) */
u_int	ndp_mmaxtries = 3;	/* max multicast solicit (3) */

#define	rt_expire	rt_rmx.rmx_expire
#define rt_mtu		rt_rmx.rmx_mtu
#define rt_metric	rt_rmx.rmx_hopcount

#ifdef MULTI_HOMED
extern	void llink_resolve __P((struct mbuf *,
			struct sockaddr *, struct rtentry *));
#endif

int	ndp_inuse, ndp_allocated;
extern	int useloopback;	/* use loopback interface for local traffic */
int	ndp6_init_done = 0;

struct	in6_addr allnodes6_group =
#if BYTE_ORDER == BIG_ENDIAN
   {{{ 0xff020000, 0, 0, 1 }}};
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
   {{{ 0x02ff, 0, 0, 0x01000000 }}};
#endif

extern	int ip6forwarding;

#define ROUTER_ADV		0x80000000
#define SOLICITED_ADV		0x40000000
#define OVERRIDE_ADV		0x20000000
#define NO_LLA_ADV		0x00000001
#define MASK_ADV		0xe0000000

/* ARGSUSED */
static int
in6_nd_match(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
	struct in6_ifaddr *ia = (struct in6_ifaddr *)h;
	struct in6_addr *addr = (struct in6_addr *)key;
	struct ifnet *ifp = (struct ifnet *)arg2;

	if (h->flags & HTF_INET)
		if (ia->ia_ifp == ifp &&
		  SAME_ADDR6(*addr, ia->ia_addr.sin6_addr))
			return (1);
	return (0);
}

/*
 * Parallel to llc_rtrequest.
 */
void
ndp6_rtrequest(req, rt, sa)
	int req;
	register struct rtentry *rt;
	struct sockaddr *sa;
{
	register struct sockaddr *gate = rt->rt_gateway;
	register struct llinfo_ndp6 *ln = (struct llinfo_ndp6 *)rt->rt_llinfo;
	register struct sockaddr_dl *sdl;
#ifdef _HAVE_SA_LEN
	struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};
#else
	struct sockaddr_dl null_sdl = {AF_LINK};
#endif
	extern struct ifnet loif;

	ASSERT(ROUTE_ISWRLOCKED());
	if (!ndp6_init_done) {
		ndp6_init_done = 1;
		/*
		 * We generate expiration times from time
		 * so avoid accidently creating permanent routes.
		 */
		if (time == 0) {
			time++;
		}
	}
#ifdef IP6PRINTFS
	if (ip6printfs & D6_NDP1)
		printf("ndp6_rtrequest(%d,%p,%p)\n", req, rt, sa);
#endif
	if (rt->rt_flags & RTF_GATEWAY)
		return;
	switch (req) {

	case RTM_ADD:
		if (rt->rt_mtu == 0)
			rt->rt_mtu = rt->rt_ifp->if_mtu;
		if ((rt->rt_flags & RTF_CLONING) &&
		    (rt->rt_flags & RTF_HOST) &&
		    (rt->rt_ifa->ifa_flags & RTF_HOST)) {
#ifdef IP6PRINTFS
			if (ip6printfs & D6_NDP0)
				printf("ndp6_rtrequest: uncloning\n");
#endif
			rt->rt_flags &= ~RTF_CLONING;
			rt_setgate(rt, rt_key(rt),
					(struct sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
		}
		if (rt->rt_flags & RTF_CLONING) {
			/*
			 * Case 1:
			 *  This route should come from a route to iface.
			 */
#ifdef IP6PRINTFS
			if (ip6printfs & D6_NDP0)
				printf("ndp6_rtrequest: cloning\n");
#endif
			rt_setgate(rt, rt_key(rt),
					(struct sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
			sdl = satosdl(gate);
			sdl->sdl_type = rt->rt_ifp->if_type;
			sdl->sdl_index = rt->rt_ifp->if_index;
			rt->rt_expire = time;
			break;
		}
#ifdef MULTI_HOMED
		if (IFND6_IS_LLINK(rt->rt_ifp)) {
			register struct ifnet *ifp;
			register struct in6_ifaddr *ia = 0;

			sdl = satosdl(gate);
			if (sdl->sdl_family != AF_LINK ||
			    sdl->sdl_len < sizeof(*sdl) ||
			    sdl->sdl_index == 0 ||
			    sdl->sdl_index == rt->rt_ifp->if_index) {
				log(LOG_ERR,
				    "llink_rtrequest: illegal route add\n");
				return;
			}
			for (ifp = ifnet;
			     ifp != 0 && ifp->if_index != sdl->sdl_index;
			     ifp = ifp->if_next)
				continue;
			if (ifp)
				IFP_TO_IA6(ifp, ia);
			if (ifp == 0 ||
			    ia == 0 ||
			    ia->ia_ifa.ifa_rtrequest != ndp6_rtrequest) {
				log(LOG_ERR,
				    "llink_rtrequest: can't find interface\n");
				return;
			}
			rt->rt_ifp = ifp;
			rt->rt_mtu = ifp->if_mtu;
		}
#endif
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
#ifdef IP6PRINTFS
		if (ip6printfs & D6_NDP0)
			 printf("ndp6_rtrequest: resolve\n");
#endif
		if (gate->sa_family != AF_LINK
#if _HAVE_SIN_LEN
		    || gate->sa_len < sizeof(null_sdl)
#endif
		    ) {
			if (req == RTM_RESOLVE)
				log(LOG_DEBUG,
				    "ndp6_rtrequest: bad gateway value\n");
			break;
		}
		sdl = satosdl(gate);
		sdl->sdl_type = rt->rt_ifp->if_type;
		sdl->sdl_index = rt->rt_ifp->if_index;
		if (ln != 0)
			break; /* This happens on a route change */
		/*
		 * Case 2:  This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		R_Malloc(ln, struct llinfo_ndp6 *, sizeof(*ln));
		rt->rt_llinfo = (caddr_t)ln;
		if (ln == 0) {
			log(LOG_DEBUG, "ndp6_rtrequest: kmem_alloc failed\n");
			break;
		}
		atomicAddInt(&ndp_inuse, 1), atomicAddInt(&ndp_allocated, 1);
		Bzero(ln, sizeof(*ln));
		ln->ln_rt = rt;
		if (req == RTM_ADD && sdl->sdl_alen == ETHER_ADDR_LEN)
			ln->ln_state = LLNDP6_REACHABLE;
		rt->rt_flags |= RTF_LLINFO | RTF_DYNAMIC;

		if ((struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
		  in6_nd_match, (caddr_t)&satosin6(rt_key(rt))->sin6_addr, 0,
		  (caddr_t)rt->rt_ifp)) {
		    /*
		     * This test used to be
		     *	if (loif.if_flags & IFF_UP)
		     * It allowed local traffic to be forced
		     * through the hardware by configuring the loopback down.
		     * However, it causes problems during network configuration
		     * for boards that can't receive packets they send.
		     * It is now necessary to clear "useloopback" and remove
		     * the route to force traffic out to the hardware.
		     */
#ifdef IP6PRINTFS
			if (ip6printfs & D6_NDP0)
				printf("ndp6_rtrequest: local hack\n");
#endif
#ifdef MULTI_HOMED
			if (IFND6_IS_LLINK(rt->rt_ifp)) {
				sdl->sdl_alen = ETHER_ADDR_LEN;
				useloopback = 1;
			} else
#endif
			if (rt->rt_ifp->if_ndtype & IFND6_ADDRES)
				Bcopy(GETL2ADDR(rt->rt_ifp),
				      LLADDR(sdl),
				      sdl->sdl_alen = rt->rt_ifp->if_addrlen);
			ln->ln_state = LLNDP6_BUILTIN;
#ifdef MULTI_HOMED
			rt->rt_expire = 0;
#else
			rt->rt_expire = time + ndpt_reachable;
#endif
			if (useloopback) {
				rt->rt_ifp = &loif;
				rt->rt_mtu = loif.if_mtu;
			}
		}
		break;

	case RTM_DELETE:
#ifdef IP6PRINTFS
		if (ip6printfs & D6_NDP0)
			printf("ndp6_rtrequest: delete\n");
#endif
		if (ln == 0)
			break;
		atomicAddInt(&ndp_inuse, -1);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~(RTF_LLINFO | RTF_DYNAMIC);
		if (ln->ln_hold)
			m_freem(ln->ln_hold);
		Free((caddr_t)ln);
		break;

	case RTM_EXPIRE:
#ifdef IP6PRINTFS
		if (ip6printfs & D6_NDP0)
			printf("ndp6_rtrequest: expire\n");
#endif
		if (ln == 0) {
			log(LOG_ERR, "IPv6 route expire without llinfo!\n");
			return;
		}
		sdl = satosdl(gate);
#ifdef IP6PRINTFS
		if (sdl == 0 || sdl->sdl_family != AF_LINK) {
			log(LOG_ERR, "IPv6 route expire with bad gateway\n");
			return;
		}
#endif

		/* Neighbor Unreachable Discovery */
		if (rt->rt_expire + ndpt_keep <= time &&
		    ln->ln_state != LLNDP6_PHASEDOUT)
			if (rt->rt_refcnt > 0) {
				ln->ln_state = LLNDP6_PHASEDOUT;
				ln->ln_asked = 0;
				rt->rt_flags &= ~RTF_REJECT;
				sdl->sdl_alen = 0;
				return;
			} else {
				rtrequest(RTM_DELETE, rt_key(rt), 0,
					  rt_mask(rt), 0, 0);
				return;
			}

		switch (ln->ln_state) {
		    case LLNDP6_PHASEDOUT:
			if (rt->rt_refcnt > 0)
				return;
			rtrequest(RTM_DELETE, rt_key(rt), 0,
				  rt_mask(rt), 0, 0);
			return;

		    case LLNDP6_INCOMPLETE:
			if (rt->rt_flags & RTF_REJECT) {
				/* holddown finished */
				rt->rt_flags &= ~RTF_REJECT;
				ln->ln_state = LLNDP6_PHASEDOUT;
				return;
			}
			if (ln->ln_asked < ndp_mmaxtries) {
				/* resend a NS */
#ifdef MULTI_HOMED
				if (IFND6_IS_LLINK(rt->rt_ifp)) {
					llink_resolve(NULL, rt_key(rt), rt);
					return;
				}
#endif
				ln->ln_asked++;
				rt->rt_expire = time + ndpt_retrans;
				ROUTE_UNLOCK();
				ndsol6_output(rt->rt_ifp,
				      ln->ln_hold, NULL,
				      &satosin6(rt_key(rt))->sin6_addr);
				ROUTE_WRLOCK();
				return;
			}
			/* enter hold down from incomplete */
		    down:
			if (ln->ln_flags) {
				sdl->sdl_alen = 0;
				ndp6_rtlost(rt, 1);
			}
			ln->ln_state = LLNDP6_INCOMPLETE;
			rt->rt_expire = time + ndpt_down;
			rt->rt_flags |= RTF_REJECT;
			ln->ln_asked = 0;
			sdl->sdl_alen = 0;
			if (ln->ln_hold &&
			    (ln->ln_hold->m_len >= sizeof(struct ipv6))) {
				NTOHS(mtod(ln->ln_hold, struct ipv6 *)->ip6_len);
				ROUTE_UNLOCK();
				icmp6_error(ln->ln_hold,
					    ICMP6_UNREACH,
					    ICMP6_UNREACH_ADDRESS,
					    NULL, rt->rt_ifp);
				ROUTE_WRLOCK();
			} else if (ln->ln_hold)
				m_freem(ln->ln_hold);
			ln->ln_hold = 0;
			return;

		    case LLNDP6_PROBING:
			if (ln->ln_asked < ndp_umaxtries) {
				ln->ln_asked++;
				rt->rt_expire = time + ndpt_retrans;
				ROUTE_UNLOCK();
				ndsol6_output(rt->rt_ifp,
				      ln->ln_hold,
				      &satosin6(rt_key(rt))->sin6_addr,
				      &satosin6(rt_key(rt))->sin6_addr);
				ROUTE_WRLOCK();
				return;
			}
			/* enter hold down from probe */
			goto down;
		}
	}
}

/*
 * Resolve an IPv6 address into an ethernet address.  If success,
 * desten is filled in.  If there is no entry in the ndp table,
 * set one up and send a GS for the IPv6 address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 1 indicates
 * that desten has been filled in and the packet should be sent
 * normally; a 0 return indicates that the packet has been
 * taken over here, either now or for later transmission.
 */
int
ndp6_resolve(ifp, rt, m, dst, desten)
	register struct ifnet *ifp;
	register struct rtentry *rt;
	struct mbuf *m;
	register struct sockaddr *dst;
	register u_char *desten;
{
	register struct llinfo_ndp6 *ln;
	struct sockaddr_dl *sdl = satosdl(0);
	struct in6_ifaddr *ia;
	static int baddriver = 0;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_NDP1)
		printf("ndp6_resolve(%p,%p,%p,%s,...)\n", ifp, rt, m,
		       ip6_sprintf(&(satosin6(dst))->sin6_addr));
#endif
	if (m->m_flags & M_MCAST) {	/* multicast */
		ETHER_MAP_IP6_MULTICAST(&satosin6(dst)->sin6_addr, desten);
		return (1);
	}
	IFP_TO_IA6(ifp, ia);
	if (ia == NULL) {
		printf("ndp6_resolve on an interface without IPv6\n");
		m_freem(m);
		return (0);
	}
	if (ia->ia_flags & IFA_BOOTING) {
		log(LOG_DEBUG, "ndp6_resolve on booting interface\n");
		m_freem(m);
		return (0);
	}
	ROUTE_RDLOCK();
	if (rt)
		ln = (struct llinfo_ndp6 *)rt->rt_llinfo;
	else {
		if ((ln = ndplookup(&satosin6(dst)->sin6_addr, 1)) != 0)
			rt = ln->ln_rt;
	}
	if (ln == 0 || rt == 0) {
		log(LOG_DEBUG,
		    "ndp6_resolve: can't allocate llinfo for %s\n",
		    ip6_sprintf(&(satosin6(dst))->sin6_addr));
		if (((ia->ia_flags & RTF_CLONING) == 0) &&
		    (ia->ia_ifp->if_type == IFT_ETHER) &&
		    ((baddriver++ & 127) == 0))
			log(LOG_ERR,
			    "the driver for %s don't call ndp6_ifinit\n",
			    ia->ia_ifp->if_name);
		ROUTE_UNLOCK();
		m_freem(m);
		return (0);
	}
	sdl = satosdl(rt->rt_gateway);
#ifdef IP6PRINTFS
	if (sdl == 0 || sdl->sdl_family != AF_LINK) {
		ROUTE_UNLOCK();
		log(LOG_ERR, "ndp6_resolve: bad gateway\n");
		m_freem(m);
		return (0);
	}
#endif
	/*
	 * If the state is not PROBING nor REACHABLE try to resolve.
	 */
	if (ln->ln_state >= LLNDP6_PROBING) {
		bcopy(LLADDR(sdl), desten, ETHER_ADDR_LEN);

		/* test if PROBING state must be entered */
		if (ln->ln_state == LLNDP6_REACHABLE &&
		    rt->rt_expire &&
		    rt->rt_expire <= time &&
		    (m->m_flags & M_NOPROBE) == 0) {
			ln->ln_state = LLNDP6_PROBING;
			rt->rt_expire = time + ndpt_probe;
		}
#ifdef IP6PRINTFS
		if (ip6printfs & D6_NDP1)
			printf("ndp6_resolve -> %s (state = %d)\n",
			       ether_sprintf((u_char *)LLADDR(sdl)),
			       ln->ln_state);
#endif
		ROUTE_UNLOCK();
		return (1);
	}
	/*
	 * There is an ndp entry, but no ethernet address
	 * response yet.  Replace the held mbuf with this
	 * latest one.
	 */
	ln->ln_state = LLNDP6_INCOMPLETE;
	if (ln->ln_hold)
		m_freem(ln->ln_hold);
	ln->ln_hold = m;
	sdl->sdl_alen = 0;
#ifdef	IP6PRINTFS
	if (rt->rt_expire == 0) {
		/* This should never happen. (Should it? -gwr) */
		printf("ndp6_resolve: unresolved and rt_expire == 0\n");
		/* Set expiration time to now (expired). */
		rt->rt_expire = time;
	}
#endif
	/*
	 * Re-send the Neighbor Solicitation when appropriate.
	 */
	if (rt->rt_expire <= time && ln->ln_asked < ndp_mmaxtries) {
		ln->ln_asked++;
		rt->rt_expire = time + ndpt_retrans;
		ROUTE_UNLOCK();
		ndsol6_output(ifp, m, NULL, &satosin6(dst)->sin6_addr);
	} else
		ROUTE_UNLOCK();
	return (0);
}

/*
 * Send a Neighbor Solicitation packet, asking who has addr on interface.
 */
void
ndsol6_output(ifp, hold, dst, target)
	register struct ifnet *ifp;
	register struct mbuf *hold;
	register struct in6_addr *dst, *target;
{
	register struct mbuf *m;
	register struct ipv6 *ip;
	register struct icmpv6 *icp;
	register struct ndx6_lladdr *lp;
	struct ip_moptions *imo = NULL;
	int icmplen;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHEROUT)
		printf("ndsol6_output(%p,%s)\n", ifp, ip6_sprintf(target));
	if ((ifp->if_ndtype & IFND6_LLSET) == 0) {
		printf("ndsol6_output on interface without local-link address\n");
		return;
	}
#endif

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;
	m->m_flags |= M_NOPROBE;
	if ((dst == NULL) || IS_MULTIADDR6(*dst)) {
		m->m_flags |= M_MCAST;
		imo = (struct ip_moptions*)kmem_alloc(sizeof(*imo), KM_NOSLEEP);
		if (imo == NULL) {
			m_free(m);
			return;
		}
		imo->imo_multicast_ifp = ifp;
		imo->imo_multicast_ttl = ICMP6_ND_HOPS;
		imo->imo_multicast_loop = 0;
	}
	icmplen = ICMP6_NSLEN;
	if (imo && (ifp->if_ndtype & IFND6_ADDRES))
		icmplen += sizeof(*lp);
	m->m_len = sizeof(*ip) + icmplen;
	if (m->m_len > MLEN)
		panic("ndsol6_output too big");
	MH_ALIGN(m, m->m_len);
	bzero(mtod(m, caddr_t), m->m_len);

	/* fill Neighbor Solicitation packet */
	ip = mtod(m, struct ipv6 *);
	icp = (struct icmpv6 *)(ip + 1);
	if (imo && (ifp->if_ndtype & IFND6_ADDRES)) {
		lp = (struct ndx6_lladdr *)((caddr_t)icp + ICMP6_NSLEN);
		lp->lla_ext = NDX6_LLADDR_SRC;
		lp->lla_len = sizeof(*lp) >> 3;
		bcopy(GETL2ADDR(ifp), lp->lla_addr, ifp->if_addrlen);
	}
	COPY_ADDR6(*target, icp->icmp6_tgt);
	icp->icmp6_type = ICMP6_SOLICITATION_ND;
	ip->ip6_head = IPV6_VERSION | ICMP6_ND_PRIORITY;
	ip->ip6_len = icmplen;
	ip->ip6_nh = IPPROTO_ICMPV6;
	ip->ip6_hlim = ICMP6_ND_HOPS;
	if (hold) {
		struct ipv6 *hip;
		struct ifnet *ifp2;

		if (hold->m_len >= (__psint_t)&(((struct ipv6 *)0)->ip6_dst))
			hip = mtod(hold, struct ipv6 *);
		else
			goto nosource;
		IN6ADDR_TO_IFP(hip->ip6_src, ifp2);
		if ((ifp2 == NULL) || (ifp2 != ifp))
			goto nosource;
		COPY_ADDR6(hip->ip6_src, ip->ip6_src);
	} else {
	    nosource:
		if (ifp->if_ndtype & IFND6_LLSET) {
			COPY_ADDR6(*GETLLADDR(ifp), ip->ip6_src);
		} else {
			struct in6_ifaddr *ia;

			IFP_TO_IA6(ifp, ia);
			if (ia == NULL) {
				(void) m_free(m);
				if (imo != NULL)
					kmem_free(imo, sizeof(*imo));
				log(LOG_ERR,
				    "ndsol6_output: interface (%p) without IPv6?!\n",
				    ifp);
				return;
			}
			COPY_ADDR6(ia->ia_addr.sin6_addr, ip->ip6_src);
		}
	}
	if (dst) {
		COPY_ADDR6(*dst, ip->ip6_dst);
	} else {
		ip->ip6_dst.s6_addr32[0] = htonl(0xff020000);
		ip->ip6_dst.s6_addr32[1] = 0;
		ip->ip6_dst.s6_addr32[2] = htonl(1);
		ip->ip6_dst.s6_addr32[3] = 0xff000000 | target->s6_addr32[3];
	}

	/* send */
	icmp6_send(m, NULL, imo);
	if (imo != NULL)
		kmem_free(imo, sizeof(*imo));
	ICMP6STAT(icp6s_snd_ndsol);
}

/*
 * Send a Neighbor Advertisement packet.
 */
void
ndadv6_output(ifp, dst, target, flags)
	register struct ifnet *ifp;
	register struct in6_addr *dst, *target;
	int flags;
{
	register struct mbuf *m;
	register struct ipv6 *ip;
	register struct icmpv6 *icp;
	register struct ndx6_lladdr *lp;
	struct ip_moptions *imo = NULL;
	int icmplen, needlla = sizeof(*lp);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHEROUT)
		printf("ndadv6_output(%p,%s,%s,%x)\n", ifp,
		       ip6_sprintf(dst), ip6_sprintf(target), flags);
#endif
	if ((flags & NO_LLA_ADV) || ((ifp->if_ndtype & IFND6_ADDRES) == 0))
		needlla = 0;
#ifdef IP6PRINTFS
	else if ((ifp->if_ndtype & IFND6_LLSET) == 0) {
		printf("ndadv6_output on interface without local-link address\n");
		return;
	}
#endif
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;
	m->m_flags |= M_NOPROBE;
	icmplen = ICMP6_NALEN + needlla;
	m->m_len = sizeof(*ip) + icmplen;
	if (m->m_len > MLEN)
		panic("ndadv6_output too big");
	MH_ALIGN(m, m->m_len);
	bzero(mtod(m, caddr_t), m->m_len);
	if (IS_MULTIADDR6(*dst)) {
		m->m_flags |= M_MCAST;
		imo = (struct ip_moptions*)kmem_alloc(sizeof(*imo), KM_NOSLEEP);
		if (imo == NULL) {
			m_free(m);
			return;
		}
		imo->imo_multicast_ifp = ifp;
		imo->imo_multicast_ttl = ICMP6_ND_HOPS;
		imo->imo_multicast_loop = 0;
	}

	/* fill Neighbor Advertisement packet */
	ip = mtod(m, struct ipv6 *);
	icp = (struct icmpv6 *)(ip + 1);
	if (needlla) {
		lp = (struct ndx6_lladdr *)((caddr_t)icp + ICMP6_NALEN);
		lp->lla_ext = NDX6_LLADDR_TGT;
		lp->lla_len = sizeof(*lp) >> 3;
		bcopy(GETL2ADDR(ifp), lp->lla_addr, ifp->if_addrlen);
	}
	COPY_ADDR6(*target, icp->icmp6_tgt);
	icp->icmp6_type = ICMP6_ADVERTISEMENT_ND;
	icp->icmp6_code = 0;
	icp->icmp6_flags = htonl(flags & MASK_ADV);
	ip->ip6_head = IPV6_VERSION | ICMP6_ND_PRIORITY;
	ip->ip6_len = icmplen;
	ip->ip6_nh = IPPROTO_ICMPV6;
	ip->ip6_hlim = ICMP6_ND_HOPS;
	if (needlla || (ifp->if_ndtype & IFND6_LLSET)) {
		COPY_ADDR6(*GETLLADDR(ifp), ip->ip6_src);
	} else {
		struct in6_ifaddr *ia;

		IFP_TO_IA6(ifp, ia);
		if (ia == NULL) {
			(void) m_free(m);
			if (imo != NULL)
				kmem_free(imo, sizeof(*imo));
			log(LOG_ERR,
			    "ndadv6_output: interface (%p) without IPv6?!\n",
			    ifp);
			return;
		}
		COPY_ADDR6(ia->ia_addr.sin6_addr, ip->ip6_src);
	}
	COPY_ADDR6(*dst, ip->ip6_dst);

	/* send */
	icmp6_send(m, NULL, imo);
	if (imo != NULL)
		kmem_free(imo, sizeof(*imo));
	ICMP6STAT(icp6s_snd_ndadv);
}

/*
 * Send a Redirect packet.
 */
void
redirect6_output(m0, rt)
	register struct mbuf *m0;
	register struct rtentry *rt;
{
	register struct mbuf *m;
	register struct ipv6 *ip0, *ip;
	register struct icmpv6 *icp;
	register struct ndx6_lladdr *lp;
	struct ndx6_any *xp;
	int len0;
	struct llinfo_ndp6 *ln;
	struct sockaddr_dl *sdl;
	struct in6_addr target;

	ip0 = mtod(m0, struct ipv6 *);
	if (rt->rt_flags & RTF_GATEWAY) {
		COPY_ADDR6(satosin6(rt->rt_gateway)->sin6_addr, target);
		if ((rt = rt->rt_gwroute) == NULL)
			goto bad;
	} else
		COPY_ADDR6(ip0->ip6_dst, target);
	ln = (struct llinfo_ndp6 *)rt->rt_llinfo;
	sdl = satosdl(rt->rt_gateway);
	if (ln == NULL || ln->ln_state < LLNDP6_PROBING ||
	    sdl == NULL || sdl->sdl_family != AF_LINK)
		goto bad;
	if ((rt->rt_ifp == NULL) ||
	    ((rt->rt_ifp->if_ndtype & IFND6_LLSET) == 0))
		goto bad;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHEROUT)
		printf("redirect6_output(%p,%p,%s,%s,%s)\n", m0, rt,
		       ip6_sprintf(&ip0->ip6_src),
		       ip6_sprintf(&ip0->ip6_dst),
		       ether_sprintf((u_char *)LLADDR(sdl)));
#endif
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		m_freem(m0);
		return;
	}
	m->m_flags |= M_NOPROBE;
	m->m_len = sizeof(*ip) + ICMP6_RDLEN + sizeof(*lp) + sizeof(*xp);
	if (m->m_len > MLEN)
		panic("redirect6_output too big");
	len0 = ip0->ip6_len + sizeof(*ip0);
	if (len0 + m->m_len > IP6_MMTU) {
		m_adj(m0, IP6_MMTU - (len0 + m->m_len));
		len0 = IP6_MMTU - m->m_len;
	}
	m->m_next = m0;
	MH_ALIGN(m, m->m_len);
	bzero(mtod(m, caddr_t), m->m_len);

	/* fill Redirect packet */
	ip = mtod(m, struct ipv6 *);
	icp = (struct icmpv6 *)(ip + 1);
	lp = (struct ndx6_lladdr *)((caddr_t)icp + ICMP6_RDLEN);
	xp = (struct ndx6_any *)((caddr_t)lp + sizeof(*lp));
	xp->x6any_ext = NDX6_RDRT_HDR;
	xp->x6any_len = (len0 >> 3) + 1;
	lp->lla_ext = NDX6_LLADDR_TGT;
	lp->lla_len = sizeof(*lp) >> 3;
	bcopy(LLADDR(sdl), (caddr_t)lp->lla_addr, ETHER_ADDR_LEN);
	COPY_ADDR6(target, icp->icmp6_tgt);
	COPY_ADDR6(ip0->ip6_dst, icp->icmp6_rdst);
	icp->icmp6_type = ICMP6_REDIRECT;
	ip->ip6_head = IPV6_VERSION | ICMP6_ND_PRIORITY;
	ip->ip6_len = (m->m_len + len0) - sizeof(*ip);
	ip->ip6_nh = IPPROTO_ICMPV6;
	ip->ip6_hlim = ICMP6_ND_HOPS;
	COPY_ADDR6(*GETLLADDR(rt->rt_ifp), ip->ip6_src);
	COPY_ADDR6(ip0->ip6_src, ip->ip6_dst);

	/* send */
	icmp6_send(m, NULL, NULL);
	ICMP6STAT(icp6s_snd_redirect);
	return;

    bad:
	m_freem(m0);
}

/*
 * Neighbor Discovery for IPv6 protocols on 10 Mbits/s Ethernet.
 * Algorithm is that given in RFC xxxx.
 * In addition, a sanity check is performed on the sender
 * protocol address, to catch impersonators.
 */

#define NEXT_EXTENSION(name) \
	if (icmplen == 0) \
		goto done; \
retry: \
/*#ifdef IP6PRINTFS*/ \
	if (ip6printfs & D6_ETHERIN) \
		printf("NEXT_EXTENSION extra=%d needed=%d icmplen=%d\n", \
		       extra, needed, icmplen); \
/*#endif*/ \
	if (icmplen < needed) { \
		log(LOG_INFO, "%s: truncated\n", name); \
		if (mcopy) \
			m_freem(m0); \
		return (0); \
	} \
	\
	if (m0->m_len >= extra + needed) { \
		xp = (struct ndx6_any *)(mtod(m0, caddr_t) + extra); \
	} else { \
		if (mcopy == 0) { \
/*#ifdef IP6PRINTFS*/ \
			if (ip6printfs & D6_ETHERIN) \
				printf("NEXT_EXTENSION m_copy\n"); \
/*#endif*/ \
			if ((mcopy = m_copy(m0, extra, M_COPYALL)) == 0) { \
				log(LOG_INFO, "%s: m_copy failed\n", name); \
				return (0); \
			} \
			m0 = mcopy; \
		} else if (m0->m_len >= extra) { \
			m0->m_off += extra; \
			m0->m_len -= extra; \
		} else \
			m_adj(m0, extra); \
		if (m0->m_len < needed && (m0 = m_pullup(m0, needed)) == 0) { \
			log(LOG_INFO, "%s: m_pullup failed\n", name); \
			return (0); \
		} \
		extra = 0; \
		xp = mtod(m0, struct ndx6_any *); \
	}

#define GETEXT(type)	\
	if (needed < sizeof(type)) { \
		needed = sizeof(type); \
		goto retry; \
	}

#define LLA(p)	((struct ndx6_lladdr *)p)
#define MTU(p)	((struct ndx6_mtu *)p)
#define PREF(p)	((struct ndx6_pref *)p)

#define GETEXTLLADDR(name)	\
	if (haslladdr) { \
		log(LOG_INFO, "%s: duplicate LLaddr\n", name); \
		goto next; \
	} \
	haslladdr = 1; \
	if ((LLA(xp)->lla_len << 3) < sizeof(struct ndx6_lladdr)) { \
		log(LOG_INFO, "%s: bad LLaddr option\n", name); \
		if (mcopy) \
			m_freem(m0); \
		return (0); \
	} \
	bcopy((caddr_t)LLA(xp)->lla_addr, lladdr, ETHER_ADDR_LEN); \
	goto next;

#define GETEXTMTU(name)	\
	if (mtu) { \
		log(LOG_INFO, "%s: duplicate MTU\n", name); \
		goto next; \
	} \
	if ((MTU(xp)->mtu_len << 3) < sizeof(struct ndx6_mtu)) { \
		log(LOG_INFO, "%s: bad MTU option\n", name); \
		if (mcopy) \
			m_freem(m0); \
		return (0); \
	} \
	mtu = ntohl(MTU(xp)->mtu_mtu); \
	if ((mtu > ifp->if_mtu) || (mtu < IP6_MMTU)) { \
		log(LOG_INFO, "%s: ignore bad MTU %d\n", name, mtu); \
		mtu = 0; \
	} \
	goto next;

#define GETNEXTEXT(name)	\
	if (xp->x6any_len == 0) { \
		log(LOG_INFO, "%s: 0 length extension!\n", name); \
		if (mcopy) \
			m_freem(m0); \
		return (0); \
	} \
	extra += xp->x6any_len << 3; \
	icmplen -= xp->x6any_len << 3; \
	if (icmplen != 0) \
		goto retry;

#define LLSRCOVERWRITE(name)	\
	if ((ln->ln_state >= LLNDP6_PROBING) && \
	    bcmp(lladdr, LLADDR(sdl), ETHER_ADDR_LEN)) { \
		log(LOG_INFO, \
		    "%s source overwrite for %s/%s\n", \
		    name, \
		    ip6_sprintf(&srcaddr), ether_sprintf(lladdr)); \
		ln->ln_state = LLNDP6_INCOMPLETE; \
	} \
	bcopy(lladdr, LLADDR(sdl), sdl->sdl_alen = ETHER_ADDR_LEN);


#define UPDATEMHIFA(name)	\
	if (rt->rt_ifp != ifp) { \
		struct in6_ifaddr *ia; \
		\
		if (!IFND6_IS_LLINK(rt->rt_ifp)) \
			log(LOG_NOTICE, \
			    "%s: router for %s ifp#%d->%d\n", \
			    name, \
			    ip6_sprintf(&srcaddr), \
			    rt->rt_ifp->if_index, \
			    ifp->if_index); \
		rt->rt_ifp = ifp; \
		rt->rt_mtu = ifp->if_mtu; \
		sdl->sdl_type = ifp->if_type; \
		sdl->sdl_index = ifp->if_index; \
		ia = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr, \
		  in6_nd_match, (caddr_t)&dstaddr, 0, (caddr_t)ifp); \
		if (ia == (struct in6_ifaddr *)0) \
			IFP_TO_IA6(rt->rt_ifp, ia); \
		if (ia) { \
			ifafree(rt->rt_ifa); \
			rt->rt_ifa = &ia->ia_ifa; \
			atomicAddUint(&rt->rt_ifa->ifa_refcnt, 1); \
		} else \
			log(LOG_ERR, "%s without ifa?\n", name); \
	}

#define ENTERSTALE()	\
	ln->ln_state = LLNDP6_REACHABLE; \
	rt->rt_expire = time; \
	rt->rt_flags &= ~RTF_REJECT; \
	ln->ln_asked = 0; \
	if (ln->ln_hold) { \
		IFNET_LOCK(ifp); \
		(*ifp->if_output)(ifp, ln->ln_hold, rt_key(rt), rt); \
		IFNET_UNLOCK(ifp); \
		ln->ln_hold = 0; \
	}

/*
 * Receive a Neighbor Solicitation packet.
 */
int
ndsol6_input(struct mbuf *m0, struct ifnet *ifp)
{
	struct ipv6 *ip = mtod(m0, struct ipv6 *);
	struct icmpv6 *icp = (struct icmpv6 *)(ip + 1);
	register struct ndx6_any *xp;
	int icmplen = ip->ip6_len - ICMP6_NSLEN;
	register struct llinfo_ndp6 *ln = 0;
	register struct rtentry *rt;
	struct mbuf *mcopy = 0;
	struct in6_ifaddr *sia = 0, *tia = 0;
	struct sockaddr_dl *sdl;
	struct in6_addr srcaddr, dstaddr, tgtaddr;
	u_char lladdr[ETHER_ADDR_LEN];
	int extra = sizeof(*ip) + ICMP6_NSLEN;
	int needed = sizeof(struct ndx6_lladdr);
	int addrconfsol = 0, haslladdr = 0;

	COPY_ADDR6(ip->ip6_src, srcaddr);
	COPY_ADDR6(ip->ip6_dst, dstaddr);
	COPY_ADDR6(icp->icmp6_tgt, tgtaddr);

#ifdef IP6PRINTFS
	bzero(lladdr, ETHER_ADDR_LEN);
	if (ip6printfs & D6_ETHERIN)
		printf("ndsol6_input from %s\n", ip6_sprintf(&srcaddr));
	if (IS_MULTIADDR6(dstaddr) &&
	    MADDR6_SCOPE(dstaddr) != MADDR6_SCP_LINK) {
		log(LOG_INFO,
		    "ndsol6_input to illegal mcast %s\n",
		    ip6_sprintf(&dstaddr));

	}
#endif
	if (icp->icmp6_code != 0) {
		log(LOG_INFO, "ndsol6_input code(%d) != 0\n", icp->icmp6_code);
		return (0);
	}
	if (IS_MULTIADDR6(tgtaddr)) {
		log(LOG_INFO, "ndsol6_input for mcast %s!\n",
		    ip6_sprintf(&tgtaddr));
		return (0);
	}
	if (IS_ANYADDR6(srcaddr))
		addrconfsol = 1;

NEXT_EXTENSION("ndsol6_input")

	switch (xp->x6any_ext) {

	case NDX6_LLADDR_SRC:
		GETEXTLLADDR("ndsol6_input");

	default:
	next:
		GETNEXTEXT("ndsol6_input");
	}
	if (mcopy)
		m_freem(m0);
    done:
	if ((haslladdr == 0) &&
	    (addrconfsol == 0) &&
	    IS_MULTIADDR6(dstaddr)) {
		log(LOG_INFO, "ndsol6_input: no needed LLaddr?\n");
		return (0);
	}

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHERIN) {
		printf("ndsol6_input target=%s", ip6_sprintf(&tgtaddr));
		if (haslladdr)
			printf(" lladdr=%s\n", ether_sprintf(lladdr));
		else
			printf("\n");
	}
#endif
	/* see if I am the source */
	sia = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
	  in6_nd_match, (caddr_t)&srcaddr, 0, (caddr_t)ifp);
	/* see if I am the target */
	tia = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
	  in6_nd_match, (caddr_t)&tgtaddr, 0, (caddr_t)ifp);

	if ((sia == 0) && (tia == 0))
		return (1);
	if (sia && !addrconfsol) {
		if (haslladdr == 0)
			return (1);
		if (bcmp(lladdr, GETL2ADDR(ifp), ifp->if_addrlen) == 0)
			return (1);	/* it should be from me, ignore it. */
		log(LOG_WARNING,
		    "duplicate IPv6 address %s with ethernet address %s\n",
		    ip6_sprintf(&srcaddr), ether_sprintf(lladdr));
		return (1);
	}
	if (addrconfsol) {
		if (tia && (tia->ia_flags & IFA_BOOTING))
			return (1);
		/* should not happen ? */
		if (ip6forwarding && in6_isanycast(&tgtaddr))
			return (1);
		log(LOG_WARNING, 
		    "duplicate IPv6 address %s!!\n",
		    ip6_sprintf(&tgtaddr));
		ndadv6_output(ifp, &allnodes6_group, &tgtaddr,
			      ip6forwarding ? ROUTER_ADV : 0);
		return (1);
	}
	if (haslladdr == 0)
		goto adv;
	/*
	 * add an entry for the source in ndp table.
	 */
	ROUTE_RDLOCK();
	ln = ndplookup(&srcaddr, 1);
	if (ln && (rt = ln->ln_rt) && (sdl = satosdl(rt->rt_gateway))) {
		LLSRCOVERWRITE("ndsol6_input");
		if (ln->ln_state >= LLNDP6_PROBING) {
			ROUTE_UNLOCK();
			goto adv;
		}
#ifdef MULTI_HOMED
		if (rt->rt_ifp != ifp) {
			if (!IFND6_IS_LLINK(rt->rt_ifp))
				log(LOG_NOTICE,
				    "ndsol6_input: router for %s ifp#%d->%d\n",
				    ip6_sprintf(&srcaddr),
				    rt->rt_ifp->if_index,
				    ifp->if_index);
			rt->rt_ifp = ifp;
			rt->rt_mtu = ifp->if_mtu;
			sdl->sdl_type = ifp->if_type;
			sdl->sdl_index = ifp->if_index;
			if (tia) {
				ifafree(rt->rt_ifa);
				rt->rt_ifa = &tia->ia_ifa;
				atomicAddUint(&rt->rt_ifa->ifa_refcnt, 1);
			} else
				log(LOG_ERR, "ndsol6_input without ifa?\n");
		}
#endif
		ENTERSTALE();
	}
	ROUTE_UNLOCK();

    adv:
	if (tia && (tia->ia_flags & IFA_BOOTING))
		return (1);
	/* anycast advertisements are done by an user process after a delay */
	if (ip6forwarding && in6_isanycast(&tgtaddr))
		return (1);
	ndadv6_output(ifp, &srcaddr, &tgtaddr,
		      (ip6forwarding ? ROUTER_ADV : 0) |
		      SOLICITED_ADV | OVERRIDE_ADV);
	return (1);
}

/*
 * Receive a Neighbor Advertisement packet.
 */
int
ndadv6_input(struct mbuf *m0, struct ifnet *ifp)
{
	struct ipv6 *ip = mtod(m0, struct ipv6 *);
	struct icmpv6 *icp = (struct icmpv6 *)(ip + 1);
	register struct ndx6_any *xp;
	int icmplen = ip->ip6_len - ICMP6_NALEN;
	register struct llinfo_ndp6 *ln = 0;
	register struct rtentry *rt;
	struct mbuf *mcopy = 0;
	struct in6_ifaddr *ia, *maybe_ia = 0;
	struct sockaddr_dl *sdl;
	struct in6_addr srcaddr, dstaddr, tgtaddr;
	u_char lladdr[ETHER_ADDR_LEN];
	int extra = sizeof(*ip) + ICMP6_NALEN;
	int needed = sizeof(struct ndx6_lladdr);
	int is_router, is_solicited, is_secondary, haslladdr = 0;

	is_router = (ntohl(icp->icmp6_flags) & ROUTER_ADV) != 0;
	is_solicited = (ntohl(icp->icmp6_flags) & SOLICITED_ADV) != 0;
	is_secondary = (ntohl(icp->icmp6_flags) & OVERRIDE_ADV) == 0;
	COPY_ADDR6(ip->ip6_src, srcaddr);
	COPY_ADDR6(ip->ip6_dst, dstaddr);
	COPY_ADDR6(icp->icmp6_tgt, tgtaddr);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHERIN)
		printf("ndadv6_input %s%s%s from %s to %s for %s\n",
		       is_router ? "R" : "",
		       is_solicited ? "S" : "",
		       is_secondary ? "" : "O",
		       ip6_sprintf(&srcaddr),
		       ip6_sprintf(&dstaddr),
		       ip6_sprintf(&tgtaddr));
#endif
	if (icp->icmp6_code != 0) {
		log(LOG_INFO, "ndadv6_input code(%d) != 0\n", icp->icmp6_code);
		return (0);
	}
	if (IS_MULTIADDR6(dstaddr)) {
		if (MADDR6_SCOPE(dstaddr) != MADDR6_SCP_LINK) {
			log(LOG_INFO, "ndadv6_input to illegal mcast\n");
		}
		if (is_solicited) {
			log(LOG_INFO, "ndadv6_input mcast solicited\n");
			return (0);
		}
	}
	if (IS_MULTIADDR6(tgtaddr)) {
		log(LOG_INFO, "ndadv6_input for mcast\n");
		return (0);
	}
NEXT_EXTENSION("ndadv6_input")

	switch (xp->x6any_ext) {

	case NDX6_LLADDR_TGT:
		GETEXTLLADDR("ndadv6_input");

	default:
	next:
		GETNEXTEXT("ndadv6_input");
	}
	if (mcopy)
		m_freem(m0);
#ifdef IP6PRINTFS
	if (haslladdr && (ip6printfs & D6_ETHERIN))
		printf("ndadv6_input lladdr=%s\n", ether_sprintf(lladdr));
#endif
    done:

	ia = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
	  in6_nd_match, (caddr_t)&tgtaddr, 0, (caddr_t)ifp);
	maybe_ia = (struct in6_ifaddr *)ifp->in6_ifaddr;
	if (maybe_ia == 0)
		return (0);
	if (ia && haslladdr) {
		if (!bcmp(lladdr, GETL2ADDR(ifp), ifp->if_addrlen))
			return (1);	/* it should be from me, ignore it. */
		log(LOG_ERR,
		    "duplicate IPv6 address %s with ethernet address %s\n",
		    ip6_sprintf(&srcaddr), ether_sprintf(lladdr));
		return (1);
	}
	ROUTE_RDLOCK();
	ln = ndplookup(&tgtaddr, 0);
	if ((ln == NULL) ||
	    ((rt = ln->ln_rt) == NULL) ||
	    ((sdl = satosdl(rt->rt_gateway)) == NULL)) {
		ROUTE_UNLOCK();
		return (1);
	}

	/* got an entry */
	if ((ln->ln_state >= LLNDP6_PROBING) && haslladdr &&
	    bcmp(lladdr, LLADDR(sdl), ETHER_ADDR_LEN)) {
		log(LOG_INFO,
		    "ndadv6_input %s overwrite for %s/%s by %s\n",
		    is_secondary ? "could" : "will",
		    ip6_sprintf(&tgtaddr), ether_sprintf(lladdr),
		    ip6_sprintf(&srcaddr));
	}
	if (haslladdr && (!is_secondary || (ln->ln_state < LLNDP6_PROBING))) {
		bcopy(lladdr, LLADDR(sdl), sdl->sdl_alen = ETHER_ADDR_LEN);
#ifdef MULTI_HOMED
		if ((ln->ln_state < LLNDP6_BUILTIN) &&
		    (rt->rt_ifp != ifp)) {
			if (!IFND6_IS_LLINK(rt->rt_ifp))
				log(LOG_NOTICE,
				    "ndadv6_input: router for %s ifp#%d->%d\n",
				    ip6_sprintf(&tgtaddr),
				    rt->rt_ifp->if_index,
				    ifp->if_index);
			rt->rt_ifp = ifp;
			rt->rt_mtu = ifp->if_mtu;
			sdl->sdl_type = ifp->if_type;
			sdl->sdl_index = ifp->if_index;
			ifafree(rt->rt_ifa);
			rt->rt_ifa = &maybe_ia->ia_ifa;
			atomicAddUint(&rt->rt_ifa->ifa_refcnt, 1);
		}
#endif
	}
	ROUTE_UNLOCK();
	if (ln->ln_flags && is_router == 0)
		ndp6_rtlost(rt, 1);
	ln->ln_flags = is_router;
	if ((haslladdr == 0) && (ln->ln_state < LLNDP6_PROBING)) {
#ifdef IP6PRINTFS
		if (ip6printfs & D6_ETHERIN)
			printf("ndadv6_input not useful\n");
#endif
		return (1);
	}
	ln->ln_state = LLNDP6_REACHABLE;
	if (rt->rt_expire) {
		rt->rt_expire = time;
		if (is_solicited)
			rt->rt_expire += ndpt_reachable;
	}
	rt->rt_flags &= ~RTF_REJECT;
	ln->ln_asked = 0;
	if (ln->ln_hold) {
#ifdef IP6PRINTFS
		if (ip6printfs & D6_ETHERIN)
			printf("ndadv6_input send held\n");
#endif
		IFNET_LOCK(ifp);
		(*ifp->if_output)(ifp, ln->ln_hold, rt_key(rt), rt);
		IFNET_UNLOCK(ifp);
		ln->ln_hold = 0;
	}
#ifdef IP6PRINTFS
	if (ip6printfs & D6_NDP1)
		printf("ndadv6_input -> %s (state = %d)\n",
		       ip6_sprintf(&tgtaddr), ln->ln_state);
#endif
	return (1);
}

/*
 * Receive a Router Solicitation packet.
 */
int
rtsol6_input(struct mbuf *m0, struct ifnet *ifp)
{
	struct ipv6 *ip = mtod(m0, struct ipv6 *);
	struct icmpv6 *icp = (struct icmpv6 *)(ip + 1);
	register struct ndx6_any *xp;
	int icmplen = ip->ip6_len - ICMP6_RSLEN;
	register struct llinfo_ndp6 *ln = 0;
	register struct rtentry *rt;
	struct mbuf *mcopy = 0;
	struct sockaddr_dl *sdl;
	struct in6_addr srcaddr, dstaddr;
	u_char lladdr[ETHER_ADDR_LEN];
	int extra = sizeof(*ip) + ICMP6_RSLEN;
	int needed = sizeof(struct ndx6_lladdr);
	int haslladdr = 0;

	COPY_ADDR6(ip->ip6_src, srcaddr);
	COPY_ADDR6(ip->ip6_dst, dstaddr);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHERIN)
		printf("rtsol6_input from %s\n", ip6_sprintf(&srcaddr));
#endif
#ifdef IP6PRINTFS
	if (IS_MULTIADDR6(dstaddr) &&
	    (MADDR6_SCOPE(dstaddr) != MADDR6_SCP_LINK))
		log(LOG_INFO, "rtsol6_input to illegal mcast\n");
#endif
	if (icp->icmp6_code != 0) {
		log(LOG_INFO, "rtsol6_input code(%d) != 0\n", icp->icmp6_code);
		return (0);
	}

NEXT_EXTENSION("rtsol6_input")

	switch (xp->x6any_ext) {

	case NDX6_LLADDR_SRC:
		GETEXTLLADDR("rtsol6_input");

	default:
	next:
		GETNEXTEXT("rtsol6_input");
	}
	if (mcopy)
		m_freem(m0);
    done:
	if (haslladdr == 0)
		return (1);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHERIN)
		printf("rtsol6_input lladdr=%s\n", ether_sprintf(lladdr));
#endif
	if (IS_ANYADDR6(srcaddr))
		return (1);
	ROUTE_RDLOCK();
	ln = ndplookup(&srcaddr, 1);
	if (ln && (rt = ln->ln_rt) && (sdl = satosdl(rt->rt_gateway))) {
		LLSRCOVERWRITE("rtsol6_input");
		if (ln->ln_state >= LLNDP6_PROBING) {
			ROUTE_UNLOCK();
			return (1);
		}
#ifdef MULTI_HOMED
		UPDATEMHIFA("rtsol6_input");
#endif
		ENTERSTALE();
		/* a router should not send router solicitations */
		if (ln->ln_flags)
			ndp6_rtlost(rt, 0);
	}
	ROUTE_UNLOCK();
	return (1);
}

/*
 * Receive a Router Advertisement packet.
 */
int
rtadv6_input(struct mbuf *m0, struct ifnet *ifp)
{
	struct ipv6 *ip = mtod(m0, struct ipv6 *);
	struct icmpv6 *icp = (struct icmpv6 *)(ip + 1);
	register struct ndx6_any *xp;
	int icmplen = ip->ip6_len - ICMP6_RALEN;
	register struct llinfo_ndp6 *ln = 0;
	register struct rtentry *rt;
	struct mbuf *mcopy = 0;
	struct sockaddr_dl *sdl;
	struct in6_addr srcaddr, dstaddr;
	u_char lladdr[ETHER_ADDR_LEN];
	int extra = sizeof(*ip) + ICMP6_RALEN;
	int needed = sizeof(struct ndx6_lladdr);
	int haslladdr = 0, mtu = 0;

	COPY_ADDR6(ip->ip6_src, srcaddr);
	COPY_ADDR6(ip->ip6_dst, dstaddr);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHERIN)
		printf("rtadv6_input from %s\n", ip6_sprintf(&srcaddr));
#endif
	if (icp->icmp6_code != 0) {
		log(LOG_INFO, "rtadv6_input code(%d) != 0\n", icp->icmp6_code);
		return (0);
	}
	if (!IS_LOCALADDR6(srcaddr)) {
		log(LOG_INFO,
		    "rtadv6_input from illegal %s\n",
		    ip6_sprintf(&srcaddr));
		return (0);
	}
#ifdef IP6PRINTFS
	if (IS_MULTIADDR6(dstaddr) &&
	    (MADDR6_SCOPE(dstaddr) != MADDR6_SCP_LINK))
			log(LOG_INFO, "rtadv6_input to illegal mcast\n");
#endif

NEXT_EXTENSION("rtadv6_input")

	switch (xp->x6any_ext) {

	case NDX6_LLADDR_SRC:
		GETEXTLLADDR("rtadv6_input");

	case NDX6_MTU:
		GETEXTMTU("rtadv6_input");

	default:
	next:
		GETNEXTEXT("rtadv6_input");
	}
	if (mcopy)
		m_freem(m0);
#ifdef IP6PRINTFS
	if (haslladdr && (ip6printfs & D6_ETHERIN))
		printf("rtadv6_input lladdr=%s\n", ether_sprintf(lladdr));
	if (mtu && (ip6printfs & D6_ETHERIN))
		printf("rtadv6_input (ignored) mtu=%d\n", mtu);
#endif
    done:

	ROUTE_RDLOCK();
	ln = ndplookup(&srcaddr, 1);
	if (ln && (rt = ln->ln_rt) && (sdl = satosdl(rt->rt_gateway))) {
		ln->ln_flags = 1;
		if (ln->ln_state >= LLNDP6_PROBING) {
			ROUTE_UNLOCK();
			return (1);
		}
#ifdef MULTI_HOMED
		UPDATEMHIFA("rtadv6_input");
#endif
		if (haslladdr == 0) {
			/* this stupid router doesn't give its address */
			ln->ln_state = LLNDP6_INCOMPLETE;
			sdl->sdl_alen = 0;
			rt->rt_expire = time;
			rt->rt_flags &= ~RTF_REJECT;
			ROUTE_UNLOCK();
			return (1);
		}
		LLSRCOVERWRITE("rtadv6_input");
		ENTERSTALE();
	}
	ROUTE_UNLOCK();
	return (1);
}

/*
 * Receive a Redirect packet.
 */
int
redirect6_input(struct mbuf *m0, struct ifnet *ifp)
{
	struct ipv6 *ip = mtod(m0, struct ipv6 *);
	struct icmpv6 *icp = (struct icmpv6 *)(ip + 1);
	register struct ndx6_any *xp;
	int icmplen = ip->ip6_len - ICMP6_RDLEN;
	register struct llinfo_ndp6 *ln = 0;
	register struct rtentry *rt;
	struct mbuf *mcopy = 0;
	struct sockaddr_dl *sdl;
	struct in6_addr srcaddr, dstaddr, tgtaddr;
	u_char lladdr[ETHER_ADDR_LEN];
	int extra = sizeof(*ip) + ICMP6_RDLEN;
	int needed = sizeof(struct ndx6_lladdr);
	int haslladdr = 0, is_router;

	COPY_ADDR6(ip->ip6_src, srcaddr);
	COPY_ADDR6(ip->ip6_dst, dstaddr);
	COPY_ADDR6(icp->icmp6_tgt, tgtaddr);
	is_router = !SAME_ADDR6(tgtaddr, icp->icmp6_rdst);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHERIN)
		printf("redirect6_input from %s for %s to %s\n",
		       ip6_sprintf(&srcaddr),
		       ip6_sprintf(&icp->icmp6_rdst),
		       ip6_sprintf(&tgtaddr));
#endif
	if (icp->icmp6_code != 0) {
		log(LOG_INFO,
		    "redirect6_input code(%d) != 0\n",
		    icp->icmp6_code);
		return (0);
	}
	if (!IS_LOCALADDR6(srcaddr)) {
		log(LOG_INFO,
		    "redirect6_input from illegal %s\n",
		    ip6_sprintf(&srcaddr));
		return (0);
	}
#ifdef IP6PRINTFS
	if (is_router && !IS_LOCALADDR6(tgtaddr)) {
		log(LOG_INFO,
		    "redirect6_input for illegal router %s\n",
		    ip6_sprintf(&tgtaddr));
		return (0);
	}
#endif
	if (IS_MULTIADDR6(dstaddr))
		log(LOG_WARNING,
		    "redirect6_input to multicast %s\n",
		    ip6_sprintf(&dstaddr));
	if (IS_MULTIADDR6(tgtaddr)) {
		log(LOG_INFO, "redirect6_input for illegal mcast\n");
		return (0);
	}

NEXT_EXTENSION("redirect6_input")

	switch (xp->x6any_ext) {

	case NDX6_LLADDR_TGT:
		GETEXTLLADDR("redirect6_input");

	case NDX6_RDRT_HDR:
		break;

	default:
	next:
		GETNEXTEXT("redirect6_input");
	}
	if (mcopy)
		m_freem(m0);
    done:
	if (haslladdr == 0)
		return (1);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ETHERIN)
		printf("redirect6_input lladdr=%s\n", ether_sprintf(lladdr));
#endif
	/* external redirect: create == 2 */
	ROUTE_RDLOCK();
	ln = ndplookup(&tgtaddr, is_router ? 1 : 2);
	if (ln && (rt = ln->ln_rt) && (sdl = satosdl(rt->rt_gateway))) {
		if ((ln->ln_state >= LLNDP6_PROBING) &&
		    bcmp(lladdr, LLADDR(sdl), ETHER_ADDR_LEN)) {
			log(LOG_INFO,
			    "redirect6_input overwrite for %s/%s by %s\n",
			    ip6_sprintf(&tgtaddr),
			    ether_sprintf(lladdr), ip6_sprintf(&srcaddr));
			ln->ln_state = LLNDP6_INCOMPLETE;
		}
		bcopy(lladdr, LLADDR(sdl), sdl->sdl_alen = ETHER_ADDR_LEN);
		if (ln->ln_state >= LLNDP6_BUILTIN) {
			ROUTE_UNLOCK();
			return (1);
		}
#ifdef MULTI_HOMED
		if (rt->rt_ifp != ifp) {
			struct in6_ifaddr *ia;

			if (!IFND6_IS_LLINK(rt->rt_ifp))
				log(LOG_NOTICE,
				    "redirect6_input: router for %s ifp#%d->%d\n",
				    ip6_sprintf(&tgtaddr),
				    rt->rt_ifp->if_index,
				    ifp->if_index);
			rt->rt_ifp = ifp;
			rt->rt_mtu = ifp->if_mtu;
			sdl->sdl_type = ifp->if_type;
			sdl->sdl_index = ifp->if_index;
			ia = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
			  in6_nd_match, (caddr_t)&dstaddr, 0, (caddr_t)ifp);
			if (ia == (struct in6_ifaddr *)0)
				IFP_TO_IA6(rt->rt_ifp, ia);
			if (ia) {
				ifafree(rt->rt_ifa);
				rt->rt_ifa = &ia->ia_ifa;
				atomicAddUint(&rt->rt_ifa->ifa_refcnt, 1);
			} else
				log(LOG_ERR, "redirect6_input without ifa?\n");
		}
#endif
		ln->ln_flags |= is_router;
		ENTERSTALE();
	}
	ROUTE_UNLOCK();
	return (1);
}

/*
 * Neighbor Unreachable Discovery hints from upper layers.
 */
void
ip6_reachhint(inp)
	register struct inpcb *inp;
{
	register struct rtentry *rt;
	register struct llinfo_ndp6 *ln;

	if (inp == 0 ||
	    (inp->inp_flags & INP_COMPATV6) == 0 ||
	    (inp->inp_latype & IPATYPE_IPV6) == 0 ||
	    (inp->inp_fatype & IPATYPE_IPV6) == 0)
		return;
	if ((rt = inp->inp_route.ro_rt) == 0)
		return;
	if (rt->rt_flags & RTF_GATEWAY)
		rt = rt->rt_gwroute;
	if (rt == 0 ||
	    rt->rt_flags & RTF_GATEWAY ||
	    (rt->rt_flags & RTF_DYNAMIC) == 0 ||
	    (rt->rt_flags & RTF_LLINFO) == 0 ||
	    rt->rt_expire == 0 ||
	    rt->rt_ifa->ifa_rtrequest != ndp6_rtrequest ||
	    !SAME_ADDR6(satosin6(&inp->inp_route.ro_dst)->sin6_addr,
			inp->inp_faddr6))
		return;
	ln = (struct llinfo_ndp6 *)rt->rt_llinfo;
	if (ln == 0 || ln->ln_state < LLNDP6_PROBING)
		return;
#ifdef IP6PRINTFS
	if (ip6printfs & D6_NDP1)
		printf("ip6_reachhint(%p)\n", rt);
#endif
	if (ln->ln_state == LLNDP6_PROBING)
		ln->ln_state = LLNDP6_REACHABLE;
	rt->rt_expire = time + ndpt_reachable;
}

/*
 * Lookup or enter a new address in the ndp table.
 */
struct llinfo_ndp6 *
ndplookup(addr, create)
	struct in6_addr *addr;
	int create;
{
	register struct rtentry *rt;
	extern struct sockaddr_in6 in6_zeroaddr;
	struct sockaddr_in6 sin = in6_zeroaddr;
	const char *why = 0;

	ASSERT(ROUTE_ISLOCKED());
#ifdef IP6PRINTFS
	if (ip6printfs & D6_NDP1)
		printf("ndplookup(%s,%d)\n", ip6_sprintf(addr), create);
#endif
	COPY_ADDR6(*addr, sin.sin6_addr);
	/* TODO: add proxy */
	rt = rtalloc1(sin6tosa(&sin), create);
	if (rt == 0)
		return (0);
	RT_RELE(rt);
	if (rt->rt_flags & RTF_GATEWAY)
		why = "host is not on local network";
	else if ((rt->rt_flags & RTF_LLINFO) == 0)
		why = "could not allocate llinfo";
	else if (rt->rt_gateway->sa_family != AF_LINK)
		why = "gateway route is not ours";
	else if ((rt->rt_ifa == 0) ||
		 (rt->rt_ifa->ifa_rtrequest == 0) ||
		 (rt->rt_ifa->ifa_rtrequest != ndp6_rtrequest))
		why = "host is not on an IEEE network";

	if (why) {
		if (create > 1) {
			/* external redirects done by an user process */
			return (0);
		} else if (create)
			log(LOG_DEBUG, "ndplookup %s failed: %s\n",
			    ip6_sprintf(addr), why);
		return (0);
	}
#ifdef IP6PRINTFS
	if (ip6printfs & D6_NDP1)
		printf("ndplookup return %p\n", rt->rt_llinfo);
#endif
	return ((struct llinfo_ndp6 *)rt->rt_llinfo);
}

void
ndp6_ifinit(ifp, ifa)
	struct ifnet *ifp;
	struct ifaddr *ifa;
{
	register struct sockaddr_in6 *sin = satosin6(ifa->ifa_addr);

	if (IS_LOCALADDR6(sin->sin6_addr)) {
		ifp->if_ndtype |= IFND6_LLSET;
		COPY_ADDR6(sin->sin6_addr, *GETLLADDR(ifp));
	}
	ifa->ifa_rtrequest = ndp6_rtrequest;
	ifa->ifa_flags |= RTF_CLONING;
}

/*
 * Neighbor Discovery has detected the lost of a router.
 */
void
ndp6_rtlost(rt, dead)
	struct rtentry *rt;
	int dead;
{
	struct rt_addrinfo info;
	struct llinfo_ndp6 *ln;

	if (dead) {
		/* we know the router is dead or no more a router */
		bzero((caddr_t)&info, sizeof(info));
		info.rti_info[RTAX_DST] = rt_key(rt);
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		rt_missmsg(RTM_RTLOST, &info, 0, 0);
	} else {
		/* the router is suspect, force a neighbor solicitation */
		ln = (struct llinfo_ndp6 *)rt->rt_llinfo;
		if (ln && ln->ln_state) {
			ln->ln_state = LLNDP6_PROBING;
			rt->rt_expire = time + ndpt_probe;
		}
	}
}
#endif /* INET6 */
