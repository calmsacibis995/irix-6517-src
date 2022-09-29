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
 * nfs_tcp.c
 * Do nfs calls over tcp.
 * NB: Does NOT implement the traditional CLNT interface.
 */

#include "types.h"
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/ksignal.h>
#include <sys/mbuf.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/atomic_ops.h>
#include <sys/sesmgr.h>
#include <sys/vfs.h>
#include <sys/protosw.h>
#include <sys/cmn_err.h>
#include <netinet/in.h>
#include "xdr.h"
#include "auth.h"
#include "clnt.h"
#include "rpc_msg.h"
#include "nfs_stat.h"
#include "nfs.h"
#include "nfs_clnt.h"

#ifdef DEBUG
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#endif

u_int soreserve_size = 256 * 1024;

typedef struct nfsreq nfsreq_t;
/* 
 * Data in the nfsreq structure is overtly protected by its NOT
 * being on the connection's request list (which IS locked).
 */
struct nfsreq {
	nfsreq_t *nfsr_next;	/* chain of requests */
	nfsreq_t *nfsr_prev;	/* locked by connection */
	int nfsr_count;		/* ref count on ... */
	char *nfsr_hdrmem;	/* clustered header kmem */
	struct rpc_msg nfsr_callmsg;	/* call msg for this request */
	struct mbuf *nfsr_mreq;	/* send request */
	int nfsr_mlen;		/* length of request */
	struct mbuf *nfsr_mrep;	/* received reply */
	u_int nfsr_status;	/* posted errors on send/rcv */
	struct cred *nfsr_cred;	/* saved user creds */
	AUTH *nfsr_auth;	/* authentication */
};

#define NFSR_OK		0x0	/* Success */
#define NFSR_RESEND	0x01	/* Forced re-xmit if reconnect */
typedef struct nfscon nfscon_t;
struct nfscon {
	nfscon_t *nfsc_prev;	/* chain of connections */
	nfscon_t *nfsc_next;	/* locked by nfscon_headlock */

	mutex_t nfsc_flaglock;	/* mutex for flags */
	u_int nfsc_flags;	/* Send/Rcv want flags */
	struct sockaddr_in nfsc_svraddr;	/* server's ip # & port */
	sv_t nfsc_rcvsig;	/* serialized rpc rcv & connect */
	sv_t nfsc_sndsig;	/* serialized rpc send */
	mutex_t nfsc_socklock;	/* mutex for socket connect */
	struct vsocket *nfsc_vsock;	/* short, thin miscellaneous connection */
	u_int nfsc_count;	/* references (protected by headlock) */

	mutex_t nfsc_reqlock;	/* mutex for request count/Qs */
	u_int nfsc_nreqs;	/* NQ'd requests (flaglock) */
	nfsreq_t *nfsc_reqtail;	/* tail of request list for sockets */
	nfsreq_t *nfsc_reqhead;	/* head of request list for sockets */

};

/* nfscon_t flags */
#define NFSC_RCVLOCK 	0x1	/* serialize receivers */
#define NFSC_SNDLOCK 	0x2	/* serialize senders */
#define NFSC_RCVWANT 	0x8	/* resource wanted */
#define NFSC_SNDWANT 	0x10

#define DBG_UNLOCK	0x20	/* added to LOCK bits, means unlock */
				/* added to WANT bits, means wakeup */
#define DBG_CONLOCK	0x40
#define DBG_CONUNLOCK	0x41
#define DBG_REQRESND	0x42


nfscon_t *nfscon_head;		/* head of list */
mutex_t nfscon_headlock;	/* mutex for list */

#ifdef DEBUG
static void dbg_statechange(int);
static void dbg_reqlist(nfscon_t *);
static void dbg_conlist(void);
static void prstate(int);
#else
#define dbg_statechange(x)
#endif


/* 
 * Client request creation/deletion and en(de)queuing.
 */
static nfsreq_t *nfsreq_create(struct mntinfo *, u_long, struct cred *,
			       xdrproc_t, caddr_t);
static void nfsreq_destroy(nfscon_t *, nfsreq_t *);
static void nfsreq_DQ(nfscon_t *, nfsreq_t *);
static void nfsreq_NQ(nfscon_t *, nfsreq_t *);

/* 
 * Client connection locking.  While the socket is locked during
 * actual data transfers, access is serialized to each end of
 * the connection as well.  This ensures duplex operation, full packet 
 * reception, and helps with error recovery/reconnects.
 */
static void sndlock(nfscon_t * con);
static void sndunlock(nfscon_t * con);
static int rcvlock(nfscon_t * con, nfsreq_t * req);
static void rcvunlock(nfscon_t * con, int locked);

/* 
 * Lower-level rpc/tcp socket/mbuf routines.
 */
int ktcp_rcv(struct socket *, struct mbuf **, int);
static int krpc_timedrcv(struct socket *, struct mbuf **, int, int);
int ktcp_timedsnd(struct socket *, struct mbuf *, int, int, int);
int krpc_peeklen(struct socket *, u_int *);
int krpc_adjust(struct mbuf **);
static void ktcp_disconnect(struct socket *);

/* 
 * Client nfs connection establishment and message passing routines.
 */
static int nfs_connect(nfscon_t *, int);
static int nfs_getreply(struct mntinfo *, nfsreq_t *, struct mbuf **);
static int nfs_sendreq(struct mntinfo *, nfsreq_t *);
static void nfsrep_post(nfscon_t *, xdr_u_long_t, struct mbuf *);
static int nfsreq_decode(nfsreq_t *, struct mbuf *, xdrproc_t, caddr_t);

static xdr_u_long_t	nfstcpxid;	/* that'd be a u_int32_t */
#define alloc_xid()     atomicAddInt((int *)&nfstcpxid, 1)

/* 
 * at connectathon 97, we shrunk this to 500 to work with auspex
 * Netapp had calculated the size to about 450 or so, and 
 * was rejecting packets bigger than 32k+that
 */
#define NFS_MAX_CALLHDR	500
#define NFS_MAX_PACKET	256*1024
#define NFS_TCP_RECMARK	0x80000000

#define LOCK_INIT(x, y)	mutex_init((x), MUTEX_DEFAULT, y)
#define LOCK(x)		mutex_lock((x), PZERO)
#define UNLOCK(x)	mutex_unlock((x))
#define WAIT(x, y, z, s)	sv_wait((x), y, (z), 0)
#define LOCK_DESTROY(x)	mutex_destroy(x);

#ifdef DEBUG
static void nfstcp_dbginit(void);
#endif

