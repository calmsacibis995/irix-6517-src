/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* 
 * Server-side nfs tcp stuff.
 */

#include "types.h"
#include <sys/atomic_ops.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/hashing.h>

#include <sys/mbuf.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <net/route.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <sys/tcpipstats.h>
#include <sys/cmn_err.h>
#include <sys/fcntl.h>
#include "tcp-param.h"
#include "nfs.h"
#include "xdr.h"
#include "auth.h"
#include "clnt.h"
#include "rpc_msg.h"
#include "svc.h"
#include "nfs_stat.h"
#include "nfs_impl.h"
#include <rpcsvc/nlm_prot.h>
#include <sys/protosw.h>
#include <sys/vsocket.h>
#include "lockd_impl.h"

#ifdef DEBUG
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#endif

/* 
 * Socket map stuff.  Theory of Operation.
 * Server has X possible tcp connections, each represented by a socket map.
 * The socket map contains the socket pointer itself, plus data structures
 * for queuing the socket for service and such.  tcp_input() calls 
 * nfstcp_input() to appended incoming data to the socket. 
 * The socket's map structure is updated to reflect this, and it is
 * put into the Q of things to do, if it's not already there.
 * Sleeping nfsd's are awakened.
 */

int best_sndsize_multiplier = 8;
#define BEST_SNDSIZE(iolen) (iolen * best_sndsize_multiplier + 65536)

typedef struct smap smap_t;	/* socket map */
struct smap {
	smap_t *next;		/* doubly link list of connections */
	smap_t *prev;
	smap_t *Qnext;		/* singly linked service Q */
	struct socket *so;
	vsock_t *vso;
	struct mbuf *mrcv;	/* received mbuf chain */
	struct mbuf **mrcv_tail;/* ptr to m_next on last mbuf in mrcv */
	int mrcv_len;		/* mlength(mrcv) */
	u_int mrcv_reclen;	/* krpc_peeklen for mrcv's 1st request */
	mutex_t sorcvlock;	/* lock when reading so */
	mutex_t sosndlock;	/* lock when writing so */
	struct sockaddr_in sin;	/* connected to address */
	time_t lastbolt;	/* lbolt of last sbappend() on sock */
	int holds;		/* number of holds on this map */
	int drop;		/* if set, drop connection on rele */
	int Qflags;		/* Q flags controlled by NFSQ_LOCK() */
};

/*
 * Unfortunately, sockets are a global pool and as such we
 * must use a global lock to access our smap references to them.
 */
lock_t nfstcpq_global;

#define NFSQ_LOCK(s) s = mutex_spinlock(&nfstcpq_global)
#define NFSQ_UNLOCK(s)  mutex_spinunlock(&nfstcpq_global, s)

/* 
 * Qflags fields changed under NFSQ_LOCK().
 */
#define SMAP_ONQ	0x1	/* map already NQ'd, don't re-Q */
#define SMAP_ONCPU	0x2	/* being worked on, don't NQ */

#define SMAPBUCKETS	64	/* must be power of 2 */
#define SMAPHASHMASK	(SMAPBUCKETS-1)		/* i.e. mod SMAPBUCKETS */
#define SMAPHASHSHIFT	8	/* drop last byte of socket address */
#define SMAPHASH(x)	((((u_int)(__psint_t)(x)) >> SMAPHASHSHIFT) & SMAPHASHMASK)


/* 
 * Hashed list for socket maps
 */
struct smapbuckets {
	lock_t bucklock;
	smap_t *head;
	smap_t *tail;
	int count;
};

struct smapbuckets smapbuckets[SMAPBUCKETS];

/* 
 * Hold/Release  a connection map.
 */
#define SMAP_HOLD(smp)	\
    (void)atomicAddInt(&smp->holds, 1)
#define SMAP_RELE(smp)	\
    (void)atomicAddInt(&smp->holds, -1)


static mutex_t nfssvctcp_lock;
static int nfssvctcp_init;	/* service initialized */

static int totalsmaps;		/* current total */
static int maxsmaps;		/* syscall param */
static int nfs_maxconn = 10000;	/* max connections.  Config var? */

/* 
 * Socket map service Q stuff
 */
smap_t *smapQhead;
smap_t *smapQtail;
int smapQlen;
static void NQ(smap_t *);

