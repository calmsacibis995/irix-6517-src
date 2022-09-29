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
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

#include <tcp-param.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/hashing.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/proc.h>		/* XXX needed for sysctl.h */
#include <sys/sysctl.h>
#include <limits.h>             /* for USHRT_MAX  */

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
#include <netinet/ip6_icmp.h>
#include <netinet/ip6_opts.h>
#include <netinet/if_ether.h>
#include <netinet/if_ndp6.h>
#include <net/netisr.h>
#include <sys/tcpipstats.h>

static void  frg6_freef(struct mbuf *);

/*
 * IP6 fragment reassembly data structures and functions.
 */
#if defined(EVEREST) || defined(SN)
#define FRAG_BUCKETS 16
#else
#define FRAG_BUCKETS 4
#endif

struct ip6frag_bucket {
	mutex_t ip6frag_lock;
	struct ip6q	ip6q;
} ip6frag_qs[FRAG_BUCKETS];

extern	struct domain inet6domain;
extern	struct protosw inet6sw[];
u_char	ip6_protox[IPPROTO_MAX];

int	ip6forwarding = 0;
int	ip6forwsrcrt = 0;

struct sockaddr_in6 in6_zeroaddr = { sizeof(struct sockaddr_in6), AF_INET6 };

/* this is used inside macros, don't put #ifdef IP6PRINTFS ! */
int	ip6printfs = 0;

/* timers */
#if 0
u_int	flcache_keep = 20;	/* 20 seconds */
#endif
u_int	ip6_prune = 2;		/* 2 seconds */

static void ip6_forward(struct mbuf *m, int srcrt, struct ifnet *ifp,
  union route_6 *forward_rt);

/*
 * IPv6 route timeout routines.
 */
/* ARGSUSED */
static int
ip6rttimeout(rn, arg)
	struct radix_node *rn;
	struct walkarg *arg;
{
	register struct rtentry *rt = (struct rtentry *)rn;
	int mtu;

	if ((rt->rt_flags & RTF_DYNAMIC) &&
	    rt->rt_rmx.rmx_expire &&
	    rt->rt_rmx.rmx_expire <= time) {
		mtu = rt->rt_rmx.rmx_mtu;
		if (mtu == 0)
			rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
		else if (mtu > rt->rt_ifp->if_mtu) {
			rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
			cmn_err(CE_WARN, "ip6rttimeout: bad MTU\n");
		}
		/* TODO: plateau and call tcp6_mtudisc */
		if ((rt->rt_flags & RTF_DYNAMIC) &&
		    (rt->rt_flags & RTF_LLINFO) &&
		    rt->rt_ifa->ifa_rtrequest)
			rt->rt_ifa->ifa_rtrequest(RTM_EXPIRE, rt, 0);
		else if (rt->rt_refcnt < 1)
			rtrequest(RTM_DELETE,
				  rt_key(rt), NULL, rt_mask(rt), 0, NULL);
	}
	return (0);
}

/* ARGSUSED */
static void
ip6rttimer(arg)
	void *arg;
{
	struct radix_node_head *rnh;

	timeout(ip6rttimer, NULL, ip6_prune * HZ);

	ROUTE_WRLOCK();
	rnh = rt_tables[AF_INET6];
	if (rnh == NULL) {
		ROUTE_UNLOCK();
		return;
	}
	rn_walktree(rnh, ip6rttimeout, NULL);
	ROUTE_UNLOCK();
}

/*
 * XXX6 A hack for now because ip6_input wants different args than the
 * netisr interface provides.  Also need to replace ip6forward_rt_6
 * with an array in netproc_data (instead of the single netproc_rt).
 * and pass the forward_rt arg to ip6_input().
 */
union	route_6 ip6forward_rt_6;
/*ARGSUSED*/
static void
ip6_preinput(struct mbuf *m, struct route *forward_rt)
{
	struct ifnet *ifp;
	int hlen;

	ifp = mtod(m, struct ifheader *)->ifh_ifp;
	hlen = mtod(m, struct ifheader *)->ifh_hdrlen;
	M_ADJ(m, hlen);

	ip6_input(m, ifp, NULL, NULL);
}

/*
 * IPv6 initialization: fill in IPv6 protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
void
ip6_init()
{
	register struct protosw *pr;
	register int i;

	pr = pffindproto(PF_INET6, IPPROTO_RAW, SOCK_RAW, NULL);
	if (pr == 0)
		panic("ip6_init");
	for (i = 0; i < IPPROTO_MAX; i++)
		ip6_protox[i] = pr - inet6sw;
	for (pr = inet6domain.dom_protosw + 1;
	    pr < inet6domain.dom_protoswNPROTOSW; pr++) {
		if (pr->pr_domain->dom_family != PF_INET6)
			continue;
		if (pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip6_protox[pr->pr_protocol] = pr - inet6sw;
		if (pr->pr_type == 0 && pr->pr_protocol == IP6_NHDR_HOP)
			ip6_protox[pr->pr_protocol] = pr - inet6sw;
	}
	for (i = 0; i < FRAG_BUCKETS; i++) {
		mutex_init(&ip6frag_qs[i].ip6frag_lock, MUTEX_DEFAULT,
		  "ip6frag");
		ip6frag_qs[i].ip6q.next = ip6frag_qs[i].ip6q.prev =
		  &ip6frag_qs[i].ip6q;
	}
	ip6_id = time;
	timeout(ip6rttimer, NULL, ip6_prune * HZ);
	network_input_setup(AF_INET6, ip6_preinput);
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
ip6_acceptit_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
  caddr_t arg2)
{
	struct in6_ifaddr *ia;
	struct in6_addr *addr;
	
	if ((h->flags & (HTF_INET|HTF_MULTICAST)) == 0) {
		/* not correct bucket type */
		return 0;
	}

	if (h->flags & HTF_MULTICAST) /* multicast address node */
		return (in6_multi_match(h, key, arg1, arg2));

	ia = (struct in6_ifaddr *)h;
	addr = (struct in6_addr *)key;
	if (SAME_ADDR6(IA_SIN6(ia)->sin6_addr, *addr))
		return (1);

	return (0);
}

/*
 * IPv6 input routine.  Checksum and byte swap header.  If fragmented
 * try to reassemble.  Process options.  Pass to next level.
 */
/*ARGSUSED*/
void
ip6_input(
	struct mbuf *m,
	struct ifnet *ifp,
	struct ipsec *ipsec,
	struct mbuf *opts)
{
	struct ipv6 *ip;
	int len;
	struct mbuf *m0;
	extern int in6_ifaddr_count;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_INPUT)
		printf("ip6_input(%p) len=%d\n",
		       m, m->m_len);
#endif
	/*
	 * We can do nothing useful with options ?!
	 */
	if (opts)
		m_freem(opts);

	/*
	 * If no IPv6 addresses have been set yet but the interfaces
	 * are receiving, can't do anything with incoming packets yet.
	 */
	if (in6_ifaddr_count == 0)
		goto bad;

	IP6STAT(ip6s_total);
	if (m->m_len < sizeof (struct ipv6) &&
	    (m = m_pullup(m, sizeof (struct ipv6))) == 0) {
		IP6STAT(ip6s_toosmall);
		return;
	}

	ip = mtod(m, struct ipv6 *);
	if ((ip->ip6_head & IPV6_FLOWINFO_VERSION) != IPV6_VERSION) {
		IP6STAT(ip6s_badvers);
		goto bad;
	}

#ifdef IP6PRINTFS
	if (ip6printfs & D6_INPUT)
		printf("ip6_input src %s dst %s len %d nh %d\n",
		       ip6_sprintf(&ip->ip6_src),
		       ip6_sprintf(&ip->ip6_dst),
		       ntohs(ip->ip6_len), ip->ip6_nh);
#endif
	/*
	 * Check source address.
	 */
	if (IS_MULTIADDR6(ip->ip6_src) || in6_isanycast(&ip->ip6_src)) {
#ifdef IP6PRINTFS
		if (ip6printfs & D6_INPUT)
			printf("ip6_input bad src\n");
#endif
		IP6STAT(ip6s_badsource);
		goto bad;
	}

	/*
	 * Check buffer length and trim extra.
	 */
	NTOHS(ip->ip6_len);
	len = -(sizeof(struct ipv6) + ip->ip6_len);
	m0 = m;
	for (;;) {
		len += m->m_len;
		if (m->m_next == 0)
			break;
		m = m->m_next;
	}
	if (len != 0) {
		if (len < 0) {
			IP6STAT(ip6s_tooshort);
			goto bad;
		}
		if (len <= m->m_len)
			m->m_len -= len;
		else
			m_adj(m0, -len);
	}
	m = m0;

	/*
	 * Check our list of addresses, to see if the packet is for us.
	 */
