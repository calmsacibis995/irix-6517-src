/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)ip_var.h	7.7 (Berkeley) 6/28/90 plus MULTICAST 1.0
 */

#ifndef __NETINET_IP_VAR_H__
#define __NETINET_IP_VAR_H__
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Overlay for ip header used by other protocols (tcp, udp).
 */
struct ipovly {
#if (_MIPS_SZLONG == 32)
	struct tcpiphdr *ih_next;	/* for protocol sequencing */
	struct tcpiphdr *ih_prev;
#else
	__uint32_t ih_next, ih_prev;	
#endif
	u_char	ih_x1;			/* (unused) */
	u_char	ih_pr;			/* protocol */
	u_short	ih_len;			/* protocol length */
	struct	in_addr ih_src;		/* source internet address */
	struct	in_addr ih_dst;		/* destination internet address */
};

/*
 * Ip reassembly queue structure.  Each fragment
 * being reassembled is attached to one of these structures.
 * They are timed out after ipq_ttl drops to 0, and may also
 * be reclaimed if memory becomes tight.
 */
struct ipq {
	struct 	ipq *next;
	struct 	ipq *prev;
	u_char	ipq_ttl;		/* time for reass q to live */
	u_char	ipq_p;			/* protocol of this fragment */
	u_short	ipq_id;			/* sequence id for reassembly */
#if (_MIPS_SZLONG == 32)
	struct	ipasfrag *ipq_next,*ipq_prev;
					/* To ip headers of fragments. */
#endif
	struct	in_addr ipq_src,ipq_dst;
	struct 	mbuf *ipq_mbuf;		/* back pointer to mbuf header */
};

/*
 * Ip header, when holding a fragment.
 *
 * Note: ipf_next must be at same offset as ipq_next above
 */
struct	ipasfrag {
#if BYTE_ORDER == LITTLE_ENDIAN 
	u_char	ip_hl:4,
		ip_v:4;
#endif
#if BYTE_ORDER == BIG_ENDIAN 
	u_char	ip_v:4,
		ip_hl:4;
#endif
	u_char	ipf_mff;		/* copied from (ip_off&IP_MF) */
	u_short	ip_len;
	u_short	ip_id;
	u_short	ip_off;
	u_char	ip_ttl;
	u_char	ip_p;
	u_short	ip_sum;
#if (_MIPS_SZLONG != 32)
	__uint64_t filler;		/* not used in 64-bit, but need to be
					   here to keep size constant */
#else
	struct	ipasfrag *ipf_next;	/* next fragment */
	struct	ipasfrag *ipf_prev;	/* previous fragment */
#endif
};

/*
 * Structure stored in mbuf in inpcb.ip_options
 * and passed to ip_output when ip options are in use.
 * The actual length of the options (including ipopt_dst)
 * is in m_len.
 */
#define MAX_IPOPTLEN	40

struct ipoption {
	struct	in_addr ipopt_dst;	/* first-hop dst if source routed */
	char	ipopt_list[MAX_IPOPTLEN];	/* options proper */
};

/*
 * Helper struct for all the memberships on a given socket
 * This list protected by the socket lock.
 */

struct ip_membership {
	struct ip_membership *next;
	struct in_multi *ipm_membership;
};

/*
 * Structure stored in an mbuf attached to inpcb.ip_moptions and
 * passed to ip_output when IP multicast options are in use.
 */
struct ip_moptions {
	struct	ifnet   *imo_multicast_ifp;  /* ifp for outgoing multicasts */
	u_char	         imo_multicast_ttl;  /* TTL for outgoing multicasts */
	u_char		 imo_multicast_loop; /* 1 => hear sends if a member */
	int		 imo_multicast_vif;  /* vif num outgoing multicasts */
	struct ip_membership *imo_membership; /* list of memberships */
};

/*
 * SGI: The statistics formerly in this file are now in sys/tcpipstats.h
 */
#ifdef _KERNEL

#include "sys/sema.h"

/* flags passed to ip_output as last parameter */
#define	IP_FORWARDING		0x1		/* most of ip header exists */
#define	IP_RAWOUTPUT		0x2		/* raw ip header exists */
#define IP_MULTICASTOPTS	0x4		/* multicast opts present */
#define IP_SWIPED               0x8             /* swIPe encrypted packet */
#define	IP_ROUTETOIF		SO_DONTROUTE	/* bypass routing tables */
#define	IP_ALLOWBROADCAST	SO_BROADCAST	/* can send broadcast packets */

extern hotIntCounter_t	ip_id;			/* ip packet ctr, for ids */

struct route;
struct socket;
struct ipsec;

extern	int ip_pcbopts(struct mbuf **, struct mbuf *);
extern	void ip_freemoptions(struct mbuf *);
extern	int ip_getmoptions(int, struct mbuf *, struct mbuf **);
extern	int ip_setmoptions(int, struct mbuf **, struct mbuf *);
extern	void ip_stripoptions( struct mbuf *, struct mbuf *);

extern	int ip_ctloutput(int, struct socket *, int, int, struct mbuf **);

extern	int	ip_output(struct mbuf *, struct mbuf *, struct route *, int,
			  struct mbuf *, struct ipsec *);

extern int	rip_ctloutput(int, struct socket *, int, int,
			      struct mbuf **);
#ifdef INET6
extern void	rip_input(struct mbuf *, struct ifnet *, struct ipsec *,
  struct mbuf *);
#else
extern void	rip_input(struct mbuf *, struct ifnet *, struct ipsec *);
#endif
extern int	rip_output(struct mbuf *, struct socket *, __uint32_t);
extern int	rip_usrreq(struct socket *, int, struct mbuf *,
			   struct mbuf *, struct mbuf *);
#endif /* KERNEL */
#ifdef __cplusplus
}
#endif
#endif /* __NETINET_IP_VAR_H__ */