static int init_done = 0;
void nfstcp_init(void)
{
	if (init_done++)
		return;
	mutex_init(&nfscon_headlock, MUTEX_DEFAULT, "nfscon_headlock");
	/*
	 * See comment preceeding clntkudpxid_init().
	 */
	nfstcpxid = (0x3fffffff & ((time & 0x7ffff) << 11));

#ifdef DEBUG
	nfstcp_dbginit();
#endif
}

void
assign_nfssigset(k_sigset_t * oldsetp)
{

#if     (_MIPS_SIM != _ABIO32)
	k_sigset_t newset =
	{
		~(sigmask(SIGHUP) | sigmask(SIGINT) | sigmask(SIGQUIT) |
		  sigmask(SIGKILL) | sigmask(SIGTERM) | sigmask(SIGTSTP))
	};
#else				/* (_MIPS_SIM != _ABIO32) */
	k_sigset_t newset =
	{
		~(sigmask(SIGHUP) | sigmask(SIGINT) | sigmask(SIGQUIT) |
		  sigmask(SIGKILL) | sigmask(SIGTERM) | sigmask(SIGTSTP)), 0xffffffff
	};
#endif				/* (_MIPS_SIM != _ABIO32) */

	assign_cursighold(oldsetp, &newset);
}

int
rfscall_tcp(struct mntinfo *mi,
		int proc,
		xdrproc_t xdrargs,
		caddr_t argsp,
		xdrproc_t xdrres,
		caddr_t resp,
		struct cred *cred,
		int frombio
)
{
	int error, tryagain;
	nfsreq_t *req;
	struct mbuf *m0 = NULL;
	int rtries = mi->mi_retrans;
	k_sigset_t cursigset;


	RCSTAT.rccalls++;

	/*
	 * Create a request structure, encoding arguments.
	 */
	req = nfsreq_create(mi, proc, cred, xdrargs, argsp);
	if (req == 0) {
		RCSTAT.rcbadcalls++;
		return (ENOBUFS);
	}


	/*
	 * Loop until a successful reply, timeout, interrupt or some hard
	 * connection error. 
	 */
	do {
		tryagain = FALSE;

		/* if interruptible mount, have process catch appropriate signals. */
		if ((mi->mi_flags & MIF_INT) && curuthread)
			assign_nfssigset(&cursigset);
		error = nfs_sendreq(mi, req);
		if (!error)
			error = nfs_getreply(mi, req, &m0);
		if (!error)
			error = nfsreq_decode(req, m0, xdrres, resp);
		/* go back to process signals */
		if ((mi->mi_flags & MIF_INT) && curuthread)
			assign_cursighold(NULL, &cursigset);

		switch (error) {
		case 0:
			/* Got a message through -- there is a server */
			
			if (mi->mi_flags & MIF_NO_SERVER)
				atomicClearUint(&mi->mi_flags, MIF_NO_SERVER);
			break;
		case EIO:	/* hard rpc error */
			break;
		case ENOTCONN:	/* `soft' disconnect errors */
		case EPIPE:
		case ECONNRESET:
		case ETIMEDOUT:	/* no response error */
		case ECONNREFUSED:	/* `hard' connect error */
		case EAGAIN:	/* rcv reconnect or misc rpc error */
			/* No response from server, no server? */
			if (!(mi->mi_flags & MIF_NO_SERVER))
				atomicSetUint(&mi->mi_flags, MIF_NO_SERVER);
			
			if (((mi->mi_flags & MIF_HARD) && !frombio) || rtries-- > 0) {
				if (error != ETIMEDOUT)
					delay(HZ);
				tryagain = TRUE;
				RCSTAT.rcretrans++;
			}
			break;
		case EINTR:
			ASSERT(mi->mi_flags & MIF_INT);
			break;
		default:
			cmn_err(CE_DEBUG, "rfscall_tcp(): unknown error %d\n", error);
			break;
		}

	} while (tryagain == TRUE);
	
	/*
	 * If we've timedout, then tcp could still be holding
	 * retransmit mbuf clusters and we must force it to 
	 * release these by nuking the socket.
	 * (This is actually true only for WRITE ops
	 * but it doesn't hurt to kill the connection anyway). 
	 */
	if (error == ETIMEDOUT) {
		sndlock(mi->mi_con);
		RCSTAT.rctimeouts++;
		(void)nfs_connect(mi->mi_con, 1);
		sndunlock(mi->mi_con);
	}

	nfsreq_destroy((nfscon_t *) mi->mi_con, req);

	if ( error != 0 )
		RCSTAT.rcbadcalls++;

	return (error);
}

static int
nfsreq_decode(nfsreq_t * req, 
			 struct mbuf *m0,
			 xdrproc_t xdrres,
			 caddr_t resp
)
{
	XDR xdrs;
	struct rpc_msg reply_msg;
	struct rpc_err rpcerr;

	rpcerr.re_errno = 0;
	ASSERT(m0);
	xdrmbuf_init(&xdrs, m0, XDR_DECODE);

	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = resp;
	reply_msg.acpted_rply.ar_results.proc = xdrres;

	/*
	 * Decode and validate the response.
	 */
	if (xdr_replymsg(&xdrs, &reply_msg)) {
		_seterr_reply(&reply_msg, &rpcerr);
		if (rpcerr.re_status == RPC_SUCCESS) {
			/*
			 * Reply is good, check auth.
			 */
			if (!AUTH_VALIDATE(req->nfsr_auth, reply_msg.acpted_rply.ar_verf)) {
				rpcerr.re_status = RPC_AUTHERROR;
				rpcerr.re_why = AUTH_INVALIDRESP;
				RCSTAT.rcbadverfs++;
			}
			if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
				/* free auth handle */
				xdrs.x_op = XDR_FREE;
				(void) xdr_opaque_auth(&xdrs, &(reply_msg.acpted_rply.ar_verf));
			}
		} else {

#ifdef notdef
			/*
			 * Maybe our credentials need to be refreshed
			 */
			if (refreshes > 0 && AUTH_REFRESH(h->cl_auth)) {
				refreshes--;
				RCSTAT.rcnewcreds++;
			}
#endif
		}
	} else
		rpcerr.re_status = RPC_CANTDECODERES;

	switch (rpcerr.re_status) {
	case RPC_SUCCESS:
		break;
	case RPC_VERSMISMATCH:
	case RPC_AUTHERROR:
	case RPC_CANTENCODEARGS:
	case RPC_CANTDECODERES:
	case RPC_PROGVERSMISMATCH:
	case RPC_CANTDECODEARGS:
	case RPC_PROCUNAVAIL:
	case RPC_PROGUNAVAIL:
		rpcerr.re_errno = EIO;
		break;
	case RPC_SYSTEMERROR:
	case RPC_FAILED:
	default:
		rpcerr.re_errno = EAGAIN;
		break;
	}
	m_freem(m0);