#define IFPTOSIN6(ifp) ((struct in6_ifaddr *)(ifp)->in6_ifaddr)->ia_addr
	if (ifp->in6_ifaddr &&
	  ((struct ifaddr *)(ifp->in6_ifaddr))->ifa_addr->sa_family ==
	  AF_INET6 && SAME_ADDR6(ip->ip6_dst, IFPTOSIN6(ifp).sin6_addr))
		goto ours;
#undef IFPTOSIN6

	if (((struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,
	  ip6_acceptit_match, (caddr_t)(&(ip->ip6_dst)), (caddr_t)ifp,
	 (caddr_t)0))) {
		/*
		 * it's for us so check when it's a multicast address AND
		 * we're also acting as a multicast router.
		 */
		if (IS_MULTIADDR6(ip->ip6_dst)) {
			struct in6_multi *inm;

			if (ip6_mrouter) {
				/*
				 * If we are acting as a multicast router, all
				 * incoming multicast packets are passed to the
				 * kernel-level multicast forwarding function.
				 * The packet is returned (relatively) intact;
				 * if ip6_mforward() returns a non-zero value,
				 * the packet must be discarded, else it may be
				 * accepted below.
				 */
				if (ip6_mforward(ip, ifp, m, 0) != 0) {
					IP6STAT(ip6s_cantforward);
					goto bad;
				}
				/*
				 * The process-level routing daemon needs to
				 * receive all multicast ICMPv6 packets,
				 * whether or not this host belongs to their
				 * destination groups.
				 */
				if (ip->ip6_nh == IPPROTO_ICMPV6)
					goto ours;
				IP6STAT(ip6s_forward);
			}
			/*
			 * See if we belong to the destination multicast group
			 * on the arrival interface.
			 */
			IN6_LOOKUP_MULTI(ip->ip6_dst, ifp, inm);
			if (inm == NULL) {
				IP6STAT(ip6s_cantforward);
				goto bad;
			}
		}
		goto ours;
	}
	/* no broadcast case with ipv6 */

	if (ip6forwarding) {
		if (in6_isanycast(&ip->ip6_dst) >= IP6ANY_ROUTER)
			goto ours;
	} else if (in6_isanycast(&ip->ip6_dst) == IP6ANY_ALLWAYS)
		goto ours;
	/*
	 * Not for us; forward if possible and desirable.
	 */
	if (ip6forwarding == 0) {
		IP6STAT(ip6s_cantforward);
		goto bad;
	} else {
		ip6_forward(m, 0, ifp, &ip6forward_rt_6);
		return;
	}

ours:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_INPUT)
		printf("ip6_input for protocol %d\n", ip->ip6_nh);
#endif
	/*
	 * Switch out to protocol's input routine.
	 */
	IP6STAT(ip6s_delivered);
	IP6STAT(ip6s_inhist[ip->ip6_nh]);
	(*inet6sw[ip6_protox[ip->ip6_nh]].pr_input)(m, ifp, 0, 0);
	return;
bad:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_INPUT)
		 printf("ip6_input bad packet\n");
#endif
	m_freem(m);
}

/*
 * Fragment header input processing.
 */

/*
 * Take incoming datagram fragment and try to
 * reassemble it into whole datagram.  If a chain for
 * reassembly of this datagram already exists, then it
 * is given as fp; otherwise have to make a chain.
 */
struct ipv6 *
ip6_reass(register struct mbuf *m, register struct ip6q *fp,
  struct mbuf **mpp, struct ip6frag_bucket *b)
{
	register struct ip6asfrag *ip = mtod(m, struct ip6asfrag *);
	register struct ip6asfrag *q;
	struct mbuf *t;
	int i, next;

	/*
	 * Presence of header sizes in mbufs
	 * would confuse code below.
	 */
	m->m_off += sizeof(*ip);
	m->m_len -= sizeof(*ip);

	/*
	 * If first fragment to arrive, create a reassembly queue.
	 */
	if (fp == 0) {
		if ((t = m_get(M_DONTWAIT, MT_FTABLE)) == NULL) {
			IP6STAT(ip6s_inomem);
			goto dropfrag;
		}
		fp = mtod(t, struct ip6q *);
		insque(fp, &b->ip6q);
		fp->ip6q_ttl = IP6FRAGTTL;
		fp->ip6q_nh = ip->ip6f_nh;
		fp->ip6q_id = ip->ip6f_id;
		fp->ip6q_next = fp->ip6q_prev = (struct ip6asfrag *)fp;
		COPY_ADDR6(((struct ipv6 *)ip)->ip6_src, fp->ip6q_src);
		COPY_ADDR6(((struct ipv6 *)ip)->ip6_dst, fp->ip6q_dst);
		q = (struct ip6asfrag *)fp;
		fp->ip6q_mbuf = t;
		goto insert;
	}

	t = fp->ip6q_mbuf;
	GOODMT(t->m_type);
	/*
	 * Find a segment which begins after this one does.
	 */
	for (q = fp->ip6q_next; q != (struct ip6asfrag *)fp; q = q->ip6f_next) {
		if (q->ip6f_off > ip->ip6f_off)
			break;
		t = t->m_act;
		GOODMT(t->m_type);
	}

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (q->ip6f_prev != (struct ip6asfrag *)fp) {
		i = q->ip6f_prev->ip6f_off +
			q->ip6f_prev->ip6f_len - ip->ip6f_off;
		if (i > 0) {
			if (i >= ip->ip6f_len)
				goto dropfrag;
			m_adj(m, i);
			ip->ip6f_off += i;
			ip->ip6f_len -= i;
		}
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	while (q != (struct ip6asfrag *)fp &&
	       ip->ip6f_off + ip->ip6f_len > q->ip6f_off) {
		struct mbuf *tmp;

		i = (ip->ip6f_off + ip->ip6f_len) - q->ip6f_off;
		if (i < q->ip6f_len) {
			q->ip6f_len -= i;
			q->ip6f_off += i;
			m_adj(t->m_act, i);
			break;
		}
		q = q->ip6f_next;
		frg6_deq(q->ip6f_prev);
		tmp = t->m_act;
		t->m_act = tmp->m_act;
		m_freem(tmp);
	}

insert:
	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 */
	frg6_enq(ip, q->ip6f_prev);
	ASSERT(m->m_act == 0);
	ASSERT(t != 0);
	GOODMT(t->m_type);
	m->m_act = t->m_act;    /* insert in mbuf chain too */
	t->m_act = m;
	next = 0;
	for (q = fp->ip6q_next;
	     q != (struct ip6asfrag *)fp;
	     q = q->ip6f_next) {
		if (q->ip6f_off != next)
			return (0);
		next += q->ip6f_len;
	}
	if (q->ip6f_prev->ip6f_mff & 1)
		return (0);

	/*
	 * Reassembly is complete; concatenate fragments.
	 */
	m = fp->ip6q_mbuf->m_act;	/* first fragment */
	t = m->m_next;
	if (t) {
		m->m_next = 0;
		m_cat(m, t);
	}
	t = m->m_act;
	m->m_act = 0;
	while (t) {
		struct mbuf *t2 = t->m_act;
		t->m_act = 0;
		m_cat(m, t);
		t = t2;
	}

	/*
	 * Create header for new IPv6 packet by
	 * modifying header of first packet;
	 * dequeue and discard fragment reassembly header.
	 * Make header visible.
	 */
	ip = fp->ip6q_next;
	ip->ip6f_len = next;
	ip->ip6f_mff &= ~1;
	COPY_ADDR6(fp->ip6q_src, ((struct ipv6 *)ip)->ip6_src);
	COPY_ADDR6(fp->ip6q_dst, ((struct ipv6 *)ip)->ip6_dst);
	remque(fp);
	(void) m_free(fp->ip6q_mbuf);
	/* m is pointing the first fragment already */
	GOODMT(m->m_type);
	m->m_len += sizeof(*ip);
	m->m_off -= sizeof(*ip);
	/* some debugging cruft by sklower, below, will go away soon */
#ifdef IP6PRINTFS
	if (ip6printfs & D6_REASS)
		printf("frg6_reass return src %s dst %s len %d\n",
		       ip6_sprintf(&((struct ipv6 *)ip)->ip6_src),
		       ip6_sprintf(&((struct ipv6 *)ip)->ip6_dst),
		       ((struct ipv6 *)ip)->ip6_len);
#endif
	*mpp = m;
	return ((struct ipv6 *)ip);

dropfrag:
	IP6STAT(ip6s_fragdropped);
	m_freem(m);
	return (0);
}

/*ARGSUSED*/
void
frg6_input(struct mbuf *m, struct ifnet *ifp,
  struct ipsec *ipsec, struct mbuf *opts)
{
	register struct ipv6 *ip;
	register struct ipv6_fraghdr *frgp;
	register struct ip6q *fp;
	struct ip6frag_bucket *b;
	__psunsigned_t errpptr;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_REASS)
		printf("frg6_input\n");
#endif
	IP6STAT_DEC(ip6s_delivered, 1);