static smap_t *sotosmap(struct socket *);
static int nfstcp_rcv(smap_t *smp, struct mbuf **mp);
static int nfstcp_accept(struct socket *so, struct socket **,
			 struct sockaddr_in *, vsock_t **);
static void nfstcp_addsock(struct socket *, vsock_t *, struct sockaddr_in *);
static void nfstcp_dropsock(struct socket *);
static void nfstcp_dropconn(smap_t *);
static void dropmap(smap_t *);

void nfstcp_input(struct socket *, struct mbuf *);


#define BUCK_INITLOCK(x) spinlock_init(&(x), "buck_lock")
#define BUCK_LOCK(x, y) (x) = mutex_spinlock(&smapbuckets[SMAPHASH(y)].bucklock)
#define BUCK_UNLOCK(x, y) mutex_spinunlock(&smapbuckets[SMAPHASH(y)].bucklock, (x))


#ifdef DEBUG
static void nfs_svcdbginit(void);
#endif

int nfstcp_onq,
 nfstcp_newdata,
 nfstcp_nqs,
 nfstcp_dqs;

/* 
 * Enqueue socket on nfsd waiters Q. 
 * Called with NFSQ_LOCK() held.
 */
static void
NQ(smap_t * smp)
{
	if (nfssvctcp_init == 0)
		return;
	if (smp->Qflags & SMAP_ONQ) {
		nfstcp_onq++;
		return;
	}
	if (smp->Qflags & SMAP_ONCPU) {
		nfstcp_newdata++;
		return;
	}
	if (!smapQhead) {
		smapQhead = smp;
		ASSERT(smapQlen == 0);
	}
	smp->Qflags = SMAP_ONQ;
	smp->Qnext = 0;
	if (smapQtail)
		smapQtail->Qnext = smp;
	smapQtail = smp;
	smapQlen++;
	nfstcp_nqs++;
}

/* 
 * Called with NFSQ_LOCK() held.
 */
void *
nfstcp_DQ(void)
{
	smap_t *smp;

	smp = smapQhead;
	if (smp) {
		smapQhead = smp->Qnext;
		smp->Qnext = 0;
		if (smapQtail == smp)
			smapQtail = 0;
		smp->Qflags = SMAP_ONCPU;
		nfstcp_dqs++;
		smapQlen--;
	} else
		ASSERT(smapQlen == 0);
	return ((void *) smp);
}

void *
nfstcp_DQGLOBAL(void)
{
	void *vp;
	int s;

	if (smapQhead == NULL)
		return NULL;
	NFSQ_LOCK(s);
	vp = nfstcp_DQ();
	NFSQ_UNLOCK(s);
	return(vp);
}

/* 
 * hash a socket pointer into a map structure pointer.
 * Called with bucket lock held.
 */

static smap_t *
sotosmap(struct socket *so)
{
	int i;
	smap_t *smp;
	struct smapbuckets *bp;

	bp = &smapbuckets[SMAPHASH(so)];
	for (i = 0, smp = bp->head; i < bp->count && smp; i++, smp = smp->next)
		if (so == smp->so)
			return (smp);

	return ((smap_t *) 0);
}

int nfstcp_nosock = 0, nfstcp_inputs = 0, nfstcp_inputs_nodata = 0;
/* 
 * tcp_input callback function.  Called with socket locked.
 * Racing with nfstcp_addsock(), so append mbufs onto socket even
 * if we can't find the connection map.
 */
void
nfstcp_input(struct socket *so, struct mbuf *m)
{

#if defined(_MP_NETLOCKS)
	int ospl;
#endif
	smap_t *smp;

	if (m) {
		nfstcp_inputs++;
		sbappend(&so->so_rcv, m);
	} else if (!soreadable(so))
		return;
	else
		nfstcp_inputs_nodata++;
#ifdef _MP_NETLOCKS
	BUCK_LOCK(ospl, so);
#endif

	smp = sotosmap(so);

#ifdef _MP_NETLOCKS
	BUCK_UNLOCK(ospl, so);
#endif

	if (smp == (smap_t *) 0) {	/* shouldn't really happen */
		nfstcp_nosock++;
		return;
	}
	smp->lastbolt = lbolt;

#ifdef _MP_NETLOCKS
	NFSQ_LOCK(ospl);
#endif

	NQ(smp);

#ifdef _MP_NETLOCKS
	NFSQ_UNLOCK(ospl);
#endif

	nfs_input(NULL);	/* just for scheduling */
}

