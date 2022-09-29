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
 *	@(#)tcp_input.c	7.25 (Berkeley) 6/30/90
 */

/*
 * SGI changes/additions lacking #ifdef sgi/#endif
 *	tcpprintfs, NETPAR, 2nd arg to sorwakeup/sowwakeup,
 *	DEBUG ifdef around tcp_trace, min/max changed to MIN/MAX.
 * 4.4BSD differences:
 *	2nd arg to sonewconn, iphlen arg to tcp_input, mbuf fields,
 *	SB_NOTIFY, sockaddr sa_len field, HTONx()/NTOHx()
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/errno.h"
#include "sys/tcpipstats.h"
#include "sys/sesmgr.h"
#include "net/if.h"
#include "net/route.h"


#ifdef TCPPRINTFS
extern int tcpprintfs;	/* defined in master.d/bsd */
#endif /* TCPPRINTFS */

extern int	tcp_winscale;	/* RFC 1323 (big windows) options control */
extern int	tcp_tsecho;

extern int tcp_mtudisc;
extern int tcp_2msl;
extern int tcpiss_md5;		/* RFC1948 randomize TCP ISS:  security fix */
extern int tcp_sack;


#include "in.h"
#include "in_systm.h"
#include "in_var.h"
#include "ip.h"
#include "in_pcb.h"
#include "ip_var.h"
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "tcpip_ovly.h"
#include "tcp_debug.h"

u_int32_t tcp_now;
extern int tcprexmtthresh;
#ifdef DEBUG
struct	tcpiphdr tcp_saveti;
#endif
struct	inpcb tcb;

/*
 * Forward references.
 */
#ifdef INET6
void tcp_dooptions(struct tcpcb *, struct mbuf *, struct tcphdr *,
		   int *, u_int32_t *, u_int32_t *);
void tcp_pulloutofband(struct socket *, struct tcphdr *, struct mbuf *);
#else
void tcp_dooptions(struct tcpcb *, struct mbuf *, struct tcpiphdr *,
		   int *, u_int32_t *, u_int32_t *);
void tcp_pulloutofband(struct socket *, struct tcpiphdr *, struct mbuf *);
#endif
int  tcp_reass(struct tcpcb *, struct tcpiphdr *, struct mbuf *);
void tcp_xmit_timer(struct tcpcb *, short);
void tcp_tw_restart(struct inpcb *);

extern void tcp_canceltimers(struct tcpcb *);

/*
 * Insert segment ti into reassembly queue of tcp with
 * control block tp.  Return TH_FIN if reassembly now includes
 * a segment with FIN.  The macro form does the common case inline
 * (segment is the next to be received on an established connection,
 * and the queue is empty), avoiding linkage into and removal
 * from the queue and repetition of various conversions.
 * Set DELACK for segments received in order, but ack immediately
 * when segments are out of order (so fast retransmit can work).
 */
#define	TCP_REASS(tp, ti, m, so, flags) { \
	if ((ti)->ti_seq == (tp)->rcv_nxt && \
	    TCP_NEXT_GET((struct tcpiphdr*)(tp)) == (struct tcpiphdr *)(tp) &&\
	    (tp)->t_state == TCPS_ESTABLISHED) { \
		(tp)->t_flags |= (tp->t_flags & TF_FASTACK) ? TF_ACKNOW : \
			TF_DELACK; \
		(tp)->rcv_nxt += (ti)->ti_len; \
		if (SACK_RCVNUM(tp)) \
			sack_rcvacked(tp, (tp)->rcv_nxt); \
		flags = (ti)->ti_flags & TH_FIN; \
		TCPSTAT(tcps_rcvpack);\
		TCPSTAT_ADD(tcps_rcvbyte, (ti)->ti_len);\
		NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL, NETEVENT_TCPUP, (ti)->ti_len, NETRES_NULL); \
		ASSERT((so)->so_input);	\
		(*(so)->so_input)((so), (m)); \
	} else { \
		(flags) = tcp_reass((tp), (ti), (m)); \
		(tp)->t_flags |= TF_ACKNOW; \
	} \
}

tcp_reass(
	register struct tcpcb *tp,
	register struct tcpiphdr *ti,
	struct mbuf *m)			/* mbuf containing *ti */
{
	register struct tcpiphdr *q;
	struct socket *so = tp->t_inpcb->inp_socket;
	int flags;
	tcp_seq contig_start, contig_end;

	/*
	 * Call with ti==0 after become established to
	 * force pre-ESTABLISHED data up to user socket.
	 */
	if (ti == 0 || m == 0) {
		ASSERT(ti == 0 && m == 0);
		goto present;
	}
	GOODMT(m->m_type);

	/*
	 * Find a segment which begins after this one does.
	 * SACK - also count continguous blocks before this
	 */
	contig_start = ti->ti_seq;
	contig_end = ti->ti_seq + ti->ti_len; 
	for (q = TCP_NEXT_GET((struct tcpiphdr*)tp) ; 
		q != (struct tcpiphdr *)tp; q = TCP_NEXT_GET(q)) {
		if (SEQ_GT(q->ti_seq, ti->ti_seq))
			break;
		if (q->ti_seq == contig_end) 
			contig_end += q->ti_len;
		else {
			contig_start = q->ti_seq;
			contig_end = q->ti_seq + q->ti_len;
		} 
	}

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 * SACK - also update contiguous block list if we are not
	 * adjacent to previous block
	 */
	if (TCP_PREV_GET(q) != (struct tcpiphdr *)tp) {
		register int i;
		q = TCP_PREV_GET(q);
		/* conversion to int (in i) handles seq wraparound */
		i = q->ti_seq + q->ti_len - ti->ti_seq;
		if (i > 0) {
			if (i >= ti->ti_len) {
				TCPSTAT(tcps_rcvduppack);
				TCPSTAT_ADD(tcps_rcvdupbyte, ti->ti_len);
				m_freem(m);
				/* If we are SACK permitted, at this
				 * point we can not return.  Instead,
				 * we must update the SACK options we
				 * will send with the ack or else a
				 * braindead transmitter may keep sending
				 * us this same duplicate packet
				 */ 
				if (tp->t_flags & TF_RCVD_SACK) {
					contig_end = q->ti_seq + q->ti_len;
					q = TCP_NEXT_GET(q);
					ti = NULL;
					goto sackwalk;
				}
				return (0);
			}
			m_adj(m, i);
			ti->ti_len -= i;
			ti->ti_seq += i;
		}
		if (i < 0)
			contig_start = ti->ti_seq;	
		q = TCP_NEXT_GET(q);
	}
	TCPSTAT(tcps_rcvoopack);
	TCPSTAT_ADD(tcps_rcvoobyte, ti->ti_len);

	/*
	 * tcp_reass(tp, ti) with non-null tp and ti destroys the following
	 * fields because of overlay.
	 * 	ti->ti_dst, ti->ti_src (for prev ptr)
	 *	ti->ti_sport, ti->ti_dport, ti->ti_ack (for REASS_MBUF pointer).
	 * It is called this way only once thur the TCP_REASS() macro.
	 * These fields are never used after TCP_REASS() is called. 
	 */

	/* store m in ti, destroying ti_sport, ti_dport and ti_ack */
	REASS_MBUF_PUT(ti, m);			/* XXX */

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 * SACK - extend current block with segments we overlap
	 */
	while (q != (struct tcpiphdr *)tp) {
		register int i = (ti->ti_seq + ti->ti_len) - q->ti_seq;
		if (i <= 0)
			break;
		if (i < q->ti_len) {
			q->ti_seq += i;
			q->ti_len -= i;
			m_adj(REASS_MBUF_GET(q), i);
			break;
		}
		q = TCP_NEXT_GET(q);
		m = REASS_MBUF_GET(TCP_PREV_GET(q));
		remque_tcb(TCP_PREV_GET(q));
		m_freem(m);
	}

	/*
	 * Stick new segment in its place, destroying ti->ti_dst, ti->ti_src.
	 */
	insque_tcb(ti, TCP_PREV_GET(q));
	contig_end = ti->ti_seq + ti->ti_len;

sackwalk:
	/*
	 * For SACK, now walk the list to find the end of our
	 * contiguous block.  If this block is going to be passed
	 * up to the socket, don't bother
	 */ 
	if (tp->t_flags & TF_RCVD_SACK && SEQ_GT(contig_start, tp->rcv_nxt)) {
		while ( q != (struct tcpiphdr *)tp) {
			if (q->ti_seq == contig_end) {
				contig_end = q->ti_seq + q->ti_len;
				q = TCP_NEXT_GET(q);
			}
			else
				break;
		}
		SACK_PRINTF(("tcp_reass: most recent block = %x %x\n", contig_start, contig_end));
		sack_rcvupdate(tp, contig_start, contig_end);
		return (0);
	}

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (TCPS_HAVERCVDSYN(tp->t_state) == 0)
		return (0);
	ti = TCP_NEXT_GET((struct tcpiphdr *)tp);
	if (ti == (struct tcpiphdr *)tp || ti->ti_seq != tp->rcv_nxt)
		return (0);
	if (tp->t_state == TCPS_SYN_RECEIVED && ti->ti_len)
		return (0);
	do {
		tp->rcv_nxt += ti->ti_len;
		flags = ti->ti_flags & TH_FIN;
		remque_tcb(ti);
		m = REASS_MBUF_GET(ti);
		ti = TCP_NEXT_GET(ti);
		if (so->so_state & SS_CANTRCVMORE) {
			NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
				 NETEVENT_TCPUP, ti->ti_len, NETRES_SBFULL);
			m_freem(m);
			/* We are renegging on a sacked packet, so invalidate
			 * our send SACK buffer */ 
			if (SACK_RCVNUM(tp))
				sack_rcvcleanup(tp);
		} else {
			ASSERT(so->so_input);
			(*so->so_input)(so, m);
			NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
				 NETEVENT_TCPUP, ti->ti_len, NETRES_NULL);
		}
	} while (ti != (struct tcpiphdr *)tp && ti->ti_seq == tp->rcv_nxt);

	/* Remove any SACK blocks before the ACK */ 
	if (SACK_RCVNUM(tp)) 
		sack_rcvacked(tp, tp->rcv_nxt);

	NETPAR(NETSCHED, NETWAKINGTKN, NETPID_NULL,
		 NETEVENT_TCPUP, NETCNT_NULL, NETRES_NULL);
	return (flags);
}

