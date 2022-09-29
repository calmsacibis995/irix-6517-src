/*
 * Copyright (c) 1982, 1986, 1988, 1990 Regents of the University of California.
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
 *	@(#)tcp_subr.c	7.19 (Berkeley) 7/25/90
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/tcpipstats.h"
#include "sys/protosw.h"

#include "sys/sysmacros.h"
#include "sys/errno.h"
#include "net/route.h"
#include "net/if.h"
#include "sys/types.h"
#include "sys/kmem.h"

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include "in_pcb.h"
#include "ip_var.h"
#include "ip_icmp.h"
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#ifdef INET6
#include "tcp6_var.h"
#endif
#include "tcpip.h"
#include "tcpip_ovly.h"
#ifdef INET6
#include <netinet/in6_var.h>
#include <netinet/ip6_icmp.h>
#endif

#ifdef TCPPRINTFS
extern int tcpprintfs;
#endif

/* patchable/settable parameters for tcp */
extern int tcp_ttl;
int 	tcp_mssdflt = TCP_MSS;
int 	tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;

extern int tcp_winscale;  	/* disable/enable RFC 1323 window scaling */
extern int tcp_tsecho;		/* disable/enable RFC 1323 timestamps */
extern int tcp_mtudisc;		/* MTUDisc */
extern int tcp_gofast;		/* LAN go fast mode; systuneable */

extern void m_pcbinc(int nbytes, int type);
extern void m_pcbdec(int nbytes, int type);
extern void m_mcbfail(void);

/*
 * Each of the following TWO constants MUST be power of 2
 *
 * The dynamic sizing algorithm is computed in the procedure
 * in_pcb_hashtablesize() which is sized depending on the physical size of
 * main memory. The maximum for TCP is TCP_MAXHASHTABLESZ but NOT less
 * than TCP_MINHASHTABLESZ.
 */
#define TCP_MINHASHTABLESZ		64
#define TCP_MAXHASHTABLESZ		8192

extern int in_pcb_hashtablesize(void);
extern int tcp_hashtablesz;

zone_t	*tcpcb_zone;
zone_t	*tcpsack_zone;

/* Make the least significant bits of the sequence number
 * hard to predict to combat source address spoofing.
 */
tcp_seq
iss_hash(struct tcpcb *tp)
{
	MD5_CTX md5_ctx;
	struct timespec tv;
	u_short *sp;
	union {
		u_int32_t n[4];
		u_char	b[MD5_DIGEST_LEN];
	} r;

	/* crunch the current time with nanosecond resolution, the
	 * address of the tcpcb, and the template psuedo-header
	 * into a few bytes, and then compute the MD5 hash of the result.
	 */
	nanotime(&tv);
	tv.tv_sec += (u_long)tp;
	sp = (u_short *)&tp->t_template;
	sp += sizeof(tp->t_template)/sizeof(*sp);
	while (sp != (u_short *)&tp->t_template)
		tv.tv_sec += *--sp;
	MD5Init(&md5_ctx);
	MD5Update(&md5_ctx, (u_char *)&tv, sizeof(tv));
	MD5Final(r.b, &md5_ctx);
	return (r.n[0] + r.n[1] + r.n[2] + r.n[3])%(TCP_ISSINCR/8);
}



/*
 * Tcp initialization
 */
void
tcp_init(void)
{
	static int tcp_inited = 0;

	if (tcp_inited == 0) {
		tcpcb_zone = kmem_zone_init(sizeof(struct tcpcb_sko), "tcpcb");
		tcpsack_zone = kmem_zone_init(sizeof(struct sack_sndhole), "tcpsack");
		tcp_inited = 1;
	}
	tcp_iss = 1;		/* wrong */

	if (tcp_hashtablesz == 0) { /* dynamic sizing */
		tcp_hashtablesz = in_pcb_hashtablesize();
	}
	if (tcp_hashtablesz < TCP_MINHASHTABLESZ)
		tcp_hashtablesz = TCP_MINHASHTABLESZ;

	if (tcp_hashtablesz > TCP_MAXHASHTABLESZ)
		tcp_hashtablesz = TCP_MAXHASHTABLESZ;
	
	tcp_hashtablesz *= 2; 	/* room for TIME-WAITers */
	(void)in_pcbinitcb(&tcb, tcp_hashtablesz, 0, TCP_PCBSTAT);
	return;
}

/*
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, allocates an mbuf and fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 */
void
tcp_template(struct tcpcb *tp)
{
	register struct inpcb *inp = tp->t_inpcb;
#ifdef INET6
	register struct tcptemp *n = &tp->t_template;

	bzero((caddr_t)n, sizeof (struct tcptemp));
	n->tt_pr = IPPROTO_TCP;
	n->tt_len = htons(sizeof (struct tcpiphdr) - sizeof (struct ip));
	n->tt_src = inp->inp_laddr;
	n->tt_dst = inp->inp_faddr;
	n->tt_sport = inp->inp_lport;
	n->tt_dport = inp->inp_fport;
	n->tt_off = 5;
	n->tt_pr6 = IPPROTO_TCP;
	n->tt_len6 = n->tt_len;
	COPY_ADDR6(inp->inp_laddr6, n->tt_src6);
	COPY_ADDR6(inp->inp_faddr6, n->tt_dst6);
#else
	register struct tcpiphdr *n = &tp->t_template;

	n->ti_next = n->ti_prev = 0;
	n->ti_x1 = 0;
	n->ti_pr = IPPROTO_TCP;
	n->ti_len = htons(sizeof (struct tcpiphdr) - sizeof (struct ip));
	n->ti_src = inp->inp_laddr;
	n->ti_dst = inp->inp_faddr;
	n->ti_sport = inp->inp_lport;
	n->ti_dport = inp->inp_fport;
	n->ti_seq = 0;
	n->ti_ack = 0;
	n->ti_x2 = 0;
	n->ti_off = 5;
	n->ti_flags = 0;
	n->ti_win = 0;
	n->ti_sum = 0;
	n->ti_urp = 0;
#endif
	return;
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == 0, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection tp->t_template.  If flags are given
 * then we send a message back to the TCP which originated the
 * segment ti, and discard the mbuf containing it and any other
 * attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 */
void
tcp_respond(
	struct tcpcb *tp,
#ifdef INET6
	void *template,
#else
	register struct tcpiphdr *ti,
#endif
	register struct mbuf *m,
	tcp_seq ack,
	tcp_seq seq,
	int flags,
#ifdef INET6
	struct ipsec * ipsec,
	int vers)
#else
	struct ipsec * ipsec)
