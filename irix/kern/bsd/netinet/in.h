#ifndef __NETINET_IN_H__
#define __NETINET_IN_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Copyright (c) 1982, 1986, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution is only permitted until one year after the first shipment
 * of 4.4BSD by the Regents.  Otherwise, redistribution and use in source and
 * binary forms are permitted provided that: (1) source distributions retain
 * this entire copyright notice and comment, and (2) distributions including
 * binaries display the following acknowledgement:  This product includes
 * software developed by the University of California, Berkeley and its
 * contributors'' in the documentation or other materials provided with the
 * distribution and in all advertising materials mentioning features or use
 * of this software.  Neither the name of the University nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)in.h	7.10 (Berkeley) 6/28/90 plus MULTICAST
 */

#include <standards.h>
#include <sgidefs.h>
#if _SGIAPI
#include <sys/bsd_types.h>
#include <sys/endian.h>
#endif
#include <sys/types.h>


/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981.
 * Updated with data from RFC 1340 (7/10/92)
 */

typedef __uint32_t in_addr_t;
typedef unsigned short in_port_t;

#if !defined(_SA_FAMILY_T)
#define _SA_FAMILY_T
typedef unsigned short sa_family_t;
#endif

#if !defined(_SIZE_T) && !defined(_SIZE_T_)
#define _SIZE_T
#if (_MIPS_SZLONG == 32)
typedef unsigned int    size_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned long   size_t;
#endif
#endif

/*
 * Protocols
 */
