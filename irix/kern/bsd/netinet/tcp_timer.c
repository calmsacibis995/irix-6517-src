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
 *	@(#)tcp_timer.c	7.18 (Berkeley) 6/28/90
 */

#include "tcp-param.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/debug.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/tcpipstats.h"
#include "sys/protosw.h"

#include "sys/errno.h"
#include "net/if.h"
#include "net/route.h"

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "ip_var.h"
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#ifdef INET6
#include <netinet/tcp6_var.h>
#endif

void tcp_zaptw(void);

int tcp_time_wait_check = 2;

extern int tcp_keepidle;	/* tunables */
extern int tcp_keepintvl;
extern int tcp_agak_hysteresis_hi;
int	tcp_maxidle;
/*
 * Fast timeout routine for processing delayed acks
 */

void
tcp_fasttimo(void)
{
	register struct inpcb *inp, *inpnxt;
	register struct tcpcb *tp;
	register struct socket *so;
	int hash, ehash;
	struct in_pcbhead *hinp;
	int havelock = 0;

	ehash = (tcb.inp_tablesz - 1) / 2; /* skip TIME-WAITers */
	for (hash = 1; hash <= ehash; hash++) {
resync:

	havelock = 1;
	hinp = &tcb.inp_table[hash];
	INHHEAD_LOCK(hinp);
	inp = hinp->hinp_next;
	if (inp != (struct inpcb *)hinp) {
		INPCB_HOLD(inp);
		for (; (inp != (struct inpcb *)hinp) &&
		       (inp->inp_hhead == hinp);
		       inp = inpnxt) {
			so = inp->inp_socket;
			inpnxt = inp->inp_next;
			INPCB_HOLD(inpnxt);
			INHHEAD_UNLOCK(hinp);
			SOCKET_LOCK(so);
			if ((tp = (struct tcpcb *)inp->inp_ppcb) &&
			  (tp->t_flags & TF_DELACK)) {
				tp->t_flags &= ~TF_DELACK;
				tp->t_flags |= TF_ACKNOW;
				TCPSTAT(tcps_delack);
				(void) tcp_output(tp, so->so_sesmgr_data);
			}
			/*
			 * If we are sending too many acks, switch off
			 * aggressive acking mode.  If we are sending too
			 * little, switch it on.
			 */
			if(tp) {
				if(tp->t_template.ti_ack > 
				    tcp_agak_hysteresis_hi) {
					tp->t_flags &= ~TF_AGGRESSIVE_ACK;
				} else if(tp->t_template.ti_ack <=
					TCP_AGAK_HYSTERESIS_LO) {
					tp->t_flags |= TF_AGGRESSIVE_ACK;
				}
				tp->t_template.ti_ack = 0;
				/*
				 * process rexmt timer here for better 
				 * granularity
				 */
				if (tp->t_timer[TCPT_REXMT] && 
				    --tp->t_timer[TCPT_REXMT] == 0) {
					(void)tcp_usrreq(tp->t_inpcb->inp_socket,
					    PRU_SLOWTIMO, (struct mbuf *)0,
					    (struct mbuf *)(NULL+TCPT_REXMT),
					    (struct mbuf *)0);
				}
			}
			if (inp->inp_next != 0 && 
			    (tp = (struct tcpcb *)inp->inp_ppcb)) {
				if (tp->t_rtt) {
					tp->t_rtt++;
				}
			}
			if (!INPCB_RELE(inp)) {
				SOCKET_UNLOCK(so);
			}
			INHHEAD_LOCK(hinp);
			if (inpnxt->inp_next == 0) {
				INHHEAD_UNLOCK(hinp);
				so = inpnxt->inp_socket;
				SOCKET_LOCK(so);
				if (!INPCB_RELE(inpnxt))
					SOCKET_UNLOCK(so);
				hash++;
				if (hash <= ehash) {
					goto resync;
				} else {
					return;
				}
			}
		}
		if (inp != (struct inpcb *)hinp) {
			so = inp->inp_socket;
			INHHEAD_UNLOCK(hinp);
			havelock = 0;
			SOCKET_LOCK(so);
			if (!INPCB_RELE(inp)) {
				SOCKET_UNLOCK(so);
			}
		}
	}
	if (havelock)
	        INHHEAD_UNLOCK(hinp);
	
	}	/* for */
}

/*
 * Tcp protocol timeout routine called every 500 ms.
 * Updates the timers in all active tcb's and
 * causes finite state machine actions if timers expire.
 */
void
tcp_slowtimo(void)
{
	register struct inpcb *ip, *ipnxt;
	register struct tcpcb *tp;
	register int i;
	register struct socket *so;
	int hash, ehash;
	struct in_pcbhead *hinp;
	static unsigned tcp_gen;

	tcp_maxidle = TCPTV_KEEPCNT * tcp_keepintvl * PR_SLOWHZ;

	/*
	 * Search through tcb's and update active timers.
	 */
	tcp_gen++;

	ehash = (tcb.inp_tablesz - 1) / 2;
	for (hash = 1; hash <= ehash; hash++) {
resync:

	hinp = &tcb.inp_table[hash];
	INHHEAD_LOCK(hinp);
	ip = hinp->hinp_next;
	if (ip == (struct inpcb *)hinp) {
		INHHEAD_UNLOCK(hinp);
		continue;	/* get next bucket */
	}
	INPCB_HOLD(ip);
	for (; (ip != (struct inpcb *)hinp) && (ip->inp_hhead == hinp);
	       ip = ipnxt) {

		so = ip->inp_socket;
		ipnxt = ip->inp_next;
		INPCB_HOLD(ipnxt);
		INHHEAD_UNLOCK(hinp)
		SOCKET_LOCK(so);
		tp = intotcpcb(ip);
		if (tp != 0 && tcp_gen != tp->t_tcpgen) {
		tp->t_tcpgen = tcp_gen;
		/* skip rexmt timer; now handled by fast timer */
		for (i = 1; i < TCPT_NTIMERS; i++) {
			if (tp->t_timer[i] && --tp->t_timer[i] == 0) {
#ifdef INET6
				if (ip->inp_flags & INP_COMPATV4)
				    (void)tcp_usrreq(tp->t_inpcb->inp_socket,
				     PRU_SLOWTIMO, (struct mbuf *)0,
				     (struct mbuf *)(NULL+i), (struct mbuf *)0);
				else
				    (void)tcp6_usrreq(tp->t_inpcb->inp_socket,
				     PRU_SLOWTIMO, (struct mbuf *)0,
				     (struct mbuf *)(NULL+i), (struct mbuf *)0);
#else
				(void) tcp_usrreq(tp->t_inpcb->inp_socket,
				    PRU_SLOWTIMO, (struct mbuf *)0,
				    (struct mbuf *)(NULL+i), (struct mbuf *)0);
#endif
				if (ip->inp_next == 0)
					goto tpgone;
			}
		}
		tp->t_idle++;
		if (tp->t_idle < 0)
			/* clamp near maximum short value */
			tp->t_idle = 0x7ff0;
		}
tpgone:
		if (!INPCB_RELE(ip)) {
			SOCKET_UNLOCK(so);
		}
		INHHEAD_LOCK(hinp);
		if (ipnxt->inp_next == 0) {
			so = ipnxt->inp_socket;
			INHHEAD_UNLOCK(hinp);
			SOCKET_LOCK(so);
			if (!INPCB_RELE(ipnxt))
				SOCKET_UNLOCK(so);
			hash++; 
			if (hash <= ehash) {
				goto resync;
			} else {
				goto out;
			}
		}
	}
	INHHEAD_UNLOCK(hinp);
	if (ip != (struct inpcb *)hinp) {
		so = ip->inp_socket;
		SOCKET_LOCK(so);
		if (!INPCB_RELE(ip)) {
			SOCKET_UNLOCK(so);
		}
	}
	}
out:
	/* XXX huy MP safe */
	tcp_iss += TCP_ISSINCR/PR_SLOWHZ;		/* increment iss */
	if ((int)tcp_iss < 0)
		tcp_iss = 0;				/* XXX */
#ifdef sgi /* RFC 1323 */
	tcp_now++;
#endif
	if ((tcp_gen % (PR_SLOWHZ * tcp_time_wait_check)) == 0) {
		tcp_zaptw();
	}
}

void
tcp_zaptw(void)
{
	register struct inpcb *ip, *ipnxt;
	register struct tcpcb *tp;
	register struct socket *so;
	int hash, shash;
	int finished = 0;
	struct in_pcbhead *hinp;

	shash = ((tcb.inp_tablesz - 1) / 2) + 1;
	for (hash = shash; hash < tcb.inp_tablesz; hash++) {
resync:

	hinp = &tcb.inp_table[hash];
	INHHEAD_LOCK(hinp);
	ip = hinp->hinp_next;
	if (ip == (struct inpcb *)hinp) {
		INHHEAD_UNLOCK(hinp);
		continue;
	}
	finished = 0;
	INPCB_HOLD(ip);
	for (; ip != (struct inpcb *)hinp; ip = ipnxt) {
		so = ip->inp_socket;
		ipnxt = ip->inp_next;
		INPCB_HOLD(ipnxt);
		INHHEAD_UNLOCK(hinp);
		SOCKET_LOCK(so);
		tp = intotcpcb(ip);
		if (tp != 0) {
		  if (time > tp->t_purgeat) {
#ifdef INET6
		       if (ip->inp_flags & INP_COMPATV4)
			    (void) tcp_usrreq(tp->t_inpcb->inp_socket,
				    PRU_SLOWTIMO, (struct mbuf *)0,
				    (struct mbuf *)(NULL+TCPT_2MSL),
				    (struct mbuf *)0);
		        else
			    (void) tcp6_usrreq(tp->t_inpcb->inp_socket,
				    PRU_SLOWTIMO, (struct mbuf *)0,
				    (struct mbuf *)(NULL+TCPT_2MSL),
				    (struct mbuf *)0);
#else
			(void) tcp_usrreq(tp->t_inpcb->inp_socket,
				    PRU_SLOWTIMO, (struct mbuf *)0,
				    (struct mbuf *)(NULL+TCPT_2MSL),
				    (struct mbuf *)0);
#endif
		  } else {
			finished = 1;
		  }
			
		}
		if (!INPCB_RELE(ip)) {
			SOCKET_UNLOCK(so);
		}
		INHHEAD_LOCK(hinp);
		if (ipnxt->inp_next == 0) {
			so = ipnxt->inp_socket;
			INHHEAD_UNLOCK(hinp);
			SOCKET_LOCK(so);
			if (!INPCB_RELE(ipnxt))
				SOCKET_UNLOCK(so);
			hash++;
			if (hash < tcb.inp_tablesz) {
				goto resync;
			} else {
				return;
			}
		} else if (finished) {
			INHHEAD_UNLOCK(hinp);
			if (ipnxt != (struct inpcb *)hinp) {
				so = ipnxt->inp_socket;
				SOCKET_LOCK(so);
				if (!INPCB_RELE(ipnxt)) {
					SOCKET_UNLOCK(so);
				}
			} else {
				INPCB_RELE(ipnxt);
			}
			hash++;
			if (hash < tcb.inp_tablesz) {
				goto resync;
			} else {
				return;
			}
		}
	}
	INHHEAD_UNLOCK(hinp);
	if (ip != (struct inpcb *)hinp) {
		so = ip->inp_socket;
		SOCKET_LOCK(so);
		if (!INPCB_RELE(ip)) {
			SOCKET_UNLOCK(so);
		}
	}
	}
}

/*
 * Cancel all timers for TCP tp.
 */
void
tcp_canceltimers(struct tcpcb *tp)
{
	register int i;

	for (i = 0; i < TCPT_NTIMERS; i++)
		tp->t_timer[i] = 0;
}

int	tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };

