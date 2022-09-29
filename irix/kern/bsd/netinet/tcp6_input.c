#ifdef INET6
/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994
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
 *	@(#)tcp_input.c	8.5 (Berkeley) 4/10/94
 */

#include <tcp-param.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/tcpipstats.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp6_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

extern	int tcprexmtthresh;

struct	tcp6hdrs tcp_savetr;

extern void tcp_canceltimers(struct tcpcb *);
extern void ip6_reachhint(struct inpcb *inp);
extern void tcp_dooptions(struct tcpcb *, struct mbuf *, struct tcphdr *,
  int *, u_int32_t *, u_int32_t *);
extern void tcp_xmit_timer(struct tcpcb *, short);
extern void tcp_pulloutofband(struct socket *so,
  struct tcphdr *th, struct mbuf *m);
extern void in_pcbrehash(struct inpcb *head, struct inpcb *inp);

static void
insque_tcb6(register struct tcp6hdrs *ep, register struct tcp6hdrs *pp)
{
	TCP6_NEXT_PUT(ep, TCP6_NEXT_GET(pp));
	TCP6_PREV_PUT(ep, pp);
	TCP6_PREV_PUT(TCP6_NEXT_GET(pp), ep);
	TCP6_NEXT_PUT(pp, ep);
}

static void
remque_tcb6(register struct tcp6hdrs *ep)
{
	TCP6_NEXT_PUT(TCP6_PREV_GET(ep),
	TCP6_NEXT_GET(ep));
	TCP6_PREV_PUT(TCP6_NEXT_GET(ep),
	TCP6_PREV_GET(ep));
}

/*
 * Insert segment tr into reassembly queue of tcp with
 * control block tp.  Return TH_FIN if reassembly now includes
 * a segment with FIN.  The macro form does the common case inline
 * (segment is the next to be received on an established connection,
 * and the queue is empty), avoiding linkage into and removal
 * from the queue and repetition of various conversions.
 * Set DELACK for segments received in order, but ack immediately
 * when segments are out of order (so fast retransmit can work).
 */
#define	TCP6_REASS(tp, tr, m, so, flags) { \
	if ((tr)->tr_seq == (tp)->rcv_nxt && \
	    TCP6_NEXT_GET(tptolnk6(tp)) == tptolnk6(tp) && \
	    (tp)->t_state == TCPS_ESTABLISHED) { \
		tp->t_flags |= TF_DELACK; \
		(tp)->rcv_nxt += (tr)->tr_len; \
		flags = (tr)->tr_flags & TH_FIN; \
		TCPSTAT(tcps_rcvpack);\
		TCPSTAT_ADD(tcps_rcvbyte, (tr)->tr_len);\
		ip6_reachhint(tp->t_inpcb); \
		ASSERT((so)->so_input); \
		(*(so)->so_input)((so), (m)); \
	} else { \
		(flags) = tcp6_reass((tp), (tr), (m)); \
		tp->t_flags |= TF_ACKNOW; \
	} \
}

int
tcp6_reass(tp, tr, m)
	register struct tcpcb *tp;
	register struct tcp6hdrs *tr;
	struct mbuf *m;
{
	register struct tcp6hdrs *q;
	struct socket *so = tp->t_inpcb->inp_socket;
	int flags;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_TCP1)
		printf("tcp6_reass(%p,%p,%p)\n", tp, tr, m);
#endif
	/*
	 * Call with tr==0 after become established to
	 * force pre-ESTABLISHED data up to user socket.
	 */
	if (tr == 0)
		goto present;

	/*
	 * Find a segment which begins after this one does.
	 */
	for (q = TCP6_NEXT_GET(tptolnk6(tp));
	  q != tptolnk6(tp); q = TCP6_NEXT_GET(q))
		if (SEQ_GT(q->tr_seq, tr->tr_seq))
			break;

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (TCP6_PREV_GET(q) != tptolnk6(tp)) {
		register int i;
		q = TCP6_PREV_GET(q);
		/* conversion to int (in i) handles seq wraparound */
		i = q->tr_seq + q->tr_len - tr->tr_seq;
		if (i > 0) {
			if (i >= tr->tr_len) {
				TCPSTAT(tcps_rcvduppack);
				TCPSTAT_ADD(tcps_rcvdupbyte, tr->tr_len);
				m_freem(m);
				/*
				 * Try to present any queued data
				 * at the left window edge to the user.
				 * This is needed after the 3-WHS
				 * completes.
				 */
				goto present;	/* ??? */
			}
			m_adj(m, i);
			tr->tr_len -= i;
			tr->tr_seq += i;
		}
		q = TCP6_NEXT_GET(q);
	}
	TCPSTAT(tcps_rcvoopack);
	TCPSTAT_ADD(tcps_rcvoobyte, tr->tr_len);
	REASS_MBUF6_PUT(tr, m);		/* XXX */

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	while (q != tptolnk6(tp)) {
		register int i = (tr->tr_seq + tr->tr_len) - q->tr_seq;
		if (i <= 0)
			break;
		if (i < q->tr_len) {
			q->tr_seq += i;
			q->tr_len -= i;
			m_adj(REASS_MBUF6_GET(q), i);
			break;
		}
		q = TCP6_NEXT_GET(q);
		m = REASS_MBUF6_GET(TCP6_PREV_GET(q));
		remque_tcb6(TCP6_PREV_GET(q));
		m_freem(m);
	}

	/*
	 * Stick new segment in its place.
	 */
	insque_tcb6(tr, TCP6_PREV_GET(q));

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (TCPS_HAVERCVDSYN(tp->t_state) == 0)
		return (0);
	tr = TCP6_NEXT_GET(tptolnk6(tp));
	if (tr == (struct tcp6hdrs *)tp || tr->tr_seq != tp->rcv_nxt)
		return (0);
	do {
		tp->rcv_nxt += tr->tr_len;
		flags = tr->tr_flags & TH_FIN;
		remque_tcb6(tr);
		m = REASS_MBUF6_GET(tr);
		tr = TCP6_NEXT_GET(tr);
		if (so->so_state & SS_CANTRCVMORE)
			m_freem(m);
		else {
			ASSERT(so->so_input);
			(*so->so_input)(so, m);
		}
	} while (tr != (struct tcp6hdrs *)tp && tr->tr_seq == tp->rcv_nxt);
	ip6_reachhint(tp->t_inpcb);
	return (flags);
}