int tcp_wakethresh = NBPP/2;	/* should agree with sosend() */
#ifdef DEBUG
int tcp_skipped_wake = 0;	/* XXX */
#endif

/* ARGSUSED */
void
tcp_wakeup(struct socket *so, int event)
{
	if ((so->so_snd.sb_hiwat <= tcp_wakethresh) ||
	    (tcp_wakethresh == 0) ||
	    (tcp_wakethresh < sbspace(&so->so_snd))) {
		sowwakeup(so, event);
		return;
	}
#ifdef DEBUG
	atomicAddInt(&tcp_skipped_wake, 1);
#endif
}

/*
 * TCP input routine, follows pages 65-76 of the
 * protocol specification dated September, 1981 very closely.
 */
/*ARGSUSED*/
void
#ifdef INET6
tcp_input(struct mbuf *m, struct ifnet *ifp, struct ipsec *ipsec,
  struct mbuf *unused)
#else
tcp_input(struct mbuf *m, struct ifnet *ifp, struct ipsec *ipsec)
#endif
{
	register struct tcpiphdr *ti;
	struct inpcb *inp = 0;
	struct mbuf *om = 0;
	int len, tlen, off;
	register struct tcpcb *tp = 0;
	register int tiflags;
	struct socket *so = 0;
	int todrop, acked, ourfinisacked, needoutput = 0;
#ifdef DEBUG
	short ostate;
#endif /* DEBUG */
	struct in_addr laddr;
	int dropsocket = 0;
	int iss = 0;
	u_int32_t ts_val, ts_ecr;
	u_int tiwin;
	int ts_present = 0;

	NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
		 NETEVENT_TCPUP, NETCNT_NULL, NETRES_PROTCALL);
	TCPSTAT(tcps_rcvtotal);
	/*
	 * Get IP and TCP header together in first mbuf.
	 * Note: IP leaves IP header in first mbuf.
	 */
	ti = mtod(m, struct tcpiphdr *);
	if (((struct ip *)ti)->ip_hl > (sizeof (struct ip) >> 2))
		ip_stripoptions(m, (struct mbuf *)0);
	if (m->m_len < sizeof (struct tcpiphdr)) {
		if ((m = m_pullup(m, sizeof (struct tcpiphdr))) == 0) {
			#pragma mips_frequency_hint NEVER
			NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
				NETEVENT_TCPUP, m->m_len, NETRES_HEADER);
			TCPSTAT(tcps_rcvshort);
			_SESMGR_SOATTR_FREE(ipsec);
			return;
		}
		ti = mtod(m, struct tcpiphdr *);
	}

	/*
	 * Checksum extended TCP header and data.
	 */
	tlen = ((struct ip *)ti)->ip_len;
	if (!(m->m_flags & M_CKSUMMED)) {
		len = sizeof (struct ip) + tlen;
		ti->ti_next = ti->ti_prev = 0;
		ti->ti_x1 = 0;
		ti->ti_len = (u_short)tlen;
		HTONS(ti->ti_len);
		if (in_cksum(m, len)) {
			#pragma mips_frequency_hint NEVER
			NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
				NETEVENT_TCPUP, tlen, NETRES_CKSUM);
#ifdef TCPPRINTFS
			if (tcpprintfs)
				printf("tcp bad sum: src %x\n",
				       ti->ti_src.s_addr);
#endif
			TCPSTAT(tcps_rcvbadsum);
			goto drop;
		}
	}

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = ti->ti_off << 2;
	if (off < sizeof (struct tcphdr) || off > tlen) {
		#pragma mips_frequency_hint NEVER
#ifdef TCPPRINTFS
		if (tcpprintfs)
			printf("tcp off: src %x off %d\n",
				ti->ti_src.s_addr, off);
#endif
		NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
			 NETEVENT_TCPUP, tlen, NETRES_OFFSET);
		TCPSTAT(tcps_rcvbadoff);
		goto drop;
	}
	tlen -= off;
	ti->ti_len = tlen;
getopts:
	if (off > sizeof (struct tcphdr)) {
		u_int32_t *opl;
		if (m->m_len < sizeof(struct ip) + off) {
			if ((m = m_pullup(m, sizeof (struct ip) + off)) == 0) {
				#pragma mips_frequency_hint NEVER
				NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
					NETEVENT_TCPUP, tlen, NETRES_SIZE);
				TCPSTAT(tcps_rcvshort);
				_SESMGR_SOATTR_FREE(ipsec);
				return;
			}
			ti = mtod(m, struct tcpiphdr *);
		}
		/* Do quick retrieval of timestamp options ("options
		 * prediction"):  If timestamp is only option and it's
		 * formatted as recommended in RFC 1323 Appendix A,
		 * we can get the values now and not bother allocating
		 * an mbuf, copying options, etc.
		 */
		if ( off == sizeof(struct tcphdr)+TCPOLEN_TSTAMP_HDR &&
			!(ti->ti_flags & TH_SYN) && *(opl=mtod(m, u_int32_t *)
			+ sizeof(struct tcpiphdr)/sizeof(u_int32_t) ) ==
			htonl( TCPOPT_TSTAMP_HDR ) ) {
			
			ts_present = 1;
			ts_val = ntohl( opl[1] );
			ts_ecr = ntohl( opl[2] );
		}
		else {
			om = m_get(M_DONTWAIT, MT_DATA);
			if (om == 0)
			{
				#pragma mips_frequency_hint NEVER
				NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
					 NETEVENT_TCPUP, tlen, NETRES_MBUF);
				goto drop;
			}
			om->m_len = off - sizeof (struct tcphdr);
			{ caddr_t op=mtod(m, caddr_t)+sizeof (struct tcpiphdr);
			  bcopy(op, mtod(om, caddr_t), (unsigned)om->m_len);
			}
		}
	}
	tiflags = ti->ti_flags;

	/*
	 * Convert TCP protocol specific fields to host format.
	 */
	NTOHL(ti->ti_seq);
	NTOHL(ti->ti_ack);
	NTOHS(ti->ti_win);
	NTOHS(ti->ti_urp);

	/*
	 * Locate pcb for segment.
	 */