#define	IPPROTO_IP		0		/* dummy for IP */
#define	IPPROTO_HOPOPTS		0		/* IPv6 Hop-by-Hop options */
#define	IPPROTO_ICMP		1		/* control message protocol */
#define	IPPROTO_IGMP		2		/* group mgmt protocol */
#define	IPPROTO_GGP		3		/* gateway^2 (deprecated) */
#define IPPROTO_IPIP		4		/* (multicast) encapsulation */
#define IPPROTO_ENCAP IPPROTO_IPIP		/* old name */
#define IPPROTO_ST		5		/* STream protocol */
#define	IPPROTO_TCP		6		/* tcp */
#define IPPROTO_UCL		7		/* UCL */
#define	IPPROTO_EGP		8		/* exterior gateway protocol */
#define IPPROTO_IGP		9		/* any private interior gateway */
#define IPPROTO_BBN_RCC_MON	10		/* BBN RCC Monitoring */
#define IPPROTO_NVP_II		11		/* Network Voice Protocol */
#define	IPPROTO_PUP		12		/* pup */
#define IPPROTO_ARGUS		13		/* ARGUS */
#define IPPROTO_EMCON		14		/* EMCON */
#define IPPROTO_XNET		15		/* Cross Net Debugger */
#define IPPROTO_CHAOS		16		/* Chaos */
#define	IPPROTO_UDP		17		/* user datagram protocol */
#define IPPROTO_MUX		18		/* Multiplexing */
#define IPPROTO_DCN_MEAS	19		/* DCN Measurement Subsystems */
#define IPPROTO_HMP		20		/* Host Monitoring */
#define IPPROTO_PRM		21		/* Packet Radio Measurement */
#define	IPPROTO_IDP		22		/* xns idp */
#define IPPROTO_TRUNK_1		23		/* Trunk-1 */
#define IPPROTO_TRUNK_2		24		/* Trunk-2 */
#define IPPROTO_LEAF_1		25		/* Leaf-1 */
#define IPPROTO_LEAF_2		26		/* Leaf-2 */
#define IPPROTO_RDP		27		/* Reliable Data Protocol */
#define IPPROTO_IRTP		28		/* Internet Reliable Transaction */
#define	IPPROTO_TP		29 		/* tp-4 w/ class negotiation */
#define IPPROTO_NETBLT		30		/* Bulk Data Transfer Protocol */
#define IPPROTO_MFE_NSP		31		/* MFE Network Services Protocol */
#define IPPROTO_MERIT_INP	32		/* MERIT Internodal Protocol */
#define IPPROTO_SEP		33		/* Sequential Exchange Protocol */
#define IPPROTO_3PC		34		/* Third Party Connect Protocol */
#define IPPROTO_IDPR		35		/* Inter-Domain Policy Routing Protocol */
#define	IPPROTO_XTP		36		/* XTP */
#define IPPROTO_DDP		37		/* Datagram Delivery Protocol */
#define IPPROTO_IDPR_CMTP	38		/* IDPR Control Message Transport Proto */
#define IPPROTO_TPPP		39		/* TP++ Transport Protocol */
#define IPPROTO_IL		40		/* IL Transport Protocol */
#define IPPROTO_IPV6		41		/* IPv6 into IPv4 */
#define IPPROTO_ROUTING		43		/* IPv6 Routing header */
#define IPPROTO_FRAGMENT	44		/* IPv6 fragmentation header */
#define IPPROTO_RSVP		46		/* resource reservation proto */
#define IPPROTO_ESP		50		/* encap security payload */
#define IPPROTO_AH		51		/* authentication header */
#define IPPROTO_ICMPV6		58		/* ICMPv6/IGMPv6/NDPv6 */
#define IPPROTO_NONE		59		/* IPv6 no next header */
#define IPPROTO_DSTOPTS		60		/* IPv6 Destination options */
#define IPPROTO_CFTP		62		/* CFTP */
#define IPPROTO_HELLO		63	/* depricated "hello" routing protocol */
#define IPPROTO_SAT_EXPAK	64		/* SATNET and Backroom EXPAK */
#define IPPROTO_KRYPTOLAN	65		/* Kryptolan */
#define IPPROTO_RVD		66		/* MIT Remote Virtual Disk Protocol */
#define IPPROTO_IPPC		67		/* Internet Pluribus Packet Core */
#define IPPROTO_SAT_MON		69		/* SATNET Monitoring */
#define IPPROTO_VISA		70		/* VISA Protocol */
#define IPPROTO_IPCV		71		/* Internet Packet Core Utility */
#define IPPROTO_CPNX		72		/* Computer Protocol Network Executive */
#define IPPROTO_CPHB		73		/* Computer Protocol Heart Beat */
#define IPPROTO_WSN		74		/* Wang Span Network */
#define IPPROTO_PVP		75		/* Packet Video Protocol */
#define IPPROTO_BR_SAT_MON	76		/* Backroom SATNET Monitoring */
#define IPPROTO_ND		77	/* depricated net disk proto */
#define IPPROTO_WB_MON		78		/* WIDEBAND Monitoring */
#define IPPROTO_WB_EXPAK	79		/* WIDEBAND EXPAK */
#define	IPPROTO_EON		80		/* ISO clnp */
#define IPPROTO_VMTP		81		/* VMTP */
#define IPPROTO_SECURE_VMTP	82		/* SECURE-VMTP */
#define IPPROTO_VINES		83		/* VINES */
#define IPPROTO_TTP		84		/* TTP */
#define IPPROTO_NSFNET_IGP	85		/* NSFNET-IGP */
#define IPPROTO_DGP		86		/* Dissimilar Gateway Protocol */
#define IPPROTO_TCF		87		/* TCF */
#define IPPROTO_IGRP		88		/* IGRP */
#define IPPROTO_OSPF		89		/* Open SPF routing */
#define IPPROTO_SPRITE_RPC	90		/* Sprite RPC Protocol */
#define IPPROTO_LARP		91		/* Locus Address Resolution Protocol */
#define IPPROTO_MTP		92		/* Multicast Transport Protocol */
#define IPPROTO_AX25		93		/* AX.25 Frames */
#define IPPROTO_SWIPE		94		/* swIPe protocol */
#define IPPROTO_MICP		95		/* Mobile Internetworking Control Pro. */
#define IPPROTO_AES_SP3_D	96		/* AES Security Protocol 3-D */
#define IPPROTO_ETHERIP		97		/* Ethernet-within-IP Encapsulation */
#define IPPROTO_ENCAPHDR	98		/* Encapsulation Header (officially ENCAP) */
#define	IPPROTO_STP		118		/* Scheduled Transfer Protocol */

#define	IPPROTO_RAW		255		/* raw IP packet */
#define	IPPROTO_MAX		256