#endif
{
	register int tlen;
	int win = 0;
	struct route *ro = 0;
#ifdef INET6
	struct tcphdr *th;
	register struct ip *ip4;
	register struct ipv6 *ip6;
	int iphlen;

	if (vers == AF_INET)
		iphlen = sizeof(struct ip);
	else
		iphlen = sizeof(struct ipv6);
#endif
	if (tp) {
		win = sbspace(&tp->t_inpcb->inp_socket->so_rcv);
		ro = &tp->t_inpcb->inp_route;
	}
	if (m == 0) {
		m = m_get(M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return;
		/* TCP_COMPAT_42 */
		tlen = 1;
#ifdef INET6
		bcopy(template, mtod(m, struct tcpiphdr *),
		  iphlen + sizeof (struct tcphdr));
		ip4 = mtod(m, struct ip *);
		th = (struct tcphdr *)((char *)ip4 + iphlen);
		ip6 = (struct ipv6 *)ip4;
#else
		*mtod(m, struct tcpiphdr *) = *ti;
		ti = mtod(m, struct tcpiphdr *);
#endif
		flags = TH_ACK;
	} else {
		m_freem(m->m_next);
		m->m_next = 0;
#ifdef INET6
		m->m_off = (__psint_t)template - (__psint_t)m;
		ASSERT((char *)template == mtod(m, char *));
#else
		m->m_off = (__psint_t)ti - (__psint_t)m;
		ASSERT((char *)ti == mtod(m, char *));
#endif
		tlen = 0;
#ifdef INET6
		th = (struct tcphdr *)(mtod(m, caddr_t) + iphlen);
#endif
#define xchg(a,b,type) { type t; t=a; a=b; b=t; }
#ifdef INET6
#define xchg6(a,b,type) { type t; COPY_ADDR6(a,t); COPY_ADDR6(b,a); \
 COPY_ADDR6(t,b); }
		if (vers == AF_INET) {
			ip4 = mtod(m, struct ip *);
			xchg(ip4->ip_dst.s_addr, ip4->ip_src.s_addr, u_long);
		} else {
			ip6 = mtod(m, struct ipv6 *);
			xchg6(ip6->ip6_dst, ip6->ip6_src, struct in6_addr);
		}
		xchg(th->th_dport, th->th_sport, u_short);
#undef xchg6
#else
		xchg(ti->ti_dst.s_addr, ti->ti_src.s_addr, u_long);
		xchg(ti->ti_dport, ti->ti_sport, u_short);
#endif /* INET6 */
#undef xchg
	}
#ifdef INET6
	if (vers == AF_INET) {
		struct tcpiphdr *ti;
		ti = mtod(m, struct tcpiphdr *);
		ti->ti_len = htons((u_short)(sizeof (struct tcphdr) + tlen));
		ti->ti_next = ti->ti_prev = 0;
		ti->ti_x1 = 0;
	} else {
		struct ip6ovck *ipc;
		ipc = mtod(m, struct ip6ovck *);
		ipc->ih6_wrd1 = ipc->ih6_wrd0 = 0;
		ipc->ih6_len = htons(tlen - sizeof(struct ipv6));
		ipc->ih6_pr = IPPROTO_TCP;
	}

	tlen += sizeof (struct tcphdr) + iphlen;
#else
	ti->ti_len = htons((u_short)(sizeof (struct tcphdr) + tlen));
	tlen += sizeof (struct tcpiphdr);
#endif
	m->m_len = tlen;

#ifdef INET6
	th->th_seq = htonl(seq);
	th->th_ack = htonl(ack);
	th->th_x2 = 0;
	th->th_off = sizeof (struct tcphdr) >> 2;
	th->th_flags = flags;
#else
	ti->ti_next = ti->ti_prev = 0;
	ti->ti_x1 = 0;
	ti->ti_seq = htonl(seq);
	ti->ti_ack = htonl(ack);
	ti->ti_x2 = 0;
	ti->ti_off = sizeof (struct tcphdr) >> 2;
	ti->ti_flags = flags;
#endif

#ifdef INET6
	if ( tp )
		th->th_win = htons( (u_short) (win >> tp->rcv_scale) );
	else
		th->th_win = htons((u_short)win);
	th->th_urp = 0;
	/* Don't assume this field has been zeroed */
	th->th_sum = 0;
	th->th_sum = in_cksum(m, tlen);
#else
	if ( tp )
		ti->ti_win = htons( (u_short) (win >> tp->rcv_scale) );
	else
		ti->ti_win = htons((u_short)win);
	ti->ti_urp = 0;
	/* Don't assume this field has been zeroed */
	ti->ti_sum = 0;
	ti->ti_sum = in_cksum(m, tlen);
#endif

	/* Make sure the board doesn't do a checksum */
	m->m_flags &= ~M_CKSUMMED;

#ifdef INET6
	if (vers == AF_INET) {
		ip4->ip_len = tlen;
		ip4->ip_ttl = tcp_ttl;
		(void) ip_output(m, (struct mbuf *)0, ro, 0,
			(struct mbuf *)NULL, ipsec);
	} else {
		ip6->ip6_nh = IPPROTO_TCP;
		ip6->ip6_len = tlen - iphlen;
		ip6->ip6_hlim = tcp_ttl;
		if (tp)
		    ip6->ip6_head = tp->t_inpcb->inp_oflowinfo | IPV6_VERSION;
		else
		    ip6->ip6_head = IPV6_VERSION;
		(void) ip6_output(m, (struct mbuf *)0, ro, 0,
			(struct ip_moptions *)NULL,
			tp != NULL ? tp->t_inpcb : NULL);
	}
#else
	((struct ip *)ti)->ip_len = tlen;
	((struct ip *)ti)->ip_ttl = tcp_ttl;
	(void) ip_output(m, (struct mbuf *)0, ro, 0,
		(struct mbuf *)NULL, ipsec);
#endif
}

