#ifdef INET6
/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)tcp_subr.c	8.1 (Berkeley) 6/10/93
 */

#include <tcp-param.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/tcpipstats.h>
#include <sys/protosw.h>
#include <sys/errno.h>

#include <net/route.h>
#include <net/if.h>

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
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp6_var.h>
#include <netinet/tcpip.h>

extern void tcp_notify(struct inpcb *, int, void *);
extern void tcp_quench(struct inpcb *, int flag, void *data);
extern void in_rtchange(struct inpcb *, int, void *);

/* ARGSUSED */
void
tcp6_ctlinput(cmd, sa, arg)
	int cmd;
	struct sockaddr *sa;
	void *arg;
{
	struct ctli_arg *ca = (struct ctli_arg *)arg;
	register struct ipv6 *ip = 0;
	register struct tcphdr *th;
	extern u_char inetctlerrmap[];
	void (*notify)(struct inpcb *, int, void *) = tcp_notify;
	extern tcp_msgsize(struct inpcb *inp, int errno,
	  void *icp, struct ipsec *ipsec);

	if (ca)
		ip = ca->ctli_ip;
	if (cmd == PRC_QUENCH)
		notify = tcp_quench;
	else if (cmd == PRC_MSGSIZE) {
		if (ip == 0)
			return;
		notify = (void(*)(struct inpcb *, int, void *))tcp_msgsize;
		/* notify will be sent to only one PCB ?!? */
	} else if (!PRC_IS_REDIRECT(cmd) &&
	  ((unsigned)cmd > PRC_NCMDS || inetctlerrmap[cmd] == 0))
		return;
	if (ip) {
		th = (struct tcphdr *)(ip + 1);
		in6_pcbnotify(&tcb, sa, th->th_dport,
			      &ip->ip6_src, th->th_sport,
			      cmd, notify, 0);
	} else
		in6_pcbnotify(&tcb, sa, 0, NULL, 0, cmd, notify, 0);
}

/*
 * Look-up the routing entry to the peer of this inpcb.  If no route
 * is found and it cannot be allocated the return NULL.  This routine
 * is called by TCP routines that access the rmx structure and by tcp_mss
 * to get the interface MTU.
 */
struct rtentry *
tcp6_rtlookup(inp)
	struct inpcb *inp;
{
	struct route *ro;
	struct rtentry *rt;

	ro = &inp->inp_route;
	ROUTE_RDLOCK();
	rt = ro->ro_rt;
	if (rt == NULL || !(rt->rt_flags & RTF_UP)) {
		/* No route yet, so try to acquire one */
		if (inp->inp_fatype & IPATYPE_IPV6) {
			ro->ro_dst.sa_family = AF_INET6;
			((struct sockaddr_new *)&ro->ro_dst)->sa_len =
			  sizeof(struct sockaddr_in6);
			COPY_ADDR6(inp->inp_faddr6,
				   satosin6(&ro->ro_dst)->sin6_addr);
			in6_rtalloc(ro, INP_IFA);
			rt = ro->ro_rt;
		}
	}
	ROUTE_UNLOCK();
	return rt;
}
#endif /* INET6 */