	/*
	 * Reassemble, first pullup headers.
	 * (if a cluster is used headers will be moved to a mbuf)
	 */
	if ((m = m_pullup(m, sizeof(*ip) + sizeof(*frgp))) == 0) {
		IP6STAT(ip6s_toosmall);
		if (opts)
			m_freem(opts);
		return;
	}
	ip = mtod(m, struct ipv6 *);
	frgp = (struct ipv6_fraghdr *)(ip + 1);
	NTOHS(frgp->if6_off);
	NTOHL(frgp->if6_id);

	/*
	 * Make sure that fragments have a data length
	 * that's a non-zero multiple of 8 bytes.
	 */
	if ((frgp->if6_off & IP6_MF) &&
	    ((ip->ip6_len <= sizeof(*frgp)) || ((ip->ip6_len & 0x7) != 0))) {
		IP6STAT(ip6s_toosmall);
#ifdef IP6PRINTFS
		if (ip6printfs & D6_REASS)
			printf("frg6_input: bad length %d\n", ip->ip6_len);
#endif
		/*
		 * error pointer contains the offset of the payload length
		 * field.
		 */
		errpptr = (__psunsigned_t)&((struct ipv6 *)0)->ip6_len;
		icmp6_errparam(m, opts, errpptr, ifp, ICMP6_PARAMPROB_HDR);
		return;
	}

	/*
	 * Look for queue of fragments of this datagram.
	 */
	b = ip6frag_qs + ip->ip6_src.s6_addr32[3] % FRAG_BUCKETS;
	mutex_lock(&b->ip6frag_lock, PZERO);
	for (fp = b->ip6q.next; fp != &b->ip6q; fp = fp->next)
		if (frgp->if6_id == fp->ip6q_id &&
		    SAME_ADDR6(ip->ip6_src, fp->ip6q_src) &&
		    SAME_ADDR6(ip->ip6_dst, fp->ip6q_dst) &&
		    frgp->if6_nh == fp->ip6q_nh)
			goto found;
	fp = 0;
found:

	/*
	 * Adjust ip6_len to not reflect header,
	 * set if6_res (same as ifp6_mff)
	 * if more fragments are expected,
	 * set ip6_nh to next header type.
	 */
	ip->ip6_len -= sizeof(*frgp);
	ip->ip6_nh = frgp->if6_nh;
	if (frgp->if6_off & IP6_MF) {
		frgp->if6_res = 1;
		frgp->if6_off &= ~IP6_MF;
		/* check if the fragment is in bound */
		if ((int)frgp->if6_off + (int)ip->ip6_len > USHRT_MAX) {
			IP6STAT(ip6s_toosmall);
#ifdef IP6PRINTFS
			if (ip6printfs & D6_REASS)
				printf("frg6_input: too large\n");
#endif
			mutex_unlock(&b->ip6frag_lock);
			/*
			 * error pointer points at the fragment offset field.
			 */
			errpptr = sizeof(*ip) +
			  (__psunsigned_t)&((struct ipv6_fraghdr *)0)->if6_off;
			icmp6_errparam(m, opts, errpptr, ifp,
			  ICMP6_PARAMPROB_HDR);
			return;
		}
	} else
		frgp->if6_res = 0;

	/*
	 * If datagram marked as having more fragments
	 * or if this is not the first fragment,
	 * attempt reassembly; if it succeeds, proceed.
	 */
	if (frgp->if6_res || frgp->if6_off) {
		struct mbuf *mp;

		IP6STAT(ip6s_fragments);
		ip = ip6_reass(m, fp, &mp, b);
		if (ip == 0) {
			if (opts)
				m_freem(opts);
			mutex_unlock(&b->ip6frag_lock);
			return;
		}
		IP6STAT(ip6s_reassembled);
		m = mp;
	} else
		if (fp)
			frg6_freef(fp->ip6q_mbuf);
	mutex_unlock(&b->ip6frag_lock);

	/*
	 * Zap fragment header
	 */
	m->m_off += sizeof(struct ipv6_fraghdr);
	m->m_len -= sizeof(struct ipv6_fraghdr);
	ovbcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ipv6));
	ip = mtod(m, struct ipv6 *);

	/*
	 * Switch out to protocol's input routine.
	 */
	if (ip->ip6_nh == IP6_NHDR_HOP) {
		icmp6_errparam(m, opts, (long)&((struct ipv6 *)0)->ip6_nh,
		  ifp, ICMP6_PARAMPROB_NH);
		return;
	}
	IP6STAT(ip6s_delivered);
	IP6STAT(ip6s_inhist[ip->ip6_nh]);
	(*inet6sw[ip6_protox[ip->ip6_nh]].pr_input)(m, ifp, 0, opts);
	return;
}

/*
 * Free an IPv6 fragment reassembly header and all
 * associated datagrams.
 */
static void
frg6_freef(struct mbuf *m)
{
	register struct mbuf *n;

	remque(mtod(m, struct ipq *));
	while (m) {
		n = m->m_act;
		m_freem(m);
		m = n;
	}
}

/*
 * Put an IPv6 fragment on a reassembly chain.
 * Like insque, but pointers in middle of structure.
 */
void
frg6_enq(p, prev)
	register struct ip6asfrag *p, *prev;
{
	p->ip6f_prev = prev;
	p->ip6f_next = prev->ip6f_next;
	prev->ip6f_next->ip6f_prev = p;
	prev->ip6f_next = p;
}

/*
 * To frg6_enq as remque is to insque.
 */
void
frg6_deq(struct ip6asfrag *p)
{
	p->ip6f_prev->ip6f_next = p->ip6f_next;
	p->ip6f_next->ip6f_prev = p->ip6f_prev;
}

/*
 * IPv6 fragment timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
frg6_slowtimo()
{
	register struct ip6q *fp;
	int bucket;
	struct ip6frag_bucket *b;

	for (bucket = 0; bucket < FRAG_BUCKETS; bucket++) {
		b = ip6frag_qs + bucket;
		mutex_lock(&b->ip6frag_lock, PZERO);
		fp = b->ip6q.next;
		if (fp == 0) {
			mutex_unlock(&b->ip6frag_lock);
			return;
		}
		while (fp != &b->ip6q) {
			--fp->ip6q_ttl;
			fp = fp->next;
			if (fp->prev->ip6q_ttl == 0) {
				IP6STAT(ip6s_fragtimeout);
				frg6_freef(fp->prev->ip6q_mbuf);
			}
		}
		mutex_unlock(&b->ip6frag_lock);
	}
}

/*
 * Drain off all datagram fragments.
 */
void
frg6_drain()
{
	struct ip6frag_bucket *b;
	int bucket;

	for (bucket = 0; bucket < FRAG_BUCKETS; bucket++) {
		b = ip6frag_qs + bucket;
		mutex_lock(&b->ip6frag_lock, PZERO);
		while (b->ip6q.next != &b->ip6q) {
			IP6STAT(ip6s_fragdropped);
			frg6_freef(b->ip6q.next->ip6q_mbuf);
		}
		mutex_unlock(&b->ip6frag_lock);
	}
}

/*
 * Copy an option (ie header) to opts mbuf chain.
 */
static struct mbuf *
ip6_saveoption(
	register struct mbuf *m0,
	struct mbuf *opts,
	register int len,
	int type)
{
	register struct ipv6 *ip = mtod(m0, struct ipv6 *);
	register struct mbuf *m;
	int saved = 0;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_OPTIN)
		printf("ip6_saveoption(%p,%p,%d,%d)\n", m0, opts, len, type);
