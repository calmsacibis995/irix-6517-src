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
 *	@(#)in_proto.c	7.2 (Berkeley) 12/7/87 plus MULTICAST 1.0
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/socket.h"
#include "sys/protosw.h"
#include "sys/domain.h"
#include "sys/mbuf.h"
#include "net/radix.h"

#include "in.h"
#include "in_systm.h"
#include "ip_var.h"
#include "if_ether.h"

/*
 * TCP/IP protocol family: IP, ICMP, UDP, TCP.
 */
int	ip_ctloutput();
void	ip_init(),ip_slowtimo(),ip_drain();

void	icmp_input();
void	igmp_init(), igmp_input(), igmp_fasttimo(), igmp_slowtimo();
void	rsvp_input(), ipip_input();	/* stub out if no mrouting */
#ifdef INET6
void	sit_input();
#endif

void	udp_input(),udp_ctlinput();
int	udp_usrreq();

void	udp_init();

void	tcp_input(),tcp_ctlinput();
int	tcp_usrreq(),tcp_ctloutput();

void	tcp_init(),tcp_fasttimo(),tcp_slowtimo(),tcp_drain();

void	rip_input();
int	rip_ctloutput();

void	swinput(),swinit();
int	swoutput();

#ifdef	STP_SUPPORT
void	st_input(), st_ctlinput();
int	st_usrreq(), st_ctloutput();
void    st_init(), st_fasttimo(), st_slowtimo(), st_drain();
#endif	/* STP_SUPPORT */

extern	int raw_usrreq();

extern	struct domain inetdomain;

/* do not mention ip_output() here because it demands different numbers
 * of args than callers of pr_output() provide.
 */
struct protosw inetsw[] = {
{ 0,		&inetdomain,	0,		0,
  0,		0,		0,		0,
  0,
  ip_init,	0,		ip_slowtimo,	ip_drain,
},
{ SOCK_DGRAM,	&inetdomain,	IPPROTO_UDP,	PR_ATOMIC|PR_ADDR|PR_RIGHTS,
  udp_input,	0,		udp_ctlinput,	ip_ctloutput,
  udp_usrreq,
  udp_init,	0,		0,		0,
},
{ SOCK_STREAM,	&inetdomain,	IPPROTO_TCP,	PR_CONNREQUIRED|PR_WANTRCVD,
  tcp_input,	0,		tcp_ctlinput,	tcp_ctloutput,
  tcp_usrreq,
  tcp_init,	tcp_fasttimo,	tcp_slowtimo,	tcp_drain,
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  rip_input,	rip_output,	0,		rip_ctloutput,
  raw_usrreq,
  0,		0,		0,		0,
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_ICMP,	PR_ATOMIC|PR_ADDR,
  icmp_input,	rip_output,	0,		rip_ctloutput,
  raw_usrreq,
  0,		0,		0,		0,
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_IGMP,	PR_ATOMIC|PR_ADDR,
  igmp_input,	rip_output,	0,		rip_ctloutput,
  raw_usrreq,
  igmp_init,	igmp_fasttimo,	igmp_slowtimo,	0,
},
	/* IP over IP (tunnels) */
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPIP,	PR_ATOMIC|PR_ADDR,
  ipip_input,	rip_output,	0,		rip_ctloutput,
  raw_usrreq,
  0,		0,		0,		0,
},
#ifdef INET6
{ SOCK_RAW,     &inetdomain,    IPPROTO_IPV6,   PR_ATOMIC|PR_ADDR,
  sit_input,    rip_output,     0,		rip_ctloutput,
  raw_usrreq,
  0,		0,		0,		0,
},
#endif
	/* IP reservation protocol hook */
{ SOCK_RAW,	&inetdomain,	IPPROTO_RSVP,	PR_ATOMIC|PR_ADDR,
  rsvp_input,	rip_output,	0,		rip_ctloutput,
  raw_usrreq,
  0,		0,		0,		0,
},
	/* swipe protocol hook */
{ SOCK_RAW,	&inetdomain,	IPPROTO_SWIPE,	PR_CONNREQUIRED|PR_WANTRCVD,
  swinput,	swoutput,	0,		0,
  0,
  swinit,	0,		0,		0,
},
	/* raw wildcard */
{ SOCK_RAW,	&inetdomain,	0,		PR_ATOMIC|PR_ADDR,
  rip_input,	rip_output,	0,		rip_ctloutput,
  raw_usrreq,
  0,		0,		0,		0,
},
#ifdef	STP_SUPPORT
{ SOCK_DGRAM,	&inetdomain,	IPPROTO_STP,	PR_CONNREQUIRED|PR_WANTRCVD,
  st_input,		0,		0,		0,
  st_usrreq,
  st_init,		0,	st_slowtimo,		0,
},
#endif	/* STP_SUPPORT */
};

struct domain inetdomain = { AF_INET, "internet", 0, 0, 0,
	inetsw, &inetsw[sizeof(inetsw)/sizeof(inetsw[0])], 0,
	rn_inithead, 32, sizeof(struct sockaddr_inarp) };