/*
 * TCP input routine, follows pages 65-76 of the
 * protocol specification dated September, 1981 very closely.
 */
/*ARGSUSED*/
void
tcp6_input(m, ifp, ipsec, opts)
	register struct mbuf *m;
	struct ifnet *ifp;
	struct ipsec *ipsec;
	struct mbuf *opts;
{
	register struct tcp6hdrs *tr;
	register struct inpcb *inp;
	struct mbuf *om = 0;
	struct ip6ovck *ipc;
	int len, tlen, off;
	register struct tcpcb *tp = 0;
	register int trflags;
	struct socket *so = 0;
	int todrop, acked, ourfinisacked, needoutput = 0;
	u_int32_t iflowinfo, oflowinfo = 0;
	struct in6_addr laddr;
	int atype, flags, hlim;
	struct ip_soptions *ipsec_list;
	int dropsocket = 0;
	int iss = 0;
	u_int32_t ts_val, ts_ecr;
	u_long trwin;
	int ts_present = 0;
#ifdef DEBUG
	short ostate = 0;
#endif

	TCPSTAT(tcps_rcvtotal);

#ifdef IP6PRINTFS
	if (ip6printfs & D6_TCP0)
		printf("tcp6_input(%p,%p)\n", m, opts);
#endif
	/*
	 * Get IPv6 and TCP header together in first mbuf.
	 * Note: IPv6 leaves IPv6 header in first mbuf.
	 * add place for queue links.
	 */
	tr = mtod(m, struct tcp6hdrs *);
	if (m->m_len < sizeof (struct tcp6hdrs)) {
		if ((m = m_pullup(m, sizeof (struct tcp6hdrs))) == 0) {
			TCPSTAT(tcps_rcvshort);
			if (opts)
				m_freem(opts);
			return;
		}
#ifdef IP6PRINTFS
		if (ip6printfs & D6_TCP1)
			printf("tcp6_input pullup %p for %d\n",
			       m, sizeof (struct tcp6hdrs));
#endif
		tr = mtod(m, struct tcp6hdrs *);
	}

	/*
	 * Checksum extended TCP header and data.
	 */
	iflowinfo = tr->tr_head & IPV6_FLOWINFO_PRIFLOW;
	tlen = tr->tr_len;
	len = tlen + sizeof(struct tcp6hdrs) - sizeof(struct tcphdr);
	hlim = tr->tr_hlim;
	ipc = (struct ip6ovck *)&tr->tr_i6;
	ipc->ih6_wrd1 = ipc->ih6_wrd0 = 0;
	ipc->ih6_pr = IPPROTO_TCP;
	ipc->ih6_len = htons(tlen);
	if (tr->tr_sum = in_cksum(m, len)) {
		TCPSTAT(tcps_rcvbadsum);
		if (opts)
			m_freem(opts);
		goto drop;
	}

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = tr->tr_off << 2;
	if (off < sizeof (struct tcphdr) || off > tlen) {
		TCPSTAT(tcps_rcvbadoff);
		if (opts)
			m_freem(opts);
		goto drop;
	}
	tlen -= off;
	tr->tr_len = tlen;
	off -= sizeof (struct tcphdr);
	if (off > 0) {
		len = off + sizeof(struct tcp6hdrs);
		if (m->m_len < len) {
			m = m_pullup(m, len);
			if (m == 0) {
				TCPSTAT(tcps_rcvshort);
				if (opts)
					m_freem(opts);
				return;
			}
#ifdef IP6PRINTFS
			if (ip6printfs & D6_TCP1)
				printf("tcp6_input pullup %p for %d\n",
				       m, len);
#endif
			tr = mtod(m, struct tcp6hdrs *);
		}
		om = m_get(M_DONTWAIT, MT_DATA);
		if (om == NULL)
			goto drop;
		om->m_len = off;
		{ caddr_t op = mtod(m, caddr_t) + sizeof (struct tcpip6hdr);
		  bcopy(op, mtod(om, caddr_t), (unsigned)om->m_len);
		}
	}
	trflags = tr->tr_flags;

	/*
	 * Convert TCP protocol specific fields to host format.
	 */
	NTOHL(tr->tr_seq);
	NTOHL(tr->tr_ack);
	NTOHS(tr->tr_win);
	NTOHS(tr->tr_urp);

	/*
	 * Drop TCP, IP headers and TCP options.
	 */
	m->m_off += sizeof(struct tcp6hdrs) + off;
	m->m_len  -= sizeof(struct tcp6hdrs) + off;

	/*
	 * Locate pcb for segment.
	 */
findpcb:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_TCP0)
		printf("tcp6_input findpcb src %s/%d dst %s/%d len %d\n",
		       ip6_sprintf(&tr->tr_src),
		       ntohs(tr->tr_sport),
		       ip6_sprintf(&tr->tr_dst),
		       ntohs(tr->tr_dport),
		       tr->tr_len);