/*
 * Local port number conventions:
 * Ports < IPPORT_RESERVED are reserved for
 * privileged processes (e.g. root).
 * Ports > 5000 used to be reserved for non-root servers,
 * but this convention can no longer be used with large scale systems.
 * All well-known ports should be registered with the Internet Assigned
 * Numbers Authority.
 */
#define	IPPORT_RESERVED		1024
#define IPPORT_MAXPORT		65535

/*
 * Internet address (a structure for historical reasons)
 */
struct in_addr {
	in_addr_t	s_addr;
};

#ifdef INET6
/*
 * IPv6 address (a structure for 64 bit architectures)
 */
struct in6_addr {
	union {
		uint32_t u6_addr32[4];
#ifdef notyet
		__uint64_t u6_addr64[2];
#endif
		uint8_t u6_addr8[16];
	} u6_addr;
};
#define s6_addr64       u6_addr.u6_addr64
#define s6_addr32       u6_addr.u6_addr32
#define s6_addr8        u6_addr.u6_addr8
#define s6_addr         u6_addr.u6_addr8

#define INET_ADDRSTRLEN         16
#define INET6_ADDRSTRLEN        46
#endif /* INET6 */

/*
 * Definitions of bits in internet address integers.
 * On subnets, the decomposition of addresses to host and net parts
 * is done according to subnet mask, not the masks here.
 */
#define	IN_CLASSA(i)		(((__int32_t)(i) & 0x80000000) == 0)
#define	IN_CLASSA_NET		0xff000000
#define	IN_CLASSA_NSHIFT	24
#define	IN_CLASSA_HOST		0x00ffffff
#define	IN_CLASSA_MAX		128

#define	IN_CLASSB(i)		(((__int32_t)(i) & 0xc0000000) == 0x80000000)
#define	IN_CLASSB_NET		0xffff0000
#define	IN_CLASSB_NSHIFT	16
#define	IN_CLASSB_HOST		0x0000ffff
#define	IN_CLASSB_MAX		65536

#define	IN_CLASSC(i)		(((__int32_t)(i) & 0xe0000000) == 0xc0000000)
#define	IN_CLASSC_NET		0xffffff00
#define	IN_CLASSC_NSHIFT	8
#define	IN_CLASSC_HOST		0x000000ff

#define	IN_CLASSD(i)		(((__int32_t)(i) & 0xf0000000) == 0xe0000000)
#define	IN_CLASSD_NET		0xf0000000	/* These ones aren't really */
#define	IN_CLASSD_NSHIFT	28		/* net and host fields, but */
#define	IN_CLASSD_HOST		0x0fffffff	/* routing needn't know.    */
#define	IN_MULTICAST(i)		IN_CLASSD(i)

#define	IN_EXPERIMENTAL(i)	(((__int32_t)(i) & 0xf0000000) == 0xf0000000)
#define	IN_BADCLASS(i)		(((__int32_t)(i) & 0xf0000000) == 0xf0000000)

#define	INADDR_ANY		(__uint32_t)0x00000000
#define	INADDR_BROADCAST	(__uint32_t)0xffffffff	/* must be masked */
#define	INADDR_LOOPBACK		(__uint32_t)0x7F000001

/* Multicast */
#define	INADDR_UNSPEC_GROUP	(__uint32_t)0xe0000000	/* 224.0.0.0   */
#define	INADDR_ALLHOSTS_GROUP	(__uint32_t)0xe0000001	/* 224.0.0.1   */
#define INADDR_ALLROUTERS_GROUP (__uint32_t)0xe0000002  /* 224.0.0.2   */
#define	INADDR_MAX_LOCAL_GROUP 	(__uint32_t)0xe00000ff	/* 224.0.0.255 */

#ifndef _KERNEL
#define	INADDR_NONE		0xffffffff		/* -1 return */
#endif

#define	IN_LOOPBACKNET		127			/* official! */

#ifdef INET6
#define IN6ADDR_ANY_INIT        {0, 0, 0, 0}
#if BYTE_ORDER == BIG_ENDIAN
#define IN6ADDR_LOOPBACK_INIT   {0, 0, 0, 1}
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
#define IN6ADDR_LOOPBACK_INIT   {0, 0, 0, 0x01000000}
#endif