extern int krpc_adjust(struct mbuf **);
extern int krpc_peeklen(struct socket *, u_int *);
extern int ktcp_rcv(struct socket *, struct mbuf **, int);
extern int ktcp_timedsnd(struct socket *, struct mbuf *, int, int, int);

static int
nfstcp_rcv(smap_t *smp, struct mbuf **mp)
{
	register struct mbuf **mtail;
	int error = 0;
	register struct socket *so = smp->so;

	*mp = NULL;
	mutex_lock(&smp->sorcvlock, PZERO);
	
	if (so->so_error) {
		error = sblock(&so->so_rcv, NETEVENT_SOUP, so, 1);
		if (error)
			goto exit;
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
		}
		sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
		if (error)
			goto exit;
	}
	if (so->so_state & SS_CANTRCVMORE) {
		error = EPIPE;
		goto exit;
	}
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto exit;
	}
	if (sbspace(&so->so_snd) <= 0) {
		error = EOVERFLOW;
		goto exit;
	}
	ASSERT (smp->mrcv || smp->mrcv_len == 0);
	ASSERT (smp->mrcv == NULL || smp->mrcv_reclen);
	ASSERT (smp->mrcv_len == 0 || smp->mrcv_len >= sizeof(u_int));

	if (smp->mrcv_len == 0) {
		if (smp->so->so_rcv.sb_cc < sizeof(u_int))
			goto exit;
		error = krpc_peeklen(smp->so, &smp->mrcv_reclen);
		if (error)
			goto exit;
	}
	if (smp->mrcv == NULL)
		mtail = &smp->mrcv;
	else
		mtail = smp->mrcv_tail;
	if (error = ktcp_rcv(smp->so, mtail,
			     smp->mrcv_reclen + sizeof(u_int) - smp->mrcv_len))
		goto exit;
	while (*mtail) {
		smp->mrcv_len += (*mtail)->m_len;
		mtail = &(*mtail)->m_next;
	}
	smp->mrcv_tail = mtail;
	ASSERT (smp->mrcv_len >= sizeof(u_int));

	if (smp->mrcv_len < smp->mrcv_reclen + sizeof(u_int))
		goto exit;
	
	ASSERT(smp->mrcv_len == smp->mrcv_reclen + sizeof(u_int));
	if (error = krpc_adjust(&smp->mrcv))
		goto exit;

	if (smp->mrcv->m_len == 0 && smp->mrcv->m_next == NULL) {
		m_freem(smp->mrcv);
		smp->mrcv = NULL;
		smp->mrcv_len = 0;
		goto exit;
	}
	*mp = smp->mrcv;
	smp->mrcv = NULL;
	smp->mrcv_len = 0;
	mutex_unlock(&smp->sorcvlock);
	return 0;
	
      exit:
	mutex_unlock(&smp->sorcvlock);
	return error ? error : EWOULDBLOCK;
}

/* 
 * nfs/kernel tcp service handler
 */
