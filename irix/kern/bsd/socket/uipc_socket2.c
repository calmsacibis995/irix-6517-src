/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)uipc_socket2.c	7.5 (Berkeley) 6/29/88
 *	plus portions of 7.15 (Berkeley) 6/28/90
 */

#ident	"$Revision: 4.105 $"

#include "tcp-param.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "bstring.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/mbuf.h"
#include "sys/proc.h"
#include "sys/ksignal.h"
#include "sys/protosw.h"
#include "sys/signal.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/strmp.h"
#include "sys/kmem.h"
#include "sys/atomic_ops.h"
#include "sys/sesmgr.h"

extern struct pollhead *phalloc(int);
extern void phfree(struct pollhead *);

extern zone_t *socket_zone;
extern void m_pcbinc(int, int);
extern void m_pcbdec(int, int);

#ifdef DEBUG
extern	void cksplnet(void);
#endif

extern int soreceive_alt;	/* systuneable */

void soqinsque(struct socket *, struct socket *, int);
void sbcompress(struct sockbuf *, struct mbuf *, struct mbuf *);
void sbdrop(struct sockbuf *, int);

/*
 * Primitive routines for operating on sockets and socket buffers
 */

/*
 * Procedures to manipulate state flags of socket
 * and do appropriate wakeups.  Normal sequence from the
 * active (originating) side is that soisconnecting() is
 * called during processing of connect() call,
 * resulting in an eventual call to soisconnected() if/when the
 * connection is established.  When the connection is torn down
 * soisdisconnecting() is called during processing of disconnect() call,
 * and soisdisconnected() is called when the connection to the peer
 * is totally severed.  The semantics of these routines are such that
 * connectionless protocols can call soisconnected() and soisdisconnected()
 * only, bypassing the in-progress calls when setting up a ``connection''
 * takes no time.
 *
 * From the passive side, a socket is created with
 * two queues of sockets: so_q0 for connections in progress
 * and so_q for connections already made and awaiting user acceptance.
 * As a protocol is preparing incoming connections, it creates a socket
 * structure queued on so_q0 by calling sonewconn().  When the connection
 * is established, soisconnected() is called, and transfers the
 * socket structure to so_q, making it available to accept().
 * 
 * If a socket is closed with sockets on either
 * so_q0 or so_q, these sockets are dropped.
 *
 * If higher level protocols are implemented in
 * the kernel, the wakeups done here will sometimes
 * cause software-interrupt process scheduling.
 */

void
soisconnecting(register struct socket *so)
{
	ASSERT(SOCKET_ISLOCKED(so));
	so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTING;
	SOCKET_SOTIMEOWAKE(so);
}

int so_wakeone = 1;

void
soisconnected(register struct socket *so)
{
	register struct socket *head = so->so_head;
	register s;

	ASSERT(SOCKET_ISLOCKED(so));
	if (head) {
		/*
		 * Incoming connection: wake up an accepter
		 * Use a special lock here because this is the one contended
		 * for by user programs doing "accepts".
		 */
		SOCKET_LOCK(head);
		if (soqremque(so, 0) == 0)
			panic("soisconnected");
		SOCKET_QLOCK(head, s);
		soqinsque(head, so, 1);
		sorwakeupconn(head, NETEVNET_SOUP);

		if (so_wakeone) {
			SOCKET_SOTIMEOWAKEONE(head);
		} else {
			SOCKET_SOTIMEOWAKE(head);
		}
		SOCKET_QUNLOCK(head, s);
		SOCKET_UNLOCK(head);
	}
		
	so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING);
	so->so_state |= SS_ISCONNECTED;
	SOCKET_SOTIMEOWAKE(so);
	_SESMGR_SAMP_SEND_DATA(so);
	sorwakeup(so, NETEVENT_SOUP);
	sowwakeup(so, NETEVENT_SODOWN);
}

void
soisdisconnecting(register struct socket *so)
{
	ASSERT(SOCKET_ISLOCKED(so));
	so->so_state &= ~SS_ISCONNECTING;
	so->so_state |= (SS_ISDISCONNECTING|SS_CANTRCVMORE|SS_CANTSENDMORE);
	SOCKET_SOTIMEOWAKE(so);
	sowwakeup(so, NETEVENT_SODOWN);
	sorwakeup(so, NETEVENT_SOUP);
}

