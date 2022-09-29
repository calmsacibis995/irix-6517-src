#ifndef __netinet_if_ether__
#define __netinet_if_ether__
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)if_ether.h	8.1 (Berkeley) 6/10/93
 */

#ifdef __sgi
#include <net/if.h>
#include <netinet/in.h>
#endif

/*
 * Structure of a 10Mb/s Ethernet header.
 */
struct	ether_header {
	u_char	ether_dhost[6];
	u_char	ether_shost[6];
	u_short	ether_type;
};

#define	ETHERTYPE_PUP		0x0200	/* PUP protocol */
#define	ETHERTYPE_IP		0x0800	/* IP protocol */
#define ETHERTYPE_IPV6		0x86dd  /* IPv6 protocol */
#define ETHERTYPE_ARP		0x0806	/* Addr. resolution protocol */
#define ETHERTYPE_DECMOP    0x6001  /* DEC dump/load (MOP) */
#define ETHERTYPE_DECCON    0x6002  /* DEC remote console */
#define ETHERTYPE_DECnet    0x6003  /* DECnet 'routing' */
#define ETHERTYPE_DECLAT    0x6004  /* DEC LAT */
#define ETHERTYPE_SG_DIAG   0x8013  /* SGI diagnostic type */
#define ETHERTYPE_SG_NETGAMES   0x8014  /* SGI network games */
#define ETHERTYPE_SG_RESV   0x8015  /* SGI reserved type */
#define ETHERTYPE_SG_BOUNCE 0x8016  /* SGI 'bounce server' */
#define ETHERTYPE_XTP       0x817D  /* Protocol Engines XTP */
#define ETHERTYPE_STP       0x8181  /* Scheduled Transfer STP */

#define ETHERTYPE_8023      0x0004  /* IEEE 802.3 packet */

#define IEEE_8023_LEN       ETHERMTU /* IEEE 802.3 Length Field */

#define	ETHERMTU	1500
#define	ETHERMIN	(60-14)

#ifdef __sgi /* MULTICAST */
#ifdef _KERNEL
/*
 * Macro to map an IP multicast address to an Ethernet multicast address.
 * The high-order 25 bits of the Ethernet address are statically assigned,
 * and the low-order 23 bits are taken from the low end of the IP address.
 */
#define ETHER_MAP_IP_MULTICAST(ipaddr, enaddr) \
	/* struct in_addr *ipaddr; */ \
	/* u_char enaddr[6];	   */ \
{ \
	(enaddr)[0] = 0x01; \
	(enaddr)[1] = 0x00; \
	(enaddr)[2] = 0x5e; \
	(enaddr)[3] = ((u_char *)ipaddr)[1] & 0x7f; \
	(enaddr)[4] = ((u_char *)ipaddr)[2]; \
	(enaddr)[5] = ((u_char *)ipaddr)[3]; \
}

/*
 * Macro to map an IP multicast address to an Token Ring multicast address.
 * This is defined in RFC 1469 and is different from Ethernet.
 * Perhaps this should be in a seperate if_token.h file but for simplicity
 * it is being added right here in if_ether.h
 */
#define TOKEN_MAP_IP_MULTICAST(ipaddr, tnaddr)				\
	/* struct in_addr *ipaddr; */					\
	/* u_char tnaddr[6];       */					\
{									\
	(tnaddr)[0] = 0xC0;						\
	(tnaddr)[1] = 0;			\
	(tnaddr)[2] = 0;			\
	(tnaddr)[3] = 4;			\
	(tnaddr)[4] = 0;			\
	(tnaddr)[5] = 0;			\
}

#endif
#endif /* __sgi */

/*
 * Ethernet Address Resolution Protocol.
 *
 * See RFC 826 for protocol description.  Structure below is adapted
 * to resolving internet addresses.  Field names used correspond to
 * RFC 826.
 */
struct	ether_arp {
	struct	arphdr ea_hdr;	/* fixed-size header */
	u_char	arp_sha[6];	/* sender hardware address */
	u_char	arp_spa[4];	/* sender protocol address */
	u_char	arp_tha[6];	/* target hardware address */
	u_char	arp_tpa[4];	/* target protocol address */
};
#define	arp_hrd	ea_hdr.ar_hrd
#define	arp_pro	ea_hdr.ar_pro
#define	arp_hln	ea_hdr.ar_hln
#define	arp_pln	ea_hdr.ar_pln
#define	arp_op	ea_hdr.ar_op


