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
 *	@(#)ip_input.c	7.10 (Berkeley) 6/29/88 plus MULTICAST 1.1
 */

#ident "$Revision: 4.88 $"

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/domain.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/errno.h"
#include "sys/par.h"
#include "sys/tcpipstats.h"
#include "sys/kmem.h"
#include "sys/cmn_err.h"
#include "sys/atomic_ops.h"
#include "sys/sesmgr.h"
#include "in.h"
#include "ipfilter.h"

#include "net/if.h"
#include "net/route.h"
#include "ksys/xthread.h"
#include "net/netisr.h"

#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "in_var.h"
#include "ip_var.h"
#include "ip_icmp.h"
#include "ip_mroute.h"
#include "icmp_var.h"
#include "tcp.h"
#include "tcpip_ovly.h"

/*
 * Local Module defined macros
 */
#define	satosin(sa)	((struct sockaddr_in *)(sa))
#define	INA	struct in_ifaddr *
#define	SA	struct sockaddr *

/*
 * External global variables
 */
extern int ipforwarding;
extern int in_interfaces;
extern int in_ifaddr_count;
extern int ipsendredirects;

extern int ipdirected_broadcast;
extern struct socket *ip_mrouter;

/*
 * Forward referenced procedures
 */
static void ip_input(struct mbuf *m, struct route *);
int ip_acceptit_broadcast(struct in_ifaddr *, struct in_addr *,struct ifnet *);
int ip_check_broadcast(struct in_ifaddr *, struct in_addr *);
int  ip_dooptions(struct mbuf *m, struct ifnet *ifp, 
		  struct ipsec *ipsec, struct route *forward_rt);

void ip_deq(struct ipasfrag *p);
void ip_enq(struct ipasfrag *p, struct ipasfrag *prev);

static void ip_forward(struct mbuf *m, int srcrt,
	struct ifnet *ifp, struct ipsec *ipsec, struct route *forward_rt);
struct in_ifaddr *ip_rtaddr(struct in_addr dst, struct route *forward_rt);

/*
 * External procedures
 */
extern void arp_init(void);

extern struct ifaddr *ifa_ifwithaddr(struct sockaddr *);
extern struct ifaddr *ifa_ifwithdstaddr(struct sockaddr *);
extern struct ifaddr *ifa_ifwithnet(struct sockaddr *);
extern int ifnet_enummatch(struct hashbucket *h, caddr_t key, caddr_t arg1,
		caddr_t arg2);
extern int ifnet_any_enummatch(struct hashbucket *h, caddr_t key, caddr_t arg1,
		caddr_t arg2);
extern int in_multi_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
		caddr_t arg2);
extern int in_canforward(struct in_addr);
extern struct in_ifaddr *ifptoia(struct ifnet *);
extern int ipfilter_kernel(struct ifnet *, struct mbuf *);

/*
 * Module global variables
 */
int ipalias_count;		/* number IP alias'es on all interfaces */

#ifdef IPPRINTFS
extern int ipprintfs;
#endif

/*
 * IP fragment reassembly data structures and functions
 */
#if defined(EVEREST) || defined(SN)
#define FRAG_BUCKETS 16
#else
#define FRAG_BUCKETS 4
#endif

struct ipfrag_bucket {
	mutex_t ipfrag_lock;		/* protects this bucket */
#if _MIPS_SIM == _ABI64
	struct  ipq	ipq_dummy; /* extra space in front for ifasfrag */
#endif
	struct 	ipq	ipq;
	int ipfrag_count;		/* counts up to ipfrag_limit */

} ipfrag_qs [FRAG_BUCKETS];

int ipfrag_limit = 2048 / FRAG_BUCKETS;

struct	ip *ip_reass(struct mbuf *m, struct ipq *fp, 
		     struct mbuf **mpp, struct ipfrag_bucket *b);
struct mbuf* ip_reass_adjm(struct mbuf *m, struct ip *);
static void ip_freef(struct mbuf *m, struct ipfrag_bucket *b);

#if 0

/*
 * We'd like to do the following in order to isolate ip_id onto its own cache
 * line but a bug in the compiler, #694654, ``-common and #pragma fill_symbol
 * don't play nice together,'' prevents us from doing that.  We should
 * uncomment this code as soon as that bug gets fixed and the fix is available
 * in the build toolroot (or a workaround if found).  Instead we'll have to
 * hack in an intermediate type with an alignment constrain in order to
 * accomplish this until this bug is fixed ...
 */

hotIntCounter_t	ip_id;			/* ip packet ctr, for ids */
#if MP
#pragma fill_symbol (ip_id, 128)
#endif

#else /* 0 */

#if MP
typedef hotIntCounter_t hotAlignedIntCounter_t;
#pragma set type attribute hotAlignedIntCounter_t align=128
hotAlignedIntCounter_t	ip_id;		/* ip packet ctr, for ids */
#else
hotIntCounter_t	ip_id;			/* ip packet ctr, for ids */
#endif

#endif /* 0 */

int rsvp_on = 0;
u_char	ip_protox[IPPROTO_MAX];

const struct sockaddr_in in_zeroaddr = { AF_INET };
struct socket *ip_rsvpd;			/* turns on RSVP intercept */

#ifdef EVEREST
extern int io4ia_war;
extern void m_io4iawar(struct mbuf*);
#endif

/*
 * IP multicast routing variables: set if ip_mroute.o configured in kernel
 */
struct socket  *ip_mrouter = NULL;
int ip_mrtproto;

/* Enable/disable IP packet filtering */
int ipfilterflag;

const u_char inetctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	ENETUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};

/*
 * Filter Process definition
 */
struct mbuf * (*ipf_process)(struct mbuf *in_pkt, int if_index, int flags);


/*
 * IP initialization: fill in IP protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
void
ip_init(void)
{
	register struct protosw *pr;
	register int i;
	extern void arp_init(void);
	extern int max_netprocs;
	u_char temp;

	pr = pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW, NULL);
	if (pr == 0)
		panic("ip_init");
	temp = pr - inetsw;
	for (i = 0; i < IPPROTO_MAX; i++)
		ip_protox[i] = temp;
	for (pr = inetdomain.dom_protosw;
	    pr < inetdomain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip_protox[pr->pr_protocol] = pr - inetsw;
	setIntHot(&ip_id, time);
	for (i = 0; i < FRAG_BUCKETS; i++) {
		mutex_init(&ipfrag_qs[i].ipfrag_lock, MUTEX_DEFAULT, "ipfrag");
		ipfrag_qs[i].ipq.next = ipfrag_qs[i].ipq.prev = &ipfrag_qs[i].ipq;
	}
	init_max_netprocs();
	ipf_process = NULL;	/* filter process */
	network_input_setup(AF_INET, ip_input);
	return;
}

/*
 * Broadcast hash lookup match procedure to check if this address matches
 * any broadcast address for this system.
 * Returns 1 if for us and zero otherwise.
 *
 * ip_acceptit_bcast_match(struct in_ifaddr *ia, struct sockaddr_in *key,
 *  struct ifnet *arg1, caddr_t arg2)
 */