#ifdef NFSTCP_DEBUG
	if (rpcerr.re_errno)
		printf("nfsreq_decode(): re_errno =  %d, re_status = %d\n",
		       rpcerr.re_errno, rpcerr.re_status);
#endif

	return (rpcerr.re_errno);
}

static bool_t
xdr_tcpcallhdr(register XDR * xdrs, register struct rpc_msg *cmsg)
{

	cmsg->rm_direction = CALL;
	cmsg->rm_call.cb_rpcvers = RPC_MSG_VERSION;
	if (
		   (xdrs->x_op == XDR_ENCODE) &&
		   xdr_u_long(xdrs, &(cmsg->rm_xid)) &&
		   xdr_enum(xdrs, (enum_t *) & (cmsg->rm_direction)) &&
		   xdr_u_long(xdrs, &(cmsg->rm_call.cb_rpcvers)) &&
		   xdr_u_long(xdrs, &(cmsg->rm_call.cb_prog)) &&
		   xdr_u_long(xdrs, &(cmsg->rm_call.cb_vers)) &&
		   xdr_u_long(xdrs, &(cmsg->rm_call.cb_proc)))
		return (TRUE);
	return (FALSE);
}

#ifdef NOTDEF
zone_t nfsreq_zone;
#endif

int nfsreq_allocs;

#ifdef DEBUG
/* ARGSUSED */
static void
reqbuf_dup(struct mbuf *m, inst_t callerspc)
#else
static void
reqbuf_dup(struct mbuf *m)
#endif
{
	nfsreq_t *req = (nfsreq_t *) m->m_darg;

	(void) atomicAddInt(&req->nfsr_count, 1);
}

#ifdef DEBUG
/* ARGSUSED */
static void
reqbuf_free(struct mbuf *m, inst_t callerspc)
#else
static void
reqbuf_free(struct mbuf *m)
#endif
{
	nfsreq_t *req = (nfsreq_t *) m->m_farg;

	if (atomicAddInt(&req->nfsr_count, -1) == 0) {
		if (req->nfsr_hdrmem) {
			kmem_free(req->nfsr_hdrmem, NFS_MAX_CALLHDR);
		}

#ifdef NOTYET
		kmem_zone_free(&nfsreq_zone, req);
#endif

		kmem_free(req, sizeof(nfsreq_t));
		atomicAddInt(&nfsreq_allocs, -1);
	}
}

/* 
 * Alloc nfs request structure.
 * Serialize rpc_msg into mbuf(s).  Call xdrags(argsp) for rest.
 */
static nfsreq_t *
nfsreq_create(struct mntinfo *mi,
	       u_long proc,
	       struct cred *cred,
	       xdrproc_t xdrargs,
	       caddr_t argsp
)
{
	struct mbuf *m;
	XDR xdrs;
	struct cred *tmpcred;
	nfsreq_t *req;
	u_int32 *recmark;
	int mlen;

#ifdef NOTDEF
	req = kmem_zone_zalloc(&nfsreq_zone, KM_SLEEP);
#endif

	req = kmem_zalloc(sizeof(nfsreq_t), KM_SLEEP);
	req->nfsr_callmsg.rm_xid = alloc_xid();
	req->nfsr_callmsg.rm_call.cb_prog = mi->mi_prog;
	req->nfsr_callmsg.rm_call.cb_vers = mi->mi_vers;
	req->nfsr_callmsg.rm_call.cb_proc = proc;

	req->nfsr_hdrmem = kmem_alloc(NFS_MAX_CALLHDR, KM_SLEEP);
	m = mclgetx(reqbuf_dup, (long) req, reqbuf_free, (long) req,
		req->nfsr_hdrmem, NFS_MAX_CALLHDR, M_WAIT);
	req->nfsr_count = 1;

	/*
	 * XXX: offset mbuf data for tcp/ip headers?
	 * xdr, c'est bullshit.
	 */
	xdrmbuf_init(&xdrs, m, XDR_ENCODE);
	recmark = mtod(m, u_int32 *);
	XDR_SETPOS(&xdrs, sizeof(u_int32));

	/* 
	 * This authentication stuff needs help
	 */
	req->nfsr_auth = authkern_create();
	req->nfsr_cred = crdup(cred);

	tmpcred = get_current_cred();
	set_current_cred(req->nfsr_cred);

	if (!xdr_tcpcallhdr(&xdrs, &req->nfsr_callmsg) ||
	    !AUTH_MARSHALL(req->nfsr_auth, &xdrs) ||
	    !((bool_t(*)(XDR *, caddr_t)) * xdrargs) (&xdrs, argsp)) {
		set_current_cred(tmpcred);

#ifdef NOTDEF
		kmem_zone_free(&nfsreq_zone, req);
#endif

		(void) m_freem(m);
		return ((nfsreq_t *) 0);
	}
	set_current_cred(tmpcred);

	/*
	 * There is only one mbuf for everything but writes. 
	 */
	if (m->m_next == 0)
		m->m_len = XDR_GETPOS(&xdrs);
	mlen = m_length(m);
	*recmark = NFS_TCP_RECMARK | (mlen - sizeof(u_int32));

	req->nfsr_mreq = m;
	req->nfsr_mlen = mlen;
	nfsreq_NQ((nfscon_t *) mi->mi_con, req);
	atomicAddInt(&nfsreq_allocs, 1);
	return (req);
}

#ifdef DEBUG
#define MAXREQCOUNT 10
int reqcount[MAXREQCOUNT];
#endif

/* 
 * Release that portion of the req structure we can.  The actual 
 * cluster mbuf with header memory may still be in use somewhere 
 * (via m_copy). That memory, together with the nfsreq struct,
 * is freed by cluster callback.
 */
static void
nfsreq_destroy(nfscon_t * con, nfsreq_t * req)
{

#ifdef DEBUG
	int index = MIN(req->nfsr_count, MAXREQCOUNT - 1);
	reqcount[index]++;
#endif

	nfsreq_DQ(con, req);
	if (req->nfsr_cred)
		crfree(req->nfsr_cred);
	if (req->nfsr_auth)
		AUTH_DESTROY(req->nfsr_auth);
/* 
 * ASSERT (req->nfsr_mrep == 0); if (NFSWRITE(req->nfsr_callmsg)) { int
 * ospl = mutex_spinlock(); sigwait; } */
	if (req->nfsr_mreq)
		m_freem(req->nfsr_mreq);
	/* req may live no longer */
}

