#ifndef __net_raw_cb__
#define __net_raw_cb__
#ifdef __cplusplus
extern "C" { 
#endif
/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *	@(#)raw_cb.h	7.2 (Berkeley) 12/30/87 plus MULTICAST 1.0
 *	plus portions of 7.6 (Berkeley) 6/28/90
 */

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/route.h>
#include <netinet/in.h>   /* for sockaddr_in6 */
#include <netinet/in_pcb.h>   /* for route_6 */

struct mac_label;

/*
 * Raw protocol interface control block.  Used
 * to tie a socket to the generic raw interface.
 */
struct rawcb {
	struct	rawcb *rcb_next;	/* doubly linked list */
	struct	rawcb *rcb_prev;
	struct	socket *rcb_socket;	/* back pointer to socket */
	struct	sockaddr *rcb_faddr;	/* destination address */
	struct	sockaddr *rcb_laddr;	/* socket's address */
	struct	sockproto rcb_proto;	/* protocol family, protocol */
	caddr_t	rcb_pcb;		/* protocol specific stuff */
	struct	mbuf *rcb_options;	/* protocol specific options */
#ifdef INET6
	union	route_6 rcb_route_6;	/* routing information */
#else
	struct	route rcb_route;	/* routing information */
#endif
	int	rcb_refcnt;		/* for race bet. detach/rawintr */
	short	rcb_flags;
	struct	mbuf *rcb_moptions;	/* proto specific multicast options */
#ifdef INET6
	u_int   rcb_flowinfo;
#endif
};

#ifdef INET6
#define rcb_route	rcb_route_6.route
#endif
#define	RAW_HDRINCL	0x8000		/* XXX user supplies IP header. */

#define	sotorawcb(so)		((struct rawcb *)(so)->so_pcb)

/*
 * Nominal space allocated to a raw socket.
 */
#define	RAWSNDQ		8192
#define	RAWRCVQ		8192

/*
 * Format of raw interface header prepended by
 * raw_input after call from protocol specific
 * input routine.
 */
struct raw_header {
	struct ifheader raw_ifh;	/* needed for all net input */
	struct	sockproto raw_proto;	/* format of packet */
#ifdef INET6
	union {
		struct	sockaddr raw_dst;     /* dst address for rawintr */
		struct	sockaddr_in6 raw_dst6;/* dst address for rawintr */
	} ud;
	union {
		struct	sockaddr raw_src;     /* src address for sbappendaddr */
		struct	sockaddr_in6 raw_src6;/* src address for sbappendaddr */
	} us;
#else
	struct	sockaddr raw_dst;	/* dst address for rawintr */
	struct	sockaddr raw_src;	/* src address for sbappendaddr */
#endif
	struct	mac_label *raw_label;	/* security label for sbappendaddr */
	uid_t   raw_uid;	        /* uid for socket DAC */
};
#ifdef INET6
#define	raw_dst		ud.raw_dst
#define	raw_src		us.raw_src
#define	raw_dst6	ud.raw_dst6
#define	raw_src6	us.raw_src6
#endif

#ifdef _KERNEL
#ifndef _IO_TPI_TPISOCKET_H_		/* only declare once */

struct rawcb rawcb;			/* head of list */
extern int rawcb_rele(struct rawcb *, int);
extern void rawcb_free(struct rawcb *);

#define RAWCB_FREE		0
#define RAWCB_NOFREE		1

extern int rawcb_hold(struct rawcb *);

#define RAWCB_HOLD(rp)  	rawcb_hold(rp)
#define RAWCB_RELEASE(rp, flag)	rawcb_rele(rp, flag) 


#endif 	/* _IO_TPI_TPISOCKET_H_ */
#endif	/* _KERNEL */
#ifdef __cplusplus
}
#endif 
#endif	/* __net_raw_cb__ */