findpcb:
	inp = 0;
	tp = 0;
	so = 0;

	if ((tiflags & (TH_SYN|TH_ACK)) == TH_SYN) {
			/*
			 * It would be ideal if we only had to scan
			 * listening PCBs when a SYN comes in but we
			 * might need to clear one in TIME-WAIT, so check
			 * the matching bucket first, and then look for
			 * listeners if nothing seems to be present.
			 */
#ifdef INET6
			inp = in_pcblookupx(&tcb, &ti->ti_src, ti->ti_sport,
			    &ti->ti_dst, ti->ti_dport, INPLOOKUP_ALL, AF_INET);
#else
			inp = in_pcblookupx(&tcb, ti->ti_src, ti->ti_sport,
			    ti->ti_dst, ti->ti_dport, INPLOOKUP_ALL);
#endif
			if (inp == 0) {
#ifdef INET6
				inp = in_pcblookupx(&tcb, &ti->ti_src, 
					ti->ti_sport, &ti->ti_dst, 
					ti->ti_dport, 
					(INPLOOKUP_LISTEN|INPLOOKUP_WILDCARD),
					AF_INET);
#else
				inp = in_pcblookupx(&tcb, ti->ti_src, 
					ti->ti_sport, ti->ti_dst, 
					ti->ti_dport, 
					(INPLOOKUP_LISTEN|INPLOOKUP_WILDCARD));
#endif

			}
	} else {
			/*
			 * Non-SYN lookup is fully specified so no wildcard
			 * needed.  Just hit the hash bucket and get the PCB.
			 */
#ifdef INET6
			inp = in_pcblookupx(&tcb, &ti->ti_src, ti->ti_sport,
			    &ti->ti_dst, ti->ti_dport, INPLOOKUP_ALL,
			    AF_INET);
#else
			inp = in_pcblookupx(&tcb, ti->ti_src, ti->ti_sport,
			    ti->ti_dst, ti->ti_dport, INPLOOKUP_ALL);
#endif

	}

	/*
	 * If the state is CLOSED (i.e., TCB does not exist) then
	 * all data in the incoming segment is discarded.
	 * If the TCB exists but is in CLOSED state, it is embryonic,
	 * but should either do a listen or a connect soon.
	 */
	if (inp == 0)
		goto dropwithreset;
	TCP_UTRACE_PCB(UTN('tcpi','npip'), inp, __return_address);
	so = inp->inp_socket;
	TCP_UTRACE_SO(UTN('tcpi','npso'), so, __return_address);
	SOCKET_LOCK(so);
	ASSERT(!(so->so_state & SS_SOFREE));
	tp = intotcpcb(inp);
	TCP_UTRACE_TP(UTN('tcpi','nptp'), tp, __return_address);
	if (tp == 0) {
		goto dropwithreset;
	}
	if (tp->t_state == TCPS_CLOSED)
		goto drop;

	/* Unscale the window into a 32-bit value.
	 */
	if (tiflags & TH_SYN)
		tiwin = ti->ti_win;
	else
		tiwin = ti->ti_win << tp->snd_scale;
	ASSERT(SOCKET_ISLOCKED(inp->inp_socket));
	if (so->so_options & (SO_DEBUG|SO_ACCEPTCONN)) {
		int tp_flags = tp->t_flags & TF_COPIED_OPTS;
#ifdef DEBUG
		if (so->so_options & SO_DEBUG) {
			ostate = tp->t_state;
			tcp_saveti = *ti;
		}
#endif
		if (so->so_options & SO_ACCEPTCONN) {
			if ((tiflags & (TH_RST|TH_ACK|TH_SYN)) != TH_SYN) {
				/*
				 * Note: dropwithreset makes sure we don't
				 * send a reset in response to a RST.
				 */
				if (tiflags & TH_ACK) {
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
				#pragma mips_frequency_hint NEVER
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
						           ETIMEDOUT, ipsec);
						}
						if (!SO_RELE(so2)) {
						    SOCKET_UNLOCK(so2);
						}
						TCPSTAT(tcps_synpurge);
						goto findpcb;
					}
				}
			}

			so = sonewconn(so, ipsec);
			/*
			 * sonewconn() no longer unlocks head, so we
			 * do it here.
			 */
			{ struct socket *oso = inp->inp_socket;

				if (INPCB_RELE(inp) == 0) {
					SOCKET_UNLOCK(oso);
				}
			}
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
			/*
			 * We don't reshuffle in the pcb hash because it
			 * will either be accepted or rejected below.
			 */
			INPCB_HOLD(inp);
			inp->inp_laddr = ti->ti_dst;
#ifdef INET6
			inp->inp_flags &= ~INP_COMPATV6;
			inp->inp_laddr6.s6_addr32[0] = 0;
			inp->inp_laddr6.s6_addr32[1] = 0;
			inp->inp_laddr6.s6_addr32[2] = htonl(0xffff);
			inp->inp_latype = IPATYPE_IPV4;