#ifndef _KERNEL
extern const struct in6_addr in6addr_any, in6addr_loopback;
#endif
#endif /* INET6 */

/*
 * Socket address, internet style.
 */
struct sockaddr_in {
#undef _HAVE_SIN_LEN			/* marker for some places to change */
#ifdef _HAVE_SIN_LEN			/* when we have sin_len */
	uchar_t	sin_len;
	uchar_t  sin_family;
#else
	sa_family_t	sin_family;
#endif
	in_port_t	sin_port;
	struct	in_addr sin_addr;
	unsigned char	sin_zero[8];
};
#if _SGIAPI
#ifndef _HAVE_SIN_LEN
struct sockaddr_in_new {
	uchar_t		sin_len;
	uchar_t		sin_family;
	ushort_t	sin_port;
	struct in_addr  sin_addr;
	char		sin_zero[8];
};
#endif

#ifdef INET6
/*
 * IPv6 socket address
 */
#define SIN6_LEN

struct sockaddr_in6 {
	uint8_t  sin6_len;
	u_int8_t  sin6_family;/* XXX6 should be sa_family_t but it is a short */
	in_port_t sin6_port;
	uint32_t sin6_flowinfo;
	struct    in6_addr sin6_addr;
};

#define IPNGVERSION     6

/*
 * Definitions for flow labels.
 */

#if BYTE_ORDER == BIG_ENDIAN
#define IPV6_FLOWINFO_FLOWLABEL         0x00ffffff
#define IPV6_FLOWINFO_PRIORITY          0x0f000000
#define IPV6_FLOWINFO_PRIFLOW           0x0fffffff
#define IPV6_FLOWINFO_SRFLAG            0x10000000
#define IPV6_FLOWINFO_VERSION           0xf0000000

#define IPV6_PRIORITY_UNCHARACTERIZED   0x00000000
#define IPV6_PRIORITY_FILLER            0x01000000
#define IPV6_PRIORITY_UNATTENDED        0x02000000
#define IPV6_PRIORITY_RESERVED1         0x03000000
#define IPV6_PRIORITY_BULK              0x04000000
#define IPV6_PRIORITY_RESERVED2         0x05000000
#define IPV6_PRIORITY_INTERACTIVE       0x06000000
#define IPV6_PRIORITY_CONTROL           0x07000000
#define IPV6_PRIORITY_8                 0x08000000
#define IPV6_PRIORITY_9                 0x09000000
#define IPV6_PRIORITY_10                0x0a000000
#define IPV6_PRIORITY_11                0x0b000000
#define IPV6_PRIORITY_12                0x0c000000
#define IPV6_PRIORITY_13                0x0d000000
#define IPV6_PRIORITY_14                0x0e000000
#define IPV6_PRIORITY_15                0x0f000000

#define IPV6_SRFLAG_STRICT              0x10000000
#define IPV6_SRFLAG_LOOSE               0x00000000

#define IPV6_VERSION                    0x60000000

#endif
#if BYTE_ORDER == LITTLE_ENDIAN
#define IPV6_FLOWINFO_FLOWLABEL         0xffffff00
#define IPV6_FLOWINFO_PRIORITY          0x0000000f
#define IPV6_FLOWINFO_PRIFLOW           0xffffff0f
#define IPV6_FLOWINFO_SRFLAG            0x00000010
#define IPV6_FLOWINFO_VERSION           0x000000f0

#define IPV6_PRIORITY_UNCHARACTERIZED   0x00000000
#define IPV6_PRIORITY_FILLER            0x00000001
#define IPV6_PRIORITY_UNATTENDED        0x00000002
#define IPV6_PRIORITY_RESERVED1         0x00000003
#define IPV6_PRIORITY_BULK              0x00000004
#define IPV6_PRIORITY_RESERVED2         0x00000005
#define IPV6_PRIORITY_INTERACTIVE       0x00000006
#define IPV6_PRIORITY_CONTROL           0x00000007
#define IPV6_PRIORITY_8                 0x00000008
#define IPV6_PRIORITY_9                 0x00000009
#define IPV6_PRIORITY_10                0x0000000a
#define IPV6_PRIORITY_11                0x0000000b
#define IPV6_PRIORITY_12                0x0000000c
#define IPV6_PRIORITY_13                0x0000000d
#define IPV6_PRIORITY_14                0x0000000e
#define IPV6_PRIORITY_15                0x0000000f