#endif
	if (type == IP6_NHDR_RT)
		len += sizeof(struct in6_addr);

	len += OPT6_HDRSIZE;  /* extra space needed by kernel */
	if (len > MCLBYTES)
		goto zap;
	m = m_vget(M_DONTWAIT, len, MT_SOOPTS);
	if (m == 0) {
		len -= OPT6_HDRSIZE;
		IP6STAT(ip6s_inomem);
		goto zap;
	}
	if (MLEN >= len)
		MH_ALIGN(m, len);
	if (type == IP6_NHDR_RT) {
		len -= sizeof(struct in6_addr);
		bzero(OPT6_DATA(m, caddr_t) + len, sizeof(struct in6_addr));
	}
	m->m_len = len;
	OPT6_TYPE(m) = type;
	if (opts) {
		m->m_next = opts;
	}
	len -= OPT6_HDRSIZE;
	m_datacopy(m0, sizeof(*ip), len, OPT6_DATA(m, caddr_t));
	opts = m;
	saved = 1;

zap:
	if (m0->m_len > sizeof(*ip) + len) {
		m0->m_off += len;
		m0->m_len -= len;
		ovbcopy((caddr_t)ip, mtod(m0, caddr_t), sizeof(struct ipv6));
	} else {
		len -= m0->m_len - sizeof(*ip);
		m0->m_len = sizeof(*ip);
		m = m0->m_next;
		/* from m_adj */
		while (len > 0) {
			if (m == NULL)
				panic("ip6_saveoption");
			if (m->m_len <= len) {
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {
				m->m_len -= len;
				m->m_off += len;
				len = 0;
			}
		}
	}
	if (opts && !saved) {
		m_freem(opts);
		opts = (struct mbuf *)0;
	}
#ifdef IP6PRINTFS
	if (ip6printfs & D6_OPTIN)
		printf("ip6_saveoption returns %p\n", opts);
#endif
	return (opts);			
}

/*
 * Delete an option (ie header).
 */
static void
ip6_deloption(register struct mbuf *m0, register int len)
{
	register struct ipv6 *ip = mtod(m0, struct ipv6 *);
	register struct mbuf *m;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_OPTIN)
		printf("ip6_deloption(%p,%d)\n", m0, len);
#endif

	if (m0->m_len > sizeof(*ip) + len) {
		m0->m_off += len;
		m0->m_len -= len;
		ovbcopy((caddr_t)ip, mtod(m0, caddr_t), sizeof(struct ipv6));
	} else {
		len -= m0->m_len - sizeof(*ip);
		m0->m_len = sizeof(*ip);
		m = m0->m_next;
		/* from m_adj */
		while (len > 0) {
			if (m == NULL)
				panic("ip6_deloption");
			if (m->m_len <= len) {
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {
				m->m_len -= len;
				m->m_off += len;
				len = 0;
			}
		}
	}
}

/*
 * Drop an option (ie header) in the saved option chain.
 */
struct mbuf *
ip6_dropoption(struct mbuf *opts, int which)
{
	register struct mbuf *o, *tbd;

	for (tbd = opts; tbd; tbd = tbd->m_next)
		if (OPT6_TYPE(tbd) == which)
			break;
	if (tbd == NULL)
		return (opts);
	if (tbd == opts)
		return (m_free(tbd));
	o = opts;
	while (o->m_next != tbd)
		o = o->m_next;
	o->m_next = m_free(tbd);
	return (opts);
}

/*
 * Hop-by-Hop header input processing.
 */
/*ARGSUSED*/
void
hop6_input(struct mbuf *m0, struct ifnet *ifp,
  struct ipsec *ipsec, struct mbuf *opts)
{
	register struct ipv6 *ip;
	register struct ipv6_h2hhdr *hp;
	register int len;
	int m0len = 0;
	struct mbuf *m;

	IP6STAT_DEC(ip6s_delivered, 1);

	if (m0->m_len < sizeof(*ip) + sizeof(*hp)) {
		if ((m0 = m_pullup(m0, sizeof(*ip) + sizeof(*hp))) == 0) {
			IP6STAT(ip6s_toosmall);
			if (opts)
				m_freem(opts);
			return;
		}
	}

	ip = mtod(m0, struct ipv6 *);
	hp = (struct ipv6_h2hhdr *)(ip + 1);
	len = (hp->ih6_hlen + 1) * sizeof(*hp);

	for (m = m0; m; m = m->m_next)
		m0len += m->m_len;

	if ((len > ip->ip6_len) || (len + sizeof(*ip) > m0len)) {
		IP6STAT(ip6s_toosmall);
		if (opts)
			m_freem(opts);
		m_freem(m0);
		return;
	}
	ip->ip6_nh = hp->ih6_nh;
	hp->ih6_nh = 0;
	ip->ip6_len -= len;
	opts = ip6_saveoption(m0, opts, len, IP6_NHDR_HOP);
	if (opts && hd6_inoptions(m0, ifp, opts, (int *)0))
		return;
	ip = mtod(m0, struct ipv6 *);

        /*
         * Switch out to protocol's input routine.
         */
	if (ip->ip6_nh == IP6_NHDR_HOP) {
		icmp6_errparam(m0, opts, (long)&((struct ipv6 *)0)->ip6_nh,
		  ifp, ICMP6_PARAMPROB_NH);
		return;
	}
	IP6STAT(ip6s_delivered);
	IP6STAT(ip6s_inhist[ip->ip6_nh]);
	(*inet6sw[ip6_protox[ip->ip6_nh]].pr_input)(m0, ifp, 0, opts);
        return;
}

/*
 * (Generic) header option processing.
 */
/*ARGSUSED*/
int
hd6_inoptions(struct mbuf *m, struct ifnet *ifp, struct mbuf *opts, int *alert)
{
	register struct ipv6 *ip;
	register struct ipv6_h2hhdr *hp;
	register struct opt6_any *op;
	register int len, olen;
	__psunsigned_t errpptr = sizeof(struct ipv6);
	struct mbuf *o;
	int error = 0;

	ip = mtod(m, struct ipv6 *);
	hp = OPT6_DATA(opts, struct ipv6_h2hhdr *);
	op = (struct opt6_any *)&hp->ih6_pad1;
	len = ((hp->ih6_hlen + 1) * sizeof(*hp)) - (2 * sizeof(u_int8_t));

	while (len > 0) {
		switch (op->o6any_ext) {

		case OPT6_PAD_0:
			op = (struct opt6_any *)&op->o6any_len;
			len--;
			break;
#ifdef OPT6_ROUTER_ALERT
                case OPT6_ROUTER_ALERT:
			if (op->o6any_len != 2)
				goto badlen;
			if (alert)
				*alert = 1;
			goto next;
#endif

		default:
			switch (OPT6_ACTION(op->o6any_ext)) {
				case OPT6_A_SKIP:
					goto next;

				case OPT6_A_DISC:
					goto discard;

				case OPT6_A_FERR:
					goto bad;

				case OPT6_A_OERR:
					if (m->m_flags & (M_BCAST|M_MCAST) ||
					    IS_MULTIADDR6(ip->ip6_dst))
						goto discard;
					else
						goto bad;
				}
		case OPT6_PAD_N:
		next:
			olen = op->o6any_len + sizeof(*op);
			if (olen > len)
				goto badlen;
			op = (struct opt6_any *)((caddr_t)op + olen);
			len -= olen;
			break;
		}
	}
	return (0);

	/*
	 * Errors.
	 */

discard:
	m_freem(opts);
	m_freem(m);
	return (1);

badlen:
	errpptr++;
bad:
	/*
	 * Note errpptr eventually is placed in a 32bit field in the
	 * packet.  This is ok because errpptr is an offset into the
	 * packet rather than a pointer.  It is declared as __psunsigned_t to
	 * keep the 64 bit compilers happy about the calculation below
	 * and passing it into icmp6_error().
	 */
	errpptr += (__psunsigned_t)op - (__psunsigned_t)hp;

	/*
	 * Well, we have to jump through a few hoops here before we can
	 * call icmp6_errparam().  errpptr is supposed to be an offset into
	 * the packet pointed at by 'm' but right now it refers to an offset
	 * in the first option in the list pointed at by opts.  We have
	 * to insert that option back into 'm' before calling icmp6_errparam().
	 */
	o = opts;
	m = ip6_insertoption(m, o, NULL, IP6_INSOPT_RAW, &error);
	opts = m_free(o);
	icmp6_errparam(m, opts, errpptr, ifp, ICMP6_PARAMPROB_OPT);
	m_freem(opts);
	return (1);
}


/*
 * routing header input processing.
 */
/*ARGSUSED*/
void
rt6_input(
	register struct mbuf *m0,
	struct ifnet *ifp, 
	struct ipsec *ipsec,
	struct mbuf *opts)
{
	register struct ipv6 *ip;
	register struct ipv6_rthdr *hp;
	register int len, numa;
	__psunsigned_t errpptr = sizeof(struct ipv6);
	struct sockaddr_in6 rt6addr = in6_zeroaddr;
	int error = 0;

