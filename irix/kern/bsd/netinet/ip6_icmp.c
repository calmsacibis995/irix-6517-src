#ifdef INET6
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

#include <tcp-param.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/hashing.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in6_var.h>
#include <netinet/if_ether.h>
#include <netinet/if_ndp6.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip6_opts.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6_icmp.h>
#include <netinet/icmp6_var.h>
#include <netinet/igmp.h>
#include <netinet/igmp_var.h>
#include <sys/tcpipstats.h>

/*
 * ICMPv6 routines: error generation, receive packet processing, and
 * routines to turnaround packets back to the originator, and
 * host table maintenance routines.
 */

u_int	icmp6_ipsec = 0;
#ifdef IP6PRINTFS
extern	int icmpprintfs;
#endif
extern	int arpt_keep;

#define rt_expire	rt_rmx.rmx_expire
#define rt_mtu		rt_rmx.rmx_mtu

#define IGMP_TIMER_SCALE	10
static int icmp6_timers_are_running = 0;
extern struct in6_addr allnodes6_group;
static void icmp6_sendgroup __P((struct in6_multi *, int, struct mbuf **));

extern	int ip6forwarding;
extern	struct protosw inet6sw[];
int	last_icmp6_error_time, last_icmp6_error_count;

extern struct sockaddr_in6 in6_zeroaddr;

static struct mbuf *grp_opts = NULL;

#define MADDR6_REPORTABLE(addr) \
	((MADDR6_SCOPE(addr) > MADDR6_SCP_LINK) || \
	 ((MADDR6_SCOPE(addr) == MADDR6_SCP_LINK) && \
	  (MADDR6_FLAGS(addr) & MADDR6_FLG_TS)))

void
icmp6_init()
{
#ifdef OPT6_ROUTER_ALERT
	register struct mbuf *m;
	register struct ipv6_fraghdr *hp;
	register struct opt6_ra *op;

	MGETHDR(m, M_WAIT, MT_SOOPTS);
	if (m == NULL)
		panic("icmp6_init");
	MH_ALIGN(m, sizeof(*hp));
	m->m_pkthdr.pktype = IP6_NHDR_HOP;
	m->m_pkthdr.len = m->m_len = sizeof(*hp);

	hp = mtod(m, struct ipv6_fraghdr *);
	bzero(hp, sizeof(*hp));
	hp->ih6_hlen = 0;

	op = (struct opt6_ra *)((caddr_t)hp + 2);
	op->ra_ext = OPT6_ROUTER_ALERT;
	op->ra_len = 2;
	op->ra_code = htons(OPT6_RA_GROUP);

	grp_opts = m;
#endif
	/* don't begin with errors ... */

	last_icmp6_error_time = time;
	last_icmp6_error_count = ICMP6_MAX_RATE;
}

void
icmp6_joingroup(inm)
	struct in6_multi *inm;
{
#ifdef IP6PRINTFS
	if (ip6printfs & D6_MCASTOUT)
		printf("icmp6_joingroup %s\n", ip6_sprintf(&inm->inm6_addr));
#endif
	inm->inm6_state = MULTI6_IDLE_MEMBER;

	if (MADDR6_REPORTABLE(inm->inm6_addr) &&
	    (inm->inm6_ifp->if_flags & IFF_LOOPBACK) == 0) {
		icmp6_sendgroup(inm, ICMP6_GROUPMEM_REPORT, 0);
		inm->inm6_state = MULTI6_DELAYING_MEMBER;
		inm->inm6_timer = ICMP6_DEFAULT_DELAY();
		icmp6_timers_are_running = 1;
	} else
		inm->inm6_timer = 0;
}

void
icmp6_leavegroup(inm)
	struct in6_multi *inm;
{
#ifdef IP6PRINTFS
	if (ip6printfs & D6_MCASTOUT)
		printf("icmp6_leavegroup %s\n", ip6_sprintf(&inm->inm6_addr));
#endif

	if ((inm->inm6_state != MULTI6_LAZY_MEMBER) &&
	    MADDR6_REPORTABLE(inm->inm6_addr) &&
	    ((inm->inm6_ifp->if_flags & IFF_LOOPBACK) == 0))
		icmp6_sendgroup(inm, ICMP6_GROUPMEM_TERM, 0);
}

/* ARGSUSED */
static int
icmp6_fasttimer_enummatch(struct hashbucket *h, caddr_t key,
  caddr_t arg1, caddr_t arg2)
{
	struct in6_multi *inm = (struct in6_multi *)h;
	struct mbuf **queue = (struct mbuf **)arg1;

	if (inm->inm6_timer == 0) {
		/* do nothing */
	} else if (--inm->inm6_timer == 0) {
		if (inm->inm6_state == MULTI6_DELAYING_MEMBER) {
			icmp6_sendgroup(inm, ICMP6_GROUPMEM_REPORT, queue);
			inm->inm6_state = MULTI6_IDLE_MEMBER;
		}
	} else {
		icmp6_timers_are_running = 1;
	}
	return (0);
}

