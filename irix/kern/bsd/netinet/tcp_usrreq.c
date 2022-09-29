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
 *	@(#)tcp_usrreq.c	7.8 (Berkeley) 3/16/88
 */

#include "tcp-param.h"
#include "sys/debug.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/tcpipstats.h"
#include "sys/protosw.h"
#include "sys/errno.h"
#include "sys/stat.h"
#include "sys/vnode.h"
#include "sys/sesmgr.h"
#include "net/if.h"
#include "net/route.h"

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
#include "tcp_debug.h"
#include "sys/mac_label.h"

/*
 * TCP protocol interface to socket abstraction.
 */
extern	char *tcpstates[];
struct	tcpcb *tcp_newtcpcb(struct inpcb *);

extern int	tcp_winscale;	/* RFC 1323 (big windows) options control */
extern int	tcp_tsecho;

/*
 * Process a TCP user request for TCP tb.  If this is a send request
 * then m is the mbuf chain of send data.  If this is a timer expiration
 * (called from the software clock routine), then timertype tells which timer.
 */
tcp_usrreq(
	struct socket *so,
	int req,
	struct mbuf *m,
	struct mbuf *nam,	/* XXX sometimes really an "int" */
	struct mbuf *rights)
{
	register struct inpcb *inp;
	register struct tcpcb *tp = 0;
	int error = 0;
	struct ipsec * ipsec = (struct ipsec *)so->so_sesmgr_data;
#ifdef DEBUG
	int ostate;
#endif
	int tcp_attach(struct socket *);
	struct timespec tv;
	extern int tcp_pcbconnect(struct inpcb *, struct mbuf *);

	ASSERT(SOCKET_ISLOCKED(so));
	if (req == PRU_CONTROL)
		return (in_control(so, (__psint_t)m, (caddr_t)nam,
			(struct ifnet *)rights));
	if (rights && rights->m_len) {
		if (m) {
			m_freem(m);
		}
		/* rights freed in sendit() */
		return (EINVAL);
	}

	inp = sotoinpcb(so);
	/*
	 * When a TCP is attached to a socket, then there will be
	 * a (struct inpcb) pointed at by the socket, and this
	 * structure will point at a subsidiary (struct tcpcb).
	 */
	if (inp == 0) {
		/*
		 * A socket without a PCB is valid only if this is an attach
		 * or a deferred close (detach with SS_CLOSING set).
		 */
		if (((req != PRU_ATTACH) && (req != PRU_DETACH)) ||
		    ((req == PRU_DETACH) && !(so->so_state & SS_CLOSING))) {
			return (EINVAL);		/* XXX */
		}
	}
	if (inp) {
		tp = intotcpcb(inp);
		/* when tp == 0, there is no connection */
		if ((tp == 0) && ((req != PRU_DETACH) || ((req == PRU_DETACH) &&
		    !(so->so_state & SS_CLOSING)))) {
			return (EINVAL);
		}
#ifdef KPROF
		if (tp) {
			tcp_acounts[tp->t_state][req]++;
		}
#endif
#ifdef DEBUG
		if (tp) {
			ostate = tp->t_state;
		} else {
			ostate = 0;
		}
	} else
		ostate = 0;
#else /* DEBUG */
	}
#endif
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
#ifdef INET6
		((struct inpcb *)so->so_pcb)->inp_flags = INP_COMPATV4;
#endif
		if ((so->so_options & SO_LINGER) && so->so_linger == 0)
			so->so_linger = TCP_LINGERTIME;
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
		/*
		 * If we have a TCB, we can do the usual thing, otherwise we
		 * need to just do in_pcbdetach() if needed and then get
		 * out of here.
		 */
		if ((so->so_state & SS_CLOSING) && (tp == 0)) {
			/*
			 * Deferred close by sosend() or soreceive() because
			 * rtnetd could not wait for the user process to exit.
			 * The PCB's reference count has been left inflated,
			 * so it is safe to call in_pcbdetach() here.
			 */
			if (inp) {
				(void) in_pcbdetach(inp);
			}
			/* No sensible error return at this point */
			return 0;
		}
		if (tp->t_state > TCPS_LISTEN)
			tp = tcp_disconnect(tp, ipsec);
		else
			tp = tcp_close(tp);
		break;

	/*
	 * Give the socket an address.
	 */
	case PRU_BIND:
		error = in_pcbbind(inp, nam);
		break;

	/*
	 * Prepare to accept connections.
	 */
	case PRU_LISTEN:
	        if (so->so_state & SS_ISCONNECTED) {
		        error = EINVAL;
			break;
		}
		if (inp->inp_lport == 0)
			error = in_pcbbind(inp, (struct mbuf *)0);
		if (error == 0) {
			tp->t_state = TCPS_LISTEN;
			in_pcblisten(inp);
		}
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
		 * First check to see if a previous connect request
		 * was interrupted and returned EINTR.
		 */
		if(so->so_state & SS_CONNINTERRUPTED) {
			struct sockaddr_in *sin = 
				mtod(nam, struct sockaddr_in *);
			/*
			 * If connecting to the same address, then
			 * continue where we left off.
			 */
			if(nam->m_len != sizeof(struct sockaddr_in)) {
				error = EINVAL;
				break;
			}
			if(sin->sin_port == inp->inp_fport &&
			   sin->sin_addr.s_addr == inp->inp_faddr.s_addr) {
				so->so_state &= ~SS_CONNINTERRUPTED;
				break;
			}
			/*
			 * Otherwise, return EISCONN.
			 */
			error = EISCONN;
			break;
		}
		if (inp->inp_lport == 0) {
			error = tcp_pcbconnect(inp, nam);
			if (error)
				break;
		} else {
			if (inp->inp_hashflags & INPFLAGS_LISTEN) {
				in_pcbunlisten(inp);
			}
			error = in_pcbconnect(inp, nam);
		}
		if (error)
			break;
		tcp_template(tp);
		/* Do RFC 1323 window scaling if either of our send or
		 * receive buffer spaces are greater than 64K.
		 */
		if ( tcp_winscale && (so->so_rcv.sb_hiwat > TCP_MAXWIN || 
		     so->so_snd.sb_hiwat > TCP_MAXWIN) ) {
			tp->t_flags |= TF_REQ_SCALE;
			if ( tcp_tsecho )
				tp->t_flags |= TF_REQ_TSTMP;
			
			/* Compute scaling value we'll need.
			 */
			tp->request_r_scale = 0;
			while ( tp->request_r_scale < TCP_MAX_WINSHIFT &&
			  TCP_MAXWIN<<tp->request_r_scale<so->so_rcv.sb_hiwat )
				tp->request_r_scale++;
		}
		soisconnecting(so);
		TCPSTAT(tcps_connattempt);
		tp->t_state = TCPS_SYN_SENT;
		tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
		/* Make the least significant bits of the sequence number
		 * hard to predict to combat source address spoofing.
		 * One could use MD5 or something similar, but that would
		 * require a lot of CPU cycles and would not be any
		 * more secure against bad guys with source.
		 */
		nanotime(&tv);
		tp->iss = tcp_iss + tv.tv_nsec%(TCP_ISSINCR/8);
		tcp_iss += TCP_ISSINCR/2;
		tcp_sendseqinit(tp);
		error = tcp_output(tp, ipsec);
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
		tp = tcp_disconnect(tp, ipsec);
		break;

	/*
	 * Accept a connection.  Essentially all the work is
	 * done at higher levels; just return the address
	 * of the peer, storing through addr.
	 */
	case PRU_ACCEPT: {
		struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

		nam->m_len = sizeof (struct sockaddr_in);
		sin->sin_family = AF_INET;
		sin->sin_port = inp->inp_fport;
		sin->sin_addr = inp->inp_faddr;
		break;
		}

	/*
	 * Mark the connection as being incapable of further output.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		tp = tcp_usrclosed(tp);
		if (tp)
			error = tcp_output(tp, ipsec);
		break;

	/*
	 * After a receive, possibly send window update to peer.
	 */
	case PRU_RCVD:
		(void) tcp_output(tp, ipsec);
		break;

	/*
	 * Do a send by putting data in output queue and updating urgent
	 * marker if URG set.  Possibly send more data.
	 */
	case PRU_SEND:
		sbappend(&so->so_snd, m);
		error = tcp_output(tp, ipsec);
		break;

	/*
	 * Abort the TCP.
	 */
	case PRU_ABORT:
		tp = tcp_drop(tp, ECONNABORTED, ipsec);
		break;

	case PRU_SENSE:
		((struct vattr *) m)->va_blksize = so->so_snd.sb_hiwat;
		break;

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
		error = tcp_output(tp, ipsec);
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
		tp = tcp_timers(tp, (__psint_t)nam);
		req |= (__psint_t)nam << 8;		/* for debug's sake */
		break;

	case PRU_SOCKLABEL:
                if (!sesmgr_enabled)
			error = EOPNOTSUPP;
                else
                        sesmgr_set_label(inp->inp_socket,(mac_label *)nam);
		break;

	default:
		panic("tcp_usrreq");
	}