#endif
	/*
	 * First look for an exact match.
	 */
	inp = in_pcblookupx(&tcb, &tr->tr_src, tr->tr_sport,
	    &tr->tr_dst, tr->tr_dport, INPLOOKUP_ALL, AF_INET6);
	/*
	 * ...and if that fails, do a wildcard search.
	 */
	if (inp == NULL) {
		inp = in_pcblookupx(&tcb, &tr->tr_src, tr->tr_sport,
		    &tr->tr_dst, tr->tr_dport, INPLOOKUP_WILDCARD, AF_INET6);
	}

	/*
	 * If the state is CLOSED (i.e., TCB does not exist) then
	 * all data in the incoming segment is discarded.
	 * If the TCB exists but is in CLOSED state, it is embryonic,
	 * but should either do a listen or a connect soon.
	 */
	if (inp == NULL) {
		if (opts)
			m_freem(opts);
#ifdef IP6PRINTFS
		if (ip6printfs & D6_TCP0)
			printf("tcp6_input no inpcb\n");
#endif
		goto dropwithreset;
	}
	if ((inp->inp_flags & INP_NEEDAUTH) &&
	    ((m->m_flags & M_AUTH) == 0 ||
	     !ipsec_match(inp, m, opts, INP_NEEDAUTH))) {
		if (opts)
			m_freem(opts);
#ifdef IP6PRINTFS
		if (ip6printfs & D6_TCP0)
			printf("tcp6_input no AH\n");
#endif
		goto dropwithreset;
	}
	if ((inp->inp_flags & INP_NEEDCRYPT) &&
	    ((m->m_flags & M_CRYPT) == 0 ||
	     !ipsec_match(inp, m, opts, INP_NEEDCRYPT))) {
		if (opts)
			m_freem(opts);
#ifdef IP6PRINTFS
		if (ip6printfs & D6_TCP0)
			printf("tcp6_input no ESP\n");
#endif
		goto dropwithreset;
	}
	so = inp->inp_socket;
	SOCKET_LOCK(so);
	ASSERT(!(so->so_state & SS_SOFREE));
	tp = intotcpcb(inp);
	if (tp == 0) {
		if (opts)
			m_freem(opts);
#ifdef IP6PRINTFS
	if (ip6printfs & D6_TCP0)
		printf("tcp6_input no tcpcb\n");
#endif
		goto dropwithreset;
	}
	if (tp->t_state == TCPS_CLOSED) {
		if (opts)
			m_freem(opts);
		goto drop;
	}

	/* Unscale the window into a 32-bit value. */
	if ((trflags & TH_SYN) == 0)
		trwin = tr->tr_win << tp->snd_scale;
	else
		trwin = tr->tr_win;

	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	if (so->so_options & (SO_DEBUG|SO_ACCEPTCONN)) {
		if (so->so_options & SO_DEBUG) {
#ifdef DEBUG
			ostate = tp->t_state;
#endif
			tcp_savetr = *tr;
		}
		if (so->so_options & SO_ACCEPTCONN) {
			if ((trflags & (TH_RST|TH_ACK|TH_SYN)) != TH_SYN) {
				/*
				 * Note: dropwithreset makes sure we don't
				 * send a RST in response to a RST.
				 */
				if (trflags & TH_ACK) {
					TCPSTAT(tcps_badsyn);
					goto dropwithreset;
				}
				goto drop;
			}


			/*
			 * If too many half-open connections, pick a
			 * random socket to drop to combat SYN bombing.
			 * Include the incoming SYN among those that get
			 * dropped, since when the random number is 0,
			 * no room is made in the queue and the incoming
			 * SYN will be dropped.
			 *
			 * Switch from drop-oldest to random-drop when
			 * the SYN rate would cause drop-oldest to kill
			 * good connections with RTT of 1 second or more.
			 *
			 * Follow Knuth's advice to use a standard, linear
			 * congruential PRNG and not rely on the least
			 * significant bits.  Assume so_qlimit will not
			 * exceed 64K.
			 */
			if (so->so_qlen + so->so_q0len > (3*so->so_qlimit)/2) {
				static int rnd;
				static time_t old_lbolt;
				static unsigned int cur_cnt, old_cnt;
				unsigned int j;
				struct socket *so2;

				/* update approximate bad SYN rate */
				if ((j = (lbolt-old_lbolt)) != 0) {
					old_lbolt = lbolt;
					old_cnt = cur_cnt/j;
					cur_cnt = 0;
				}

				j = so->so_q0len;
				if (++cur_cnt > j || old_cnt > j) {
					rnd = (314157*rnd + 66329) & 0xffff;
					j = ((j+1) * rnd) >> 16;
				} else {
					j = 1;
				}
				if (j != 0) {
					so2 = so->so_q0;
					while (--j != 0)
						so2 = so2->so_q0;
					if (so2 != so) {
						/*
						 * We can't depend on so_pcb
						 * being valid, so hold the
						 * socket instead.
						 */
						struct inpcb *inp2;
						SO_HOLD(so2);
						if (INPCB_RELE(inp) == 0) {
							SOCKET_UNLOCK(so);
						}
						SOCKET_LOCK(so2);
						inp2 = sotoinpcb(so2);
						if (inp2 && intotcpcb(inp2)) {
						  tcp_drop(intotcpcb(inp2),
						           ETIMEDOUT, 0);
						}
						if (!SO_RELE(so2)) {
						    SOCKET_UNLOCK(so2);
						}
						TCPSTAT(tcps_synpurge);
						goto findpcb;
					}
				}
			}

			so = sonewconn(so, 0);
			/*
			 * sonewconn() no longer unlocks head, so we
			 * do it here.
			 */
			{ struct socket *oso = inp->inp_socket;

				if (INPCB_RELE(inp) == 0) {
					SOCKET_UNLOCK(oso);
				}
			}
			flags = inp->inp_flags;
			inp = 0;
			if (so == 0) {
				TCPSTAT(tcps_listendrop);
				goto drop;
			}
			/*
			 * This is ugly, but ....
			 *
			 * Mark socket as temporary until we're
			 * committed to keeping it.  The code at
			 * ``drop'' and ``dropwithreset'' check the
			 * flag dropsocket to see if the temporary
			 * socket created here should be discarded.
			 * We mark the socket as discardable until
			 * we're committed to it below in TCPS_LISTEN.
			 */
			dropsocket++;
			inp = (struct inpcb *)so->so_pcb;
			INPCB_HOLD(inp);
			oflowinfo = inp->inp_oflowinfo;
			ipsec_list = &inp->inp_soptions;
			inp->inp_flags |= flags;
			inp->inp_flags &= ~INP_COMPATV4;
			COPY_ADDR6(tr->tr_dst, inp->inp_laddr6);
			inp->inp_latype = IPATYPE_IPV6;
			inp->inp_lport = tr->tr_dport;
			/*
			 * Don't bother calling in_pcbrehash() here.  We
			 * will call it as a result of the call to
			 * in6_pcbconnect below (in the LISTEN case).
			 */
			if (opts) {
				opts = ip6_dropoption(opts, IP6_NHDR_ESP);
				opts = ip6_dropoption(opts, IP6_NHDR_AUTH);
				opt6_reverse(&tr->tr_i6, opts);
			}
			inp->inp_options = opts;
			opts = (struct mbuf *)0;
			ipsec_duplist(ipsec_list, &inp->inp_soptions);
			tp = intotcpcb(inp);
			tp->t_state = TCPS_LISTEN;

#ifdef IP6PRINTFS
			if (ip6printfs & D6_TCP0)
				printf("tcp6_input newconn0\n");
#endif
			/* Compute proper scaling value from buffer space */
			while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
			   TCP_MAXWIN << tp->request_r_scale < so->so_rcv.sb_hiwat)
				tp->request_r_scale++;
		}
	}
	/* after the last place where opts are useful: */
	if ((so->so_options & SO_ACCEPTCONN) == 0) {
		if (opts)
			m_freem(opts);
	}
	inp->inp_rcvttl = hlim;
	inp->inp_rcvif = ifp;

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 */
	tp->t_idle = 0;
	if (tp->t_state >= TCPS_ESTABLISHED)
		tp->t_timer[TCPT_KEEP] = tcp_keepidle*PR_SLOWHZ;

	/*
	 * Process options if not in LISTEN state,
	 * else do it below (after getting remote address).
	 */
	if (om && tp->t_state != TCPS_LISTEN) {
		tcp_dooptions(tp, om, &tr->tr_ti6.ti6_t,
		  &ts_present, &ts_val, &ts_ecr);
		om = 0;
	}	

	/* 
	 * Header prediction: check for the two common cases
	 * of a uni-directional data xfer.  If the packet has
	 * no control flags, is in-sequence, the window didn't
	 * change and we're not retransmitting, it's a
	 * candidate.  If the length is zero and the ack moved
	 * forward, we're the sender side of the xfer.  Just
	 * free the data acked & wake any higher level process
	 * that was blocked waiting for space.  If the length
	 * is non-zero and the ack didn't move, we're the
	 * receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to
	 * the socket buffer and note that we need a delayed ack.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    (trflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
	    tr->tr_seq == tp->rcv_nxt &&
	    (!ts_present || !TSTMP_LT(ts_val, tp->ts_recent)) &&
	    trwin && trwin == tp->snd_wnd &&
	    tp->snd_nxt == tp->snd_max) {

		/* 
		 * If last ACK falls within this segment's sequence numbers,
		 * record the timestamp.
		 * NOTE that the test is modified according to the latest
		 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
		 */
		if (ts_present && SEQ_LEQ(tr->tr_seq, tp->last_ack_sent)) {
			tp->ts_recent_age = tcp_now;
			tp->ts_recent = ts_val;
		}

		if (tr->tr_len == 0) {
			if (SEQ_GT(tr->tr_ack, tp->snd_una) &&
			    SEQ_LEQ(tr->tr_ack, tp->snd_max) &&
			    tp->snd_cwnd >= tp->snd_wnd) {
				/*
				 * this is a pure ack for outstanding data.
				 */
				TCPSTAT(tcps_predack);
				if (ts_present)
					tcp_xmit_timer(tp,
					    tcp_now - ts_ecr + 1);
				else if (tp->t_rtt &&
					    SEQ_GT(tr->tr_ack, tp->t_rtseq))
					tcp_xmit_timer(tp, tp->t_rtt);
				acked = tr->tr_ack - tp->snd_una;
				TCPSTAT(tcps_rcvackpack);
				TCPSTAT_ADD(tcps_rcvackbyte, acked);
				sbdrop(&so->so_snd, acked);
				tp->snd_una = tr->tr_ack;
				m_freem(m);
				/* some progress has been done */
				ip6_reachhint(tp->t_inpcb);

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selwakeup/signal.  If data
				 * are ready to send, let tcp6_output
				 * decide between more output or persist.
				 */
				if (tp->snd_una == tp->snd_max)
					tp->t_timer[TCPT_REXMT] = 0;
				else if (tp->t_timer[TCPT_PERSIST] == 0)
					tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

				if (so->so_snd.sb_flags & SB_NOTIFY)
					sowwakeup(so, NETEVENT_TCPUP);
				if (so->so_snd.sb_cc)
					(void) tcp6_output(tp);
				{
				struct socket *oso = inp->inp_socket;
				if (!INPCB_RELE(inp) || oso != so)
					SOCKET_UNLOCK(so);
				}
				return;
			}
		} else if (tr->tr_ack == tp->snd_una &&
		    TCP6_NEXT_GET(tptolnk6(tp)) ==
		    (struct tcp6hdrs *)tp &&
		    tr->tr_len <= sbspace(&so->so_rcv)) {
			/*
			 * this is a pure, in-sequence data packet
			 * with nothing on the reassembly queue and
			 * we have enough buffer space to take it.
			 */
			TCPSTAT(tcps_preddat);
			tp->rcv_nxt += tr->tr_len;
			TCPSTAT(tcps_rcvpack);
			TCPSTAT_ADD(tcps_rcvbyte, tr->tr_len);
			/* some progress has been done */
			ip6_reachhint(tp->t_inpcb);
			/*
			 * Add data to socket buffer.
			 */
			(*so->so_input)(so, m);
			tp->t_flags |= TF_DELACK;
			{
			struct socket *oso = inp->inp_socket;
			if (!INPCB_RELE(inp) || oso != so)
				SOCKET_UNLOCK(so);
			}
			return;
		}
	}

	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	{ int win;

	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = MAX(win, (int)(tp->rcv_adv - tp->rcv_nxt));
	}

	switch (tp->t_state) {

	/*
	 * If the state is LISTEN then ignore segment if it contains an RST.
	 * If the segment contains an ACK then it is bad and send a RST.
	 * If it does not contain a SYN then it is not interesting; drop it.
	 * Don't bother responding if the destination was a broadcast.
	 * Otherwise initialize tp->rcv_nxt, and tp->irs, select an initial
	 * tp->iss, and send a segment:
	 *     <SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
	 * Also initialize tp->snd_nxt to tp->iss+1 and tp->snd_una to tp->iss.
	 * Fill in remote peer address fields if not previously specified.
	 * Enter SYN_RECEIVED state, and process any other fields of this
	 * segment in this state.
	 */
	case TCPS_LISTEN: {
		struct mbuf *am;
		register struct sockaddr_in6 *sin;

		if (trflags & TH_RST)
			goto drop;
		if (trflags & TH_ACK)
			goto dropwithreset;
		if ((trflags & TH_SYN) == 0)
			goto drop;
#ifdef IP6PRINTFS
		if (ip6printfs & D6_TCP0)
			printf("tcp6_input connecting\n");
#endif
		/*
		 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
		 */
		if (IS_MULTIADDR6(tr->tr_dst))
			goto drop;
		am = m_get(M_DONTWAIT, MT_SONAME);	/* XXX */
		if (am == NULL)
			goto drop;
		am->m_len = sizeof (struct sockaddr_in6);
		sin = mtod(am, struct sockaddr_in6 *);
		sin->sin6_family = AF_INET6;
		sin->sin6_len = sizeof(*sin);
		sin->sin6_port = tr->tr_sport;
		/* should randomize oflowinfo here !!!  */
		sin->sin6_flowinfo = oflowinfo;
		COPY_ADDR6(tr->tr_src, sin->sin6_addr);
		COPY_ADDR6(inp->inp_laddr6, laddr);
		atype = inp->inp_latype;
		if (atype == IPATYPE_UNBD) {
			COPY_ADDR6(tr->tr_dst, inp->inp_laddr6);
			inp->inp_latype = IPATYPE_IPV6;
		}
		if (in6_pcbconnect(inp, am)) {
			COPY_ADDR6(laddr, inp->inp_laddr6);
			inp->inp_latype = atype;
			(void) m_free(am);
#ifdef IP6PRINTFS
			if (ip6printfs & D6_TCP0)
				printf("tcp6_input connect failed\n");
#endif
			goto drop;
		}
		(void) m_free(am);
		inp->inp_flags &= ~INP_COMPATV4;
		inp->inp_iflowinfo = iflowinfo;
		tcp_template(tp);
		if (om) {
			tcp_dooptions(tp, om, &tr->tr_ti6.ti6_t,
			  &ts_present, &ts_val, &ts_ecr);
			om = 0;
		}
		if (iss)
			tp->iss = iss;
		else
			tp->iss = tcp_iss;
		tcp_iss += TCP_ISSINCR/2;
		tp->irs = tr->tr_seq;
		tcp_sendseqinit(tp);
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		tp->t_state = TCPS_SYN_RECEIVED;
		tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
		dropsocket = 0;		/* committed to socket */
		TCPSTAT(tcps_accepts);
		ip6_reachhint(inp);
		goto trimthenstep6;
		}

	/*
	 * If the state is SYN_RECEIVED:
	 *	do just the ack and RST checks from SYN_SENT state.
	 * If the state is SYN_SENT:
	 *	if seg contains an ACK, but not for our SYN, drop the input.
	 *	if seg contains a RST, then drop the connection.
	 *	if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment
	 *	initialize tp->rcv_nxt and tp->irs
	 *	if seg contains ack then advance tp->snd_una
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_RECEIVED:
	case TCPS_SYN_SENT:
		if ((trflags & TH_ACK) &&
		    (SEQ_LEQ(tr->tr_ack, tp->iss) ||
		     SEQ_GT(tr->tr_ack, tp->snd_max))) {
			goto dropwithreset;
		}
		if (trflags & TH_RST) {
			if (trflags & TH_ACK)
				tp = tcp_drop(tp, ECONNREFUSED, 0);
			goto drop;
		}
		if (tp->t_state == TCPS_SYN_RECEIVED)
			break;
		if ((trflags & TH_SYN) == 0)
			goto drop;
		tp->irs = tr->tr_seq;
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		if (trflags & TH_ACK) {
#ifdef IP6PRINTFS
			if (ip6printfs & D6_TCP0)
				printf("tcp6_input try connected\n");
#endif
			TCPSTAT(tcps_connects);
			soisconnected(so);
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}
			tp->rcv_adv += tp->rcv_wnd;
			tp->snd_una++;		/* SYN is acked */
			tp->t_state = TCPS_ESTABLISHED;
			tp->t_timer[TCPT_KEEP] = tcp_keepidle*PR_SLOWHZ;
		} else {
			tp->t_timer[TCPT_REXMT] = 0;
			tp->t_state = TCPS_SYN_RECEIVED;
		}