/* ARGSUSED */
int
ip_acceptit_bcast_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in_bcast *ia_bcast;
	struct in_addr *addr;
	struct ifnet *ifp;
	unsigned long t;
	int accept = 1;

	ia_bcast = (struct in_bcast *)h;
	addr = (struct in_addr *)key;
	ifp = (struct ifnet *)arg1;

	/*
	 * NOTE: Something maybe wrong here in the case where IP directed
	 * is TRUE and IP forwarding is ON since broadcast packets destined
	 * for network A that arrived on network B's interface must be
	 * broadcast on network A. The problem is that the 'ifp' pointer
	 * will point to the interface record for network A and NOT network B.
	 * So the check if the 'ia_ifp' field matches the 'ifp' maybe
	 * wrong in this case. Need to verify this case. XXX WEF
	 */
	if ((!ipdirected_broadcast || (ia_bcast->ifp == ifp)) &&
	    (ia_bcast->ifp->if_flags & IFF_BROADCAST)) {

		if (ia_bcast->ia_broadaddr.sin_addr.s_addr == addr->s_addr) {
			goto ours;
		}

		if (addr->s_addr == ia_bcast->ia_netbroadcast.s_addr) {
			goto ours;
		}
		/*
		 * Look for all-0's host part (old broadcast address),
		 * either for subnet or net.
		 */
		t = ntohl(addr->s_addr);
		if ((t == ia_bcast->ia_subnet) || (t == ia_bcast->ia_net)) {
			goto ours;
		}
		/*
		 * Reject it as it doesn't match any of our
		 * broadcast addresses
		 */
		accept = 0;
	}
ours:
	return accept;
}

/*
 * hash lookup match procedure which check if the address supplied matches
 * one associated with this system.
 * Returns 1 if for us and zero otherwise.
 *
 * ip_acceptit_match(struct in_ifaddr *ia, struct sockaddr_in *ia,
 *  struct ifnet *ifp)
 */
/* ARGSUSED */
int
ip_acceptit_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in_ifaddr *ia;
	struct in_addr *addr;
	struct ifnet *ifp;
	int accept;

	if ((h->flags & (HTF_INET|HTF_MULTICAST)) == 0) {
		/* not correct bucket type */
		return 0;
	}
	if (h->flags & HTF_MULTICAST) { /* multicast address node */
		return (in_multi_match(h, key, arg1, arg2));
	}

	ia = (struct in_ifaddr *)h;
	addr = (struct in_addr *)key;
	ifp = (struct ifnet *)arg1;
	accept = 1;

	if (satosin(&ia->ia_addr)->sin_addr.s_addr == addr->s_addr) {
		goto ours;
	}

	if (ip_acceptit_broadcast(ia, addr, ifp)) {
		goto ours;
	}
	accept = 0;
ours:
	return accept;
}

/*
 * This procedure does the fast check to see if the IP address supplied
 * matches the broadcast address for each interface configured onto the
 * system. We return 1 if we should accept the packet and* zero otherwise.
 * NOTE: This procedure avoids the more expensive enumeration of all the
 * entire IP address table for the high frequency loop-back packet case.
 */
int
ip_accept_primary_broadcast(struct in_addr *addr)
{
	struct ifnet *ifp;
	struct in_ifaddr *ia;

	for (ifp = ifnet; ifp; ifp = ifp->if_next) {

		if ((ia = (struct in_ifaddr *)(ifp->in_ifaddr)) == NULL) {
			continue;
		}
		if (ip_check_broadcast(ia, addr)) {
			return 1;
		}
	}
	return 0;
}

/*
 * This procedure checks if the IP address supplied matches the broadcast
 * address for this interface or the network broadcast address for the
 * connected network. We return 1 if we should accept the packet and
 * zero otherwise.
 */
int
ip_acceptit_broadcast(struct in_ifaddr *ia, struct in_addr *addr,
	struct ifnet *ifp)
{
	int accept = 1;

	/*
	 * NOTE: Something maybe wrong here in the case where IP directed
	 * is TRUE and IP forwarding is ON since broadcast packets destined
	 * for network A that arrived on network B's interface must be
	 * broadcast on network A. The problem is that the 'ifp' pointer
	 * will point to the interface record for network A and NOT betwork
	 * B. So the check if the 'ia_ifp' field matches the 'ifp' maybe
	 * wrong in this case. Ned to check this out. XXX WEF
	 */
	if ((!ipdirected_broadcast || (ia->ia_ifp == ifp)) && 
	    (ia->ia_ifp->if_flags & IFF_BROADCAST)) {

		if (ip_check_broadcast(ia, addr)) {
			goto ours;
		}
	}
	accept = 0;
ours:
	return accept;
}

/*
 * This procedure checks if the IP address supplied matches the broadcast
 * address for this interface or the network broadcast address for the
 * connected network. We return 1 if we should accept the packet and
 * zero otherwise.
 */
int
ip_check_broadcast(struct in_ifaddr *ia, struct in_addr *addr)
{
	register u_long t;
	register int accept = 1;

	if ((ia->ia_ifp->if_flags & IFF_LOOPBACK) == 0) {

		/* broadcast flags not set on loopback interfaces */
		if ((ia->ia_ifp->if_flags & IFF_BROADCAST) == 0) {
			goto not_ours;
		}
	}
	      
	if (ia->ia_broadaddr.sin_addr.s_addr == addr->s_addr) {
		goto ours;
	}
	if (addr->s_addr == ia->ia_netbroadcast.s_addr) {
		goto ours;
	}
	/*
	 * Look for all-0's host part (old broadcast address),
	 * either for subnet or net.
	 */
	t = ntohl(addr->s_addr);
	if (t == ia->ia_subnet) {
		goto ours;
	}
	if (t == ia->ia_net) {
		goto ours;
	}
not_ours:
	accept = 0;
ours:
	return accept;
}

/*
 * Ip input routine.  Checksum and byte swap header.  If fragmented
 * try to reassemble.  If complete and fragment queue exists, discard.
 * Process options.  Pass to next level.
 */