#endif
			inp->inp_lport = ti->ti_dport;
			tp = intotcpcb(inp);
			tp->t_state = TCPS_LISTEN;
			tp->t_flags |= tp_flags;
		}
	}

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 */
	tp->t_idle = 0;
	tp->t_timer[TCPT_KEEP] = (tp->t_state >= TCPS_ESTABLISHED) ?
		(tcp_keepidle*PR_SLOWHZ) : TCPTV_KEEP_INIT;

	/*
	 * Process options if not in LISTEN state,
	 * else do it below (after getting remote address).
	 */
	if (om && tp->t_state != TCPS_LISTEN) {
#ifdef INET6
		tcp_dooptions(tp, om, &ti->ti_t,
		  &ts_present, &ts_val, &ts_ecr );
#else
		tcp_dooptions(tp, om, ti, &ts_present, &ts_val, &ts_ecr );
#endif
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
	    (tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
	    ti->ti_seq == tp->rcv_nxt &&
	    ( ! ts_present || ! TSTMP_LT( ts_val, tp->ts_recent ) ) &&
	    tiwin && tiwin == tp->snd_wnd &&
	    tp->snd_nxt == tp->snd_max) {
		if (ti->ti_len == 0) {
			if (SEQ_GT(ti->ti_ack, tp->snd_una) &&
			    SEQ_LEQ(ti->ti_ack, tp->snd_max) &&
			    tp->snd_cwnd >= tp->snd_wnd
			    /*
			     * SCA -- added based on Brakmo/Peterson
			     * Fixes performance problem with long timeouts;
			     * ensures that this ACK will be processed below
			     * where it belongs.
			     */
			    && (tp->t_dupacks < tcprexmtthresh)) {
				/*
				 * this is a pure ack for outstanding data.
				 */
				TCPSTAT(tcps_predack);
				if (tp->t_rtt && SEQ_GT(ti->ti_ack,tp->t_rtseq))
				{
				   if (ts_present) {
					/* If a time-stamp is available, use
					 * it.  Afterwards, keep rtt "timer"
					 * going.
					 */
					tcp_xmit_timer(tp, tcp_now-ts_ecr+1 );
					tp->t_rtt = 1;
					tp->t_rtseq = ti->ti_ack;
				   }
				   else
					tcp_xmit_timer(tp,tp->t_rtt);
				}
				acked = ti->ti_ack - tp->snd_una;
				if (SACK_SNDHOLES(tp)) {
					#pragma mips_frequency_hint NEVER
					sack_sndacked(tp, ti->ti_ack);
				}
				TCPSTAT(tcps_rcvackpack);
				TCPSTAT_ADD(tcps_rcvackbyte, acked);
				sbdrop(&so->so_snd, acked);
				tp->snd_una = ti->ti_ack;
				SND_HIGH(tp) = ti->ti_ack;

				/* fix for 766435; fast path header prediction
				   logic was not updating window information
				   (see "step6:" label) causing potential 
				   problems if two consecutive pure window 
				   updates were received in the wrong order; 
				   since we know snd_wnd == tiwin and
				   ti->ti_ack > tp->snd_una and 
				   ti->ti_seq == tp->rcv_nxt, the test has
				   been optimized away ...                  */
				tp->snd_wl1 = ti->ti_seq;
				tp->snd_wl2 = ti->ti_ack;

				m_freem(m);

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selwakeup/signal.  If data
				 * are ready to send, let tcp_output
				 * decide between more output or persist.
				 */
				if (tp->snd_una == tp->snd_max) 
					tp->t_timer[TCPT_REXMT] = 0;
				else if (tp->t_timer[TCPT_PERSIST] == 0)
					tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

				if (acked && (so->so_rcv.sb_flags & SB_INPUT_ACK)) {
					ASSERT(so->so_input);
					(*so->so_input)(so, NULL);
				}
				if (so->so_snd.sb_flags & SB_NOTIFY)
					tcp_wakeup(so, NETEVENT_TCPUP);
				if (so->so_snd.sb_cc)
					(void) tcp_output(tp, ipsec);
				{
				struct socket *oso = inp->inp_socket;
				if (!INPCB_RELE(inp) || oso != so)
					SOCKET_UNLOCK(so);
				}
				_SESMGR_SOATTR_FREE(ipsec);
				return;
			}
		} else if (ti->ti_ack == tp->snd_una &&
		    TCP_NEXT_GET((struct tcpiphdr *)tp)
			== (struct tcpiphdr *)tp &&
		    ti->ti_len <= sbspace(&so->so_rcv)) {
			/*
			 * this is a pure, in-sequence data packet
			 * with nothing on the reassembly queue and
			 * we have enough buffer space to take it.
			 */
			TCPSTAT(tcps_preddat);
			tp->rcv_nxt += ti->ti_len;
			TCPSTAT(tcps_rcvpack);
			TCPSTAT_ADD(tcps_rcvbyte, ti->ti_len);
			/* If last ACK falls within this segment's range
			 * of sequence numbers, record the timestamp.
			 */
			if ( ts_present &&
			   SEQ_LEQ( ti->ti_seq, tp->last_ack_sent ) &&
			   SEQ_LT( tp->last_ack_sent, ti->ti_seq+ti->ti_len)){
				tp->ts_recent = ts_val;
				tp->ts_recent_age = tcp_now;
			}
			
			/* fix related to 766435; although this specific
			   code was not the source of the problem with
			   the bug, this seems to be another area where
			   the window information needs to be updated
			   (see "step6:" label); since we know
			   snd_wnd == tiwin, the test has been
			   optimized so redundant checks are not made.    */
			if (SEQ_LT(tp->snd_wl1, ti->ti_seq) || 
			    tp->snd_wl1 == ti->ti_seq &&
			    SEQ_LT(tp->snd_wl2, ti->ti_ack)) {
				tp->snd_wl1 = ti->ti_seq;
				tp->snd_wl2 = ti->ti_ack;
			}

			/*
			 * Drop TCP, IP headers and options.  Then add data
			 * to socket buffer
			 */
			m->m_off +=
			   sizeof(struct tcpiphdr)+off-sizeof(struct tcphdr);
			m->m_len -=
			   sizeof(struct tcpiphdr)+off-sizeof(struct tcphdr);
			ASSERT(so->so_input);
			(*so->so_input)(so, m);
			if((tp->t_flags & TF_AGGRESSIVE_ACK &&
			   ((int)(tp->rcv_nxt - tp->last_ack_sent)) >=
				(tp->t_maxseg << 1)) ||
				/*
				 * TF_IMMEDIATE_ACK is set when we suspect
				 * the peer is sending its first packet of
				 * slow start.  This avoids the peer
				 * waiting 200ms before it can ramp up.
				 */
				(tp->t_flags & TF_IMMEDIATE_ACK) ||
				tiflags & TH_PUSH) {
				tp->t_flags = (tp->t_flags | TF_ACKNOW) &
						~TF_IMMEDIATE_ACK;
				(void) tcp_output(tp, ipsec);
			} else {
				tp->t_flags |= (tp->t_flags & TF_FASTACK) ?
					TF_ACKNOW : TF_DELACK;
			}
			{
			struct socket *oso = inp->inp_socket;
			if (!INPCB_RELE(inp) || oso != so)
				SOCKET_UNLOCK(so);
			}
			_SESMGR_SOATTR_FREE(ipsec);
			return;
		}
	}

	/*
	 * Drop TCP, IP headers and TCP options.
	 */
	m->m_off += sizeof(struct tcpiphdr)+off-sizeof(struct tcphdr);
	m->m_len -= sizeof(struct tcpiphdr)+off-sizeof(struct tcphdr);

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
		struct sockaddr_in sin;

		if (tiflags & TH_RST)
			goto drop;
		if (tiflags & TH_ACK)
			goto dropwithreset;
		if ((tiflags & TH_SYN) == 0)
			goto drop;
		/* RFC1122 4.2.3.10, p. 104: discard bcast/mcast */
		if (m->m_flags & (M_BCAST|M_MCAST) ||
		    IN_MULTICAST(ti->ti_dst.s_addr))
			goto drop;
		sin.sin_family = AF_INET;
		sin.sin_addr = ti->ti_src;
		sin.sin_port = ti->ti_sport;
		/* XXX do we need lock? */
		laddr = inp->inp_laddr;
#ifdef INET6
		if (inp->inp_latype == IPATYPE_UNBD)
#else
		if (inp->inp_laddr.s_addr == INADDR_ANY)
#endif
			laddr = ti->ti_dst;
		/* checks regular PCBs and TIME-WAITers */
		if (in_pcbsetaddrx(inp, &sin, laddr, (struct inaddrpair *)0)) {
			goto drop;
		}

#ifdef INET6
		inp->inp_flags &= ~INP_COMPATV6;
#endif
		tcp_template(tp);
		if (om) {
#ifdef INET6
			tcp_dooptions(tp, om, &ti->ti_t, &ts_present, &ts_val,
				&ts_ecr);
#else
			tcp_dooptions(tp, om, ti, &ts_present, &ts_val,
				&ts_ecr);
#endif
			om = 0;
		}
		/* Use RFC 1323 options on this connection if we've received
		 * those options on a listen socket.
		 */
		if ( tcp_winscale && (tp->t_flags & TF_RCVD_SCALE) ) {
			tp->t_flags |= TF_REQ_SCALE;

			/* Compute scaling value we need.
			 */
			tp->request_r_scale = 0;
			while ( tp->request_r_scale < TCP_MAX_WINSHIFT &&
			  TCP_MAXWIN<<tp->request_r_scale<so->so_rcv.sb_hiwat )
				tp->request_r_scale++;
		}
		if ( tcp_tsecho && (tp->t_flags & TF_RCVD_TSTMP) )
			tp->t_flags |= TF_REQ_TSTMP;
		
		if (iss) {
			tp->iss = iss;
		} else {
			/* Make the least significant bits of the sequence
			 * number hard to predict to combat source address
			 * spoofing.  Use MD5 for this if the systune 
			 * tunable "md5_iss" is set.   Otherwise,  use a
			 * combination of nanoclock and start address of
			 * the newly allocated tcpcb which by itself should 
			 * plug the security hole pretty well 
			 */
			if (tcpiss_md5) {
				tp->iss = tcp_iss + iss_hash(tp);
			} else {
				struct timespec tv;
				nanotime(&tv);
				tp->iss = tcp_iss + 
					(tv.tv_nsec+(u_long)tp)%(TCP_ISSINCR/8);
			}
		}
		tcp_iss += TCP_ISSINCR/2;
		tp->irs = ti->ti_seq;
		tcp_sendseqinit(tp);
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		tp->t_state = TCPS_SYN_RECEIVED;
		tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
		dropsocket = 0;		/* committed to socket */
		TCPSTAT(tcps_accepts);
		goto trimthenstep6;
		}

	/*
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
	case TCPS_SYN_SENT:
		if ((tiflags & TH_ACK) &&
		    (SEQ_LEQ(ti->ti_ack, tp->iss) ||
		     SEQ_GT(ti->ti_ack, tp->snd_max)))
			goto dropwithreset;
		if (tiflags & TH_RST) {
			#pragma mips_frequency_hint NEVER
			if (tiflags & TH_ACK)
				tp = tcp_drop(tp, ECONNREFUSED, ipsec);
			goto drop;
		}
		if ((tiflags & TH_SYN) == 0)
			goto drop;
		if (tiflags & TH_ACK) {
			tp->snd_una = ti->ti_ack;
			SND_HIGH(tp) = ti->ti_ack;
			if (SEQ_LT(tp->snd_nxt, tp->snd_una))
				tp->snd_nxt = tp->snd_una;
		}
		tp->t_timer[TCPT_REXMT] = 0;
		tp->irs = ti->ti_seq;
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		if (tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss)) {
			TCPSTAT(tcps_connects);
			soisconnected(so);
			tp->t_state = TCPS_ESTABLISHED;
			/* Do window scaling on this connection? */
			if ( (tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE) ) {
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}
			(void) tcp_reass(tp, (struct tcpiphdr *)0,
				(struct mbuf *)0);
			/*
			 * if we didn't have to retransmit the SYN,
			 * use its rtt as our initial srtt & rtt var.
			 */
			if (tp->t_rtt)
				tcp_xmit_timer(tp,tp->t_rtt);
		} else
			tp->t_state = TCPS_SYN_RECEIVED;