void
soisdisconnected(register struct socket *so)
{
	ASSERT(SOCKET_ISLOCKED(so));
	so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
	so->so_state |= (SS_CANTRCVMORE|SS_CANTSENDMORE);
	SOCKET_SOTIMEOWAKE(so);
	sowwakeup(so, NETEVENT_SODOWN);
	sorwakeup(so, NETEVENT_SOUP);
}

/*
 * When an attempt at a new connection is noted on a socket
 * which accepts connections, sonewconn() is called.  If the
 * connection is possible (subject to space constraints, etc.)
 * then we allocate a new structure, properly linked into the
 * data structure of the original socket, and return this.
 */
struct socket *
sonewconn(register struct socket *head, struct ipsec *ipsec)
{
	register struct socket *so;

	ASSERT(SOCKET_ISLOCKED(head));
	if (head->so_qlen + head->so_q0len > 3 * head->so_qlimit / 2 ||
		head->so_state & SS_CLOSING)
		goto bad;
	so = (struct socket *)kmem_zone_zalloc(socket_zone, KM_NOSLEEP);
	if (so == NULL) {
		m_mcbfail();
		goto bad;
	}
	m_pcbinc(sizeof(*so), MT_SOCKET);
	so->so_type = head->so_type;
	so->so_options = head->so_options &~ SO_ACCEPTCONN;
	so->so_linger = head->so_linger;
	so->so_input = head->so_input;
	so->so_data = head->so_data;
	so->so_state = (head->so_state | SS_NOFDREF) &
			~(SS_WANT_CALL | SS_TPISOCKET | SS_CONN_IND_SENT);
	so->so_proto = head->so_proto;
	so->so_timeo = head->so_timeo;
	so->so_pgid = head->so_pgid;
	if (head->so_rcv.sb_flags & SB_INPUT_ACK)
		so->so_rcv.sb_flags |= SB_INPUT_ACK;
	initpollhead(&so->so_ph);
	so->so_refcnt = 1;
	SOCKET_INITLOCKS(so);
	SOCKET_LOCK(so);	
	if (_SESMGR_SOCKET_COPY_ATTRS(head, so, ipsec) != 0) {
		kmem_zone_free(socket_zone, so);
		m_pcbdec(sizeof(*so), MT_SOCKET);
		goto bad;
	}
	(void) soreserve(so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat);
	SO_UTRACE(UTN('sone','wcon'), so, __return_address);
	soqinsque(head, so, 0);
	if (0 == (*so->so_proto->pr_usrreq)(so, PRU_ATTACH,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0)) {
		return (so);	/* success */
	}

	/* failure, unwind all the allocations */
	(void) soqremque(so, 0);
	SOCKET_UNLOCK(so);
	SOCKET_FREELOCKS(so);
	destroypollhead(&so->so_ph);

#ifdef DEBUG
	{
		long *p = (long *)so;
		size_t l = sizeof(*so) / sizeof(long);
		size_t i;
		for (i = 0; i < l; p++, i++) {
			*p = (long)__return_address;
		}
	}
#endif
	kmem_zone_free(socket_zone, so);
	m_pcbdec(sizeof(*so), MT_SOCKET);
bad:
	return ((struct socket *)0);
}

void
soqinsque(register struct socket *head, register struct socket *so, int q)
{
	ASSERT(SOCKET_ISLOCKED(so));
	so->so_head = head;

#define QINSERT(so, head, q) \
	(head)->so_##q##prev->so_##q = (so); \
	(so)->so_##q##prev = (head)->so_##q##prev; \
	(so)->so_##q = (head); \
	(head)->so_##q##prev = (so); \
	(head)->so_##q##len++;

	ASSERT((q >= 0) && (q <= 1));

	if (q == 0) {
		ASSERT(SOCKET_ISLOCKED(head));
		SO_UTRACE(UTN('soqi','nsq0'), so, __return_address);
		QINSERT(so, head, q0);
	} else {
		ASSERT(SOCKET_Q_ISLOCKED(head));
		SO_UTRACE(UTN('soqi','nsq1'), so, __return_address);
		QINSERT(so, head, q);
	}
#undef	QINSERT
}