/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.
 */
struct tcpcb *
tcp_newtcpcb(struct inpcb *inp)
{
	register struct tcpcb *tp = (struct tcpcb *)
		kmem_zone_zalloc(tcpcb_zone, curuthread ? KM_SLEEP :
			KM_NOSLEEP);
	if (tp == NULL) {
		m_mcbfail();
		return((struct tcpcb *)0);
	}

	TCP_UTRACE_TP(UTN('tcpn','ewtc'), tp, __return_address);
	m_pcbinc(sizeof(*tp), MT_PCB);
	TCP_NEXT_PUT((struct tcpiphdr *)tp, (struct tcpiphdr *)tp);
	TCP_PREV_PUT((struct tcpiphdr *)tp, (struct tcpiphdr *)tp);

#ifdef INET6
	TCP6_NEXT_PUT(tptolnk6(tp), tptolnk6(tp));
	TCP6_PREV_PUT(tptolnk6(tp), tptolnk6(tp));
#endif

	tp->t_maxseg = tcp_mssdflt;
	tp->t_maxseg0 = tcp_mssdflt;

	tp->t_flags = TF_AGGRESSIVE_ACK | TF_IMMEDIATE_ACK; /* sends options! */
	tp->t_inpcb = inp;
	/*
	 * Initialise nanosecond RTT measurement
	 */
	SKO_TIMESPEC(tp).tv_sec = 0;
	SKO_FMTHRESH(tp) = tcp_gofast;

	/*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = tcp_rttdflt * PR_SLOWHZ << 2;
	tp->t_rttmin = TCPTV_MIN;
	TCPT_RANGESET(tp->t_rxtcur, 
	    ((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1,
	    TCPTV_MIN, TCPTV_REXMTMAX);

	tp->snd_cwnd = TCP_MAXWIN<<TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN<<TCP_MAX_WINSHIFT;

	inp->inp_ip_ttl = tcp_ttl;
	inp->inp_ppcb = (caddr_t)tp;
	return (tp);
}

/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *
tcp_drop(register struct tcpcb *tp, int errno, struct ipsec *ipsec)
{
	struct socket *so = tp->t_inpcb->inp_socket;

	TCP_UTRACE_TP(UTN('tcp_','drop'), tp, __return_address);
	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
#ifdef INET6
		if (tp->t_inpcb->inp_flags & INP_COMPATV4)
			(void) tcp_output(tp, ipsec);
		else
			(void) tcp6_output(tp);
#else
		(void) tcp_output(tp, ipsec);
#endif
		TCPSTAT(tcps_drops);
	} else
		TCPSTAT(tcps_conndrops);
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(tp));
}

/*
 * Close a TCP control block:
 *	discard all space held by the tcp
 *	discard internet protocol block
 *	wake up any sleepers
 */
struct tcpcb *
tcp_close(register struct tcpcb *tp)
{
	register struct tcpiphdr *t;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
	register struct mbuf *m;
	register struct rtentry *rt;

	TCP_UTRACE_TP(UTN('tcp_','clos'), tp, __return_address);
	ASSERT(SOCKET_ISLOCKED(so));

	/*
	 * Ensure that no one can look us up again between now and the time
	 * that we are detaching.
	 */
	inp->inp_fport = 0;
	inp->inp_ppcb = 0;

	/*
	 * If we sent enough data to get some meaningful characteristics,
	 * save them in the routing entry.  'Enough' is arbitrarily 
	 * defined as the sendpipesize (default 4K) * 16.  This would
	 * give us 16 rtt samples assuming we only get one sample per
	 * window (the usual case on a long haul net).  16 samples is
	 * enough for the srtt filter to converge to within 5% of the correct
	 * value; fewer samples and we could save a very bogus rtt.
	 *
	 * Don't update the default route's characteristics and don't
	 * update anything that the user "locked".
	 */
	ROUTE_RDLOCK();
	if (SEQ_LT(tp->iss + so->so_snd.sb_hiwat * 16, tp->snd_max) &&
	    (rt = inp->inp_route.ro_rt) &&
#ifdef INET6
	    ((rt_key(rt)->sa_family == AF_INET &&
	    ((struct sockaddr_in *)rt_key(rt))->sin_addr.s_addr != INADDR_ANY)||
	    (rt_key(rt)->sa_family == AF_INET6 &&
	    !IS_ANYSOCKADDR(satosin6(rt_key(rt)))))) {
#else
	    ((struct sockaddr_in *)rt_key(rt))->sin_addr.s_addr != INADDR_ANY) {
#endif
		register __uint32_t i;

		if ((rt->rt_rmx.rmx_locks & RTV_RTT) == 0) {
			i = tp->t_srtt *
			    (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTT_SCALE));
			if (rt->rt_rmx.rmx_rtt && i)
				/*
				 * filter this update to half the old & half
				 * the new values, converting scale.
				 * See route.h and tcp_var.h for a
				 * description of the scaling constants.
				 */
				rt->rt_rmx.rmx_rtt =
				    (rt->rt_rmx.rmx_rtt + i) / 2;
			else
				rt->rt_rmx.rmx_rtt = i;
		}
		if ((rt->rt_rmx.rmx_locks & RTV_RTTVAR) == 0) {
			i = tp->t_rttvar *
			    (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTTVAR_SCALE));
			if (rt->rt_rmx.rmx_rttvar && i)
				rt->rt_rmx.rmx_rttvar =
				    (rt->rt_rmx.rmx_rttvar + i) / 2;
			else
				rt->rt_rmx.rmx_rttvar = i;
		}
		/*
		 * update the pipelimit (ssthresh) if it has been updated
		 * already or if a pipesize was specified & the threshhold
		 * got below half the pipesize.  I.e., wait for bad news
		 * before we start updating, then update on both good
		 * and bad news.
		 */
		if ((rt->rt_rmx.rmx_locks & RTV_SSTHRESH) == 0 &&
		    (i = tp->snd_ssthresh) && rt->rt_rmx.rmx_ssthresh ||
		    i < (rt->rt_rmx.rmx_sendpipe / 2)) {
			/*
			 * convert the limit from user data bytes to
			 * packets then to packet data bytes.
			 */
			i = (i + tp->t_maxseg / 2) / tp->t_maxseg;
			if (i < 2)
				i = 2;
#ifdef INET6
			if (inp->inp_flags & INP_COMPATV4)
				i *= (__uint32_t)(tp->t_maxseg +
				  sizeof (struct tcpiphdr));
			else
				i *= (__uint32_t)(tp->t_maxseg +
				  sizeof (struct tcpip6hdr));
#else
			i *= (__uint32_t)(tp->t_maxseg + sizeof (struct tcpiphdr));
#endif
			if (rt->rt_rmx.rmx_ssthresh)
				rt->rt_rmx.rmx_ssthresh =
				    (rt->rt_rmx.rmx_ssthresh + i) / 2;
			else
				rt->rt_rmx.rmx_ssthresh = i;
		}
	}
	ROUTE_UNLOCK();

	/* free the reassembly queue, if any */