static void
ip_input(struct mbuf *m, struct route *forward_rt)
{
	register struct ip *ip;
	register struct mbuf *m0;
	struct ipq *fp;
	struct in_ifaddr *ia;
	struct ifnet *ifp;
	int i, hlen, verdict, multicast_acceptit;
	struct ipsec *ipsec;
	extern int allow_brdaddr_srcaddr;

	IPSTAT(ips_total);
	ifp = mtod(m, struct ifheader *)->ifh_ifp;
	hlen = mtod(m, struct ifheader *)->ifh_hdrlen;
	M_ADJ(m, hlen);
	if (m == NULL || m->m_len < sizeof (struct ip) &&
	    (m = m_pullup(m, sizeof (struct ip))) == 0) {
#pragma mips_frequency_hint NEVER
		IPSTAT(ips_toosmall);
		return;
	}
	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) {	/* minimum header length */
#pragma mips_frequency_hint NEVER
		IPSTAT(ips_badhlen);
		m_freem(m);
		return;
	}
	if (hlen > m->m_len) {
#pragma mips_frequency_hint NEVER
		if ((m = m_pullup(m, hlen)) == 0) {
			IPSTAT(ips_badhlen);
			return;
		}
		ip = mtod(m, struct ip *);
	}
	NETPAR(NETSCHED, NETEVENTKN, ip->ip_id,
		 NETEVENT_IPUP, NETCNT_NULL, NETRES_INT);
	/*
	 * Do in-line header checksum in the normal case.
	 */
	if (hlen == sizeof(struct ip)) {
		ulong cksum;
#define ckp ((ushort*)ip)
		cksum = (ckp[0] + ckp[1] + ckp[2] + ckp[3] + ckp[4] +
			 ckp[5] + ckp[6] + ckp[7] + ckp[8] + ckp[9]);
		cksum = (cksum & 0xffff) + (cksum >> 16);
		cksum = (cksum & 0xffff) + (cksum >> 16);
#undef ckp
		if (cksum != 0xffff && cksum) {
			IPSTAT(ips_badsum);
			m_freem(m);
			return;
		}
	} else if (ip->ip_sum = in_cksum(m, hlen)) {
#pragma mips_frequency_hint NEVER
		IPSTAT(ips_badsum);
#ifdef IPPRINTFS
		if (ipprintfs)
		  printf("bad IP cksum in: %d:%d %x %d %x %x %d %d %x %x %x\n",
			    ip->ip_hl, ip->ip_v, ip->ip_tos,
			    ip->ip_len, ip->ip_id, ip->ip_off,
			    ip->ip_ttl, ip->ip_p, ip->ip_sum,
			    ip->ip_src.s_addr, ip->ip_dst.s_addr);
#endif
		NETPAR(NETFLOW, NETDROPTKN, ip->ip_id,
			NETEVENT_IPUP, ntohs(ip->ip_len), NETRES_CKSUM);
		m_freem(m);
		return;
	}
	if (ip->ip_v != IPVERSION) {
#pragma mips_frequency_hint NEVER
		m_freem(m);
		return;
	}

	/*
	 * If filtering is enabled, does this packet pass?
	 */
	if (ipfilterflag &&
	    ((verdict = ipfilter_kernel(ifp, m))== IPF_DROPIT)) {
#pragma mips_frequency_hint NEVER
		return;
	}
	/*
	 * Stateful filtering on input packets
	 */
	if (ipf_process) {
#pragma mips_frequency_hint NEVER
	    if ((m = ((*ipf_process)(m, ifp->if_index, IPF_IN))) == 0) {
		IPSTAT(ips_cantforward);
		return;
	    }
	    else {
		/* do some rechecking on packet and set values */
		if (m->m_len < sizeof (struct ip) &&
		    (m = m_pullup(m, sizeof (struct ip))) == 0) {
		    IPSTAT(ips_toosmall);
		    return;
		}
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
		if (hlen < sizeof(struct ip)) {	/* minimum header length */
		    IPSTAT(ips_badhlen);
		    m_freem(m);
		    return;
		}
		if (hlen > m->m_len) {
		    if ((m = m_pullup(m, hlen)) == 0) {
			IPSTAT(ips_badhlen);
			return;
		    }
		    ip = mtod(m, struct ip *);
		}
	    }	 
	}  

	/*
	 * Convert fields to host representation.
	 */
 	NTOHS(ip->ip_len);
	if (ip->ip_len < hlen) {
#pragma mips_frequency_hint NEVER
		IPSTAT(ips_badlen);
		NETPAR(NETFLOW, NETDROPTKN, ip->ip_id,
			 NETEVENT_IPUP, ntohs(ip->ip_len), NETRES_SIZE);
		m_freem(m);
		return;
	}
 	NTOHS(ip->ip_id);
 	NTOHS(ip->ip_off);

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	i = -(u_short)ip->ip_len;
	m0 = m;
	for (;;) {
		i += m->m_len;
		if (m->m_next == 0)
			break;
		m = m->m_next;
	}
	if (i != 0) {
		if (i < 0) {
#pragma mips_frequency_hint NEVER
			IPSTAT(ips_tooshort);
			m_freem(m0);
			return;
		}
		if (i <= m->m_len)
			m->m_len -= i;
		else
			m_adj(m0, -i);
	}
	m = m0;

	/*
	 * Reject bogus source addresses
	 * XXX also must reject [net,{subnet,-1},-1]
	 * as per (RFC1122 Section 3.2.1.3)
	 */
	if ((ip->ip_src.s_addr == INADDR_BROADCAST && !allow_brdaddr_srcaddr) ||
	    IN_MULTICAST(ntohl(ip->ip_src.s_addr))) {
#pragma mips_frequency_hint NEVER
		m_freem(m);
		return;
	}

	/*
	 * Process options and, if not destined for us,
	 * ship it on.  ip_dooptions returns 1 when an
	 * error was detected (causing an icmp message
	 * to be sent and the original packet to be freed).
	 */
	ASSERT(ip == mtod(m, struct ip *));

	if (_SESMGR_IP_OPTIONS(m, &ipsec) != 0) {
#pragma mips_frequency_hint NEVER
		m_freem(m);
		return;
	}
	if (hlen > sizeof (struct ip) && 
	    ip_dooptions(m, ifp, ipsec, forward_rt)) {
#pragma mips_frequency_hint NEVER
	    	return;
	}

	/*
	 * If RSVP interception is on, grab the packet no matter what
	 * the destination is (for auto-tunnelling to work)
	 */
	if ((ip->ip_p == IPPROTO_RSVP) && (rsvp_on || ip_rsvpd != NULL)) {
#pragma mips_frequency_hint NEVER
		goto ours;
	}

	/*
	 * Check our addresses, to see if the packet is for us. This is
	 * a special short circuit if we match the primary.
	 */
#define	IFPTOSIN(ifp) ((INA)(ifp)->in_ifaddr)->ia_addr

	if ((ifp->in_ifaddr && 
	    ip->ip_dst.s_addr == IFPTOSIN(ifp).sin_addr.s_addr)) {
		goto ours;
	}
