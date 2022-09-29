/* route.h - routing table header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
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
 *      @(#)route.h     7.4 (Berkeley) 6/27/88
 */

/*
modification history
--------------------
02i,14feb94,elh  added in mib2 support.
02h,22sep92,rrr  added support for c++
02g,26may92,rrr  the tree shuffle
02f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
02e,10jun91.del  added pragma for gnu960 alignment.
02d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02c,16apr89,gae  updated to new 4.3BSD.
02b,04nov87,dnw  moved definitions of rthost, rtnet, and rtstat to route.c.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCrouteh
#define __INCrouteh

#ifdef __cplusplus
extern "C" {
#endif

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */
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
struct route
    {
    struct	rtentry *ro_rt;
    struct	sockaddr ro_dst;
    };

/*
 * We distinguish between routes to hosts and routes to networks,
 * preferring the former if available.  For each route we infer
 * the interface to use from the gateway address supplied when
 * the route was entered.  Routes that forward packets through
 * gateways are marked so that the output routines know to address the
 * gateway rather than the ultimate destination.
 */
struct rtentry
    {
    u_long	rt_hash;		/* to speed lookups */
    struct	sockaddr rt_dst;	/* key */
    struct	sockaddr rt_gateway;	/* value */
    short	rt_flags;		/* up/down?, host/net */
    short	rt_refcnt;		/* # held references */
    u_long	rt_use;			/* raw # packets forwarded */
    struct	ifnet *rt_ifp;		/* the answer: interface to use */
    int		rt_mod;			/* last modified */
    };

#define	RTF_UP		0x1		/* route useable */
#define	RTF_GATEWAY	0x2		/* destination is a gateway */
#define	RTF_HOST	0x4		/* host entry (net otherwise) */
#define	RTF_DYNAMIC	0x10		/* created dynamically (by redirect) */
#define RTF_MODIFIED    0x20            /* modified dynamically (by redirect) */
#define RTF_MGMT	0x100		/* modfied by managment proto */

/* Routing statistics. */

struct	rtstat
    {
    short	rts_badredirect;	/* bogus redirect calls */
    short	rts_dynamic;		/* routes created by redirects */
    short	rts_newgateway;		/* routes modified by redirects */
    short	rts_unreach;		/* lookups which failed */
    short	rts_wildcard;		/* lookups satisfied by a wildcard */
    };

#define	RTFREE(rt) \
	if ((rt)->rt_refcnt == 1) \
		rtfree(rt); \
	else \
		(rt)->rt_refcnt--;

#ifdef	GATEWAY
#define	RTHASHSIZ	64
#else
#define	RTHASHSIZ	8
#endif

#if	(RTHASHSIZ & (RTHASHSIZ - 1)) == 0
#define RTHASHMOD(h)	((h) & (RTHASHSIZ - 1))
#else
#define RTHASHMOD(h)	((h) % RTHASHSIZ)
#endif

extern struct mbuf *rthost[RTHASHSIZ];
extern struct mbuf *rtnet[RTHASHSIZ];
extern struct rtstat rtstat;
extern int rtmodified;

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCrouteh */
