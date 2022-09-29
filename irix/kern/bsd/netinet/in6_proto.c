#ifdef INET6
/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in_proto.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/radix.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in6_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6_icmp.h>
#include <netinet/igmp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp6_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/udp6_var.h>
/*
 * TCP/IP protocol family: IP, ICMP, UDP, TCP.
 */

extern struct domain inet6domain;
extern void tcp_drain(void);
extern int udp_usrreq();

struct protosw inet6sw[] = {
{ 0,		&inet6domain,	0,		0,
  0,		0,		0,		0,
  0,
  ip6_init,	0,		0,		0,
},
{ SOCK_DGRAM,	&inet6domain,	IPPROTO_UDP,	PR_ATOMIC|PR_ADDR,
  udp6_input,	0, 	udp6_ctlinput, ip6_ctloutput,
  udp_usrreq,
  0,		0,		0,		0,
},
/* the fast and slow timeouts are the same as v4 and thus not used here */
{ SOCK_STREAM,	&inet6domain,	IPPROTO_TCP, PR_CONNREQUIRED|PR_WANTRCVD,
  tcp6_input,	0, 	tcp6_ctlinput,	tcp6_ctloutput,
  tcp6_usrreq,
  0,	0,	0,	tcp_drain,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  rip6_input,	0,		rip6_ctlinput,	rip6_ctloutput,
  rip6_usrreq,
  rip6_init,	0,		0,		0,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_ICMPV6,	PR_ATOMIC|PR_ADDR,
  icmp6_input,	0,		rip6_ctlinput,	rip6_ctloutput,
  rip6_usrreq,
  icmp6_init,	icmp6_fasttimo,	0,		0,
},
{ 0,		&inet6domain,	IP6_NHDR_FRAG,	0,
  frg6_input,	0,		opt6_ctlinput,	0,
  0,
  0,		0,		frg6_slowtimo,	frg6_drain,
},
{ 0,		&inet6domain,	IP6_NHDR_HOP,	0,
  hop6_input,	0,		opt6_ctlinput,	0,
  0,
  0,		0,		0,		0,
},
{ 0,		&inet6domain,	IP6_NHDR_RT,	0,
  rt6_input,	0,		opt6_ctlinput,	0,
  0,
  0,		0,		0,		0,
},
{ 0,		&inet6domain,	IP6_NHDR_DOPT,	0,
  dopt6_input,	0,		opt6_ctlinput,	0,
  0,
  0,		0,		0,		0,
},
{ 0,		&inet6domain,	IP6_NHDR_AUTH,	0,
  ah6_input,	0,		opt6_ctlinput,	0,
  0,
  ipsec_init,	0,		0,		0,
},
{ 0,		&inet6domain,	IP6_NHDR_ESP,	0,
  esp6_input,	0,		opt6_ctlinput,	0,
  0,
  0,		0,		0,		0,
},
{ 0,		&inet6domain,	IP6_NHDR_NONH,	0,
  end6_input,	0,		0,		0,
  0,
  0,		0,		0,		0,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR,
  ip6ip6_input,	0,	0,	rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,
},
	/* raw wildcard */
{ SOCK_RAW,	&inet6domain,	0,		PR_ATOMIC|PR_ADDR,
  rip6_input,	0,		rip6_ctlinput,	rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,
},
};

struct domain inet6domain =
    { AF_INET6, "internet6", 0, 0, 0, 
      inet6sw, &inet6sw[sizeof(inet6sw)/sizeof(inet6sw[0])], 0,
      rn_inithead, 64, sizeof(struct sockaddr_in6)
    };

#endif /* INET6 */