#undef IFPTOSIN

	/*
	 * Check our addresses, to see if the packet is for us. We will get
	 * a match both unicast(and aliaes) and multicast addresses.
	 */
	if ((ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
		ip_acceptit_match,
		(caddr_t)(&(ip->ip_dst)),
		(caddr_t)ifp,
		(caddr_t)0))) {
		/*
		 * it's for us so check when it's a multicast address AND
		 * we're also acting as a multicast router.
		 */
		if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
			struct hashbucket *h_ia = (struct hashbucket *)ia;

			/*
			 * Pkt is in one of the dst multicast group addresses
			 * on the arrival interface AND dst ip address is
			 * multicast, so check if we're acting as a multicast
			 * router. Go to the common code and note that this
			 * packet may also need to be accepted.
			 */
			multicast_acceptit =
				(h_ia->flags & HTF_MULTICAST) ? 1 : 0;

			if (ip_mrouter) {
				goto multicast_forwarding;
			}
			/*
			 * dst ip addr is multicast address which we are
			 * also accepting so double check that the address
			 * entry is also multicast, if not drop the packet.
			 */
			if (multicast_acceptit == 0) { /* inconsistent case */
#pragma mips_frequency_hint NEVER
				m_freem(m);
				_SESMGR_SOATTR_FREE(ipsec);
				return;
			}
		}

		/* it's for us so go process the packet */
		goto ours;
	} else {
		/*
		 * Failed to match a unicast or multicast IP address.
		 * Now check if we've received a broadcast IP address which
		 * matches the broadcast address or the network broadcast
		 * address for the interface.
		 */
		if ((ia = (struct in_ifaddr *)ifp->in_ifaddr)) {
			if (ip_acceptit_broadcast(ia, &(ip->ip_dst), ifp)) {
				goto ours;
			}
		}
		/*
		 * Check if we match broadcast address on any network interface
		 * NOTE: This check handles receiving broadcast address'es
		 * on the loopback interface, where the dst IP address does
		 * NOT correspond to the one for this network interface.
		 */
		if (ip_accept_primary_broadcast(&(ip->ip_dst))) {
			goto ours;
		}

		/*
		 * NOTE: There are two hash tables for address'es; One which
		 * contains unicast and multicast IP address, which correspond
		 * to unicast IP or an IP alias address, and any of the
		 * multicast addresses which this system wants to receive.
		 * The second hash table contains entries for each broadcast
		 * address, the full broadcast address for the network and
		 * the netbroadcast address if they differ.
		 *
		 * NOTE: Both of these broadcast address'es are inserted into
		 * the bcast address hash table, on address registration.
		 * We also handle checking for multiple subnet's on the same
		 * physical network interface as with IP alias'es where a
		 * different IP broadcast address exists for each subnet.
		 * This is handled by having multiple broadcast hash table
		 * entries to cover these cases, so a simply lookup works.
		 *
		 * Now check if the pkt's dst ip address matches a broadcast
		 * address OR matches the netbroadcast address for any address
		 * associated with this interface.
		 */
		if (hash_lookup(&hashinfo_inaddr_bcast,
			ip_acceptit_bcast_match,
			(caddr_t)(&(ip->ip_dst)),
			(caddr_t)ifp,
			(caddr_t)0)) { /* match a bcast address */
				goto ours;
		}
		/*
		 * Now check if the destination ip address is the obvious
		 * broadcast address'es since it isn't multicast or one
		 * of the broadcast addresses we know about.
		 */
		if ((ip->ip_dst.s_addr == (__uint32_t)INADDR_BROADCAST) ||
		    (ip->ip_dst.s_addr == (__uint32_t)INADDR_ANY)) {
			goto ours;
		}

		/* Check if it's a multicast address */
		if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
			/*
			 * When we arrive here we aren't a member of this
			 * multicast address group since we would have found
			 * the address in first hash lookup done above.
			 */
			multicast_acceptit = 0;

			if (ip_mrouter) {
				/*
				 * We're acting as a multicast router, so all
				 * incoming multicast packets are passed to the
				 * kernel level multicast forwarding function.
				 * The packet is returned (relatively) intact;
				 * if ip_mforward() returns a non-zero value,
				 * the packet must be discarded otherwise it
				 * may be accepted below.
				 *
				 * The IP ident field is put in the same byte
				 * order as expected when ip_mforward() is
				 * called from ip_output().
				 */
multicast_forwarding:
				HTONS(ip->ip_id);
				if (ip_mforward(ip, m, ifp,
					    (struct ip_moptions *)NULL) != 0) {
					m_freem(m);
					_SESMGR_SOATTR_FREE(ipsec);
					IPSTAT(ips_cantforward);
					return;
				}
				NTOHS(ip->ip_id);
				/*
				 * The process-level routing demon needs to
				 * receive all multicast IGMP packets, whether
				 * or not this host belongs to their
				 * destination groups.
				 */
				if (ip->ip_p == IPPROTO_IGMP) {
					goto ours;
				}
				/*
				 * Increment the ip forwarding statistic
				 * since the multicast forwarding worked.
				 */
				 IPSTAT(ips_forward);
			} /* end of ip_mrouter case */

			/*
			 * If pkt dst addr was a member of a multicast group
			 * after multicast forwarding, accept the packet
			 * otherwise drop it.
			 */
			if (multicast_acceptit == 0) { /* drop it */
				m_freem(m);
				_SESMGR_SOATTR_FREE(ipsec);
				return;
			}
			goto ours;
		} /* end of ip_dst is multicast address case */
	}

	/*
	 * Check if ip filtering was ON and verdict was to accept packet
	 */
 	if (ipfilterflag && (verdict == IPF_GRABIT)) {
  		goto ours;
	}

	/*
	 * Not for us; forward if possible, desirable and enabled.
	 */
#pragma mips_frequency_hint NEVER
	NETPAR(NETFLOW, NETDROPTKN, ip->ip_id,
	       NETEVENT_IPUP, ntohs(ip->ip_len), NETRES_NOTOURS);
	ASSERT(ip == mtod(m, struct ip *));
	ip_forward(m, 0, ifp, ipsec, forward_rt);
	return;

ours:
	/*
	 * If offset or IP_MF are set, must reassemble.
	 * Otherwise, nothing need be done.
	 * (We could look in the reassembly queue to see
	 * if the packet was previously fragmented,
	 * but it's not worth the time; just let them time out.)
	 */
	if (ip->ip_off & (IP_MF|IP_OFFMASK)) {
		struct mbuf *mp;
		struct ipfrag_bucket *b;
#if _MIPS_SIM == _ABI64
		/* Ensure that there is enough safe room in front of ip. */
		m = ip_reass_adjm(m, ip);
		if (m == 0) {
#pragma mips_frequency_hint NEVER
			_SESMGR_SOATTR_FREE(ipsec);
			return;
		}
		ip = mtod(m, struct ip *);
#endif
		/*
		 * Look for queue of fragments of this datagram.
		 */
		b = ipfrag_qs + (ip->ip_id ^ ip->ip_src.s_addr) % 
		    FRAG_BUCKETS;
		mutex_lock(&b->ipfrag_lock, PZERO);
		for (fp = b->ipq.next; fp != &b->ipq; fp = fp->next) {
			if (ip->ip_id == fp->ipq_id &&
			    ip->ip_src.s_addr == fp->ipq_src.s_addr &&
			    ip->ip_dst.s_addr == fp->ipq_dst.s_addr &&
			    ip->ip_p == fp->ipq_p)
				break;
		}
		if (fp == &b->ipq)
			fp = 0;

		/*
		 * Adjust ip_len to not reflect header,
		 * set ip_mff if more fragments are expected,
		 * convert offset of this to bytes.
		 */
		ip->ip_len -= hlen;
		((struct ipasfrag *)ip)->ipf_mff = 
			(((struct ipasfrag *)ip)->ipf_mff & ~1)
			| ((ip->ip_off & IP_MF) != 0);
		ip->ip_off <<= 3;

		/*
		 * attempt reassembly; if it succeeds, proceed.
		 */
		IPSTAT(ips_fragments);

		ASSERT(ip == mtod(m, struct ip *));
		ip = ip_reass(m, fp, &mp, b);
		mutex_unlock(&b->ipfrag_lock);

		if (ip == 0) {
			_SESMGR_SOATTR_FREE(ipsec);
			return;
		}
		IPSTAT(ips_reassembled);
		m = mp;
		GOODMT(m->m_type);
	} else {
		ip->ip_len -= hlen;
	}

	/*
	 * Switch out to protocol's input routine.
	 */
	NETPAR(NETFLOW, NETFLOWTKN, ip->ip_id,
		 NETEVENT_IPUP, ip->ip_len, NETRES_NULL);
	IPSTAT(ips_delivered);
	GOODMT(m->m_type);

	/* Call protocol specific input procedure */