/*
 * Structure shared between the ethernet driver modules and
 * the address resolution code.  For example, each ec_softc or il_softc
 * begins with this structure.
 */
struct	arpcom {
	struct	ifnet ac_if;		/* network-visible interface */
	u_char	ac_enaddr[6];		/* ethernet hardware address */
	struct	in_addr ac_ipaddr;	/* copy of ip address- XXX */
	struct	ether_multi *ac_multiaddrs; /* list of ether multicast addrs */
	int	ac_multicnt;		/* length of ac_multiaddrs list */
};

struct llinfo_arp {
	struct	llinfo_arp *la_next;
	struct	llinfo_arp *la_prev;
	struct	rtentry *la_rt;
	struct	mbuf *la_hold;		/* held packet until resolved */
	void	*la_data;		/* driver-specific private data */
	long	la_asked;		/* # of queries for this address */
};

/*
 * SRCROUTE table
 */
#define _ARPTAB_BSIZ    16      /* bucket size */
#define _ARPTAB_NB  41      /* number of buckets */
#define _ARPTAB_SIZE    (_ARPTAB_BSIZ * _ARPTAB_NB)

struct	sritab {
	u_char	st_flags;		/* SRCROUTE flags */
	u_char	st_timer;		/* minutes since last reference */
	u_char  st_ref_cnt;		/* Reference count */
	u_char	pad1;
#define	SRIF_VALID	0x1		/* this entry has valid SRC ROUTE */
#define	SRIF_ARP	0x2		/* this entry used by ARP */
#define	SRIF_NONARP	0x4		/* this entry used by none ARP */
#define	SRIF_PERM	0x8		/* Permanat staic entry */
	u_char	st_addr[6];		/* MAC address */
	u_short	st_ri[SRI_MAXLEN];	/* Route info */
#define SRI_LENMASK	0x1f00		/* Route info len including cnotrol */
	u_short	pad2;
};

#ifdef	_KERNEL
extern void srarpinput(struct arpcom*, struct mbuf*, u_short*); /*SRCROUTE*/
extern u_short* srilook(char*, u_char);				/*SRCROUTE*/
extern char *ether_sprintf(u_char *);
#endif /* _KERNEL */

struct sockaddr_inarp {
	u_char	sarp_len;
	u_char	sarp_family;
	u_short sarp_port;
	struct	in_addr sarp_addr;
	struct	in_addr sarp_srcaddr;
	u_short	sarp_tos;
	u_short	sarp_other;
#define SARP_PROXY 1
};
/*
 * IP and ethernet specific routing flags
 */
#define RTF_ANNOUNCE	RTF_PROTO2	/* announce new arp entry */

#ifdef	_KERNEL
extern union ebcast_align {     /* align it to a short to work */
	u_char  ebcast_char[6];     /* around compiler problem */
	u_short ebcast_long;
} ebcast_align;
#define etherbroadcastaddr ebcast_align.ebcast_char

extern ushort	ether_ipmulticast_min[3];
extern ushort	ether_ipmulticast_max[3];
/*struct	ifqueue arpintrq;*/

extern struct	llinfo_arp llinfo_arp;		/* head of the llinfo queue */

#define ARP_WHOHAS(ac, addr) \
	arprequest(ac, &(ac)->ac_ipaddr, addr, (ac)->ac_enaddr)

extern int	arpwhohas(struct arpcom *ac, struct in_addr *addr);
extern void	arprequest(struct arpcom *ac, struct in_addr *sip,
		struct in_addr *tip, u_char *enaddr);
extern void	arpinput(struct arpcom *, struct mbuf *);
extern int	arpresolve(struct arpcom *, struct rtentry *, struct mbuf *,
		struct in_addr *, u_char *, int *);
extern int	ip_arpresolve(struct arpcom *, struct mbuf *, struct in_addr *,
		u_char *);
extern void	arp_rtrequest(int, struct rtentry *, struct sockaddr *);
extern struct in_ifaddr *arp_findaddr(struct ifnet *ifp,
		struct in_addr *itaddr, struct in_addr *isaddr,u_char *tenaddr,
		u_char *senaddr, int op);
extern int	send_arp(struct arpcom *ac, struct in_addr *sip,
		struct in_addr *tip, u_char *enaddr, short op,
		u_char *ether_dest, struct mbuf *m);
extern void	expire_arp(__uint32_t ipaddr);