#ifdef INET6
	if (inp->inp_flags & INP_COMPATV4) {
		t = TCP_NEXT_GET((struct tcpiphdr *)tp);
		while (t != (struct tcpiphdr *)tp) {
			t = TCP_NEXT_GET(t);
			m = REASS_MBUF_GET(TCP_PREV_GET(t));
			remque_tcb(TCP_PREV_GET(t));
			m_freem(m);
		}
	} else {
		register struct tcp6hdrs *t;
		t = TCP6_NEXT_GET(tptolnk6(tp));
		while (t != tptolnk6(tp)) {
			t = TCP6_NEXT_GET(t);
			m = REASS_MBUF6_GET(TCP6_PREV_GET(t));
			remque(TCP6_PREV_GET(t));
			m_freem(m);
		}
	}
#else
	t = TCP_NEXT_GET((struct tcpiphdr *)tp);
	while (t != (struct tcpiphdr *)tp) {
		t = TCP_NEXT_GET(t);
		m = REASS_MBUF_GET(TCP_PREV_GET(t));
		remque_tcb(TCP_PREV_GET(t));
		m_freem(m);
	}
#endif
	kmem_zone_free(tcpcb_zone, tp);
	m_pcbdec(sizeof (struct tcpcb), MT_PCB);
	soisdisconnected(so);

	/*
	 * Wait for sosend()/soreceive() to exit uiomove() critical section.
	 * This used to sleep in a loop, but on systems with only a single
	 * rtnetd, that could deadlock with a process hung waiting on NFS.
	 *
	 * So, now if anybody is in sosend() or soreceive(), they simply
	 * exit, ensuring that so_holds is decremented.  When soclose() is
	 * called, in_pcbdetach() will be called by tcp_usrreq() in response
	 * to the PRU_DETACH operation.
	 */
	so->so_state |= SS_CLOSING;
	if (so->so_holds == 0) {
		TCP_UTRACE_TP(UTN('tcpc','clo0'), tp, __return_address);
			TCP_UTRACE_PCB(UTN('tcpc','cl0i'), inp, __return_address);
		/*
		 * In the case of being called by soabort(), if the connection
		 * is not freed, unlock the socket; that is what callers of
		 * soabort() expect.
		 */
		if (!in_pcbdetach(inp) && so->so_error == ECONNABORTED) {
			TCP_UTRACE_TP(UTN('tcpc','clo1'), tp, __return_address);
			TCP_UTRACE_PCB(UTN('tcpc','cl1i'), inp, __return_address);
			SOCKET_UNLOCK(so);
		}
	} else {
		/*
		 * In order to ensure that no incoming segments find this
		 * PCB in its semi-dead state, remove it from the list of
		 * valid PCBs
		 */
		TCP_UTRACE_TP(UTN('tcpc','clo2'), tp, __return_address);
		TCP_UTRACE_PCB(UTN('tcpc','cl2i'), inp, __return_address);
		in_pcbunlink(inp);
		/*
		 * Leave socket locked for caller to unlock, unless it is
		 * soabort().
		 */
		if (so->so_error == ECONNABORTED) {
			TCP_UTRACE_TP(UTN('tcpc','clo3'), tp, __return_address);
			SOCKET_UNLOCK(so);
		}
		/*
		 * Note that we exit this section of code with the PCB
		 * reference count inflated.  The call to in_pcbdetach() when
		 * the deferred close happens will account for this.
		 */
	}
	TCPSTAT(tcps_closed);
	return ((struct tcpcb *)0);
}

void
tcp_drain(void)
{

}