trimthenstep6:
		/*
		 * Advance tr->tr_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		tr->tr_seq++;
		if (tr->tr_len > tp->rcv_wnd) {
			todrop = tr->tr_len - tp->rcv_wnd;
			m_adj(m, -todrop);
			tr->tr_len = tp->rcv_wnd;
			trflags &= ~TH_FIN;
			TCPSTAT(tcps_rcvpackafterwin);
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		}
		tp->snd_wl1 = tr->tr_seq - 1;
		tp->rcv_up = tr->tr_seq;
		goto step6;
	}

	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check timestamp, if present.
	 * Then check the connection count, if present.
	 * Then check that at least some bytes of segment are within
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 * 
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if (ts_present && (trflags & TH_RST) == 0 &&
	    tp->ts_recent && TSTMP_LT(ts_val, tp->ts_recent)) {

		/* Check to see if ts_recent is over 24 days old.  */
		if ((int)(tcp_now - tp->ts_recent_age) > TCP_PAWS_IDLE) {
			/*
			 * Invalidate ts_recent.  If this segment updates
			 * ts_recent, the age will be reset later and ts_recent
			 * will get a valid value.  If it does not, setting
			 * ts_recent to zero will at least satisfy the
			 * requirement that zero be placed in the timestamp
			 * echo reply when ts_recent isn't valid.  The
			 * age isn't reset until we get a valid ts_recent
			 * because we don't want out-of-order segments to be
			 * dropped when ts_recent is old.
			 */
			tp->ts_recent = 0;
		} else {
			TCPSTAT(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, tr->tr_len);
			TCPSTAT(tcps_pawsdrop);
			goto dropafterack;
		}
	}

	todrop = tp->rcv_nxt - tr->tr_seq;
	if (todrop > 0) {
		if (trflags & TH_SYN) {
			trflags &= ~TH_SYN;
			tr->tr_seq++;
			if (tr->tr_urp > 1)
				tr->tr_urp--;
			else
				trflags &= ~TH_URG;
			todrop--;
		}
		/*
		 * Following if statement from Stevens, vol. 2, p. 960.
		 */
		if (todrop > tr->tr_len
		    || (todrop == tr->tr_len && (trflags & TH_FIN) == 0)) {
			/*
			 * Any valid FIN must be to the left of the window.
			 * At this point the FIN must be a duplicate or out
			 * of sequence; drop it.
			 */
			trflags &= ~TH_FIN;

			/*
			 * Send an ACK to resynchronize and drop any data.
			 * But keep on processing for RST or ACK.
			 */
			tp->t_flags |= TF_ACKNOW;
			todrop = tr->tr_len;
			TCPSTAT(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, todrop);
		} else {
			TCPSTAT(tcps_rcvpartduppack);
			TCPSTAT_ADD(tcps_rcvpartdupbyte, todrop);
		}
		m_adj(m, todrop);
		tr->tr_seq += todrop;
		tr->tr_len -= todrop;
		if (tr->tr_urp > todrop)
			tr->tr_urp -= todrop;
		else {
			trflags &= ~TH_URG;
			tr->tr_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
	if ((so->so_state & SS_NOFDREF) &&
	    tp->t_state > TCPS_CLOSE_WAIT && tr->tr_len) {
		tp = tcp_close(tp);
		TCPSTAT(tcps_rcvafterclose);
#ifdef IP6PRINTFS
		if (ip6printfs & D6_TCP0)
			printf("tcp6_input connection closed\n");
#endif
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (tr->tr_seq+tr->tr_len) - (tp->rcv_nxt+tp->rcv_wnd);
	if (todrop > 0) {
		TCPSTAT(tcps_rcvpackafterwin);
		if (todrop >= tr->tr_len) {
			TCPSTAT_ADD(tcps_rcvbyteafterwin, tr->tr_len);
			/*
			 * If a new connection request is received
			 * while in TIME_WAIT, drop the old connection
			 * and start over if the sequence numbers
			 * are above the previous ones.
			 */
			if (trflags & TH_SYN &&
			    tp->t_state == TCPS_TIME_WAIT &&
			    SEQ_GT(tr->tr_seq, tp->rcv_nxt)) {
				iss = tp->rcv_nxt + TCP_ISSINCR;
				tp = tcp_close(tp);
				{
				struct socket *oso = inp->inp_socket;
				if (!INPCB_RELE(inp) || oso != so)
					SOCKET_UNLOCK(so);
				}
				so = 0;
				goto findpcb;
			}
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && tr->tr_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				TCPSTAT(tcps_rcvwinprobe);
			} else
				goto dropafterack;
		} else
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		m_adj(m, -todrop);
		tr->tr_len -= todrop;
		trflags &= ~(TH_PUSH|TH_FIN);
	}

	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record its timestamp.
	 * NOTE that the test is modified according to the latest
	 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 */
	if (ts_present && SEQ_LEQ(tr->tr_seq, tp->last_ack_sent)) {
		tp->ts_recent_age = tcp_now;
		tp->ts_recent = ts_val;
	}

	/*
	 * If the RST bit is set examine the state:
	 *    SYN_RECEIVED STATE:
	 *	If passive open, return to LISTEN state.
	 *	If active open, inform user that connection was refused.
	 *    ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
	 *	Inform user that connection was reset, and close tcb.
	 *    CLOSING, LAST_ACK, TIME_WAIT STATES
	 *	Close the tcb.
	 */
	if (trflags&TH_RST)
#ifdef IP6PRINTFS
	{
	if (ip6printfs & D6_TCP0)		
		printf("tcp6_input received RST\n");
#endif
	switch (tp->t_state) {
	case TCPS_SYN_RECEIVED:
		so->so_error = ECONNREFUSED;
		goto close;

	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
		so->so_error = ECONNRESET;
	close:
		tp->t_state = TCPS_CLOSED;
		TCPSTAT(tcps_drops);
		tp = tcp_close(tp);
		goto drop;

	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:
		tp = tcp_close(tp);
		goto drop;
	}
#ifdef IP6PRINTFS
	}
#endif

	/*
	 * If a SYN is in the window, then this is an
	 * error and we send an RST and drop the connection.
	 */
	if (trflags & TH_SYN) {
		tp = tcp_drop(tp, ECONNRESET, 0);
		goto dropwithreset;
	}

	/*
	 * If the ACK bit is off we drop segment and return.
	 */
	if ((trflags & TH_ACK) == 0)
		goto drop;

#ifdef IP6PRINTFS
	if (ip6printfs & D6_TCP0)
		printf("tcp6_input received ACK\n");
#endif
	/*
	 * Ack processing.
	 */
	switch (tp->t_state) {

	/*
	 * In SYN_RECEIVED state if the ack ACKs our SYN then enter
	 * ESTABLISHED state and continue processing, otherwise
	 * send an RST.
	 */
	case TCPS_SYN_RECEIVED:
		if (SEQ_GT(tp->snd_una, tr->tr_ack) ||
		    SEQ_GT(tr->tr_ack, tp->snd_max))
			goto dropwithreset;
		TCPSTAT(tcps_connects);
		soisconnected(so);
		tp->t_state = TCPS_ESTABLISHED;
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
		}
		(void) tcp6_reass(tp, (struct tcp6hdrs *)0, (struct mbuf *)0);
		tp->snd_wl1 = tr->tr_seq - 1;
		/* fall into ... */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < tr->tr_ack <= tp->snd_max
	 * then advance tp->snd_una to tr->tr_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:

		if (SEQ_LEQ(tr->tr_ack, tp->snd_una)) {
			if (tr->tr_len == 0 && trwin == tp->snd_wnd) {
				TCPSTAT(tcps_rcvdupack);
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change), the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshhold of them, assume a packet
				 * has been dropped and retransmit it.
				 * Kludge snd_nxt & the congestion
				 * window so we send only this one
				 * packet.
				 *
				 * We know we're losing at the current
				 * window size so do congestion avoidance
				 * (set ssthresh to half the current window
				 * and pull our congestion window back to
				 * the new ssthresh).
				 *
				 * Dup acks mean that packets have left the
				 * network (they're now cached at the receiver)
				 * so bump cwnd by the amount in the receiver
				 * to keep a constant cwnd packets in the
				 * network.
				 */
				if (tp->t_timer[TCPT_REXMT] == 0 ||
				    tr->tr_ack != tp->snd_una)
					tp->t_dupacks = 0;
				else if (++tp->t_dupacks == tcprexmtthresh) {
					tcp_seq onxt = tp->snd_nxt;
					u_int win =
					    min(tp->snd_wnd, tp->snd_cwnd) / 2 /
						tp->t_maxseg;

					if (win < 2)
						win = 2;
					tp->snd_ssthresh = win * tp->t_maxseg;
					tp->t_timer[TCPT_REXMT] = 0;
					tp->t_rtt = 0;
					tp->snd_nxt = tr->tr_ack;
					tp->snd_cwnd = tp->t_maxseg;
					(void) tcp6_output(tp);
					tp->snd_cwnd = tp->snd_ssthresh +
					       tp->t_maxseg * tp->t_dupacks;
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;
				} else if (tp->t_dupacks > tcprexmtthresh) {
					tp->snd_cwnd += tp->t_maxseg;
					(void) tcp6_output(tp);
					goto drop;
				}
			} else
				tp->t_dupacks = 0;
			break;
		}
		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
		if (tp->t_dupacks > tcprexmtthresh &&
		    tp->snd_cwnd > tp->snd_ssthresh)
			tp->snd_cwnd = tp->snd_ssthresh;
		tp->t_dupacks = 0;
		if (SEQ_GT(tr->tr_ack, tp->snd_max)) {
			TCPSTAT(tcps_rcvacktoomuch);
			goto dropafterack;
		}
		acked = tr->tr_ack - tp->snd_una;
		TCPSTAT(tcps_rcvackpack);
		TCPSTAT_ADD(tcps_rcvackbyte, acked);
		if (acked > 0)
			ip6_reachhint(tp->t_inpcb);

		/*
		 * If we have a timestamp reply, update smoothed
		 * round trip time.  If no timestamp is present but
		 * transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 */
		if (ts_present)
			tcp_xmit_timer(tp, tcp_now - ts_ecr + 1);
		else if (tp->t_rtt && SEQ_GT(tr->tr_ack, tp->t_rtseq))
			tcp_xmit_timer(tp,tp->t_rtt);

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (tr->tr_ack == tp->snd_max) {
			tp->t_timer[TCPT_REXMT] = 0;
			needoutput = 1;
		} else if (tp->t_timer[TCPT_PERSIST] == 0)
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

		/*
		 * If no data (only SYN) was ACK'd,
		 *    skip rest of ACK processing.
		 */
		if (acked == 0)
			goto step6;

		/*
		 * When new data is acked, open the congestion window.
		 * If the window gives us less than ssthresh packets
		 * in flight, open exponentially (maxseg per packet).
		 * Otherwise open linearly: maxseg per window
		 * (maxseg^2 / cwnd per packet).
		 */
		{
		register u_int cw = tp->snd_cwnd;
		register u_int incr = tp->t_maxseg;

		if (cw > tp->snd_ssthresh)
			incr = incr * incr / cw;
		tp->snd_cwnd = min(cw + incr, TCP_MAXWIN<<tp->snd_scale);
		}
		if (acked > so->so_snd.sb_cc) {
			tp->snd_wnd -= so->so_snd.sb_cc;
			sbdrop(&so->so_snd, (int)so->so_snd.sb_cc);
			ourfinisacked = 1;
		} else {
			sbdrop(&so->so_snd, acked);
			tp->snd_wnd -= acked;
			ourfinisacked = 0;
		}
		if (so->so_snd.sb_flags & SB_NOTIFY)
			sowwakeup(so, NETEVENT_TCPUP);
		tp->snd_una = tr->tr_ack;
		if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;

		switch (tp->t_state) {

		/*
		 * In FIN_WAIT_1 STATE in addition to the processing
		 * for the ESTABLISHED state if our FIN is now acknowledged
		 * then enter FIN_WAIT_2.
		 */
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked) {
				/*
				 * If we can't receive any more
				 * data, then closing user can proceed.
				 * Starting the timer is contrary to the
				 * specification, but if we don't get a FIN
				 * we'll hang forever.
				 */
				if (so->so_state & SS_CANTRCVMORE) {
					soisdisconnected(so);
					tp->t_timer[TCPT_2MSL] = tcp_maxidle;
				}
				tp->t_state = TCPS_FIN_WAIT_2;
			}
			break;

	 	/*
		 * In CLOSING STATE in addition to the processing for
		 * the ESTABLISHED state if the ACK acknowledges our FIN
		 * then enter the TIME-WAIT state, otherwise ignore
		 * the segment.
		 */
		case TCPS_CLOSING:
			if (ourfinisacked) {
				tp->t_state = TCPS_TIME_WAIT;

				in_pcbmove(inp);

				tcp_canceltimers(tp);
				tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
				soisdisconnected(so);
			}
			break;

		/*
		 * In LAST_ACK, we may still be waiting for data to drain
		 * and/or to be acked, as well as for the ack of our FIN.
		 * If our FIN is now acknowledged, delete the TCB,
		 * enter the closed state and return.
		 */
		case TCPS_LAST_ACK:
			if (ourfinisacked) {
				tp = tcp_close(tp);
				goto drop;
			}
			break;

		/*
		 * In TIME_WAIT state the only thing that should arrive
		 * is a retransmission of the remote FIN.  Acknowledge
		 * it and restart the finack timer.
		 */
		case TCPS_TIME_WAIT:
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			goto dropafterack;
		}
	}

