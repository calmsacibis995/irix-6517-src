/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/rt_table.h,v 1.4 1995/10/11 14:11:56 vjs Exp $
 *
 */

/*
 * rt_table.h
 *
 * Routing table data and parameter definitions.
 *
 * Modified from Routing Table Management Daemon, routed/table.h
 *
 * Structure "rt_entry" stores information about one particular route.
 * routes to networks and hosts are used, much like the kernel does.
 *
 * The net routes are organized as a hashed array of doubly linked
 * lists. The hash array is named nethash[].
 *
 * Routing table structure differs a bit from kernel tables.
 *
 * Note: the union below must agree in the first 4 members
 * so the ioctl's will work.
 */

#define	WINDOW_INTERVAL		6	/* in minutes */
#define HELLO_INTERVAL		15	/* HELLO rate coming in, in secs */
#define HWINSIZE		(WINDOW_INTERVAL * (60 / HELLO_INTERVAL))

struct hello_win {
	int h_win[HWINSIZE];
	int h_index;
	int h_min;
	int h_min_ttl;
};

struct rthash {
	struct	rt_entry *rt_forw;
	struct	rt_entry *rt_back;
};

struct rt_entry {
	struct	rt_entry *rt_forw;
	struct	rt_entry *rt_back;
	union {
#ifndef sgi
		struct	rtentry rtu_rt;
#endif
		struct {
			u_long	rtu_hash;
			struct	sockaddr rtu_dst;
			struct	sockaddr rtu_router;
			short	rtu_flags;
#ifdef  vax11c
                        short   rtu_dummy1;     /* Padding */
#endif  vax11c
			int	rtu_state;
			int	rtu_timer;
			int	rtu_metric;
			struct	interface *rtu_ifp;
			int	rtu_proto;
			struct	hello_win rtu_h_window;
			struct  restrictlist *rtu_announce;
			struct  restrictlist *rtu_noannounce;
			struct  restrictlist *rtu_listen;
			struct  restrictlist *rtu_srclisten;
                        int     rtu_fromproto;
			u_short	rtu_as;
			u_int	rtu_metric_exterior;
		} rtu_entry;
	} rt_rtu;
};

#ifndef sgi
#define	rt_rt		  rt_rtu.rtu_rt			  /* pass to ioctl */
#endif
#define	rt_hash		  rt_rtu.rtu_entry.rtu_hash	  /* for net or host */
#define	rt_dst		  rt_rtu.rtu_entry.rtu_dst	  /* match value */
#define	rt_router	  rt_rtu.rtu_entry.rtu_router	  /* who to forward to*/
#define	rt_flags	  rt_rtu.rtu_entry.rtu_flags	  /* kernel flags */
#define	rt_timer	  rt_rtu.rtu_entry.rtu_timer	  /* for invalidation */
#define	rt_state	  rt_rtu.rtu_entry.rtu_state	  /* see below */
#define	rt_metric	  rt_rtu.rtu_entry.rtu_metric	  /* cost of route */
#define	rt_ifp		  rt_rtu.rtu_entry.rtu_ifp	  /* interface to take*/
#define	rt_proto	  rt_rtu.rtu_entry.rtu_proto	  /* proto of route */
#define	rt_hwindow	  rt_rtu.rtu_entry.rtu_h_window	  /* rt histry window */
#define	rt_announcelist	  rt_rtu.rtu_entry.rtu_announce	  /* rt announce list */
#define	rt_noannouncelist rt_rtu.rtu_entry.rtu_noannounce /* rt announce list */
#define	rt_listenlist	  rt_rtu.rtu_entry.rtu_listen	  /* rt. forbid list */
#define	rt_srclisten	  rt_rtu.rtu_entry.rtu_srclisten  /* src forbid list */
#define	rt_fromproto	  rt_rtu.rtu_entry.rtu_fromproto  /* protocols supported
                                     			   * by gw to route
							   */
#define rt_as		  rt_rtu.rtu_entry.rtu_as	  /* autonomous system */
#define	rt_metric_exterior	  rt_rtu.rtu_entry.rtu_metric_exterior	/* Metric provided by exterior protocol */
#define	ROUTEHASHSIZ	64
#define	ROUTEHASHMASK	(ROUTEHASHSIZ - 1)

/*
 * "State" of routing table entry.
 */