soqremque(register struct socket *so, int q)
{
	register struct socket *head;

	ASSERT((q >= 0) && (q <= 1));

	ASSERT(SOCKET_ISLOCKED(so));
	/*
	 * XXX Hack to fix a bug reported in comp.bugs.4sd by John LoVerso 
	 * (10/9/87).  If mget() fails in tcp_newtcpcb(), this routine is 
	 * eventually called twice.  so->so_head is null on the 2nd call, 
	 * hence the following to prevent a null ptr. dereference.
	 */
	head = so->so_head;
    	if (head == 0) {
		return (0);
	}

#define QREMOVE(so, head, q)	\
	if ((so)->so_##q == 0 || (so)->so_##q##prev == 0) { \
		return 0; \
	} \
	(so)->so_##q##prev->so_##q = (so)->so_##q; \
	(so)->so_##q##->so_##q##prev = (so)->so_##q##prev; \
	(head)->so_##q##len--;	\
	(so)->so_##q = (so)->so_##q##prev = (struct socket *)0; \
	(so)->so_head = (struct socket *)0;

	if (q == 0) {
		ASSERT(SOCKET_ISLOCKED(head));
		/* QREMOVE will return 0 if so is not on head's list */
		SO_UTRACE(UTN('soqr','emq0'), so, __return_address);
		QREMOVE(so, head, q0);
	} else {
		ASSERT(SOCKET_Q_ISLOCKED(head));
		/* QREMOVE will return 0 if so is not on head's list */
		SO_UTRACE(UTN('soqr','emq1'), so, __return_address);
		QREMOVE(so, head, q);
	}
	return 1;
#undef	QREMOVE
}

/*
 * Socantsendmore indicates that no more data will be sent on the
 * socket; it would normally be applied to a socket when the user
 * informs the system that no more data is to be sent, by the protocol
 * code (in case PRU_SHUTDOWN).  Socantrcvmore indicates that no more data
 * will be received, and will normally be applied to the socket by a
 * protocol when it detects that the peer will send no more data.
 * Data queued for reading in the socket may yet be read.
 */

void
socantsendmore(struct socket *so)
{
	ASSERT(SOCKET_ISLOCKED(so));
	so->so_state |= SS_CANTSENDMORE;
	sowwakeup(so, NETEVENT_SODOWN);
}

void
socantrcvmore(struct socket *so)
{

	ASSERT(SOCKET_ISLOCKED(so));
	so->so_state |= SS_CANTRCVMORE;
	sorwakeup(so, NETEVENT_SOUP);
}

/* ARGSUSED */
sbunlock_wait(struct sockbuf *sb, struct socket *so, int intr)
{
	k_sigset_t	oldset;
	int		retval;

	ASSERT(SOCKET_ISLOCKED(so));
	sb->sb_flags |= SB_WAIT;

	/* do sbunlock in line */
	(sb)->sb_flags &= ~(SB_LOCK|SB_LOCK_RCV);  
        if ((sb)->sb_flags & SB_WANT) { 
                (sb)->sb_flags &= ~SB_WANT; 
                NETPAR(NETSCHED, NETWAKINGTKN, NETPID_NULL, (event), \
                       NETCNT_NULL, NETRES_SBUNLOCK);   
		SOCKET_SBWANT_WAKE(sb);
        }  
	if (intr) {
		if (intr == 2)
			hold_nonfatalsig(&oldset);
		retval = SOCKET_MPUNLOCK_SBWAIT(so, sb, PZERO+1);
		if (intr == 2)
			release_nonfatalsig(&oldset);
		if (retval)
			return EINTR;
	} else
		SOCKET_MPUNLOCK_SBWAIT_NO_INTR(so, sb, PZERO+1);
	return 0;
}

