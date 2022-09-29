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
 *	@(#)tcp_debug.c	7.2 (Berkeley) 12/7/87
 */

#ifdef sgi
#include "tcp-param.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#define PRUREQUESTS
#include "sys/protosw.h"
#include "sys/errno.h"
#include "sys/debug.h"


#else /* !sgi */
#include "param.h"
#include "systm.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"
#define PRUREQUESTS
#include "protosw.h"
#include "errno.h"
#endif /* sgi */

#include "net/route.h"
#include "net/if.h"
#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "ip_var.h"
#include "in_pcb.h"
#include "tcp.h"
#define TCPSTATES
#include "tcp_fsm.h"
#include "tcp_seq.h"
#define	TCPTIMERS
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#define	TANAMES
#include "tcp_debug.h"

#ifdef sgi
struct	tcp_debug tcp_debug[TCP_NDEBUG];
int	tcpconsdebug = 1;
#else
int	tcpconsdebug = 0;
#endif /* sgi */
/*
 * Tcp debug routines
 */
#ifdef sgi
void
#endif
tcp_trace(
	short act,
	short ostate,
	struct tcpcb *tp,
#ifdef INET6
	void *ti,
#else
	struct tcpiphdr *ti,
#endif
	int req)
{
	tcp_seq seq, ack;
	int len, flags;
	struct tcp_debug *td = &tcp_debug[tcp_debx++];
#ifdef INET6
	struct  tcphdr *th;
#endif

	if (tcp_debx == TCP_NDEBUG)
		tcp_debx = 0;
	td->td_time = iptime();
	td->td_act = act;
	td->td_ostate = ostate;
	td->td_tcb = (caddr_t)tp;
	if (tp)
		td->td_cb = *tp;
	else
		bzero((caddr_t)&td->td_cb, sizeof (*tp));
#ifdef INET6
	if (ti) {
		if ((((struct tcpip6hdr *)ti)->ti6_i.ip6_head &
		  IPV6_FLOWINFO_VERSION) != IPV6_VERSION) {
			th = &((struct tcpiphdr *)ti)->ti_t;
			len = ((struct tcpiphdr *)ti)->ti_len;
			td->u.td_ti = *((struct tcpiphdr *)ti);
		} else {
			th = &((struct tcpip6hdr *)ti)->ti6_t;
			len = ((struct tcpip6hdr *)ti)->ti6_len;
			td->u.td_ti6 = *((struct tcpip6hdr *)ti);
		}
	} else
		bzero((caddr_t)&td->u.td_ti, sizeof (struct tcpiphdr));
#else
	if (ti)
		td->td_ti = *ti;
	else
		bzero((caddr_t)&td->td_ti, sizeof (*ti));
#endif
	td->td_req = req;
	if (tcpconsdebug == 0)
		return;
	if (tp)
		printf("%x %s:", tp, tcpstates[ostate]);
	else
		printf("???????? ");
	printf("%s ", tanames[act]);
	switch (act) {

	case TA_INPUT:
	case TA_OUTPUT:
	case TA_DROP:
		if (ti == 0)
			break;
#ifdef INET6
		seq = th->th_seq;
		ack = th->th_ack;
#else
		seq = ti->ti_seq;
		ack = ti->ti_ack;
		len = ti->ti_len;
#endif
		if (act == TA_OUTPUT) {
			seq = ntohl(seq);
			ack = ntohl(ack);
			len = ntohs((u_short)len);
		}
		if (act == TA_OUTPUT)
			len -= sizeof (struct tcphdr);
		if (len)
			printf("[%x..%x)", seq, seq+len);
		else
			printf("%x", seq);
#ifdef INET6
		printf("@%x, urp=%x", ack, th->th_urp);
		flags = th->th_flags;
#else
		printf("@%x, urp=%x", ack, ti->ti_urp);
		flags = ti->ti_flags;
#endif
		if (flags) {
#ifndef lint
			char *cp = "<";
#ifdef sgi
#include <sys/cdefs.h>
#ifdef INET6
#define pf(f) { if (th->th_flags&__CONCAT(TH_,f)) { printf("%s%s", cp, "f"); cp = ","; } }
#else
#define pf(f) { if (ti->ti_flags&__CONCAT(TH_,f)) { printf("%s%s", cp, "f"); cp = ","; } }
#endif
#else
#ifdef INET6
#define pf(f) { if (th->th_flags&TH_/**/f) { printf("%s%s", cp, "f"); cp = ","; } }
#else
#define pf(f) { if (ti->ti_flags&TH_/**/f) { printf("%s%s", cp, "f"); cp = ","; } }
#endif
#endif
			pf(SYN); pf(ACK); pf(FIN); pf(RST); pf(PUSH); pf(URG);
#endif
			printf(">");
		}
		break;

	case TA_USER:
		printf("%s", prurequests[req&0xff]);
		if ((req & 0xff) == PRU_SLOWTIMO)
			printf("<%s>", tcptimers[req>>8]);
		break;
	}
	if (tp)
		printf(" -> %s", tcpstates[tp->t_state]);
	/* print out internal state of tp !?! */
	printf("\n");
	if (tp == 0)
		return;
	printf("\trcv_(nxt,wnd,up) (%x,%x,%x) snd_(una,nxt,max) (%x,%x,%x)\n",
	    tp->rcv_nxt, tp->rcv_wnd, tp->rcv_up, tp->snd_una, tp->snd_nxt,
	    tp->snd_max);
	printf("\tsnd_(wl1,wl2,wnd) (%x,%x,%x)\n",
	    tp->snd_wl1, tp->snd_wl2, tp->snd_wnd);
}