trimthenstep6:
		/*
		 * Advance ti->ti_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		ti->ti_seq++;
		if (ti->ti_len > tp->rcv_wnd) {
			#pragma mips_frequency_hint NEVER
			todrop = ti->ti_len - tp->rcv_wnd;
			m_adj(m, -todrop);
			ti->ti_len = tp->rcv_wnd;
			tiflags &= ~TH_FIN;
			TCPSTAT(tcps_rcvpackafterwin);
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		}
		tp->snd_wl1 = ti->ti_seq - 1;
		tp->rcv_up = ti->ti_seq;
		goto step6;

	/*
	 * If the state is SYN_RECEIVED:
	 *	If seg contains an ACK, but not for our SYN, drop the input
	 *	and generate an RST.  See page 36, rfc793
	 */
	case TCPS_SYN_RECEIVED:
		if ((tiflags & TH_ACK) &&
		    (SEQ_LEQ(ti->ti_ack, tp->iss) ||
		     SEQ_GT(ti->ti_ack, tp->snd_max)))
			goto dropwithreset;
		break;
  	}
  
	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check RFC 1323 timestamp, if present.
	 * Then check that at least some bytes of segment are within 
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 */
	/* PAWS:  If we have a timestamp on this segment and it's
	 * less than ts_recent, drop it.  But, before dropping it,
	 * see if TS.recent is older than 24 days.
	 */
	if ( ts_present && ! (tiflags & TH_RST) && tp->ts_recent &&
	    TSTMP_LT( ts_val, tp->ts_recent ) ) {
		if ( (int)(tcp_now - tp->ts_recent_age) > TCP_PAWS_IDLE ) {

                        /* Invalidate ts_recent.  If this segment updates
			 * ts_recent, the age will be reset later and ts_recent
			 * will get a valid value.  If it does not, setting
			 * ts_recent to zero will satisfy the requirement
			 * that zero be placed in the timestamp echo reply
			 * when ts_recent isn't valid and will prevent PAWS
			 * dropping until we get a valid ts_recent.
			 */
			tp->ts_recent = 0;
		}
		else {
			TCPSTAT(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, ti->ti_len);
			TCPSTAT(tcps_pawsdrop);
			goto dropafterack;
		}
	}
	todrop = tp->rcv_nxt - ti->ti_seq;
	if (todrop > 0) {
		#pragma mips_frequency_hint NEVER
		if (tiflags & TH_SYN) {
			if ((tp->t_state == TCPS_TIME_WAIT) &&
			    (SEQ_LT(ti->ti_seq, tp->irs))) {
				/*
				 * If a new connection request is received
				 * while in TIME_WAIT, drop the old connection
				 * and start over if the sequence number of this
				 * SYN is lower than the last IRS, and the
				 * difference between the SYN and the last IRS
				 * is more than the amount of data received on
				 * the old connection.  This will help to
				 * ensure that any lingering duplicates are
				 * out of range.
				 * As an additional hack, since we ignore RSTs
				 * in TIME-WAIT, if we've received one, we're
				 * probably being killed by a peer that did not
				 * like our ACK to their first SYN attempt, so
				 * blow the old one away.
				 * (this would have happened prior to ignoring
				 * RSTs in TIME-WAIT, so it is no worse than
				 * the old behavior).
				 */
				if (((tp->irs - ti->ti_seq) > 
				     (tp->rcv_nxt - tp->irs)) ||
				    (tp->t_flags & TF_WAS_RST)) {
					goto cleartw;
				}
			}
			tiflags &= ~TH_SYN;
			ti->ti_seq++;
			if (ti->ti_urp > 1) 
				ti->ti_urp--;
			else
				tiflags &= ~TH_URG;
			todrop--;
		}
		if (todrop > ti->ti_len ||
		    todrop == ti->ti_len && (tiflags&TH_FIN) == 0) {
			TCPSTAT(tcps_rcvduppack);
			TCPSTAT_ADD(tcps_rcvdupbyte, ti->ti_len);
			/*
			 * If segment is just one to the left of the window,
			 * check three special cases:
			 * 1. Don't toss RST in response to 4.2-style keepalive.
			 * 2. If the only thing to drop is a FIN, we can drop
			 *    it, but check the ACK or we will get into FIN
			 *    wars if our FINs crossed (both CLOSING).
			 * 3. The packet is an ACK from a crossing probe,
			 *    ti->ti_len == 0 && todrop == 1.
			 * In either case, send ACK to resynchronize,
			 * but keep on processing for RST or ACK.
			 */
			if ((tiflags & TH_FIN || todrop == 1)
			    && (todrop == ti->ti_len + 1)
#ifdef TCP_COMPAT_42
			  || (tiflags & TH_RST && ti->ti_seq == tp->rcv_nxt - 1)
#endif
			   ) {
				todrop = ti->ti_len;
				tiflags &= ~TH_FIN;
				tp->t_flags |= TF_ACKNOW;
			} else
			/*
			 * Patch to fix the case when a bound socket connects
			 * to itself. For that test case, the protocol
			 * stack gets into an infinite loop since the 
			 * original BSD code would just drop processing
			 * and only ACK. The connection therefore can never 
			 * gets pass TCPS_SYN_RECEIVED state.
			 * Added TF_ACKNOW to guarantee an ack for the more
			 * common case of a lost ACK which just need a
			 * reply.
			 */
    			if ((todrop == 0 && (tiflags & TH_ACK))) 
				tp->t_flags |= TF_ACKNOW;
			else
				goto dropafterack;
		} else {
			#pragma mips_frequency_hint NEVER
			TCPSTAT(tcps_rcvpartduppack);
			TCPSTAT_ADD(tcps_rcvpartdupbyte, todrop);
		}
		m_adj(m, todrop);
		ti->ti_seq += todrop;
		ti->ti_len -= todrop;
		if (ti->ti_urp > todrop)
			ti->ti_urp -= todrop;
		else {
			tiflags &= ~TH_URG;
			ti->ti_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
	if ((so->so_state & SS_NOFDREF) &&
	    tp->t_state > TCPS_CLOSE_WAIT && ti->ti_len) {
		#pragma mips_frequency_hint NEVER
		tp = tcp_close(tp);
		TCPSTAT(tcps_rcvafterclose);
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (ti->ti_seq+ti->ti_len) - (tp->rcv_nxt+tp->rcv_wnd);
	if (todrop > 0) {
		#pragma mips_frequency_hint NEVER
		TCPSTAT(tcps_rcvpackafterwin);
		if (todrop >= ti->ti_len) {
			TCPSTAT_ADD(tcps_rcvbyteafterwin, ti->ti_len);
			/*
			 * If a new connection request is received
			 * while in TIME_WAIT, drop the old connection
			 * and start over if the sequence numbers
			 * are above the previous ones.
			 * As an additional hack, since we ignore RSTs in
			 * TIME-WAIT, if we've received one, we're probably
			 * being killed by the peer, so blow it away.
			 */
			if ((tiflags & TH_SYN) &&
			    (tp->t_state == TCPS_TIME_WAIT) &&
			    (SEQ_GT(ti->ti_seq, tp->rcv_nxt) ||
			     (tp->t_flags & TF_WAS_RST))) {
cleartw:
				iss = tp->snd_nxt + TCP_ISSINCR;
				tp = tcp_close(tp);
				/* Fix problem where TCP options are
				 * ignored in this case.  Put back
				 * headers and start over.
				 */
				m->m_off -= sizeof(struct tcpiphdr)+off-
					sizeof(struct tcphdr);
				m->m_len += sizeof(struct tcpiphdr)+off-
					sizeof(struct tcphdr);
				{
				struct socket *oso = inp->inp_socket;
				if (!INPCB_RELE(inp) || oso != so)
					SOCKET_UNLOCK(so);
				}
				so = 0;
				goto getopts;
			}
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && ti->ti_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				TCPSTAT(tcps_rcvwinprobe);
			} else
				goto dropafterack;
		} else
			TCPSTAT_ADD(tcps_rcvbyteafterwin, todrop);
		m_adj(m, -todrop);
		ti->ti_len -= todrop;
		tiflags &= ~(TH_PUSH|TH_FIN);
	}

	/* If last ACK falls within this segment's range of sequence numbers,
	 * record its timestamp.
	 */
	if ( ts_present && SEQ_LEQ( ti->ti_seq, tp->last_ack_sent ) &&
	   SEQ_LT( tp->last_ack_sent, ti->ti_seq+ti->ti_len+
			((tiflags&(TH_SYN|TH_FIN))!=0) ) ) {
		tp->ts_recent = ts_val;
		tp->ts_recent_age = tcp_now;
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
	if (tiflags&TH_RST) switch (tp->t_state) {
	#pragma mips_frequency_hint NEVER

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
		tp = tcp_close(tp);
		goto drop;
	case TCPS_TIME_WAIT:
	/* 
           RFC 1337 says ignore RSTs in TIME-WAIT.
	   But because some stacks are screwed up and reuse the ISN for
	   the next subsequent connection, we need to blow away the tcb
	   when we get a RST in response to our earlier ACK in TIME_WAIT
	   for their SYN with a screwed up ISN number. Check bug 663214
	*/
		tp->t_flags |= TF_WAS_RST;
		tp = tcp_close(tp);
		goto drop;
	}

	/*
	 * If a SYN is in the window, then this is an
	 * error and we send an RST and drop the connection.
	 */
	if (tiflags & TH_SYN) {
		tp = tcp_drop(tp, ECONNRESET, ipsec);
		goto dropwithreset;
	}

	/*
	 * If the ACK bit is off we drop the segment and return.
	 */
	if ((tiflags & TH_ACK) == 0)
		goto drop;
	
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
		if (SEQ_GT(tp->snd_una, ti->ti_ack) ||
		    SEQ_GT(ti->ti_ack, tp->snd_max))
			goto dropwithreset;
		TCPSTAT(tcps_connects);
		soisconnected(so);
		tp->t_state = TCPS_ESTABLISHED;
		/* Do window scaling? */
		if ( (tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE) ) {
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
		}
		(void) tcp_reass(tp, (struct tcpiphdr *)0, (struct mbuf *)0);
		tp->snd_wl1 = ti->ti_seq - 1;
		/* fall into ... */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < ti->ti_ack <= tp->snd_max
	 * then advance tp->snd_una to ti->ti_ack and drop
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

		if (SEQ_LEQ(ti->ti_ack, tp->snd_una)) {
			if (ti->ti_len == 0 && tiwin == tp->snd_wnd) {
				#pragma mips_frequency_hint NEVER
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
				    ti->ti_ack != tp->snd_una)
					tp->t_dupacks = 0;
				else if (++tp->t_dupacks == tcprexmtthresh) {
					tcp_seq onxt = tp->snd_nxt;
					u_long ocwnd = tp->snd_cwnd;
					u_int win =
					    MIN(tp->snd_wnd, tp->snd_cwnd) / 2 /
						tp->t_maxseg;

					if (win < 2)
						win = 2;
					/*
					 * Stop go-fast mode if loss is
					 * suspected.
					 */
					tp->t_flags &= ~TF_GOFAST;
					/*
					 * Don't set ssthresh yet.  Wait until
					 * we check we aren't already doing
					 * fast-recovery below.
					 */
					tp->t_timer[TCPT_REXMT] = 0;
					tp->t_rtt = 0;
					tp->snd_nxt = ti->ti_ack;
					tp->snd_cwnd = tp->t_maxseg;
					(void) tcp_output(tp, ipsec);
                                        /*
                                         * If we are entering fast-recovery
                                         * from normal data flow, set
                                         * snd_high to the highest byte sent
                                         * and shrink cwnd.
                                         */
                                        if((SEQ_LEQ(SND_HIGH(tp), 
					    tp->snd_una))) {
                                                tp->snd_ssthresh =
                                                    win * tp->t_maxseg;
						SND_HIGH(tp) = onxt;
                                                ocwnd = tp->snd_ssthresh +
                                                    tp->t_maxseg * tp->t_dupacks;
                                        }
					tp->snd_cwnd = ocwnd;
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;
				} else if (SEQ_LT(tp->snd_una, SND_HIGH(tp))) {
					/*
					 * We're in fast-recovery, so bump
					 * cwnd for every new ack.
					 */
					tp->snd_cwnd += tp->t_maxseg;
					(void) tcp_output(tp, ipsec);
					goto drop;
				}
			} else
				tp->t_dupacks = 0;
			break;
		}

		/*
		 * If the socket has a LAN go-fast-mode threshold set, then
		 * _if_ this ack can be used to measure a previously
		 * timestamped segment, measure the RTT and if we
		 * appear to be on a low-delay LAN, go into fast mode.
		 * Check SKO_FMTHRESH(tp) first so that if we don't have
		 * the threshold set (default), we avoid extra work.
		 */
		if(SKO_FMTHRESH(tp) && !(tp->t_flags & TF_GOFAST) &&
		    SKO_TIMESPEC(tp).tv_sec && 
		    SEQ_GEQ(ti->ti_ack, SKO_TS_SEQ(tp))) {
			struct timespec n_now;
			nanotime(&n_now);
			if(NTS_DELTA(n_now,SKO_TIMESPEC(tp)) 
			    < SKO_FMTHRESH(tp)) {
				tp->t_flags |= TF_GOFAST;
			}
			SKO_TIMESPEC(tp).tv_sec = 0;
		}

		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets and we have acked
		 * the end of the fast-recovery region, retract it.
		 * This implements <draft-ietf-tcpimpl-cong-control-00>.
		 */
		if ((SEQ_LT(tp->snd_una, SND_HIGH(tp)) && /* Brakmo/Peterson */
		    SEQ_LEQ(SND_HIGH(tp), ti->ti_ack)) && /* end of fast-rcvry*/
		    tp->snd_cwnd > tp->snd_ssthresh)
			tp->snd_cwnd = tp->snd_ssthresh;
		tp->t_dupacks = 0;
		if (SEQ_GT(ti->ti_ack, tp->snd_max)) {
			TCPSTAT(tcps_rcvacktoomuch);
			goto dropafterack;
		}
		acked = ti->ti_ack - tp->snd_una;
		TCPSTAT(tcps_rcvackpack);
		TCPSTAT_ADD(tcps_rcvackbyte, acked);

		/*
		 * If transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 */
		if (tp->t_rtt && SEQ_GT(ti->ti_ack,tp->t_rtseq)) {
		   if (ts_present) {
			/* If a time-stamp is available, use
			 * it.  Afterwards, keep rtt "timer"
			 * going (but we won't use value in
			 * tp->t_rtt).
			 */
			tcp_xmit_timer(tp, tcp_now-ts_ecr+1 );
			tp->t_rtt = 1;
			tp->t_rtseq = ti->ti_ack;
		   }
		   else
			tcp_xmit_timer(tp,tp->t_rtt);
		}

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (ti->ti_ack == tp->snd_max) {
			tp->t_timer[TCPT_REXMT] = 0;
			needoutput = 1;
		} else if (tp->t_timer[TCPT_PERSIST] == 0)
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
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

		if(tp->t_flags & TF_GOFAST) {
			incr = incr << 3;
			if(incr > TCP_MAX_GOFAST_INC) {
				/*
				 * If the increment is too big, the congestion
				 * avoidance calculation below can overflow if
				 * int is 32 bits.
				 */
				incr = TCP_MAX_GOFAST_INC;
			}
		}
		if (cw > tp->snd_ssthresh)
			incr = incr * incr / cw;
		tp->snd_cwnd = MIN(cw + incr, TCP_MAXWIN<<tp->snd_scale);
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
		if (acked && (so->so_rcv.sb_flags & SB_INPUT_ACK)) {
			ASSERT(so->so_input);
			(*so->so_input)(so, NULL);
		}
		if (so->so_snd.sb_flags & SB_NOTIFY)
			tcp_wakeup(so, NETEVENT_TCPUP);
		if (SACK_SNDHOLES(tp))
			sack_sndacked(tp, ti->ti_ack);
		tp->snd_una = ti->ti_ack;
		/*
		 * Unless we are in fast-recovery, keep snd_high
		 * in sync with snd_una.
		 */
		if(SEQ_LT(SND_HIGH(tp), tp->snd_una))
			SND_HIGH(tp) = tp->snd_una;
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
				tp->t_purgeat = time + tcp_2msl;
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
			tcp_tw_restart(inp);
			tp->t_purgeat = time + tcp_2msl;
			goto dropafterack;
		}
	}