#define IPV6_SRFLAG_STRICT              0x00000010
#define IPV6_SRFLAG_LOOSE               0x00000000

#define IPV6_VERSION                    0x00000060

#endif

#define IPV6_GET_FLOWLABEL(x)		(ntohl(x) & 0x00ffffff)
#define IPV6_GET_PRIORITY(x)		((ntohl(x) >> 24) & 0xf)
#define IPV6_GET_VERSION(x)		((ntohl(x) >> 28) & 0xf)
#define IPV6_SET_FLOWLABEL(x)		(htonl(x) & IPV6_FLOWINFO_FLOWLABEL)
#define IPV6_SET_PRIORITY(x)		(htonl((x & 0xf) << 24))

/*
 * Definitions for IPv6 addresses.
 */

#define CLR_ADDR6(a) \
	{ \
		(a).s6_addr32[0] = 0; \
		(a).s6_addr32[1] = 0; \
		(a).s6_addr32[2] = 0; \
		(a).s6_addr32[3] = 0; \
	}

#define COPY_ADDR6(from, to) \
	{ \
		(to).s6_addr32[0] = (from).s6_addr32[0]; \
		(to).s6_addr32[1] = (from).s6_addr32[1]; \
		(to).s6_addr32[2] = (from).s6_addr32[2]; \
		(to).s6_addr32[3] = (from).s6_addr32[3]; \
	}

#define SAME_SOCKADDR(a, b) \
	(((a)->sin6_addr.s6_addr32[0] == (b)->sin6_addr.s6_addr32[0]) && \
	 ((a)->sin6_addr.s6_addr32[1] == (b)->sin6_addr.s6_addr32[1]) && \
	 ((a)->sin6_addr.s6_addr32[2] == (b)->sin6_addr.s6_addr32[2]) && \
	 ((a)->sin6_addr.s6_addr32[3] == (b)->sin6_addr.s6_addr32[3]))

#define SAME_ADDR6(a, b) \
	(((a).s6_addr32[0] == (b).s6_addr32[0]) && \
	 ((a).s6_addr32[1] == (b).s6_addr32[1]) && \
	 ((a).s6_addr32[2] == (b).s6_addr32[2]) && \
	 ((a).s6_addr32[3] == (b).s6_addr32[3]))

#define IS_ANYSOCKADDR(a) \
	(((a)->sin6_addr.s6_addr32[0] == 0) && \
	 ((a)->sin6_addr.s6_addr32[1] == 0) && \
	 ((a)->sin6_addr.s6_addr32[2] == 0) && \
	 ((a)->sin6_addr.s6_addr32[3] == 0))

#define IS_ANYADDR6(a) \
	(((a).s6_addr32[0] == 0) && \
	 ((a).s6_addr32[1] == 0) && \
	 ((a).s6_addr32[2] == 0) && \
	 ((a).s6_addr32[3] == 0))

#define IS_COMPATSOCKADDR(a) \
	(((a)->sin6_addr.s6_addr32[0] == 0) && \
	((a)->sin6_addr.s6_addr32[1] == 0) && \
	((a)->sin6_addr.s6_addr32[2] == 0) && \
	((a)->sin6_addr.s6_addr8[12] != 0))

#define IS_COMPATADDR6(a) \
	(((a).s6_addr32[0] == 0) && \
	((a).s6_addr32[1] == 0) && \
	((a).s6_addr32[2] == 0) && \
	((a).s6_addr8[12] != 0))

#if BYTE_ORDER == BIG_ENDIAN
#define IS_LOOPSOCKADDR(a) \
	(((a)->sin6_addr.s6_addr32[0] == 0) && \
	 ((a)->sin6_addr.s6_addr32[1] == 0) && \
	 ((a)->sin6_addr.s6_addr32[2] == 0) && \
	 ((a)->sin6_addr.s6_addr32[3] == 1))

