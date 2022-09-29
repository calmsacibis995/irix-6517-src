#ifdef INET6
/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)tcp_usrreq.c	8.2 (Berkeley) 1/3/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ipsec.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/in6_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip6_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp6_var.h>
#include <netinet/tcp_debug.h>
#include <sys/tcpipstats.h>

/*
 * Process a TCP user request for TCP tb.  If this is a send request
 * then m is the mbuf chain of send data.  If this is a timer expiration
 * (called from the software clock routine), then timertype tells which timer.
 */
/*ARGSUSED*/
int
tcp6_usrreq(so, req, m, nam, control)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
{
	register struct inpcb *inp;
	register struct tcpcb *tp = 0;
	struct sockaddr_in6 *sin6p;
	struct sockaddr_in *sinp;
	int error = 0;
#ifdef DEBUG
	int ostate;
#endif
	extern int tcp_attach(struct socket *);

	ASSERT(SOCKET_ISLOCKED(so));
	if (req == PRU_CONTROL)
		return (in6_control(so, (__psint_t)m, (caddr_t)nam,
			(struct ifnet *)control));
	if (control && control->m_len) {
		m_freem(control);
		if (m)
			m_freem(m);
		return (EINVAL);
	}

	inp = sotoinpcb(so);
	/*
	 * When a TCP is attached to a socket, then there will be
	 * a (struct inpcb) pointed at by the socket, and this
	 * structure will point at a subsidary (struct tcpcb).
	 */
	if (inp == 0) {
		/*
		 * A socket without a PCB is valid only if this is an attach
		 * or a deferred close (detach with SS_CLOSING set).
		 */
		if (((req != PRU_ATTACH) && (req != PRU_DETACH)) ||
		  ((req == PRU_DETACH) && !(so->so_state & SS_CLOSING))) {
			if (m && (req == PRU_SEND || req == PRU_SENDOOB))
				m_freem(m);
			return (EINVAL);		/* XXX */
		}
	}
	if (inp) {
		tp = intotcpcb(inp);
		/* WHAT IF TP IS 0? */
#ifdef KPROF
		tcp_acounts[tp->t_state][req]++;
#endif
#ifdef DEBUG
		ostate = tp->t_state;
	} else
		ostate = 0;
#else /* DEBUG */
	}