/*ARGSUSED*/
void
nfstcp_service(SVCXPRT * xprt, void *vp, struct pernfsq *p)
{
	int currcc, lastcc, ospl, error = 0, space;
	struct mbuf *m;
	smap_t *smp = (smap_t *) vp;

	SMAP_HOLD(smp);

	if (smp->drop) {
		nfstcp_dropconn(smp);
		return;
	}
	/*
	 * Save the instantaneous receive buffer count for later use.
	 */
	lastcc = smp->so->so_rcv.sb_cc;

	error = nfstcp_rcv(smp, &m);
	ASSERT (error || m);

	switch (error) {
	case 0:
	case EWOULDBLOCK:	/* not enough data yet */
	case ENOBUFS:		/* bad data alignment, should be rare */
	case EOVERFLOW:		/* no room to enqueue reply on send buffer */
		/* 
		 * This is locking semantic rape, but works ONLY because
		 * Qflags has 3 states, ONCPU, ONQ or neither.
		 * We KNOW we are currently ONCPU.
		 */
		ASSERT(smp->Qflags == SMAP_ONCPU);
		smp->Qflags = 0;

		/*
		 * If there is more or new data on the socket, re-NQ the map.
		 * We may re-NQ the map when not necessary due to race issues,
		 * but this should not be a problem.
		 * Wakeup another thread to deal with the new data.
		 * I'm doing the reply.
		 *
		 * NB:  The order of first clearing Qflags, THEN checking
		 * receive buffer count is important! Otherwise a call to NQ()
		 * would think the smp was in use and not do the enqueue
		 * if the NQ comes in after the checks below, but before we
		 * cleared the Qflags.
		 */
		space = sbspace(&smp->so->so_snd);
		currcc = smp->so->so_rcv.sb_cc;
		if (currcc && space > 0 &&
		    (error != EWOULDBLOCK || currcc != lastcc))
		{
			NFSQ_LOCK(ospl);
			NQ(smp);
			NFSQ_UNLOCK(ospl);
			nfs_input(NULL);
		}
		if (error == 0) {
			xprt->xp_type = XPRT_TCP;
			xprt->xp_so = smp->so;
			xprt->xp_sosndlock = &smp->sosndlock;
			xprt->xp_raddr.sin_addr = smp->sin.sin_addr;
			xprt->xp_raddr.sin_port = smp->sin.sin_port;
			rpc_decode_req(xprt, m, p);
		}
		SMAP_RELE(smp);
		break;
	case ENOTCONN:		/* happens? */
	case EPIPE:		/* child disconnects */
	case ECONNRESET:	/* "" */
		nfstcp_dropconn(smp);
		break;
	default:
		cmn_err(CE_WARN, "NFS/TCP: service rcv error %d on socket\n", error);
		nfstcp_dropconn(smp);
	}
}

#define NFS_TCP_RECMARK	0x80000000

int
nfstcp_sendreply(SVCXPRT * xprt, struct mbuf *m)
{
	int error;
	u_int mlen;

	mutex_lock(xprt->xp_sosndlock, PZERO);
	/*
	 * Set the send-buffer's hiwat mark based upon the size of the
	 * IOs the client is requesting. Always allow a certain number of
	 * readaheads to fit on the outbound queue.
	 * Note: if sbreserve() is changed to cap sb_hiwat, this code
	 *       needs to be changed to not try to raise it continuously.
	 */
	mlen = m_length(m);
	if (BEST_SNDSIZE(mlen) > xprt->xp_so->so_snd.sb_hiwat) {
		SOCKET_LOCK(xprt->xp_so);
		if (BEST_SNDSIZE(mlen) > xprt->xp_so->so_snd.sb_hiwat)
			sbreserve(&xprt->xp_so->so_snd, BEST_SNDSIZE(mlen));
		SOCKET_UNLOCK(xprt->xp_so);
	}
	/* 
	 * Caller guarantees enough space in mbuf head to accomodate
	 * the record mark.
	 */
	m->m_off -= sizeof(u_int);
	m->m_len += sizeof(u_int);
	*mtod(m, u_int *) = mlen | NFS_TCP_RECMARK;

	if (error = ktcp_timedsnd(xprt->xp_so, m, mlen + sizeof(u_int), 0, 1)) {
		/* ktcp_timedsnd() will have freed 'm'. */
		if (error == ENOTCONN || error == EPIPE || error == ECONNRESET)
			nfstcp_dropsock(xprt->xp_so);
		else if (error == EWOULDBLOCK) {
			cmn_err(CE_WARN, "NFSTCP: got EWOULDBLOCK, dropped %d bytes\n", mlen);
		} else
			cmn_err(CE_WARN, "NFSTCP: service send error %d on socket\n", error);
	}
	mutex_unlock(xprt->xp_sosndlock);
	return (error);
}

#define NFSTCP_CONNTIMEOUT	20*60*HZ	/* 20 minutes */
int nfstcp_doreap;

/* 
 * Loop through all the connections, reaping those which haven't
 * received data in a long while, and those marked drop.
 */
void
nfstcp_reapconn(void)
{
	int i, ospl;
	smap_t *smp, *smpnext;
	struct smapbuckets *bp;

	for (i = 0, bp = smapbuckets; i < SMAPBUCKETS; i++, bp++) {
		ospl = mutex_spinlock(&bp->bucklock);
		for (smp = bp->head; smp;) {
			if (smp->Qflags == 0 &&	/* !ONCPU && !ONQ */
			    smp->holds == 0 &&
			    (smp->lastbolt + NFSTCP_CONNTIMEOUT < lbolt || 
			    smp->drop != 0)) {
				dropmap(smp);
				mutex_spinunlock(&bp->bucklock, ospl);
				bhv_remove(&(smp->vso->vs_bh), 
						&(smp->so->so_bhv));
				mutex_destroy(&smp->sorcvlock);
				mutex_destroy(&smp->sosndlock);
				vsocket_release(smp->vso);
				soclose(smp->so);
				if (smp->mrcv)
					m_freem(smp->mrcv);
				smpnext = smp->next;
				kmem_free(smp, sizeof(smap_t));
				smp = smpnext;
				ospl = mutex_spinlock(&bp->bucklock);
			} else {
				smp = smp->next;
			}
		}
		mutex_spinunlock(&bp->bucklock, ospl);
	}
}

#define NFSTCP_TIMEOUT		2*60*HZ		/* 2 minutes */

static
void nfstcp_timeout(void)
{
	atomicSetInt(&nfstcp_doreap, 1);
	/* 
	 * No reason to run a timein thread just to increment a variable.
	 * Since code can't sleep, just increment on interrupt stack.
	 */
	timeout_nothrd(nfstcp_timeout, (caddr_t) 0, NFSTCP_TIMEOUT);
}

void
svcktcp_init(void)
{
	int i;

	timeout_nothrd(nfstcp_timeout, (caddr_t) 0, NFSTCP_CONNTIMEOUT);
	mutex_init(&nfssvctcp_lock, MUTEX_DEFAULT, "nfssvctcp_lock");
	for (i = 0; i < SMAPBUCKETS; i++)
		BUCK_INITLOCK(smapbuckets[i].bucklock);

#ifdef DEBUG
	nfs_svcdbginit();
#endif
}

/* 
 * Remove a connection map from the smap list
 * Called with the bucket lock held.
 * Caller must free memory.
 */
static void
dropmap(smap_t * smp)
{
	struct smapbuckets *bp;

#ifdef NFSTCP_DEBUG
	cmn_err(CE_DEBUG, "NFSTCP: dropping sock 0%x from svr/port %s/%d\n",
		smp->so, inet_ntoa(smp->sin.sin_addr), smp->sin.sin_port);
#endif
	bp = &smapbuckets[SMAPHASH(smp->so)];

	ASSERT(smp->Qnext == 0);
	if (bp->head == smp)
		bp->head = smp->next;
	if (bp->tail == smp)
		bp->tail = smp->prev;
	if (smp->next)
		smp->next->prev = smp->prev;
	if (smp->prev)
		smp->prev->next = smp->next;
	bp->count--;
	totalsmaps--;
	if (bp->head == 0)
		ASSERT(bp->count == 0 && bp->tail == 0);
	if (bp->tail == 0)
		ASSERT(bp->count == 0 && bp->head == 0);
}

/* 
 * Mark a connection map to be dropped.
 * We're called with the connection still marked ONCPU
 * so we know that we're not ONQ and that there will be no
 * more holds.  There could be one (plus ours) or more
 * current holds.
 * The socket has to be closed last 'cause of lock ordering.
 */
static void
nfstcp_dropconn(smap_t * smp)
{
	int ospl;
	/* REFERENCED */
	struct smapbuckets *bp;
	struct socket *so;

	bp = &smapbuckets[SMAPHASH(smp->so)];
	ospl = mutex_spinlock(&bp->bucklock);
	SMAP_RELE(smp);
	if (smp->holds > 0) {
		smp->drop = 1;
		mutex_spinunlock(&bp->bucklock, ospl);
		return;
	}
	so = smp->so;
	dropmap(smp);
	mutex_spinunlock(&bp->bucklock, ospl);
	mutex_destroy(&smp->sorcvlock);
	mutex_destroy(&smp->sosndlock);
	bhv_remove(&(smp->vso->vs_bh), &(so->so_bhv));
	soclose(so);
	vsocket_release(smp->vso);
	if (smp->mrcv)
		m_freem(smp->mrcv);
	kmem_free(smp, sizeof(smap_t));
}

/* 
 * Mark a connection map (by socket addr) to be dropped.
 * We can't actually drop the map 'cause someone else could
 * be using it.
 */
static void
nfstcp_dropsock(struct socket *so)
{
	int ospl;
	smap_t *smp;

	BUCK_LOCK(ospl, so);
	smp = sotosmap(so);
	if (smp == (smap_t *) 0) {
		BUCK_UNLOCK(ospl, so);
		cmn_err(CE_WARN, "NFSTCP: socket 0%x doesn't map\n", so);
		return;
	}
	smp->drop = 1;
	BUCK_UNLOCK(ospl, so);
}

/* 
 * Add a new accepted socket to the list being polled by the nfsds.
 * Put ONQ, just in case race with nfstcp_input() lost, which is often
 * the case.
 */
static void
nfstcp_addsock(struct socket *so, vsock_t * vso, struct sockaddr_in *sin)
{
	int ospl, ospl2;
	smap_t *smp;
	struct smapbuckets *bp;

	if (totalsmaps >= maxsmaps) {
		bhv_remove(&(vso->vs_bh), &(so->so_bhv));
		soclose(so);
		vsocket_release(vso);
		cmn_err(CE_WARN, "NFSTCP: too many connections %d\n, dropped connection from %s\n", inet_ntoa(sin->sin_addr));
		return;
	}
	/*
	 * Make sure there are still nfsd's around to deal with this
	 * connection.
	 */
	mutex_lock(&nfssvctcp_lock, PZERO);
	if (nfssvctcp_init == 0) {
		mutex_unlock(&nfssvctcp_lock);
		bhv_remove(&(vso->vs_bh), &(so->so_bhv));
		soclose(so);
		vsocket_release(vso);
		return;
	}
	mutex_unlock(&nfssvctcp_lock);

	smp = kmem_zalloc(sizeof(smap_t), KM_SLEEP);
	mutex_init(&smp->sorcvlock, MUTEX_DEFAULT, "nfs3 sorcvlock");
	mutex_init(&smp->sosndlock, MUTEX_DEFAULT, "nfs3 sosndlock");
	smp->so = so;
	smp->vso = vso;
	smp->sin = *sin;
	smp->lastbolt = lbolt;
	BUCK_LOCK(ospl, so);
	bp = &smapbuckets[SMAPHASH(so)];
	if (bp->head == 0) {
		ASSERT(bp->count == 0);
		bp->head = smp;
	}
	if (bp->tail) {
		bp->tail->next = smp;
		smp->prev = bp->tail;
	} else
		ASSERT(bp->count == 0);

	bp->tail = smp;
	bp->count++;
	totalsmaps++;

	/*
	 * I'd like to drop the bucket lock first, but there always is
	 * the chance that smp could then point to kmem_free'd() memory 
	 * if we happen to be in a race with an nfsd wanting to drop the 
	 * connection for whatever reason.
	 */
	NFSQ_LOCK(ospl2);
	NQ(smp);
	NFSQ_UNLOCK(ospl2);

	BUCK_UNLOCK(ospl, so);
	nfs_input(NULL);
}

/* 
 * Remove the smaps.  
 * Called with nfsd_count_lock held, so no other daemons are manipulating
 * the maps.  nfstcp_input() may be called to NQ more input, however,
 * which we neutralize by clearing nfssvctcp_init.
 * The tip-toe through the lock (mine) field is because nfstcp_input()
 * holds a socket lock, which prevents us (deadlock) from holding
 * the bucket lock while closing the socket.
 */
void
nfstcp_stop(void)
{
	int ospl, n, i;
	smap_t *smp, *m;
	struct smapbuckets *bp;

	mutex_lock(&nfssvctcp_lock, PZERO);
	if (nfssvctcp_init == 0) {
		mutex_unlock(&nfssvctcp_lock);
		return;
	}

	/*
	 * First, turn off enqueuing then get everything off the active Q
	 */
	NFSQ_LOCK(ospl);
	nfssvctcp_init = 0;
	while (nfstcp_DQ() != (void *) 0);
	NFSQ_UNLOCK(ospl);
	ASSERT(smapQhead == 0);
	ASSERT(smapQtail == 0);
	ASSERT(smapQlen == 0);

	/*
	 * Now free the smaps, bucket by bucket.
	 */
	for (i = 0, bp = smapbuckets; i < SMAPBUCKETS; i++, bp++) {
		ospl = mutex_spinlock(&bp->bucklock);
		smp = bp->head;
		n = bp->count;
		bp->count = 0;
		bp->head = (smap_t *) 0;
		bp->tail = (smap_t *) 0;
		mutex_spinunlock(&bp->bucklock, ospl);
		for (; smp && n > 0; n--, totalsmaps--) {
			mutex_destroy(&smp->sorcvlock);
			mutex_destroy(&smp->sosndlock);
			bhv_remove(&(smp->vso->vs_bh), &(smp->so->so_bhv));
			soclose(smp->so);
			vsocket_release(smp->vso);
			if (smp->mrcv)
				m_freem(smp->mrcv);
			m = smp;
			smp = smp->next;
			kmem_free(m, sizeof(smap_t));
		}
		ASSERT(smp == 0 && n == 0);
	}
	ASSERT(totalsmaps == 0);
	mutex_unlock(&nfssvctcp_lock);
}

/* 
 * Initialize NFS service on a bound tcp socket.
 * Listens for connections, then runs an infinite loop accepting
 * new connections and registering them in the socket map.
 * The acception part could be done via polling by any of the nfsd's.
 * Perhaps even a socket callback function?
 */
extern u_int soreserve_size;
int
nfstcp_start(vsock_t * vso, int nconn)
{
	struct socket *so, *newso;
	int error, n, i;
	vsock_t *nvso;
	struct sockaddr_in sin;
	struct mbuf *nam;

	mutex_lock(&nfssvctcp_lock, PZERO);
	if (nfssvctcp_init != 0) {
		mutex_unlock(&nfssvctcp_lock);
		cmn_err(CE_WARN, "NFSTCP: start initialization race?\n");
		return EINVAL;
	}
	if (nconn > 0)
		n = nconn;
	else
		n = nfs_maxconn;

	so = (struct socket *) (BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	SOCKET_LOCK(so);
	so->so_state |= SS_NBIO;
	so->so_input = nfstcp_input;
	so->so_rcv.sb_flags |= SB_INPUT_ACK;

        /*
         * Set the recv & send window sizes, plus nodelay.
         */
	(void) soreserve(so, soreserve_size, soreserve_size);
	SOCKET_UNLOCK(so);
	VSOP_LISTEN(vso, n / 4, error);
	if (error) {
		mutex_unlock(&nfssvctcp_lock);
		return(error);
	}

        nam = m_get(M_WAIT, MT_SOOPTS);
        if (!nam) {
		mutex_unlock(&nfssvctcp_lock);
                return (ENOBUFS);
        }
        *mtod(nam, int*) = 1;
        nam->m_len = sizeof(int);
        VSOP_SETATTR(vso, IPPROTO_TCP, TCP_NODELAY, nam, error);

	ASSERT(totalsmaps == 0);
	for (i = 0; i < SMAPBUCKETS; i++) {
		ASSERT(smapbuckets[i].head == 0);
		ASSERT(smapbuckets[i].tail == 0);
		ASSERT(smapbuckets[i].count == 0);
	}

	maxsmaps = n;
	nfssvctcp_init++;
	mutex_unlock(&nfssvctcp_lock);

	for (;;) {
		if (nfstcp_accept(so, &newso, &sin, &nvso))
			break;
		nfstcp_addsock(newso, nvso, &sin);
	}
	return 0;
}

/* 
 * This is essentially a re-written accept() for kernel internal use.
 * This could be done non-blocking, allowing us to forgo a dedicated
 * thread just for connection acceptance.
 */
static int
nfstcp_accept(struct socket *so,
		 struct socket **newso,
		 struct sockaddr_in *sin,
		 vsock_t ** nvso)
{
	struct socket *aso;
	int error = 0;
	NETSPL_DECL(s)
	  struct mbuf *nam;
	vsock_t *vso;
	extern int soaccept(struct socket *, struct mbuf *);

      restart:
	SOCKET_QLOCK(so, s);
	while (so->so_qlen == 0 && so->so_error == 0) {
		int rv;

		if (so->so_state & SS_CANTRCVMORE) {
			so->so_error = ECONNABORTED;
			break;
		}
		rv = SOCKET_WAIT_SIG(&so->so_timeosem, PZERO + 1,
				     &so->so_qlock, s);
		if (rv) {
			return EINTR;
		}
		SOCKET_QLOCK(so, s);
	}
	if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		SOCKET_QUNLOCK(so, s);
		return error;
	}
	aso = so->so_q;

#ifdef _MP_NETLOCKS
	if (!SOCKET_TRYLOCK(aso)) {
		SO_HOLD(aso);
		SOCKET_QUNLOCK(so, s);
		SOCKET_LOCK(aso);
		if (SO_RELE(aso) == 0) {
			SOCKET_UNLOCK(aso);
			goto restart;
		}
		if (aso->so_head == 0) {
			SOCKET_UNLOCK(aso);
			goto restart;
		}
		SOCKET_QLOCK(so, s);
		ASSERT(aso->so_head);
	}
#endif				/* _MP_NETLOCKS */

	error = vsowrap(aso, &vso, AF_INET, aso->so_type,
			aso->so_proto->pr_protocol);
	if (error) {
		/*
		 * Retry on error from vsowrap().  Since the socket was
		 * already created, this should be space-related only.
		 */
		SOCKET_UNLOCK(aso);
		SOCKET_QUNLOCK(so, s);
		goto restart;
	}
	if (soqremque(aso, 1) == 0)
		panic("nfstcp_accept");

	SOCKET_QUNLOCK(so, s);

	aso->so_state &= ~SS_NOFDREF;

	nam = m_get(M_WAIT, MT_SONAME);
	if (aso->so_pcb == 0) {
		aso->so_state |= SS_NOFDREF;
		bhv_remove(&(vso->vs_bh), &(aso->so_bhv));
		SOCKET_UNLOCK(aso);
		soclose(aso);
		m_freem(nam);
		vsocket_release(vso);
		goto restart;
	}

#ifdef DEBUG
	error = soaccept(aso, nam);
	ASSERT(!error);
#else
	(void) soaccept(aso, nam);
#endif				/* DEBUG */

	SOCKET_UNLOCK(aso/*, s*/);
/* 
 * NB:  removed sgi auditing code.
 */
	/* XXX why would this ever need to be pulled up? */
	if ((nam->m_len < sizeof(*sin)) &&
	    (nam = m_pullup(nam, sizeof(*sin))) == 0) {
		bhv_remove(&(vso->vs_bh), &(aso->so_bhv));
		soclose(aso);
		vsocket_release(vso);
		goto restart;
	}
	bcopy(mtod(nam, caddr_t), (caddr_t) sin, sizeof(*sin));
	m_freem(nam);

#ifdef NFSTCP_DEBUG
	cmn_err(CE_DEBUG, "nfstcp_accept(): new sock %x from svr/port %s/%d\n",
		aso, inet_ntoa(sin->sin_addr), sin->sin_port);
#endif

	*newso = aso;
	*nvso = vso;
	return error;
}

