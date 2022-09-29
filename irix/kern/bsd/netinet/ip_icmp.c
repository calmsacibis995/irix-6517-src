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
 *	@(#)ip_icmp.c	8.2 (Berkeley) 1/4/94
 */

#ident "$Revision: 4.25 $"

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/time.h"
#include "sys/errno.h"
#include "sys/sesmgr.h"

#include "net/raw.h"
#include "net/route.h"
#include "net/if.h"

#include "if_ether.h"
#include "in.h"
#include "in_systm.h"
#include "in_var.h"
#include "ip.h"
#include "ip_icmp.h"
#include "ip_var.h"
#include "icmp_var.h"

#include "bsd/sys/tcpipstats.h"

#define	satosin(sa)	((struct sockaddr_in *)(sa))

/*
 * Module global variables
 */
int icmpmaskrepl = 0;

/*
 * External global variables
 */
extern int icmp_dropredirects;

/*
 * Forward referenced procedures
 */
void icmp_reflect(struct mbuf *,  struct ifnet *, struct ipsec *);
void icmp_send(struct mbuf *, struct mbuf *, struct ipsec *);

/*
 * ICMP routines: error generation, receive packet processing, and
 * routines to turnaround packets back to the originator, and
 * host table maintenance routines.
 */
#ifdef ICMPPRINTFS
extern int icmpprintfs;
#endif

extern struct sockaddr_in in_zeroaddr;
extern struct protosw inetsw[];
extern u_char ip_protox[];
extern int in_ifaddr_count;

/*
 * Generate an error packet of type error in response to bad packet ip.
 */
void
icmp_error(struct mbuf *n,
	int type,
	int code,
	struct ifnet *ifp,
	struct in_addr dest,
	struct ifnet *destifp,
	struct ipsec *ipsec)
{
	register struct ip *oip = mtod(n, struct ip *), *nip;
	register unsigned oiplen = oip->ip_hl << 2;
	register struct icmp *icp;
	register struct mbuf *m;
	unsigned icmplen;

#ifdef ICMPPRINTFS
	if (icmpprintfs)
		printf("icmp_error(%x, %d, %d)\n", oip, type, code);
#endif
	if (type != ICMP_REDIRECT)
		ICMPSTAT(icps_error);
	/*
	 * Don't send error if not the first fragment of message.
	 * Don't error if the old packet protocol was ICMP
	 * error message, only known informational types.
	 */
	if (oip->ip_off &~ (IP_MF|IP_DF))
		goto freeit;
	if (oip->ip_p == IPPROTO_ICMP && type != ICMP_REDIRECT &&
	  n->m_len >= oiplen + ICMP_MINLEN &&
	  !ICMP_INFOTYPE(((struct icmp *)((caddr_t)oip + oiplen))->icmp_type)) {
		ICMPSTAT(icps_oldicmp);
		goto freeit;
	}
	/* Don't send error in response to a multicast or broadcast packet */
	if (n->m_flags & (M_BCAST|M_MCAST))
		goto freeit;
	if (in_broadcast(oip->ip_dst, ifp) ||
	    IN_MULTICAST(ntohl(oip->ip_dst.s_addr)) ||
	    IN_EXPERIMENTAL(ntohl(oip->ip_dst.s_addr)))
		goto freeit;

	/*
	 * First, formulate icmp message
	 */
	m = m_get(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		goto freeit;
	icmplen = oiplen + MIN(8, oip->ip_len);
	m->m_len = icmplen + ICMP_MINLEN;
	m->m_off = (MMAXOFF - m->m_len) & ~(sizeof(long) - 1);
	icp = mtod(m, struct icmp *);
	if ((u_int)type > ICMP_MAXTYPE)
		panic("icmp_error");
	ICMPSTAT_OUTHIST(type);
	icp->icmp_type = type;
	if (type == ICMP_REDIRECT)
		icp->icmp_gwaddr = dest;
	else {
		icp->icmp_void = 0;
		/* 
		 * The following assignments assume an overlay with the
		 * zeroed icmp_void field.
		 */
		if (type == ICMP_PARAMPROB) {
			icp->icmp_pptr = code;
			code = 0;
		} else if (type == ICMP_UNREACH &&
			code == ICMP_UNREACH_NEEDFRAG && destifp) {
			if (sesmgr_enabled)
				/* Leave room for IP security options */
				icp->icmp_nextmtu =
					htons(destifp->if_mtu - MAX_IPOPTLEN);
			else
				icp->icmp_nextmtu = htons(destifp->if_mtu);
		}
	}

	icp->icmp_code = code;
	bcopy((caddr_t)oip, (caddr_t)&icp->icmp_ip, icmplen);
	nip = &icp->icmp_ip;
	nip->ip_len = htons((u_short)(nip->ip_len + oiplen));

	/*
	 * Now, copy old ip header (without options)
	 * in front of icmp message.
	 */
	if (m->m_len > MLEN-sizeof(struct ip) || m->m_off < sizeof(struct ip))
		panic("icmp len");
	m->m_off -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);
	nip = mtod(m, struct ip *);
	bcopy((caddr_t)oip, (caddr_t)nip, sizeof(struct ip));
	nip->ip_len = m->m_len;
	nip->ip_hl = sizeof(struct ip) >> 2;
	nip->ip_p = IPPROTO_ICMP;
	nip->ip_tos = 0;
	icmp_reflect(m, ifp, ipsec);

freeit:
	m_freem(n);
	if (ipsec)
		_SESMGR_SOATTR_FREE(ipsec);
	return;
}

/*
 * Process a received ICMP message.
 */
/*ARGSUSED*/
void
#ifdef INET6
icmp_input(struct mbuf *m, struct ifnet *ifp, struct ipsec * ipsec,
  struct mbuf *unused)
#else
icmp_input(struct mbuf *m, struct ifnet *ifp, struct ipsec * ipsec)
#endif
{
	register struct icmp *icp;
	register struct ip *ip = mtod(m, struct ip *);
	int icmplen = ip->ip_len, hlen = ip->ip_hl << 2;
	register int i;
	struct in_ifaddr *ia;
	struct ifaddr *ifa;
	void (*ctlfunc)(int , struct sockaddr*, void *);
	int code;

	struct sockaddr_in icmpsrc = in_zeroaddr; 
	struct sockaddr_in icmpdst = in_zeroaddr;
	struct sockaddr_in icmpgw = in_zeroaddr;

	/*
	 * Locate icmp structure in mbuf, and check
	 * that not corrupted and of at least minimum length.
	 */
#ifdef ICMPPRINTFS
	if (icmpprintfs)
		printf("icmp_input from %x to %x, len %d\n",
			ntohl(ip->ip_src.s_addr), ntohl(ip->ip_dst.s_addr),
			icmplen);
#endif
	if (icmplen < ICMP_MINLEN) {
		ICMPSTAT(icps_tooshort);
		goto freeit;
	}
	i = hlen + MIN(icmplen, ICMP_ADVLENMIN);
	if (m->m_len < i && (m = m_pullup(m, i)) == 0)  {
		ICMPSTAT(icps_tooshort);
		_SESMGR_SOATTR_FREE(ipsec);
		return;
	}
	ip = mtod(m, struct ip *);
	m->m_len -= hlen;
	m->m_off += hlen;
	icp = mtod(m, struct icmp *);
	if (in_cksum(m, icmplen)) {
		ICMPSTAT(icps_checksum);
		goto freeit;
	}
	m->m_len += hlen;
	m->m_off -= hlen;

	/*
	 * Message type specific processing.
	 */
#ifdef ICMPPRINTFS
	if (icmpprintfs)
		printf("icmp_input, type %d code %d\n", icp->icmp_type,
		    icp->icmp_code);
#endif
	if (icp->icmp_type > ICMP_MAXTYPE)
		goto raw;
	ICMPSTAT_INHIST(icp->icmp_type);
	code = icp->icmp_code;
	switch (icp->icmp_type) {

	case ICMP_UNREACH:
		switch (code) {
			case ICMP_UNREACH_NET:
			case ICMP_UNREACH_HOST:
			case ICMP_UNREACH_PROTOCOL:
			case ICMP_UNREACH_PORT:
			case ICMP_UNREACH_SRCFAIL:
				code += PRC_UNREACH_NET;
				break;

			case ICMP_UNREACH_NEEDFRAG:
				code = PRC_MSGSIZE;
				break;
				
			case ICMP_UNREACH_NET_UNKNOWN:
			case ICMP_UNREACH_NET_PROHIB:
			case ICMP_UNREACH_TOSNET:
				code = PRC_UNREACH_NET;
				break;

			case ICMP_UNREACH_HOST_UNKNOWN:
			case ICMP_UNREACH_ISOLATED:
			case ICMP_UNREACH_HOST_PROHIB:
			case ICMP_UNREACH_TOSHOST:
				code = PRC_UNREACH_HOST;
				break;

			default:
				goto badcode;
		}
		goto deliver;

	case ICMP_TIMXCEED:
		if (code > 1)
			goto badcode;
		code += PRC_TIMXCEED_INTRANS;
		goto deliver;

	case ICMP_PARAMPROB:
		if (code > 1)
			goto badcode;
		code = PRC_PARAMPROB;
		goto deliver;

	case ICMP_SOURCEQUENCH:
		if (code)
			goto badcode;
		code = PRC_QUENCH;
	deliver:
		/*
		 * Problem with datagram; advise higher level routines
		 * if not sent in response to a multicast.
		 */
		if (icmplen < ICMP_ADVLENMIN || icmplen < ICMP_ADVLEN(icp) ||
		    icp->icmp_ip.ip_hl < (sizeof(struct ip) >> 2)) {
			ICMPSTAT(icps_badlen);
			goto freeit;
		}
		if (IN_MULTICAST(ntohl(icp->icmp_ip.ip_dst.s_addr))) {
			goto badcode;
		}
		NTOHS(icp->icmp_ip.ip_len);
#ifdef ICMPPRINTFS
		if (icmpprintfs)
			printf("deliver to protocol %d\n", icp->icmp_ip.ip_p);
#endif
		icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
		if (ctlfunc = inetsw[ip_protox[icp->icmp_ip.ip_p]].pr_ctlinput)
			(*ctlfunc)(code, (struct sockaddr *)&icmpsrc,
			    icp );
		break;

	badcode:
		ICMPSTAT(icps_badcode);
		break;

	case ICMP_ECHO:
		icp->icmp_type = ICMP_ECHOREPLY;
		goto reflect;

	case ICMP_TSTAMP:
		if (icmplen < ICMP_TSLEN) {
			ICMPSTAT(icps_badlen);
			break;
		}
		icp->icmp_type = ICMP_TSTAMPREPLY;
		icp->icmp_rtime = iptime();
		icp->icmp_ttime = icp->icmp_rtime;	/* bogus, do later! */
		goto reflect;
		
	case ICMP_MASKREQ:

		if (icmpmaskrepl == 0)
			break;
		/*
		 * We are not able to respond with all ones broadcast
		 * unless we receive it over a point-to-point interface.
		 */
		if (icmplen < ICMP_MASKLEN)
			break;
		switch (ip->ip_dst.s_addr) {

		case INADDR_BROADCAST:
		case INADDR_ANY:
			icmpdst.sin_addr = ip->ip_src;
			break;

		default:
			icmpdst.sin_addr = ip->ip_dst;
		}
		ifa = ifaof_ifpforaddr((struct sockaddr *)&icmpdst, ifp);
		ia = (ifa) ? (struct in_ifaddr *)(ifa->ifa_start_inifaddr)
			   :(struct in_ifaddr *)0;
		if (ia == 0)
			break;
		icp->icmp_type = ICMP_MASKREPLY;
		icp->icmp_mask = ia->ia_sockmask.sin_addr.s_addr;
		if (ip->ip_src.s_addr == 0) {
			if (ia->ia_ifp->if_flags & IFF_BROADCAST)
			    ip->ip_src = satosin(&ia->ia_broadaddr)->sin_addr;
			else if (ia->ia_ifp->if_flags & IFF_POINTOPOINT)
			    ip->ip_src = satosin(&ia->ia_dstaddr)->sin_addr;
		}
reflect:
		ip->ip_len += hlen;	/* since ip_input deducts this */
		ICMPSTAT(icps_reflect);
		ICMPSTAT_OUTHIST(icp->icmp_type);
		icmp_reflect(m, ifp, ipsec);
		_SESMGR_SOATTR_FREE(ipsec);
		return;

	case ICMP_REDIRECT:
		if (code > 3)
			goto badcode;
		/* Drop ICMP_REDIRECT if icmp_dropredirects set */
		if (icmp_dropredirects) {
			ICMPSTAT(icps_dropped);
			break;
		}
		if (icmplen < ICMP_ADVLENMIN || icmplen < ICMP_ADVLEN(icp) ||
		    icp->icmp_ip.ip_hl < (sizeof(struct ip) >> 2)) {
			ICMPSTAT(icps_badlen);
			break;
		}
		/*
		 * Short circuit routing redirects to force
		 * immediate change in the kernel's routing
		 * tables.  The message is also handed to anyone
		 * listening on a raw socket (e.g. the routing
		 * daemon for use in updating its tables).
		 */
		icmpgw.sin_addr = ip->ip_src;
		icmpdst.sin_addr = icp->icmp_gwaddr;
#ifdef	ICMPPRINTFS
		if (icmpprintfs)
			printf("redirect dst %x to %x\n", icp->icmp_ip.ip_dst,
				icp->icmp_gwaddr);
#endif
		icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
		rtredirect((struct sockaddr *)&icmpsrc,
		  (struct sockaddr *)&icmpdst,
		  0, RTF_GATEWAY | RTF_HOST,
		  (struct sockaddr *)&icmpgw, (struct rtentry **)0);
		pfctlinput(PRC_REDIRECT_HOST, (struct sockaddr *)&icmpsrc);
		break;

	/*
	 * No kernel processing for the following;
	 * just fall through to send to raw listener.
	 */
	case ICMP_ECHOREPLY:
	case ICMP_ROUTERADVERT:
	case ICMP_ROUTERSOLICIT:
	case ICMP_TSTAMPREPLY:
	case ICMP_IREQREPLY:
	case ICMP_MASKREPLY:
	default:
		break;
	}
raw:
#ifdef INET6
	rip_input(m, ifp, ipsec, NULL);
#else
	rip_input(m, ifp, ipsec);
#endif
	return;

freeit:
	_SESMGR_SOATTR_FREE(ipsec);
	m_freem(m);
	return;
}

/*
 * This is a broadcast hash lookup match procedure which returns non-zero
 * IF the IP address supplied matches the broadcast address in the in_bcast
 * record AND the interface supports broadcast.
 *
 * NOTE: This procedure is called with all hash bucket entries so you have
 * to check the specified type, here it's HTF_BROADCAST types.
 */
/* ARGSUSED */
int
icmp_reflect_bcastmatch(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in_bcast *ia_bcast = (struct in_bcast *)h;
	struct in_addr *addr = (struct in_addr *)key;
	int match = 0;
	unsigned long t;

	if ((h->flags & HTF_BROADCAST) == 0) { /* forget it */
		return 0;
	}

	if (ia_bcast->ifp->if_flags & IFF_BROADCAST) {

		if (ia_bcast->ia_broadaddr.sin_addr.s_addr == addr->s_addr) {
			match = 1;
			goto ours;
		}

		if (addr->s_addr == ia_bcast->ia_netbroadcast.s_addr) {
			match = 1;
			goto ours;
		}
		/*
		 * Look for all-0's host part (old broadcast address),
		 * either for subnet or net.
		 */
		t = ntohl(addr->s_addr);
		if ((t == ia_bcast->ia_subnet) || (t == ia_bcast->ia_net)) {
			match = 1;
		}
	}
ours:
	return match;
}

/*
 * This is a hash lookup match procedure which return non-zero
 * IF the IP address supplied matches the address in the in_ifaddr record OR
 * the interface supports broadcast and it matches the broadcast address.
 *
 * NOTE: This procedure is called with all hash bucket entries so you have
 * to check the specified type, here it's HTF_INET types.
 */
/* ARGSUSED */
int
icmp_reflect_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in_ifaddr *ia;
	struct in_addr *addr;
	int match;

	if ((h->flags & HTF_INET) == 0) { /* forget it */
		return 0;
	}

	ia = (struct in_ifaddr *)h;
	addr = (struct in_addr *)key;
	match = 0;

	if (addr->s_addr == IA_SIN(ia)->sin_addr.s_addr) {
		match = 1;
	} else {
		if ((ia->ia_ifp->if_flags & IFF_BROADCAST) &&
		 addr->s_addr == satosin(&ia->ia_broadaddr)->sin_addr.s_addr) {
			match = 1;
		}
	}
	return match;
}