step6:
	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if ((tiflags & TH_ACK) &&
	    (SEQ_LT(tp->snd_wl1, ti->ti_seq) || tp->snd_wl1 == ti->ti_seq &&
	    (SEQ_LT(tp->snd_wl2, ti->ti_ack) ||
	     tp->snd_wl2 == ti->ti_ack && tiwin > tp->snd_wnd))) {
		/* keep track of pure window updates */
		if (ti->ti_len == 0 &&
		    tp->snd_wl2 == ti->ti_ack && tiwin > tp->snd_wnd)
			TCPSTAT(tcps_rcvwinupd);
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = ti->ti_seq;
		tp->snd_wl2 = ti->ti_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;
	}

	/*
	 * Process segments with URG.
	 */
	if ((tiflags & TH_URG) && ti->ti_urp &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
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
		if (SEQ_GT(ti->ti_seq+ti->ti_urp, tp->rcv_up)) {
			tp->rcv_up = ti->ti_seq + ti->ti_urp;
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
		if (ti->ti_urp <= ti->ti_len
#ifdef SO_OOBINLINE
		     && (so->so_options & SO_OOBINLINE) == 0
#endif
		     )
#ifdef INET6
			tcp_pulloutofband(so, &ti->ti_t, m);
#else
			tcp_pulloutofband(so, ti, m);
#endif
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
	 * is presented to the user (this happens in tcp_usrreq.c,
	 * case PRU_RCVD).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 */
	if ((ti->ti_len || (tiflags&TH_FIN)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		TCP_REASS(tp, ti, m, so, tiflags);
		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 */
		len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
		if((tp->t_flags & TF_AGGRESSIVE_ACK &&
		  ((int)(tp->rcv_nxt - tp->last_ack_sent)) >= 
			(tp->t_maxseg << 1)) ||
			/*
			 * TF_IMMEDIATE_ACK is set when we suspect
			 * the peer is sending its first packet of
			 * slow start.  This avoids the peer
			 * waiting 200ms before it can ramp up.
			 */
			(tp->t_flags & TF_IMMEDIATE_ACK) ||
			tiflags & TH_PUSH) {
			tp->t_flags = (tp->t_flags | TF_ACKNOW) &
					~TF_IMMEDIATE_ACK;
		}
	} else {
		m_freem(m);
		tiflags &= ~TH_FIN;
	}

	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.
	 */
	if (tiflags & TH_FIN) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			socantrcvmore(so);
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
			tp->t_purgeat = time + tcp_2msl;
			soisdisconnected(so);
			break;

		/*
		 * In TIME_WAIT state restart the 2 MSL time_wait timer.
		 */
		case TCPS_TIME_WAIT:
			tcp_tw_restart(inp);
			tp->t_purgeat = time + tcp_2msl;
			break;
		}
	}
#ifdef DEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, &tcp_saveti, 0);
#endif

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW))
		(void) tcp_output(tp, ipsec);
	{
	struct socket *oso = inp->inp_socket;
	if (!INPCB_RELE(inp) || oso != so)
		SOCKET_UNLOCK(so);
	}
	_SESMGR_SOATTR_FREE(ipsec);
	return;

dropafterack:
	TCP_UTRACE_TP(UTN('tcpi','ndaa'), tp, __return_address);
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 */
	if (tiflags & TH_RST)
		goto drop;
	m_freem(m);
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp, ipsec);
	{
	struct socket *oso = inp->inp_socket;
	if (!INPCB_RELE(inp) || oso != so)
		SOCKET_UNLOCK(so);
	}
	_SESMGR_SOATTR_FREE(ipsec);
	return;

dropwithreset:
	TCP_UTRACE_TP(UTN('tcpi','ndwr'), tp, __return_address);
	if (om) {
		(void) m_free(om);
		om = 0;
	}
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 * Don't bother to respond if destination was broadcast.
	 */
	if ((tiflags & TH_RST) || m->m_flags & (M_BCAST|M_MCAST) || 
	    IN_MULTICAST(ti->ti_dst.s_addr))
		goto drop;
	if (tiflags & TH_ACK)
#ifdef INET6
		tcp_respond(tp, ti, m, (tcp_seq)0, ti->ti_ack, TH_RST, ipsec,
		  AF_INET);
#else
		tcp_respond(tp, ti, m, (tcp_seq)0, ti->ti_ack, TH_RST, ipsec);