#ifdef INET6
	(*inetsw[ip_protox[ip->ip_p]].pr_input)(m, ifp, ipsec, 0);
#else
	(*inetsw[ip_protox[ip->ip_p]].pr_input)(m, ifp, ipsec);
#endif
}

#if _MIPS_SIM == _ABI64
/*
 * Adjust mbuf so that there is enough room in front of the ip header
 * for two aligned chaining pointers.  Ip headers are obtained by 
 * IF_DEQUEUEHEADER(&ipintrq, m, ifp).   There is usually dead space, 
 * which was used by ifheader, MAC header, etc, in the front. So this
 * routine is mostly a NOP.  It is rare, but possible, that there isn't
 * enough room, e.g, when a fragmented packet is delivered via loopback.
 * We allocate an extra mbuf in these rare cases. 
 */
struct mbuf*
ip_reass_adjm(struct mbuf *m, struct ip *ip)
{
	int bytes_needed;
	int hasroom;
	struct mbuf *m1;
#ifdef IPPRINTFS
	int old_len = m->m_len;
#endif

	bytes_needed = 2 * sizeof(void *) + (__uint64_t)ip % sizeof(void *);
	hasroom = m_hasroom(m, bytes_needed);

#ifndef IPTESTALLOC
	if (hasroom)
		return m;
#endif
	/* not enough free space in the front; get an extra mbuf */
	m1 = m_get(M_DONTWAIT, m->m_type);
	if (m1 == 0) {
#pragma mips_frequency_hint NEVER
		m_freem(m);
		return 0;
	}
	m1->m_off += 2 * sizeof(void*);
	m1->m_len = ip->ip_hl << 2;
	bcopy(mtod(m, void*), mtod(m1, void*), m1->m_len);
	m_adj(m, m1->m_len);
	m_cat(m1, m);
#ifdef IPPRINTFS
	if (ipprintfs)
		printf("ipadjm, n=%d, f0=%d, l0=%d, off1=%d, l1=%d\n",
		  bytes_needed, hasroom, old_len, m1->m_off, m1->m_len);
#endif
	return m1;
}
#endif

/*
 * Take incoming datagram fragment and try to
 * reassemble it into whole datagram.  If a chain for
 * reassembly of this datagram already exists, then it
 * is given as fp; otherwise have to make a chain.
 */
struct ip *
ip_reass(struct mbuf *m, struct ipq *fp, 
	 struct mbuf **mpp, struct ipfrag_bucket *b)
{
	register struct ipasfrag *ip = mtod(m, struct ipasfrag *);
	register struct ipasfrag *q;
	struct mbuf *t;
	int hlen = ip->ip_hl << 2;
	int i, next;

	/*
	 * Presence of header sizes in mbufs
	 * would confuse code below.
	 */
	m->m_off += hlen;
	m->m_len -= hlen;