/*
 * Reflect the ip packet back to the source
 */
void
icmp_reflect(struct mbuf *m, struct ifnet *ifp, struct ipsec *ipsec)
{
	register struct ip *ip = mtod(m, struct ip *);
	register struct in_ifaddr *ia;
	struct in_bcast *in_bcastp;
	struct ifaddr *ifa;
	struct in_addr t;
	struct mbuf *opts = 0;
	int optlen = (ip->ip_hl << 2) - sizeof(struct ip);
	struct sockaddr_in icmpdst = in_zeroaddr;

	if (!in_canforward(ip->ip_src) &&
	    ((ntohl(ip->ip_src.s_addr) & IN_CLASSA_NET) !=
	     (IN_LOOPBACKNET << IN_CLASSA_NSHIFT))) {
		m_freem(m);	/* Bad return address */
		goto done;	/* Ip_output() will check for broadcast */
	}
	t = ip->ip_dst;
	ip->ip_dst = ip->ip_src;
	/*
	 * If the incoming packet was addressed directly to us,
	 * use dst as the src for the reply.  Otherwise (broadcast
	 * or anonymous), use the address which corresponds
	 * to the incoming interface.
	 *
	 * We look for a 'struct in_ifaddr' matching the ip_dst address OR
	 * an interface which supports broadcast AND the broadcast
	 * address for the interface matches the ip_dst address.
	 * NOTE: Since the ip_dst field maybe a broadcast address and
	 * not always a unicast address we can't always use a direct hash
	 * lookup operation which might fail in some cases.
	 */
	ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
		icmp_reflect_match, (caddr_t)&t, (caddr_t)0, (caddr_t)0);

	icmpdst.sin_addr = t;
	if (ia == (struct in_ifaddr *)0) {
		/*
		 * check if dst ip addr matches a broadcast address
		 */
		in_bcastp = (struct in_bcast *)hash_lookup(
			&hashinfo_inaddr_bcast, icmp_reflect_bcastmatch,
			(caddr_t)&t, (caddr_t)0, (caddr_t)0);

		if (in_bcastp) { /* yes so use ip address associated */
			t = in_bcastp->ia_addr.sin_addr;
		} else {
			/*
			 * Failed to find an interface address matching 't' OR
			 * a broadcast address match, so try to find ANY
			 * address structure associated with this interface.
			 */
			ifa = ifaof_ifpforaddr((struct sockaddr *)&icmpdst,
					ifp);

			if (ifa) {
			    ia = (struct in_ifaddr *)(ifa->ifa_start_inifaddr);
			}

			if (ia == 0) {
				/*
				 * Arrive here when no ifa was found BUT the
				 * packet was not addressed to us but was
				 * received on some interface. In that case
				 * 't' was set to the dst ip address above,
				 * which was for us!
				 * NOTE: This is a pretty weird case so we'll
				 * try and set 't' to the primary IP address
				 * of the first network interface.
				 */
				if (in_ifaddr_count <= 0) {
					/*
					 * No addresses configured so how
					 * did we get the pkt? Nearly
					 * impossible case so drop the packet!
					 */
					m_freem(m);
					goto done;
				} else {
					/*
					 * Ok, use the address on the
					 * first network interface
					 */
					ia = (struct in_ifaddr *)
						(ifnet->in_ifaddr);
				}
			}
			t = IA_SIN(ia)->sin_addr;
		} /* end of else clause */
	}
	ip->ip_src = t;
	ip->ip_ttl = MAXTTL;

	if (optlen > 0) {
		register u_char *cp;
		int opt, cnt;
		u_int len;

		/*
		 * Source routing replies no longer supported!
		 * add on any record-route or timestamp options.
		 */
		cp = (u_char *) (ip + 1);
		if (opts = m_get(M_DONTWAIT, MT_HEADER)) {
			opts->m_len = sizeof(struct in_addr);
			mtod(opts, struct in_addr *)->s_addr = 0;
		}
		if (opts) {
#ifdef ICMPPRINTFS
		    if (icmpprintfs)
			    printf("icmp_reflect optlen %d rt %d => ",
				optlen, opts->m_len);
#endif
		    for (cnt = optlen; cnt > 0; cnt -= len, cp += len) {
			    opt = cp[IPOPT_OPTVAL];
			    if (opt == IPOPT_EOL)
				    break;
			    if (opt == IPOPT_NOP)
				    len = 1;
			    else {
				    len = cp[IPOPT_OLEN];
				    if (len <= 0 || len > cnt)
					    break;
			    }
			    /*
			     * Should check for overflow, but it "can't happen"
			     */
			    if (opt == IPOPT_RR || opt == IPOPT_TS || 
				opt == IPOPT_SECURITY) {
				    bcopy((caddr_t)cp,
					mtod(opts, caddr_t) + opts->m_len,len);
				    opts->m_len += len;
			    }
		    }
		    /* Terminate & pad, if necessary */
		    if (cnt = opts->m_len % 4) {
			    for (; cnt < 4; cnt++) {
				    *(mtod(opts, caddr_t) + opts->m_len) =
					IPOPT_EOL;
				    opts->m_len++;
			    }
		    }
		    ASSERT((opts->m_len & 3) == 0);
#ifdef ICMPPRINTFS
		    if (icmpprintfs)
			    printf("%d\n", opts->m_len);
#endif
		}
		/*
		 * Now strip out original options by copying rest of first
		 * mbuf's data back, and adjust the IP length.
		 */
		ip->ip_len -= optlen;
		ip->ip_hl = sizeof(struct ip) >> 2;
		m->m_len -= optlen;
		optlen += sizeof(struct ip);
		bcopy((caddr_t)ip + optlen, (caddr_t)(ip + 1),
			 (unsigned)(m->m_len - sizeof(struct ip)));
	}
	 m->m_flags &= ~(M_BCAST|M_MCAST);
	ASSERT(ip == mtod(m, struct ip *));
	icmp_send(m, opts, ipsec);