#endif
	else {
		if (tiflags & TH_SYN)
			ti->ti_len++;
		tcp_respond(tp, ti, m, ti->ti_seq+ti->ti_len, (tcp_seq)0,
#ifdef INET6
			TH_RST|TH_ACK, ipsec, AF_INET);
#else
			TH_RST|TH_ACK, ipsec);
#endif
	}
	TCPSTAT(tcps_sndrst);
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
	_SESMGR_SOATTR_FREE(ipsec);
	return;

drop:
	TCP_UTRACE_TP(UTN('tcpi','ndrp'), tp, __return_address);
	if (om)
		(void) m_free(om);
	/*
	 * Drop space held by incoming segment and return.
	 */
#ifdef DEBUG
	if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, &tcp_saveti, 0);
#endif
	_SESMGR_SOATTR_FREE(ipsec);
	m_freem(m);
	if (inp) {
		struct socket *oso = inp->inp_socket;

		if (INPCB_RELE(inp) && oso == so)
			so = 0;
	}
	/* destroy temporarily created socket */
	if (so) {
		if (dropsocket) {
			(void) soabort(so);
		} else {
			SOCKET_UNLOCK(so);
		}
	}
	return;
}

void
tcp_dooptions(
	struct tcpcb *tp,
	struct mbuf *om,
#ifdef INET6
	struct tcphdr *th,
#else
	struct tcpiphdr *ti,
#endif
	int *ts_present,
	u_int32_t *ts_val,
	u_int32_t *ts_ecr)
{
	register u_char *cp;
	u_short mss = 0;
	int opt, optlen, cnt;

	cp = mtod(om, u_char *);
	cnt = om->m_len;
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[1];
			if (optlen <= 0)
				break;
		}
		switch (opt) {

		default:
			continue;

		case TCPOPT_MAXSEG:
			if (optlen != 4)
				continue;
#ifdef INET6
			if (!(th->th_flags & TH_SYN))
#else
			if (!(ti->ti_flags & TH_SYN))
#endif
				continue;
			bcopy((char *) cp + 2, (char *) &mss, sizeof(mss));
			NTOHS(mss);
			break;

		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW)
				continue;
#ifdef INET6
			if (!(th->th_flags & TH_SYN))
#else
			if (!(ti->ti_flags & TH_SYN))
#endif
				continue;
			tp->t_flags |= TF_RCVD_SCALE;
			tp->requested_s_scale = MIN( cp[2], TCP_MAX_WINSHIFT );
			break;

		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP)
				continue;
			
			/* Put value into ts_val in host byte order.
			 * Could be unaligned.
			 */
			*ts_present = 1;
			*ts_val = cp[2]<<24 | cp[3]<<16 | cp[4]<<8 | cp[5];
			*ts_ecr = cp[6]<<24 | cp[7]<<16 | cp[8]<<8 | cp[9];

			/* A timestamp received in a SYN makes
			 * it ok to send timestamp requests and replies.
			 */
#ifdef INET6
			if (th->th_flags & TH_SYN) {
#else
			if ( ti->ti_flags & TH_SYN ) {
#endif
				tp->t_flags |= TF_RCVD_TSTMP;
				tp->ts_recent = *ts_val;
				tp->ts_recent_age = tcp_now;
			}
			break;

		case TCPOPT_SACKPERMIT:
			if (!tcp_sack || optlen != TCPOLEN_SACKPERMIT)
				continue;
#ifdef INET6
			if (th->th_flags & TH_SYN) {
#else
			if (ti->ti_flags & TH_SYN) {
#endif
				tp->t_flags |= TF_RCVD_SACK;
#if 0
				TCPSTAT(tcps_sackconns);
#endif
				SACK_PRINTF(("tcp_dooptions SACKPERMIT tcpcb: %x  flags %x\n", tp, tp->t_flags));
			}
			break;

		case TCPOPT_SACK:
			if (!tcp_sack || tp->t_flags & TF_NOOPT || 
			     optlen < TCPOLEN_SACKMIN || optlen % 8 != 2) 
				continue;
			/* RFC if ack with sack, the sender should
			   record sack for future reference, and
			   turn on the SACK bit.
			*/
			SACK_PRINTF(("tcp_dooptions SACK tcpcb: %x\n", tp));
			SACK_PRINTF(("  optlen = %d\n", optlen));
#ifdef INET6
			if (th->th_flags & TH_ACK) {
#else
			if (ti->ti_flags & TH_ACK) {
#endif
				tp->t_flags |= TF_SNDCHECK_SACK;
#if 0
				TCPSTAT(tcps_sackacks);
#endif
				sack_sndlist(tp, (char *)&cp[2], optlen-2); 
				/* sndlist does byte reordering for us */
			}
#ifdef SACK_DEBUG
			else {
				printf("tcp_dooptions SACK: NO ACK!!\n"); 
			}
#endif 
			break;

		}		
	}
	if (mss)
		(void) tcp_mss(tp, mss);	/* sets t_maxseg */
	(void) m_free(om);
}

/*
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 */
void
tcp_pulloutofband(
	struct socket *so,
#ifdef INET6
	struct tcphdr *th,
#else
	struct tcpiphdr *ti,
#endif
	register struct mbuf *m)
{
#ifdef INET6
	int cnt = th->th_urp - 1;
#else
	int cnt = ti->ti_urp - 1;
#endif
	while (cnt >= 0) {
		if (m->m_len > cnt) {
			char *cp = mtod(m, caddr_t) + cnt;
			struct tcpcb *tp = sototcpcb(so);

			tp->t_iobc = *cp;
			tp->t_oobflags |= TCPOOB_HAVEDATA;
			bcopy(cp+1, cp, (unsigned)(m->m_len - cnt - 1));
			m->m_len--;
			return;
		}
		cnt -= m->m_len;
		m = m->m_next;
		if (m == 0)
			break;
	}
	panic("tcp_pulloutofband");
}

/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 */
void
tcp_xmit_timer(register struct tcpcb *tp, short rtt)
{
	register short delta;

	TCPSTAT(tcps_rttupdated);
	if (tp->t_srtt != 0) {
		/*
		 * srtt is stored as fixed point with 3 bits after the
		 * binary point (i.e., scaled by 8).  The following magic
		 * is equivalent to the smoothing algorithm in rfc793 with
		 * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
		 * point).  Adjust rtt to origin 0.
		 */
		/* Brakmo/Peterson */
		delta = ((rtt - 1) << TCP_RTTVAR_SHIFT) - 
			(tp->t_srtt >> TCP_RTT_SHIFT);
		if ((tp->t_srtt += delta) <= 0)
			tp->t_srtt = 1;
		/*
		 * We accumulate a smoothed rtt variance (actually, a
		 * smoothed mean difference), then set the retransmit
		 * timer to smoothed rtt + 4 times the smoothed variance.
		 * rttvar is stored as fixed point with 2 bits after the
		 * binary point (scaled by 4).  The following is
		 * equivalent to rfc793 smoothing with an alpha of .75
		 * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
		 * rfc793's wired-in beta.
		 */
		if (delta < 0)
			delta = -delta;
		delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
		if ((tp->t_rttvar += delta) <= 0)
			tp->t_rttvar = 1;
	} else {
		/* 
		 * No rtt measurement yet - use the unsmoothed rtt.
		 * Set the variance to half the rtt (so our first
		 * retransmit happens at 2*rtt)
		 */
		tp->t_srtt = rtt << TCP_RTT_SHIFT;
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT - 1);
	}
	tp->t_rtt = 0;
	tp->t_rxtshift = 0;

	/*
	 * the retransmit should happen at rtt + 4 * rttvar.
	 * Because of the way we do the smoothing, srtt and rttvar
	 * will each average +1/2 tick of bias.  When we compute
	 * the retransmit timer, we want 1/2 tick of rounding and
	 * 1 extra tick because of +-1/2 tick uncertainty in the
	 * firing of the timer.  The bias will give us exactly the
	 * 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below
	 * the minimum feasible timer (which is 2 ticks).
	 */
	/* Brakmo/Peterson */
	TCPT_RANGESET(tp->t_rxtcur,
	      (((tp)->t_srtt >> TCP_RTT_SHIFT) +
	      (tp)->t_rttvar) >> TCP_RTTVAR_SHIFT, rtt + 1, TCPTV_REXMTMAX);
	
	/*
	 * We received an ack for a packet that wasn't retransmitted;
	 * it is probably safe to discard any error indications we've
	 * received recently.  This isn't quite right, but close enough
	 * for now (a route might have failed after we sent a segment,
	 * and the return path might not be symmetrical).
	 */
	tp->t_softerror = 0;
}

/*
 * Determine a reasonable value for maxseg size.
 * If the route is known, check route for mtu.
 * If none, use an mss that can be handled on the outgoing
 * interface without forcing IP to fragment; if bigger than
 * an mbuf cluster (MCLBYTES), round down to nearest multiple of MCLBYTES
 * to utilize large mbufs.  If no route is found, route has no mtu,
 * or the destination isn't local, use a default, hopefully conservative
 * size (usually 512 or the default IP max size, but no more than the mtu
 * of the interface), as we can't discover anything about intervening
 * gateways or networks.  We also initialize the congestion/slow start
 * window to be a single segment if the destination isn't local.
 * While looking at the routing entry, we also initialize other path-dependent
 * parameters from pre-set or cached values in the routing entry.
 */

