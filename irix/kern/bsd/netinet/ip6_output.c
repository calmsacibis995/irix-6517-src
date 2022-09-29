#ifdef INET6
/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)ip_output.c	8.3 (Berkeley) 1/21/94
 */

#include <tcp-param.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/hashing.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
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
#include <netinet/ip6_opts.h>
#include <sys/tcpipstats.h>

#ifdef vax
#include <machine/mtpr.h>
#endif

u_int32_t ip6_id;
extern zone_t *ipmember_zone;

static void ip6_mloopback __P((struct ifnet *,
		struct mbuf *, struct mbuf *, struct sockaddr_in6 *));
extern struct sockaddr_in6 in6_zeroaddr;
extern int looutput(struct ifnet *, struct mbuf *, struct sockaddr *,
        struct rtentry *);
extern int in6addr_ifnetaddr_match(struct hashbucket *h,
  caddr_t key, caddr_t arg1, caddr_t arg2);

#define M_LEADINGSPACE(m) \
        ((m)->m_flags & M_CLUSTER ? /* (m)->m_data - (m)->m_farg */ 0 : \
	    (m)->m_off - MMINOFF)


/*
 * IPv6 output. The packet in mbuf chain m contains a skeletal IPv6
 * header (with everything :-).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
int
ip6_output(m0, opts, ro, flags, imo, inp)
	struct mbuf *m0;
	struct mbuf *opts;
	struct route *ro;
	int flags;
	struct ip_moptions *imo;
	struct inpcb *inp;
{
	struct ipv6 *ip;
	struct ifnet *ifp;
	int len, mtu, error = 0, oplen;
	/*
	 * It might seem obvious at first glance that one could easily
	 * make a one-behind cache out of this by simply making `iproute'
	 * static and eliminating the bzero() below.  However, this turns
	 * out not to work, for two reasons:
	 *
	 * 1) This routine needs to be reentrant.  It can be called
	 * recursively from encapsulating network interfaces, and it
	 * is always called recursively from ip6_mforward().
	 *
	 * 2) You turn out not to gain much.  There is already a one-
	 * behind cache implemented for the specific case of forwarding,
	 * and sends on a connected socket will use a route associated
	 * with the PCB.  The only cases left are sends on unconnected
	 * and raw sockets, and if these cases are really significant,
	 * something is seriously wrong.
	 */
	union route_6 iproute_6;
	struct sockaddr_in6 *dst;
	struct in6_ifaddr *ia;
	struct ip_soptions *iso = inp ? &inp->inp_soptions : 0;

#define iproute		iproute_6.route

#ifdef  IP6PRINTFS
	if (ip6printfs & D6_OUTPUT) {
		ip = mtod(m0, struct ipv6 *);		
		printf("ip6_output(%p,%p,%x,%d,%p,%p)",
		       m0, opts, ro, flags, imo, iso);
		printf(" src %s dst %s len %d (mbuf %d)\n",
		       ip6_sprintf(&ip->ip6_src),
		       ip6_sprintf(&ip->ip6_dst),
		       ip->ip6_len, m0->m_len);
	}
#endif
	if (opts) {
	nextopt:
		switch (OPT6_TYPE(opts)) {
		case IP6_NHDR_DOPT:
			if (dopt6_dontfrag(opts))
				break;
			/* falls into */

		case IP6_NHDR_AUTH:
		case IP6_NHDR_ESP:
			m0 = ip6_insertoption(m0, opts, iso, 0, &error);
			if (error != 0) {
				m_freem(m0);
				return (error);
			}
			/* falls into */

		default:
			/* not yet supported: ignored !? */
			opts = opts->m_next;
			if (opts)
				goto nextopt;
			break;

		case IP6_NHDR_RT: {
			register struct ipv6_rthdr *rhp;

			/* update destination address */
			ip = mtod(m0, struct ipv6 *);
			rhp = OPT6_DATA(opts, struct ipv6_rthdr *);
			bcopy((caddr_t)rhp + OPT6_LEN(opts),
			      (caddr_t)&ip->ip6_dst,
			      sizeof(struct in6_addr));
			if (ntohl(rhp->ir6_slmsk) & IP6_RT_SLBIT(0))
				flags |= IP6_RT_SLBIT(0);
			}
			break;

		case IP6_NHDR_HOP:
			/* fragment (if necessary) before */
			break;
		}
	}
	ip = mtod(m0, struct ipv6 *);
	len = (u_int16_t)ip->ip6_len + sizeof(struct ipv6);
	if (opts) {
		struct mbuf *o;

		for (o = opts, oplen = 0; o; o = o->m_next)
			oplen += OPT6_LEN(o);
		len += oplen;
	}

	if ((flags & IP_FORWARDING) == 0)
		IP6STAT(ip6s_localout);
	/*
	 * Route packet.
	 */
	if (ro == 0) {
		ro = &iproute;
		bzero((caddr_t)ro, sizeof(union route_6));
	}
	dst = satosin6(&ro->ro_dst);

	/*
	 * If there is a cached route,
	 * check that it is to the same destination
	 * and is still up.  If not, free it and try again.
	 */
	ROUTE_RDLOCK();
	if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
			  !SAME_ADDR6(dst->sin6_addr, ip->ip6_dst))) {
		rtfree_needpromote(ro->ro_rt);
		ro->ro_rt = (struct rtentry *)0;
	}
	if (ro->ro_rt == 0) {
		dst->sin6_family = AF_INET6;
		dst->sin6_len = sizeof(struct sockaddr_in6);
		COPY_ADDR6(ip->ip6_dst, dst->sin6_addr);
	}

	/*
	 * If routing to interface only,
	 * short circuit routing lookup.
	 */
	if (flags & IP_ROUTETOIF) {
		ROUTE_UNLOCK();
		if ((ia = ifatoia6(ifa_ifwithdstaddr(sin6tosa(dst)))) == 0 &&
		    (ia = ifatoia6(ifa_ifwithnet(sin6tosa(dst)))) == 0) {
			IP6STAT(ip6s_noroute);
			error = ENETUNREACH;
			goto bad;
		}
		ifp = ia->ia_ifp;
		ip->ip6_hlim = 1;
	} else if (flags & IP6_RT_SLBIT(0) &&
		   (ia = ifatoia6(ifa_ifwithdstaddr(sin6tosa(dst)))) == 0 &&
		   (ia = ifatoia6(ifa_ifwithnet(sin6tosa(dst)))) == 0) {
		/*
		 * We are holding the route lock across the ifa_xxx
		 * calls above.  It would be better not to but
		 * (flags & IP6_RT_SLBIT(0)) will almost never be true
		 * so we almost never hit that case.
		 */
		ROUTE_UNLOCK();
		IP6STAT(ip6s_noroute);
		error = EHOSTUNREACH;
		goto bad;
	} else {
		if (ro->ro_rt == 0)
			in6_rtalloc(ro, INP_IFA);
		if (ro->ro_rt == 0) {
			ROUTE_UNLOCK();
			IP6STAT(ip6s_noroute);
			error = EHOSTUNREACH;
			goto bad;
		}
		ia = ifatoia6(ro->ro_rt->rt_ifa);
		ifp = ro->ro_rt->rt_ifp;
		ro->ro_rt->rt_use++;
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = satosin6(ro->ro_rt->rt_gateway);
		ROUTE_UNLOCK();
	}
	if (IS_MULTIADDR6(ip->ip6_dst)) {
		struct in6_multi *inm;

#ifdef  IP6PRINTFS
		if (ip6printfs & D6_MCASTOUT)
			printf("ip6_output multicast\n");
#endif
		m0->m_flags |= M_MCAST;
		/*
		 * IPv6 destination address is multicast.  Make sure "dst"
		 * still points to the address in "ro".  (It may have been
		 * changed to point to a gateway address, above.)
		 */
		dst = satosin6(&ro->ro_dst);
		/*
		 * See if the caller provided any multicast options
		 */
		if (imo != NULL) {
			ip->ip6_hlim = imo->imo_multicast_ttl;
			if (imo->imo_multicast_ifp != NULL)
				ifp = imo->imo_multicast_ifp;
		} else
			ip->ip6_hlim = IP_DEFAULT_MULTICAST_TTL;
		/*
		 * Confirm that the outgoing interface supports multicast.
		 */
		if ((ifp->if_flags & IFF_MULTICAST) == 0) {
			IP6STAT(ip6s_noroute);
			error = ENETUNREACH;
			goto bad;
		}
		/*
		 * If source address not specified yet, use address
		 * of outgoing interface unless we know what we are doing.
		 */
		if (IS_ANYADDR6(ip->ip6_src) && (flags & IP_RAWOUTPUT) == 0) {
			register struct in6_ifaddr *ia;

			ia = (struct in6_ifaddr *)ifp->in6_ifaddr;
			COPY_ADDR6(IA_SIN6(ia)->sin6_addr, ip->ip6_src);
		}

		IN6_LOOKUP_MULTI(ip->ip6_dst, ifp, inm);
		if (inm != NULL &&
		   (imo == NULL || imo->imo_multicast_loop)) {
			/*
			 * If we belong to the destination multicast group
			 * on the outgoing interface, and the caller did not
			 * forbid loopback, loop back a copy.
			 */
			ip6_mloopback(ifp, m0, opts, dst);
		} else {
			/*
			 * If we are acting as a multicast router, perform
			 * multicast forwarding as if the packet had just
			 * arrived on the interface to which we are about
			 * to send.  The multicast forwarding function
			 * recursively calls this function, using the
			 * IP_FORWARDING flag to prevent infinite recursion.
			 *
			 * Multicasts that are looped back by ip_mloopback(),
			 * above, will be forwarded by the ip_input() routine,
			 * if necessary.
			 */
			if (ip6_mrouter && (flags & IP_FORWARDING) == 0) {
				/*
				 * Check if rsvp daemon is running.
				 * If not, don't set ip_moptions.
				 * This ensures that the packet is multicast
				 * and not just sent down one link
				 * as prescribed by rsvpd.
				 */
#ifdef notyet
				if (!rsvp_on)
#endif
					imo = NULL;
				if (ip6_mforward(ip, ifp, m0, imo) != 0) {
					m_freem(m0);
					goto done;
				}
			}
		}
		/*
		 * Multicasts with a time-to-live of zero may be looped-
		 * back, above, but must not be transmitted on a network.
		 * Also, multicasts addressed to the loopback interface
		 * are not sent -- the above call to ip6_mloopback() will
		 * loop back a copy if this host actually belongs to the
		 * destination group on the loopback interface.
		 */
		if (ip->ip6_hlim == 0 || (ifp->if_flags & IFF_LOOPBACK) != 0) {
			m_freem(m0);
			goto done;
		}

		goto sendit;
	}