/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 */
/*ARGSUSED*/
void
tcp_notify(register struct inpcb *inp, int error, void *data)
{
	struct tcpcb *tp;
	/* if the connection is already closed, don't bother */
	if ((tp = intotcpcb(inp)) == 0)
		return;

	TCP_UTRACE_TP(UTN('tcpn','not0'), tp, __return_address);
	/* ignore some errors if we are hooked up */
	if (intotcpcb(inp)->t_state == TCPS_ESTABLISHED &&
	    (error == EHOSTUNREACH || error == ENETUNREACH ||
	     error == EHOSTDOWN)) {
		return;
	}

	if (tp->t_state == TCPS_SYN_RECEIVED) {
		/* help stop SYN-bombing */
		tp->t_timer[TCPT_KEEP] = 1;
		return;
	}

	tp->t_softerror = error;
	wakeup((caddr_t) &inp->inp_socket->so_timeo);
	sorwakeup(inp->inp_socket, NETEVENT_TCPUP);
	sowwakeup(inp->inp_socket, NETEVENT_TCPUP);
}

void
tcp_ctlinput(cmd, sa, icp)
	int cmd;
	struct sockaddr *sa;
	struct icmp *icp;
{
	register struct tcphdr *th;
	struct in_addr zeroin_addr;
	extern u_char inetctlerrmap[];
	void tcp_quench(struct inpcb *, int, void *);
	void (*notify)(struct inpcb *, int, void *) = tcp_notify;
	void tcp_msgsize(struct inpcb *inp, int errno,
#ifdef INET6
		void *icp, struct ipsec *);
#else
		struct icmp *icp, struct ipsec *);
#endif
	register struct ip *ip = icp ? &icp->icmp_ip : NULL;
	struct in_addr src;
	register u_short dport = 0, sport = 0;

	zeroin_addr.s_addr = 0;

	if (cmd == PRC_QUENCH)
		notify = tcp_quench;
	else if ( cmd == PRC_MSGSIZE )
		notify = (void(*)(struct inpcb *, int, void *))tcp_msgsize;
	else if (!PRC_IS_REDIRECT(cmd) &&
		    ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0))
		return;
	if (ip) {
		/* Make sure the packet contains at least the ports. */
		th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		if (((caddr_t)th+sizeof(th->th_sport)+sizeof(th->th_dport)) <=
		    (caddr_t)ip + ip->ip_len) {
			sport = th->th_sport;
			dport = th->th_dport;
		}
		src = ip->ip_src;
	} else
		src = zeroin_addr;
	in_pcbnotify(&tcb, sa, dport, src, sport, cmd, notify, (void *) icp );
}

/*
 * When a source quench is received, close congestion window
 * to one segment.  We will gradually open it again as we proceed.
 */
/*ARGSUSED*/
void
tcp_quench(struct inpcb *inp, int flag, void *data)
{
	struct tcpcb *tp = intotcpcb(inp);

	if (tp)
		tp->snd_cwnd = tp->t_maxseg;
}


/*
 * Receive a message size error from a router.
 * This means that we're doing (RFC 1191) MTU discovery and our
 * mss is too big.  If we're lucky, the router will tell us
 * what the far side mtu size is.
 */
/* ARGSUSED */
void
tcp_msgsize(
	struct inpcb *inp,
	int     errno,
#ifdef INET6
	register void *icp,
#else
	register struct icmp *icp,
#endif
	struct ipsec *ipsec)
{
	int nextmtu, mss, tcp_optionslen;		/* MUST BE signed ! */
	struct tcpcb *tp = intotcpcb(inp);
	extern int tcp_mtutable[];
#ifdef INET6
	struct icmp *icp4;
	struct icmpv6 *icp6;
	int hsize, iplen;
#endif

	if ( ! tcp_mtudisc || ! tp || ! icp )
		return;		/* Must be a stale message */

	/* If we're using RFC 1323 time-stamps, adjust for them. */
	tcp_optionslen = (tp->t_flags&(TF_REQ_TSTMP|TF_RCVD_TSTMP)) ==
		(TF_REQ_TSTMP|TF_RCVD_TSTMP) ? TCPOLEN_TSTAMP_HDR : 0;

	/* Get next MTU from icmp message (RFC1191 extension to routers) */
#ifdef INET6
	if (inp->inp_flags & INP_COMPATV4) {
		icp4 = (struct icmp *)icp;
		hsize = sizeof(struct tcpiphdr);
		nextmtu = ntohs(icp4->icmp_nextmtu);
		iplen = icp4->icmp_ip.ip_len;
	} else {
		icp6 = (struct icmpv6 *)icp;
		hsize = sizeof(struct tcpip6hdr);
		nextmtu = ntohl(icp6->icmp6_pmtu);
		iplen = ntohs(icp6->icmp6_ip.ip6_len);
	}
#else
	nextmtu = ntohs( icp->icmp_nextmtu );
#endif

	/* If router doesn't give us an MTU value, then try the next
	 * smaller value in our table of typical MTU values.  This table
	 * is in master.d/bsd.
	 */
	if (nextmtu == 0 ) {
		register int i = 0;
#ifdef INET6
		int previous_len = iplen;
#else
		int previous_len = ntohs( icp->icmp_ip.ip_len );
#endif

		/* Correct for broken BSD based routers.  They return
		 * a total length field that is the total length plus
		 * the header length.  See Note in section 5 (page 8)
		 * of RFC 1191.
		 */
#ifdef INET6
		if (inp->inp_flags & INP_COMPATV4)
			if (previous_len > tp->t_maxseg+sizeof(struct tcpiphdr)+
			   tcp_optionslen)
				previous_len -= icp4->icmp_ip.ip_hl<<2;
#else
		if ( previous_len > tp->t_maxseg+sizeof(struct tcpiphdr)+
		   tcp_optionslen )
			previous_len -= icp->icmp_ip.ip_hl<<2;
#endif

		while ( previous_len <= tcp_mtutable[i] && tcp_mtutable[i] )
			i++;

		nextmtu = tcp_mtutable[i];	/* Could be zero. */
	}

#ifdef INET6
	if (nextmtu == 0 || nextmtu - hsize <= tcp_mssdflt)
		mss = tcp_mssdflt-tcp_optionslen;
	else
		mss = nextmtu - hsize - tcp_optionslen;
#else
	if (nextmtu==0 || nextmtu-sizeof(struct tcpiphdr) <= tcp_mssdflt)
		mss = tcp_mssdflt-tcp_optionslen;
	else
		mss = nextmtu-sizeof(struct tcpiphdr)-tcp_optionslen;
#endif
	