#ifdef DEBUG
static void
dbg_reqlist(nfscon_t * con)
{
	nfsreq_t *tp;

	LOCK(&con->nfsc_reqlock);
	for (tp = con->nfsc_reqhead; tp; tp = tp->nfsr_next) {
		qprintf("req 0x%x\n", tp);
	}
	UNLOCK(&con->nfsc_reqlock);
}
static void dbg_conlist(void)
{
	nfscon_t *np;

	/* not locking connection list */
	for (np = nfscon_head; np; np = np->nfsc_next) {
		qprintf("con 0x%x: flags 0x%x nreqs %d\n", np, np->nfsc_flags, np->nfsc_nreqs);
		qprintf("svr/port %x/%d\n", np->nfsc_svraddr.sin_addr.s_addr, np->nfsc_svraddr.sin_port);
		dbg_reqlist(np);
	}
}
#endif

/* 
 * Remove a request from the list.
 */
static void
nfsreq_DQ(nfscon_t * con, nfsreq_t * req)
{
	nfsreq_t *tp;

	LOCK(&con->nfsc_reqlock);
	ASSERT(con->nfsc_nreqs > 0);
	ASSERT(con->nfsc_reqhead != NULL);
	ASSERT(con->nfsc_reqtail != NULL);
	for (tp = con->nfsc_reqhead; tp; tp = tp->nfsr_next) {
		if (tp == req) {
			if (tp->nfsr_next)
				tp->nfsr_next->nfsr_prev = tp->nfsr_prev;
			if (tp->nfsr_prev)
				tp->nfsr_prev->nfsr_next = tp->nfsr_next;
			break;
		}
	}
	ASSERT(tp);
	if (con->nfsc_reqhead == req)
		con->nfsc_reqhead = req->nfsr_next;
	if (con->nfsc_reqtail == req)
		con->nfsc_reqtail = req->nfsr_prev;

	con->nfsc_nreqs--;

#ifdef DEBUG
	req->nfsr_next = req->nfsr_prev = NULL;
#endif

	UNLOCK(&con->nfsc_reqlock);
}

/* 
 * Append a request to the end of the list.
 */
static void
nfsreq_NQ(nfscon_t * con, nfsreq_t * req)
{
	nfsreq_t *tp;

#ifdef DEBUG
	req->nfsr_next = req->nfsr_prev = NULL;
#endif

	LOCK(&con->nfsc_reqlock);
	if (con->nfsc_reqhead == NULL) {
		ASSERT(con->nfsc_reqtail == NULL);
		ASSERT(con->nfsc_nreqs == 0);
		con->nfsc_reqhead = req;
	} else {
		tp = con->nfsc_reqtail;
		tp->nfsr_next = req;
		req->nfsr_prev = tp;
	}
	con->nfsc_reqtail = req;
	con->nfsc_nreqs++;

	UNLOCK(&con->nfsc_reqlock);
}

/* 
 * Post the reply mbuf chain onto the nfs request struct matching xid.
 * Called with rcv lock held.
 */
static void
nfsrep_post(nfscon_t * con, xdr_u_long_t xid, struct mbuf *m)
{
	int found = 0;
	nfsreq_t *req;

	LOCK(&con->nfsc_reqlock);
	for (req = con->nfsc_reqhead; req; req = req->nfsr_next) {
		if (xid == req->nfsr_callmsg.rm_xid) {
			if (req->nfsr_mrep == (struct mbuf *) NULL) {
				req->nfsr_mrep = m;
				found = 1;
				break;
			}
		}
	}
	if (!found) {
		RCSTAT.rcbadxids++;
		m_freem(m);

#ifdef NFSTCP_DEBUG
		printf("nfsrep_attach(): dropping reply packet 0x%x\n", xid);
#endif
	} else {
		(void) sv_broadcast(&con->nfsc_rcvsig);		/* bummer! */
		dbg_statechange(DBG_UNLOCK | NFSC_RCVWANT);
	}

	UNLOCK(&con->nfsc_reqlock);
}

static int
nfs_sendreq(struct mntinfo *mi, nfsreq_t * req)
{
	nfscon_t *con = (nfscon_t *) mi->mi_con;
	struct socket *so;
	int error;
	struct mbuf *m;
	/* this is effectivly 100 * the udp timeout */
	int timohz = (mi->mi_timeo * HZ) * 10;

	/*
	 * XXX: gotta get rid of this!
	 */
	m = m_copy(req->nfsr_mreq, 0, M_COPYALL);
	if (!m)
		return (ENOBUFS);

	/* 
	 * Serialize senders.  This isn't necessary for the socket, 
	 * per se, but makes the reconnection state easier.
	 */
	sndlock(con);

	req->nfsr_status &= ~NFSR_RESEND;
	if (con->nfsc_vsock) {
		so = (struct socket *) 
		    (BHV_PDATA(VSOCK_TO_FIRST_BHV(con->nfsc_vsock)));
		error = ktcp_timedsnd(so, m, req->nfsr_mlen, timohz, 
		    !!(mi->mi_flags & MIF_INT));
	} else {
		error = ENOTCONN;
		m_freem(m);
	}
	/* ktcp_timedsnd() always frees 'm' */
	if (error == ENOTCONN || error == EPIPE || error == ECONNRESET) {
		error = nfs_connect(con, 0);
		if (!error)
			error = EAGAIN;
	}
	sndunlock(con);

#ifdef NFSTCP_DEBUG
	if (error)
		printf("nfs_sendreq(): error = %d\n", error);
#endif

	return (error);
}

/* 
 * Get my reply off the socket or wait until someone else receives it for me.
 */
static int
nfs_getreply(struct mntinfo *mi, nfsreq_t * req, struct mbuf **mpp)
{
	struct socket *so;
	nfscon_t *con = (nfscon_t *) mi->mi_con;
	int timohz = (mi->mi_timeo * HZ) * 10;
	int intr = !!(mi->mi_flags & MIF_INT);
	int error = 0;
	int locked;
	xdr_u_long_t xid;
	struct mbuf *m;

	locked = rcvlock(con, req);

	/*
	 * Check for reply without waiting for socklock.
	 * It is OK to check for nfsr_mrep here, and not after
	 * getting socklock, because rcvlock above must have
	 * either found nfsr_mrep set to non-NULL, or we now
	 * have the full rcvlock.
	 */

	if (req->nfsr_mrep) {	/* reply already received */
		*mpp = req->nfsr_mrep;
		req->nfsr_mrep = NULL;
		goto out;
	} else if (req->nfsr_status & NFSR_RESEND) {
		error = EAGAIN;	/* connection reset */
		goto out;
	} else if (con->nfsc_vsock == NULL) {
		error = ENOTCONN;
		goto out;
	}

	mutex_lock(&con->nfsc_socklock, PZERO);
	if (req->nfsr_status & NFSR_RESEND) {
		error = EAGAIN;	/* connection reset */
	} else if (con->nfsc_vsock == NULL) {
		error = ENOTCONN;
	} else {
		so = (struct socket *) 
		    (BHV_PDATA(VSOCK_TO_FIRST_BHV(con->nfsc_vsock)));
		while ((error = krpc_timedrcv(so, &m, timohz, intr)) == 0) {
			/* 
			 * Sneak a peek at the reply xid. If it's mine, we're done.
			 * If it's someone else's, post it for them and continue.
			 */
			if (m->m_len < sizeof(xdr_u_long_t))
				m = m_pullup(m, sizeof(xdr_u_long_t));
			if (m != (struct mbuf *) NULL) {
				xid = *mtod(m, xdr_u_long_t *);
				if (xid == req->nfsr_callmsg.rm_xid) {
					*mpp = m;
					break;
				} else
					nfsrep_post(con, xid, m);
			}
		}
	}
	mutex_unlock(&con->nfsc_socklock);
out:
	rcvunlock(con, locked);

#ifdef NFSTCP_DEBUG
	if (error)
		printf("nfs_getreply(): error %d\n", error);
#endif

	return (error);
}