#endif /* DEBUG */

	switch (req) {

	/*
	 * TCP attaches to socket via PRU_ATTACH, reserving space,
	 * and an internet control block.
	 */
	case PRU_ATTACH:
		if (inp) {
			error = EISCONN;
			break;
		}
		error = tcp_attach(so);
		if (error)
			break;
		((struct inpcb *) so->so_pcb)->inp_flags = INP_COMPATANY;
		if ((so->so_options & SO_LINGER) && so->so_linger == 0)
			so->so_linger = TCP_LINGERTIME * HZ;
		tp = sototcpcb(so);
		break;

	/*
	 * PRU_DETACH detaches the TCP protocol from the socket.
	 * If the protocol state is non-embryonic, then can't
	 * do this directly: have to initiate a PRU_DISCONNECT,
	 * which may finish later; embryonic TCB's can just
	 * be discarded here.
	 */
	case PRU_DETACH:
		if (tp->t_state > TCPS_LISTEN)
			tp = tcp_disconnect(tp, 0);
		else
			tp = tcp_close(tp);
		break;

	/*
	 * Give the socket an address.
	 */
	case PRU_BIND:
		/*
		 * Must check for multicast addresses and disallow binding
		 * to them.
		 */
		sin6p = mtod(nam, struct sockaddr_in6 *);
		if (sin6p->sin6_family == AF_INET6 &&
		    IS_MULTIADDR6(sin6p->sin6_addr)) {
			error = EAFNOSUPPORT;
			break;
		}
		sinp = mtod(nam, struct sockaddr_in *);
		if (sinp->sin_family == AF_INET &&
		    IN_MULTICAST(ntohl(sinp->sin_addr.s_addr))) {
			error = EAFNOSUPPORT;
			break;
		}
		error = in6_pcbbind(inp, nam);
		if (error)
			break;
		break;

	/*
	 * Prepare to accept connections.
	 */
	case PRU_LISTEN:
		if (inp->inp_lport == 0)
			error = in6_pcbbind(inp, NULL);
		if (error == 0)
			tp->t_state = TCPS_LISTEN;
		break;

	/*
	 * Initiate connection to peer.
	 * Create a template for use in transmissions on this connection.
	 * Enter SYN_SENT state, and mark socket as connecting.
	 * Start keep-alive timer, and seed output sequence space.
	 * Send initial segment on connection.
	 */
	case PRU_CONNECT:
		/*
		 * Must disallow TCP ``connections'' to multicast addresses.
		 */
		sin6p = mtod(nam, struct sockaddr_in6 *);
		if (sin6p->sin6_family == AF_INET6
		    && IS_MULTIADDR6(sin6p->sin6_addr)) {
			error = EAFNOSUPPORT;
			break;
		}
		sinp = mtod(nam, struct sockaddr_in *);
		if (sinp->sin_family == AF_INET
		    && IN_MULTICAST(ntohl(sinp->sin_addr.s_addr))) {
			error = EAFNOSUPPORT;
			break;
		}

		if ((error = tcp6_connect(tp, nam)) != 0)
			break;
		if (inp->inp_flags & INP_COMPATV4)
			error = tcp_output(tp, 0);
		else
			error = tcp6_output(tp);
		break;

	/*
	 * Create a TCP connection between two sockets.
	 */
	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	/*
	 * Initiate disconnect from peer.
	 * If connection never passed embryonic stage, just drop;
	 * else if don't need to let data drain, then can just drop anyways,
	 * else have to begin TCP shutdown process: mark socket disconnecting,
	 * drain unread data, state switch to reflect user close, and
	 * send segment (e.g. FIN) to peer.  Socket will be really disconnected
	 * when peer sends FIN and acks ours.
	 *
	 * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
	 */
	case PRU_DISCONNECT:
		tp = tcp_disconnect(tp, 0);
		break;

	/*
	 * Accept a connection.  Essentially all the work is
	 * done at higher levels; just return the address
	 * of the peer, storing through addr.
	 */
	case PRU_ACCEPT:
		if ((inp->inp_flags & INP_COMPATV6) == 0) {
			/* connected to an IPv4 peer */
			inp->inp_laddr6.s6_addr32[2] = htonl(0xffff);
		}
		in_setpeeraddr(inp, nam);
		break;

	/*
	 * Mark the connection as being incapable of further output.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		tp = tcp_usrclosed(tp);
		if (tp) {
			if (inp->inp_flags & INP_COMPATV4)
				error = tcp_output(tp, 0);
			else
				error = tcp6_output(tp);
		}
		break;

	/*
	 * After a receive, possibly send window update to peer.
	 */
	case PRU_RCVD:
		if (inp->inp_flags & INP_COMPATV4)
			tcp_output(tp, 0);
		else
			tcp6_output(tp);
		break;

	/*
	 * Do a send by putting data in output queue and updating urgent
	 * marker if URG set.  Possibly send more data.
	 */
	case PRU_SEND:
		sbappend(&so->so_snd, m);
		if (inp->inp_flags & INP_COMPATV4)
			error = tcp_output(tp, 0);
		else
			error = tcp6_output(tp);
		break;

	/*
	 * Abort the TCP.
	 */
	case PRU_ABORT:
		tp = tcp_drop(tp, ECONNABORTED, 0);
		break;

	case PRU_SENSE:
		((struct stat *) m)->st_blksize = so->so_snd.sb_hiwat;
		return (0);

	case PRU_RCVOOB:
		if ((so->so_oobmark == 0 &&
		    (so->so_state & SS_RCVATMARK) == 0) ||
		    so->so_options & SO_OOBINLINE ||
		    tp->t_oobflags & TCPOOB_HADDATA) {
			error = EINVAL;
			break;
		}
		if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) {
			error = EWOULDBLOCK;
			break;
		}
		m->m_len = 1;
		*mtod(m, caddr_t) = tp->t_iobc;
		if (((__psint_t)nam & MSG_PEEK) == 0)
			tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		break;

	case PRU_SENDOOB:
		if (sbspace(&so->so_snd) < -512) {
			m_freem(m);
			error = ENOBUFS;
			break;
		}
		/*
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section.
		 * Otherwise, snd_up should be one lower.
		 */
		sbappend(&so->so_snd, m);
		tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
		tp->t_force = 1;
		if (inp->inp_flags & INP_COMPATV4)
			error = tcp_output(tp, 0);
		else
			error = tcp6_output(tp);
		tp->t_force = 0;
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, nam);
		break;

	/*
	 * TCP slow timer went off; going through this
	 * routine for tracing's sake.
	 */
	case PRU_SLOWTIMO:
		tp = tcp_timers(tp, (long)nam);