	/* straight out of tcp_mss()... */
#if (MCLBYTES & (MCLBYTES - 1)) == 0
	if (mss > MCLBYTES)
		mss &= ~(MCLBYTES-1);
#else
	if (mss > MCLBYTES)
		mss = mss / MCLBYTES * MCLBYTES;
#endif

	/* Only start retransmission if this lowers the mss.  Otherwise,
	 * it might be a duplicate message in which case we don't want
	 * to retransmit smaller segments.  If something is really wrong,
	 * the retransmission timer should clean things up.
	 */
	if ( mss < tp->t_maxseg ) {

#ifdef DEBUG
		static int tcp_mtudebug = 1;
		if ( tcp_mtudebug )
			printf( "tcp_msgsize: tp=%x,nextmtu=%d, IP.len=%d, oldmss=%d, newmss=%d\n",
#ifdef INET6
			(long) tp, nextmtu, iplen, tp->t_maxseg, mss);
#else
			(long) tp, nextmtu, icp->icmp_ip.ip_len, tp->t_maxseg,
			mss );
#endif
#endif /* DEBUG */


		tp->t_maxseg = mss;

		/* start slow-start phase again but don't
		 * change snd_ssthresh.
		 */
		tp->snd_cwnd = mss;

		/* Set up TCP timer to expire this smaller mtu in
		 * 5 minutes.
		 */
		tp->t_timer[TCPT_MTUEXP] = TCPTV_MTUEXP;

		/* Begin retransmission.  Reset retransmit timer and 
		 * turn off rtt measurement.
		 */
		tp->snd_nxt = tp->snd_una;
		SND_HIGH(tp) = tp->snd_una;
		tp->t_rtt = 0;
		tp->t_timer[TCPT_REXMT] = 0;
		tcp_output(tp, ipsec);
	}
}

void
insque_tcb(register struct tcpiphdr *ep, register struct tcpiphdr *pp)
{
#ifdef TCPPRINTFS
	if (tcpprintfs & 0x02)
		printf("tcp_reass, insert, ep=0x%x, pp=0x%x\n", ep, pp);
#endif
	TCP_NEXT_PUT(ep, TCP_NEXT_GET(pp));
	TCP_PREV_PUT(ep, pp);
	TCP_PREV_PUT(TCP_NEXT_GET(pp), ep);
	TCP_NEXT_PUT(pp, ep);
}

void
remque_tcb(register struct tcpiphdr *ep)
{
#ifdef TCPPRINTFS
	if (tcpprintfs & 0x02)
		printf("tcp_reass, remove, ep=0x%x\n", ep);
#endif
	TCP_NEXT_PUT(TCP_PREV_GET(ep), TCP_NEXT_GET(ep));
	TCP_PREV_PUT(TCP_NEXT_GET(ep), TCP_PREV_GET(ep));
}

#define SACK_CONDENSE(tp,loc,num) \
{ register j,k; \
  for (j = k = loc; k != ((loc)+(num)) % SACK_MAX; k = SACK_INCR(k)) { \
	while(SACK_FREE(SACK_RCV(tp,j)))	j = SACK_INCR(j);\
	if (k != j) SACK_RCV(tp,k) = SACK_RCV(tp,j);\
	j = SACK_INCR(j);\
  }\
}

#ifdef SACK_DEBUG
void
sack_dump(char *p, int len)
{
        int i;
        for (i=0; i<len; ) {
                printf("  %x%x%x%x ", (*p>>4)&0xf, 0xf & *p,
                                    (*(p+1))>>4&0xf, 0xf & *(p+1) );
                p+=2;
                i+=2;
                if ((i % 8) == 0 )
                        printf("\n");
        }
        printf("\n");
}

void
sack_rcvdump(struct tcpcb *tp, char *prc)
{
        printf("%s \ttcpcb: %x\n", prc, tp);
        printf("  sack_rcvcur: %d\n", SACK_RCVCUR(tp));
        printf("  sack_rcvnum: %d\n", SACK_RCVNUM(tp));
        sack_dump((char *)&SACK_RCV(tp,0), SACK_MAX * sizeof(struct sack_rcvblock));
}

void
sack_snddump(struct tcpcb *tp, char *prc)
{
        struct sack_sndhole *s;
        printf("%s \ttcpcb: %x\n", prc, tp);
        printf("  sack_sndmax: %x\n", SACK_SNDMAX(tp));
        printf("  sack_sndholes: %d\n", SACK_SNDHOLES(tp));
        s = &SACK_SND(tp);
        while (s && s->start != s->end) {
                printf("  start: %x  end: %x\n", s->start, s->end);
		ASSERT(s != s->next);
                s = s->next;
        }
}
#else
#define sack_dump(p,x)
#define sack_rcvdump(t,p)
#define sack_snddump(t,p)
#endif

/* Update data receivers SACK blocks
 * We don't need to keep a list of higher than acked data 
 * received, as we get this for free from the reassembly
 * queue.  Since network packet reodering is not uncommon,
 * optimize this code path for processing one out of order packet
 */