/*
 * Take a peek at the record mark, which should be the next 4 bytes
 * in the mbuf.
 */
int
krpc_peeklen(struct socket *so, u_int *reclen)
{
	uio_t uio;
	iovec_t iov;
	int error;
	int flags = MSG_PEEK | MSG_DONTWAIT;
	u_int len, marker;

	iov.iov_base = &marker;
	iov.iov_len = sizeof(u_int);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = sizeof(u_int);
	/*
	 * Just peek the record mark/length.  This allows soreceive()
	 * to avoids calling tcp_output().  A "good thing".
	 */
	error = soreceive(&(so->so_bhv), NULL, &uio, &flags, NULL);
	if (error)
		return (error);
	if (uio.uio_resid > 0)	/* `Can't happen' */
		return (EPIPE);

	NTOHL(marker);
	/* XXX: The record marker isn't required, but should be set */
	if ((marker & NFS_TCP_RECMARK) == 0) {
	    cmn_err(CE_WARN, "NFS/TCP: missing record mark 0x%x?\n", marker);
	    return(EPIPE);
	}
	len = (u_int) (marker & ~NFS_TCP_RECMARK);
	*reclen = len;
	return (0);
}

int krpc_rcv_memcopys;
int krpc_rcv_mempullups;

static void
sndlock(nfscon_t * con)
{
	LOCK(&con->nfsc_flaglock);
	while (con->nfsc_flags & NFSC_SNDLOCK) {
		con->nfsc_flags |= NFSC_SNDWANT;
		dbg_statechange(NFSC_SNDWANT);
		WAIT(&con->nfsc_sndsig, 0, &con->nfsc_flaglock, ospl);
		LOCK(&con->nfsc_flaglock);
	}
	con->nfsc_flags |= NFSC_SNDLOCK;
	dbg_statechange(NFSC_SNDLOCK);
	UNLOCK(&con->nfsc_flaglock);
}

/* 
 * Serialize Readers.  This is necessary so that we read
 * an entire packet, and that each reader can check if
 * his reply has already arrived.  The complication here is that
 * we want to wakeup on 2 events:  either the RCVLOCK is free,
 * or our packet has arrived.  If another context happens to read 
 * this packet, it will broadcast so that all receivers wakeup.
 */
static int
rcvlock(nfscon_t * con, nfsreq_t * req)
{
	LOCK(&con->nfsc_flaglock);
	while (con->nfsc_flags & NFSC_RCVLOCK &&
	       req->nfsr_mrep == (struct mbuf *) NULL) {
		con->nfsc_flags |= NFSC_RCVWANT;
		dbg_statechange(NFSC_RCVWANT);
		WAIT(&con->nfsc_rcvsig, 0, &con->nfsc_flaglock, ospl);
		LOCK(&con->nfsc_flaglock);
	}
	if (req->nfsr_mrep != (struct mbuf *) NULL) {
		UNLOCK(&con->nfsc_flaglock);
		return (0);
	}
	con->nfsc_flags |= NFSC_RCVLOCK;
	dbg_statechange(NFSC_RCVLOCK);
	UNLOCK(&con->nfsc_flaglock);
	return (1);
}

static void
sndunlock(nfscon_t * con)
{
	LOCK(&con->nfsc_flaglock);
	con->nfsc_flags &= ~NFSC_SNDLOCK;
	dbg_statechange(DBG_UNLOCK | NFSC_SNDLOCK);
	if (con->nfsc_flags & NFSC_SNDWANT) {
		dbg_statechange(DBG_UNLOCK | NFSC_SNDWANT);
		if (sv_signal(&con->nfsc_sndsig) == 0)	/* no waiters? */
			con->nfsc_flags &= ~NFSC_SNDWANT;
	}
	UNLOCK(&con->nfsc_flaglock);
}

static void
rcvunlock(nfscon_t * con, int locked)
{
	if (!locked) {
		dbg_statechange(DBG_UNLOCK | NFSC_RCVWANT);
		(void) sv_signal(&con->nfsc_rcvsig);
	} else {
		LOCK(&con->nfsc_flaglock);
		con->nfsc_flags &= ~NFSC_RCVLOCK;
		dbg_statechange(DBG_UNLOCK | NFSC_RCVLOCK);
		if (con->nfsc_flags & NFSC_RCVWANT) {
			dbg_statechange(DBG_UNLOCK | NFSC_RCVWANT);
			if (sv_signal(&con->nfsc_rcvsig) == 0)	/* no waiters? */
				con->nfsc_flags &= ~NFSC_RCVWANT;
		}
		UNLOCK(&con->nfsc_flaglock);
	}
}

void 
nfscon_free(struct mntinfo *mi)
{
	nfscon_t *con;
	/* REFERENCED */
	int error;

	/*
	 * If last reference to connection, unlink it from the list
	 * and free resources.
	 * XXX: assumes that there can be no 'in progress' transactions.
	 * This follows from not being able to unmount an active file system.
	 */
	mutex_lock(&nfscon_headlock, PZERO - 1);
	con = (nfscon_t *) mi->mi_con;

#ifdef NFSTCP_DEBUG
	printf("nfscon_free(): freeing svr/port %x/%d @ %x\n", con->nfsc_svraddr.sin_addr.s_addr, con->nfsc_svraddr.sin_port, con);
#endif

	mi->mi_con = NULL;
	if (--con->nfsc_count <= 0) {
		ASSERT(con->nfsc_flags == 0);

#ifdef NFSTCP_DEBUG
		printf("nfscon_free(): destroying svr/port %x/%d @ %x\n", con->nfsc_svraddr.sin_addr.s_addr, con->nfsc_svraddr.sin_port, con);
#endif

		if (con->nfsc_next)
			con->nfsc_next->nfsc_prev = con->nfsc_prev;
		if (con->nfsc_prev)
			con->nfsc_prev->nfsc_next = con->nfsc_next;
		if (nfscon_head == con)
			nfscon_head = con->nfsc_next;
		if (con->nfsc_vsock)
			VSOP_CLOSE(con->nfsc_vsock, 1, 0, error);
		LOCK_DESTROY(&con->nfsc_flaglock);
		LOCK_DESTROY(&con->nfsc_reqlock);
		mutex_destroy(&con->nfsc_socklock);
		sv_destroy(&con->nfsc_rcvsig);
		sv_destroy(&con->nfsc_sndsig);
		kmem_free(con, sizeof(nfscon_t));
	}
	mutex_unlock(&nfscon_headlock);
}

