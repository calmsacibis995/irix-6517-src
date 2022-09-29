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
 *	@(#)tcp_output.c	7.21 (Berkeley) 6/28/90
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/tcpipstats.h"
#include "sys/errno.h"
#include "sys/sesmgr.h"
#include "net/if.h"
#include "net/route.h"

#define	max_linkhdr	48		/* XXX belongs elsewhere */
#define	DATASPACE (MMAXOFF - (MMINOFF + max_linkhdr + sizeof (struct tcpiphdr)))
#define TCP_MAX_OPTIONSPC   40		/* max # bytes that go in options */

#if _MIPS_SZPTR == 32 
#define TCP_OPTION_OFLOW
#endif

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "ip_var.h"
#include "tcp.h"
#define	TCPOUTFLAGS
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "tcp_debug.h"

/*
 * Initial options.
 */
u_char	tcp_initopt[4] = { TCPOPT_MAXSEG, 4, 0x0, 0x0, };

extern int tcp_mtudisc;
extern int tcp_mssdflt;
extern int swipeflag;	/* true if swipe is on */

void tcp_setpersist(struct tcpcb *tp);		/* Forward reference */
extern void tcp_quench( struct inpcb *);

/*
 * Tcp output routine: figure out what should be sent and send it.
 */
tcp_output(register struct tcpcb *tp, struct ipsec *ipsec)
{
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
	register int len, fulllen, win, curlen;
	int off, flags, error;
	register struct mbuf *m;
#ifdef TCP_OPTION_OFLOW
	struct mbuf *m_opt = NULL;
#else
#define m_opt  m
#endif
	register struct tcpiphdr *ti;
	unsigned optlen, hdrlen;
	u_char *opt;
	int idle, sendalot;
	int do_fastpath = 0;
	int do_ts = 0;
	struct ifnet *ifp;
#ifdef INET6
	extern int tcp6_output(struct tcpcb *tp);
#endif

	ASSERT(SOCKET_ISLOCKED(so));
#ifdef INET6
	if ((tp->t_inpcb->inp_flags & INP_COMPATV4) == 0)
		(void) tcp6_output(tp);
#endif
	/*
	 * Determine length of data that should be transmitted,
	 * and flags that will be used.
	 * If there is some data or critical controls (SYN, RST)
	 * to send, then transmit; otherwise, investigate further.
	 */
	idle = (tp->snd_max == tp->snd_una);
	if (idle && tp->t_idle >= tp->t_rxtcur)
		/*
		 * We have been idle for "a while" and no acks are
		 * expected to clock out any data we send --
		 * slow start to get ack "clock" running again.
		 */
		tp->snd_cwnd = tp->t_maxseg;
again:
	sendalot = 0;
	off = tp->snd_nxt - tp->snd_una;
	win = MIN(tp->snd_wnd, tp->snd_cwnd);

	/*
	 * If in persist timeout with window of 0, send 1 byte.
	 * Otherwise, if window is small but nonzero
	 * and timer expired, we will send what we can
	 * and go to transmit state.
	 */
	flags = tcp_outflags[tp->t_state];
	if (tp->t_force) {
		if (win == 0) {
			/*
			 * If we still have some data to send, then
			 * clear the FIN bit.  Usually this would
			 * happen below when it realizes that we
			 * aren't sending all the data.  However,
			 * if we have exactly 1 byte of unset data,
			 * then it won't clear the FIN bit below,
			 * and if we are in persist state, we wind
			 * up sending the packet without recording
			 * that we sent the FIN bit.
			 *
			 * We can't just blindly clear the FIN bit,
			 * because if we don't have any more data
			 * to send then the probe will be the FIN
			 * itself.
			 */
			if (off < so->so_snd.sb_cc)
				flags &= ~TH_FIN;
			win = 1;
		} else {
			tp->t_timer[TCPT_PERSIST] = 0;
			tp->t_rxtshift = 0;
		}
	}

	NETPAR(NETSCHED, NETEVENTKN, (short)(tp->snd_nxt),
		 NETEVENT_TCPDOWN, NETCNT_NULL, NETRES_PROTCALL);
	len = MIN(so->so_snd.sb_cc, win) - off;
	if (len < 0) {
		/*
		 * If FIN has been sent but not acked,
		 * but we haven't been called to retransmit,
		 * len will be -1.  Otherwise, window shrank
		 * after we sent into it.  If window shrank to 0,
		 * cancel pending retransmit, pull snd_nxt back
		 * to (closed) window, and set the persist timer
		 * if it isn't already going.  If the window didn't
		 * close completely, just wait for an ACK.
		 */
		len = 0;
		if (win == 0) {
			tp->t_timer[TCPT_REXMT] = 0;
			tp->t_rxtshift = 0;
			tp->snd_nxt = tp->snd_una;
			if (tp->t_timer[TCPT_PERSIST] == 0)
				tcp_setpersist(tp);
		}
	}
	if (len > tp->t_maxseg) {
		len = tp->t_maxseg;
		sendalot = 1;
	}

	/* Fix for PV # 583284 - need to consider IP and TCP option lengths */
	if (len > tp->t_maxseg - 80 && (inp->inp_options || sesmgr_enabled || 
		SACK_RCVNUM(tp))) {
		#pragma mips_frequency_hint NEVER
		int count = 0;
		if (sesmgr_enabled)
			count = 40;	
		else if (inp->inp_options)
			count = inp->inp_options->m_len - sizeof(struct in_addr); 
		if (SACK_RCVNUM(tp))
			count += 4 + SACK_RCVNUM(tp) * 8;
		if (len + count > tp->t_maxseg) {
			len = len - count;
			if (len < 0) {
				error = EINVAL;
				goto out;
			}
			sendalot = 1;
		}
	}

	/* Should be here after the len in determined.
	   it is going to send [snd_una, snd_una+len], if
	   the packet falls in the sack_list, advance snd_nxt without
	   really sending the data.  When snd_nxt is advanced, the off
	   will be advanced, and snd.sb_mb+off will be advanced.  Note
	   that this does not mean that we can discard sack'd data -
	   we must wait for the data to be acked by normal means as
	   sack is non-committal 
	*/
	if (len && tp->t_flags & TF_SNDCHECK_SACK && SACK_SNDHOLES(tp)) {
		tcp_seq start;
		start = tp->snd_una + off;
		if (sack_sndcheck(tp, start, start+len)) {
			SACK_PRINTF(("***** tcp_output:snder skip seq=%x, len=%d, off = %d\n", start, len, off));
			tp->snd_nxt += len;
			if (sendalot)
				goto again;
			else {
				tp->t_flags &= ~TF_SNDCHECK_SACK;
				return(0);
			}
		}
#ifdef SACK_DEBUG
		else printf("===== snder NO skip seq = %x, end=%x, len=%d off=%d\n", start, start+len, len, off);
#endif
	}


	/*
	 * Keep page-aligned data page-aligned as it goes out to
	 * the driver and the network. Don't send part of a
	 * page-sized mbuf, or an mbuf preceded by a page-sized
	 * mbuf.
	 */
	fulllen = len;
	if (tp->t_maxseg % NBPP == 0 && off < so->so_snd.sb_cc &&
	    (m = so->so_snd.sb_mb))
	{
		curlen = 0;
		while (m && curlen < off) {
			curlen += m->m_len;
			m = m->m_next;
		}
		curlen -= off;
		if (curlen < 0)
			goto splitlen_skip;
		while (m && curlen < len) {
			if ((m->m_len == NBPP) && (curlen + NBPP > len))
				break;
			curlen += m->m_len;
			if (curlen >= len)
				break;
			if (m->m_next == NULL)
				break;
			if ((m->m_len == NBPP) && (m->m_next->m_len != NBPP))
				break;
			m = m->m_next;
		}
		if (0 < curlen && curlen < len)
			len = curlen;
	}
	splitlen_skip:
	if (SEQ_LT(tp->snd_nxt + len, tp->snd_una + so->so_snd.sb_cc))
		flags &= ~TH_FIN;

	win = sbspace(&so->so_rcv);

	/*
	 * Sender silly window avoidance.  If connection is idle
	 * and can send all data, a maximum segment,
	 * at least a maximum default-size segment do it,
	 * or are forced, do it; otherwise don't bother.
	 * If peer's buffer is tiny, then send
	 * when window is at least half open.
	 * If retransmitting (possibly after persist timer forced us
	 * to send into a small window), then must resend.
	 */
	if (len) {
		if (fulllen == tp->t_maxseg)
			goto send;
		if ((idle || tp->t_flags & TF_NODELAY || !(off % tp->t_maxseg))
		    && fulllen + off >= so->so_snd.sb_cc)
			goto send;
		if (tp->t_force)
			goto send;
		if (fulllen >= tp->max_sndwnd / 2)
			goto send;
		if (SEQ_LT(tp->snd_nxt, tp->snd_max))
			goto send;
	}

	/*
	 * Compare available window to amount of window
	 * known to peer (as advertised window less
	 * next expected input).  If the difference is at least two
	 * max size segments, or at least 50% of the maximum possible
	 * window, then want to send a window update to peer.
	 */
	if (win > 0) {
		extern int soreceive_alt;
		/* adv is the amount we can increase the window,
		 * taking into account that we are limited by
		 * (TCP_MAXWIN << tp->rcv_scale)&(-MCLBYTES)
		 */
#if (-MCLBYTES & MCLBYTES) != MCLBYTES
#error "urgh error MCLBYTES is assumed to be a power of 2."
#endif
		int adv =
		  MIN(win,((int)TCP_MAXWIN<<tp->rcv_scale)&(-MCLBYTES)) -
			(tp->rcv_adv - tp->rcv_nxt);
		/* 
		 * Fix from Dave Cornelius (dc@ncd.com) 8/22/90:
		 *  if adv is coerced to unsigned in these comparisons and
		 *  adv is negative, the wrong branch would be taken.
		 */
		if (adv >= (int)((((tp->t_flags & TF_AGGRESSIVE_ACK) ||
				!soreceive_alt) ? 2 : 8) * tp->t_maxseg))
			goto send;
		if (2 * adv >= (int)so->so_rcv.sb_hiwat)
			goto send;
	}

	/*
	 * Send if we owe peer an ACK.
	 */
	if (tp->t_flags & TF_ACKNOW)
		goto send;
	if (flags & (TH_SYN|TH_RST))
		goto send;
	if (SEQ_GT(tp->snd_up, tp->snd_una))
		goto send;
	/*
	 * If our state indicates that FIN should be sent
	 * and we have not yet done so, or we're retransmitting the FIN,
	 * then we need to send.
	 */
	if (flags & TH_FIN &&
	    ((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una))
		goto send;

	/*
	 * TCP window updates are not reliable, rather a polling protocol
	 * using ``persist'' packets is used to insure receipt of window
	 * updates.  The three ``states'' for the output side are:
	 *	idle			not doing retransmits or persists
	 *	persisting		to move a small or zero window
	 *	(re)transmitting	and thereby not persisting
	 *
	 * tp->t_timer[TCPT_PERSIST]
	 *	is set when we are in persist state.
	 * tp->t_force
	 *	is set when we are called to send a persist packet.
	 * tp->t_timer[TCPT_REXMT]
	 *	is set when we are retransmitting
	 * The output side is idle when both timers are zero.
	 *
	 * If send window is too small, there is data to transmit, and no
	 * retransmit or persist is pending, then go to persist state.
	 * If nothing happens soon, send when timer expires:
	 * if window is nonzero, transmit what we can,
	 * otherwise force out a byte.
	 */
	if (so->so_snd.sb_cc && tp->t_timer[TCPT_REXMT] == 0 &&
	    tp->t_timer[TCPT_PERSIST] == 0) {
		tp->t_rxtshift = 0;
		tcp_setpersist(tp);
	}

	/*
	 * No reason to send a segment, just return.
	 */
	NETPAR(NETFLOW, NETDROPTKN, (short)(tp->snd_nxt),
		 NETEVENT_TCPDOWN, fulllen, NETRES_NULL);
	return (0);

send:

	/*
	 * Grab an mbuf to hold TCP/IP headers + TCP options.
	 * Leave room for 48 bytes of a link level header for
	 * any devices that may need it.  We can't really change
	 * this value since we've always done it that way, and
	 * some 3rd party drivers may depend on having exactly
	 * that much space.  Of course this means they don't
	 * work with IP options in all cases. 
	 */
	hdrlen = sizeof (struct tcpiphdr);
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (m == NULL) {
		#pragma mips_frequency_hint NEVER
		error = ENOBUFS;
		goto out;
	}
	m->m_off = MMINOFF + max_linkhdr;
	m->m_len = hdrlen;
	m->m_next = NULL;
	m_opt = m;

	/* Write TCP options directly into mbuf */
	ti = mtod(m, struct tcpiphdr *);
	opt = (u_char *)(ti + 1); 
	optlen = 0;

        do_ts = (tp->t_flags&(TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
                ! (flags&TH_RST) && ((flags&(TH_SYN|TH_ACK))==TH_SYN ||
                (tp->t_flags&TF_RCVD_TSTMP));

	if (flags & TH_SYN)
		goto options_syn;

#ifdef TCP_OPTION_OFLOW 
	/* On 32 bit machines, it is possible that all our SACK
	 * options will not fit into one mbuf.  Check for this
	 * case and allocate a new mbuf if that is the case.
	 * Note that this shouldn't impact performance, as it
	 * should be very rare conditions under which we need to
	 * send more than 2 sack options
	 */
	if ((do_ts && SACK_RCVNUM(tp) > 1) ||
	    (!do_ts && SACK_RCVNUM(tp) > 2)) {
		#pragma mips_frequency_hint NEVER
		SACK_PRINTF(("sack option overflow detected, timestamps = %d\n", do_ts));
		MGETHDR(m_opt, M_DONTWAIT, MT_HEADER);	
		if (m_opt == NULL) {
			#pragma mips_frequency_hint NEVER
			error = ENOBUFS;
			goto out;
		}
		m_opt->m_off = MMINOFF;
		m_opt->m_len = 0;
		m->m_next = m_opt;
		opt = mtod(m_opt, u_char *);
	} 
#endif

	ASSERT(opt);

        /* ACK packet w/SACK
         * RFC: if not ack the highest on recvQ, all acks should include
         * SACK options.  SACK_RCVNUM > 0 ensures that we still have out
         * of sequence packets waiting for delivery to user
	 * Since broken implementations might depend on the position of
	 * SACK options with respect to timestamp, this may need to be
	 * moved
         */
        if (SACK_RCVNUM(tp)) {
                #pragma mips_frequency_hint NEVER
                int i,j;

                SACK_PRINTF(("tcp_output  tcpcb : %x flags: %x\n", tp, tp->t_flags));

                /* RFC2018: max 4 sack to be sent because options <=40
                   if we are using timestamps, max = 3
                */
                j = do_ts ? 3 : 4;
                j = (SACK_RCVNUM(tp) > j) ? j : SACK_RCVNUM(tp);
                if (j > 0) {
                        *( (u_int32_t *) (opt+optlen) ) = htonl(
                                TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_SACK<<8|2+(j<<3));
                        optlen += 4;
                        i = SACK_RCVCUR(tp);
#if 0
                        TCPSTAT_ADD(tcps_sacksent, j);
#endif
                }
#if 0
                else
                        TCPSTAT(tcps_sacksnotroom);
#endif
                for (; j>0; i = SACK_INCR(i)) {
                        if (SACK_FREE(SACK_RCV(tp,i)))
                                ASSERT(0);
                        SACK_PRINTF(("tcp_output   send SACK j = %d ofs = %d, num = %d, rcvcur=%d\n", j, optlen, i, SACK_RCVCUR(tp)));
                        *((u_int32_t *)(opt+optlen)) = htonl(SACK_RCV(tp,i).start);
                        *((u_int32_t *)(opt+optlen+4)) = htonl(SACK_RCV(tp,i).end);
                        optlen += 8;
                        j--;
                }
        }

options_syn:

        /*
         * Before ESTABLISHED, force sending of initial options
         * unless TCP set not to do any options.
	 */
	if (flags & TH_SYN && (tp->t_flags & TF_NOOPT) == 0) {
		/* ensure retransmitted SYN done correctly */
		tp->snd_nxt = tp->iss;
		/* Always send a MSS option.
		 */
		opt[0] = TCPOPT_MAXSEG;
		opt[1] = 4;
		*(u_short *)(opt + 2) = htons((u_short) tcp_mss(tp, 0));
		optlen = 4;

		/* Send a window scaling request if necessary.
		 */
		if ( tp->t_flags & TF_REQ_SCALE ) {
			*( (u_int32_t *) (opt+optlen) ) = htonl(
				TCPOPT_NOP<<24 |
			     TCPOPT_WINDOW<<16 |
			     TCPOLEN_WINDOW<<8 |
			     tp->request_r_scale );
			optlen += 4;
		}
		win = sbspace(&so->so_rcv); /* in case tcp_mss() changed it*/
	}

        /* Send a timestamp and echo-reply if:
         * This is a SYN and our side wants to use timestamps
         * (TF_REQ_TSTMP is set) or both our side and our peer have sent
         * timestamps in our SYN's.
         */
        if (do_ts) {

                /* Format timestamp option as shown in appendix A of RFC 1323.
                 */
                *( (u_int32_t *) (opt+optlen) ) = htonl( TCPOPT_TSTAMP_HDR );
                *( (u_int32_t *) (opt+optlen+4) ) = htonl( tcp_now );
                *( (u_int32_t *) (opt+optlen+8) ) = htonl( tp->ts_recent );
                optlen += TCPOLEN_TSTAMP_HDR;
        }

	/* Tack SACK permit option onto SYN packets */
	if (flags & TH_SYN && (tp->t_flags & TF_NOOPT) == 0) {
		*( (u_int32_t *) (opt+optlen) ) = htonl(TCPOPT_SACKPERMIT_HDR);
		optlen += 4;
		SACK_PRINTF(("tcp_output:SACKPERMIT tcpcb: %x  optlen = %d\n", tp, optlen));
	}

	m_opt->m_len += optlen;
	hdrlen += optlen;
	ASSERT(optlen <= TCP_MAX_OPTIONSPC && !(optlen &0x3));

        /*
         * Attach copy of data to be transmitted, any initialize
         * the header from the template for sends on this connection
         */

	if (len) {
		if (tp->t_force && len == 1)
			TCPSTAT(tcps_sndprobe);
		else if (SEQ_LT(tp->snd_nxt, tp->snd_max)) {
			TCPSTAT(tcps_sndrexmitpack);
			TCPSTAT_ADD(tcps_sndrexmitbyte, len);
		} else {
			TCPSTAT(tcps_sndpack);
			TCPSTAT_ADD(tcps_sndbyte, len);
		}

		if (len <= (MMAXOFF - m_opt->m_off - m_opt->m_len)) {
			(void)m_datacopy(so->so_snd.sb_mb, off, len,
			    mtod(m_opt, caddr_t) + m_opt->m_len);
			m_opt->m_len += len;
		} else {
			m_opt->m_next = m_copy(so->so_snd.sb_mb, off, (int) len);
			if (m_opt->m_next == 0)
				fulllen = len = 0;
		}
		/*
		 * If we're sending everything we've got, set PUSH.
		 * (This will keep happy those implementations which only
		 * give data to the user when a buffer fills or
		 * a PUSH comes in.)
		 */
		if (off + len == so->so_snd.sb_cc)
			flags |= TH_PUSH;
	} else {
		/*
		 * XXX Use the ack field of the TCP/IP header template
		 * to count the number of zero length segments sent (normally
		 * acks).  To allow us to implement a kind of rate limiting
		 * algorithm.  See bug 623309.
		 */
		tp->t_template.ti_ack += 1;
		if (tp->t_flags & TF_ACKNOW) {
			TCPSTAT(tcps_sndacks);
		} else if (flags & (TH_SYN|TH_FIN|TH_RST))
			TCPSTAT(tcps_sndctrl);
		else if (SEQ_GT(tp->snd_up, tp->snd_una))
			TCPSTAT(tcps_sndurg);
		else
			TCPSTAT(tcps_sndwinup);
	}

	bcopy((caddr_t)&tp->t_template, (caddr_t)ti, sizeof (struct tcpiphdr));

	ASSERT(m->m_off + m->m_len <= MMAXOFF);
	ASSERT(m_opt->m_off + m_opt->m_len <= MMAXOFF); 

	/*
	 * Fill in fields, remembering maximum advertised
	 * window for use in delaying messages about window sizes.
	 * If resending a FIN, be sure not to use a new sequence number.
	 */
	if (flags & TH_FIN && tp->t_flags & TF_SENTFIN && 
	    tp->snd_nxt == tp->snd_max)
		tp->snd_nxt--;
	/*
	 * If we are doing retransmissions, then snd_nxt will
	 * not reflect the first unsent octet.  For ACK only
	 * packets, we do not want the sequence number of the
	 * retransmitted packet, we want the sequence number
	 * of the next unsent octet.  So, if there is no data
	 * (and no SYN or FIN), use snd_max instead of snd_nxt
	 * when filling in ti_seq.  But if we are in persist
	 * state, snd_max might reflect one byte beyond the
	 * right edge of the window, so use snd_nxt in that
	 * case, since we know we aren't doing a retransmission.
	 * (retransmit and persist are mutally exclusive...)
	 */
	if (len || (flags & (TH_SYN | TH_FIN)) || tp->t_timer[TCPT_PERSIST])
		ti->ti_seq = htonl(tp->snd_nxt);
	else
		ti->ti_seq = htonl(tp->snd_max);
	ti->ti_ack = htonl(tp->rcv_nxt);
	ti->ti_flags = flags;
        ti->ti_off = (sizeof (struct tcphdr) + optlen) >> 2;
	/*
	 * Calculate receive window.  Don't shrink window,
	 * but avoid silly window syndrome.
	 */
	if (win < (int)(so->so_rcv.sb_hiwat / 4) && win < (int)tp->t_maxseg)
		win = 0;
	if (win > (((int)TCP_MAXWIN<<tp->rcv_scale)&(-MCLBYTES)))
		win = ((int)TCP_MAXWIN<<tp->rcv_scale)&(-MCLBYTES);
	if (win < (int)(tp->rcv_adv - tp->rcv_nxt))
		win = (int)(tp->rcv_adv - tp->rcv_nxt);
	ti->ti_win = htons( (u_short) (win>>tp->rcv_scale) );
	if (SEQ_GT(tp->snd_up, tp->snd_nxt)) {
		ti->ti_urp = htons((u_short)(tp->snd_up - tp->snd_nxt));
		ti->ti_flags |= TH_URG;
	} else
		/*
		 * If no urgent pointer to send, then we pull
		 * the urgent pointer to the left edge of the send window
		 * so that it doesn't drift into the send window on sequence
		 * number wraparound.
		 */
		tp->snd_up = tp->snd_una;		/* drag it along */

	/*
	 * Put TCP length in extended header, and then
	 * checksum extended header and data.
	 */
	if (len + optlen)
		ti->ti_len = htons((u_short)(sizeof (struct tcphdr) +
		    optlen + len));

	GOODMP(m);
	GOODMP(m_opt);
	GOODMT(m->m_type);
	GOODMT(m_opt->m_type);

	/*
	 * Let the link layer compute the checksum if it wants
	 */
	{
		struct rtentry *rt;

		if (0 != (rt = inp->inp_route.ro_rt)
		    && 0 != (ifp = rt->rt_ifp)
		    && 0 != (rt->rt_flags & RTF_UP)
		    && (((struct sockaddr_in *)&inp->inp_route.ro_dst
			 )->sin_addr.s_addr == ti->ti_dst.s_addr)
		    && (hdrlen + len) <= ifp->if_mtu) {
			if ((0 != (ifp->if_flags & IFF_CKSUM)) ||
			    (0 != (rt->rt_flags & RTF_CKSUM))) {
				ti->ti_sum = -1;
				m->m_flags |= M_CKSUMMED;
			} else {
				ti->ti_sum = in_cksum(m, (int)(hdrlen + len));
			}
			if (inp->inp_options == 0 &&
			    !(so->so_options & SO_DONTROUTE) &&
			    !sesmgr_enabled) {
				do_fastpath = 1;
			} else {
				do_fastpath = 0;
			}
		} else {
			do_fastpath = 0;
			ti->ti_sum = in_cksum(m, (int)(hdrlen + len));
		}
	}

	/*
	 * In transmit state, time the transmission and arrange for
	 * the retransmit.  In persist state, just set snd_max.
	 */
	if (tp->t_force == 0 || tp->t_timer[TCPT_PERSIST] == 0) {
		tcp_seq startseq = tp->snd_nxt;

		/*
		 * Advance snd_nxt over sequence space of this segment.
		 */
		if (flags & (TH_SYN|TH_FIN)) {
			if (flags & TH_SYN)
				tp->snd_nxt++;
			if (flags & TH_FIN) {
				tp->snd_nxt++;
				tp->t_flags |= TF_SENTFIN;
			}
		}
		tp->snd_nxt += len;
		if (SEQ_GT(tp->snd_nxt, tp->snd_max)) {
			tp->snd_max = tp->snd_nxt;
			/*
			 * Time this transmission if not a retransmission and
			 * not currently timing anything.
			 */
			if (tp->t_rtt == 0) {
				tp->t_rtt = 1;
				tp->t_rtseq = startseq;
				TCPSTAT(tcps_segstimed);
			}
			/*
			 * If this is not a retransmission and we aren't 
			 * already measuring a high resolution RTT, then 
			 * timestamp this transmission (_if_ the option
			 * is set).
			 */
			if(SKO_FMTHRESH(tp) && SKO_TIMESPEC(tp).tv_sec == 0) {
				nanotime(&(SKO_TIMESPEC(tp)));
				SKO_TS_SEQ(tp) = tp->snd_max;
			}
		}

		/*
		 * Set retransmit timer if not currently set,
		 * and not doing an ack or a keep-alive probe.
		 * Initial value for retransmit timer is smoothed
		 * round-trip time + 2 * round-trip time variance.
		 * Initialize shift counter which is used for backoff
		 * of retransmit time.
		 */
		if (tp->t_timer[TCPT_REXMT] == 0 &&
		    tp->snd_nxt != tp->snd_una) {
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
			if (tp->t_timer[TCPT_PERSIST]) {
				tp->t_timer[TCPT_PERSIST] = 0;
				tp->t_rxtshift = 0;
			}
		}
	} else
		if (SEQ_GT(tp->snd_nxt + len, tp->snd_max))
			tp->snd_max = tp->snd_nxt + len;

	/*
	 * Trace.
	 */
#ifdef DEBUG
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_OUTPUT, tp->t_state, tp, ti, 0);
#endif

	/*
	 * Fill in IP length and desired time to live and
	 * send to IP level.  There should be a better way
	 * to handle ttl and tos; we could keep them in
	 * the template, but need a way to checksum without them.
	 */
	((struct ip *)ti)->ip_len = hdrlen + len;
	((struct ip *)ti)->ip_ttl = inp->inp_ip_ttl;
	((struct ip *)ti)->ip_tos = inp->inp_ip_tos;
	if ( tcp_mtudisc && tp->t_maxseg > tcp_mssdflt )
		((struct ip *)ti)->ip_off = IP_DF;

	if (do_fastpath && !swipeflag) {
		struct ip *ip = (struct ip *)ti;
		struct sockaddr *dst;
		__uint32_t cksum;

	        (*(char *)ip) = 0x45;
		ip->ip_id = htons(atomicAddIntHot(&ip_id, 1));
		HTONS(ip->ip_off);
		HTONS(ip->ip_len);
#define ckp ((ushort*)ip)
		cksum = (ckp[0] + ckp[1] + ckp[2] + ckp[3] + ckp[4]
			+ ckp[6] + ckp[7] + ckp[8] + ckp[9]);
		cksum = (cksum & 0xffff) + (cksum >> 16);
		cksum = (cksum & 0xffff) + (cksum >> 16);
		ip->ip_sum = cksum ^ 0xffff;
		IPSTAT(ips_localout);
		IFNET_UPPERLOCK(ifp);
		dst = (inp->inp_route.ro_rt->rt_flags & RTF_GATEWAY) ?
			inp->inp_route.ro_rt->rt_gateway :
			&inp->inp_route.ro_dst;
		error = (*ifp->if_output)(ifp, m, dst, inp->inp_route.ro_rt);
		IFNET_UPPERUNLOCK(ifp);
	} else {
		error = ip_output(m, inp->inp_options, &inp->inp_route,
		    so->so_options & SO_DONTROUTE, (struct mbuf *)NULL, ipsec);
	}
	if (error) {
out:
		if (error == ENOBUFS) {
			#pragma mips_frequency_hint NEVER 
        		if (len)
                		NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
                		  NETEVENT_TCPDOWN, NETCNT_NULL, NETRES_MBUF);
			tcp_quench(tp->t_inpcb);
			return (0);
		}
		/*
		 * Fix for 416381; used to ignore errors in SYN_RCVD, which
		 * can congest busy servers.
		 */
		if (error == EHOSTUNREACH || error == ENETDOWN || 
		    error == ENETUNREACH) {
			if (tp->t_state >= TCPS_ESTABLISHED) {
				tp->t_softerror = error;
				return (0);
			} else if (tp->t_state == TCPS_SYN_RECEIVED) {
				/* help stop SYN bombing */
				tp->t_timer[TCPT_KEEP] = 1;
			}
		}
		return (error);
	}
	TCPSTAT(tcps_sndtotal);

	/*
	 * Data sent (as far as we can tell).
	 * If this advertises a larger window than any other segment,
	 * then remember the size of the advertised window.
	 * Any pending ACK has now been sent.
	 */
	if (win > 0 && SEQ_GT(tp->rcv_nxt+win, tp->rcv_adv))
		tp->rcv_adv = tp->rcv_nxt + win;
	tp->last_ack_sent = tp->rcv_nxt;
	tp->t_flags &= ~(TF_ACKNOW|TF_DELACK);
	if (sendalot || len < fulllen)
		goto again;
	/* After sending this window of data, turn off TF_SNDCHECK_SACK
	   to prevent check_sack from upper stream call such as sosend().
	   TF_SNDCHECK_SACK is turn on by tcp_input() when it
	   received an ACK with SACK set
	 */
	tp->t_flags &= ~ TF_SNDCHECK_SACK;

	return (0);

}

void
tcp_setpersist(register struct tcpcb *tp)
{
	register t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;

	if (tp->t_timer[TCPT_REXMT])
		panic("tcp_output REXMT");
	/*
	 * Start/restart persistance timer.
	 */
	TCPT_RANGESET(tp->t_timer[TCPT_PERSIST],
	    t * tcp_backoff[tp->t_rxtshift],
	    TCPTV_PERSMIN, TCPTV_PERSMAX);
	if (tp->t_rxtshift < TCP_MAXRXTSHIFT)
		tp->t_rxtshift++;
}