	IP6STAT_DEC(ip6s_delivered, 1);

	if (m0->m_len < sizeof(*ip) + sizeof(*hp)) {
		if ((m0 = m_pullup(m0, sizeof(*ip) + sizeof(*hp))) == 0) {
			IP6STAT(ip6s_toosmall);
			if (opts)
				m_freem(opts);
			return;
		}
	}

	ip = mtod(m0, struct ipv6 *);
	hp = (struct ipv6_rthdr *)(ip + 1);
	len = (hp->ir6_hlen << 3) + sizeof(struct ipv6_rthdr);
	numa = hp->ir6_hlen >> 1;
	if (hp->ir6_type != IP6_LSRRT) {
		errpptr += 2;
		if (hp->ir6_sglt)
			goto bad;
		else {
			ip->ip6_nh = hp->ir6_nh;
			hp->ir6_nh = 0;
			ip->ip6_len -= len;
			ip6_deloption(m0, len);
			ip = mtod(m0, struct ipv6 *);
			goto next;
		}
	}
	if ((hp->ir6_hlen & 1) || (numa > IP6_RT_MAX)) {
		errpptr++;
		goto bad;
	}
#ifdef notdef
	if (ntohl(hp->ir6_slmsk) & ~IP6_RT_SLMSK) {
		errpptr += 4;
		goto bad;
	}
#endif
	if (hp->ir6_sglt > numa) {
		errpptr += 3;
		goto bad;
	} else if (hp->ir6_sglt != 0) {
		register struct mbuf *m;
		register int off, optalloc = 0;
		struct in6_addr temp, *addr;

		if ((ip6forwarding == 0) && (ip6forwsrcrt == 0))
			goto noforward;
		len += sizeof(struct ipv6);
		if (len - sizeof(struct ipv6) > ip->ip6_len) {
			IP6STAT(ip6s_toosmall);
			if (opts)
				m_freem(opts);
			m_freem(m0);
			return;
		}
		hp->ir6_sglt--;
		off = sizeof(struct ipv6) + sizeof(struct ipv6_rthdr);
		off += (numa - hp->ir6_sglt - 1) * sizeof(struct in6_addr);
		if (off + sizeof(struct in6_addr) > m0->m_len) {
			int olen;
			struct mbuf *o;

			if (opts) {
				for (o = opts; o; o = o->m_next)
					olen = OPT6_LEN(o);
				len += olen;
			}
			if (len > MCLBYTES)
				goto noforward;
			m = m_vget(M_DONTWAIT, len, MT_DATA);
			if (m == 0)
				goto noforward;
			m->m_flags = m0->m_flags & M_COPYFLAGS;
			m->m_len = len;
			if (opts)
				m->m_off += olen;
			m_datacopy(m0, 0, len, mtod(m, caddr_t));
			m_adj(m0, len);
			m->m_next = m0;
			m0 = m;
			optalloc = IP6_INSOPT_NOALLOC;
			ip = mtod(m0, struct ipv6 *);
			hp = (struct ipv6_rthdr *)(ip + 1);
		}
		addr = (struct in6_addr *)(mtod(m0, caddr_t) + off);
		if (IS_MULTIADDR6(*addr) || IS_MULTIADDR6(ip->ip6_dst)) {
			if (opts)
				m_freem(opts);
			m_freem(m0);
			return;
		}
		COPY_ADDR6(*addr, temp);
		COPY_ADDR6(ip->ip6_dst, *addr);
		COPY_ADDR6(temp, ip->ip6_dst);
		if (ntohl(hp->ir6_slmsk) & IP6_RT_SLBIT(numa - hp->ir6_sglt)) {
			register struct ifaddr *ia;

			COPY_ADDR6(temp, rt6addr.sin6_addr);
			ia = ifa_ifwithdstaddr(sin6tosa(&rt6addr));
			if (ia == 0)
				ia = ifa_ifwithnet(sin6tosa(&rt6addr));
			if (ia == 0) {
				if (opts)
					m_freem(opts);
				icmp6_error(m0, ICMP6_UNREACH,
					    ICMP6_UNREACH_RTFAIL, NULL, ifp);
				return;
			}
		}
		while (opts) {
			m0 = ip6_insertoption(m0, opts, NULL, optalloc, &error);
			opts = opts->m_next;
		}
		ip6_forward(m0, 1, ifp, &ip6forward_rt_6);
		return;
	    noforward:
		if (opts)
			m_freem(opts);
		m_freem(m0);
		return;
	}
	if (len > ip->ip6_len) {
		IP6STAT(ip6s_toosmall);
		if (opts)
			m_freem(opts);
		m_freem(m0);
		return;
	}
	ip->ip6_nh = hp->ir6_nh;
	hp->ir6_nh = 0;
	ip->ip6_len -= len;
	opts = ip6_saveoption(m0, opts, len, IP6_NHDR_RT);
	ip = mtod(m0, struct ipv6 *);

next:
        /*
         * Switch out to protocol's input routine.
         */
	if ((ip->ip6_nh == IP6_NHDR_HOP) ||
	  (ip->ip6_nh == IP6_NHDR_RT)) {
		icmp6_errparam(m0, opts, (long)&((struct ipv6 *)0)->ip6_nh,
		  ifp, ICMP6_PARAMPROB_NH);
		return;
	}
	IP6STAT(ip6s_delivered);
	IP6STAT(ip6s_inhist[ip->ip6_nh]);
	(*inet6sw[ip6_protox[ip->ip6_nh]].pr_input)(m0, ifp, 0, opts);
        return;

bad:
	/*
	 * Note errpptr eventually is placed in a 32bit field in the
	 * packet.  This is ok because errpptr is an offset into the
	 * packet rather than a pointer.  It is declared as __psunsigned_t to
	 * keep the 64 bit compilers happy about passing it into icmp6_error().
	 */
	icmp6_errparam(m0, opts, errpptr, ifp, ICMP6_PARAMPROB_HDR);
	return;
}

/*
 * Reverse a source route in a routing header.
 */
void
opt6_reverse(ip, opts)
	struct ipv6 *ip;
	register struct mbuf *opts;
{
	register struct ipv6_rthdr *hp;
	struct in6_addr *addr0, *addr1, addrt;
	int numa, i, j;
	u_int32_t bitmsk;

	/* get router header */
	while (opts && OPT6_TYPE(opts) != IP6_NHDR_RT)
		opts = opts->m_next;
	if (opts == 0)
		return;
	hp = OPT6_DATA(opts, struct ipv6_rthdr *);
	numa = hp->ir6_hlen >> 1;

	/* fast check */
	if ((numa * sizeof(struct in6_addr) + sizeof(*hp) != OPT6_LEN(opts)) ||
	    (hp->ir6_type != IP6_LSRRT) ||
	    (hp->ir6_hlen & 1) ||
	    (numa > IP6_RT_MAX) ||
#ifdef notdef
	    (ntohl(hp->ir6_slmsk) & ~IP6_RT_SLMSK) ||
#endif
	    (hp->ir6_sglt != 0))
		return;

	/* test if the option has been already reversed (cf AH processing) */
	addr0 = (struct in6_addr *)(hp + 1);
	addr1 = addr0 + numa - 1;
	if (!IS_ANYADDR6(*(addr1 + 1)))
		return;
	/* stupid empty routing header */
	if (numa == 0) {
		COPY_ADDR6(ip->ip6_src, *addr0);
		return;
	}

	/* reverse bitmask and addresses */
	hp->ir6_sglt = numa;
	bitmsk = ntohl(hp->ir6_slmsk);
	hp->ir6_slmsk = 0;
	for (i = numa, j = 0; i >= 0; --i, j++) {
		if (bitmsk & IP6_RT_SLBIT(i))
			hp->ir6_slmsk |= htonl((u_int32_t)IP6_RT_SLBIT(j));
	}
	COPY_ADDR6(*addr1, *(addr1 + 1));
	COPY_ADDR6(ip->ip6_src, *addr1);
	addr1--;
	while (addr1 > addr0) {
		COPY_ADDR6(*addr0, addrt);
		COPY_ADDR6(*addr1, *addr0);
		COPY_ADDR6(addrt, *addr1);
		addr1--, addr0++;
	}
}

/*
 * Destination Options header input processing.
 */
/*ARGSUSED*/
void
dopt6_input(m0, ifp, ipsec, opts)
	register struct mbuf *m0;
	struct ifnet *ifp;
	struct ipsec *ipsec;
	struct mbuf *opts;
{
	register struct ipv6 *ip;
	register struct ipv6_dopthdr *hp;
	register int len;

again:
	IP6STAT_DEC(ip6s_delivered, 1);