int
nfscon_get(struct mntinfo *mi)
{
	nfscon_t *np;
	int error;

	mutex_lock(&nfscon_headlock, PZERO - 1);

	/*
	 * Look for an existing connection to server/port pair.
	 */
	for (np = nfscon_head; np; np = np->nfsc_next) {
		if (np->nfsc_svraddr.sin_addr.s_addr == mi->mi_addr.sin_addr.s_addr &&
		    np->nfsc_svraddr.sin_port == mi->mi_addr.sin_port) {

#ifdef NFSTCP_DEBUG
			printf("nfscon_get(): found existing svr/port %x/%d @ %x\n", np->nfsc_svraddr.sin_addr.s_addr, np->nfsc_svraddr.sin_port, np);
#endif

			np->nfsc_count++;
			mi->mi_con = np;
			mutex_unlock(&nfscon_headlock);
			return (0);
		}
	}

	/*
	 * Create a new one, putting it at the head of the list, on the
	 * theory that the next mount would go to the same server ala
	 * automounter.
	 */
	np = kmem_zalloc(sizeof(nfscon_t), KM_SLEEP);
	if (nfscon_head) {
		np->nfsc_next = nfscon_head;
		nfscon_head->nfsc_prev = np;
	}
	nfscon_head = np;
	np->nfsc_vsock = NULL;
	np->nfsc_count = 1;
	np->nfsc_svraddr.sin_addr = mi->mi_addr.sin_addr;
	np->nfsc_svraddr.sin_port = mi->mi_addr.sin_port;
	LOCK_INIT(&np->nfsc_reqlock, "nfsc_reqlock");
	LOCK_INIT(&np->nfsc_flaglock, "nfsc_flaglock");
	mutex_init(&np->nfsc_socklock, MUTEX_DEFAULT, "nfsc_socklock");
	sv_init(&np->nfsc_rcvsig, SV_DEFAULT, "nfsc_rcvsig");
	sv_init(&np->nfsc_sndsig, SV_DEFAULT, "nfsc_sndsig");

#ifdef NFSTCP_DEBUG
	printf("nfscon_get(): creating svr/port %x/%d @ %x\n", np->nfsc_svraddr.sin_addr.s_addr, np->nfsc_svraddr.sin_port, np);
#endif

	error = nfs_connect(np, 1);
	mi->mi_con = np;
	mutex_unlock(&nfscon_headlock);
	return (error);
}

static void
ktcp_disconnect(struct socket *so)
{
	SOCKET_LOCK(so);
	sodisconnect(so);
	SOCKET_UNLOCK(so);
}

int
krpc_timedrcv(struct socket *so, struct mbuf **m0, int timeo, int intr)
{
	u_int reclen, count;
	int error, peeked = 0;
	timespec_t ts, rts;
	struct mbuf **tail = m0;

	ts.tv_sec = timeo / HZ;
	ts.tv_nsec = (timeo % HZ) * TICK;
	*m0 = NULL;
	reclen = 0;
	count = sizeof(u_int);

      again:
	if ((error = sblock(&so->so_rcv, NETEVENT_SOUP, so, intr)) != 0)
		goto release_nounlock;
	if (timeo == 0 && sbspace(&so->so_snd) <= 0) {
		error = EOVERFLOW;
		goto release;
	}
	while (so->so_rcv.sb_cc < (peeked ? 1 : count)) {
		ASSERT((signed) so->so_rcv.sb_cc >= 0);
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE) {
			error = EPIPE;
			goto release;
		}
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			goto release;
		}
		if (timeo == 0) {
			error = EWOULDBLOCK;
			goto release;
		}
		NETPAR(NETSCHED, NETSLEEPTKN, (char) &so->so_rcv,
		       NETEVENT_SOUP, NETCNT_NULL, NETRES_SBEMPTY);

		error = sbunlock_timedwait(&so->so_rcv, so, &ts, &rts, intr);
		if (error) {	/* EINTR */
			goto release_nounlock;
		}
		if (rts.tv_sec == 0 && rts.tv_nsec == 0) {
			error = ETIMEDOUT;
			goto release_nounlock;
		}
		NETPAR(NETSCHED, NETWAKEUPTKN, (char) &so->so_rcv,
		       NETEVENT_SOUP, NETCNT_NULL, NETRES_SBEMPTY);
		error = sblock(&so->so_rcv, NETEVENT_SOUP, so, intr);
		if (error) {	/* EINTR */
			goto release_nounlock;
		}
	}
	sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	if (peeked == 0) {
		if (error = krpc_peeklen(so, &reclen)) {
			if (error == EWOULDBLOCK)
				goto again;
			goto release_nounlock;
		}
		peeked = 1;
		count = reclen + sizeof(u_int);
	}
	if (error = ktcp_rcv(so, tail, count))
		goto release_nounlock;

	while (*tail) {
		ASSERT(count >= (*tail)->m_len);
		count -= (*tail)->m_len;
		tail = &(*tail)->m_next;
	}
	if (count)
		goto again;
	
	return (krpc_adjust(m0));

      release:
	if (*m0) {
		m_freem(*m0);
		*m0 = NULL;
		sodisconnect(so);
	}
	ASSERT(SOCKET_ISLOCKED(so));
	sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	NETPAR(error ? NETFLOW : 0, NETDROPTKN, NETPID_NULL,
	       NETEVENT_SOUP, NETCNT_NULL, NETRES_ERROR);
	return (error);

      release_nounlock:
	if (*m0) {
		m_freem(*m0);
		*m0 = NULL;
		ktcp_disconnect(so);
	}
	return (error);
}

int krpc_rcv_drops;
int krpc_rcv_pullups;
int krpc_rcv_copies;

/* 
 * krpc_adjust(): 
 * First mbuf must contain enough data to drop the record mark.
 * A good tcp packet has only a byte alignment guarantee.
 * Since the callers may cheat with XDR, try to u_int align.
 */