/* ARGSUSED */
sbunlock_timedwait(struct sockbuf *sb, struct socket *so, timespec_t *ts,
	timespec_t *rts, int intr)
{
	k_sigset_t	oldset;
	int		retval;

	ASSERT(SOCKET_ISLOCKED(so));
	sb->sb_flags |= SB_WAIT;

	/* do sbunlock in line */
	(sb)->sb_flags &= ~(SB_LOCK|SB_LOCK_RCV);  
        if ((sb)->sb_flags & SB_WANT) { 
                (sb)->sb_flags &= ~SB_WANT; 
                NETPAR(NETSCHED, NETWAKINGTKN, NETPID_NULL, (event), \
                       NETCNT_NULL, NETRES_SBUNLOCK);   
		SOCKET_SBWANT_WAKE(sb);
        }  
	if (intr) {
		if (intr == 2)
			hold_nonfatalsig(&oldset);
		retval = sv_timedwait_sig(&sb->sb_waitsem, PZERO+1,
			&so->so_sem, 0, 0, ts, rts);
		if (intr == 2)
			release_nonfatalsig(&oldset);
		if (retval)
			return EINTR;
	} else
		sv_timedwait(&sb->sb_waitsem, PZERO+1, &so->so_sem, 0, 0, ts,
			rts);
	return 0;
}

/*
 * Wakeup socket readers and writers.
 * Do asynchronous notification via SIGIO
 * if the socket has the SS_ASYNC flag set.
 */
void
sowakeup(register struct socket *so,
	 struct sockbuf *sb,
	 int event)
{
	ASSERT(SOCKET_ISLOCKED(so));
	if (so->so_state & SS_WANT_CALL) {
		struct socket *head;
		if (so->so_callout) {
			ASSERT(so->so_monpp);
			mp_streams_interrupt(so->so_monpp,
				  0,
				  (void(*)())so->so_callout,
				  so, 
		                  (void *)((sb == &so->so_rcv)
					? (NULL+SSCO_INPUT_ARRIVED)
		                        : (NULL+SSCO_OUTPUT_AVAILABLE)),
		                  so->so_callout_arg);
			  return;
		}
		/*
		 * This hack lets tpisocket know that a connection received a
		 * FIN while waiting to be accepted, and that a T_DISCON_IND
		 * event needs to be generated.  Only do this if we want
		 * a callback but we have no callout function and we are
		 * marked as SS_CANTRCVMORE.
		 */
		if (head = so->so_head) {
			if (so->so_state & SS_CANTRCVMORE) {
				/*
				 * clear SS_NOFDREF so that we won't disappear
				 * tpisocket will have to unclear it after
				 * it's done
				 */
				so->so_state &= ~SS_NOFDREF;
				if (head->so_callout) {
					ASSERT(head->so_monpp);
					mp_streams_interrupt(head->so_monpp,
						0,
						(void(*)())head->so_callout,
						head,
						(void *)(NULL+SSCO_INPUT_ARRIVED),
						head->so_callout_arg);
				}
			}
		}
		return;
	}
	pollwakeup(&so->so_ph, event);
	if (sb->sb_flags & SB_WAIT) {
		sb->sb_flags &= ~SB_WAIT;
		SOCKET_SBWAIT_WAKEALL(sb);
	}
	if (so->so_state & SS_ASYNC) {
		if (so->so_pgid < 0) {
			signal(-so->so_pgid, SIGIO);
		} else if (so->so_pgid > 0) {
			sigtopid(so->so_pgid, SIGIO, SIG_ISKERN, 0, 0, 0);
		}
	}
}

/*
 * Socket buffer (struct sockbuf) utility routines.
 *
 * Each socket contains two socket buffers: one for sending data and
 * one for receiving data.  Each buffer contains a queue of mbufs,
 * information about the number of mbufs and amount of data in the
 * queue, and other fields allowing select() statements and notification
 * on data availability to be implemented.
 *
 * Data stored in a socket buffer is maintained as a list of records.
 * Each record is a list of mbufs chained together with the m_next
 * field.  Records are chained together with the m_act field. The upper
 * level routine soreceive() expects the following conventions to be
 * observed when placing information in the receive buffer:
 *
 * 1. If the protocol requires each message be preceded by the sender's
 *    name, then a record containing that name must be present before
 *    any associated data (mbuf's must be of type MT_SONAME).
 * 2. If the protocol supports the exchange of ``access rights'' (really
 *    just additional data associated with the message), and there are
 *    ``rights'' to be received, then a record containing this data
 *    should be present (mbuf's must be of type MT_RIGHTS).
 * 3. If a name or rights record exists, then it must be followed by
 *    a data record, perhaps of zero length.
 *
 * Before using a new socket structure it is first necessary to reserve
 * buffer space to the socket, by calling sbreserve().  This should commit
 * some of the available buffer space in the system buffer pool for the
 * socket (currently, it does nothing but enforce limits).  The space
 * should be released by calling sbrelease() when the socket is destroyed.
 */