#define	RTS_CHANGED	0x1 		/* route has been altered recently */
#define	RTS_POINTOPOINT	IFF_POINTOPOINT	/* route is point-to-point */
#define RTS_STATIC	0x20		/* route entered on startup */
#define RTS_NOTINSTALL  0x100		/* don't install this route in kernel */
#define RTS_NOTADVISENR 0x200 		/* This route not to be advised */
#define RTS_SUBNET	IFF_SUBNET	/* is this a subnet route? */
#define	RTS_PASSIVE	IFF_PASSIVE	/* don't time out route */
#define	RTS_INTERFACE	IFF_INTERFACE   /* route is for network interface */
#define	RTS_REMOTE	IFF_REMOTE	/* route is for ``remote'' entity */
#define RTS_HOSTROUTE	0x10000		/* a host route */
#define RTS_INTERIOR    0x20000     	/* an interior route */
#define RTS_EXTERIOR    0x40000     	/* an exterior route */

#undef  HOPCNT_INFINITY			/* don't want routed.h one */
#define HOPCNT_INFINITY	255		/* unreachable net distance */
#ifdef	NSS
#define	DELAY_INFINITY	255		/* Maximum delay when part of NSS code */
#define	ETOTD_CONV	1		/* EGP to time delay conv. factor when part of NSS code */
#else	NSS
#define ETOTD_CONV	100		/* EGP to time delay conv. factor */
#endif	NSS
#define TDHOPCNT_INFINITY (HOPCNT_INFINITY * ETOTD_CONV)

#define INTERIOR	RTS_INTERIOR	/* interior routing table */
#define EXTERIOR	RTS_EXTERIOR	/* exterior routing table */
#define HOSTTABLE	RTS_HOSTROUTE	/* host routing table */
#define DEFAULTNET	0x00		/* net # for default route */
#define RT_MINAGE	240		/* minimum time in seconds before
					   route is deleted when not updated*/
#define RT_NPOLLAGE	3		/* minimum number of poll intervals
					   before a route is deleted when
					   not updated */
#define RT_TIMERRATE	60		/* minimum time in seconds between
					route age increments. Actually use
					multiple of EGP hello interval as this
					is timer interrupt period */
#define HOLD_DOWN	60		/* Holddown of a route in seconds */
#define FINAL_DELETE	EXPIRE_TIME+HOLD_DOWN	/* Final expire time in seconds of a route */
#define INSTALLED	1		/* status of default route */
#define NOTINSTALLED	2

#define RTPROTO_EGP		0x01	/*  route was received via EGP */
#ifdef	NSS
#define RTPROTO_IGP		0x02	/*  route was received via IGP */
#else	NSS
#define RTPROTO_RIP		0x02	/*  route was received via RIP */
#endif	NSS
#define RTPROTO_HELLO		0x04	/*  route was received via HELLO */
#define RTPROTO_KERNEL		0x08	/*  route was received via KERNEL */
#define RTPROTO_REDIRECT	0x10	/*  route was received via REDIRECT */
#define RTPROTO_DIRECT		0x20	/*  route is directly connected */
#define RTPROTO_DEFAULT		0x40	/*  route is GATEWAY default */

#define KERNEL_INTR	0		/* delete route kernel and interior */
#define KERNEL_ONLY	1		/* delete route from kernel only */

#ifndef	NSS
#define RIP_TO_HELLO	1		/* converting from RIP metric to ms */
#define HELLO_TO_RIP	2		/* converting from HELLO ms to metric */
#define	EGP_TO_HELLO	3		/* converting from EGP metric to ms */
#define HELLO_TO_EGP	4		/* converting from HELLO ms to EGP metric */

#define HOP_TO_DELAY {			\
          0,		/* 0 */ 	\
          100,		/* 1 */ 	\
          148,		/* 2 */ 	\
          219,		/* 3 */ 	\
          325,		/* 4 */ 	\
          481,		/* 5 */ 	\
          713,		/* 6 */ 	\
          1057,		/* 7 */ 	\
          1567,		/* 8 */ 	\
          2322,		/* 9 */		\
          3440,		/* 10 */	\
          5097,		/* 11 */	\
          7552,		/* 12 */	\
          11190,	/* 13 */	\
          16579,	/* 14 */	\
          24564,	/* 15 */	\
          30000 }	/* 16 */
#else	NSS
#define	mapmetric(type, metric)	metric
#endif	NSS