#ifdef DEBUG
void
idbg_smapdump(__psint_t addr)
{
	int i,
	 j;
	smap_t *smp;
	struct smapbuckets *bp;
	int ospl;

	j = (int) addr;
	if (j < 0 || j > totalsmaps)
		qprintf("invalid num %d, defaulting to %d\n", j, j = totalsmaps);
	qprintf("nfstcp server connections:\n");
	for (i = 0, bp = smapbuckets; i < SMAPBUCKETS; i++, bp++) {
		ospl = mutex_spinlock(&bp->bucklock);
		for (smp = bp->head, j = 0; smp && j < bp->count; j++, smp = smp->next) {
			if (smp->so != 0) {
				qprintf("(%d:%d): 0x%x so=0x%x vso=%s/%d %x\n", i, j, smp,
					smp->so, smp->vso, inet_ntoa(smp->sin.sin_addr),
					smp->sin.sin_port, smp->Qflags);
			}
		}
		mutex_spinunlock(&bp->bucklock, ospl);
	}
	qprintf("nfstcp server Q:\n");
	NFSQ_LOCK(ospl);
	for (smp = smapQhead; smp; smp = smp->Qnext)
		qprintf("%s/%x\n", inet_ntoa(smp->sin.sin_addr),
			smp->sin.sin_port);
	NFSQ_UNLOCK(ospl);
	qprintf("nfstcp_onQ: %d, nfstcp_newdata: %d, nfstcp_nosock: %d\n",
		nfstcp_onq, nfstcp_newdata, nfstcp_nosock);
	qprintf("nfstcp_nqs: %d, nfstcp_dqs: %d, nfstcp_inputs: %d, nfstcp_inputs_nodata: %d\n",
		nfstcp_nqs, nfstcp_dqs, nfstcp_inputs, nfstcp_inputs_nodata);
}

static void
nfs_svcdbginit(void)
{
	idbg_addfunc("smapdump", idbg_smapdump);
}

#endif				/* DEBUG */