	if (m0->m_len < sizeof(*ip) + sizeof(*hp)) {
		if ((m0 = m_pullup(m0, sizeof(*ip) + sizeof(*hp))) == 0) {
			IP6STAT(ip6s_toosmall);
			if (opts)
				m_freem(opts);
			return;
		}
	}

	ip = mtod(m0, struct ipv6 *);
	hp = (struct ipv6_dopthdr *)(ip + 1);
	len = (hp->io6_hlen + 1) * sizeof(*hp);
	if (len > ip->ip6_len) {
		IP6STAT(ip6s_toosmall);
		if (opts)
			m_freem(opts);
		m_freem(m0);
		return;
	}
	ip->ip6_nh = hp->io6_nh;
	hp->io6_nh = 0;
	ip->ip6_len -= len;
	opts = ip6_saveoption(m0, opts, len, IP6_NHDR_DOPT);
	if (opts && hd6_inoptions(m0, ifp, opts, (int *)0))
		return;
	ip = mtod(m0, struct ipv6 *);

        /*
         * Switch out to protocol's input routine.
         */
	if (ip->ip6_nh == IP6_NHDR_HOP) {
		icmp6_errparam(m0, opts, (long)&((struct ipv6 *)0)->ip6_nh,
		  ifp, ICMP6_PARAMPROB_NH);
		return;
	}
	IP6STAT(ip6s_delivered);
	IP6STAT(ip6s_inhist[ip->ip6_nh]);
	/* remove tail recursion */
	if (ip->ip6_nh == IP6_NHDR_DOPT)
		goto again;
	(*inet6sw[ip6_protox[ip->ip6_nh]].pr_input)(m0, ifp, 0, opts);
        return;
}

/*
 * Authentication Header input processing.
 */
/*ARGSUSED*/
void
ah6_input(m0, ifp, ipsec, opts)
	register struct mbuf *m0;
	struct ifnet *ifp;
	struct ipsec *ipsec;
	struct mbuf *opts;
{
	register struct ipv6 *ip;
	register struct ipv6_authhdr *hp;
	register struct ipsec_entry_header *ieh = 0;
	register caddr_t state;
	struct mbuf *m = 0;
	int len, hlen;
	struct sockaddr_ipsec ips6addr;
	char hash0[64], hash[64];

	IP6STAT_DEC(ip6s_delivered, 1);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_AH)
		printf("ah6_input(%p,%p)\n", m0, opts);
#endif

	if (m0->m_len < sizeof(*ip) + sizeof(*hp)) {
		if ((m0 = m_pullup(m0, sizeof(*ip) + sizeof(*hp))) == 0) {
			IP6STAT(ip6s_toosmall);
			if (opts)
				m_freem(opts);
			return;
		}
	}

	ip = mtod(m0, struct ipv6 *);
	hp = (struct ipv6_authhdr *)(ip + 1);
	hlen = hp->ah6_hlen * 8;
	len = hlen + sizeof(*hp);
	if (len > ip->ip6_len) {
		IP6STAT(ip6s_toosmall);
		if (opts)
			m_freem(opts);
		m_freem(m0);
		return;
	}
	if (m0->m_len < sizeof(*ip) + len) {
		if ((m0 = m_pullup(m0, sizeof(*ip) + len)) == 0) {
			IP6STAT(ip6s_toosmall);
			if (opts)
				m_freem(opts);
			return;
		}
	}
	ip = mtod(m0, struct ipv6 *);
	hp = (struct ipv6_authhdr *)(ip + 1);
	if (hp->ah6_spi == 0)
		goto drop;

	ips6addr.sis_len = sizeof (ips6addr);
	ips6addr.sis_family = AF_INET6;
	ips6addr.sis_way = IPSEC_IN;
	ips6addr.sis_kind = IP6_NHDR_AUTH;
	ips6addr.sis_spi = hp->ah6_spi;
	COPY_ADDR6(ip->ip6_dst, ips6addr.sis_addr);
	ieh = ipsec_lookup(&ips6addr, NULL);
	if (ieh)
		ieh->ieh_refcnt--;
	if (ieh == 0 ||
	    (ieh->ieh_algo == 0) ||
	    (ieh->ieh_algo->isa_ah_reslen > hlen) ||
	    (ieh->ieh_algo->isa_ah_reslen > sizeof(hash0)))
		goto bad;

	ieh->ieh_use++;
	bcopy((caddr_t)(hp + 1), hash0, ieh->ieh_algo->isa_ah_reslen);
	bzero((caddr_t)(hp + 1), ieh->ieh_algo->isa_ah_reslen);
	if ((m = ah6_build(m0, opts, IPSEC_IN)) == 0)
		goto drop;
	if ((state = ieh->ieh_algo->isa_ah_init(ieh)) == 0)
		goto drop;
	state = ieh->ieh_algo->isa_ah_update(state, m);
	ieh->ieh_algo->isa_ah_finish(state, hash, ieh);
	if (bcmp(hash0, hash, ieh->ieh_algo->isa_ah_reslen) == 0) {
		m0->m_flags |= M_AUTH;
#ifdef IP6PRINTFS
		if (ip6printfs & D6_AH)
			printf("ah6_input -> %p\n", ieh);
#endif
	} else {
	    bad:
		cmn_err(CE_WARN,
"ah6_input: Auth failed for packet from %s to %s SPI %x%sflow %x nh %d\n",
		    ip6_sprintf(&ip->ip6_src),
		    ip6_sprintf(&ip->ip6_dst),
		    ntohl(hp->ah6_spi),
		    m == NULL ? "? " : " ",
		    IPV6_GET_FLOWLABEL(ip->ip6_head),
		    hp->ah6_nh);
		m_freem(m);
		m_freem(m0);
		if (opts)
			m_freem(opts);
		return;
	    drop:
		m0->m_flags &= ~M_AUTH;
	}
	if (m)
		m_freem(m);

	ip->ip6_nh = hp->ah6_nh;
	hp->ah6_nh = 0;
	ip->ip6_len -= len;
	if (m0->m_flags & M_AUTH) {
		opts = ip6_saveoption(m0, opts, len, IP6_NHDR_AUTH);
		if (opts && OPT6_TYPE(opts) == IP6_NHDR_AUTH)
			*OPT6_DATA(opts, struct ipsec_entry_header **) = ieh;
	} else
		ip6_deloption(m0, len);
	ip = mtod(m0, struct ipv6 *);

	/*
	 * Switch out to protocol's input routine.
	 */
	if ((ip->ip6_nh == IP6_NHDR_HOP) || (ip->ip6_nh == IP6_NHDR_FRAG)) {
		icmp6_errparam(m0, opts, (long)&((struct ipv6 *)0)->ip6_nh,
		  ifp, ICMP6_PARAMPROB_NH);
		return;
	}
	IP6STAT(ip6s_delivered);
	IP6STAT(ip6s_inhist[ip->ip6_nh]);
	(*inet6sw[ip6_protox[ip->ip6_nh]].pr_input)(m0, ifp, 0, opts);
	return;
}

/*
 * Encryption Security Payload input processing.
 */
/*ARGSUSED*/
void
esp6_input(m0, ifp, ipsec, opts)
	register struct mbuf *m0;
	struct ifnet *ifp;
	struct ipsec *ipsec;
	struct mbuf *opts;
{
	register struct ipv6 *ip;
	register struct ipv6_esphdr *hp;
	register struct ipsec_entry_header *ieh;
	caddr_t state;
	u_char *p;
	struct mbuf *m = 0;
	int len, padlen = -2, nh = 0, len0;
	struct sockaddr_ipsec ips6addr;

	IP6STAT_DEC(ip6s_delivered, 1);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_ESP)
		printf("esp6_input(%p,%p)\n", m0, opts);
