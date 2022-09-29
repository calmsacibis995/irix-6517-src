/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *	@(#)tcpip.h	7.2 (Berkeley) 12/7/87
 */

#ifndef _NETINET_TCPIP_H
#define _NETINET_TCPIP_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/ip6_var.h>
#endif

/*
 * Tcp+ip header, after ip options removed.
 */
struct tcpiphdr {
	struct 	ipovly ti_i;		/* overlaid ip structure */
	struct	tcphdr ti_t;		/* tcp header */
};
#define	ti_next		ti_i.ih_next
#define	ti_prev		ti_i.ih_prev
#define	ti_x1		ti_i.ih_x1
#define	ti_pr		ti_i.ih_pr
#define	ti_len		ti_i.ih_len
#define	ti_src		ti_i.ih_src
#define	ti_dst		ti_i.ih_dst
#define	ti_sport	ti_t.th_sport
#define	ti_dport	ti_t.th_dport
#define	ti_seq		ti_t.th_seq
#define	ti_ack		ti_t.th_ack
#define	ti_x2		ti_t.th_x2
#define	ti_off		ti_t.th_off
#define	ti_flags	ti_t.th_flags
#define	ti_win		ti_t.th_win
#define	ti_sum		ti_t.th_sum
#define	ti_urp		ti_t.th_urp

#ifdef INET6
/*
 * IPv6+TCP headers.
 */
struct tcpip6hdr {
	struct 	ipv6 ti6_i;		/* IPv6 header */
	struct	tcphdr ti6_t;		/* TCP header */
};
#define	ti6_head	ti6_i.ip6_head
#define	ti6_len		ti6_i.ip6_len
#define	ti6_nh		ti6_i.ip6_nh
#define	ti6_hlim	ti6_i.ip6_hlim
#define	ti6_src		ti6_i.ip6_src
#define	ti6_dst		ti6_i.ip6_dst
#define	ti6_sport	ti6_t.th_sport
#define	ti6_dport	ti6_t.th_dport
#define	ti6_seq		ti6_t.th_seq
#define	ti6_ack		ti6_t.th_ack
#define	ti6_x2		ti6_t.th_x2
#define	ti6_off		ti6_t.th_off
#define	ti6_flags	ti6_t.th_flags
#define	ti6_win		ti6_t.th_win
#define	ti6_sum		ti6_t.th_sum
#define	ti6_urp		ti6_t.th_urp


struct tcp6hdrs {
	struct	tcpip6hdr tr_ti6;		/* headers */
};

#define tptolnk6(x)	(&(x)->u.t_lnk6)

/*
 * We store the next and prev pointers in the src addr field of the ip header.
 * The second pointer starts at s6_addr32[2] so that 64bit pointers can fit.
 * That way 32bit and 64bit kernels use the same code.
 */
#define TCP6_NEXT_GET(x) \
	(*((struct  tcp6hdrs **)&(x)->tr_ti6.ti6_i.ip6_src.s6_addr32[0]))
#define TCP6_NEXT_PUT(x,v) \
	(*((struct  tcp6hdrs **)&(x)->tr_ti6.ti6_i.ip6_src.s6_addr32[0])) = v
#define TCP6_PREV_GET(x) \
	(*((struct  tcp6hdrs **)&(x)->tr_ti6.ti6_i.ip6_src.s6_addr32[2]))
#define TCP6_PREV_PUT(x,v) \
	(*((struct  tcp6hdrs **)&(x)->tr_ti6.ti6_i.ip6_src.s6_addr32[2])) = v

#define	tr_i6		tr_ti6.ti6_i
#define	tr_head		tr_ti6.ti6_head
#define	tr_len		tr_ti6.ti6_len
#define	tr_nh		tr_ti6.ti6_nh
#define	tr_hlim		tr_ti6.ti6_hlim
#define	tr_src		tr_ti6.ti6_src
#define	tr_dst		tr_ti6.ti6_dst
#define	tr_t		tr_ti6.ti6_t
#define	tr_sport	tr_ti6.ti6_sport
#define	tr_dport	tr_ti6.ti6_dport
#define	tr_seq		tr_ti6.ti6_seq
#define	tr_ack		tr_ti6.ti6_ack
#define	tr_x2		tr_ti6.ti6_x2
#define	tr_off		tr_ti6.ti6_off
#define	tr_flags	tr_ti6.ti6_flags
#define	tr_win		tr_ti6.ti6_win
#define	tr_sum		tr_ti6.ti6_sum
#define	tr_urp		tr_ti6.ti6_urp

/*
 * Dual template for IPv4/IPv6 TCP.
 *
 * Optimized for IPv4
 */
struct tcptemp {
	struct	ipovly tt_i;		/* overlaid ip structure */
	struct	tcphdr tt_t;		/* tcp header */
	struct	ip6ovck tt_i6;		/* IPv6 header^2 */
	struct	in6_addr tt_src6;	/* source address */
	struct	in6_addr tt_dst6;	/* destination address */
};
#define	tt_pr		tt_i.ih_pr
#define	tt_len		tt_i.ih_len
#define	tt_src		tt_i.ih_src
#define	tt_dst		tt_i.ih_dst
#define	tt_sport	tt_t.th_sport
#define	tt_dport	tt_t.th_dport
#define	tt_off		tt_t.th_off
#define	tt_pr6		tt_i6.ih6_pr
#define	tt_len6		tt_i6.ih6_len
#endif /* INET6 */

#ifdef __cplusplus
}
#endif
#endif	/* _NETINET_TCPIP_H */