#ifdef TCPDEBUG
		req |= (int)nam << 8;		/* for debug's sake */
#endif
		break;

	case PRU_SOCKLABEL:
		error = EOPNOTSUPP;
		break;
	
	default:
		panic("tcp6_usrreq");
	}
#if defined(sgi) && defined(DEBUG)
	if (tp && (so->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, 0, req);
#endif
	return (error);
}

/*
 * Common subroutine to open a TCP connection to remote host specified
 * by struct sockaddr_in6 in mbuf *nam.  Call in6_pcbbind to assign a local
 * port number if needed.  Call in6_pcbladdr to do the routing and to choose
 * a local host address (interface).  If there is an existing incarnation
 * of the same connection in TIME-WAIT state and if the remote host was
 * sending CC options and if the connection duration was < MSL, then
 * truncate the previous TIME-WAIT state and proceed.
 * Initialize connection parameters and enter SYN-SENT state.
 */
int
tcp6_connect(tp, nam)
	register struct tcpcb *tp;
	struct mbuf *nam;
{
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
	int error;

	if (inp->inp_lport == 0) {
		error = in6_pcbbind(inp, NULL);
		if (error)
			return error;
	}

	error = in6_pcbconnect(inp, nam);
	if (error)
		return error;

	tcp_template(tp);

	/* Compute window scaling to request.  */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < so->so_rcv.sb_hiwat)
		tp->request_r_scale++;

	soisconnecting(so);
	TCPSTAT(tcps_connattempt);
	tp->t_state = TCPS_SYN_SENT;
	tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
	tp->iss = tcp_iss; tcp_iss += TCP_ISSINCR/2;
	tcp_sendseqinit(tp);

	return 0;
}

int
tcp6_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	int error = 0;
	struct inpcb *inp;
	register struct tcpcb *tp;
	register struct mbuf *m;

	ASSERT(SOCKET_ISLOCKED(so));
	inp = sotoinpcb(so);
	if (inp == NULL) {
		if (op == PRCO_SETOPT && *mp)
			(void) m_free(*mp);
		return (ECONNRESET);
	}
	if (level != IPPROTO_TCP) {
		error = ip6_ctloutput(op, so, level, optname, mp);
		return (error);
	}
	tp = intotcpcb(inp);

	switch (op) {

	case PRCO_SETOPT:
		m = *mp;
		switch (optname) {

		case TCP_NODELAY:
			if (m == NULL || m->m_len < sizeof (int))
				error = EINVAL;
			else if (*mtod(m, int *))
				tp->t_flags |= TF_NODELAY;
			else
				tp->t_flags &= ~TF_NODELAY;
			break;

		case TCP_MAXSEG:	/* not yet */
		default:
			error = ENOPROTOOPT;
			break;
		}
		if (m)
			(void) m_free(m);
		break;

	case PRCO_GETOPT:
		*mp = m = m_get(M_WAIT, MT_SOOPTS);
		m->m_len = sizeof(int);

		switch (optname) {
		case TCP_NODELAY:
			*mtod(m, int *) = tp->t_flags & TF_NODELAY;
			break;
		case TCP_MAXSEG:
			*mtod(m, int *) = tp->t_maxseg;
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}
#endif /* INET6 */