int tcp_totbackoff = 511;		/* sum of tcp_backoff[] array */

extern int tcp_maxpersistidle;

/*
 * TCP timer processing.
 */
struct tcpcb *
tcp_timers(struct tcpcb *tp, __psint_t timer)
{
	register int rexmt;

	switch (timer) {

	/*
	 * 2 MSL timeout in shutdown went off.  If we're closed but
	 * still waiting for peer to close and connection has been idle
	 * too long, or if 2MSL time is up from TIME_WAIT, delete connection
	 * control block.  Otherwise, check again in a bit.
	 */
	case TCPT_2MSL:
		if (tp->t_state != TCPS_TIME_WAIT &&
		    tp->t_idle <= tcp_maxidle)
			tp->t_timer[TCPT_2MSL] = tcp_keepintvl*PR_SLOWHZ;
		else
			tp = tcp_close(tp);
		break;

	/*
	 * Retransmission timer went off.  Message has not
	 * been acked within retransmit interval.  Back off
	 * to a longer retransmit interval and retransmit one segment.
	 */
	case TCPT_REXMT:
		/*
		 * Stop go-fast mode in case of loss; we can restart later.
		 */
		tp->t_flags &= ~TF_GOFAST;
		if (++tp->t_rxtshift > TCP_MAXRXTSHIFT) {
			tp->t_rxtshift = TCP_MAXRXTSHIFT;
			TCPSTAT(tcps_timeoutdrop);
			tp = tcp_drop(tp, tp->t_softerror ?
			    tp->t_softerror : ETIMEDOUT, NULL);
			break;
		}
		TCPSTAT(tcps_rexmttimeo);
		/* SCA -- fix 2 from Brakmo/Peterson */
		rexmt = (((tp->t_srtt >> TCP_RTT_SHIFT) +
			tp->t_rttvar)) *
			tcp_backoff[tp->t_rxtshift];
		TCPT_RANGESET(tp->t_rxtcur, rexmt,
		    tp->t_rttmin, TCPTV_REXMTMAX);
		tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
		/*
		 * If losing, let the lower level know and try for
		 * a better route.  Also, if we backed off this far,
		 * our srtt estimate is probably bogus.  Clobber it
		 * so we'll take the next rtt measurement as our srtt;
		 * move the current srtt into rttvar to keep the current
		 * retransmit times until then.
		 */
		if (tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) {
			in_losing(tp->t_inpcb);
			tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
			tp->t_srtt = 0;
		}
		tp->snd_nxt = tp->snd_una;
		/*
		 * If timing a segment in this window, stop the timer.
		 */
		tp->t_rtt = 0;
		/*
		 * Close the congestion window down to one segment
		 * (we'll open it by one segment for each ack we get).
		 * Since we probably have a window's worth of unacked
		 * data accumulated, this "slow start" keeps us from
		 * dumping all that data as back-to-back packets (which
		 * might overwhelm an intermediate gateway).
		 *
		 * There are two phases to the opening: Initially we
		 * open by one mss on each ack.  This makes the window
		 * size increase exponentially with time.  If the
		 * window is larger than the path can handle, this
		 * exponential growth results in dropped packet(s)
		 * almost immediately.  To get more time between 
		 * drops but still "push" the network to take advantage
		 * of improving conditions, we switch from exponential
		 * to linear window opening at some threshhold size.
		 * For a threshhold, we use half the current window
		 * size, truncated to a multiple of the mss.
		 *
		 * (the minimum cwnd that will give us exponential
		 * growth is 2 mss.  We don't allow the threshhold
		 * to go below this.)
		 */
		{
		u_int win = MIN(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
		if (win < 2)
			win = 2;
		tp->snd_cwnd = tp->t_maxseg;
		tp->snd_ssthresh = win * tp->t_maxseg;
		tp->t_dupacks = 0;
		}

		/* RFC: turn off all sacked holes since the timeroute may
		   indicate that the receiver has discarded the data for
		   lack of its rcv buffer.
		*/ 
		sack_sndcleanup(tp);
		tp->t_flags &= ~TF_SNDCHECK_SACK;

		(void) tcp_output(tp, tp->t_inpcb->inp_socket->so_sesmgr_data);
		break;

	/*
	 * Persistance timer into zero window.
	 * Force a byte to be output, if possible.
	 */
	case TCPT_PERSIST:
		TCPSTAT(tcps_persisttimeo);
		/*
		 * Hack: if the peer is dead/unreachable, we do not
		 * time out if the window is closed.  After a full
		 * backoff, drop the connection if the idle time
		 * (no responses to probes) reaches the maximum
		 * backoff that we would use if retransmitting.
		 */
		if (tp->t_rxtshift == TCP_MAXRXTSHIFT &&
		    (tp->t_idle >= (tcp_maxpersistidle*PR_SLOWHZ) ||
		    tp->t_idle >= TCP_REXMTVAL(tp) * tcp_totbackoff)) {
		        TCPSTAT(tcps_persistdrop);
		        tp = tcp_drop(tp, ETIMEDOUT, NULL);
		        break;
		}
		tcp_setpersist(tp);
		tp->t_force = 1;
		(void) tcp_output(tp, tp->t_inpcb->inp_socket->so_sesmgr_data);
		tp->t_force = 0;
		break;

	/*
	 * Keep-alive timer went off; send something
	 * or drop connection if idle for too long.
	 * We used to only process connections in states up to,
	 * and including CLOSING.  We now terminate everything to
	 * help servers dealing with lame HTTP clients.
	 */
	case TCPT_KEEP:
		TCPSTAT(tcps_keeptimeo);
		if (tp->t_state < TCPS_ESTABLISHED)
			goto dropit;
		if (tp->t_inpcb->inp_socket->so_options & SO_KEEPALIVE &&
		     (tp->t_state < TCPS_TIME_WAIT)) {
		    	if (tp->t_idle >= 
			    ((tcp_keepidle*PR_SLOWHZ) + tcp_maxidle))
				goto dropit;
		     	if (tp->t_state > TCPS_CLOSE_WAIT &&
			    (tp->t_flags & TF_SENTFIN))
				goto dropit;
			/*
			 * Send a packet designed to force a response
			 * if the peer is up and reachable:
			 * either an ACK if the connection is still alive,
			 * or an RST if the peer has closed the connection
			 * due to timeout or reboot.
			 * Using sequence number tp->snd_una-1
			 * causes the transmitted zero-length segment
			 * to lie outside the receive window;
			 * by the protocol spec, this requires the
			 * correspondent TCP to respond.
			 */
			TCPSTAT(tcps_keepprobe);

#ifdef INET6
			tcp_respond(tp, (struct tcpiphdr *)&tp->t_template,
			  (struct mbuf *)NULL,
#else
			tcp_respond(tp, &tp->t_template, (struct mbuf *)NULL,
#endif
			/*
			 * The keepalive packet must have nonzero length
			 * to get a 4.2 host to respond.
			 */
			    tp->rcv_nxt - 1,
			    tp->snd_una - 1,
#ifdef INET6
			    0, NULL, tp->t_inpcb->inp_flags & INP_COMPATV6 ?
			    AF_INET6 : AF_INET);
#else
			    0, tp->t_inpcb->inp_socket->so_sesmgr_data);
#endif

			tp->t_timer[TCPT_KEEP] = tcp_keepintvl*PR_SLOWHZ;
		} else
			tp->t_timer[TCPT_KEEP] = tcp_keepidle*PR_SLOWHZ;
		break;
	
	case TCPT_MTUEXP:
		/* Go back to mss that we started with at beginning
		 * of connection.
		 */
		tp->t_maxseg = tp->t_maxseg0;
		break;

	dropit:
		TCPSTAT(tcps_keepdrops);
		tp = tcp_drop(tp, ETIMEDOUT, NULL);
		break;
	}
	return (tp);
}