#endif
	if (m0->m_len < sizeof(*ip) + sizeof(*hp)) {
		if ((m0 = m_pullup(m0, sizeof(*ip) + sizeof(*hp))) == 0) {
			IP6STAT(ip6s_toosmall);
			if (opts)
				m_freem(opts);
			return;
		}
	}
	ip = mtod(m0, struct ipv6 *);
	hp = (struct ipv6_esphdr *)(ip + 1);
	if (hp->esp6_spi == 0)
		goto bad;

	ips6addr.sis_len = sizeof (ips6addr);
	ips6addr.sis_family = AF_INET6;
	ips6addr.sis_way = IPSEC_IN;
	ips6addr.sis_kind = IP6_NHDR_ESP;
	ips6addr.sis_spi = hp->esp6_spi;
	COPY_ADDR6(ip->ip6_dst, ips6addr.sis_addr);
	ieh = ipsec_lookup(&ips6addr, NULL);
	if (ieh)
		ieh->ieh_refcnt--;
	if (ieh == 0 || (ieh->ieh_algo == 0)) {
		cmn_err(CE_WARN,
"esp6_input: Crypt failed for packet from %s to %s SPI %x? flow %x\n",
		    ip6_sprintf(&ip->ip6_src),
		    ip6_sprintf(&ip->ip6_dst),
		    ntohl(hp->esp6_spi),
		    IPV6_GET_FLOWLABEL(ip->ip6_head));
		goto bad;
	}
	len = sizeof(*hp) + ieh->ieh_ivlen;
	if (len > ip->ip6_len) {
		IP6STAT(ip6s_toosmall);
		if (opts)
			m_freem(opts);
		m_freem(m0);
		return;
	}
	if (m0->m_len < sizeof(*ip) + len) {
		if ((m0 = m_pullup(m0, sizeof(*ip) + len)) == 0) {
			IP6STAT(ip6s_toosmall);
			if (opts)
				m_freem(opts);
			return;
		}
	}
	ip = mtod(m0, struct ipv6 *);
	hp = (struct ipv6_esphdr *)(ip + 1);

	ieh->ieh_use++;
	if ((m = esp6_build(m0, ieh->ieh_ivlen, -2, 0)) == 0)
		goto bad;
	if ((state = ieh->ieh_algo->isa_esp_init(ieh, (caddr_t)(hp + 1))) == 0)
		goto bad;
	state = ieh->ieh_algo->isa_esp_decrypt(state, m);
	ieh->ieh_algo->isa_esp_finish(state);
#ifdef IP6PRINTFS
	if (ip6printfs & D6_ESP)
		printf("esp6_input -> %p with %p\n", m, ieh);
#endif
	m0->m_flags |= M_CRYPT;
	if (m0->m_next)
		m_freem(m0->m_next);
	m0->m_next = m;
	m0->m_len = sizeof(struct ipv6) + len;
	len0 = m0->m_len + m->m_len;
	while (m->m_next) {
		m = m->m_next;
		len0 += m->m_len;
	}
	p = mtod(m, u_char *) + m->m_len;
	m = 0;
	nh = (int)*--p;
	padlen = (int)*--p + 2;
	if ((padlen < 0) || (padlen > len0 - m0->m_len))
		goto bad;
	m_adj(m0, -padlen);

	ip->ip6_nh = nh;
	ip->ip6_len -= len + padlen;
	opts = ip6_saveoption(m0, opts, len, IP6_NHDR_ESP);
	if (opts && OPT6_TYPE(opts) == IP6_NHDR_ESP)
		*OPT6_DATA(opts, struct ipsec_entry_header **) = ieh;
	ip = mtod(m0, struct ipv6 *);

	/*
	 * Switch out to protocol's input routine.
	 */
	if ((ip->ip6_nh == IP6_NHDR_HOP) || (ip->ip6_nh == IP6_NHDR_FRAG)) {
		icmp6_errparam(m0, opts, (long)&((struct ipv6 *)0)->ip6_nh,
		  ifp, ICMP6_PARAMPROB_NH);
		return;
	}
	IP6STAT(ip6s_delivered);
	IP6STAT(ip6s_inhist[ip->ip6_nh]);
	(*inet6sw[ip6_protox[ip->ip6_nh]].pr_input)(m0, ifp, 0, opts);
	return;

    bad:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_ESP)
		printf("esp6_input failed\n");
#endif
	if (m)
		m_freem(m);
	m_freem(m0);
	if (opts)
		m_freem(opts);
}

/*
 * No Next Header header input processing.
 */
/*ARGSUSED*/
void
end6_input(m0, ifp, ipsec, opts)
	register struct mbuf *m0;
	struct ifnet *ifp;
	struct ipsec *ipsec;
	struct mbuf *opts;
{
	if (opts)
		m_freem(opts);
	m_freem(m0);
}

/*
 * Generic option ctlinput function.
 */
void
opt6_ctlinput(cmd, sa, arg0)
	int cmd;
	struct sockaddr *sa;
	void *arg0;
{
	struct ctli_arg *ca = (struct ctli_arg *)arg0;
	register struct ipv6 *ip0 = 0;
	register struct mbuf *m0 = 0;
	struct ipv6 *ip;
	struct mbuf *m = 0;
	int type, off, len;
	void (*ctlfunc) __P((int, struct sockaddr *, void *));
	struct ctli_arg arg;

	if (ca) {
		ip0 = ca->ctli_ip;
		m0 = ca->ctli_m;
	}
	if ((unsigned)cmd >= PRC_NCMDS ||
	    ip0 == 0 || m0 == 0)
		return;
	off = (int)((caddr_t)ip0 - mtod(m0, caddr_t));
	type = ip0->ip6_nh;
	if ((m = m_copy(m0, off, M_COPYALL)) == 0)
		return;
	/* keep the m_pullup in case of a shared cluster */
	if ((m = m_pullup(m, sizeof(struct ipv6) + 8)) == 0)
		return;
	ip = mtod(m, struct ipv6 *);

    again:
	switch (type) {
	    case IP6_NHDR_HOP: {
		struct ipv6_h2hhdr *hp;

		hp = (struct ipv6_h2hhdr *)(ip + 1);
		len = (hp->ih6_hlen + 1) * sizeof(*hp);
		type = hp->ih6_nh;
		}
		break;

	    case IP6_NHDR_RT: {
		struct ipv6_rthdr *hp;

		hp = (struct ipv6_rthdr *)(ip + 1);
		len = (hp->ir6_hlen + 1) * sizeof(*hp);
		if ((hp->ir6_type != IP6_LSRRT) ||
		    (hp->ir6_hlen & 1) ||
		    (hp->ir6_hlen > (IP6_RT_MAX << 1)))
			goto freeit;

		type = hp->ir6_nh;
		if (hp->ir6_sglt == 0)
			break;
		/* in transit */
		off = len + sizeof(struct ipv6);
		m_datacopy(m, off - sizeof(struct in6_addr),
		  sizeof(struct in6_addr), (caddr_t)&satosin6(sa)->sin6_addr);
		}
		break;

	    case IP6_NHDR_FRAG: {
		struct ipv6_fraghdr *hp;

		hp = (struct ipv6_fraghdr *)(ip + 1);
		if (ntohs(hp->if6_off) & IP6_OFFMASK)
			goto freeit;
		len = sizeof(struct ipv6_fraghdr);
		type = hp->if6_nh;
		}
		break;

	    case IP6_NHDR_AUTH: {
	    	struct ipv6_authhdr *hp;

		hp = (struct ipv6_authhdr *)(ip + 1);
		len = (hp->ah6_hlen + 1) * sizeof(*hp);
		type = hp->ah6_nh;
		}

	    case IP6_NHDR_DOPT: {
		struct ipv6_dopthdr *hp;

		hp = (struct ipv6_dopthdr *)(ip + 1);
		len = (hp->io6_hlen + 1) * sizeof(*hp);
		type = hp->io6_nh;
		/* Should deal with the OPT6_HOME_ADDR option! */
		}
		break;

	    default:
		goto freeit;
	}
	m_adj(m, len);
	/* keep the m_pullup in case of a shared cluster */
	if ((m = m_pullup(m, sizeof(struct ipv6) + 8)) == 0)
		return;
	bcopy((caddr_t)ip0, mtod(m, caddr_t), sizeof(struct ipv6));
	ip = mtod(m, struct ipv6 *);
	ip->ip6_nh = type;
	switch (type) {
	    case IP6_NHDR_HOP:
	    case IP6_NHDR_RT:
	    case IP6_NHDR_FRAG:
	    case IP6_NHDR_AUTH:
	    case IP6_NHDR_ESP:
	    case IP6_NHDR_DOPT:
		goto again;
	}
	arg.ctli_ip = ip;
	arg.ctli_m = m;
	ctlfunc = inet6sw[ip6_protox[type]].pr_ctlinput;
	if (ctlfunc)
		(*ctlfunc)(cmd, sa, &arg);
freeit:
	if (m)
		m_freem(m);
}

/*
 * Forward an IPv6 packet.
 * If some error occurs return the sender an ICMPv6 packet.
 * If not forwarding, just drop the packet.
 */