step6:
	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if ((trflags & TH_ACK) &&
	    (SEQ_LT(tp->snd_wl1, tr->tr_seq) ||
	    (tp->snd_wl1 == tr->tr_seq && (SEQ_LT(tp->snd_wl2, tr->tr_ack) ||
	     (tp->snd_wl2 == tr->tr_ack && trwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
		if (tr->tr_len == 0 &&
		    tp->snd_wl2 == tr->tr_ack && trwin > tp->snd_wnd)
			TCPSTAT(tcps_rcvwinupd);
		tp->snd_wnd = trwin;
		tp->snd_wl1 = tr->tr_seq;
		tp->snd_wl2 = tr->tr_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;
	}

	/*
	 * Process segments with URG.
	 */
	if ((trflags & TH_URG) && tr->tr_urp &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
#ifdef IP6PRINTFS
		if (ip6printfs & D6_TCP0)
			printf("tcp6_input received URG\n");
#endif
		/*
		 * If this segment advances the known urgent pointer,
		 * then mark the data stream.  This should not happen
		 * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
		 * a FIN has been received from the remote side.
		 * In these states we ignore the URG.
		 *
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section as the original
		 * spec states (in one of two places).
		 */
		if (SEQ_GT(tr->tr_seq+tr->tr_urp, tp->rcv_up)) {
			tp->rcv_up = tr->tr_seq + tr->tr_urp;
			so->so_oobmark = so->so_rcv.sb_cc +
			    (tp->rcv_up - tp->rcv_nxt) - 1;
			if (so->so_oobmark == 0)
				so->so_state |= SS_RCVATMARK;
			sohasoutofband(so);
			tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		}
		/*
		 * Remove out of band data so doesn't get presented to user.
		 * This can happen independent of advancing the URG pointer,
		 * but if two URG's are pending at once, some out-of-band
		 * data may creep in... ick.
		 */
		if (tr->tr_urp <= (u_long)tr->tr_len
#ifdef SO_OOBINLINE
		     && (so->so_options & SO_OOBINLINE) == 0
#endif
		     )
			tcp_pulloutofband(so, &tr->tr_ti6.ti6_t, m);
	} else
		/*
		 * If no out of band data is expected,
		 * pull receive urgent pointer along
		 * with the receive window.
		 */
		if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
			tp->rcv_up = tp->rcv_nxt;
	/*
	 * Process the segment text, merging it into the TCP sequencing queue,
	 * and arranging for acknowledgment of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data
	 * is presented to the user (this happens in tcp6_usrreq.c,
	 * case PRU_RCVD).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 */
	if ((tr->tr_len || (trflags&TH_FIN)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		TCP6_REASS(tp, tr, m, so, trflags);
		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 */
		len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
	} else {
		m_freem(m);
		trflags &= ~TH_FIN;
	}

#ifdef IP6PRINTFS
	if (ip6printfs & D6_TCP0)
		printf("tcp6_input received data\n");
#endif
	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.
	 */
	if (trflags & TH_FIN) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			socantrcvmore(so);
			/*
			 *  If connection is half-synchronized
			 *  (ie NEEDSYN flag on) then delay ACK,
			 *  so it may be piggybacked when SYN is sent.
			 *  Otherwise, since we received a FIN then no
			 *  more input can be expected, send ACK now.
			 */
			tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}
		switch (tp->t_state) {

	 	/*
		 * In SYN_RECEIVED and ESTABLISHED STATES
		 * enter the CLOSE_WAIT state.
		 */
		case TCPS_SYN_RECEIVED:
		case TCPS_ESTABLISHED:
			tp->t_state = TCPS_CLOSE_WAIT;
			break;

	 	/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			tp->t_state = TCPS_CLOSING;
			break;

	 	/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
			tp->t_state = TCPS_TIME_WAIT;

			in_pcbmove(inp);

			tcp_canceltimers(tp);
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			soisdisconnected(so);
			break;

		/*
		 * In TIME_WAIT state restart the 2 MSL time_wait timer.
		 */
		case TCPS_TIME_WAIT:
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			break;
		}
	}
#if DEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, &tcp_savetr, 0);
#endif

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW))
		(void) tcp6_output(tp);
	{
	struct socket *oso = inp->inp_socket;
	if (!INPCB_RELE(inp) || oso != so)
		SOCKET_UNLOCK(so);
	}
	return;

dropafterack:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_TCP0)
		printf("tcp6_input drops after ask\n");