#if defined(sgi) && defined(DEBUG)
	if (tp && (so->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, 0, req);
#endif

	return (error);
}

tcp_ctloutput(int op, struct socket *so, int level, int optname, 
	struct mbuf **mp)
{
	int error = 0;
	struct inpcb *inp;
	register struct tcpcb *tp;
	register struct mbuf *m;

	ASSERT(SOCKET_ISLOCKED(so));
	inp = sotoinpcb(so);
	if (inp == NULL) {
		return (ECONNRESET);
	}
	if (level != IPPROTO_TCP) {
		error = ip_ctloutput(op, so, level, optname, mp);
		return (error);
	}
	if ((tp = intotcpcb(inp)) == NULL) {
		return ECONNRESET;
	}

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

		case TCP_FASTACK:
			if (m == NULL || m->m_len < sizeof (int))
				error = EINVAL;
			else if (*mtod(m, int *))
				tp->t_flags |= TF_FASTACK;
			else
				tp->t_flags &= ~TF_FASTACK;
			break;

		case TCP_MAXSEG:	/* not yet */
		default:
			error = EINVAL;
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
		case TCP_FASTACK:
			*mtod(m, int *) = tp->t_flags & TF_FASTACK;
			break;
		case TCP_MAXSEG:
			*mtod(m, int *) = tp->t_maxseg;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	}
	return (error);
}