#ifdef IP6PRINTFS
	if (in6_isanycast(&ip->ip6_src))
		log(LOG_ERR,
		    "illegal anycast source %s\n",
		    ip6_sprintf(&ip->ip6_src));
#endif

    sendit:

	if (flags & IP_ROUTETOIF)
		mtu = ifp->if_mtu;
	else if (ro->ro_rt->rt_rmx.rmx_mtu) {
		mtu = ro->ro_rt->rt_rmx.rmx_mtu;
		if (mtu > ifp->if_mtu) {
			/* This case can happen if the user changed the MTU
			 * of an interface after enabling IPv6 on it.
			 */
			mtu = ro->ro_rt->rt_rmx.rmx_mtu = ifp->if_mtu;
			/* notify MTU change */
		}
	} else
		mtu = ro->ro_rt->rt_rmx.rmx_mtu = ifp->if_mtu;
#ifdef IP6PRINTFS
	if (ip6printfs & D6_OUTPUT)
		printf("ip6_output src %s ifp %p len %d mtu %d\n",
		       ip6_sprintf(&ip->ip6_src),
		       ifp, len, mtu);
#endif
	/*
	 * If small enough for interface, can just send directly.
	 */
	if (len <= mtu) {
		mtu = 0;
		while (opts) {
			m0 = ip6_insertoption(m0, opts, iso, 0, &error);
			if (error != 0)
				goto bad;
			opts = opts->m_next;
			mtu = 1;
		}
		if (mtu)
			ip = mtod(m0, struct ipv6 *);
		ip->ip6_len = htons((u_int16_t)ip->ip6_len);
		IFNET_UPPERLOCK(ifp);
		error = (*ifp->if_output)(ifp, m0, sin6tosa(dst), ro->ro_rt);
		IFNET_UPPERUNLOCK(ifp);
		goto done;
	}
	/*
	 * IPv6 fragmentation is end-to-end only.
	 */
	if (flags & (IP_FORWARDING|IP_RAWOUTPUT|IP6_DONTFRAG)) {
		error = EMSGSIZE;
		goto bad;
	}
    {
	register struct ipv6_fraghdr *frgp;
	register struct mbuf *m;
	int off, mhlen, firstlen, plen;
	struct mbuf **mnext = &m0->m_act;

	len += sizeof(struct ipv6_fraghdr);
	mhlen = sizeof(struct ipv6) + sizeof(struct ipv6_fraghdr);
	if (opts)
		mhlen += oplen;
	plen = firstlen = (mtu - mhlen) &~ 7;
	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (firstlen < 8) {
		error = EMSGSIZE;
		goto bad;
	}
	/*
	 * Insert fragment header and others
	 */
	if (M_LEADINGSPACE(m0) >= mhlen - sizeof(struct ipv6)) {
		m0->m_off -= sizeof(struct ipv6_fraghdr);
		m0->m_len += sizeof(struct ipv6_fraghdr);
		ovbcopy((caddr_t)ip, mtod(m0, caddr_t), sizeof(*ip));
	} else {
		if (mhlen > MCLBYTES) {
			error = EMSGSIZE;
			goto bad;
		}
		m = m_vget(M_DONTWAIT, mhlen, MT_HEADER);
		if (m == 0) {
			error = ENOBUFS;
			IP6STAT(ip6s_odropped);
			goto bad;
		}
		m->m_flags = m0->m_flags & M_COPYFLAGS;
		if (MLEN >= mhlen)
			MH_ALIGN(m, mhlen);
		m->m_next = m0;
		m->m_len = mhlen;
		if (opts)
			m->m_off += oplen;
		bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ipv6));
		m0->m_off += sizeof(struct ipv6);
		m0->m_len -= sizeof(struct ipv6);
		m0 = m;
		mnext = &m0->m_act;
	}
	ip = mtod(m0, struct ipv6 *);
	frgp = (struct ipv6_fraghdr *)((caddr_t)ip + mhlen) - 1;
	frgp->if6_res = 0;
	frgp->if6_nh = ip->ip6_nh;
	ip->ip6_nh = IP6_NHDR_FRAG;
	frgp->if6_id = htonl(atomicAddInt(&ip6_id, 1));
	mtu = 0;
	while (opts) {
		m0 = ip6_insertoption(m0, opts, iso,
		  IP6_INSOPT_NOALLOC, &error);
		if (error != 0)
			goto bad;
		opts = opts->m_next;
		mtu = 1;
	}
	if (mtu)
		ip = mtod(m0, struct ipv6 *);
	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	for (off = mhlen + firstlen; off < len; off += firstlen) {
		m = m_vget(M_DONTWAIT, mhlen, MT_HEADER);
		if (m == 0) {
			error = ENOBUFS;
			IP6STAT(ip6s_odropped);
			goto sendorfree;
		}
		if (MLEN >= mhlen)
			MH_ALIGN(m, mhlen);
		*mnext = m;
		mnext = &m->m_act;
		m->m_len = mhlen;
		bcopy(mtod(m0, caddr_t), mtod(m, caddr_t), mhlen);
		ip = mtod(m, struct ipv6 *);
		frgp = (struct ipv6_fraghdr *)((caddr_t)ip + mhlen) - 1;
		frgp->if6_res = 0;
		frgp->if6_off = off - mhlen;
		if (off + plen >= len)
			plen = len - off;
		else
			frgp->if6_off |= IP6_MF;
		frgp->if6_off = htons((u_int16_t)frgp->if6_off);
		ip->ip6_len = htons((u_int16_t)(mhlen + plen - sizeof(*ip)));
		m->m_next = m_copy(m0, off, plen);
		if (m->m_next == 0) {
			error = ENOBUFS;	/* ??? */
			IP6STAT(ip6s_odropped);
			goto sendorfree;
		}
		IP6STAT(ip6s_ofragments);
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m = m0;
	m_adj(m, mhlen + firstlen - len);
	ip = mtod(m0, struct ipv6 *);
	frgp = (struct ipv6_fraghdr *)((caddr_t)ip + mhlen) - 1;
	frgp->if6_off = htons((u_int16_t)IP6_MF);
	ip->ip6_len = htons((u_int16_t)((mhlen + firstlen)- sizeof(*ip)));
sendorfree:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_FRAG)
		printf("ip6_output fragment (error=%d)\n", error);
#endif
	for (m = m0; m; m = m0) {
		m0 = m->m_act;
		m->m_act = 0;
		if (error == 0) {
			IFNET_UPPERLOCK(ifp);
			error = (*ifp->if_output)(ifp, m,
						  sin6tosa(dst), ro->ro_rt);
			IFNET_UPPERUNLOCK(ifp);
		} else
			m_freem(m);
	}

	if (error == 0)
		IP6STAT(ip6s_fragmented);
    }