tcp_mss(
	register struct tcpcb *tp,
	u_short offer)
{
	struct route *ro;
	register struct rtentry *rt;
	struct ifnet *ifp;
	register int rtt, mss;
	u_long bufsize;
	struct inpcb *inp;
	struct socket *so;
	extern int tcp_mssdflt;
	extern u_int tcp_recvspace, tcp_sendspace;
	int	mss_advertise;

	inp = tp->t_inpcb;
	ro = &inp->inp_route;
	ROUTE_RDLOCK();
	if ((rt = ro->ro_rt) == (struct rtentry *)0) {
		/* No route yet, so try to acquire one */
#ifdef INET6
		if (inp->inp_fatype != IPATYPE_UNBD) {
			if (inp->inp_flags & INP_COMPATV4) {
				ro->ro_dst.sa_family = AF_INET;
				((struct sockaddr_in *) &ro->ro_dst)->sin_addr =
					inp->inp_faddr;
				rtalloc(ro);
			} else {
				ro->ro_dst.sa_family = AF_INET6;
				((struct sockaddr_new *)&ro->ro_dst)->sa_len =
				  sizeof(struct sockaddr_in6);
				COPY_ADDR6(inp->inp_faddr6,
				  satosin6(&ro->ro_dst)->sin6_addr);
				in6_rtalloc(ro, INP_IFA);
			}
#else
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			ro->ro_dst.sa_family = AF_INET;
			((struct sockaddr_in *) &ro->ro_dst)->sin_addr =
				inp->inp_faddr;
			rtalloc(ro);
#endif
		}
		if ((rt = ro->ro_rt) == (struct rtentry *)0) {
			ROUTE_UNLOCK();
			return (tcp_mssdflt);
		}
	}
	ifp = rt->rt_ifp;
	so = inp->inp_socket;
	if ((so->so_rcv.sb_hiwat == tcp_recvspace) && ifp->if_recvspace) {
		so->so_rcv.sb_hiwat = ifp->if_recvspace;
	}
	if ((so->so_snd.sb_hiwat == tcp_sendspace) && ifp->if_sendspace) {
		so->so_snd.sb_hiwat = ifp->if_sendspace;
	}

	/* Do RFC 1323 window scaling if either of our send or
	 * receive buffer spaces are greater than 64K.
	 */
	if ( tcp_winscale && (so->so_rcv.sb_hiwat > TCP_MAXWIN || 
	     so->so_snd.sb_hiwat > TCP_MAXWIN) ) {
		tp->t_flags |= TF_REQ_SCALE;
		if ( tcp_tsecho ) {
			if ((offer == 0) || 
			    (offer && (tp->t_flags & TF_RCVD_TSTMP))) {
				tp->t_flags |= TF_REQ_TSTMP;
			}
		}
		
		/* Compute scaling value we'll need.
		 */
		tp->request_r_scale = 0;
		while ( tp->request_r_scale < TCP_MAX_WINSHIFT &&
		  TCP_MAXWIN<<tp->request_r_scale<so->so_rcv.sb_hiwat )
			tp->request_r_scale++;
#if 0
		if (tp->request_r_scale == 0) {
			tp->t_flags &= ~TF_REQ_SCALE;
		}
#endif
	}
	ROUTE_UNLOCK();

#ifdef RTV_MTU	/* if route characteristics exist ... */
	/*
	 * While we're here, check if there's an initial rtt
	 * or rttvar.  Convert from the route-table units
	 * to scaled multiples of the slow timeout timer.
	 */
	if (tp->t_srtt == 0 && (rtt = rt->rt_rmx.rmx_rtt)) {
		if (rt->rt_rmx.rmx_locks & RTV_MTU)
			tp->t_rttmin = rtt / (RTM_RTTUNIT / PR_FASTHZ);
		tp->t_srtt = rtt / (RTM_RTTUNIT / (PR_FASTHZ * TCP_RTT_SCALE));
		if (rt->rt_rmx.rmx_rttvar)
			tp->t_rttvar = rt->rt_rmx.rmx_rttvar /
			    (RTM_RTTUNIT / (PR_FASTHZ * TCP_RTTVAR_SCALE));
		else
			/* default variation is +- 1 rtt */
			tp->t_rttvar =
			    tp->t_srtt * TCP_RTTVAR_SCALE / TCP_RTT_SCALE;
		TCPT_RANGESET(tp->t_rxtcur,
		    ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1,
		    tp->t_rttmin, TCPTV_REXMTMAX);
	}
	/*
	 * if there's an mtu associated with the route, use it
	 */
	if (rt->rt_rmx.rmx_mtu) {
#ifdef INET6
		if (inp->inp_flags & INP_COMPATV4)
			mss = rt->rt_rmx.rmx_mtu - sizeof(struct tcpiphdr);
		else
			mss = rt->rt_rmx.rmx_mtu - sizeof(struct tcpip6hdr);
#else
		mss = rt->rt_rmx.rmx_mtu - sizeof(struct tcpiphdr);
#endif
	} else
#endif /* RTV_MTU */
	{
#ifdef INET6
		if (inp->inp_flags & INP_COMPATV4)
			mss = ifp->if_mtu - sizeof(struct tcpiphdr);
		else	
			mss = ifp->if_mtu - sizeof(struct tcpip6hdr);
#else
		mss = ifp->if_mtu - sizeof(struct tcpiphdr);
#endif
		if (!tcp_mtudisc && !in_localaddr(inp->inp_faddr)) {
			mss = MIN(mss, tcp_mssdflt);
		}
	}
	/*
	 * The current mss, t_maxseg, is initialized to the default value.
	 * If we compute a smaller value, reduce the current mss.
	 * If we compute a larger value, return it for use in sending
	 * a max seg size option, but don't store it for use
	 * unless we received an offer at least that large from peer.
	 * However, do not accept offers under 32 bytes.
	 */
	if (offer)
		mss = MIN(mss, offer);
	mss = MAX(mss, 32);		/* sanity */

	/* At this point, we've computed the mss we should advertise to
	 * the peer in the max seg size option.  Now we will reduce the
	 * mss by the space needed for RFC 1323 options and then further
	 * reduce it for efficiency (make it a page-size multiple if it's
	 * big).
	 */
	mss_advertise = mss;
	if ( offer == 0 && (tp->t_flags & TF_REQ_TSTMP) ||
	     offer != 0 && (tp->t_flags & TF_RCVD_TSTMP) ) {
		mss -= TCPOLEN_TSTAMP_HDR;
	}

	/* XXX hack for 16K pagesize machines to do FDDI packets in 4K chunks,
	 * so that our 32-bit machines have speedy reception.  For some
	 * reason, this improves FDDI performance between IP26's as well.
	 */
#if MCLBYTES > 4096
	if (mss >= 4096 && mss < 8192)
		mss = 4096;
	else
#endif
	if (mss > MCLBYTES)
		mss &= ~(MCLBYTES-1);

	if (mss < tp->t_maxseg || offer != 0) {
		/*
		 * If there's a pipesize, change the socket buffer
		 * to that size.  Make the socket buffers an integral
		 * number of mss units; if the mss is larger than
		 * the socket buffer, decrease the mss.
		 */
#ifdef RTV_SPIPE
		if ((bufsize = rt->rt_rmx.rmx_sendpipe) == 0)
#endif
			bufsize = so->so_snd.sb_hiwat;
		if (bufsize < mss)
			mss = bufsize;
		else {
			bufsize = bufsize / mss * mss;
			(void) sbreserve(&so->so_snd, bufsize);
		}
		tp->t_maxseg = mss;
		tp->t_maxseg0 = mss;

#ifdef RTV_RPIPE
		if ((bufsize = rt->rt_rmx.rmx_recvpipe) == 0)
#endif
			bufsize = so->so_rcv.sb_hiwat;
		if (bufsize > mss) {
			bufsize = bufsize / mss * mss;
			(void) sbreserve(&so->so_rcv, bufsize);
		}
	}
	tp->snd_cwnd = mss;

#ifdef RTV_SSTHRESH
	if (rt->rt_rmx.rmx_ssthresh) {
		/*
		 * There's some sort of gateway or interface
		 * buffer limit on the path.  Use this to set
		 * the slow start threshhold, but set the
		 * threshold to no less than 2*mss.
		 */
		tp->snd_ssthresh = MAX(2 * mss, rt->rt_rmx.rmx_ssthresh);
	}
#endif /* RTV_MTU */
	return (mss_advertise);
}

#ifndef BUCKET_INVAL
#define BUCKET_INVAL           ((u_short)-1)
#endif
/*
 * Re-start a TIME-WAIT connection
 */
void
tcp_tw_restart(struct inpcb *inp)
{
	ASSERT(inp);
	INHHEAD_LOCK(inp->inp_hhead);
	remque(inp);

	ASSERT(inp->inp_hashval != BUCKET_INVAL);
	insque(inp, _TBLELEMTAIL(inp));

	INHHEAD_UNLOCK(inp->inp_hhead);
	return;
}