extern u_int	tcp_sendspace;	/* Tunable */
extern u_int	tcp_recvspace;

/*
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * bufer space, and entering LISTEN state if to accept connections.
 */
tcp_attach(struct socket *so)
{
	register struct tcpcb *tp;
	struct inpcb *inp;
	int error;

	ASSERT(SOCKET_ISLOCKED(so));
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		soreserve(so, tcp_sendspace, tcp_recvspace);
	}
	error = in_pcballoc(so, &tcb);
	if (error)
		return (error);
	inp = sotoinpcb(so);
	tp = tcp_newtcpcb(inp);
	if (tp == 0) {
		int nofd = so->so_state & SS_NOFDREF;	/* XXX */

		so->so_state &= ~SS_NOFDREF;	/* don't free the socket yet */
		in_pcbdetach(inp);
		so->so_state |= nofd;
		return (ENOMEM);
	}
	tp->t_state = TCPS_CLOSED;
	return (0);
}

/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
struct tcpcb *
tcp_disconnect(register struct tcpcb *tp, struct ipsec *ipsec)
{
	struct socket *so = tp->t_inpcb->inp_socket;

	TCP_UTRACE_SO(UTN('tcpd','isc0'), so, __return_address);
	TCP_UTRACE_PCB(UTN('tcpd','isc1'), tp->t_inpcb, __return_address);
	TCP_UTRACE_TP(UTN('tcpd','isc2'), tp, __return_address);
	if (tp->t_state < TCPS_ESTABLISHED)
		tp = tcp_close(tp);
	else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		tp = tcp_drop(tp, 0, ipsec);
	else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tp = tcp_usrclosed(tp);
		if (tp)
			(void) tcp_output(tp, ipsec);
	}
	return (tp);
}

/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
struct tcpcb *
tcp_usrclosed(register struct tcpcb *tp)
{

	TCP_UTRACE_TP(UTN('tcpu','src0'), tp, __return_address);
	switch (tp->t_state) {

	case TCPS_CLOSED:
	case TCPS_LISTEN:
	case TCPS_SYN_SENT:
		tp->t_state = TCPS_CLOSED;
		tp = tcp_close(tp);
		break;

	case TCPS_SYN_RECEIVED:
	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_FIN_WAIT_1;
		break;

	case TCPS_CLOSE_WAIT:
		tp->t_state = TCPS_LAST_ACK;
		break;
	}
	if (tp && tp->t_state >= TCPS_FIN_WAIT_2 &&
	    tp->t_inpcb->inp_socket->so_state & SS_CANTRCVMORE) {
		soisdisconnected(tp->t_inpcb->inp_socket);
		/*
		 * Make sure 2MSL timer is set (e.g. SS_CANTRCVMORE may not
		 * have been set when we got the ACK that put us in FIN_WAIT_2
		 */
		if(!tp->t_timer[TCPT_2MSL])
		    tp->t_timer[TCPT_2MSL] = tcp_maxidle;
	}
	return (tp);
}