done:
	if (ro == &iproute && (flags & IP_ROUTETOIF) == 0 && ro->ro_rt) {
		rtfree_needlock(ro->ro_rt);
	}
	return (error);
bad:
#ifdef  IP6PRINTFS
	if (ip6printfs & D6_OUTPUT)
		printf("ip6_output bad\n");
#endif
	m_freem(m0);
	goto done;
}

/*
 * Test if a destination header is front or back.
 * (should use option types)
 */
int
dopt6_dontfrag(opts0)
	struct mbuf *opts0;
{
	register struct mbuf *opts = opts0;

	while ((opts = opts->m_next) != 0)
		switch (OPT6_TYPE(opts)) {
		case IP6_NHDR_RT:
		case IP6_NHDR_FRAG:
		case IP6_NHDR_AUTH:
		case IP6_NHDR_ESP:
		case IP6_NHDR_DOPT:
			return (0);
		}
	/* decode options (TODO) */
	/* default is can fragment */
	return (0);
}

/*
 * Insert an IPv6 option into a packet.
 * Adjust IPv6 destination as required for IPv6 source routing.
 */
struct mbuf *
ip6_insertoption(m, opt, iso, flags, error)
	register struct mbuf *m;
	struct mbuf *opt;
	struct ip_soptions *iso;
	int flags;
	int *error;
{
	register struct mbuf *n;
	register struct ipv6 *ip = mtod(m, struct ipv6 *);
	register u_char *cp;
	register unsigned optlen;
	int nh = 0;

	optlen = OPT6_LEN(opt);
	if (OPT6_TYPE(opt) == IP6_NHDR_ESP) {
		/* the size of init vector is in the last byte */
		cp = OPT6_DATA(opt, u_char *) + optlen - 1;
		optlen = *cp + sizeof(struct ipv6_esphdr);
	}
#ifdef IP6PRINTFS
	if (ip6printfs & D6_OPTOUT) {
		printf("ip6_insertoption(%p,%p,%p,%d)", m, opt, iso, flags);
		printf(" type %d olen %d\n",
		       OPT6_TYPE(opt), optlen);
	}
#endif
	if (optlen + (u_int16_t)ip->ip6_len + sizeof(*ip) > IP_MAXPACKET) {
		*error = EMSGSIZE;  /* options are too big */
		return (m);
	}
	if (((flags & IP6_INSOPT_NOALLOC) == 0) &&
	    (M_LEADINGSPACE(m) < optlen)) {
		n = m_vget(M_DONTWAIT, sizeof(*ip) + optlen, MT_HEADER);
		if (n == 0) {
			IP6STAT(ip6s_onomem);
			*error = ENOBUFS;
			return (m);
		}
		n->m_flags = m->m_flags & M_COPYFLAGS;
		if (MLEN >= sizeof(*ip) + optlen)
			MH_ALIGN(n, sizeof(*ip) + optlen);
		m->m_len -= sizeof(struct ipv6);
		m->m_off += sizeof(struct ipv6);
		n->m_next = m;
		m = n;
		m->m_len = optlen + sizeof(struct ipv6);
		bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(*ip));
	} else {
		m->m_off -= optlen;
		m->m_len += optlen;
		ovbcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(*ip));
	}
	ip = mtod(m, struct ipv6 *);
	cp = (u_char *)(ip + 1);
	bcopy(OPT6_DATA(opt, caddr_t), cp, optlen);
	ip->ip6_len += optlen;
	if (OPT6_TYPE(opt) == IP6_NHDR_ESP)
		nh = ip->ip6_nh;
	else
		*cp = ip->ip6_nh;
	ip->ip6_nh = OPT6_TYPE(opt);
	switch (ip->ip6_nh) {
	    case IP6_NHDR_RT:
		if ((flags & IP6_INSOPT_RAW) == 0)
			bcopy(OPT6_DATA(opt, caddr_t) + optlen,
			      (caddr_t)&ip->ip6_dst,
			      sizeof(struct in6_addr));
		break;

	    case IP6_NHDR_HOP:
	    case IP6_NHDR_DOPT:
		if ((flags & IP6_INSOPT_RAW) == 0)
			hd6_outoptions(m);
		break;

	    case IP6_NHDR_AUTH:
		if ((flags & IP6_INSOPT_RAW) == 0)
			m = ah6_output(m, opt->m_next, iso);
		break;

	    case IP6_NHDR_ESP:
		m = esp6_output(m, opt, iso, optlen, nh);
		break;
	}
	return (m);
}

/*
 * (Generic) header option processing.
 */
/*ARGSUSED*/
void
hd6_outoptions(m)
	struct mbuf *m;
{
#ifdef notdef
	register struct ipv6 *ip;
	register struct ipv6_h2hhdr *hp;
	register struct opt6_any *op;
	register int len;

	ip = mtod(m, struct ipv6 *);
	hp = (struct ipv6_h2hhdr *)(ip + 1);
	op = (struct opt6_any *)&hp->ih6_pad1;
	len = (hp->ih6_hlen + 1) * sizeof(*hp) - 2 * sizeof(u_int8_t);

	/* Zap the whole thing ?! */
	/* TODO: use OPT6_ACTION in order to be more clever ! */
	/* note: this can be called with an incorrect option for ICMP */
	/* but now we have IP6_INSOPT_RAW */
	op->o6any_ext = OPT6_PAD_N;
	op->o6any_len = len - sizeof(*op);
#endif
}

/*
 * Authentication Header processing.
 */
struct mbuf *
ah6_output(m0, opts, iso)
	struct mbuf *m0, *opts;
	struct ip_soptions *iso;
{
	register struct ipv6 *ip;
	register struct ipv6_authhdr *hp;
	register struct ipsec_entry_header *ieh;
	register caddr_t state;
	struct mbuf *m = 0;
	int len, hlen;
	struct sockaddr_ipsec ips6addr;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_AH)
		printf("ah6_output(%p,%p,%p)\n", m0, opts, iso);
#endif
	ip = mtod(m0, struct ipv6 *);
	hp = (struct ipv6_authhdr *)(ip + 1);
	hlen = hp->ah6_hlen * 8;
	len = hlen + sizeof(*hp);
	ips6addr.sis_len = sizeof (ips6addr);
	ips6addr.sis_family = AF_INET6;
	ips6addr.sis_way = IPSEC_OUT;
	ips6addr.sis_kind = IP6_NHDR_AUTH;
	ips6addr.sis_spi = hp->ah6_spi;
	COPY_ADDR6(ip->ip6_dst, ips6addr.sis_addr);
	ieh = ipsec_lookup(&ips6addr, iso ? iso->lh_first : 0);
	if (ieh)
		ieh->ieh_refcnt--;
	if ((len > m0->m_len) ||
	    (ieh == 0) ||
	    (ieh->ieh_algo == 0) ||
	    (ieh->ieh_algo->isa_ah_reslen > hlen))
		goto bad;

	ieh->ieh_use++;
	bzero((caddr_t)(hp + 1), ieh->ieh_algo->isa_ah_reslen);
	if ((m = ah6_build(m0, opts, IPSEC_OUT)) == 0)
		goto bad;
	if ((state = ieh->ieh_algo->isa_ah_init(ieh)) == 0)
		goto bad;
	state = ieh->ieh_algo->isa_ah_update(state, m);
	ieh->ieh_algo->isa_ah_finish(state, (caddr_t)(hp + 1), ieh);
	m_freem(m);
	return (m0);

    bad:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_AH)
		printf("ah6_output failed\n");
#endif
	if (m)
		m_freem(m);
	return (0);
}

/*
 * Encryption Security Payload processing.
 */
struct mbuf *
esp6_output(m0, opts, iso, optlen, nh)
	struct mbuf *m0, *opts;
	struct ip_soptions *iso;
	int optlen, nh;
{
	register struct ipv6 *ip;
	register struct ipv6_esphdr *hp;
	register struct ipsec_entry_header *ieh;
	register caddr_t state;
	struct mbuf *m = 0;
	int len, padlen;
	struct sockaddr_ipsec ips6addr;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ESP)
		printf("esp6_output(%p,%p,%p,%d,%d)\n",
		       m0, opts, iso, optlen, nh);