/*
 * We can't call icmp6_send from inside icmp6_fasttimer_enummatch because
 * ip6_output will grab the same hash lock that was held when
 * icmp6_fasttimer_enummatch was called so we must pass a queue in for
 * icmp6_sendgroup to use and then call icmp6_send on each queued item.
 */
void
icmp6_fasttimo()
{
	struct mbuf *queue;
	struct mbuf *m, *mnext;
	register struct ip_moptions *imo;

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */
	if (!icmp6_timers_are_running)
		return;

	icmp6_timers_are_running = 0;

	queue = NULL;
	(void)hash_enum(&hashinfo_in6addr, icmp6_fasttimer_enummatch,
		HTF_MULTICAST, (caddr_t)0, (caddr_t)&queue, (caddr_t)0);
	for (m = queue; m; m = mnext) {
		imo = (struct ip_moptions *)m->m_act;
		mnext = m->m_next;
		m->m_act = m->m_next = NULL;
		icmp6_send(m, grp_opts, imo);
		kmem_free(imo, sizeof(*imo));
	}
}

static void
icmp6_sendgroup(inm, type, queue)
	register struct in6_multi *inm;
	int type;
	struct mbuf **queue;
{
	register struct mbuf *m;
	register struct icmpv6 *icp;
	register struct ipv6 *ip;
	register struct ip_moptions *imo;
	register struct in6_ifaddr *ia;
	struct sockaddr_in6 icmpdst = in6_zeroaddr;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_MCASTOUT)
		printf("icmp6_sendgroup %s %d\n",
		       ip6_sprintf(&inm->inm6_addr), type);
#endif
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return;
	imo = (struct ip_moptions*)kmem_alloc(sizeof(*imo), KM_NOSLEEP);
	if (imo == NULL) {
		m_free(m);
		return;
	}
	m->m_len = sizeof(struct ipv6) + ICMP6_GRPLEN;
	MH_ALIGN(m, m->m_len);
	bzero(mtod(m, caddr_t), m->m_len);

	ip = mtod(m, struct ipv6 *);
	ip->ip6_head = IPV6_VERSION;
	ip->ip6_len = ICMP6_GRPLEN;
	ip->ip6_nh = IPPROTO_ICMPV6;
	ip->ip6_hlim = 1;
	COPY_ADDR6(inm->inm6_addr, icmpdst.sin6_addr);
	ia = (struct in6_ifaddr *)ifaof_ifpforaddr(
				sin6tosa(&icmpdst), inm->inm6_ifp);
	if (ia == NULL) {
		log(LOG_ERR, "icmp6_sendgroup: bad address\n");
		m_free(m);
		kmem_free(imo, sizeof(*imo));
	}
	COPY_ADDR6(ia->ia_addr.sin6_addr, ip->ip6_src);
	COPY_ADDR6(inm->inm6_addr, ip->ip6_dst);

	icp = (struct icmpv6 *)(ip + 1);
	icp->icmp6_type = type;
	COPY_ADDR6(inm->inm6_addr, icp->icmp6_grp);

	imo->imo_multicast_ifp = inm->inm6_ifp;
	imo->imo_multicast_ttl = 1;
        /*
         * Request loopback of the report if we are acting as a multicast
         * router, so that the process-level routing demon can hear it.
         */
	imo->imo_multicast_loop = (ip6_mrouter != NULL);

	/*
	 * We queue it when called by icmp6_fasttimer_enummatch because
	 * icmp6_send calls ip6_output which will grab the hash lock
	 * that was held when icmp6_fasttimer_enummatch was called causing
	 * a deadlock.  icmp6_fasttimer will call icmp6_send after the
	 * hash_enum.
	 */
	if (queue) {
		m->m_act = (struct mbuf *)imo;
		m->m_next = *queue;
		*queue = m;
	} else {
		icmp6_send(m, grp_opts, imo);
		kmem_free(imo, sizeof(*imo));
	}

	switch (type) {
	case ICMP6_GROUPMEM_QUERY:
		ICMP6STAT(icp6s_snd_grpqry);
		break;

	case ICMP6_GROUPMEM_REPORT:
		ICMP6STAT(icp6s_snd_grprep);
		break;

	case ICMP6_GROUPMEM_TERM:
		ICMP6STAT(icp6s_snd_grpterm);
		break;
	}
}