void
sack_rcvupdate(struct tcpcb *tp, tcp_seq start, tcp_seq end)
{
	register int i,j;
	int kept = 0;

	sack_rcvdump(tp, "sack_rcvupdate");

	if (start == end) {
		#pragma mips_frequency_hint NEVER
		SACK_PRINTF(("SACK_RCVUPDATE:!@#%@!%!@%@!@!%!@@!% BAD UPDATE\n"));
		return;
	}

	/* Erase any block info that is now obsolete */
	for (i = SACK_RCVCUR(tp), j=0; j < SACK_RCVNUM(tp); i=SACK_INCR(i), j++) {
		ASSERT(!SACK_FREE(SACK_RCV(tp,i))); 
		if (SEQ_GEQ(SACK_RCV(tp,i).start, start) &&
		    SEQ_LEQ(SACK_RCV(tp,i).end, end)) {
			SACK_PRINTF(("sack_rcvupdate : clearing start %x end %x\n", SACK_RCV(tp,i).start, SACK_RCV(tp,i).end));
			SACK_CLEAR(SACK_RCV(tp,i));
			continue;
		}
#ifdef SACK_DEBUG
		if (SEQ_GEQ(SACK_RCV(tp,i).start, start) &&
		    SEQ_GEQ(SACK_RCV(tp,i).end, end) &&
		    SEQ_LT(SACK_RCV(tp,i).start, end)) {
			printf("sack_rcvupdate: OVERLAP 1 IMPOSSIBLE\n");
			ASSERT(0);
		}
		if (SEQ_EQ(SACK_RCV(tp,i).start, end)) {
			printf("sack_rcvupdate: UNJOINED IMPOSSIBLE\n");
			ASSERT(0);
		}
		if (SEQ_LEQ(SACK_RCV(tp,i).start, start) &&
		    SEQ_LEQ(SACK_RCV(tp,i).end, end) &&
		    SEQ_GT(SACK_RCV(tp,i).end, start)) {
			printf("sack_rcvupdate: OVERLAPPING PARTIAL ACK IMPOSSIBLE\n");
			ASSERT(0);
		}
		if (SEQ_EQ(SACK_RCV(tp,i).end, start)) {
			printf("sack_rcvupdate: UNJOINED PRIOR PARTIAL ACK IMPOSSIBLE\n");
			ASSERT(0);
		}
		if (SEQ_LT(SACK_RCV(tp,i).start, start) &&
		    SEQ_GT(SACK_RCV(tp,i).end, end)) {
			printf("sack_rcvupdate: OVERLAP 3 IMPOSSIBLE\n");
			ASSERT(0);
		}
#endif
		kept++;
	}

	ASSERT(kept <= SACK_MAX);

	ASSERT(kept <= SACK_RCVNUM(tp));

	SACK_RCVNUM(tp) = (kept == SACK_MAX) ? SACK_MAX : kept+1;
	if (kept > 0) {
		#pragma mips_frequency_hint NEVER
		SACK_PRINTF(("sack_rcvupdate: start = %x end =%x kept == %d\n", start, end, kept));
		if (kept < SACK_MAX)
			SACK_CONDENSE(tp,SACK_RCVCUR(tp),kept);
		SACK_RCVCUR(tp) = SACK_DECR(SACK_RCVCUR(tp));
	}
	SACK_RCVCURBLK(tp).start = start;
	SACK_RCVCURBLK(tp).end = end;

	sack_rcvdump(tp, "sack_rcvupdate done");
}

/* Get rid of any sack data prior to this ack. 
 */
void
sack_rcvacked(struct tcpcb *tp, tcp_seq ack)
{
	register int i,j;

	sack_rcvdump(tp, "sack_rcvacked");
	
	/* Clear out any blocks sequenced prior to the ack */
	for (i = SACK_RCVCUR(tp), j = 0; j < SACK_RCVNUM(tp); i = SACK_INCR(i), j++) {
		if (SEQ_LT(SACK_RCV(tp,i).start,ack)) {
			SACK_CLEAR(SACK_RCV(tp,i));
			SACK_RCVNUM(tp)--;
		}
	}

	/* Now fix the current receive block if we messed with it */
	ASSERT(SACK_RCVNUM(tp) <= SACK_MAX);
	if (SACK_RCVNUM(tp)) {
		for (i = 0; i < SACK_MAX; i++) {
			if (!SACK_FREE(SACK_RCVCURBLK(tp)))
				break;
			SACK_RCVCUR(tp) = SACK_INCR(SACK_RCVCUR(tp));
		}
		SACK_CONDENSE(tp,SACK_RCVCUR(tp),SACK_RCVNUM(tp));
	}

	sack_rcvdump(tp, "sack_rcvacked done");

}

void
sack_rcvcleanup(struct tcpcb *tp)
{
	sack_rcvdump(tp, "sack_rcvupdate");
	SACK_RCVCUR(tp) = 0;
	SACK_RCVNUM(tp) = 0;
	bzero(&SACK_RCV(tp,0), sizeof(struct sack_rcvblock));
}

/* For the transmitting TCP, rather than track SACKs and merge them,
 * we track holes and shrink them.  We avoid dynamic allocation in the
 * common case of a single out of order packet, and when we do need to
 * allocate, we use a zone allocator because the size of the structures
 * frequency, and allocation pattern is such that it would probably cause
 * significant fragmentation 
 */  
