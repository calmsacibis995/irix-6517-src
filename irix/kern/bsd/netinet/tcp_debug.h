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
 *	@(#)tcp_debug.h	7.2 (Berkeley) 12/7/87
 */
#ifdef __cplusplus
extern "C" {
#endif

struct	tcp_debug {
	n_time	td_time;
	short	td_act;
	short	td_ostate;
	caddr_t	td_tcb;
#ifdef INET6
	union {
		struct	tcpiphdr td_ti;
		struct	tcpip6hdr td_ti6;
	} u;
#else
	struct	tcpiphdr td_ti;
#endif
	short	td_req;
	struct	tcpcb td_cb;
};

#define	TA_INPUT 	0
#define	TA_OUTPUT	1
#define	TA_USER		2
#define	TA_RESPOND	3
#define	TA_DROP		4

#ifdef TANAMES
char	*tanames[] =
    { "input", "output", "user", "respond", "drop" };
#endif

#define	TCP_NDEBUG 100
#ifndef __sgi
struct	tcp_debug tcp_debug[TCP_NDEBUG];
#endif
int	tcp_debx;

#ifdef __sgi
struct tcpcb;
struct tcpiphdr;
#ifdef INET6
extern void tcp_trace(short, short, struct tcpcb *, void *, int);
#else
extern void tcp_trace(short, short, struct tcpcb *, struct tcpiphdr *, int);
#endif
#endif
#ifdef __cplusplus
}
#endif