#endif
	ip = mtod(m0, struct ipv6 *);
	hp = OPT6_DATA(opts, struct ipv6_esphdr *);
	len = ip->ip6_len - optlen + 2;
	padlen = (-len) & 7;
	ips6addr.sis_len = sizeof (ips6addr);
	ips6addr.sis_family = AF_INET6;
	ips6addr.sis_way = IPSEC_OUT;
	ips6addr.sis_kind = IP6_NHDR_ESP;
	ips6addr.sis_spi = hp->esp6_spi;
	COPY_ADDR6(ip->ip6_dst, ips6addr.sis_addr);
	ieh = ipsec_lookup(&ips6addr, iso ? iso->lh_first : 0);
	if (ieh)
		ieh->ieh_refcnt--;
	if ((ieh == 0) ||
	    (ieh->ieh_algo == 0) ||
	    (ieh->ieh_ivlen + sizeof(*hp) != optlen))
		goto bad;

	ieh->ieh_use++;
	if ((m = esp6_build(m0, ieh->ieh_ivlen, padlen, nh)) == 0)
		goto bad;
	if ((state = ieh->ieh_algo->isa_esp_init(ieh, (caddr_t)(hp + 1))) == 0)
		goto bad;
	state = ieh->ieh_algo->isa_esp_encrypt(state, m);
	ieh->ieh_algo->isa_esp_finish(state);

	if (m0->m_next)
		m_freem(m0->m_next);
	m0->m_next = m;
	m0->m_len = optlen + sizeof(*ip);
	ip->ip6_len += padlen + 2;
	return (m0);

    bad:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_ESP)
		printf("esp6_output failed\n");
#endif
	if (m)
		m_freem(m);
	return (0);
}

/* ARGSUSED */
static int
ifp_to_localaddr_enum(struct hashbucket *h, caddr_t key,
  caddr_t arg1, caddr_t arg2)
{
	struct in6_ifaddr *ia = (struct in6_ifaddr *)h;
	struct ifnet *ifp = (struct ifnet *)ifp;

	if (ia->ia_ifp == ifp) {
		if (IS_LOCALADDR6(IA_SIN6(ia)->sin6_addr))
			return (1);
	}
	return (0);
}

/*
 * IPv6 socket option processing.
 */
int
ip6_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	register struct inpcb *inp = sotoinpcb(so);
	register struct mbuf *m = *mp;
	register int optval = 0;
	struct sockaddr_in6 *sin = 0;
	struct ifnet *ifp = 0;
	struct in6_ifaddr *ia = 0;
	struct in6_pktinfo *pi = 0;
	int error = 0;

	if ((level == IP6_NHDR_AUTH) || (level == IP6_NHDR_ESP)) {
		if ((inp->inp_flags & INP_COMPATV6) == 0)
			return (ENOPROTOOPT);
		inp->inp_flags &= ~INP_COMPATV4;
		if (op == PRCO_SETOPT)
			return (ip6_setsoptions(so, optname, m));
		else
			return (ip6_getsoptions(inp, optname, mp));
	}

	if ((level != IPPROTO_IP) && (level != IPPROTO_IPV6)) {
		error = EINVAL;
		if (op == PRCO_SETOPT && *mp)
			(void) m_free(*mp);
	} else switch (op) {

	case PRCO_SETOPT:
		switch (optname) {
		case IP_OPTIONS:
			if ((inp->inp_flags & INP_COMPATV6) == 0) {
				error = ENOPROTOOPT;
				break;
			}
			inp->inp_flags &= ~INP_COMPATV4;
			return (ip6_setoptions(&inp->inp_options, m, inp));

		case IP_TOS:
			if (so->so_proto->pr_type != SOCK_STREAM)
				goto fallin;
			/* try to remap TOS into Priority */
			if (m == NULL || m->m_len != sizeof(int)) {
				error = EINVAL;
				break;
			}
			optval = *mtod(m, int *);
			switch (optval) {

			case IPTOS_LOWDELAY:
				inp->inp_oflowinfo =
				  IPV6_PRIORITY_INTERACTIVE;
				break;

			case IPTOS_THROUGHPUT:
				inp->inp_oflowinfo =
				  IPV6_PRIORITY_BULK;
				break;
			}
			/* falls into */

		case IPV6_UNICAST_HOPS:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IPV6_RECVPKTINFO:
		case IPV6_RECVHOPS:
		fallin:
			return(ip_ctloutput(op, so, level, optname, mp));

		case IPV6_MULTICAST_IF:
		case IPV6_MULTICAST_HOPS:
		case IPV6_MULTICAST_LOOP:
		case IPV6_ADD_MEMBERSHIP:
		case IPV6_DROP_MEMBERSHIP:
			if ((inp->inp_flags & INP_COMPATV6) == 0) {
				error = ENOPROTOOPT;
				break;
			}
			inp->inp_flags &= ~INP_COMPATV4;
			error = ip6_setmoptions(&inp->inp_moptions, optname, m);
			break;

		case IPV6_SENDIF:
			if (m == NULL ||
			    (m->m_len && m->m_len < sizeof(u_short))) {
				error = EINVAL;
				break;
			}
			if (m->m_len == 0) {
#ifdef INTERFACES_ARE_SELECTABLE
				if (inp->inp_ifa)
					ifafree(inp->inp_ifa);
				inp->inp_ifa = 0;
#endif
				break;
			} else if (m->m_len == sizeof(u_short))
				optval = *mtod(m, u_short *);
			else {
				sin = mtod(m, struct sockaddr_in6 *);
				if (sin->sin6_len > m->m_len) {
					error = EINVAL;
					break;
				}
				if (sin->sin6_family == AF_LINK) {
#define SDLINDEX(x)	(((struct sockaddr_dl *)x)->sdl_index)
					optval = SDLINDEX(sin);
					sin = 0;
				} else if (sin->sin6_family == AF_INET6) {
					if (sin->sin6_len != sizeof(*sin)) {
						error = EINVAL;
						break;
					}
					optval = sin->sin6_port;
					sin->sin6_port = 0;
				}
			}
		sendif1:
			if (optval) {
				for (ifp = ifnet; ifp; ifp = ifp->if_next)
					if (ifp->if_index == optval)
						break;
				if (ifp == 0) {
					error = ENXIO;
					break;
				}
				/* loopback is a special case! */
				if (ifp->if_flags & IFF_LOOPBACK) {
#ifdef MULTI_HOMED
					if (inp->inp_ifa)
						ifafree(inp->inp_ifa);
					inp->inp_ifa = 0;
#endif
					break;
				}
			}
			if (optval && sin == 0) {
				ia = (struct in6_ifaddr *)hash_enum(
				  &hashinfo_in6addr,
				  ifp_to_localaddr_enum, HTF_INET,
				  (caddr_t)0, (caddr_t)ifp, (caddr_t)0);
				if (ia == 0)
					IFP_TO_IA6(ifp, ia);
				if (ia == 0) {
					error = ENETDOWN;
					break;
				}
				goto sendif_done;
			}
			if (optval) {
				ia = (struct in6_ifaddr *)hash_lookup(
				  &hashinfo_in6addr, in6addr_ifnetaddr_match,
				  (caddr_t)&sin->sin6_addr, (caddr_t)ifp,
				  (caddr_t)0);
				goto sendif_done;
			}
			if (sin == 0)
				goto sendif_done;
			ia = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
			  in6addr_match, (caddr_t)&(sin->sin6_addr),
			  (caddr_t)0, (caddr_t)0);
		sendif_done:
			if (ia == 0) {
				error = EADDRNOTAVAIL;
				break;
			}
			if (inp->inp_moptions) {
				struct ip_moptions *imo;

				ifp = ia->ia_ifp;
				if ((ifp->if_flags & IFF_MULTICAST) == 0) {
					error = EADDRNOTAVAIL;
					break;
				}
				imo = mtod(inp->inp_moptions,
				  struct ip_moptions *);;
				imo->imo_multicast_ifp = ifp;
			}
#ifdef INTERFACES_ARE_SELECTABLE
			error = in6_setifa(inp, &ia->ia_ifa);
#else
#ifdef IP6PRINTFS
			printf("set inp_ifa %p\n", ia);
#endif
#endif

			break;

		case IPV6_PKTINFO:
			if (m == NULL ||
			    m->m_len != sizeof(struct in6_pktinfo)) {
				error = EINVAL;
				break;
			}
			if ((inp->inp_flags & INP_COMPATV6) == 0) {
				error = ENOPROTOOPT;
				break;
			}
			inp->inp_flags &= ~INP_COMPATV4;
			pi = mtod(m, struct in6_pktinfo *);
			optval = pi->ipi6_ifindex;
			if (!IS_ANYADDR6(pi->ipi6_addr)) {
				bhv_desc_t bd;

				m->m_len = sizeof(*sin);
				sin = mtod(m, struct sockaddr_in6 *);
				COPY_ADDR6(pi->ipi6_addr, sin->sin6_addr);
				sin->sin6_family = AF_INET6;
				sin->sin6_len = sizeof(*sin);
				sin->sin6_port = 0;
				sin->sin6_flowinfo = 0;

			        bhv_desc_init(&bd, so, 0, 0);
				sobind(&bd, m);
				if (error)
					break;
			}
			if (optval == 0) {
#ifdef INTERFACES_ARE_SELECTABLE
				if (inp->inp_ifa)
					ifafree(inp->inp_ifa);
				inp->inp_ifa = 0;
#endif
				break;
			}
			goto sendif1;

		default:
			error = ENOPROTOOPT;
			break;
		}
		if (m)
			(void)m_free(m);
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case IP_OPTIONS:
			if ((inp->inp_flags & INP_COMPATV6) == 0) {
				error = ENOPROTOOPT;
				break;
			}
			inp->inp_flags &= ~INP_COMPATV4;
			error = ip6_getoptions(inp->inp_options, mp, 0);
			break;

		case IPV6_PKTINFO:
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(struct in6_pktinfo);
			pi = mtod(m, struct in6_pktinfo *);
			COPY_ADDR6(inp->inp_laddr6, pi->ipi6_addr);
			if (inp->inp_rcvif != NULL)
				pi->ipi6_ifindex = inp->inp_rcvif->if_index;
			else
				pi->ipi6_ifindex = 0;
			break;

		case IP_TOS:
		case IPV6_UNICAST_HOPS:
		case IP_RECVOPTS:
		case IP_RECVRETOPTS:
		case IP_RECVDSTADDR:
		case IPV6_RECVPKTINFO:
		case IPV6_RECVHOPS:
			return(ip_ctloutput(op, so, level, optname, mp));

		case IPV6_MULTICAST_IF:
		case IPV6_MULTICAST_HOPS:
		case IPV6_MULTICAST_LOOP:
		case IPV6_ADD_MEMBERSHIP:
		case IPV6_DROP_MEMBERSHIP:
			if ((inp->inp_flags & INP_COMPATV6) == 0) {
				error = ENOPROTOOPT;
				break;
			}
			inp->inp_flags &= ~INP_COMPATV4;
			error = ip6_getmoptions(optname, inp->inp_moptions, mp);
			break;

		case IPV6_SENDIF:
#ifdef INTERFACES_ARE_SELECTABLE
			if (inp->inp_ifa == 0) {
#endif
				*mp = 0;
				break;
#ifdef INTERFACES_ARE_SELECTABLE
			}
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(struct sockaddr_in6 *);
			sin = mtod(m, struct sockaddr_in6 *);
			bcopy((caddr_t)inp->inp_ifa->ifa_addr,
			      (caddr_t)sin, m->m_len);
			sin->sin6_port = inp->inp_ifa->ifa_ifp->if_index;
			break;
#endif
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}

/*
 * IPv6 ancillary data (aka msg_control) output processing
 */
int
ip6_setcontrol(inp, control)
	register struct inpcb *inp;
	struct mbuf *control;
{
	register struct cmsghdr *cp;
	struct mbuf *temp = 0;
	struct in6_pktinfo pi;
	struct sockaddr_in6 *sin = 0;
	struct ifnet *ifp;
	struct in6_ifaddr *ia;
	struct ip_moptions *imo;
	int optval, len, error = 0;
	struct mbuf *mopt;

	MGET(mopt, M_WAIT, MT_IPMOPTS);
	if (mopt == NULL)
		return (ENOBUFS);
	imo = mtod(mopt, struct ip_moptions *);

	if (inp->inp_moptions)
		bcopy(inp->inp_moptions, imo, sizeof(*imo));
	else {
		imo->imo_multicast_ifp = NULL;
		imo->imo_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
		imo->imo_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
	}

	while (control->m_len > 0) {
	    cp = mtod(control, struct cmsghdr *);
	    /* check alignment */
	    len = (cp->cmsg_len + sizeof(int) - 1) & ~(sizeof(int) - 1);
	    if (len > control->m_len) {
		error = EINVAL;
		goto done;
	    }
	    switch (cp->cmsg_type) {
	      case IP_OPTIONS:
		goto unimpl;

	      case IP_TOS:
		goto unimpl;

	      case IPV6_UNICAST_HOPS:
		if ((cp->cmsg_level != IPPROTO_IP) &&
		    (cp->cmsg_level != IPPROTO_IPV6)) {
		    error = EINVAL;
		    goto done;
		}
		if (cp->cmsg_len != sizeof(*cp) + sizeof(int)) {
		    error = EINVAL;
		    goto done;
		}
		bcopy(CMSG_DATA(cp), &optval, sizeof(int));
		if (optval == -1)
		    optval = IP_TTL;
		if ((optval < 0) || (optval > MAXTTL)) {
		    error = EINVAL;
		    goto done;
		}
		inp->inp_ttl = optval;
		break;

	      case IPV6_PKTINFO:
		if (cp->cmsg_level != IPPROTO_IPV6) {
		    error = EINVAL;
		    goto done;
		}
		if (cp->cmsg_len != sizeof(*cp) + sizeof(pi)) {
		    error = EINVAL;
		    goto done;
		}
		if ((inp->inp_flags & INP_COMPATV6) == 0) {
		    error = ENOPROTOOPT;
		    goto done;
		}
		inp->inp_flags &= ~INP_COMPATV4;
		bcopy(CMSG_DATA(cp), &pi, sizeof(pi));
		optval = pi.ipi6_ifindex;

		if (!IS_ANYADDR6(pi.ipi6_addr)) {
		    bhv_desc_t bd;

		    temp = m_get(M_DONTWAIT, MT_SONAME);
		    if (temp == 0) {
			error = ENOBUFS;
			goto done;
		    }
		    temp->m_len = sizeof(*sin);
		    MH_ALIGN(temp, sizeof(*sin));
		    bzero(mtod(temp, caddr_t), sizeof(*sin));
		    sin = mtod(temp, struct sockaddr_in6 *);
		    sin->sin6_len = sizeof(*sin);
		    sin->sin6_family = AF_INET6;
		    COPY_ADDR6(pi.ipi6_addr, sin->sin6_addr);

		    bhv_desc_init(&bd, inp->inp_socket, 0, 0);
		    sobind(&bd, temp);
		    if (error)
			goto donepi;
		}

		if (optval == 0) {
#ifdef INTERFACES_ARE_SELECTABLE
		    /* will be restored: don't free */
		    inp->inp_ifa = 0;
#endif
		    goto donepi;
		}
		for (ifp = ifnet; ifp; ifp = ifp->if_next)
		    if (ifp->if_index == optval)
			break;
		if (ifp == 0) {
		    error = ENXIO;
		    goto donepi;
		}
		if (sin == 0) {
		    ia = (struct in6_ifaddr *)hash_enum(
		      &hashinfo_in6addr,
		      ifp_to_localaddr_enum, HTF_INET,
		      (caddr_t)0, (caddr_t)ifp, (caddr_t)0);
		} else {
		    ia = (struct in6_ifaddr *)hash_lookup(
		      &hashinfo_in6addr, in6addr_ifnetaddr_match,
		      (caddr_t)&sin->sin6_addr, (caddr_t)ifp,
		      (caddr_t)0);
		}
		if (ia == 0)
		    IFP_TO_IA6(ifp, ia);
		if (ia == 0) {
		    error = sin == 0 ? ENETDOWN : EADDRNOTAVAIL;
		    goto donepi;
		}
		if (inp->inp_moptions) {
		    if ((ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			goto donepi;
		    }
		}
		imo->imo_multicast_ifp = ifp;
		inp->inp_moptions = mopt;
		imo = NULL;
#ifdef INTERFACES_ARE_SELECTABLE
		error = in6_setifa(inp, &ia->ia_ifa);
#else
#ifdef IP6PRINTFS
		printf("set inp_ifa %p\n", ia);
#endif
#endif
	      donepi:
		if (temp)
		    (void)m_free(temp);
		if (error)
		    goto done;
		break;

	      case IPV6_MULTICAST_HOPS:
		if ((cp->cmsg_level != IPPROTO_IP) &&
		    (cp->cmsg_level != IPPROTO_IPV6)) {
		    error = EINVAL;
		    goto done;
		}
		if (cp->cmsg_len != sizeof(*cp) + sizeof(int)) {
		    error = EINVAL;
		    goto done;
		}
		bcopy(CMSG_DATA(cp), &optval, sizeof(int));
		if (optval == -1)
		    optval = IP_DEFAULT_MULTICAST_TTL;
		if ((optval < 0) || (optval > MAXTTL)) {
		    error = EINVAL;
		    goto done;
		}
		imo->imo_multicast_ttl = optval;
		inp->inp_moptions = mopt;
		imo = NULL;
		break;

	      default:
	      unimpl:
		error = ENOPROTOOPT;
		goto done;
	    }
	    control->m_len -= len;
	    control->m_off += len;
	}
    done:
	if (imo)
		m_free(mopt);
	return (error);
}


/*
 * Set up IPv6 options in pcb for insertion in output packets.
 * Store in a mbuf chain with pointer in pcbopts.
 */
/*ARGSUSED*/
int
ip6_setoptions(
	register struct mbuf **pcbopts,
	register struct mbuf *m0,
	struct inpcb *inp)
{
	register u_int len = 0;
	register struct mbuf *m;
	register u_char *cp, *end, nh;
	int error = 0;