int
krpc_adjust(struct mbuf **m0)
{
	struct mbuf *m;

	m = *m0;
	if (m->m_len <= sizeof(u_int) || (mtod(m, u_int) & (sizeof(u_int) - 1))) {
		krpc_rcv_pullups++;
		*m0 = m_pullup(m, sizeof(u_int));
		if ((m = *m0) == NULL)
			return (ENOBUFS);
	}
	m->m_off += sizeof(u_int);
	m->m_len -= sizeof(u_int);

	/* 
	 * Now recheck for alignment.  The pullup might have failed if
	 * the mbuf was non-cluster.  If it still insists on being 
	 * mis-aligned, nuke it.
	 */
	if (mtod(m, u_int) & (sizeof(u_int) - 1)) {
		krpc_rcv_copies++;
		*m0 = m_copy(m, 0, M_COPYALL);
		if (*m0 == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		m = *m0;
		if (mtod(m, u_int) & (sizeof(u_int) - 1)) {
			krpc_rcv_drops++;
			m_freem(m);
			return (ENOBUFS);
		}
	}
	return (0);
}

/* 
 * Take up to count bytes of mbufs off tcp socket w/o copying when possible.
 */
int
ktcp_rcv(struct socket *so, struct mbuf **m0, int count)
{
        int error, len;
	struct mbuf *m, *m2, *nextrecord, **mx;

	*m0 = 0;

	if ((error = sblock(&so->so_rcv, NETEVENT_SOUP, so, 1)) != 0)
		return (error);

	m = so->so_rcv.sb_mb;
	mx = m0;
#define APPEND(m) \
	{ *mx = m; mx = &m->m_next; *mx = 0; }
#define APPEND_LAST(m) *mx = m

	while (count > 0 && m) {
		GOODMT(m->m_type);
		len = MIN(count, m->m_len);
		so->so_state &= ~SS_RCVATMARK;
		if (len == m->m_len) {	/* xfer entire mbuf */
			nextrecord = m->m_act;
			m->m_act = 0;
			count -= len;
			so->so_rcv.sb_cc -= len;	/* sbfree() */
			ASSERT((signed) so->so_rcv.sb_cc >= 0);
			so->so_rcv.sb_mb = m->m_next;	/* MFREE() */
			APPEND(m);
			if (m = so->so_rcv.sb_mb)
				m->m_act = nextrecord;
			else
				m = so->so_rcv.sb_mb = nextrecord;

		} else {	/* len < m_len, copy partial mbuf */
			if ((m2 = m_copy(m, 0, len)) == 0) {
				m_freem(*m0);
				*m0 = 0;
				error = ENOBUFS;
				break;
			}
			APPEND_LAST(m2);
			so->so_rcv.sb_cc -= len;
			ASSERT((signed) so->so_rcv.sb_cc >= 0);
			m->m_off += len;
			m->m_len -= len;
			count -= len;
			ASSERT(count == 0);
		}
	}
	/* Allow tcp to advertize its window.  Significance of so_pcb?? */
	if (so->so_pcb)
		(*so->so_proto->pr_usrreq) (so, PRU_RCVD, 0, 0, 0);

	ASSERT(SOCKET_ISLOCKED(so));
	sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	NETPAR(error ? NETFLOW : 0, NETDROPTKN, NETPID_NULL,
	       NETEVENT_SOUP, NETCNT_NULL, NETRES_ERROR);
	return (error);
}

/* 
 * ktcp_timedsnd()- a simplified sosend() for kernel tcp
 */
int
ktcp_timedsnd(struct socket *so, struct mbuf *m, int mlen, int timeo, int intr)
{
	int error;
	timespec_t ts, rts;
	int space, moved = 0, curlen;
	struct mbuf *next_chain, *prev_chain;

	ts.tv_sec = timeo / HZ;
	ts.tv_nsec = (timeo % HZ) * TICK;

	/* Should never happen */
	/* Ignore for server (ie. timeo == 0) */
	if (timeo && mlen > so->so_snd.sb_hiwat) {
		NETPAR(NETFLOW, NETDROPTKN, NETPID_NULL,
		       NETEVENT_SODOWN, uio->uio_resid, NETRES_SBFULL);
		error = EMSGSIZE;
		goto bail;
	}
	if (error = sblock(&so->so_snd, NETEVENT_SODOWN, so, intr))
		goto bail;
	while (error == 0 && m) {
		ASSERT(SOCKET_ISLOCKED(so));
		if (so->so_state & (SS_CANTSENDMORE|SS_CANTRCVMORE)) {
			error = EPIPE;
			break;
		}
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;	/* ??? */
			break;
		}
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			break;
		}
		/*
		 * If timeo is 0 (server), ignore sbspace. Requests are
		 * throttled on the socket-read side.
		 */
		if (timeo == 0)
			space = mlen;
		else if (m->m_len > (space = sbspace(&so->so_snd))) {
			NETPAR(NETSCHED, NETSLEEPTKN, (char) &so->so_snd,
			       NETEVENT_SODOWN, NETCNT_NULL, NETRES_SBFULL);
			error = sbunlock_timedwait(&so->so_snd, so,
						   &ts, &rts, intr);
			NETPAR(NETSCHED, NETWAKEUPTKN, (char) &so->so_snd,
			       NETEVENT_SODOWN, NETCNT_NULL, NETRES_SBFULL);
			if (error == 0 && rts.tv_sec == 0 && rts.tv_nsec == 0)
				error = ETIMEDOUT;
			if (error) {	/* EINTR */
				goto bail;
			}
			error = sblock(&so->so_snd, NETEVENT_SODOWN, so, intr);
			if (error) {	/* EINTR */
				goto bail;
			}
			continue;
		}

		/*
		 * Chain together mbufs that fit in sbspace().
		 */
		curlen = m->m_len;
		prev_chain = m;
		next_chain = m->m_next;

		while (next_chain && curlen + next_chain->m_len <= space) {
			curlen += next_chain->m_len;
			prev_chain = next_chain;
			next_chain = next_chain->m_next;
		}
		if (next_chain)
			prev_chain->m_next = NULL;

		NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL, NETEVENT_SODOWN,
		       curlen, NETRES_NULL);
		error = (*so->so_proto->pr_usrreq) (so, PRU_SEND, m, NULL, NULL);
		/* an error in the usrreq function will have freed 'm' */
		m = next_chain;
		if (!error)
			moved += curlen;
	}
	ASSERT(error || moved == mlen);
	ASSERT(SOCKET_ISLOCKED(so));
	sbunlock(&so->so_snd, NETEVENT_SODOWN, so);