soreserve(register struct socket *so,
	  u_int sndcc,
	  u_int rcvcc)
{

	ASSERT(SOCKET_ISLOCKED(so));
	sbreserve(&so->so_snd, sndcc); 
	sbreserve(&so->so_rcv, rcvcc);
	return (0);
}

/*
 * Allot mbufs to a sockbuf.
 * Attempt to scale cc so that mbcnt doesn't become limiting
 * if buffering efficiency is near the normal case.
 */
sbreserve(struct sockbuf *sb, u_int cc)
{
	/*
	 * There used to be a check for SB_MAX here, but since
	 * this is not a real "reserve" anyway, there is no need
	 * for any specific limit.
	 */
	sb->sb_hiwat = cc;
	return (1);
}

/*
 * Free mbufs held by a socket, and reserved mbuf space.
 */
void
sbrelease(struct sockbuf *sb)
{
	sbflush(sb);
	sb->sb_hiwat = 0;
}

/*
 * Routines to add and remove
 * data from an mbuf queue.
 *
 * The routines sbappend() or sbappendrecord() are normally called to
 * append new mbufs to a socket buffer, after checking that adequate
 * space is available, comparing the function sbspace() with the amount
 * of data to be added.  sbappendrecord() differs from sbappend() in
 * that data supplied is treated as the beginning of a new record.
 * To place a sender's address, optional access rights, and data in a
 * socket receive buffer, sbappendaddr() should be used.  To place
 * access rights and data in a socket receive buffer, sbappendrights()
 * should be used.  In either case, the new data begins a new record.
 * Note that unlike sbappend() and sbappendrecord(), these routines check
 * for the caller that there will be enough space to store the data.
 * Each fails if there is not enough space, or if it cannot find mbufs
 * to store additional information in.
 *
 * Reliable protocols may use the socket send buffer to hold data
 * awaiting acknowledgement.  Data is normally copied from a socket
 * send buffer in a protocol with m_copy for output to a peer,
 * and then removing the data from the socket buffer with sbdrop()
 * or sbdroprecord() when the data is acknowledged by the peer.
 */

#if defined(sgi) && defined(DEBUG)
void
sbccverify(struct sockbuf *sb)
{
	register struct mbuf *m;
	register int cc;

	m = sb->sb_mb;
	if (m == NULL) {
		ASSERT(sb->sb_cc == 0);
		return;
	}
	cc = m_length(m);
	while (m = m->m_act) {
		cc += m_length(m);
	}
	ASSERT(sb->sb_cc == cc);
}
#endif

/*
 * Append mbuf chain m to the last record in the
 * socket buffer sb.  The additional space associated
 * the mbuf chain is recorded in sb.  Empty mbufs are
 * discarded and mbufs are compacted where possible.
 */
void
sbappend(struct sockbuf *sb,
	 struct mbuf *m)
{
	register struct mbuf *n;

	if (m == 0)
		return;
	GOODMT(m->m_type);
	if (n = sb->sb_mb) {
		while (n->m_act) {
			n = n->m_act;
			GOODMT(n->m_type);
		}
		while (n->m_next) {
			n = n->m_next;
			GOODMT(n->m_type);
		}
	} 
	sbcompress(sb, m, n);
#ifdef DEBUG
	sbccverify(sb);
#endif
}

/*
 * As above, except the mbuf chain
 * begins a new record.
 */
void
soinput(struct socket *so,
	struct mbuf *m)
{
	sbappend(&so->so_rcv, m);
	sorwakeup(so, NETEVENT_TCPUP);
}

/*
 * Append address and data, and optionally, rights
 * to the receive queue of a socket.  Return 0 if
 * no space in sockbuf or insufficient mbufs.
 */