#endif
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 */
	if (trflags & TH_RST)
		goto drop;
	m_freem(m);
	tp->t_flags |= TF_ACKNOW;
	(void) tcp6_output(tp);
	{
	struct socket *oso = inp->inp_socket;
	if (!INPCB_RELE(inp) || oso != so)
		SOCKET_UNLOCK(so);
	}
	return;

dropwithreset:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_TCP0)
		printf("tcp6_input drops with reset\n");
#endif
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 * Don't bother to respond if destination was multicast.
	 */
	if ((trflags & TH_RST) || IS_MULTIADDR6(tr->tr_dst))
		goto drop;
	if (trflags & TH_ACK)
		tcp_respond(tp, tr, m, (tcp_seq)0, tr->tr_ack, TH_RST, 0,
		  AF_INET6);
	else {
		if (trflags & TH_SYN)
			tr->tr_len++;
		tcp_respond(tp, tr, m, tr->tr_seq+tr->tr_len, (tcp_seq)0,
		    TH_RST|TH_ACK, 0, AF_INET6);
	}
	/* destroy temporarily created socket */
	if (inp) {
		struct socket *oso = inp->inp_socket;

		if (INPCB_RELE(inp) && oso == so)
			so = 0;
	}
	if (so) {
		if (dropsocket) {
			(void) soabort(so);
		} else {
			SOCKET_UNLOCK(so);
		}
	}
	return;