bail:
	NETPAR(error ? NETFLOW : 0, NETDROPTKN, NETPID_NULL,
	       NETEVENT_SODOWN, mlen, NETRES_ERROR);
	if (error && m) {
		m_freem(m);
	}
	/* We have no way to return partial-send to caller, so force reset */
	if (error && moved) {
		error = ECONNRESET;
		ktcp_disconnect(so);
	}
	return (error);
}

/* 
 * (Re-)connect to server.  Socket may already exists.  
 * If it is connected and without errors, nukeit controls its fate.
 * Basically ripped off from connect() in uipc_syscalls.c
 * No checks for non-blocking io.
 * XXX: review wrt mount intr
 */
static int
nfs_connect(nfscon_t * con, int nukeit)
{
	struct vsocket *vso;
	struct socket *so;
	struct mbuf *m = NULL;
	int error, rv;
	struct sockaddr_in *sin;
	nfsreq_t *req;
	u_int recvspace;

	mutex_lock(&con->nfsc_socklock, PZERO);
	vso = con->nfsc_vsock;
	if (vso) {
		so = (struct socket *) (BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
		SOCKET_LOCK(so);
		if ((nukeit == 0) && (so->so_error == 0) &&
		    ((so->so_state & (SS_ISCONNECTED|SS_CANTRCVMORE|SS_CANTSENDMORE)) == 
			SS_ISCONNECTED)) {
			SOCKET_UNLOCK(so);
			mutex_unlock(&con->nfsc_socklock);
			return (0);
		}
		SOCKET_UNLOCK(so);
		/*
		 * Mark any enqueued requests for resend.
		 */
		LOCK(&con->nfsc_reqlock);
		dbg_statechange(DBG_REQRESND);
		for (req = con->nfsc_reqhead; req; req = req->nfsr_next) {
			req->nfsr_status |= NFSR_RESEND;
			/* sv_signal(&req->nfsr_rcvsig); */
		}
		UNLOCK(&con->nfsc_reqlock);
		sv_broadcast(&con->nfsc_rcvsig);
		soclose(so);
	}
	con->nfsc_vsock = NULL;
	error = vsocreate(AF_INET, &vso, SOCK_STREAM, IPPROTO_TCP);
	if (error) {
		mutex_unlock(&con->nfsc_socklock);
		return (error);
	}
	con->nfsc_vsock = vso;
	so = (struct socket *) (BHV_PDATA(VSOCK_TO_FIRST_BHV(vso)));
	error = bindresvport(vso);
	if (error) {
		mutex_unlock(&con->nfsc_socklock);
		return (error);
	}
	VSOP_GETATTR(vso, SOL_SOCKET, SO_RCVBUF, &m, error);
	SOCKET_LOCK(so);
	if (error) {
		if (m)
			m_free(m);
		goto out;
	}
	recvspace = *mtod(m, u_int *);
	if (soreserve_size > recvspace)
		soreserve(so, soreserve_size, soreserve_size);

	sin = mtod(m, struct sockaddr_in *);
	m->m_len = sizeof(struct sockaddr_in);
	sin->sin_family = AF_INET;
	sin->sin_port = con->nfsc_svraddr.sin_port;
	sin->sin_addr = con->nfsc_svraddr.sin_addr;
	error = soconnect(so, m);
	m_freem(m);
	if (!error) {
		while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
			SOCKET_MPUNLOCK_SOTIMEO(so, (PZERO + 1), rv);
			SOCKET_LOCK(so);
			if (rv) {
				error = EINTR;
				break;
			}
		}
	}
	if (!error) {
		error = so->so_error;
		so->so_error = 0;
	}
	so->so_state &= ~SS_ISCONNECTING;
	/*
	 * m = m_get(M_WAIT, MT_SOOPTS);
	 * *mtod(m, int *) = 1;
	 * m->m_len = sizeof(int);
	 * sosetopt(so, IPPROTO_TCP, TCP_NOSLOWSTART, m);
	 */
out:
	SOCKET_UNLOCK(so);
	mutex_unlock(&con->nfsc_socklock);
	return (error);
}

#ifdef DEBUG

static mutex_t mydbglock;
static lock_t xmydbglock;
static int mydbgindex;
#define MYMAXINDEX 30
static int mydbgstate[MYMAXINDEX];
static int nchanges;

static void
dbg_statechange(int newstate)
{
	LOCK(&mydbglock);
	nchanges++;
	mydbgstate[mydbgindex++] = newstate;
	if (mydbgindex >= MYMAXINDEX)
		mydbgindex = 0;
	UNLOCK(&mydbglock);
}

static void
prstate(int state)
{
	switch (state) {
	case NFSC_RCVLOCK:
		qprintf("RCVLOCK\n");
		break;
	case DBG_UNLOCK | NFSC_RCVLOCK:
		qprintf("RCVUNLOCK\n");
		break;
	case NFSC_SNDLOCK:
		qprintf("SNDLOCK\n");
		break;
	case DBG_UNLOCK | NFSC_SNDLOCK:
		qprintf("SNDUNLOCK\n");
		break;
	case NFSC_RCVWANT:
		qprintf("RCVWANT\n");
		break;
	case DBG_UNLOCK | NFSC_RCVWANT:
		qprintf("RCVWAKEUP\n");
		break;
	case NFSC_SNDWANT:
		qprintf("SNDWANT\n");
		break;
	case DBG_UNLOCK | NFSC_SNDWANT:
		qprintf("SNDWAKEUP\n");
		break;
	case DBG_CONLOCK:
		qprintf("CONLOCK\n");
		break;
	case DBG_CONUNLOCK:
		qprintf("CONUNLOCK\n");
		break;
	case DBG_REQRESND:
		qprintf("REQRESND\n");
		break;
	default:
		qprintf("Unknown State 0x%x\n", state);
		break;
	}
}
/* 
 * The argument 'addr' tells us how many to print out.
 */
void
idbg_ktcpstate(__psint_t addr)
{
	int i,
	 j,
	 k,
	 ospl;

	dbg_conlist();
	k = (int) addr;
	if (k < 0 || k > MYMAXINDEX - 1)
		qprintf("invalid num %d, defaulting to %d\n", k, k = MYMAXINDEX);
	ospl = mutex_spinlock(&xmydbglock);
	qprintf("total number of state changes %d:\n", nchanges);
	for (j = mydbgindex - 1, i = 0; i < k; i++) {
		if (j < 0)
			j = MYMAXINDEX - 1;
		qprintf("state %d: ", i);
		prstate(mydbgstate[j]);
		j--;
	}
	mutex_spinunlock(&xmydbglock, ospl);
}

void
nfstcp_dbginit(void)
{
	LOCK_INIT(&mydbglock, "mydbglock");
	idbg_addfunc("ktcpstate", idbg_ktcpstate);
}

#endif				/* DEBUG */