sbappendaddr(register struct sockbuf *sb,
	     struct sockaddr *asa,
	     struct mbuf *m0,
	     struct mbuf *rights0,
	     struct ifnet *ifp
	     )
{
	register struct mbuf *m, *n, *mif;
	int space;
	
	if (asa->sa_family == AF_UNIX) 
	  space = sizeof(asa->sa_family) + strlen(asa->sa_data) + 1;
	else
#ifdef INET6
	  space = _FAKE_SA_LEN_DST(asa);
#else
	  space = sizeof (*asa);
#endif

	mif = 0;

	for (m = m0; m; m = m->m_next) {
		GOODMT(m->m_type);
		space += m->m_len;
	}
	if (rights0)
		space += rights0->m_len;
	if (ifp)
		space += IFNAMSIZ;
	if (space > sbspace(sb))
		return (0);
	MGET(m, M_DONTWAIT, MT_SONAME);
	if (m == 0)
		return (0);

	/* Make sure we bcopy a null terminated string for unix domain*/
	if (asa->sa_family == AF_UNIX) 
		m->m_len = sizeof(asa->sa_family) + strlen(asa->sa_data) + 1;
	else
#ifdef INET6
		m->m_len = _FAKE_SA_LEN_DST(asa);
#else
		m->m_len = sizeof (*asa);
#endif
	bcopy(asa, mtod(m, struct sockaddr *), m->m_len);

	if (ifp) {
		MGET(mif, M_DONTWAIT, MT_DATA);
		if (mif == 0) {
			m_freem(m);
			return (0);
		}
		mif->m_len = IFNAMSIZ;
		bzero(mtod(mif, char *), IFNAMSIZ);
		sprintf(mtod(mif, char *), "%s%d",
			ifp->if_name, (int)ifp->if_unit);
		mif->m_next = m0;
		m0 = mif;
	}

	if (rights0 && rights0->m_len) {
		m->m_next = m_copy(rights0, 0, rights0->m_len);
		if (m->m_next == 0) {
			m_freem(m);
			if (mif)
				m_free(mif);
			return (0);
		}
		sballoc(sb, m->m_next);
	}
	sballoc(sb, m);

	mif = m;
	if (m->m_next)
		m = m->m_next;
	if (m0)
		sbcompress(sb, m0, m);
	if (n = sb->sb_mb) {
		GOODMT(n->m_type);
		while (n->m_act) {
			n = n->m_act;
			GOODMT(n->m_type);
		}
		n->m_act = mif;
	} else
		sb->sb_mb = mif;
#ifdef DEBUG
	sbccverify(sb);
#endif
	return (1);
}

sbappendrights(struct sockbuf *sb,
	       struct mbuf *m0,
	       struct mbuf *rights)
{
	register struct mbuf *m, *n;
	int space = 0;

	if (rights == 0)
		panic("sbappendrights");
	for (m = m0; m; m = m->m_next) {
		GOODMT(m->m_type);
		space += m->m_len;
	}
	space += rights->m_len;
	if (space > sbspace(sb))
		return (0);
	m = m_copy(rights, 0, rights->m_len);
	if (m == 0)
		return (0);
	sballoc(sb, m);
	if (m0)
		sbcompress(sb, m0, m);
	if (n = sb->sb_mb) {
		GOODMT(n->m_type);
		while (n->m_act) {
			n = n->m_act;
			GOODMT(n->m_type);
		}
		n->m_act = m;
	} else
		sb->sb_mb = m;
#ifdef DEBUG
	sbccverify(sb);
#endif
	return (1);
}

/*
 * Compress mbuf chain m into the socket
 * buffer sb following mbuf n.  If n
 * is null, the buffer is presumed empty.
 */