drop:
#ifdef IP6PRINTFS
	if (ip6printfs & D6_TCP0)
		printf("tcp6_input drops\n");
#endif
	/*
	 * Drop space held by incoming segment and return.
	 */
#if DEBUG
	if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, &tcp_savetr, 0);
#endif
	m_freem(m);
	/* destroy temporarily created socket */
	if (inp) {
		struct socket *oso = inp->inp_socket;

		if (INPCB_RELE(inp) && oso == so)
			so = 0;
	}
	if (so) {
		if (dropsocket) {
			(void) soabort(so);
		} else {
			SOCKET_UNLOCK(so);
		}
	}
	return;
}

/*
 * Determine the MSS option to send on an outgoing SYN.
 */
int
tcp6_mssopt(tp)
	struct tcpcb *tp;
{
	struct rtentry *rt;
	extern int tcp_mssdflt;

	rt = tcp6_rtlookup(tp->t_inpcb);
	if (rt == NULL) {
#ifdef IP6PRINTFS
		if (ip6printfs & D6_PMTU)
			printf("tcp6_mssopt no route -> %d\n", tcp_mssdflt);
#endif
		return tcp_mssdflt;
	}

	return rt->rt_ifp->if_mtu - sizeof(struct tcpip6hdr);
}
#endif /* INET6 */