/*
 * Generate a parameter problem, bad next header error.
 * 'off' is the offset into the packet (pointed at by m) where the error
 * exists.  When we add the options back into the packet we have to
 * adjust 'off' for the additional size of the options.
 */
void
icmp6_errparam(m, opts, off, ifp, code)
	struct mbuf *m, *opts;
	int off;
	struct ifnet *ifp;
	int code;
{
	int olen;
	struct mbuf *o;

	olen = 0;
	for (o = opts; o != NULL; o = o->m_next) {
		olen += OPT6_LEN(o);
		m = ip6_insertoption(m, o, 0, IP6_INSOPT_RAW);
		if (m == NULL) {
			m_freem(opts);
			return;
		}
	}
	if (off >= sizeof(struct ipv6)) {
		/*
		 * The offset is pointing somewhere after the ipv6 header.
		 * When we inserted the options above, we placed them right
		 * after the ipv6 header so we need to add their length to
		 * off since off is now further back in the packet.
		 */
		off += olen;
	}
	icmp6_error(m, ICMP6_PARAMPROB, code, (caddr_t)((long)off), ifp);
	if (opts)
		m_freem(opts);
}

/*
 * Generate an error packet of type error
 * in response to bad packet IPv6.
 */
void
icmp6_error(n, type, code, arg, ifp)
	struct mbuf *n;
	int type, code;
	caddr_t arg;
	struct ifnet *ifp;
{
	register struct ipv6 *oip = mtod(n, struct ipv6 *), *nip;
	register struct icmpv6 *icp;
	register struct mbuf *m;
	int len;

#ifdef IP6PRINTFS
	if (icmpprintfs || (ip6printfs & D6_CTLOUT))
		printf("icmp6_error(%p, %d, %d, %p)\n",
		       oip, type, code, arg);
#endif
	ICMP6STAT(icp6s_error);

	/*
	 * Don't send error in response to packet
	 * without an identified source or
	 * a multicast if it is not for path MTU discovery.
	 */
	if (IS_MULTIADDR6(oip->ip6_src) ||
	    IS_ANYADDR6(oip->ip6_src) ||
	    in6_isanycast(&oip->ip6_src) ||
	    (type != ICMP6_PKTTOOBIG &&
	     (n->m_flags & (M_BCAST|M_MCAST) || IS_MULTIADDR6(oip->ip6_dst))))
		goto freeit;
	/*
	 * Don't error if the old packet protocol was ICMPv6 error.
	 */
	if ((oip->ip6_nh == IPPROTO_ICMPV6) &&
	    ((oip->ip6_len < ICMP6_MINLEN) ||
	     (n->m_len < sizeof(struct ipv6) + sizeof(u_int8_t)) ||
	     !ICMP6_INFOTYPE(((struct icmpv6 *)(oip + 1))->icmp6_type))) {
		ICMP6STAT(icp6s_oldicmp);
		goto freeit;
	}
	/*
	 * Limit rate of icmp6 errors.
	 */
	if (last_icmp6_error_time == time) {
		if (last_icmp6_error_count++ > ICMP6_MAX_RATE) {
			ICMP6STAT(icp6s_ratelim);
			goto freeit;
		}
	} else {
		last_icmp6_error_time = time;
		last_icmp6_error_count = 1;
	}

	len = 0;
	for (m = n; m; m = m->m_next)
		len += m->m_len;
	/*
	 * First, formulate ICMPv6 message
	 */
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		goto freeit;
	m->m_len = sizeof(struct ipv6) + ICMP6_MINLEN;
	MH_ALIGN(m, m->m_len);
	m->m_next = n;
	len += m->m_len;
	if (len > IP6_MMTU) {
		m_adj(m, IP6_MMTU - len);
		len = IP6_MMTU;
	}
	nip = mtod(m, struct ipv6 *);
	icp = (struct icmpv6 *)(nip + 1);
	icp->icmp6_type = type;
	switch (type) {
	case ICMP6_PKTTOOBIG:
		if (arg == NULL)
			panic("icmpv6 pmtu");
		icp->icmp6_pmtu = htonl(((struct ifnet *)arg)->if_mtu);
		ICMP6STAT(icp6s_snd_pkttoobig);
		break;

	case ICMP6_PARAMPROB:
		/*
		 * arg is an offset into the packet so the upper 32 bits
		 * can safely be discarded.  The double cast keeps the
		 * compiler happy.
		 */
		icp->icmp6_pptr = htonl((u_int32_t)((long)arg));
		ICMP6STAT(icp6s_snd_paramprob);
		break;

	case ICMP6_TIMXCEED:
		ICMP6STAT(icp6s_snd_timxceed);
		icp->icmp6_pmtu = 0;
		break;

	case ICMP6_UNREACH:
		ICMP6STAT(icp6s_snd_unreach);
	default:
		icp->icmp6_pmtu = 0;
		break;
	}
	icp->icmp6_code = code;
	oip->ip6_len = htons(oip->ip6_len);