static void
ip6_forward(
	struct mbuf *m,
	int srcrt,
	struct ifnet *ifp,
	union   route_6 *forward_rt)
{
#define ip6forward_rt	forward_rt->route
	register struct ipv6 *ip = mtod(m, struct ipv6 *);
	register struct sockaddr_in6 *sin;
	register struct rtentry *rt;
	int error = 0, type = 0, code = 0;
	struct mbuf *mcopy;
	struct in6_addr dest;
	struct ifnet *destifp;

	CLR_ADDR6(dest);
#ifdef IP6PRINTFS
	if (ip6printfs & D6_FORWARD)
		printf("ip6_forward: src %s dst %s hlim %d\n",
		       ip6_sprintf(&ip->ip6_src),
		       ip6_sprintf(&ip->ip6_dst),
		       ip->ip6_hlim);
#endif

	if (IS_MULTIADDR6(ip->ip6_dst) ||
	    m->m_flags & (M_BCAST|M_MCAST) ||
	    IS_LINKLADDR6(ip->ip6_src)) {
		IP6STAT(ip6s_cantforward);
		m_freem(m);
		return;
	}
	if (ip->ip6_hlim <= IPTTLDEC) {
		icmp6_error(m, ICMP6_TIMXCEED, ICMP6_TIMXCEED_INTRANS,
		  NULL, ifp);
		return;
	}
	ip->ip6_hlim -= IPTTLDEC;

	sin = satosin6(&ip6forward_rt.ro_dst);
	if ((rt = ip6forward_rt.ro_rt) == 0 ||
	    !SAME_ADDR6(ip->ip6_dst, sin->sin6_addr)) {
		if (ip6forward_rt.ro_rt) {
			RTFREE(ip6forward_rt.ro_rt);
			ip6forward_rt.ro_rt = 0;
		}
		sin->sin6_family = AF_INET6;
		sin->sin6_len = sizeof(*sin);
		COPY_ADDR6(ip->ip6_dst, sin->sin6_addr);

		rtalloc(&ip6forward_rt);
		rt = ip6forward_rt.ro_rt;
		if (rt == 0) {
			icmp6_error(m, ICMP6_UNREACH,
				    ICMP6_UNREACH_NOROUTE, NULL, ifp);
			return;
		}
		/* out of link routing */
		if (IS_LINKLADDR6(ip->ip6_dst) &&
		  ifp != rt->rt_ifp) {
			icmp6_error(m, ICMP6_UNREACH,
			  ICMP6_UNREACH_ADDRESS, NULL, ifp);
			return;
		}
		/* out of site routing */
		if ((IS_SITELADDR6(ip->ip6_dst) ||
		  IS_SITELADDR6(ip->ip6_src)) &&
		  ((rt->rt_ifp->if_site6 == 0) ||
		  (ifp && (ifp->if_site6 != rt->rt_ifp->if_site6)))) {
			if (IS_SITELADDR6(ip->ip6_src)) {
				IP6STAT(ip6s_cantforward);
				m_freem(m);
				return;
			}
			icmp6_error(m, ICMP6_UNREACH,
			  ICMP6_UNREACH_ADDRESS, NULL, ifp);
			return;
		}
	}

	/*
	 * Save at most IP6_MMTU bytes of the packet in case
	 * we need to generate an ICMP message to the src.
	 */
	mcopy = m_copy(m, 0, imin((int)ip->ip6_len + sizeof(*ip), IP6_MMTU));

	/*
	 * If forwarding packet using same interface that it came in on,
	 * perhaps should send a redirect to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 */
	if (rt->rt_ifp == ifp &&
	    (rt->rt_ifp->if_flags & IFF_POINTOPOINT) == 0 &&
	    !srcrt) {
		if (rt->rt_flags & RTF_GATEWAY) {
			COPY_ADDR6(satosin6(rt->rt_gateway)->sin6_addr, dest);
		} else {
			COPY_ADDR6(ip->ip6_dst, dest);
		}
		type = ICMP6_REDIRECT;
#ifdef IP6PRINTFS
		if (ip6printfs & D6_FORWARD)
		        printf("redirect to %s\n", ip6_sprintf(&dest));
#endif
	}

	/*
	 * TODO
	 * Hop-by-Hop header must be processed !!!
	 * (cf Daniel S. Decasper)
	 */
	if ((srcrt == 0) && (ip->ip6_nh == IP6_NHDR_HOP)) {
		register struct ipv6_h2hhdr *hp;
		register struct mbuf *opts;
		int len, alert = 0;

		if (m->m_len < sizeof(*ip) + sizeof(*hp)) {
			if ((m = m_pullup(m, sizeof(*ip)+sizeof(*hp))) == 0) {
				return;
			}
		}

		ip = mtod(m, struct ipv6 *);
		hp = (struct ipv6_h2hhdr *)(ip + 1);
		len = (hp->ih6_hlen + 1) * sizeof(*hp);
		if (len > ip->ip6_len) {
			m_freem(m);
			return;
		}
		ip->ip6_nh = hp->ih6_nh;
		hp->ih6_nh = 0;
		ip->ip6_len -= len;
		opts = ip6_saveoption(m, NULL, len, IP6_NHDR_HOP);
		if (opts && hd6_inoptions(m, ifp, opts, &alert))
			return;
		m = ip6_insertoption(m, opts, NULL, 0, &error);
		ip = mtod(m, struct ipv6 *);
		if (alert) {
			struct mbuf *n;

			n = m_copy(m, 0, M_COPYALL);
			if (n)
				hop6_input(n, ifp, NULL, NULL);
		}
	}

	error = ip6_output(m, 0, &ip6forward_rt, IP_FORWARDING, 0, 0);
	if (error)
		IP6STAT(ip6s_cantforward);
	else {
		IP6STAT(ip6s_forward);
		if (!type) {
			if (mcopy)
				m_freem(mcopy);
			return;
		}
	}
	if (mcopy == NULL)
		return;
	destifp = NULL;

	switch (error) {

	case 0:				/* forwarded, but need redirect */
		if (type == ICMP6_REDIRECT)
			redirect6_output(mcopy, rt);
		return;

	case ENETUNREACH:		/* shouldn't happen, checked above */
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP6_UNREACH;
		code = ICMP6_UNREACH_ADDRESS;
		break;

	case EMSGSIZE:
		type = ICMP6_PKTTOOBIG;
		IP6STAT(ip6s_toobig);
		if (ip6forward_rt.ro_rt)
			destifp = ip6forward_rt.ro_rt->rt_ifp;
		break;

	case ENOBUFS:
		m_freem(mcopy);
		return;
	}
	icmp6_error(mcopy, type, code, (caddr_t)destifp, ifp);
}

#ifndef sgi
int
ip6_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	u_int val;
	int errno;
	extern u_int ndpt_keep, ndpt_reachable, ndpt_retrans, ndpt_probe,
	    ndpt_down, ndp_umaxtries, ndp_mmaxtries, icmp6_ipsec;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case IP6CTL_FORWARDING:
		val = ip6forwarding;
		errno = sysctl_int(oldp, oldlenp, newp, newlen, &val);
		if (errno == 0)
			ip6forwarding = val != 0;
		/* the allrouters_group join/leave is done by the user */
		return (errno);
	case IP6CTL_FORWSRCRT:
		val = ip6forwsrcrt;
		errno = sysctl_int(oldp, oldlenp, newp, newlen, &val);
		if (errno == 0)
			ip6forwsrcrt = val != 0;
		return (errno);
	case IP6CTL_DEFTTL:
		if (newp)
			cmn_err(CE_WARN,
			    "default TTL is shared between IPv4 and IPv6\n");
		return (sysctl_int(oldp, oldlenp, newp, newlen, &ip_defttl));
	case IP6CTL_PRUNE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &ip6_prune));
	case IP6CTL_KEEP:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &ndpt_keep));
	case IP6CTL_REACHABLE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &ndpt_reachable));
	case IP6CTL_RETRANS:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &ndpt_retrans));
	case IP6CTL_PROBE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &ndpt_probe));
	case IP6CTL_DOWN:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &ndpt_down));
	case IP6CTL_UMAXTRIES:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &ndp_umaxtries));
	case IP6CTL_MMAXTRIES:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &ndp_mmaxtries));
	case IP6CTL_ICMPSEC:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &icmp6_ipsec));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
#endif

void
log(int level, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	icmn_err(level, fmt, ap);
	va_end(ap);
}

void
ip6ip6_input(struct mbuf *m, struct ifnet *ifp,
  struct ipsec *ipsec, struct mbuf *opts)
{
	m->m_flags &= ~(M_AUTH|M_CRYPT);
	m->m_len -= sizeof(struct ipv6);
	m->m_off += sizeof(struct ipv6);
	ip6_input(m, ifp, ipsec, opts);
}
#endif /* INET6 */