void
sack_sndupdate(struct tcpcb *tp, tcp_seq start, tcp_seq end)
{
	struct sack_sndhole *s, *sprev;
	int obliterated_root = 0;

	SACK_PRINTF(("sack_sndupdate   start : %x  end : %x\n", start, end));

	/* Make sure it is a sane sequence */
	if (SEQ_LEQ(end, tp->snd_una) || SEQ_LEQ(end,start)) {
		#pragma mips_frequency_hint NEVER
		return;
	}

	/* If there are no holes, make the first */
	if (!SACK_SNDHOLES(tp)) {
		ASSERT(SACK_SND(tp).start == SACK_SND(tp).end);
		s = &SACK_SND(tp);
		s->start = tp->snd_una;
		s->end = start;
		s->next = NULL;
		SACK_SNDHOLES(tp)++;
		SACK_SNDMAX(tp) = end;
		goto done; 
	}

	s = sprev = &SACK_SND(tp);
	while (s) {
		/* Overlaps data from start */
		if (SEQ_LEQ(start, s->start) && SEQ_GT(end, s->start)) {
			/* Completely obliterated the hole */
			if (SEQ_GEQ(end, s->end)) {
				SACK_SNDHOLES(tp)--;
				sprev->next = s->next;
				if (s != &SACK_SND(tp))  
					kmem_zone_free(tcpsack_zone, s);
				else 
					obliterated_root = 1;
				s = sprev->next;
				continue;
			}
			/* Doesn't extend past this hole */
			s->start = end;
			goto out;
		}
		/* Doesn't touch any further data */
		else if (SEQ_LEQ(end, s->start)) {
			goto out;
		}
		/* Overlaps data from end */
		else if (SEQ_LT(start, s->end) && SEQ_GEQ(end, s->end)) {
			#pragma mips_frequency_hint NEVER
			s->end = start;
			sprev = s;
			s = s->next;
			continue;
		}
		/* Subset, splits current hole in two */
		else if (SEQ_GT(start, s->start) && SEQ_LT(end, s->end)) {
			#pragma mips_frequency_hint NEVER
			sprev = s;
			s = kmem_zone_alloc(tcpsack_zone, KM_NOSLEEP);
			if (!s || SACK_SNDHOLES(tp) == SACK_MAXHOLES) {
				#pragma mips_frequency_hint NEVER;
				goto done;
			}
			SACK_SNDHOLES(tp)++;
			s->next = sprev->next;
			s->start = end;
			s->end = sprev->end;
			sprev->next = s;
			sprev->end = start;
			/* Can't have affected any other holes, return */
			goto done;
		}
		sprev = s;	
		s = s->next;
	}

	/* We must have data past all the holes.  Maybe created a new one */  
	if (SEQ_GT(start, SACK_SNDMAX(tp))) {
		s = kmem_zone_alloc(tcpsack_zone, KM_NOSLEEP);
		if (!s || SACK_SNDHOLES(tp) == SACK_MAXHOLES) {
			#pragma mips_frequency_hint NEVER
			goto done;
		}
		SACK_SNDHOLES(tp)++;
		s->next = NULL;
		s->start = SACK_SNDMAX(tp);
		s->end = start;
		sprev->next = s;
	}	

	if (SEQ_GEQ(end, SACK_SNDMAX(tp)))
		SACK_SNDMAX(tp) = end;

out:	
	if (obliterated_root) {
		/* All holes removed */
		if (!SACK_SND(tp).next) {
			ASSERT(SACK_SNDHOLES(tp) == 0);
			SACK_SND(tp).start = SACK_SND(tp).end = 0;
			SACK_SND(tp).next = &SACK_SND(tp);
			tp->t_flags &= ~TF_SNDCHECK_SACK;
		}
		bcopy((void *)SACK_SND(tp).next,(void *)&SACK_SND(tp),sizeof(struct sack_sndhole)); 
	}

done:
	sack_snddump(tp, "sack_sndupdate done");
}

void sack_sndlist(struct tcpcb *tp, char *opt, int len)
{
	tcp_seq *o;
	
	sack_snddump(tp, "sack_sndlist");
	o = (tcp_seq *)opt;
	while (len) {
		sack_sndupdate(tp, ntohl(*o), ntohl(*(o+1)));
		o+=2;
		len -= 8; 
	}
	ASSERT(len == 0);
}

void sack_sndacked(struct tcpcb *tp, tcp_seq ack)
{
	if (SEQ_GT(ack, tp->snd_una))
		sack_sndupdate(tp, tp->snd_una, ack);
}

int sack_sndcheck(struct tcpcb *tp, tcp_seq start, tcp_seq end)
{
	struct sack_sndhole *s;

	sack_snddump(tp,"sack_sndcheck");
	ASSERT(SACK_SNDHOLES(tp));

	if (SEQ_GT(end, SACK_SNDMAX(tp)))
		return 0;

	s = &SACK_SND(tp);
	while (s) {
		if (SEQ_LEQ(end, s->start))
			return 1;
		if ((SEQ_LEQ(start, s->start) && SEQ_GT(end, s->start))
		  || (SEQ_LT(start, s->end) && SEQ_GEQ(end, s->end))
		  || (SEQ_GT(start, s->start) && SEQ_LEQ(end, s->end)))
			return 0;
		s = s->next;
	}
	return 1;
}

/* delete sack_hole list.
*/ 
void
sack_sndcleanup(struct tcpcb *tp)
{
	struct sack_sndhole *s, *t;

	sack_snddump(tp,"sack_sndcleanup");
	if (SACK_SND(tp).next != &SACK_SND(tp)) {
		s = SACK_SND(tp).next;
		while (s) {
			t = s->next;
			ASSERT(s != s->next);
			kmem_zone_free(tcpsack_zone, s);	
			s = t;
		}
		SACK_SND(tp).next = &SACK_SND(tp);
		SACK_SND(tp).start = SACK_SND(tp).end = 0;
		SACK_SNDHOLES(tp) = 0;
	}
	ASSERT(SACK_SNDHOLES(tp) == 0);
	ASSERT(SACK_SND(tp).start == SACK_SND(tp).end);
	tp->t_flags &= ~TF_SNDCHECK_SACK;
}

