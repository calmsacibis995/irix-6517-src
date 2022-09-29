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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/if.h,v 1.2 1990/01/09 15:35:38 jleong Exp $
 *
 */

/*
 * Interface data definitions.
 *
 * Modified from Routing Table Management Daemon, routed/interface.h
 *
 * Structure interface stores information about a directly attached interface,
 * such as name, internet address, and bound sockets. The interface structures
 * are in a singly linked list pointed to by external variable "ifnet".
 */

/*
 * we will keep a list of active routing gateways per interface.  THis is
 * not pretty, but we need this to properly handle split horizon in the
 * multi-routing protocol environment
 */

struct active_gw {
	int 	proto;			/* routing protocols supported */
	u_long	addr;			/* address of gateway */
	int	timer;			/* age of active gateway */
	struct active_gw *next;		/* ptr to next in list */
	struct active_gw *back;		/* ptr to what precedes me in list */
};

struct interface {
	struct	interface *int_next;
	struct	sockaddr int_addr;		/* address on this host */
	union {
		struct	sockaddr intu_broadaddr;
		struct	sockaddr intu_dstaddr;
	} int_intu;
#define	int_broadaddr	int_intu.intu_broadaddr	/* broadcast address */
#define	int_dstaddr	int_intu.intu_dstaddr	/* other end of p-to-p link */
	u_long	int_net;			/* network # */
	u_long	int_netmask;			/* net mask for addr */
	u_long	int_subnet;			/* subnet # */
	u_long	int_subnetmask;			/* subnet mask for addr */
	int	int_metric;			/* init's routing entry */
	int	int_flags;			/* see below */
	int	int_ipackets;			/* input packets received */
	int	int_opackets;			/* output packets sent */
	char	*int_name;			/* from kernel if structure */
	u_short	int_transitions;		/* times gone up-down */
	int	int_egpsock;			/* egp raw socket */
	int	int_icmpsock;			/* icmp raw socket */
	struct active_gw *int_active_gw;	/* gw's using routing */
	int	int_ripfixedmetric;		/* fixed metric for RIP */
	u_short	int_hellofixedmetric;		/* fixed metric for HELLO */
#ifdef	NSS
	int	int_type;			/* Inter-NSS/Regional */
	struct sockaddr_in intra_nss_int;	/* Intra-NSS interface	*/
#endif	NSS
};

/*
 * 0x1 to 0x10 are reused from the kernel's ifnet definitions,
 * the others agree with the RTS_ flags defined elsewhere.
 *	0x20 - 0x200 were used by the kernel in 4.3 BSD, had to push up
 *	the other defines as shown below.  The corresponding RTS_ flags
 *	were also pushed up.	- fedor
 */
#define	IFF_UP		0x1		/* interface is up */
#define	IFF_BROADCAST	0x2		/* broadcast address valid */
#define	IFF_DEBUG	0x4		/* turn on debugging */
#define	IFF_ROUTE	0x8		/* routing entry installed */
#define	IFF_POINTOPOINT	0x10		/* interface is point-to-point link */
#ifdef sgi
#define	IFF_RUNNING	0x40		/* interface is happy */
#define IFF_MASK        0x5F            /* Values to read from kernel */
#define _IFF_UP_RUNNING(flags)	\
		((flags & (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING))
#else
#define	IFF_MASK	0x1F		/* Values to read from kernel */
#endif

#define IFF_SUBNET	0x800		/* is this a subnet interface? */
#define	IFF_PASSIVE	0x1000		/* can't tell if up/down */
#define	IFF_INTERFACE	0x2000		/* hardware interface */
#define	IFF_REMOTE	0x4000		/* interface isn't on this machine */

#define	IFF_NORIPOUT	0x8000		/* Talk RIP on this interface? */
#define	IFF_NORIPIN	0x10000		/* Listen to RIP on this interface? */
#define	IFF_NOHELLOOUT	0x20000		/* Talk HELLO on this interface? */
#define	IFF_NOHELLOIN	0x40000		/* Listen to HELLO on this interface? */
#define	IFF_NOAGE	0x80000		/* don't time out/age this interface */
#define	IFF_ANNOUNCE	0x100000	/* send info on only what is listed */
#define	IFF_NOANNOUNCE	0x200000	/* send info on everything but listed */
#define IFF_RIPFIXEDMETRIC	0x400000	/* Use a fixed metric for RIP out */
#define IFF_HELLOFIXEDMETRIC	0x800000	/* Use a fixed metric for HELLO out */

#define	IFF_KEEPMASK	0x1ff8000	/* Flags to maintain through an if_check() */