#define IS_LOOPADDR6(a) \
	(((a).s6_addr32[0] == 0) && \
	 ((a).s6_addr32[1] == 0) && \
	 ((a).s6_addr32[2] == 0) && \
	 ((a).s6_addr32[3] == 1))

#define IS_IPV4SOCKADDR(a) \
	(((a)->sin6_addr.s6_addr32[0] == 0) && \
	 ((a)->sin6_addr.s6_addr32[1] == 0) && \
	 ((a)->sin6_addr.s6_addr32[2] == 0x0000ffff))

#define IS_IPV4ADDR6(a) \
	(((a).s6_addr32[0] == 0) && \
	 ((a).s6_addr32[1] == 0) && \
	 ((a).s6_addr32[2] == 0x0000ffff))
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
#define IS_LOOPSOCKADDR(a) \
	(((a)->sin6_addr.s6_addr32[0] == 0) && \
	 ((a)->sin6_addr.s6_addr32[1] == 0) && \
	 ((a)->sin6_addr.s6_addr32[2] == 0) && \
	 ((a)->sin6_addr.s6_addr32[3] == 0x1000000))

#define IS_LOOPADDR6(a) \
	(((a).s6_addr32[0] == 0) && \
	 ((a).s6_addr32[1] == 0) && \
	 ((a).s6_addr32[2] == 0) && \
	 ((a).s6_addr32[3] == 0x1000000))

#define IS_IPV4SOCKADDR(a) \
	(((a)->sin6_addr.s6_addr32[0] == 0) && \
	 ((a)->sin6_addr.s6_addr32[1] == 0) && \
	 ((a)->sin6_addr.s6_addr32[2] == 0xffff0000))

#define IS_IPV4ADDR6(a) \
	(((a).s6_addr32[0] == 0) && \
	 ((a).s6_addr32[1] == 0) && \
	 ((a).s6_addr32[2] == 0xffff0000))
#endif

#define IS_LOCALADDR6(a)	((a).s6_addr8[0] == 0xfe)

#define IS_LINKLADDR6(a) \
	(IS_LOCALADDR6(a) && ((a).s6_addr8[1] == 0x80))

#define IS_SITELADDR6(a) \
	(IS_LOCALADDR6(a) && ((a).s6_addr8[1] == 0xc0))

#define IS_MULTIADDR6(a)	((a).s6_addr8[0] == 0xff)

#define MADDR6_FLAGS(a)		((a).s6_addr8[1] >> 4)

#define MADDR6_FLG_WK		0	/* "well-known" multicast address */
#define MADDR6_FLG_TS		1	/* "transient" multicast address */

#define MADDR6_SCOPE(a)		((a).s6_addr8[1] & 0x0f)

#define MADDR6_SCP_NODE		0x1	/* node-local scope */
#define MADDR6_SCP_LINK		0x2	/* link-local scope */
#define MADDR6_SCP_SITE		0x5	/* site-local scope */
#define MADDR6_SCP_ORG		0x8	/* organization-local scope */
#define MADDR6_SCP_GLO		0xe	/* global scope */

#define MADDR6_ALLNODES		1	/* all-nodes groups */
#define MADDR6_ALLROUTERS	2	/* all-routers groups */
#define MADDR6_ALLHOSTS		3	/* all-hosts groups */

/*
 * New IPv6 address macros of BSD API
 */

#define IN6_IS_ADDR_UNSPECIFIED(p)	IS_ANYADDR6(*p)
#define IN6_IS_ADDR_LOOPBACK(p)		IS_LOOPADDR6(*p)
#define IN6_IS_ADDR_MULTICAST(p)	IS_MULTIADDR6(*p)
#define IN6_IS_ADDR_LINKLOCAL(p)	IS_LINKLADDR6(*p)
#define IN6_IS_ADDR_SITELOCAL(p)	IS_SITELADDR6(*p)
#define IN6_IS_ADDR_V4MAPPED(p)		IS_IPV4ADDR6(*p)
#define IN6_IS_ADDR_V4COMPAT(p)		IS_COMPATADDR6(*p)

#define IN6_IS_ADDR_MC_NODELOCAL(p) \
	(IS_MULTIADDR6(*p) && (MADDR6_SCOPE(*p) == MADDR6_SCP_NODE))