void
sbcompress(register struct sockbuf *sb,
	   register struct mbuf *m,
	   register struct mbuf *n)
{
	/*
	 * If someone else has the SB_LOCK, add entire chain atomically.
	 * They may be reading from sb_mb while we're adding! So don't
	 * compress.  Remove any zero length mbuf chains before adding.
	 */
	if (sb->sb_flags & SB_LOCK_RCV && soreceive_alt) {
		struct mbuf *comprsd = m;
		struct mbuf *mcur = comprsd, **prev = &comprsd;
		int added = 0;

		while (mcur) {
			if(mcur->m_len == 0) {
				#pragma mips_frequency_hint NEVER
				/*
				 * Don't lose next record pointer.
				 */
				if (n == 0 && mcur->m_next != 0)
					mcur->m_next->m_act = mcur->m_act;
				/*
				 * Extract mbuf from list
				 */
				mcur = m_free(mcur);
				*prev = mcur;
			    	continue;
			}
			added += mcur->m_len;
			prev = &(mcur->m_next);
			mcur = mcur->m_next;
		}
		sb->sb_cc += added;
		if (n)
			n->m_next = comprsd;
		else
			sb->sb_mb = comprsd;
		return;
	}

	while (m) {
		GOODMT(m->m_type);
		if (m->m_len == 0) {
			if (n == 0 && m->m_next != 0)
				m->m_next->m_act = m->m_act;
			m = m_free(m);
			continue;
		}
		if (m->m_len <= MLEN*4 && M_HASCL(m) && n) {
			/*
			 * There is a very sparse cluster about to be put on a
			 * non-empty socket input queue. Copy it to a bunch of 
			 * short mbufs to conserve memory. We do this here
			 * to avoid the time/space trade-off in the case of NFS
			 * and TCP acks, since those packets are never queued
			 * for a significant period of time.
			 */
			struct mbuf *new;
			struct mbuf *first;
			struct mbuf **prev;

			first = (struct mbuf *)0;
			while (m->m_len > 0) {
				new = m_get(M_DONTWAIT, m->m_type);
				if (first == (struct mbuf *)0) {
					first = new;
				} else {
					*prev = new;
				}
				if (new == (struct mbuf *)0)
				    break;
				prev = &new->m_next;
				if (m->m_len > MLEN)
				    new->m_len = MLEN;
				else
				    new->m_len = m->m_len;
				bcopy( mtod(m, caddr_t), mtod(new, caddr_t),
				      new->m_len);
				m->m_len -= new->m_len;
				m->m_off += new->m_len;
			}
			if (new) {
				/*
				 * We got new small mbufs, free the OLD one,
				 * and use the new chain below.
				 */
				new->m_next = m_free(m);
				m = first;
			} else {
				/*
				 * One of the m_gets failed, so in this case
				 * we need to free the NEW ones (if any),
				 * and re-adjust the counts in the old one.
				 */
				while (first) {
					new = first;
					first = new->m_next;
					m->m_len += new->m_len;
					m->m_off -= new->m_len;
					m_free(new);
				}
			}
		}
		if (n && !M_HASCL(n) &&
		    (n->m_off + n->m_len + m->m_len) <= MMAXOFF &&
		    n->m_type == m->m_type) {
			bcopy(mtod(m, caddr_t), mtod(n, caddr_t) + n->m_len,
			    (unsigned)m->m_len);
			n->m_len += m->m_len;
			sb->sb_cc += m->m_len;
			m = m_free(m);
			continue;
		}
		sballoc(sb, m);
		if (n)
			n->m_next = m;
		else
			sb->sb_mb = m;
		n = m;
		m = m->m_next;
		n->m_next = 0;
	}
}

/*
 * Free all mbufs in a sockbuf.
 * Check that all resources are reclaimed.
 */
void
sbflush(register struct sockbuf *sb)
{
	ASSERT(!(sb->sb_flags & SB_LOCK));
	while (sb->sb_cc)
		sbdrop(sb, (int)sb->sb_cc);
	ASSERT(0 == sb->sb_cc && 0 == sb->sb_mb);
}

/*
 * Drop data from (the front of) a sockbuf.
 */
void
sbdrop(register struct sockbuf *sb,
       register int len)
{
	register struct mbuf *m, *mn;
	struct mbuf *next;

	ASSERT((signed)sb->sb_cc >= 0);
	ASSERT(len >= 0);
#if defined(sgi) && defined(DEBUG)
	sbccverify(sb);
#endif
	next = (m = sb->sb_mb) ? m->m_act : 0;
	while (len > 0) {
		if (m == 0) {
			if (next == 0)
				panic("sbdrop");
			m = next;
			next = m->m_act;
			continue;
		}
		GOODMT(m->m_type);
		if (m->m_len > len) {
			m->m_len -= len;
			m->m_off += len;
			sb->sb_cc -= len;
			ASSERT((signed)sb->sb_cc >= 0);
			break;
		}
		len -= m->m_len;
		sbfree(sb, m);
		ASSERT((signed)sb->sb_cc >= 0);
		MFREE(m, mn);
		m = mn;
	}
	while (m && m->m_len == 0) {
		sbfree(sb, m);		/* why bother to subtract 0 ? */
		MFREE(m, mn);
		m = mn;
	}
	if (m) {
		GOODMT(m->m_type);
		sb->sb_mb = m;
		m->m_act = next;
	} else
		sb->sb_mb = next;
}