	/*
	 * Now, copy old IPv6 header in front of ICMPv6 message.
	 */
	bcopy((caddr_t)oip, (caddr_t)nip, sizeof(struct ipv6));
	nip->ip6_head = IPV6_VERSION;
	nip->ip6_len = len - sizeof(struct ipv6);
	nip->ip6_nh = IPPROTO_ICMPV6;
	icmp6_reflect(m, ifp, 0);
	return;

freeit:
	m_freem(n);
}

static int
icmp6_checktimer_enummatch(struct hashbucket *h, caddr_t key,
        caddr_t arg1, caddr_t arg2)
{
	struct in6_multi *inm = (struct in6_multi *)h;
	struct ipv6 *ip = (struct ipv6 *)key;
	struct ifnet *ifp = (struct ifnet *)arg1;
	register struct icmpv6 *icp = (struct icmpv6 *)arg2;
	int timer;

	timer = ICMP6_TIMER2HZ(icp->icmp6_mrd);
	if (inm->inm6_ifp == ifp &&
	    MADDR6_REPORTABLE(inm->inm6_addr) &&
	    (SAME_ADDR6(ip->ip6_dst, allnodes6_group) ||
	     SAME_ADDR6(ip->ip6_dst, inm->inm6_addr))) {
		switch (inm->inm6_state) {
		case MULTI6_DELAYING_MEMBER:
			if (inm->inm6_timer <= timer)
				break;
			/* FALLTHROUGH */
		case MULTI6_IDLE_MEMBER:
		case MULTI6_LAZY_MEMBER:
			inm->inm6_state =
			    MULTI6_DELAYING_MEMBER;
			inm->inm6_timer =
			    ICMP6_RANDOM_DELAY(timer);
			icmp6_timers_are_running = 1;
			break;
		}
	}
	/* return zero so all multicast entries will be enumerated */
	return (0);
}

/*
 * Process a received ICMPv6 message.
 */
/* ARGSUSED */
void
icmp6_input(
	register struct mbuf *m,
	struct ifnet *ifp,
	struct ipsec *ipsec,
	struct mbuf *opts)
{
	register struct icmpv6 *icp;
	register struct ipv6 *ip = mtod(m, struct ipv6 *);
	int icmplen = ip->ip6_len;
	register int i;
	struct rtentry *rt;
	struct in6_multi *inm;
	void (*ctlfunc) __P((int, struct sockaddr *, void *));
	struct ctli_arg arg;
	int code;
	struct in6_ifaddr *ia;
	extern u_char ip6_protox[];
	struct sockaddr_in6 icmpsrc = in6_zeroaddr;

	/*
	 * Locate ICMPv6 structure in mbuf, and check
	 * that not corrupted and of at least minimum length.
	 */
#ifdef IP6PRINTFS
	if (icmpprintfs || (ip6printfs & D6_CTLIN))
		printf("icmp6_input from %s to %s, len %d\n",
		       ip6_sprintf(&ip->ip6_src),
		       ip6_sprintf(&ip->ip6_dst),
		       icmplen);
#endif
	if (icmplen < ICMP6_MINLEN) {
		ICMP6STAT(icp6s_tooshort);
		goto freeit;
	}
	i = min(sizeof(struct ipv6) + icmplen, MLEN);
	if (m->m_len < i && (m = m_pullup(m, i)) == 0)  {
		ICMP6STAT(icp6s_tooshort);
		if (opts)
			m_freem(opts);
		return;
	}
	ip = mtod(m, struct ipv6 *);
	icp = (struct icmpv6 *)(ip + 1);
	{
		register struct ip6ovck *ipc = (struct ip6ovck *)ip;
		struct ip6ovck save_ip = *ipc;

		ipc->ih6_wrd1 = ipc->ih6_wrd0 = 0;
		ipc->ih6_len = htons(icmplen);
		ipc->ih6_pr = IPPROTO_ICMPV6;
		if (in_cksum(m, icmplen + sizeof(*ip))) {
			ICMP6STAT(icp6s_checksum);
			goto freeit;
		}
		*ipc = save_ip;
	}

#ifdef IP6PRINTFS
	if (icmpprintfs || (ip6printfs & D6_CTLIN))
		printf("icmp6_input type %d code %d\n",
		       icp->icmp6_type, icp->icmp6_code);
#endif
	/*
	 * Message type specific processing.
	 */
	code = icp->icmp6_code;
	if (IS_ANYADDR6(ip->ip6_src) &&
	    (icp->icmp6_type != ICMP6_SOLICITATION_ND) &&
	    (icp->icmp6_type != ICMP6_SOLICITATION_RT)) {
		log(LOG_INFO, "icmp6_input: unspec source\n");
		goto freeit;
	}

