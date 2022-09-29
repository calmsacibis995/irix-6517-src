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
 *	@(#)tcp.h	7.7 (Berkeley) 6/28/90
 */
#ifndef __NETINET_TCP_H__
#define __NETINET_TCP_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <sgidefs.h>
#include <sys/endian.h>

typedef	__uint32_t tcp_seq;

/*
 * TCP header.
 * Per RFC 793, September, 1981.
 */
struct tcphdr {
	u_short	th_sport;		/* source port */
	u_short	th_dport;		/* destination port */
	tcp_seq	th_seq;			/* sequence number */
	tcp_seq	th_ack;			/* acknowledgement number */
/*
 * NOTE:  strict compilers will complain about the use of bitfields
 *        for char-width variables.  But the field is properly
 *        described, here.
 */
#if BYTE_ORDER == LITTLE_ENDIAN 
	u_char	th_x2:4,		/* (unused) */
		th_off:4;		/* data offset */
#endif
#if BYTE_ORDER == BIG_ENDIAN 
	u_char	th_off:4,		/* data offset */
		th_x2:4;		/* (unused) */
#endif
	u_char	th_flags;
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
	u_short	th_win;			/* window */
	u_short	th_sum;			/* checksum */
	u_short	th_urp;			/* urgent pointer */
};

#define	TCPOPT_EOL		0
#define	TCPOPT_NOP		1
#define	TCPOPT_MAXSEG		2
#define TCPOPT_WINDOW		3       /* RFC 1323 options */
#define TCPOLEN_WINDOW		3
#define TCPOPT_SACKPERMIT	4
#define TCPOLEN_SACKPERMIT	2
#define TCPOPT_SACK		5
#define TCPOLEN_SACKMIN		10	/* min sack option length */
#define TCPOPT_TIMESTAMP	8
#define TCPOLEN_TIMESTAMP	10

/* RFC 1323 Appendix A layout of timestamp option */
#define TCPOLEN_TSTAMP_HDR	(TCPOLEN_TIMESTAMP+2)
#define TCPOPT_TSTAMP_HDR \
     (TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)

/* RFC 2018 Layout of selective acknowledgement option */
#define TCPOPT_SACKPERMIT_HDR \
     (TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_SACKPERMIT<<8|TCPOLEN_SACKPERMIT)

/*
 * Default maximum segment size for TCP.
 * With an IP MSS of 576, this is 536,
 * but 512 is probably more convenient.
 * This should be defined as MIN(512, IP_MSS - sizeof (struct tcpiphdr)).
 */
#define	TCP_MSS	512

#define	TCP_MAXWIN		65535	/* largest value in window field */
#define TCP_MAX_WINSHIFT	14	/* largest shift value for window */

/*
 * User-settable options (used with setsockopt).
 */
#define	TCP_NODELAY	0x01	/* don't delay send to coalesce packets */
#define	TCP_MAXSEG	0x02	/* set maximum segment size (not implemented) */
#define	TCP_FASTACK	0x01001	/* don't delay acknowledgements */

/* for modulo comparisons of RFC 1323 timestamps */
#define TSTMP_LT(a,b)   ((int)((a)-(b)) < 0)

#define TCP_PAWS_IDLE   (24 * 24 * 60 * 60 * PR_SLOWHZ)         /* 24 days */

#ifdef __cplusplus
}
#endif
#endif /* __NETINET_TCP_H__ */
