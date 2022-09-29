/*
 * Copyright (c) 1985, 1986, 1993
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
 *	@(#)in_var.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IN6_VAR_H_
#define _NETINET_IN6_VAR_H_

/*
 * Interface address, IPv6 version.  One of these structures
 * is allocated for each interface with an IPv6 address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */
struct in6_ifaddr {
	struct	ifaddr ia_ifa;		/* protocol-independent info */
#define ia_hashbucket   ia_ifa.ifa_hashbucket
#define	ia_ifp		ia_ifa.ifa_ifp
#define ia_flags	ia_ifa.ifa_flags
#define ia_addrflags    ia_ifa.ifa_addrflags
	struct	sockaddr_in6 ia_addr;	/* space for interface name */
	struct	sockaddr_in6 ia_dstaddr; /* space for broadcast addr */
	struct	sockaddr_in6 ia_sockmask; /* space for general netmask */
};

struct	in6_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_in6 ifra_addr;
	struct	sockaddr_in6 ifra_dstaddr;
	struct	sockaddr_in6 ifra_mask;
	int	cookie;			/* SIOCLIFADDR enumeration cookie */
};

struct in6_ifreq {
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_in6 ifr_Addr;
};

/*
 * Given a pointer to an in6_ifaddr (ifaddr),
 * return a pointer to the addr as a sockaddr_in6.
 */
#define	IA_SIN6(ia) (&(((struct in6_ifaddr *)(ia))->ia_addr))
#define	IA_DSTSIN6(ia) (&(((struct in6_ifaddr *)(ia))->ia_dstaddr))

#define SIOCSIFADDR6  _IOW('i', 12, struct in6_ifreq)   /* set ifnet address */
#define SIOCGIFADDR6 _IOWR('i', 33, struct in6_ifreq)   /* get ifnet address */
#define SIOCSIFDSTADDR6  _IOW('i', 14, struct in6_ifreq)/* set p-p address */
#define SIOCSIFNETMASK6 _IOW('i', 22, struct in6_ifreq) /* set net addr mask */
#define SIOCGIFDSTADDR6 _IOWR('i', 34, struct in6_ifreq)/* get p-p address */
#define SIOCLIFADDR6 _IOWR('i', 35, struct in6_aliasreq)/* return IF alias */
#define SIOCGIFNETMASK6 _IOWR('i', 37, struct in6_ifreq)/* get net addr mask */

#define SIOCDIFADDR6  _IOW('i', 25, struct in6_ifreq)   /* delete IF addr */
#define SIOCAIFADDR6  _IOW('i', 26, struct in6_aliasreq)/* add/chg IF alias */
#define SIOCVIFADDR6  _IOW('i', 40, struct in6_aliasreq) /* validate IF addr */

#define SIOCADDANY6   _IOW('i', 41, struct in6_ifreq)   /* add an anycast */
#define SIOCDELANY6   _IOW('i', 42, struct in6_ifreq)   /* delete an anycast */

#define SIOCGIFSITE6  _IOWR('i', 43, struct ifreq)      /* get ifnet site */
#define SIOCSIFSITE6  _IOW('i', 44, struct ifreq)       /* set ifnet site */

#if defined(_KERNEL) && defined(INET6)

extern struct ifqueue ip6intrq;
extern int in6_multi_match(struct hashbucket *h, caddr_t key,
  caddr_t arg1, caddr_t arg2);

extern int in6addr_match(struct hashbucket *h, caddr_t key,
  caddr_t arg1, caddr_t arg2);

/*
 * Macro for finding the interface (ifnet structure) corresponding to one
 * of our IPv6 addresses.
 */
#define IN6ADDR_TO_IFP(addr, ifp) \
	/* struct in6_addr addr; */ \
	/* struct ifnet *ifp; */ \
{ \
	struct in6_ifaddr *ia;						\
	ia = (struct in6_ifaddr *)hash_lookup(&hashinfo_in6addr,	\
	  in6addr_match, (caddr_t)&(addr), (caddr_t)0, (caddr_t)0);	\
	(ifp) = (((ia) ? ia->ia_ifp : (struct ifnet *)0));		\
}

/*
 * Macro for finding the IPv6 address structure (in6_ifaddr) corresponding
 * to a given interface (ifnet structure).
 */
#define IFP_TO_IA6(ifp, ia) \
	/* struct ifnet *ifp; */ \
	/* struct in6_ifaddr *ia; */ \
{ \
	extern int ifnet_enummatch(struct hashbucket *h, caddr_t key,   \
	  caddr_t arg1, caddr_t arg2);					\
	(ia) = (struct in6_ifaddr *)hash_enum(&hashinfo_in6addr, \
	  ifnet_enummatch, HTF_INET, 0, (caddr_t)ifp, 0);	\
}
#endif

/*
 * IPv6 multicast address structure.  There is one of these for each IPv6
 * multicast group to which this host belongs on a given network interface.
 * They are kept in a linked list, rooted in the interface's in6_ifaddr
 * structure.
 */