done:
	if (opts)
		(void)m_free(opts);
}

/*
 * This is a hash enumeration match procedure which compares the interface
 * address with that in an HTF_INET type hash bucket entry.
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
int
ifptoia_enummatch(struct hashbucket *h, caddr_t key, caddr_t arg1,caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct ifnet *ifp = (struct ifnet *)key;
	int match = 0;

	if (ia->ia_ifp == ifp) match = 1;
	return match;
}

/*
 * Find the first address structure associated with the interface.
 */
struct in_ifaddr *
ifptoia(struct ifnet *ifp)
{
	register struct in_ifaddr *ia;

	ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
		ifptoia_enummatch, HTF_INET,
		(caddr_t)ifp, (caddr_t)0, (caddr_t)0);
	return ia;
}

/*
 * Send an icmp packet back to the ip level, after supplying a checksum.
 */
void
icmp_send(struct mbuf *m, struct mbuf *opts, struct ipsec *ipsec)
{
	register struct ip *ip = mtod(m, struct ip *);
	register int hlen;
	register struct icmp *icp;

	hlen = ip->ip_hl << 2;
	m->m_off += hlen;
	m->m_len -= hlen;
	icp = mtod(m, struct icmp *);
	icp->icmp_cksum = 0;
	icp->icmp_cksum = in_cksum(m, ip->ip_len - hlen);
	m->m_flags &= ~M_CKSUMMED;
	m->m_off -= hlen;
	m->m_len += hlen;
#ifdef ICMPPRINTFS
	if (icmpprintfs)
		printf("icmp_send dst %x src %x\n", ip->ip_dst, ip->ip_src);
#endif
	(void)ip_output(m, opts, NULL, 0, NULL, ipsec);
	return;
}

n_time
iptime(void)
{
	struct timeval atv;
	u_long t;

	microtime(&atv);
	t = (atv.tv_sec % (24*60*60)) * 1000 + atv.tv_usec / 1000;
	return (htonl(t));
}

#ifndef sgi
int
icmp_sysctl(int *name,
	    u_int namelen,
	    void *oldp,
	    size_t *oldlenp,
	    void *newp,
	    size_t newlen)
{

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case ICMPCTL_MASKREPL:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &icmpmaskrepl));
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
#endif