/*
 * Ethernet multicast address structure.  There is one of these for each
 * multicast address or range of multicast addresses that we are supposed
 * to listen to on a particular interface.  They are kept in a linked list,
 * rooted in the interface's arpcom structure.  (This really has nothing to
 * do with ARP, or with the Internet address family, but this appears to be
 * the minimally-disrupting place to put it.)
 */
struct ether_multi {
	u_char	enm_addrlo[6];		/* low  or only address of range */
	u_char	enm_addrhi[6];		/* high or only address of range */
	struct	arpcom *enm_ac;		/* back pointer to arpcom */
	u_int	enm_refcount;		/* no. claims to this addr/range */
	struct	ether_multi *enm_next;	/* ptr to next ether_multi */
};

/*
 * Structure used by macros below to remember position when stepping through
 * all of the ether_multi records.
 */
struct ether_multistep {
	struct ether_multi  *e_enm;
};

/*
 * Macro for looking up the ether_multi record for a given range of Ethernet
 * multicast addresses connected to a given arpcom structure.  If no matching
 * record is found, "enm" returns NULL.
 */
#define ETHER_LOOKUP_MULTI(addrlo, addrhi, ac, enm) \
	/* u_char addrlo[6]; */ \
	/* u_char addrhi[6]; */ \
	/* struct arpcom *ac; */ \
	/* struct ether_multi *enm; */ \
{ \
	for ((enm) = (ac)->ac_multiaddrs; \
	    (enm) != NULL && \
	    (bcmp((enm)->enm_addrlo, (addrlo), 6) != 0 || \
	     bcmp((enm)->enm_addrhi, (addrhi), 6) != 0); \
		(enm) = (enm)->enm_next); \
}

/*
 * Macro to step through all of the ether_multi records, one at a time.
 * The current position is remembered in "step", which the caller must
 * provide.  ETHER_FIRST_MULTI(), below, must be called to initialize "step"
 * and get the first record.  Both macros return a NULL "enm" when there
 * are no remaining records.
 */
#define ETHER_NEXT_MULTI(step, enm) \
	/* struct ether_multistep step; */  \
	/* struct ether_multi *enm; */  \
{ \
	if (((enm) = (step).e_enm) != NULL) \
		(step).e_enm = (enm)->enm_next; \
}

#define ETHER_FIRST_MULTI(step, ac, enm) \
	/* struct ether_multistep step; */ \
	/* struct arpcom *ac; */ \
	/* struct ether_multi *enm; */ \
{ \
	(step).e_enm = (ac)->ac_multiaddrs; \
	ETHER_NEXT_MULTI((step), (enm)); \
}

#define	ETHER_ISBROAD(addr) \
	(*(u_char*)(addr) == 0xff \
	 && !bcmp(addr, etherbroadcastaddr, sizeof etherbroadcastaddr))

#define	ETHER_ISGROUP(addr) (*(u_char*)(addr) & 0x01)

#define	ETHER_ISMULTI(addr) \
	(ETHER_ISGROUP(addr) \
	 && bcmp(addr, etherbroadcastaddr, sizeof etherbroadcastaddr))

/*
 * The address needs filtering before the packet is accepted when we're
 * in promiscuous mode and it's not for us and it's not a broadcast, or
 * we're filtering multicasts only and it's a multicast address.
 */
#define	ETHER_NEEDSFILTER(addr, ac) \
	(((ac)->ac_if.if_flags & IFF_PROMISC) \
	 && bcmp(addr, (ac)->ac_enaddr, sizeof (ac)->ac_enaddr) \
	 && !ETHER_ISBROAD(addr) \
	 || (((ac)->ac_if.if_flags & \
		(IFF_FILTMULTI|IFF_ALLMULTI)) == IFF_FILTMULTI) \
	 && ETHER_ISMULTI(addr))

extern int ether_cvtmulti(struct sockaddr *, int *);

/*
 * Macros for copying and comparing Ethernet and IP addresses,
 * that are short aligned, but not necessarily long aligned.
 */
#define ether_copy(src, dst) \
	((short *)(dst))[0] = ((short *)(src))[0]; \
	((short *)(dst))[1] = ((short *)(src))[1]; \
	((short *)(dst))[2] = ((short *)(src))[2]

#define IP_copy(src, dst) \
	((short *)(dst))[0] = ((short *)(src))[0]; \
	((short *)(dst))[1] = ((short *)(src))[1]

#endif /* KERNEL */
#ifdef __cplusplus
}
#endif
#endif /* !__netinet_if_ether__ */
