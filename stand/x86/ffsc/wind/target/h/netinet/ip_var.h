/* ip_var.h - interface protocol header file */

/* Copyright 1984-1993 Wind River Systems, Inc. */
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
 *      @(#)ip_var.h    7.5 (Berkeley) 6/29/88
 */

/*
modification history
--------------------
02k,13jan94,jag  added variables ipTimeToLive, ipInUnknownProtos, ipOutDatagrams
		 ipOutNoRoutes, ipReasmReqds, ipReasmOKs, ipFragFails, 
		 ipFragCreates, ipFragOKs for MIB-II support. Added extern
		 reference to ipforward and ipstat.
02j,03mar93,ccc  added underscore to _BYTE_ORDER, _LITTLE_ENDIAN and
		 _BIG_ENDIAN.  SPR 2356.
02i,22sep92,rrr  added support for c++
02h,26may92,rrr  the tree shuffle
02g,30oct91,rrr  removed bitfields
02f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
02e,10jun91.del  added pragma for gnu960 alignment.
02d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02c,16apr89,gae  updated to new 4.3BSD.
02b,04nov87,dnw  moved ipstat, ipq, ip_id structures to ip_input.c.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCip_varh
#define __INCip_varh

#ifdef __cplusplus
extern "C" {
#endif

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/* Overlay for ip header used by other protocols (tcp, udp). */
struct ipovly
    {
    caddr_t	ih_next;		/* for protocol sequence q's */
    caddr_t	ih_prev;
    u_char	ih_x1;			/* (unused) */
    u_char	ih_pr;			/* protocol */
    short	ih_len;			/* protocol length */
    struct	in_addr ih_src;		/* source internet address */
    struct	in_addr ih_dst;		/* destination internet address */
    };

/*
 * Ip reassembly queue structure.  Each fragment
 * being reassembled is attached to one of these structures.
 * They are timed out after ipq_ttl drops to 0, and may also
 * be reclaimed if memory becomes tight.
 */
struct ipq
    {
    struct	ipq *next;		/* to other reass headers */
    struct	ipq *prev;
    u_char	ipq_ttl;		/* time for reass q to live */
    u_char	ipq_p;			/* protocol of this fragment */
    u_short	ipq_id;			/* sequence id for reassembly */
    struct	ipasfrag *ipq_next;	/* to ip headers of fragments */
    struct	ipasfrag *ipq_prev;
    struct	in_addr ipq_src,ipq_dst;
    };

/*
 * Ip header, when holding a fragment.
 *
 * Note: ipf_next must be at same offset as ipq_next above
 */
struct	ipasfrag
    {
#ifdef USE_BITFIELDS
#undef USE_BITFIELDS
#endif /* USE_BITFIELDS */
#ifdef USE_BITFIELDS
#if _BYTE_ORDER == _LITTLE_ENDIAN
    u_char	ip_hl:4,
		ip_v:4;
#endif
#if _BYTE_ORDER == _BIG_ENDIAN
    u_char	ip_v:4,
		ip_hl:4;
#endif
#else
    u_char	ip_v_hl;		/* version (4msb) hdr length (4lsb) */
#endif
    u_char	ipf_mff;		/* copied from (ip_off&IP_MF) */
    short	ip_len;
    u_short	ip_id;
    short	ip_off;
    u_char	ip_ttl;
    u_char	ip_p;
    u_short	ip_sum;
    struct	ipasfrag *ipf_next;	/* next fragment */
    struct	ipasfrag *ipf_prev;	/* previous fragment */
    };

/*
 * Structure stored in mbuf in inpcb.ip_options
 * and passed to ip_output when ip options are in use.
 * The actual length of the options (including ipopt_dst)
 * is in m_len.
 */
#define MAX_IPOPTLEN	40

struct ipoption
    {
    struct	in_addr ipopt_dst;		/* 1st-hop dst if src routed */
    char	ipopt_list[MAX_IPOPTLEN];	/* options proper */
    };

struct	ipstat
    {
    long	ips_total;		/* total packets received */
    long	ips_badsum;		/* checksum bad */
    long	ips_tooshort;		/* packet too short */
    long	ips_toosmall;		/* not enough data */
    long	ips_badhlen;		/* ip header length < data size */
    long	ips_badlen;		/* ip length < ip header length */
    long	ips_fragments;		/* fragments received */
    long	ips_fragdropped;	/* frags dropped (dups, out of space) */
    long	ips_fragtimeout;	/* fragments timed out */
    long	ips_forward;		/* packets forwarded */
    long	ips_cantforward;	/* packets rcvd for unreachable dest */
    long	ips_redirectsent;	/* packets forwarded on same net */
    };

/* flags passed to ip_output as last parameter */
#define	IP_FORWARDING		0x1		/* most of ip header exists */
#define	IP_ROUTETOIF		SO_DONTROUTE	/* bypass routing tables */
#define	IP_ALLOWBROADCAST	SO_BROADCAST	/* can send broadcast packets */

extern u_short	ip_id;				/* ip packet ctr, for ids */

extern struct	mbuf *ip_srcroute();

extern int             ipforwarding;	/* Declared and used in ip_input.c */
extern struct ipstat	ipstat;			/* exported */

extern long ipTimeToLive;		/* MIB-II Read Write Variable */
extern long ipInUnknownProtos;		/* MIB-II Read Variable */
extern long ipInDelivers;		/* MIB-II Read Variable */
extern long ipOutDatagrams;		/* MIB-II Read Variable */
extern long ipOutNoRoutes;		/* MIB-II Read Variable */
extern long ipReasmReqds;		/* MIB-II Read Variable */
extern long ipReasmOKs;			/* MIB-II Read Variable */
extern long ipFragFails;		/* MIB-II Read Variable */
extern long ipFragCreates;		/* MIB-II Read Variable */
extern long ipOutDiscards;		/* MIB-II Read Variable */
extern long ipFragOKs;			/* MIB-II Read Variable */

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */


#ifdef __cplusplus
}
#endif

#endif /* __INCip_varh */
