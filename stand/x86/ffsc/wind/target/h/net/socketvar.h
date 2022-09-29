/* socketvar.h - socket header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */
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
 *      @(#)socketvar.h 7.3 (Berkeley) 12/30/87
 */

/*
modification history
--------------------
02o,18aug94,dzb  added sb_want to struct sockbuf (SPR #3552).
                 added check of m_len for user clusters in sballoc and sbfree.
		 added so_userArg to struct socket for use w/ sowakeup callback.
02n,22sep92,rrr  added support for c++
02m,26may92,rrr  the tree shuffle
02l,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
02k,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02j,26jun90,jcf  changed SEM_BINARY to SEMAPHORE.
02i,19apr90,hjb  removed sb_flagSem, changed MIN to min, deleted
		 sblock and sbunlock which are not needed anymore.
02h,20mar90,jcf  changed semTake to include timeout of WAIT_FOREVER.
02g,16mar90,rdc  added select support.
02f,16apr89,gae  updated to new 4.3BSD.
02e,04may88,jcf  changed SEMAPHOREs to SEM_IDs.
02d,14dec87,gae  removed vxWorks.h include!!!
02c,04nov87,dnw  removed unnecessary KERNEL conditional.
02b,03apr87,ecs  added header and copyright.
02a,02feb87,jlf  removed ifdef CLEAN
01a,19nov86,llk  fixed include file references.
*/

#ifndef __INCsocketvarh
#define __INCsocketvarh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "semlib.h"
#include "selectlib.h"

/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */

struct socket
    {
    short	so_type;		/* generic type, see socket.h */
    short	so_options;		/* from socket call, see socket.h */
    short	so_linger;		/* time to linger while closing */
    short	so_state;		/* internal state flags SS_*, below */
    caddr_t	so_pcb;			/* protocol control block */
    struct	protosw *so_proto;	/* protocol handle */

    int		so_userArg;		/* WRS extension - user argument */
    SEMAPHORE	so_timeoSem;		/* WRS extension */
    int		so_fd;			/* WRS extension - file descriptor */

    /*
     * Variables for connection queueing.
     * Socket where accepts occur is so_head in all subsidiary sockets.
     * If so_head is 0, socket is not related to an accept.
     * For head socket so_q0 queues partially completed connections,
     * while so_q is a queue of connections ready to be accepted.
     * If a connection is aborted and it has so_head set, then
     * it has to be pulled out of either so_q0 or so_q.
     * We allow connections to queue up based on current queue lengths
     * and limit on number of queued connections for this socket.
     */

    struct	socket *so_head;	/* back pointer to accept socket */
    struct	socket *so_q0;		/* queue of partial connections */
    struct	socket *so_q;		/* queue of incoming connections */
    short	so_q0len;		/* partials on so_q0 */
    short	so_qlen;		/* number of connections on so_q */
    short	so_qlimit;		/* max number queued connections */
    short	so_timeo;		/* connection timeout */
    u_short	so_error;		/* error affecting connection */
    short	so_pgrp;		/* pgrp for signals */
    u_long	so_oobmark;		/* chars to oob mark */

    /* Variables for socket buffering. */

    struct	sockbuf
	{
	u_long		sb_cc;		/* actual chars in buffer */
	u_long		sb_hiwat;	/* max actual char count */
	u_long		sb_mbcnt;	/* chars of mbufs used */
	u_long		sb_mbmax;	/* max chars of mbufs to use */
	u_long		sb_lowat;	/* low water mark (not used yet) */
	struct		mbuf *sb_mb;	/* the mbuf chain */
	struct		proc *sb_sel;	/* process selecting read/write */
	short		sb_timeo;	/* timeout (not used yet) */
	short		sb_flags;	/* flags, see below */

	SEMAPHORE	sb_Sem;		/* WRS extension */
	int		sb_want;	/* WRS extension */
	} so_rcv, so_snd;

#define	SB_MAX		(64*1024)	/* max chars in sockbuf */
#define	SB_LOCK		0x01		/* lock on data queue (so_rcv only) */
#define	SB_WANT		0x02		/* someone is waiting to lock */
#define	SB_WAIT		0x04		/* waiting for data/space (not used) */
#define	SB_SEL		0x08		/* buffer is selected */
#define	SB_COLL		0x10		/* collision selecting */
    SEL_WAKEUP_LIST so_selWakeupList; /* WRS extension */
    };

/* Socket state bits. */

#define	SS_NOFDREF		0x001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x008	/* in process of disconnecting */
#define	SS_CANTSENDMORE		0x010	/* can't send more data to peer */
#define	SS_CANTRCVMORE		0x020	/* can't receive more data from peer */
#define	SS_RCVATMARK		0x040	/* at mark on input */

#define	SS_PRIV			0x080	/* privileged for broadcast, raw... */
#define	SS_NBIO			0x100	/* non-blocking ops */
#define	SS_ASYNC		0x200	/* async i/o notify */


/* Macros for sockets and socket buffering. */

/* how much space is there in a socket buffer (so->so_snd or so->so_rcv) */
#define	sbspace(sb) \
    (min((long)((sb)->sb_hiwat - (sb)->sb_cc),\
	 (long)((sb)->sb_mbmax - (sb)->sb_mbcnt)))

/* do we have to send all at once on a socket? */
#define	sosendallatonce(so) \
    ((so)->so_proto->pr_flags & PR_ATOMIC)

/* can we read something from so? */
#define	soreadable(so) \
    ((so)->so_rcv.sb_cc || ((so)->so_state & SS_CANTRCVMORE) || \
	(so)->so_qlen || (so)->so_error)

/* can we write something to so? */
#define	sowriteable(so) \
    (sbspace(&(so)->so_snd) > 0 && \
	(((so)->so_state&SS_ISCONNECTED) || \
	  ((so)->so_proto->pr_flags&PR_CONNREQUIRED)==0) || \
     ((so)->so_state & SS_CANTSENDMORE) || \
     (so)->so_error)

/* adjust counters in sb reflecting allocation of m */
/* N.B., have to check m_len on user clusers... could be more than MCLBYTES */
#define	sballoc(sb, m) { \
	(sb)->sb_cc += (m)->m_len; \
	(sb)->sb_mbcnt += MSIZE; \
	if ((m)->m_off > MMAXOFF) \
		(sb)->sb_mbcnt += max (MCLBYTES, (m)->m_olen); \
}

/* adjust counters in sb reflecting freeing of m */
/* N.B., have to check m_len on user clusers... could be more than MCLBYTES */
#define	sbfree(sb, m) { \
	(sb)->sb_cc -= (m)->m_len; \
	(sb)->sb_mbcnt -= MSIZE; \
	if ((m)->m_off > MMAXOFF) \
		(sb)->sb_mbcnt -= max (MCLBYTES, (m)->m_olen); \
}

#define sorwakeup(so)   sowakeup((so), &(so)->so_rcv, SELREAD)
#define sowwakeup(so)   sowakeup((so), &(so)->so_snd, SELWRITE)

IMPORT struct socket *sonewconn();
IMPORT VOIDFUNCPTR sowakeupHook;

#ifdef __cplusplus
}
#endif

#endif /* __INCsocketvarh */
