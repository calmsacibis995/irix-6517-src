#ifndef __net_route__
#define __net_route__
#ifdef __cplusplus
extern "C" { 
#endif
/*
 * Copyright (c) 1980, 1986, 1993
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
 *	@(#)route.h	8.3 (Berkeley) 4/19/94
 */

#ifdef sgi
#ident "$Revision: 4.26 $"
#include <sys/socket.h>
#include <sys/cdefs.h>
struct rtentry;
struct mbuf;
struct socket;
#endif

/*
 * Kernel resident routing tables.
 * 
 * The routing tables are initialized when interface addresses
 * are set by making entries for all directly connected interfaces.
 */

/*
 * A route consists of a destination address and a reference
 * to a routing entry.  These are often held by protocols
 * in their control blocks, e.g. inpcb.
 */
struct route {
	struct	rtentry *ro_rt;
	struct	sockaddr ro_dst;
};

/*
 * These numbers are used by reliable protocols for determining
 * retransmission behavior and are included in the routing structure.
 */
struct rt_metrics {
	__uint32_t	rmx_locks;	/* Kernel must leave  values alone */
	__uint32_t	rmx_mtu;	/* MTU for this path */
	__uint32_t	rmx_hopcount;	/* max hops expected */
	__uint32_t	rmx_expire;	/* lifetime for route, eg. redirect */
	__uint32_t	rmx_recvpipe;	/* inbound delay-bandwith product */
	__uint32_t	rmx_sendpipe;	/* outbound delay-bandwith product */
	__uint32_t	rmx_ssthresh;	/* outbound gateway buffer limit */
	__uint32_t	rmx_rtt;	/* estimated round trip time */
	__uint32_t	rmx_rttvar;	/* estimated rtt variance */
	__uint32_t	rmx_pksent;	/* packets sent using this route */
};

/*
 * rmx_rtt and rmx_rttvar are stored as microseconds;
 * RTTTOPRHZ(rtt) converts to a value suitable for use
 * by a protocol fasttimo counter.
 */
#define	RTM_RTTUNIT	1000000	/* units for rtt, rttvar, as units per sec */
#define	RTTTOPRHZ(r)	((r) / (RTM_RTTUNIT / PR_FASTHZ))

/*
 * We distinguish between routes to hosts and routes to networks,
 * preferring the former if available.  For each route we infer
 * the interface to use from the gateway address supplied when
 * the route was entered.  Routes that forward packets through
 * gateways are marked so that the output routines know to address the
 * gateway rather than the ultimate destination.
 */
#include <net/radix.h>

struct rtentry {
	struct	radix_node rt_nodes[2];	/* tree glue, and other values */
#define	rt_key(r)	((struct sockaddr *)((r)->rt_nodes->rn_key))
#ifdef _HAVE_SA_LEN
#define	rt_mask(r)	((struct sockaddr *)((r)->rt_nodes->rn_mask))
#else
#define	rt_mask(r)	((struct sockaddr_new *)((r)->rt_nodes->rn_mask))
#endif
	struct	sockaddr *rt_gateway;	/* value */
	__uint32_t	rt_flags;		/* up/down?, host/net */
	int	rt_refcnt;		/* # held references */
	__uint32_t rt_use;		/* raw # packets forwarded */
	struct	ifnet *rt_ifp;		/* the answer: interface to use */
	struct	ifaddr *rt_ifa;		/* the answer: interface to use */
	struct	ifaddr *rt_srcifa;	/* source address to use */
#ifdef _HAVE_SA_LEN
	struct	sockaddr *rt_genmask;	/* for generation of cloned routes */
#else
	struct	sockaddr_new *rt_genmask;
#endif
	caddr_t	rt_llinfo;		/* pointer to link level info cache */
	struct	rt_metrics rt_rmx;	/* metrics used by rx'ing protocols */
	struct	rtentry *rt_gwroute;	/* implied entry for gatewayed routes */
};


/*
 * 32-bit field for routing flags 
 */
#define	RTF_UP		0x1		/* route usable */
#define	RTF_GATEWAY	0x2		/* destination is a gateway */
#define	RTF_HOST	0x4		/* host entry (net otherwise) */
#define	RTF_REJECT	0x8		/* host or net unreachable */

#define	RTF_DYNAMIC	0x10		/* created dynamically (by redirect) */
#define	RTF_MODIFIED	0x20		/* modified dynamically (by redirect)*/
#define RTF_DONE	0x40		/* message confirmed */
#define RTF_MASK	0x80		/* subnet mask present */

#define RTF_CLONING	0x100		/* generate new routes on use */
#define RTF_XRESOLVE	0x200		/* external daemon resolves name */
#define RTF_LLINFO	0x400		/* generated by ARP or ESIS */
#define RTF_STATIC	0x800		/* manually added */

#define RTF_BLACKHOLE	0x1000		/* just discard pkts (during updates)*/
#define RTF_HOSTALIAS	0x2000		/* host route for ip alias address */
#define RTF_PROTO2	0x4000		/* protocol specific routing flag */
#define RTF_PROTO1	0x8000		/* protocol specific routing flag */
#define RTF_CKSUM	0x10000		/* perform checksumming on this route */

/*
 * Routing statistics.
 */
struct	rtstat {
	short	rts_badredirect;	/* bogus redirect calls */
	short	rts_dynamic;		/* routes created by redirects */
	short	rts_newgateway;		/* routes modified by redirects */
	short	rts_unreach;		/* lookups which failed */
	short	rts_wildcard;		/* lookups satisfied by a wildcard */
};

/*
 * Structures for routing messages.
 */
struct rt_msghdr {
	u_short	rtm_msglen;	/* to skip over non-understood messages */
	u_char	rtm_version;	/* future binary compatibility */
	u_char	rtm_type;	/* message type */
	u_short	rtm_index;	/* index for associated ifp */
	__uint32_t rtm_flags;	/* flags, incl. kern & message, e.g. DONE */
	int	rtm_addrs;	/* bitmask identifying sockaddrs in msg */
	pid_t	rtm_pid;	/* identify sender */
	int	rtm_seq;	/* for sender to identify action */
	int	rtm_errno;	/* why failed */
	int	rtm_use;	/* from rtentry */
	__uint32_t rtm_inits;	/* which metrics we are initializing */
	struct	rt_metrics rtm_rmx; /* metrics themselves */
};

#define RTM_VERSION	3	/* Up the ante and ignore older versions */

#define RTM_ADD		0x1	/* Add Route */
#define RTM_DELETE	0x2	/* Delete Route */
#define RTM_CHANGE	0x3	/* Change Metrics or flags */
#define RTM_GET		0x4	/* Report Metrics */
#define RTM_LOSING	0x5	/* Kernel Suspects Partitioning */
#define RTM_REDIRECT	0x6	/* Told to use different route */
#define RTM_MISS	0x7	/* Lookup failed on this address */
#define RTM_LOCK	0x8	/* fix specified metrics */
#define RTM_OLDADD	0x9	/* caused by SIOCADDRT */
#define RTM_OLDDEL	0xa	/* caused by SIOCDELRT */
#define RTM_RESOLVE	0xb	/* req to resolve dst to LL addr */
#define RTM_NEWADDR	0xc	/* address being added to iface */
#define RTM_DELADDR	0xd	/* address being removed from iface */
#define RTM_IFINFO	0xe	/* iface going up/down etc. */
#ifdef INET6
#define RTM_EXPIRE	0xf	/* Route has expired */
#define RTM_RTLOST	0x10	/* Route has been lost */
#endif

#define RTV_MTU		0x1	/* init or lock _mtu */
#define RTV_HOPCOUNT	0x2	/* init or lock _hopcount */
#define RTV_EXPIRE	0x4	/* init or lock _hopcount */
#define RTV_RPIPE	0x8	/* init or lock _recvpipe */
#define RTV_SPIPE	0x10	/* init or lock _sendpipe */
#define RTV_SSTHRESH	0x20	/* init or lock _ssthresh */
#define RTV_RTT		0x40	/* init or lock _rtt */
#define RTV_RTTVAR	0x80	/* init or lock _rttvar */

/*
 * Bitmask values for rtm_addr.
 */
#define RTA_DST		0x1	/* destination sockaddr present */
#define RTA_GATEWAY	0x2	/* gateway sockaddr present */
#define RTA_NETMASK	0x4	/* netmask sockaddr present */
#define RTA_GENMASK	0x8	/* cloning mask sockaddr present */
#define RTA_IFP		0x10	/* interface name sockaddr present */
#define RTA_IFA		0x20	/* interface addr sockaddr present */
#define RTA_AUTHOR	0x40	/* sockaddr for author of redirect */
#define RTA_BRD		0x80	/* for NEWADDR, broadcast or p-p dest addr */

/*
 * Index offsets for sockaddr array for alternate internal encoding.
 */
#define RTAX_DST	0	/* destination sockaddr present */
#define RTAX_GATEWAY	1	/* gateway sockaddr present */
#define RTAX_NETMASK	2	/* netmask sockaddr present */
#define RTAX_GENMASK	3	/* cloning mask sockaddr present */
#define RTAX_IFP	4	/* interface name sockaddr present */
#define RTAX_IFA	5	/* interface addr sockaddr present */
#define RTAX_AUTHOR	6	/* sockaddr for author of redirect */
#define RTAX_BRD	7	/* for NEWADDR, broadcast or p-p dest addr */
#define RTAX_MAX	8	/* size of array to allocate */

struct rt_addrinfo {
	int	rti_addrs;
	struct	sockaddr *rti_info[RTAX_MAX];
};

struct route_cb {
	int	ip_count;
	int	ns_count;
	int	iso_count;
	int	any_count;
};

/* kludge to make sysctl(3) work for routing sockets
 */
struct rtsysctl {
	__int32_t *name;
	u_int	namelen;
	void	*oldp;
	size_t	oldlen;
	void	*newp;
	size_t	newlen;
};


#ifdef _KERNEL

#define RT_HOLD(rt)	atomicAddInt(&(rt)->rt_refcnt, 1)
#define RT_RELE(rt)	atomicAddInt(&(rt)->rt_refcnt, -1)
#define RT_CMPANDRELE(rt)	compare_and_dec_int_gt(&(rt)->rt_refcnt, 1)
#define	RTFREE(rt)	rtfree((rt))

extern struct	route_cb route_cb;
extern struct	rtstat	rtstat;
extern struct	radix_node_head *rt_tables[AF_MAX+1];

extern void route_init(void);
extern int  route_output(struct mbuf *, struct socket *, __uint32_t);
extern int  route_usrreq(struct socket *, int,
			 struct mbuf *, struct mbuf *, struct mbuf *);
extern void rt_ifmsg(struct ifnet *);
#ifdef _HAVE_SIN_LEN
extern void rt_maskedcopy(struct sockaddr *,
			  struct sockaddr *, struct sockaddr *);
#else
extern void rt_maskedcopy(struct sockaddr *,
			  struct sockaddr *, struct sockaddr_new *);
#endif
extern void rt_missmsg(int, struct rt_addrinfo *, int, int);
extern void rt_newaddrmsg(int, struct ifaddr *, int, struct rtentry *);
extern int  rt_setgate(struct rtentry *,
		       struct sockaddr *, struct sockaddr *);
extern void rt_setmetrics(__uint32_t, struct rt_metrics *,
			  struct rt_metrics *);
extern void rtable_init(void **);
extern void rtalloc(struct route *);
extern struct rtentry * rtalloc1(struct sockaddr *, int);
extern void rtfree(struct rtentry *);
extern void rtfree_needlock(struct rtentry *);
extern void rtfree_needpromote(struct rtentry *);
extern int  rtinit(struct ifaddr *, int, int);
extern int  rtioctl(int, caddr_t);
#ifdef _HAVE_SIN_LEN
extern void rtredirect(struct sockaddr *, struct sockaddr *,
	    struct sockaddr *, int, struct sockaddr *, struct rtentry **);
extern int  rtrequest(int, struct sockaddr *,
	    struct sockaddr *, struct sockaddr *, int, struct rtentry **);
#else
extern void rtredirect(struct sockaddr *, struct sockaddr *,
	    struct sockaddr_new *, int, struct sockaddr *, struct rtentry **);
extern int  rtrequest(int, struct sockaddr *,
	    struct sockaddr *, struct sockaddr_new *, int, struct rtentry **);
#endif
extern int  sysctl_rtable(int *, int, caddr_t, size_t *, caddr_t *, size_t);

extern mrlock_t route_lock;
#define ROUTE_INITLOCK()	mrlock_init(&route_lock, MRLOCK_BARRIER, \
					"routelock", 0)
#define ROUTE_RDLOCK()		mraccess(&route_lock)
#define ROUTE_WRLOCK()		mrupdate(&route_lock)
#define ROUTE_UNLOCK()		mrunlock(&route_lock)
#define ROUTE_ISLOCKED()	mrislocked_any(&route_lock)
#define ROUTE_ISRDLOCKED()	mrislocked_access(&route_lock)
#define ROUTE_ISWRLOCKED()	mrislocked_update(&route_lock)
#define ROUTE_DEMOTE()	        mrdemote(&route_lock)

#endif

#ifdef __cplusplus
}
#endif 
#endif	/* __net_route__ */