	/* turn off any old options */
	if (*pcbopts)
		m_freem(*pcbopts);
	*pcbopts = 0;
	if (m0 == (struct mbuf *)0 || m0->m_len == 0) {
		/*
		 * Only turning off any previous options.
		 */
		if (m0)
			m_free(m0);
		return (0);
	}

	if ((mtod(m0, u_int32_t) % 4) || (m0->m_len % 4)) {
		error = EINVAL;
		goto done;
	}
	cp = mtod(m0, u_char *);
	end = cp + (u_int)m0->m_len;
	while (cp < end) {
		switch (nh = *cp) {
		case IP6_NHDR_HOP:
			len = (((struct ipv6_h2hhdr *)cp)->ih6_hlen * 8) +
				sizeof(struct ipv6_h2hhdr);
			if ((len % 8) || (cp + len > end)) {
				error = EINVAL;
				goto done;
			}
			*cp = 0;
			break;

		case IP6_NHDR_DOPT:
			len = (((struct ipv6_dopthdr *)cp)->io6_hlen * 8) +
				sizeof(struct ipv6_dopthdr);
			if ((len % 8) || (cp + len > end)) {
				error = EINVAL;
				goto done;
			}
			*cp = 0;
			break;

		case IP6_NHDR_RT: {
			register struct ipv6_rthdr *hp;

			hp = (struct ipv6_rthdr *)cp;
			hp->ir6_sglt = hp->ir6_hlen >> 1;
			if ((hp->ir6_type != IP6_LSRRT) ||
			    ((hp->ir6_hlen & 1) != 0) ||
			    (hp->ir6_sglt > IP6_RT_MAX) ||
			    (ntohl(hp->ir6_slmsk) & ~IP6_RT_SLMSK)) {
				error = EINVAL;
				goto done;
			}
			len = (hp->ir6_hlen * 8) + sizeof(struct ipv6_rthdr);
			len += sizeof(struct in6_addr);
			if (cp + len > end) {
				error = EINVAL;
				goto done;
			}
			*cp = 0;
			break;
			}

		case IP6_NHDR_AUTH:
			len = (((struct ipv6_authhdr *)cp)->ah6_hlen * 8) +
				sizeof(struct ipv6_authhdr);
			if ((len % 8) || (cp + len > end)) {
				error = EINVAL;
				goto done;
			}
			*cp = 0;
			break;

		case IP6_NHDR_ESP: {
			u_int32_t spi;

			/* first word is nh|len|ivlen|xpad */
			len = cp[1];
			if ((cp + len > end) ||
			    (len < 2 * sizeof(spi) + cp[2])) {
				error = EINVAL;
				goto done;
			}
			/* copy SPI & ivlen at their place */
			spi = *(u_int32_t *)&cp[len - sizeof(u_int32_t)];
			cp[len - 1] = cp[2];
			*(u_int32_t *)cp = spi;
			}
			break;

		default:
			error = EINVAL;
			goto done;
		}

		/*
		 * Allocate mbuf.
		 */
		/*
		 * Add in extra space needed by kernel.  This part is not
		 * sent over the wire.
		 */
		len += OPT6_HDRSIZE;
		if (len > MCLBYTES) {
			error = EMSGSIZE;
			goto done;
		}
		m = m_vget(M_WAIT, len, MT_SOOPTS);
		if (m == 0) {
			error = ENOBUFS;
			goto done;
		}
		if (MLEN >= len)
			MH_ALIGN(m, len);

		/*
		 * Chain mbuf into pcbopts.
		 */
		m->m_next = *pcbopts;
		*pcbopts = m;

		/*
		 * Save option.
		 */
		OPT6_TYPE(m) = nh;
		m->m_len = len;
		bcopy((caddr_t)cp, OPT6_DATA(m, caddr_t), len - OPT6_HDRSIZE);
		if (OPT6_TYPE(m) == IP6_NHDR_RT) {
			m->m_len -= sizeof(struct in6_addr);
		}
		cp += len;

	}
done:
	m_free(m0);
	return (error);
}

/*
 * Get IPv6 options from pcb for user getsockopt() et IP_RECVOPTS.
 */
int
ip6_getoptions(pcbopts, mp, control)
	register struct mbuf *pcbopts;
	register struct mbuf **mp;
	int control;
{
	register u_int len = sizeof(struct in6_addr);
	register struct mbuf *m;
	register caddr_t cp;
	int error = 0, oplen;

	if (pcbopts) {
		for (m = pcbopts, oplen = 0; m; m = m->m_next)
			oplen += OPT6_LEN(m);
		len += oplen;
	}
	if (control)
		len += sizeof(struct cmsghdr);
	if (len > MCLBYTES)
		return (EMSGSIZE);
	
	m = m_vget(M_WAIT, len, MT_SOOPTS);
	if (m == 0) {
		error = ENOBUFS;
		goto done;
	}
	*mp = m;
	cp = mtod(m, caddr_t) + len;
	len -= sizeof(struct in6_addr);
	if (control)
		len -= sizeof(struct cmsghdr);
	m->m_len = len;
	while (pcbopts) {
		register int optlen = OPT6_LEN(pcbopts);

#ifdef IP6PRINTFS
		{
		struct mbuf *o;
		for (o = pcbopts, oplen = 0; o; o = o->m_next)
			oplen += OPT6_LEN(o);
		if (oplen != len)
			panic("ip6_getoptions (hdr)");
		}
#endif
		if (OPT6_TYPE(pcbopts) == IP6_NHDR_RT) {
			optlen += sizeof(struct in6_addr);
			len += sizeof(struct in6_addr);
			m->m_len += sizeof(struct in6_addr);
		}
		if (optlen > len)
			panic("ip6_getoptions (big)");
		bcopy(mtod(pcbopts, caddr_t), (caddr_t)cp - optlen, optlen);
		cp -= optlen;
		*cp = OPT6_TYPE(pcbopts);
		len -= optlen;
		pcbopts = pcbopts->m_next;
	}
#ifdef IP6PRINTFS
	if (len != 0)
		panic("ip6_getoptions (lost)");
#endif

done:
	if (error && m)
		m_free(m);
	return (error);
}

/*
 * Set the IPv6 multicast options in response to user setsockopt().
 */
/*ARGSUSED*/
int
ip6_setmoptions(mopts, optname, m)
	struct mbuf **mopts;
	int optname;
	struct mbuf *m;
{
	register int error = 0;
	u_int loop;
	struct in6_addr addr;
	unsigned int index;
	register struct ipv6_mreq *mreq;
	register struct ifnet *ifp = 0;
	register struct ip_moptions *imo;
	struct ip_membership *ipm, **pipm;
	union route_6 iproute6;
#define ro	iproute6.route
	register struct sockaddr_in6 *dst;

#ifdef INTERFACES_ARE_SELECTABLE
	/*
	 * XXX6 inp used to be passed in so we'll need to do something
	 * for inp_ifa if we end up using it.
	 */
	if (inp->inp_ifa) {
		ifp = inp->inp_ifa->ifa_ifp;
		if ((ifp->if_flags & IFF_MULTICAST) == 0) {
			ifafree(inp->inp_ifa);
			inp->inp_ifa = NULL;
			ifp = NULL;
		}
	}
#endif
	if (*mopts == NULL) {
		/*
		 * No multicast option buffer attached to the pcb;
		 * allocate one and initialize to default values.
		 */
		MGET(*mopts, M_WAIT, MT_IPMOPTS);
		if (*mopts == NULL)
			return (ENOBUFS);
		imo = mtod(*mopts, struct ip_moptions *);
		imo->imo_multicast_ifp = ifp;
		imo->imo_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
		imo->imo_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
		imo->imo_membership = NULL;
	}
	imo = mtod(*mopts, struct ip_moptions *);

