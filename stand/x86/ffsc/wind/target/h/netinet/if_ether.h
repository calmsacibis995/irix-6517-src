/* if_ether.h - network interface Ethernet header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
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
 *      @(#)if_ether.h  7.3 (Berkeley) 6/29/88
 */

/*
modification history
--------------------
03l,22sep92,rrr  added support for c++
03k,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
03j,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
03i,22jul91,rfs  typedef'd ethernet header
03h,19jul91,hdn  moved SIZEOF_ETHERHEADER from if_subr.h.
03g,10jun91.del  added pragma for gnu960 alignment.
03f,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
03e,19apr90,hjb  added ETHERSMALL
02d,16apr89,gae  updated to new 4.3BSD.
02c,220ct87,ecs  changed declaration of etherbroadcastaddr to be extern.
02b,28aug87,dnw  added include of in.h.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCif_etherh
#define __INCif_etherh

#ifdef __cplusplus
extern "C" {
#endif

#include "netinet/in.h"

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */


/*
 * Structure of a 10Mb/s Ethernet header.
 */
typedef struct ether_header
    {
    u_char	ether_dhost[6];
    u_char	ether_shost[6];
    u_short	ether_type;
    } ETH_HDR;

#define	ETHERTYPE_PUP	0x0200		/* PUP protocol */
#define	ETHERTYPE_IP	0x0800		/* IP protocol */
#define ETHERTYPE_ARP	0x0806		/* Addr. resolution protocol */
/*
 * for compilers that have 4-bytes structure alignment rule,
 * it is understood that sizeof (ether_header) is 0x10
 */
#define SIZEOF_ETHERHEADER      0xe

/*
 * The ETHERTYPE_NTRAILER packet types starting at ETHERTYPE_TRAIL have
 * (type-ETHERTYPE_TRAIL)*512 bytes of data followed
 * by an ETHER type (as given above) and then the (variable-length) header.
 */
#define	ETHERTYPE_TRAIL		0x1000		/* Trailer packet */
#define	ETHERTYPE_NTRAILER	16

#define	ETHERMTU	1500
#define	ETHERMIN	(60-14)

/* minimal size of an ethernet frame including the headers supported
 * by most ethernet chips.
 */

#define ETHERSMALL	60

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
	struct 	ifnet ac_if;		/* network-visible interface */
	u_char	ac_enaddr[6];		/* ethernet hardware address */
	struct in_addr ac_ipaddr;	/* copy of ip address- XXX */
};

/*
 * Internet to ethernet address resolution table.
 */
struct	arptab {
	struct	in_addr at_iaddr;	/* internet address */
	u_char	at_enaddr[6];		/* ethernet address */
	u_char	at_timer;		/* minutes since last reference */
	u_char	at_flags;		/* flags */
	struct	mbuf *at_hold;		/* last packet until resolved/timeout */
};

extern u_char etherbroadcastaddr[6];

struct	arptab *arptnew();
char *ether_sprintf();

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_etherh */