	/*
	 * If first fragment to arrive, create a reassembly queue.
	 */
	if (fp == 0) {
		if (++b->ipfrag_count > ipfrag_limit) {
			/*
			 * If there are "too many" fragments already on the
			 * reassembly queue, drop an OLD one first.
			 */
#pragma mips_frequency_hint NEVER
			IPSTAT(ips_fragdropped);
			ip_freef(b->ipq.next->ipq_mbuf, b);
		}			
		if ((t = m_get(M_DONTWAIT, MT_FTABLE)) == NULL) {
#pragma mips_frequency_hint NEVER
			b->ipfrag_count--;
			goto dropfrag;
		}
#if _MIPS_SIM == _ABI64
		/* make room in the front for chaining pointers */
		t->m_off += 2 * sizeof(void*);
#endif
		fp = mtod(t, struct ipq *);
		insque(fp, &b->ipq);
		fp->ipq_ttl = IPFRAGTTL;
		fp->ipq_p = ip->ip_p;
		fp->ipq_id = ip->ip_id;
		IPQ_NEXT(fp) = IPQ_PREV(fp) = (struct ipasfrag *)fp;
		fp->ipq_src = ((struct ip *)ip)->ip_src;
		fp->ipq_dst = ((struct ip *)ip)->ip_dst;
		q = (struct ipasfrag *)fp;
		fp->ipq_mbuf = t;
		goto insert;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	t = fp->ipq_mbuf;
	ASSERT(t != 0);
	GOODMT(t->m_type);
	for (q = IPQ_NEXT(fp); q != (struct ipasfrag *)fp; q = IPF_NEXT(q)) {
		if (q->ip_off > ip->ip_off)
			break;
		t = t->m_act;
		ASSERT(t != 0);
		GOODMT(t->m_type);
	}

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (IPF_PREV(q) != (struct ipasfrag *)fp) {
		i = IPF_PREV(q)->ip_off +
		IPF_PREV(q)->ip_len - ip->ip_off;
		if (i > 0) {
			if (i >= ip->ip_len)
#pragma mips_frequency_hint NEVER
				goto dropfrag;
			m_adj(m, i);
			ip->ip_off += i;
			ip->ip_len -= i;
			NETPAR(NETFLOW, NETDROPTKN, ip->ip_id,
				NETEVENT_IPUP, i, NETRES_FRAG);
		}
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	while (q != (struct ipasfrag *)fp && ip->ip_off + ip->ip_len > q->ip_off) {
		i = (ip->ip_off + ip->ip_len) - q->ip_off;
		if (i < q->ip_len) {
			q->ip_len -= i;
			q->ip_off += i;
			m_adj(t->m_act, i);
			NETPAR(NETFLOW, NETDROPTKN, ip->ip_id,
				NETEVENT_IPUP, i, NETRES_FRAG);
			break;
		}
		q = IPF_NEXT(q);
		NETPAR(NETFLOW, NETDROPTKN, ip->ip_id,
			NETEVENT_IPUP,
			IPF_PREV(q)->ip_len,
			NETRES_FRAG);
		{
			register struct mbuf *tmp;

			/* dequeue must be done before m_free */
			ip_deq(IPF_PREV(q));
			/* remove from mbuf chain and free it */
			tmp = t->m_act;
			t->m_act = tmp->m_act;
			m_freem(tmp);
		}
	}

insert:
	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 */
	ip_enq(ip, IPF_PREV(q));
	ASSERT(m->m_act == 0);
	ASSERT(t != 0);
	GOODMT(t->m_type);
	m->m_act = t->m_act;	/* insert in mbuf chain too */
	t->m_act = m;
	next = 0;
	for (q = IPQ_NEXT(fp); q != (struct ipasfrag *)fp; q = IPF_NEXT(q)) {
		if (q->ip_off != next)
			return (0);
		next += q->ip_len;
	}
	if (IPF_PREV(q)->ipf_mff & 1)
		return (0);

	/*
	 * Reassembly is complete; concatenate fragments.
	 */
	m = fp->ipq_mbuf->m_act;	/* first fragment */
	t = m->m_next;
	if (t) {
		m->m_next = 0;
		m_cat(m, t);
	}
	t = m->m_act;
	m->m_act = 0;
	while (t) {
		struct mbuf *t2 = t->m_act;
		t->m_act = 0;		/* prevent receive 1 panic */
		m_cat(m, t);
		t = t2;			/* use value obtained before m_free */
	}

	/*
	 * Create header for new ip packet by
	 * modifying header of first packet;
	 * dequeue and discard fragment reassembly header.
	 * Make header visible.
	 */
	ip = IPQ_NEXT(fp);
	ip->ip_len = next;
	ip->ipf_mff &= ~1;
	((struct ip *)ip)->ip_src = fp->ipq_src;
	((struct ip *)ip)->ip_dst = fp->ipq_dst;
	remque(fp);
	b->ipfrag_count--;
	ASSERT(fp == mtod(fp->ipq_mbuf, struct ipq *));
	ASSERT(fp->ipq_mbuf->m_act == m);
	fp->ipq_mbuf->m_act = 0;	/* play safe */
	(void) m_free(fp->ipq_mbuf);
	/* m is pointing the first fragment already */
	GOODMT(m->m_type);
	m->m_len += (ip->ip_hl << 2);
	m->m_off -= (ip->ip_hl << 2);
	m->m_act = 0;	/* play safe */

	/*
	 * Stop the ping bomb.
	 */
	if (ip->ip_len > (IP_MAXPACKET - (ip->ip_hl << 2))) {
#pragma mips_frequency_hint NEVER
		while (m) {
			IPSTAT(ips_fragdropped);
			m = m_free(m);
		}
		return 0;
	}
	*mpp = m;
	return ((struct ip *)ip);

#pragma mips_frequency_hint NEVER
dropfrag:
	NETPAR(NETFLOW, NETDROPTKN, ip->ip_id,
		 NETEVENT_IPUP, ip->ip_len, NETRES_FRAG);
	IPSTAT(ips_fragdropped);
	m_freem(m);
	return (0);
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
static void
ip_freef(register struct mbuf *m, struct ipfrag_bucket *b)
{
	register struct mbuf *n;

	remque(mtod(m, struct ipq *));
	b->ipfrag_count--;
	while (m) {
		n = m->m_act;
		m_freem(m);
		m = n;
	}
}

/*
 * Put an ip fragment on a reassembly chain.
 * Like insque, but pointers in middle of structure.
 */
void
ip_enq(register struct ipasfrag *p, register struct ipasfrag *prev)
{
	IPF_PREV(p) = prev;
	IPF_NEXT(p) = IPF_NEXT(prev);
	IPF_PREV(IPF_NEXT(prev)) = p;
	IPF_NEXT(prev) = p;
}

/*
 * To ip_enq as remque is to insque.
 */
void
ip_deq(register struct ipasfrag *p)
{
	IPF_NEXT(IPF_PREV(p)) = IPF_NEXT(p);
	IPF_PREV(IPF_NEXT(p)) = IPF_PREV(p);
}

/*
 * IP timer processing;
 * if a timer expires on a reassembly queue, discard it.
 */
void
ip_slowtimo(void)
{
	register struct ipq *fp;
	int bucket;
	struct ipfrag_bucket *b;

	for (bucket = 0; bucket < FRAG_BUCKETS; bucket++) {
		b = ipfrag_qs + bucket;
		mutex_lock(&b->ipfrag_lock, PZERO);
		fp = b->ipq.next;
		if (fp == 0) {
			mutex_unlock(&b->ipfrag_lock);
			continue;
		}
		while (fp != &b->ipq) {
			--fp->ipq_ttl;
			fp = fp->next;
			if (fp->prev->ipq_ttl == 0) {
				IPSTAT(ips_fragtimeout);
				ASSERT(fp->prev == mtod(fp->prev->ipq_mbuf, 
							struct ipq *));
				ip_freef(fp->prev->ipq_mbuf, b);
			}
		}
		mutex_unlock(&b->ipfrag_lock);
	}
}

/*
 * Drain off all datagram fragments.
 */
void
ip_drain(void)
{
	int bucket;
	struct ipfrag_bucket *b;

	for (bucket = 0; bucket < FRAG_BUCKETS; bucket++) {
		b = ipfrag_qs + bucket;
		mutex_lock(&b->ipfrag_lock, PZERO);
		while (b->ipq.next != &b->ipq) {
			IPSTAT(ips_fragdropped);
			ASSERT(b->ipq.next == mtod(b->ipq.next->ipq_mbuf, 
						struct ipq *));
			ip_freef(b->ipq.next->ipq_mbuf, b);
		}
		mutex_unlock(&b->ipfrag_lock);
	}
}

/*
 * Do option processing on a datagram,
 * possibly discarding it if bad options are encountered,
 * or forwarding it if source-routed.
 * Returns 1 if packet has been forwarded/freed,
 * 0 if the packet should be processed further.
 */
int
ip_dooptions(struct mbuf *m, struct ifnet *ifp, 
	     struct ipsec *ipsec, struct route *forward_rt)
{
	register struct ip *ip = mtod(m, struct ip *);
	register u_char *cp;
	struct ip_timestamp *ipt;
	struct in_ifaddr *ia;
	struct ifaddr *ifa;
	int opt, optlen, cnt, off, code, type = ICMP_PARAMPROB, forward = 0;
	struct in_addr *sin;
	n_time ntime;
	struct sockaddr_in ipaddr = in_zeroaddr;

	cp = (u_char *)(ip + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= 0 || optlen > cnt) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
		}
		switch (opt) {
		default:
			break;

		/*
		 * Source routing with record.
		 * Find interface with current destination address.
		 * If none on this machine then drop if strictly routed,
		 * or do nothing if loosely routed.
		 * Record interface address and bring up next address
		 * component.  If strictly routed make sure next
		 * address on directly accessible net.
		 */
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			ipaddr.sin_addr = ip->ip_dst;
			ifa = ifa_ifwithaddr((struct sockaddr *)&ipaddr);
			if (ifa) {
			   ia = (struct in_ifaddr *)ifa->ifa_start_inifaddr;
			} else {
			   ia = (struct in_ifaddr *)0;
			}
			if (ia == 0) {
				if (opt == IPOPT_SSRR) {
					type = ICMP_UNREACH;
					code = ICMP_UNREACH_SRCFAIL;
					goto bad;
				}
				/*
				 * Loose routing, and not at next destination
				 * yet; nothing to do except forward.
				 */
				break;
			}
			off--;			/* 0 origin */
			if (off > optlen - sizeof(struct in_addr)) {
				/*
				 * End of source route.  Should be for us.
				 */
				break;
			}
			/*
			 * locate outgoing interface
			 */
			bcopy((caddr_t)(cp + off), (caddr_t)&ipaddr.sin_addr,
			    sizeof(ipaddr.sin_addr));

			if (opt == IPOPT_SSRR) {

			    ifa = ifa_ifwithdstaddr((SA)&ipaddr);

			    if (ifa == 0) {
				ifa = ifa_ifwithnet((SA)&ipaddr);
			    }
			    if (ifa) {
				ia=(struct in_ifaddr *)ifa->ifa_start_inifaddr;
			    } else {
				ia = (struct in_ifaddr *)0;
			    }
			} else {
				ia = ip_rtaddr(ipaddr.sin_addr, forward_rt);
			}
			if (ia == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}
			ip->ip_dst = ipaddr.sin_addr;
			bcopy((caddr_t)&(IA_SIN(ia)->sin_addr),
			    (caddr_t)(cp + off), sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			/* 
			 * Let ip_intr's mcast routing check handle mcast pkts
			 */
			forward = !IN_MULTICAST(ntohl(ip->ip_dst.s_addr));
			break;

		case IPOPT_RR:
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			/*
			 * If no space remains, ignore.
			 */
			off--;			/* 0 origin */
			if (off > optlen - sizeof(struct in_addr))
				break;
			bcopy((caddr_t)(&ip->ip_dst),
			      (caddr_t)&ipaddr.sin_addr,
			      sizeof(ipaddr.sin_addr));
			/*
			 * locate outgoing interface; if we're the destination,
			 * use the incoming interface (should be same).
			 */
			ifa = ifa_ifwithaddr((SA)&ipaddr);
			if (ifa == 0) {
				ia = ip_rtaddr(ipaddr.sin_addr, forward_rt);
				if (ia == 0) {
					type = ICMP_UNREACH;
					code = ICMP_UNREACH_HOST;
					goto bad;
				}
			} else {
				ia=(struct in_ifaddr *)ifa->ifa_start_inifaddr;
			}

			bcopy((caddr_t)&(IA_SIN(ia)->sin_addr),
			    (caddr_t)(cp + off), sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			break;

		case IPOPT_TS:
			code = cp - (u_char *)ip;
			ipt = (struct ip_timestamp *)cp;
			if (ipt->ipt_len < 5)
				goto bad;
			if (ipt->ipt_ptr > ipt->ipt_len - sizeof (__int32_t)) {
				if (++ipt->ipt_oflw == 0)
					goto bad;
				break;
			}
			sin = (struct in_addr *)(cp + ipt->ipt_ptr - 1);
			switch (ipt->ipt_flg) {

			case IPOPT_TS_TSONLY:
				break;

			case IPOPT_TS_TSANDADDR:
				if (ipt->ipt_ptr + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;
				ia = ifptoia(ifp);
				bcopy((caddr_t)&IA_SIN(ia)->sin_addr,
				    (caddr_t)sin, sizeof(struct in_addr));
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			case IPOPT_TS_PRESPEC:
				if (ipt->ipt_ptr + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;
				bcopy((caddr_t)sin, (caddr_t)&ipaddr.sin_addr,
				    sizeof(struct in_addr));
				if (ifa_ifwithaddr((struct sockaddr *)&ipaddr) == 0)
					continue;
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			default:
				goto bad;
			}
			ntime = iptime();
			bcopy((caddr_t)&ntime, (caddr_t)cp + ipt->ipt_ptr - 1,
			    sizeof(n_time));
			ipt->ipt_ptr += sizeof(n_time);
		}
	}
	if (forward) {
	    	ip_forward(m, 1, ifp, ipsec, forward_rt);
		return (1);
	}
	return (0);
bad:
	ASSERT(ip == mtod(m, struct ip *));
	ip->ip_len -= ip->ip_hl << 2;   /* icmp_error adds in header length */
#ifdef sgi
	{
	struct in_addr dest = {0};
	icmp_error(m, type, code, ifp, dest, NULL, ipsec);
	}
#else
	icmp_error(m, type, code, ifp, 0, NULL);
#endif
	IPSTAT(ips_badoptions);
	return (1);
}

/*
 * Given address of next destination (final or next hop),
 * return internet address info of interface to be used to get there.
 */
struct in_ifaddr *
ip_rtaddr(struct in_addr dst, struct route *forward_rt)
{
	register struct sockaddr_in *sin;
	register struct in_ifaddr *ia;
	struct ifnet *ifp;
	sin = (struct sockaddr_in *) &forward_rt->ro_dst;

	ROUTE_RDLOCK();
	if (forward_rt->ro_rt == 0 || dst.s_addr != sin->sin_addr.s_addr) {
		if (forward_rt->ro_rt) {
			rtfree_needpromote(forward_rt->ro_rt);
			forward_rt->ro_rt = 0;
		}
		sin->sin_family = AF_INET;
		sin->sin_addr = dst;
		rtalloc(forward_rt);
	}
	if (forward_rt->ro_rt == 0) {
		ROUTE_UNLOCK();
		return ((struct in_ifaddr *)0);
	}

	/*
	 * Find address associated with outgoing interface.
	 * First check the primary address for this interface.
	 * If NULL then look for any address associated with the interface.
	 */
	if ((ifp = forward_rt->ro_rt->rt_ifp)) {
		ia = (struct in_ifaddr *)(ifp->in_ifaddr);
		if (ia == 0) {
			/*
			 * If primary address of interface NOT set then
			 * enumerate all addresses looking for any address
			 * associated with this interface.
			 */
			ia = (struct in_ifaddr *)hash_enum(&hashinfo_inaddr,
				ifnet_enummatch,
				HTF_INET,
				(caddr_t)0, /* key */
				(caddr_t)ifp,
				(caddr_t)0);

			if (ia == 0) { /* find any address for the interface */

				ia = (struct in_ifaddr *)hash_enum(
					&hashinfo_inaddr,
					ifnet_any_enummatch,
					HTF_INET,
					(caddr_t)0, /* key */
					(caddr_t)ifp,
					(caddr_t)0);	
			}
		}
	}
	ROUTE_UNLOCK();
	return ia;
}

/*
 * Strip out IP options, at higher
 * level protocol in the kernel.
 * Second argument is buffer to which options
 * will be moved, and return value is their length.
 */
/*ARGSUSED*/
void
ip_stripoptions(struct mbuf *m, struct mbuf *mopt)
{
	register int i;
	struct ip *ip = mtod(m, struct ip *);
	register caddr_t opts;
	int olen;

	olen = (ip->ip_hl<<2) - sizeof (struct ip);
	opts = (caddr_t)(ip + 1);
	i = m->m_len - (sizeof (struct ip) + olen);
	bcopy(opts  + olen, opts, (unsigned)i);
	m->m_len -= olen;
	ip->ip_hl = sizeof(struct ip) >> 2;
}

/*
 * hash enumeration match procedure returns non-zero if the address
 * in 'key' after AND'ing with the network mask matches the net number
 * and the subnetmask differs from the netmask of this entry.
 *
 * ip_forward_enummatch(struct hashbucket *h, struct sockaddr_in *key,
 *  struct ifnet *arg1, caddr_t arg2)
 *
 * NOTE: This procedure is called with only hash bucket entries of the
 * specified type on the enumerate call. Here that is HTF_INET types.
 */
/* ARGSUSED */
int
ip_forward_enummatch(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	unsigned long dst = (unsigned long)key;
	int match = 0;

	if ((dst & ia->ia_netmask) == ia->ia_net) {
		if (ia->ia_subnetmask != ia->ia_netmask) {
			match = 1;
		}
	}
	return match;
}

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding (possibly because we have only a single external
 * network), just drop the packet.  This could be confusing if ipforwarding
 * was zero but some routing protocol was advancing us as a gateway
 * to somewhere.  However, we must let the routing protocol deal with that.
 *
 * The srcrt parameter indicates whether the packet is being forwarded
 * via a source route.
 */
static void
ip_forward(struct mbuf *m, int srcrt, struct ifnet *ifp, 
	   struct ipsec *ipsec, struct route *forward_rt)
{
	register struct ip *ip = mtod(m, struct ip *);
	register struct sockaddr_in *sin;
	register struct rtentry *rt;
	register int error, type = 0, code;
	struct mbuf *mcopy;
	struct in_addr dest;
	struct ifnet *destifp;


	dest.s_addr = 0;
#ifdef IPPRINTFS
	if (ipprintfs)
		printf("forward: src %x dst %x ttl %x\n", ip->ip_src.s_addr,
			ip->ip_dst.s_addr, ip->ip_ttl);
#endif
	if (m->m_flags & (M_BCAST|M_MCAST) || !in_canforward(ip->ip_dst)) {
		IPSTAT(ips_cantforward);
		m_freem(m);
		_SESMGR_SOATTR_FREE(ipsec);
		return;
	}
	if (ipforwarding == 0 || (!srcrt && in_interfaces <= 1) ||
	    (srcrt && (ipforwarding == 2))) {
		IPSTAT(ips_cantforward);
		m_freem(m);
		_SESMGR_SOATTR_FREE(ipsec);
		return;
	}
 	HTONS(ip->ip_id);
	if (ip->ip_ttl <= IPTTLDEC) {
		icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, ifp, dest,
				NULL, ipsec);
		return;
	}
	ip->ip_ttl -= IPTTLDEC;

#ifdef	EVEREST
	if (io4ia_war)
		m_io4iawar(m);
#endif

	/*
	 * The interface should not bother recomputing the checksum.
	 */
	m->m_flags &= ~M_CKSUMMED;
	sin = (struct sockaddr_in *)&forward_rt->ro_dst;
	ROUTE_RDLOCK();
	if ((rt = forward_rt->ro_rt) == 0 ||
	    ip->ip_dst.s_addr != sin->sin_addr.s_addr) {
		if (forward_rt->ro_rt) {
			rtfree_needpromote(forward_rt->ro_rt);
			forward_rt->ro_rt = 0;
		}
		sin->sin_family = AF_INET;
#ifdef _HAVE_SIN_LEN
		sin->sin_len = sizeof(*sin);
#endif
		sin->sin_addr = ip->ip_dst;
		rtalloc(forward_rt);
		if (forward_rt->ro_rt == 0) {
			ROUTE_UNLOCK();
			if (in_localaddr(ip->ip_dst))
			        code = ICMP_UNREACH_HOST;
			else
			        code = ICMP_UNREACH_NET;
			icmp_error(m, ICMP_UNREACH, code, ifp,
				dest, NULL, ipsec);
			return;
		}
		rt = forward_rt->ro_rt;
	}

	/*
	 * If forwarding packet using same interface that it came in on,
	 * perhaps should send a redirect to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a default route
	 * or a route modfied by a redirect.
	 */
	if (rt->rt_ifp == ifp &&
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0 &&
	    satosin(rt_key(rt))->sin_addr.s_addr != 0 &&
	    ipsendredirects && !srcrt) {
		struct in_ifaddr *ia;

		u_long src = ntohl(ip->ip_src.s_addr);
		u_long dst = ntohl(ip->ip_dst.s_addr);

		if ((ia = ifptoia(ifp)) &&
		   (src & ia->ia_subnetmask) == ia->ia_subnet) {

		    if (rt->rt_flags & RTF_GATEWAY)
			dest = satosin(rt->rt_gateway)->sin_addr;
		    else
			dest = ip->ip_dst;
		    /*
		     * If the destination is reached by a route to host,
		     * is on a subnet of a local net, or is directly
		     * on the attached net (!), use host redirect.
		     * (We may be the correct first hop for other subnets.)
		     */
		    type = ICMP_REDIRECT;
		    code = ICMP_REDIRECT_NET;
			if ((rt->rt_flags & RTF_HOST) ||
				(rt->rt_flags & RTF_GATEWAY) == 0) {
				code = ICMP_REDIRECT_HOST;
			} else {
				ia = (struct in_ifaddr *)hash_enum(
					&hashinfo_inaddr,
					ip_forward_enummatch,
					HTF_INET,
					(caddr_t)dst, (caddr_t)0, (caddr_t)0);
				
				if (ia) {
					code = ICMP_REDIRECT_HOST;
				}
			}
#ifdef IPPRINTFS
		    if (ipprintfs)
		        printf("redirect (%d) to %x\n", code, dest.s_addr);
#endif
		}
	}

	ROUTE_UNLOCK();

	/*
	 * Save at most 64 bytes of the packet in case
	 * we need to generate an ICMP message to the src.
	 */
	mcopy = m_copy(m, 0, imin((int)ip->ip_len, 64));

	error = ip_output(m, (struct mbuf *)0, forward_rt,
			IP_FORWARDING, (struct mbuf *)NULL, ipsec);
	if (error)
		IPSTAT(ips_cantforward);
	else {
		IPSTAT(ips_forward);
		if (type)
			IPSTAT(ips_redirectsent);
		else {
			_SESMGR_SOATTR_FREE(ipsec);
			if (mcopy)
				m_freem(mcopy);
			return;
		}
	}
	if (mcopy == NULL) {
		_SESMGR_SOATTR_FREE(ipsec);
		return;
	}
	ip = mtod(mcopy, struct ip *);
	destifp = NULL;
	switch (error) {

	case 0:				/* forwarded, but need redirect */
		type = ICMP_REDIRECT;
		/* code set above */
		break;

	case ENETUNREACH:
	case ENETDOWN:
		type = ICMP_UNREACH;
		if (in_localaddr(ip->ip_dst))
			code = ICMP_UNREACH_HOST;
		else
			code = ICMP_UNREACH_NET;
		break;

	case EHOSTDOWN:
	case EHOSTUNREACH:
	default:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_HOST;
		break;

	case EMSGSIZE:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_NEEDFRAG;
		IPSTAT(ips_cantfrag);
		if (forward_rt->ro_rt)
			destifp = forward_rt->ro_rt->rt_ifp;
		break;

	case ENOBUFS:
		type = ICMP_SOURCEQUENCH;
		code = 0;
		break;
	}
	icmp_error(mcopy, type, code, ifp, dest, destifp, ipsec);
	return;
}