	switch (optname) {

	case IPV6_MULTICAST_IF:
		/*
		 * Select the interface for outgoing multicast packets.
		 */
		if (m == NULL) {
			error = EINVAL;
			break;
		}
		if (m->m_len == sizeof(unsigned int)) {
		    if (ifp) {
			error = EADDRINUSE;
			break;
		    }
		    index = *mtod(m, unsigned int *);
		    /*
		     * zero index is used to remove a previous selection.
		     * When no interface is selected, a default one is
		     * chosen every time a multicast packet is sent.
		     */
		    if (index == 0) {
			imo->imo_multicast_ifp = NULL;
			break;
		    }
		    /*
		     * The selected interface is identified by its index.
		     * Find the interface and confirm that it supports
		     * IPv6 and multicasting.
		     */
		    for (ifp = ifnet; ifp && ifp->if_index != index;
		      ifp = ifp->if_next)
			continue;
		    if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		    }
		    imo->imo_multicast_ifp = ifp;
		} else if (m->m_len == sizeof(struct in6_addr)) {
		    if (ifp) {
			error = EADDRINUSE;
			break;
		    }
		    COPY_ADDR6(*mtod(m, struct in6_addr *), addr);
		    /*
		     * unspec address is used to remove a previous selection.
		     * When no interface is selected, a default one is
		     * chosen every time a multicast packet is sent.
		     */
		    if (IS_ANYADDR6(addr)) {
			imo->imo_multicast_ifp = NULL;
			break;
		    }
		    /*
		     * The selected interface is identified by its local
		     * IPv6 address.  Find the interface and confirm that
		     * it supports multicasting.
		     */
		    IN6ADDR_TO_IFP(addr, ifp);
		    if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		    }
		    imo->imo_multicast_ifp = ifp;
		} else {
			error = EINVAL;
			break;
		}
		break;

	case IPV6_MULTICAST_HOPS:
		/*
		 * Set the IP time-to-live for outgoing multicast packets.
		 */
		if (m == NULL ||
		    (m->m_len != 1 && m->m_len != sizeof(int))) {
			error = EINVAL;
			break;
		}
		if (m->m_len == sizeof(int)) {
			int optval = *(mtod(m, int *));

			if (optval == -1)
				optval = IP_DEFAULT_MULTICAST_TTL;
			if ((optval < 0) || (optval > MAXTTL)) {
				error = EINVAL;
				break;
			}
			imo->imo_multicast_ttl = optval;
		} else
			imo->imo_multicast_ttl = *(mtod(m, u_char *));
		break;

	case IPV6_MULTICAST_LOOP:
		/*
		 * Set the loopback flag for outgoing multicast packets.
		 * Must be zero or one.
		 */
		if (m == NULL ||
		    (m->m_len != 1 && m->m_len != sizeof(u_int))) {
			error = EINVAL;
			break;
		}
		if (m->m_len == sizeof(u_int))
			loop = *(mtod(m, u_int *));
		else
			loop = *(mtod(m, u_char *));
		if (loop > 1) {
			error = EINVAL;
			break;
		}
		imo->imo_multicast_loop = loop;
		break;

	case IPV6_ADD_MEMBERSHIP:
		/*
		 * Add a multicast group membership.
		 * Group must be a valid IPv6 multicast address.
		 */
		if (m == NULL) {
			error = EINVAL;
			break;
		}
		if (m->m_len == sizeof(struct ipv6_mreq)) {
		    mreq = mtod(m, struct ipv6_mreq *);
		    if (!IS_MULTIADDR6(mreq->ipv6mr_multiaddr)) {
			error = EINVAL;
			break;
		    }
		    /*
		     * If interface index is zero
		     * use the interface of the route to the
		     * given multicast address.
		     */
		    if (mreq->ipv6mr_interface == 0) {
			bzero((caddr_t)&ro, sizeof(iproute6));
			dst = satosin6(&ro.ro_dst);
			dst->sin6_len = sizeof(*dst);
			dst->sin6_family = AF_INET6;
			COPY_ADDR6(mreq->ipv6mr_multiaddr, dst->sin6_addr);
			rtalloc(&ro);
			if (ro.ro_rt == NULL) {
			    error = EADDRNOTAVAIL;
			    break;
			}
			ifp = ro.ro_rt->rt_ifp;
			rtfree(ro.ro_rt);
		    }
		    else {
		        for (ifp = ifnet; ifp &&
			  ifp->if_index != mreq->ipv6mr_interface;
		          ifp = ifp->if_next)
			    continue;
		    }
		    /*
		     * See if we found an interface, and confirm that it
		     * supports multicast.
		     */
		    if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EADDRNOTAVAIL;
			break;
		    }
		    /*
		     * See if the membership already exists or if all the
		     * membership slots are full.
		     */
		    for (ipm = imo->imo_membership; ipm; ipm = ipm->next) {
			struct in6_multi *inm =
			    (struct in6_multi *)ipm->ipm_membership;
			if (inm->inm6_ifp == ifp && SAME_ADDR6(inm->inm6_addr,
			    mreq->ipv6mr_multiaddr))
				break;
		    }
		    if (ipm) {
			error = EADDRINUSE;
			break;
		    }
		    ipm = (struct ip_membership *)
		        kmem_zone_alloc(ipmember_zone, KM_NOSLEEP);
		    if (ipm == NULL) {
			error = ENOBUFS;
			break;
		    }
		    /*
		     * Everything looks good; add a new record to the multicast
		     * address list for the given interface.
		     */
		    IFNET_LOCK(ifp);
		    ipm->ipm_membership =
		      (struct in_multi *)in6_addmulti(&mreq->ipv6mr_multiaddr,
		      ifp);
		    if (ipm->ipm_membership == NULL) {
			error = ENOBUFS;
			IFNET_UNLOCK(ifp);
			kmem_zone_free(ipmember_zone, ipm);
			break;
		    }
		    IFNET_UNLOCK(ifp);
		    ipm->next = imo->imo_membership;
		    imo->imo_membership = ipm;
		} else
		    error = EINVAL;
		break;

	case IPV6_DROP_MEMBERSHIP:
		/*
		 * Drop a multicast group membership.
		 * Group must be a valid IP multicast address.
		 */
		if (m == NULL) {
			error = EINVAL;
			break;
		}
		if (m->m_len == sizeof(struct ipv6_mreq)) {
		    mreq = mtod(m, struct ipv6_mreq *);
		    if (!IS_MULTIADDR6(mreq->ipv6mr_multiaddr)) {
			error = EINVAL;
			break;
		    }

		    /*
		     * If interface index is not zero, get a pointer
		     * to its ifnet structure.
		     */
		    if (mreq->ipv6mr_interface == 0)
			ifp = NULL;
		    else {
		        for (ifp = ifnet; ifp &&
			  ifp->if_index != mreq->ipv6mr_interface;
		          ifp = ifp->if_next)
			    continue;
			if (ifp == NULL) {
			    error = EADDRNOTAVAIL;
			    break;
			}
		    }
		    /*
		     * Find the membership in the membership array.
		     */
		    for (ipm = imo->imo_membership; ipm; ipm = ipm->next) {
			register struct in6_multi *inm =
			    (struct in6_multi *)ipm->ipm_membership;
			if ((ifp == NULL || inm->inm6_ifp == ifp) &&
			    SAME_ADDR6(inm->inm6_addr, mreq->ipv6mr_multiaddr))
				break;
		    }
		    if (ipm == NULL) {
			error = EADDRNOTAVAIL;
			break;
		    }
		    /*
		     * Give up the multicast address record to which the
		     * membership points.
		     */
		    in6_delmulti((struct in6_multi *)ipm->ipm_membership);
		    /*
		     * Remove the gap in the membership array.
		     */
		    for (pipm = &(imo->imo_membership); *pipm;
			pipm = &(*pipm)->next) {
			    if (*pipm == ipm) {
				    *pipm = ipm->next;
				    kmem_zone_free(ipmember_zone, ipm);
				    break;
			    }
		    }
		}
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	/*
	 * If all options have default values, no need to keep the mbuf.
	 */
	if (imo->imo_multicast_ifp == NULL &&
	    imo->imo_multicast_ttl == IP_DEFAULT_MULTICAST_TTL &&
	    imo->imo_multicast_loop == IP_DEFAULT_MULTICAST_LOOP &&
	    imo->imo_membership == NULL) {
		m_free(*mopts);
		*mopts = NULL;
	}

	return (error);
}

/*
 * Return the IPv6 multicast options in response to user getsockopt().
 */