struct in6_multi {
	struct hashbucket hashbucket;   /* hash bucket header */
	struct	in6_addr inm6_addr;	/* IPv6 multicast address */
	struct	ifnet *inm6_ifp;	/* back pointer to ifnet */
	u_int	inm6_refcount;		/* no. membership claims by sockets */
	u_int	inm6_timer;		/* ICMPv6 membership report timer */
	u_int	inm6_state;		/* state of the membership */
};

#define MULTI6_DELAYING_MEMBER		1
#define MULTI6_IDLE_MEMBER		2
#define MULTI6_LAZY_MEMBER		3

#if defined(_KERNEL) && defined(INET6)

/*
 * Macro for looking up the in6_multi record
 * for a given IPv6 multicast address on a given interface.
 * If no matching record is found, "inm" returns NULL.
 */
#define IN6_LOOKUP_MULTI(addr, ifp, inm) \
	/* struct in6_addr addr; */ \
	/* struct ifnet *ifp; */ \
	/* struct in6_multi *inm; */ \
{ \
	(inm) = (struct in6_multi *)hash_lookup(&hashinfo_in6addr, \
	  in6_multi_match, (caddr_t)(&(addr)), (caddr_t)(ifp), (caddr_t)0); \
}

int	in6_ifinit __P((struct ifnet *,
	    struct in6_ifaddr *, struct sockaddr_in6 *, int));
struct	in6_multi *in6_addmulti __P((struct in6_addr *, struct ifnet *));
void	in6_delmulti __P((struct in6_multi *));
void	in6_ifscrub __P((struct ifnet *, struct in6_ifaddr *));
int	in6_control __P((struct socket *, __psint_t, caddr_t, struct ifnet *));

/*
 * Anycast address structure.
  */

struct in6_anycast {
#ifdef _KERNEL
	struct {
		struct in6_anycast *le_next;
		struct in6_anycast **le_prev;
	} ina6_list;      /* list glue */
#endif
	struct  in6_addr ina6_addr;             /* IPv6 anycast address */
	u_int   ina6_refcount;                  /* reference count */
	u_int   ina6_flags;			/* flags */
};
#define IP6ANY_VALID            1               /* valid entry */
#define IP6ANY_ROUTER           2               /* router anycast */
#define IP6ANY_ALLWAYS          3               /* allways get it! */

extern struct hashinfo hashinfo_in6addr;

/*
 * Functions for manipulating ipv6 options in the kernel.  The ip6_options
 * structure is stored in an mbuf so we use m_len to store the length of
 * the option.
 */
struct ip6_opthdr {
	int opttype;
	int pad;		/* to keep o_optdata 64bit aligned */
};

struct ip6_options {
	struct ip6_opthdr o_hdr;
	char o_optdata[1];	/* variable length */
};

#define OPT6_HDRSIZE	  sizeof(struct ip6_opthdr)
#define OPT6_TYPE(opt)	  (mtod(opt, struct ip6_options *)->o_hdr.opttype)
#define OPT6_DATA(opt,t)  ((t)(mtod(opt, struct ip6_options *)->o_optdata))
#define OPT6_LEN(opt)	  ((opt)->m_len - OPT6_HDRSIZE)

#endif

/*
 * Definitions for IPv6 sysctl operations.
 *
 * Third level is protocol number.
 * Fourth level is desired variable within that protocol.
 */
#define	IP6PROTO_MAXID	(1)		/* don't list to IP6PROTO_MAX */

#define	CTL_IP6PROTO_NAMES { \
	{ "ipv6", CTLTYPE_NODE }, \
}

/*
 * Names for IPv6 sysctl objects
 */
#define	IP6CTL_FORWARDING	1	/* act as router */
#define	IP6CTL_FORWSRCRT	2	/* forward source-routed packets */
#define IP6CTL_DEFTTL		3	/* default TTL */
#define	IP6CTL_PRUNE		4	/* route prune timer */
#define	IP6CTL_KEEP		5	/* route default lifetime */
#define	IP6CTL_REACHABLE	6	/* reachable time */
#define	IP6CTL_RETRANS		7	/* retrans timer */
#define	IP6CTL_PROBE		8	/* delay first probe */
#define	IP6CTL_DOWN		9	/* hold down timer */
#define	IP6CTL_UMAXTRIES	10	/* max unicast solicit */
#define	IP6CTL_MMAXTRIES	11	/* max multicast solicit */
#define	IP6CTL_ICMPSEC		12	/* ICMPv6 IPSec */
#define	IP6CTL_MAXID		13

#define	IP6CTL_NAMES { \
	{ 0, 0 }, \
	{ "forwarding", CTLTYPE_INT }, \
	{ "forwsrcrt", CTLTYPE_INT }, \
	{ "ttl", CTLTYPE_INT }, \
	{ "prune", CTLTYPE_INT }, \
	{ "keep", CTLTYPE_INT }, \
	{ "reachable", CTLTYPE_INT }, \
	{ "retrans", CTLTYPE_INT }, \
	{ "probe", CTLTYPE_INT }, \
	{ "hold_down", CTLTYPE_INT }, \
	{ "utries_max", CTLTYPE_INT }, \
	{ "mtries_max", CTLTYPE_INT }, \
	{ "icmp_ipsec", CTLTYPE_INT }, \
}
#endif