	switch (icp->icmp6_type) {

	case ICMP6_UNREACH:
		if ((icmp6_ipsec & ICMP6SEC_ERRORS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		ICMP6STAT(icp6s_rcv_unreach);
		switch (code) {
		case ICMP6_UNREACH_NOROUTE:
			code = PRC_UNREACH_NET;
			break;

		case ICMP6_UNREACH_ADMIN:
			code = PRC_UNREACH_HOST;
			break;

		case ICMP6_UNREACH_RTFAIL:
			code = PRC_UNREACH_SRCFAIL;
			break;

		case ICMP6_UNREACH_ADDRESS:
			code = PRC_UNREACH_HOST;
			break;

		case ICMP6_UNREACH_PORT:
			code = PRC_UNREACH_PORT;
			break;

		default:
			goto badcode;
		}
		goto deliver;

	case ICMP6_PKTTOOBIG:
		if ((icmp6_ipsec & ICMP6SEC_ERRORS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		ICMP6STAT(icp6s_rcv_pkttoobig);
		code = PRC_MSGSIZE;
		ROUTE_RDLOCK();
		rt = in6_rthost(&icp->icmp6_ip.ip6_dst);
		if (rt == 0) {
			log(LOG_NOTICE,
			    "icmp6_input/pkttoobig: in6_rthost failed\n");
			goto deliver;
		}
		if ((rt->rt_mtu > rt->rt_ifp->if_mtu) ||
		    (ntohl(icp->icmp6_pmtu) > rt->rt_mtu) ||
		    (ntohl(icp->icmp6_pmtu) < IP6_MMTU)) {
			/* note: this can happen with real path */
			log(LOG_NOTICE,
			    "icmp6_input/pkttoobig: bad MTU\n");
			goto freeit;
		}
		if ((rt->rt_flags & RTF_HOST) == 0) {
			log(LOG_NOTICE,
			    "icmp6_input/pkttoobig: in6_rthost doesn't work\n");
				goto freeit;
		}
		if ((ntohl(icp->icmp6_pmtu) < rt->rt_mtu)
		     /* || ((rt->rt_rmx.rmx_locks & RTV_MTU) == 0) */)
			rt->rt_mtu = ntohl(icp->icmp6_pmtu);
		ROUTE_UNLOCK();
		goto deliver;
				
	case ICMP6_TIMXCEED:
		if ((icmp6_ipsec & ICMP6SEC_ERRORS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		ICMP6STAT(icp6s_rcv_timxceed);
		if ((code != ICMP6_TIMXCEED_INTRANS) &&
		    (code != ICMP6_TIMXCEED_REASS))
			goto badcode;
		code += PRC_TIMXCEED_INTRANS;
		goto deliver;

	case ICMP6_PARAMPROB:
		if ((icmp6_ipsec & ICMP6SEC_ERRORS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		ICMP6STAT(icp6s_rcv_paramprob);
		if (code == ICMP6_PARAMPROB_NH)
			code = PRC_UNREACH_PROTOCOL;
		else
			code = PRC_PARAMPROB;
		goto deliver;

	deliver:
		/*
		 * Problem with datagram; advise higher level routines.
		 */
		NTOHS(icp->icmp6_ip.ip6_len);
#ifdef IP6PRINTFS
		if (icmpprintfs || (ip6printfs & D6_CTLIN))
			printf("deliver to IPv6 protocol %d\n",
			       icp->icmp6_ip.ip6_nh);
#endif
		COPY_ADDR6(icp->icmp6_ip.ip6_dst, icmpsrc.sin6_addr);
		ctlfunc = inet6sw[ip6_protox[icp->icmp6_ip.ip6_nh]].pr_ctlinput;
		arg.ctli_ip = &icp->icmp6_ip;
		arg.ctli_m = m;
		if (ctlfunc)
			(*ctlfunc)(code, sin6tosa(&icmpsrc), &arg);
		HTONS(icp->icmp6_ip.ip6_len);
		break;

	case ICMP6_SOLICITATION_RT:
		if (ip->ip6_hlim != ICMP6_ND_HOPS) {
			ICMP6STAT(icp6s_rcv_badrtsol);
			goto freeit;
		}
		if (icmplen < ICMP6_RSLEN) {
			ICMP6STAT(icp6s_badlen);
			goto freeit;
		}
		if ((icmp6_ipsec & ICMP6SEC_NEIGHBORS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		if (!ip6forwarding)
			break;
		IFP_TO_IA6(ifp, ia);
		if ((ia == NULL) || (ia->ia_flags & IFA_BOOTING))
			break;
		if (!rtsol6_input(m, ifp)) {
			ICMP6STAT(icp6s_rcv_badrtsol);
			goto freeit;
		}
		ICMP6STAT(icp6s_rcv_rtsol);
		break;

	case ICMP6_ADVERTISEMENT_RT:
		if (ip->ip6_hlim != ICMP6_ND_HOPS) {
			ICMP6STAT(icp6s_rcv_badrtadv);
			goto freeit;
		}
		if (icmplen < ICMP6_RALEN) {
			ICMP6STAT(icp6s_badlen);
			goto freeit;
		}
		if ((icmp6_ipsec & ICMP6SEC_NEIGHBORS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		IFP_TO_IA6(ifp, ia);
		if ((ia == NULL) || (ia->ia_flags & IFA_BOOTING))
			break;
		if (!rtadv6_input(m, ifp)) {
			ICMP6STAT(icp6s_rcv_badrtadv);
			goto freeit;
		}
		ICMP6STAT(icp6s_rcv_rtadv);
		break;

	case ICMP6_SOLICITATION_ND:
		if (ip->ip6_hlim != ICMP6_ND_HOPS) {
			ICMP6STAT(icp6s_rcv_badndsol);
			goto freeit;
		}
		if (icmplen < ICMP6_NSLEN) {
			ICMP6STAT(icp6s_badlen);
			goto freeit;
		}
		if ((icmp6_ipsec & ICMP6SEC_NEIGHBORS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		if (!ndsol6_input(m, ifp)) {
			ICMP6STAT(icp6s_rcv_badndsol);
			goto freeit;
		}
		ICMP6STAT(icp6s_rcv_ndsol);
		break;

	case ICMP6_ADVERTISEMENT_ND:
		if (ip->ip6_hlim != ICMP6_ND_HOPS) {
			ICMP6STAT(icp6s_rcv_badndadv);
			goto freeit;
		}
		if (icmplen < ICMP6_NALEN) {
			ICMP6STAT(icp6s_badlen);
			goto freeit;
		}
		if ((icmp6_ipsec & ICMP6SEC_NEIGHBORS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		IFP_TO_IA6(ifp, ia);
		if ((ia == NULL) || (ia->ia_flags & IFA_BOOTING))
			break;
		if (!ndadv6_input(m, ifp)) {
			ICMP6STAT(icp6s_rcv_badndadv);
			goto freeit;
		}
		ICMP6STAT(icp6s_rcv_ndadv);
		break;

	case ICMP6_REDIRECT:
		if (ip->ip6_hlim != ICMP6_ND_HOPS) {
			ICMP6STAT(icp6s_rcv_badredirect);
			goto freeit;
		}
		if (icmplen < ICMP6_RDLEN) {
			ICMP6STAT(icp6s_badlen);
			goto freeit;
		}
		if ((icmp6_ipsec & ICMP6SEC_NEIGHBORS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		IFP_TO_IA6(ifp, ia);
		if ((ia == NULL) || (ia->ia_flags & IFA_BOOTING))
			break;
		COPY_ADDR6(icp->icmp6_rdst, icmpsrc.sin6_addr);
		if (!redirect6_input(m, ifp)) {
			ICMP6STAT(icp6s_rcv_badredirect);
			goto freeit;
		}
		ICMP6STAT(icp6s_rcv_redirect);
		/* a direct call to rtredirect should fail */
		pfctlinput(PRC_REDIRECT_HOST, sin6tosa(&icmpsrc));
		break;

	badcode:
		ICMP6STAT(icp6s_badcode);
		break;

	case ICMP6_GROUPMEM_QUERY:
		if (icmplen < ICMP6_GRPLEN) {
			ICMP6STAT(icp6s_badlen);
			goto freeit;
		}
		if ((icmp6_ipsec & ICMP6SEC_GROUPS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		ICMP6STAT(icp6s_rcv_grpqry);

		if (ifp->if_flags & IFF_LOOPBACK)
			break;
		IFP_TO_IA6(ifp, ia);
		if ((ia == NULL) || (ia->ia_flags & IFA_BOOTING))
			break;

		if (!IS_MULTIADDR6(ip->ip6_dst)) {
			ICMP6STAT(icp6s_rcv_bad_grpqry);
			goto freeit;
		}

		/*
		 * - Start the timers in all of our membership records
		 *   for the interface on which the query arrived
		 *   except those that are already running and those
		 *   that are "local".
		 * - For timers already running check if they need to
		 *   be reset.
		 */
		(void) hash_enum(&hashinfo_inaddr, icmp6_checktimer_enummatch,
		  HTF_MULTICAST, (caddr_t)ip, (caddr_t)ifp, (caddr_t)icp);
		break;

	case ICMP6_GROUPMEM_REPORT:
		if (icmplen < ICMP6_GRPLEN) {
			ICMP6STAT(icp6s_badlen);
			goto freeit;
		}
		if ((icmp6_ipsec & ICMP6SEC_GROUPS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		ICMP6STAT(icp6s_rcv_grprep);

		if (ifp->if_flags & IFF_LOOPBACK)
			break;
		IFP_TO_IA6(ifp, ia);
		if ((ia == NULL) || (ia->ia_flags & IFA_BOOTING))
			break;

		if (!IS_MULTIADDR6(icp->icmp6_grp) ||
		    !SAME_ADDR6(icp->icmp6_grp, ip->ip6_dst)) {
			ICMP6STAT(icp6s_rcv_bad_grprep);
			goto freeit;
		}
		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		IN6_LOOKUP_MULTI(icp->icmp6_grp, ifp, inm);
		if (inm != NULL) {
			inm->inm6_timer = 0;
			ICMP6STAT(icp6s_rcv_our_grprep);
			inm->inm6_state = MULTI6_LAZY_MEMBER;
		}
		break;

	case ICMP6_GROUPMEM_TERM:
		if (icmplen < ICMP6_GRPLEN) {
			ICMP6STAT(icp6s_badlen);
			goto freeit;
		}
		if ((icmp6_ipsec & ICMP6SEC_GROUPS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		ICMP6STAT(icp6s_rcv_grpterm);

		if (ifp->if_flags & IFF_LOOPBACK)
			break;
		IFP_TO_IA6(ifp, ia);
		if ((ia == NULL) || (ia->ia_flags & IFA_BOOTING))
			break;

		ICMP6STAT(icp6s_rcv_bad_grpterm);
		break;

	case ICMP6_ECHO:
		if ((icmp6_ipsec & ICMP6SEC_REQUESTS) &&
		    ((m->m_flags & M_AUTH) == 0))
			goto logandfree;
		ICMP6STAT(icp6s_rcv_echoreq);
		icp->icmp6_type = ICMP6_ECHOREPLY;
		/* reflect */
		ICMP6STAT(icp6s_reflect);
		/*
		 * Drop the options that came with the packet because we
		 * only return the data portion of the packet.
		 * XXX6 Is this right?  What if the contents was encrypted?
		 * Do we need to encrypt the return packet?
		 */
		if (opts)
			m_freem(opts);
		icmp6_reflect(m, ifp, 0);
		return;

	/*
	 * No kernel processing for the following;
	 * just fall through to send to raw listener.
	 */
	case ICMP6_ECHOREPLY:
		/* no IPSec processing for this */
		ICMP6STAT(icp6s_rcv_echorep);
		break;

	default:
		log(LOG_INFO,
		    "icmp6_input: unknown type %d\n", icp->icmp6_type);
		if (ICMP6_INFOTYPE(icp->icmp6_type))
			goto freeit;
		break;
	}

	rip6_input(m, ifp, 0, opts);
	return;

logandfree:
	log(LOG_WARNING,
    "icmp6_input: Auth failed for packet from %s to %s type %d code %d\n",
	    ip6_sprintf(&ip->ip6_src), ip6_sprintf(&ip->ip6_dst),
	    icp->icmp6_type, icp->icmp6_code);

freeit:
	if (opts)
		m_freem(opts);
	m_freem(m);
}

/*
 * Reflect the IPv6 packet back to the source
 */
void
icmp6_reflect(m, ifp, opts)
	struct mbuf *m;
	struct ifnet *ifp;
	struct mbuf *opts;
{
	register struct ipv6 *ip = mtod(m, struct ipv6 *);
	register struct in6_ifaddr *ia;
	struct in6_addr t;
	struct sockaddr_in6 icmpdst = in6_zeroaddr;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_CTLOUT)
		printf("icmp6_reflect(%p,%p) %s <> %s\n", m, opts,
		       ip6_sprintf(&ip->ip6_src), ip6_sprintf(&ip->ip6_dst));
#endif
	if (IS_ANYADDR6(ip->ip6_src) || IS_MULTIADDR6(ip->ip6_src)) {
		m_freem(m);	/* Bad return address */
		goto done;
	}

	if (opts) {
		if (m->m_flags & M_AUTH)
			opts = ip6_dropoption(opts, IP6_NHDR_AUTH);
		/* drop the whole packet ? */
		if (m->m_flags & M_CRYPT)
			opts = ip6_dropoption(opts, IP6_NHDR_ESP);
		opt6_reverse(ip, opts);
	}

	COPY_ADDR6(ip->ip6_dst, t);
	COPY_ADDR6(ip->ip6_src, ip->ip6_dst);
	/*
	 * If the incoming packet was addressed directly to us,
	 * use dst as the src for the reply.  Otherwise (broadcast
	 * or anonymous), use the address which corresponds
	 * to the incoming interface.
	 */
	ia = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
	  in6addr_match, (caddr_t)&t, (caddr_t)0, (caddr_t)0);
	COPY_ADDR6(t, icmpdst.sin6_addr);
	if (ia == (struct in6_ifaddr *)0 && ifp)
		ia = (struct in6_ifaddr *)ifaof_ifpforaddr(
				sin6tosa(&icmpdst), ifp);
	/*
	 * The following happens if the packet was not addressed to us,
	 * and was received on an interface with no IPv6 address
	 * or for raw ip packets with options IP_HDRINCL.
	 * We use the address on the first network interface that has
	 * an ipv6 address.  Normally we would have used the first
	 * network interface on the list but it may not have an ipv6 addr.
	 */
	if (ia == (struct in6_ifaddr *)0) {
		struct ifnet *p;

		IFHEAD_LOCK();
		for (p = ifnet; p != NULL; p = p->if_next) {
			if (p->in6_ifaddr != NULL)
				break;
		}
		IFHEAD_UNLOCK();
		ia = (struct in6_ifaddr *)(ifnet->in6_ifaddr);
	}
	COPY_ADDR6(IA_SIN6(ia)->sin6_addr, t);
	COPY_ADDR6(t, ip->ip6_src);
	ip->ip6_hlim = MAXTTL;
	m->m_flags &= ~(M_BCAST|M_MCAST);
	icmp6_send(m, opts, NULL);
done:
	if (opts)
		m_freem(opts);
}

/*
 * Send an ICMPv6 packet back to the IPv6 level,
 * after supplying a checksum.
 */
void
icmp6_send(m, opts, imo)
	register struct mbuf *m;
	struct mbuf *opts;
	struct ip_moptions *imo;
{
	register struct ipv6 *ip = mtod(m, struct ipv6 *);
	register struct icmpv6 *icp;
	register struct ip6ovck *ipc = (struct ip6ovck *)ip;
	struct ip6ovck save_ip = *ipc;
	u_int16_t len = ip->ip6_len;

	ipc->ih6_wrd1 = ipc->ih6_wrd0 = 0;
	ipc->ih6_len = htons(len);
	ipc->ih6_pr = IPPROTO_ICMPV6;
	icp = (struct icmpv6 *)(ip + 1);
	icp->icmp6_cksum = 0;
	icp->icmp6_cksum = in_cksum(m, len + sizeof(*ip));
	*ipc = save_ip;
#ifdef IP6PRINTFS
	if (icmpprintfs || (ip6printfs & D6_CTLOUT))
		printf("icmp6_send dst %s src %s\n",
		       ip6_sprintf(&ip->ip6_dst),
		       ip6_sprintf(&ip->ip6_src));
#endif
	(void) ip6_output(m, opts, NULL, 0, imo, NULL);
}


/*
 * icmp6 filtering routines
 */
int
icmp6_filter_set(struct inpcb *inp, struct mbuf *m)
{
	if ((m == NULL) || (m->m_len != sizeof(struct icmp6_filter)))
		return (EINVAL);
	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	bcopy(mtod(m, caddr_t), (caddr_t)inp->inp_filter.icmp6_filt,
	  sizeof(struct icmp6_filter));
	return (0);
}

int
icmp6_filter_get(struct inpcb *inp, struct mbuf **mp)
{
	struct mbuf *m;

	*mp = m = m_get(M_WAIT, MT_SOOPTS);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = sizeof(struct icmp6_filter);
	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	bcopy((caddr_t)(inp->inp_filter.icmp6_filt), mtod(m, caddr_t),
	  sizeof(struct icmp6_filter));
	return (0);
}

/* Filter (1 == filter it) */
int
icmp6_filter(struct inpcb *inp, struct ipv6 *ip)
{
	struct icmpv6 *icp = (struct icmpv6 *)(ip + 1);

	/*
	 * We don't bother requiring the socket lock to read the
	 * bit in inp->inp_filter.  This means the bit could change
	 * state right after we get it but this could happen right after
	 * dropping the lock too so the effect is the same.  And, it saves
	 * from doing some rather inefficient programming in rip6_input to
	 * avoid the lockj ordering problem with the socket lock and the
	 * inp_head lock.
	 */
	return (ICMP6_FILTER_WILLBLOCK(icp->icmp6_type, &inp->inp_filter));
}
#endif /* INET6 */