int
ip6_getmoptions(optname, mopts, mp)
	int optname;
	struct mbuf *mopts;
	register struct mbuf **mp;
{
	u_int *opval;
	register struct ip_moptions *imo;

	*mp = m_get(M_WAIT, MT_SOOPTS);

	imo = (mopts == NULL) ? NULL : mtod(mopts, struct ip_moptions *);
	switch (optname) {

	case IPV6_MULTICAST_IF:
		opval = mtod(*mp, u_int *);
		(*mp)->m_len = sizeof(u_int);
		if (imo == NULL || imo->imo_multicast_ifp == NULL)
			*opval = 0;
		else
			*opval = imo->imo_multicast_ifp->if_index;
		return (0);

	case IPV6_MULTICAST_HOPS:
		opval = mtod(*mp, u_int *);
		(*mp)->m_len = sizeof(int);
		*opval = (imo == NULL) ? IP_DEFAULT_MULTICAST_TTL
				     : imo->imo_multicast_ttl;
		return (0);

	case IPV6_MULTICAST_LOOP:
		opval = mtod(*mp, u_int *);
		(*mp)->m_len = sizeof(u_int);
		*opval = (imo == NULL) ? IP_DEFAULT_MULTICAST_LOOP
				      : imo->imo_multicast_loop;
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

/*
 * Discard the IPv6 multicast options.
 */
void
ip6_freemoptions(struct mbuf *mopts)
{
	register struct ip_moptions *imo;
	struct ip_membership *ipm, *nipm;

	if (mopts != NULL) {
		imo = mtod(mopts, struct ip_moptions *);
		for (ipm = imo->imo_membership; ipm; ipm = nipm) {
			nipm = ipm->next;
			in6_delmulti((struct in6_multi *)ipm->ipm_membership);
			kmem_zone_free(ipmember_zone, ipm);
		}
		m_free(mopts);
	}
}

/*
 * Set the IPv6 Security options in response to user setsockopt().
 */
int
ip6_setsoptions(so, optname, m)
	struct socket *so;
	int optname;
	struct mbuf *m;
{
	register int error = 0;
	register struct sockaddr_ipsec *addr;
	register struct ipsec_req *req;
	register struct inpcb *inp = sotoinpcb(so);

	ASSERT(SOCKET_ISLOCKED(so));
#ifdef IP6PRINTFS
	if (ip6printfs & D6_KEY)
		printf("ip6_setsoptions %d\n", optname);
#endif
	switch (optname) {
	case IPSEC_WANTAUTH:
		/*
		 * This socket wants authentic packets.
		 */
		if (m == NULL || m->m_len != sizeof(int)) {
			error = EINVAL;
			break;
		}
		if (*mtod(m, int *))
			inp->inp_flags |= INP_NEEDAUTH;
		else
			inp->inp_flags &= ~INP_NEEDAUTH;
		break;

	case IPSEC_WANTCRYPT:
		/*
		 * This socket wants confidential packets.
		 */
		if (m == NULL || m->m_len != sizeof(int)) {
			error = EINVAL;
			break;
		}
		if (*mtod(m, int *))
			inp->inp_flags |= INP_NEEDCRYPT;
		else
			inp->inp_flags &= ~INP_NEEDCRYPT;
		break;

	case IPSEC_CREATE:
		/*
		 * Create a new key entry.
		 */
		if (m == NULL || m->m_len < sizeof(struct ipsec_req)) {
			error = EINVAL;
			break;
		}
		req = mtod(m, struct ipsec_req *);
		if (m->m_len < sizeof(*req) + req->ipr_klen) {
			error = EINVAL;
			break;
		}
		if ((so->so_state & SS_PRIV) == 0) {
			error = EPERM;
			break;
		}
		error = ipsec_create(req);
		break;

	case IPSEC_CHANGE:
		/*
		 * Change a key entry.
		 */
		if (m == NULL || m->m_len < sizeof(struct ipsec_req)) {
			error = EINVAL;
			break;
		}
		req = mtod(m, struct ipsec_req *);
		if (m->m_len < sizeof(*req) + req->ipr_klen) {
			error = EINVAL;
			break;
		}
		if ((so->so_state & SS_PRIV) == 0) {
			error = EPERM;
			break;
		}
		error = ipsec_change(req);
		break;

	case IPSEC_DELETE:
		/*
		 * Delete a key entry.
		 */
		if (m == NULL || m->m_len != sizeof(struct sockaddr_ipsec)) {
			error = EINVAL;
			break;
		}
		addr = mtod(m, struct sockaddr_ipsec *);
		if ((so->so_state & SS_PRIV) == 0) {
			error = EPERM;
			break;
		}
		error = ipsec_delete(addr);
		break;

	case IPSEC_ATTACH:
	case IPSEC_ATTACHX:
		/*
		 * Attach a key entry to a socket (shared/exclusive use).
		 */
		if (m == NULL || m->m_len != sizeof(struct sockaddr_ipsec)) {
			error = EINVAL;
			break;
		}
		addr = mtod(m, struct sockaddr_ipsec *);
		error = ipsec_attach(inp, addr,
			    optname == IPSEC_ATTACHX ? IPSEC_EXCLUSIVE : 0);
		break;

	case IPSEC_ATTACHW:
		/*
		 * Attach the "wildcard" key entry to a socket.
		 */
		inp = sotoinpcb(so);
		error = ipsec_attach(inp, NULL, IPSEC_WILDCARD);
		break;

	case IPSEC_DETACH:
	case IPSEC_DETACHX:
		/*
		 * Detach a key entry from a socket (shared/exclusive use).
		 */
		if (m == NULL || m->m_len != sizeof(struct sockaddr_ipsec)) {
			error = EINVAL;
			break;
		}
		addr = mtod(m, struct sockaddr_ipsec *);
		error = ipsec_detach(inp, addr,
			    optname == IPSEC_DETACHX ? IPSEC_EXCLUSIVE : 0);
		break;

	case IPSEC_DETACHW:
		/*
		 * Detach the "wildcard" key entry from a socket.
		 */
		error = ipsec_detach(inp, NULL, IPSEC_WILDCARD);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

/*
 * Return the IPv6 Security options in response to user getsockopt().
 */
int
ip6_getsoptions(
	struct inpcb *inp,
	int optname,
	struct mbuf **mp)
{
	int *flags;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_KEY)
		printf("ip6_getsoptions %d\n", optname);
#endif
	*mp = m_get(M_WAIT, MT_SOOPTS);

	switch (optname) {
	case IPSEC_WANTAUTH:
		flags = mtod(*mp, int *);
		(*mp)->m_len = sizeof(int);
		*flags = (inp->inp_flags & INP_NEEDAUTH) == INP_NEEDAUTH;
		return (0);

	case IPSEC_WANTCRYPT:
		flags = mtod(*mp, int *);
		(*mp)->m_len = sizeof(int);
		*flags = (inp->inp_flags & INP_NEEDCRYPT) == INP_NEEDCRYPT;
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

/*
 * Discard the IPv6 security options.
 */
void
ip6_freesoptions(inp)
	register struct inpcb *inp;
{
	register struct ip_seclist *isl;

	while ((isl = inp->inp_soptions.lh_first) != 0) {
		if ((isl)->isl_list.le_next != NULL)
			(isl)->isl_list.le_next->isl_list.le_prev =
			  (isl)->isl_list.le_prev;
		*(isl)->isl_list.le_prev = (isl)->isl_list.le_next; 

		if (isl->isl_iep)
			isl->isl_iep->ieh_refcnt--;
		kmem_free(isl, sizeof (*isl));
	}
}

/*
 * Routine called from ip6_output() to loop back a copy of an IPv6 multicast
 * packet to the input queue of a specified interface.  Note that this
 * calls the output routine of the loopback "driver", but with an interface
 * pointer that might NOT be a loopback interface -- evil, but easier than
 * replicating that code here.
 */
static void
ip6_mloopback(ifp, m, opts, dst)
	struct ifnet *ifp;
	register struct mbuf *m, *opts;
	register struct sockaddr_in6 *dst;
{
	register struct ipv6 *ip;
	struct mbuf *copym;
	int error = 0;

	copym = m_copy(m, 0, M_COPYALL);
	if (copym != NULL) {
		while (opts) {
			copym = ip6_insertoption(copym, opts, NULL, 0, &error);
			if (error != 0) {
				IP6STAT(ip6s_odropped);
				m_freem(copym);
				return;
			}
			opts = opts->m_next;
		}
		/*
		 * We don't bother to fragment if the IPv6 length is greater
		 * than the interface's MTU.  Can this possibly matter?
		 */
		ip = mtod(copym, struct ipv6 *);
		ip->ip6_len = htons((u_int16_t)ip->ip6_len);
		IFNET_UPPERLOCK(ifp);
		(void) looutput(ifp, copym, sin6tosa(dst), NULL);
		IFNET_UPPERUNLOCK(ifp);
	} else
		IP6STAT(ip6s_odropped);
}
#endif /* INET6 */