/*
 * Drop a record off the front of a sockbuf
 * and move the next record to the front.
 */
void
sbdroprecord(register struct sockbuf *sb)
{
	register struct mbuf *m, *mn;

	m = sb->sb_mb;
	if (m) {
		sb->sb_mb = m->m_act;
		do {
			sbfree(sb, m);
			MFREE(m, mn);
		} while (m = mn);
	}
}

int
socket_pair_cmplock(struct socket *so1, struct socket *so2)
{
	timespec_t backoff = {0, 1L};

	if (so1 == so2)
		return(1);
	ASSERT(SOCKET_ISLOCKED(so1));
	if ((so1) < (so2) || (so2->so_options & SO_ACCEPTCONN)) {
		SOCKET_LOCK(so2);
		return(1);
	} else if (SOCKET_TRYLOCK(so2) == 0) {
		/*
		 * Couldn't get lock.  Hold so2 so that it won't go away
		 * while we drop the lock on so1.
		 * If so2 is freed prior to our return, sofree() will mark it
		 * so that we clean it up.
		 */
		SO_HOLD(so2);
		SOCKET_UNLOCK(so1);
		/*
		 * This really sticks in my craw, but we need to avoid livelock
		 * with AF_UNIX.
		 */
		backoff.tv_nsec = 4 * (curthreadp->k_id & 0x0f);
		nano_delay(&backoff);
		SOCKET_LOCK(so2);
		if (SO_RELE(so2) == 0) {
			SOCKET_UNLOCK(so2);
		}
		SOCKET_LOCK(so1);
		return(0);
	}
	return(1);
}

void
socket_pair_mplock(struct socket *so1, struct socket *so2)
{
	ASSERT(!SOCKET_ISLOCKED(so1));
	ASSERT(!SOCKET_ISLOCKED(so2));
	if ((so1) == (so2)) {
		SOCKET_LOCK(so1);
		return;
	}
	if ((so1) < (so2)) { 
                SOCKET_LOCK(so1); 
		ASSERT(&(so1)->so_sem != &(so2)->so_sem);
                SOCKET_LOCK(so2);  
        } else { 
                SOCKET_LOCK(so2); 
		ASSERT(&(so1)->so_sem != &(so2)->so_sem);
                SOCKET_LOCK(so1);  
        } 
}

void
socket_pair_mpunlock(struct socket *so1, struct socket *so2)
{
	if ((so1) == (so2)) {
		SOCKET_UNLOCK(so1);
		return;
	}
	if ((so1) < (so2)) { 
		ASSERT(&(so1)->so_sem != &(so2)->so_sem);
                SOCKET_UNLOCK(so2); 
                SOCKET_UNLOCK(so1); 
        } else { 
		ASSERT(&(so1)->so_sem != &(so2)->so_sem);
		SOCKET_UNLOCK(so1); 
                SOCKET_UNLOCK(so2); 
        }
}

#ifdef __SO_LOCK_DEBUG

so_mutex_lock(struct socket *so, mutex_t *lock, int pri)
{
	mutex_lock(lock, pri);
	so->so_locker = __return_address;
	so->so_lockthread = private.p_curkthread;
	return 1;
}

so_mutex_unlock(struct socket *so, mutex_t *lock)
{
	so->so_unlocker = __return_address;
	so->so_unlockthread = private.p_curkthread;
	mutex_unlock(lock);
	return 1;
}

so_mutex_trylock(struct socket *so, mutex_t *lock)
{
	int r;

	if (r = mutex_trylock(lock)) {
		so->so_locker = __return_address;
		so->so_lockthread = private.p_curkthread;
	}
	return r;
}

#endif

#if defined(__SO_LOCK_DEBUG) || defined(_SO_UTRACE)
int
sohold(struct socket *so)
{
#ifdef _SO_UTRACE
	SO_UTRACE(UTN('soho','ld  '), so, __return_address);
#else
	SO_TRACE(so, 9);
#endif
	return atomicAddInt(&so->so_refcnt, 1);
}
#endif