#define IN6_IS_ADDR_MC_LINKLOCAL(p) \
	(IS_MULTIADDR6(*p) && (MADDR6_SCOPE(*p) == MADDR6_SCP_LINK))
#define IN6_IS_ADDR_MC_SITELOCAL(p) \
	(IS_MULTIADDR6(*p) && (MADDR6_SCOPE(*p) == MADDR6_SCP_SITE))
#define IN6_IS_ADDR_MC_ORGLOCAL(p) \
	(IS_MULTIADDR6(*p) && (MADDR6_SCOPE(*p) == MADDR6_SCP_ORG))
#define IN6_IS_ADDR_MC_GLOBAL(p) \
	(IS_MULTIADDR6(*p) && (MADDR6_SCOPE(*p) == MADDR6_SCP_GLO))

#define IN6_ARE_ADDR_EQUAL(pa,pb)	SAME_ADDR6(*pa,*pb)
#endif /* INET6 */

/*
 * Structure used to describe IP options.
 * Used to store options internally, to pass them to a process,
 * or to restore options retrieved earlier.
 * The ip_dst is used for the first-hop gateway when using a source route
 * (this gets put into the header proper).
 */
struct ip_opts {
	struct	in_addr ip_dst;		/* first hop, 0 w/o src rt */
	char	ip_opts[40];		/* actually variable in size */
};
#endif

/*
 * Options for use with [gs]etsockopt at the IP level.
 * First word of comment is data type; bool is stored in int.
 */

#define	IP_OPTIONS	    1	/* buf/ip_opts; set/get IP per-packet options */
#define	IP_HDRINCL	    2	/* int; header is included with data (raw) */
#define	IP_TOS		    3	/* int; IP type of service and precedence */
#define	IP_TTL		    4	/* int; IP time to live */
#define	IP_RECVOPTS	    5 	/* bool; receive all IP options w/datagram */
#define	IP_RECVRETOPTS	    6 	/* bool; receive IP options for response */
#define	IP_RECVDSTADDR	    7 	/* bool; receive IP dst addr w/datagram */
#define	IP_RETOPTS	    8 	/* ip_opts; set/get IP per-packet options */
#define	IP_MULTICAST_IF	   20	/* in_addr; set/get IP multicast interface */
#define	IP_MULTICAST_TTL   21	/* u_char;  set/get IP multicast timetolive */
#define	IP_MULTICAST_LOOP  22	/* u_char;  set/get IP multicast loopback */
#define	IP_ADD_MEMBERSHIP  23	/* ip_mreq; add  an IP group membership */
#define	IP_DROP_MEMBERSHIP 24	/* ip_mreq; drop an IP group membership */
#define IP_MULTICAST_VIF   25	/* int; set/get IP mcast vir. interface */
#define IP_RSVP_VIF_ON	   26	/* int: set rsvp per-vif socket */
#define IP_RSVP_VIF_OFF	   27	/* int: unset rsvp per-vif socket */
#define IP_RSVP_ON         28	/* void; enable rsvp in kernel */
#define	IP_SENDSRCADDR     36 	/* in_addr; send from specified address */
#ifdef INET6
#define IPV6_UNICAST_HOPS	IP_TTL
#define IPV6_MULTICAST_IF	IP_MULTICAST_IF		/*u_int */
#define IPV6_MULTICAST_HOPS     IP_MULTICAST_TTL        /* int */
#define IPV6_MULTICAST_LOOP     IP_MULTICAST_LOOP       /* u_int */
#define IPV6_ADD_MEMBERSHIP     IP_ADD_MEMBERSHIP
#define IPV6_DROP_MEMBERSHIP    IP_DROP_MEMBERSHIP
#define IPV6_SENDIF        40   /* sockaddr; set outgoing iface */
#define IPV6_NOPROBE       42	/* int; don't trigger NUD probes */
#define IPV6_RECVPKTINFO   43   /* int; receive packet info */
#define IPV6_PKTINFO	   44   /* int; set packet info */
#define IP_RECVTTL	   45   /* int; receive TTL/hlim */
#define IPV6_RECVHOPS	   IP_RECVTTL
#define IPV6_CHECKSUM	   46	/* int; calc and insert chksum at offset */
#define ICMP6_FILTER	   47   /* struct icmp6_filter; icmp6 pkt filtering */
#define IPV6_HOPLIMIT	   48	/* int; set/get v6 hop limit */
#define IPV6_HOPOPTS	   49	/* int; when set use hop by hop options */
#define IPV6_DSTOPTS	   50	/* int; when set use destination options */
#define IPV6_RTHDR	   51	/* int; when set use routing header */
#define IPV6_PKTOPTIONS	   52	/* cmsghdr; "sticky" options */
#define IPV6_NEXTHOP	   53	/* only used as ancillary data not in
				   [gs]etsockopt */
#endif

/* End of [gs]etsockopt options */

#define	IP_DEFAULT_MULTICAST_TTL   1	/* normally limit m'casts to 1 hop  */
#define	IP_DEFAULT_MULTICAST_LOOP  1	/* normally hear sends if a member  */

#if _SGIAPI
/*
 * Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP.
 */
struct ip_mreq {
	struct in_addr	imr_multiaddr;	/* IP multicast address of group */
	struct in_addr	imr_interface;	/* local IP address of interface */
};
#endif

#ifdef INET6
struct ipv6_mreq {
	struct  in6_addr ipv6mr_multiaddr;/* IPv6 multicast address of group */
	unsigned int ipv6mr_interface;  /* local IPv6 address of interface */
};

/*
 * packet information
 */
struct in6_pktinfo {
	struct in6_addr ipi6_addr;	/* src/dst IPv6 address */
	int		ipi6_ifindex;	/* send/recv interface index */
};
#endif

#ifdef _KERNEL
extern	struct domain inetdomain;
extern	struct protosw inetsw[];
struct ifnet;				/* forward reference */

extern	int		in_broadcast(struct in_addr, struct ifnet *);
extern	int		in_canforward(struct in_addr);
extern	int		in_localaddr(struct in_addr);
extern	__uint32_t	in_netof(struct in_addr);
extern	__uint32_t	inet_addr(char *);

#ifdef INET6
extern int      in6_isanycast(struct in6_addr *);
extern char    *ip6_sprintf(struct in6_addr *);

#define satosin6(sa)    ((struct sockaddr_in6 *)(sa))
#define sin6tosa(sin6)  ((struct sockaddr *)(sin6))
#define ifatoia6(ifa)   ((struct in6_ifaddr *)(ifa))
#endif
#endif /* _KERNEL */

#ifdef INET6
#define IPV6_RTHDR_LOOSE	0
#define IPV6_RTHDR_STRICT	1

#define IPV6_RTHDR_TYPE_0	0
#endif

#ifdef INET6
#ifndef _KERNEL
/* hop by hop and destination options processing functions. */
struct cmsghdr;  /* defined in socket.h */
extern uint8_t *inet6_option_alloc(struct cmsghdr *, int, int, int);
extern int inet6_option_append(struct cmsghdr *, const uint8_t *, int, int);
extern int inet6_option_find(const struct cmsghdr *, uint8_t **, int);
extern int inet6_option_init(void *, struct cmsghdr **, int);
extern int inet6_option_next(const struct cmsghdr *, uint8_t **);
extern int inet6_option_space(int);

/* router header option */
extern int inet6_rthdr_add(struct cmsghdr *, const struct in6_addr *,
  unsigned int);
extern struct in6_addr *inet6_rthdr_getaddr(struct cmsghdr *, int);
extern int inet6_rthdr_getflags(const struct cmsghdr *, int);
extern struct cmsghdr *inet6_rthdr_init(void *, int);
extern int inet6_rthdr_lasthop(struct cmsghdr *, unsigned int);
extern int inet6_rthdr_reverse(const struct cmsghdr *, struct cmsghdr *);
extern int inet6_rthdr_segments(const struct cmsghdr *);
extern size_t inet6_rthdr_space(int, int);
#endif /* !_KERNEL */
#endif /* INET6 */

#ifdef __cplusplus
}
#endif
#endif /* __NETINET_IN_H__ */
