/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.59 88/02/16
 */
#ident "$Revision: 3.111 $"

/*
 * clnt_kudp.c
 * Implements a kernel UPD/IP based, client side RPC.
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
#include <sys/vsocket.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/atomic_ops.h>
#include <sys/sesmgr.h>
#include <sys/mac_label.h>
#include <sys/cmn_err.h>
#include "netinet/in.h"
#include "net/route.h"
#include "netinet/in_pcb.h"
#include "xdr.h"
#include "auth.h"
#include "clnt.h"
#include "rpc_msg.h"
#include "nfs_stat.h"
#ifdef DEBUG
#include "os/proc/pproc_private.h"
#endif

#define hz	HZ

#define alloc_xid()	atomicAddInt((int *)&clntkudpxid, 1)

extern udp_output(struct inpcb *, struct mbuf *, 
		  struct inaddrpair *, struct ipsec *);

static enum clnt_stat	clntkudp_callit();
extern char  *inet_ntoa(struct in_addr);

void		clntkudp_error();
void		clntkudp_destroy();
bool_t clntkudp_freeres(CLIENT *cl, xdrproc_t  xdr_res, void *res_ptr);

/*
 * Operations vector for UDP/IP based RPC
 */
static struct clnt_ops udp_ops = {
	clntkudp_callit,	/* do rpc call */
	0,			/* abort call */
	clntkudp_error,		/* return error status */
	clntkudp_freeres,	/* free results */
	clntkudp_destroy,	/* destroy rpc handle */
	0			/* control */
};

/*
 * Private data per rpc handle.  This structure is allocated by
 * clntkudp_create, and freed by cku_destroy.
 */
struct cku_private {
	CLIENT			 cku_client;	/* common RPC stuff */
	xdr_long_t		 cku_xid;	/* xid to use in next call */
	int			 cku_flags;	/* see below */
	int			 cku_retrys;	/* request retrys */
	struct vsocket		*cku_vsock;	/* open udp socket */
	struct inaddrpair	 cku_iap;	/* addresses */
	struct rpc_err		 cku_err;	/* error status */
	struct cred		*cku_cred;	/* credentials */
	xdr_long_t		 cku_pgm;	/* RPC program */
	xdr_long_t		 cku_vers;	/* version of above */
#ifdef DEBUG
	pid_t			 cku_owner;
	struct cku_private	*cku_next;
	struct cku_private	*cku_prev;
#endif
};

#ifdef DEBUG
struct cku_private *ckulist;
mutex_t ckulist_lock;
#endif

#define HEADER_OFFSET 64 /* max size of transport and data link header */

#define	htop(h)		((struct cku_private *)((h)->cl_private))
#define getreal(v) ((struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(v))))

/* cku_flags */
#define CKU_INTR	0x1	/* interruptable sleeps */
#define	CKU_XIDPERCALL	0x2	/* new xids on each call */

__uint32_t	clntkudpxid;	/* transaction id used by all clients */

u_long
clnt_getaddr(CLIENT *cl)
{
	struct cku_private *p = htop(cl);

	return(p->cku_iap.iap_faddr.s_addr);
}

/*
 * We need the following properties for clntkudpxid:
 *
 *	1.  Large difference between reboots.  xids can be used
 *		up quickly.
 *	2.  New xid > last xid within the time a server's
 *		duplicate cache will contain the entry.
 *	3.  Can't have xid rollover during normal operation
 *		even when client stays up for quite a while.
 *
 *	"time" is a 32-bit quantity, given in seconds.
 *
 * This gives us:
 *
 *	1.  11 bits of xid space per second or 2048 xids per
 *		second.  This should be sufficient, even on
 *		large MP machines which might be a compute
 *		server attached to lots of NFS fileservers.
 *		A diskless Indy used about 60 xids per second
 *		during a multi-user bootup.
 *
 *	2.  19 bits of seconds or almost 1 year.
 *	3.  2 upper bits set to zero, so we can better
 *		prevent rollover.
 * This is called from main.c early in system booting.
 */
void
clntkudpxid_init()
{
	clntkudpxid = (0x3fffffff & ((time & 0x7ffff) << 11));
#ifdef DEBUG
	mutex_init(&ckulist_lock, MUTEX_DEFAULT, "ckulist lock");
#endif
}

#ifdef DEBUG
/*
 * check for client handles using the same port
 */
clntkudp_unique_port(struct cku_private *p)
{
	struct cku_private *tmp;

	ASSERT(!ckulist || ckulist->cku_prev == NULL);
	for (tmp = ckulist; tmp; tmp = tmp->cku_next) {
		if (p->cku_iap.iap_lport == tmp->cku_iap.iap_lport) {
			cmn_err(CE_PANIC, "port sharing, port %d, ckus 0x%p 0x%p, owners %d %d, inps 0x%p 0x%p, vsocks 0x%p 0x%p\n",
				p->cku_iap.iap_lport, p, tmp, p->cku_owner,
				tmp->cku_owner, sotoinpcb(getreal(p->cku_vsock)),
				sotoinpcb(getreal(tmp->cku_vsock)), p->cku_vsock,
				p->cku_vsock);
		}
		ASSERT(!tmp->cku_next || (pnum(tmp->cku_next) != 0));
	}
	return(1);
}
#endif

/*
 * Create an rpc handle for a udp rpc connection.
 * Allocates space for the handle structure and the private data, and
 * opens a socket.  Note sockets and handles are one to one.
 */
CLIENT *
clntkudp_create(addr, pgm, vers, retrys, intr, xidflag, cred)
	struct sockaddr_in *addr;
	u_long pgm;
	u_long vers;
	int retrys;
	enum kudp_intr intr;
	enum kudp_xid xidflag;
	struct cred *cred;
{
	/* REFERENCED */
	int tmp;
	register CLIENT *h;
	register struct cku_private *p;
	int error = 0;

	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	bzero((caddr_t)p, sizeof(*p));
	h = &p->cku_client;

	/* handle */
	h->cl_ops = &udp_ops;
	h->cl_private = (caddr_t) p;
	h->cl_auth = authkern_create();

	p->cku_pgm = pgm;
	p->cku_vers = vers;

	/* open udp socket */
	error = vsocreate(AF_INET, &p->cku_vsock, SOCK_DGRAM, IPPROTO_UDP);
	if (error) {
		goto bad;
	}
	error = bindresvport(p->cku_vsock);
	if (error) {
		goto bad;
	}
	/* set up the private fields */
	error = clntkudp_init(h, addr, retrys, intr, xidflag, cred);
	if (error == 0) {
#ifdef DEBUG
		mutex_enter(&ckulist_lock);
		ASSERT(p->cku_vsock->vs_type == SOCK_DGRAM);
		ASSERT(p->cku_vsock->vs_protocol == IPPROTO_UDP);
		ASSERT(p->cku_vsock->vs_domain == AF_INET);
		p->cku_owner = curprocp ? curprocp->p_pid : -1;
		ASSERT(clntkudp_unique_port(p));
		p->cku_next = ckulist;
		if (ckulist) {
			ckulist->cku_prev = p;
		}
		ckulist = p;
		ASSERT(!ckulist || ckulist->cku_prev == NULL);
		mutex_exit(&ckulist_lock);
#endif
	    return (h);
	}

bad:
	if (p->cku_vsock)
		VSOP_CLOSE(p->cku_vsock, 1, 0, tmp);
	kmem_free(p, sizeof (struct cku_private));
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = error;
	return (NULL);
}

/*
 * resets the parameters without re-allocating
 */
int
clntkudp_init(h, addr, retrys, intr, xidflag, cred)
	CLIENT *h;
	struct sockaddr_in *addr;
	int retrys; 
	enum kudp_intr intr;
	enum kudp_xid xidflag;
	struct cred *cred;
{
	struct cku_private *p = htop(h);
	struct inpcb *inp = sotoinpcb(getreal(p->cku_vsock));
	int error;

	p->cku_retrys = retrys;
	p->cku_cred = cred;
	if (intr == KUDP_INTR) {
		p->cku_flags |= CKU_INTR;
	} else {
		p->cku_flags &= ~CKU_INTR;
	}
	if (xidflag == KUDP_XID_PERCALL) {
		p->cku_flags |= CKU_XIDPERCALL;
	} else if (xidflag == KUDP_XID_CREATE) {
		p->cku_flags &= ~CKU_XIDPERCALL;
		p->cku_xid = alloc_xid();
	}

	SOCKET_LOCK(inp->inp_socket);

	if (addr->sin_addr.s_addr == p->cku_iap.iap_faddr.s_addr &&
	    addr->sin_port == p->cku_iap.iap_fport) {

		if(inp->inp_route.ro_rt) {
			register struct sockaddr_in *so;
			register struct route *ro;

			ro = &inp->inp_route;

			if( ro && ro->ro_rt && ro->ro_rt->rt_ifa && 
			    ro->ro_rt->rt_ifa->ifa_addr) {
				so = (struct sockaddr_in *)ro->ro_rt->rt_ifa->ifa_addr;

				if( p->cku_iap.iap_laddr.s_addr == so->sin_addr.s_addr) {
					/* same remote address as last time */
					SOCKET_UNLOCK(inp->inp_socket);
					return (0);
				}
			}
		}
	}
	/*
	 * disconnect first
	 */
	inp->inp_faddr.s_addr = INADDR_ANY;
	inp->inp_laddr.s_addr = INADDR_ANY;
	/*
	 * set the address and mark the socket as connected
	 */
	error = in_pcbsetaddr(inp, addr, &p->cku_iap);
	if (!error)
		inp->inp_socket->so_state |= SS_ISCONNECTED;
	SOCKET_UNLOCK(inp->inp_socket);
	return (error);
}

/*
 * Time out back off function. tim is in hz
 */
#define MAXTIMO	(60 * hz)
#define backoff(tim)	((((tim) << 1) > MAXTIMO) ? MAXTIMO : ((tim) << 1))

/*
 * Call remote procedure.
 * Most of the work of rpc is done here.  We encode
 * the arguments, and send it off.  We wait for a reply or a time out.
 * Timeout causes an immediate return, other packet problems may cause
 * a retry on the receive.  When a good packet is received we decode
 * it, and check verification.  A bad reply code will cause one retry
 * with full (longhand) credentials.
 * Each client handle MUST be single threaded!
 */
static enum clnt_stat
clntkudp_callit(h, procnum, xdr_args, argsp, xdr_results, resultsp, wait)
	register CLIENT	*h;
	u_long		procnum;
	xdrproc_t	xdr_args;
	caddr_t		argsp;
	xdrproc_t	xdr_results;
	caddr_t		resultsp;
	struct timeval	wait;
{
	register struct cku_private *p = htop(h);
	XDR xdr;
	struct socket *so = getreal(p->cku_vsock);
	struct inpcb *inp = sotoinpcb(so);
	int rtries;
	int stries = p->cku_retrys;
	struct cred *tmpcred;
	struct mbuf *m, *n;
	int64_t timohz;
	xdr_long_t xid;
	int refreshes = 2;	/* number of times to refresh credential */
	xdr_long_t *xdrl;
	timespec_t ts;
	timespec_t rts;
	int doprint = 1;
	int reset_timeout = 1;

	RCSTAT.rccalls++;

	/*
	 * Set credentials as appropriate
	 */
	tmpcred = get_current_cred();
	set_current_cred(p->cku_cred);

	if (p->cku_flags & CKU_XIDPERCALL)
		xid = alloc_xid();
	else
		xid = p->cku_xid;

	/*
	 * This is dumb but easy: keep the time out in units of hz
	 * so it is easy to call timeout and modify the value.
	 */
	timohz = wait.tv_sec * hz + (wait.tv_usec * hz) / 1000000;

call_again:

	m = m_vget(M_DONTWAIT, MBUFCL_SIZE, MT_DATA);
	if (!m) {
		p->cku_err.re_status = RPC_CANTSEND;
		p->cku_err.re_errno = ENOBUFS;
		goto done;
	}
	/*
	 * Put a little head-room in the buffer for the lower level headers.
	 * Then do an in-line construction of the RPC reply header.
	 * Finally, XDR encode the request.
	 */
	m->m_len -= HEADER_OFFSET;
	m->m_off += HEADER_OFFSET;
	xdrmbuf_init(&xdr, m, XDR_ENCODE);
	xdrl = XDR_INLINE(&xdr, 24);
	/*
	 * Here, the only way XDR_INLINE will fail is if the pointer it
	 * would return is not properly aligned.
	 */
	ASSERT(xdrl);
	IXDR_PUT_LONG(xdrl, xid);
	IXDR_PUT_LONG(xdrl, CALL);
	IXDR_PUT_LONG(xdrl, RPC_MSG_VERSION);
	IXDR_PUT_LONG(xdrl, p->cku_pgm);
	IXDR_PUT_LONG(xdrl, p->cku_vers);
	IXDR_PUT_LONG(xdrl, procnum);
	/*
	 * Serialize request into the output buffer.
	 */
	if ((! AUTH_MARSHALL(h->cl_auth, &xdr)) ||
	    (! xdr_args(&xdr, argsp))) {
		p->cku_err.re_status = RPC_CANTENCODEARGS;
		p->cku_err.re_errno = EIO;
		(void) m_freem(m);
		goto done;
	}

	/*
	 * The xdrmbuf ops do not update m->m_len as args are encoded.
	 * Instead, the last mbuf in the chain must be explicitly set to the
	 * current position.  All other mbufs keep the initial (full) m_len.
	 */
	n = m;
	while (n->m_next)
		n = n->m_next;
	n->m_len = XDR_GETPOS(&xdr);

	SOCKET_LOCK(so);
	/*
	 * Clear the socket error indicator to get rid of any stray
	 * errors that may have occurred since the last RPC.  We are
	 * only interested in errors occurring on this RPC call.
	 */
	so->so_error = 0;
	if (p->cku_err.re_errno = udp_output(inp, m, &p->cku_iap,
					     so->so_sesmgr_data)) {
		p->cku_err.re_status = RPC_CANTSEND;
		SOCKET_UNLOCK(so);
		goto done;
	}

	/*
	 * If timohz is zero, then the caller is doing an asynchronous RPC,
	 * so the receive should be skipped.
	 */
	for (rtries = (timohz ? p->cku_retrys : 0); rtries; rtries--) {
		if (!(m = so->so_rcv.sb_mb)) {
			/*
			 * Set timeout, then wait for input, timeout, or
			 * interrupt.  We hold all signals but the four that
			 * Sun naively considers "keyboard interrupts", plus
			 * SIGKILL and SIGTSTP, which are useful together
			 * when backgrounding and killing a hung process.
			 */
			so->so_rcv.sb_flags |= SB_WAIT;
			if (reset_timeout) {
				/*
				 * Normalize by dividing by hz.
				 * Avoid overflow because timohz is a 64bit int.
				 */
				ts.tv_sec = timohz / hz;
				ts.tv_nsec = (timohz % hz) * 1000000000 / hz;
			} else {
				ts.tv_sec = rts.tv_sec;
				ts.tv_nsec = rts.tv_nsec;
				reset_timeout = 1;
			}
			if ((p->cku_flags & CKU_INTR) && 
			    !KT_CUR_ISSTHREAD()) {
				int interrupted;
				k_sigset_t old;
#if	(_MIPS_SIM != _ABIO32)
				k_sigset_t new = {
					    ~(sigmask(SIGHUP)|sigmask(SIGINT)|
					    sigmask(SIGQUIT)|sigmask(SIGKILL)|
					    sigmask(SIGTERM)) };
#else	/* (_MIPS_SIM != _ABIO32) */
				k_sigset_t new = {
					    ~(sigmask(SIGHUP)|sigmask(SIGINT)|
					    sigmask(SIGQUIT)|sigmask(SIGKILL)|
					    sigmask(SIGTERM)), 0xffffffff };
#endif	/* (_MIPS_SIM != _ABIO32) */
				assign_cursighold(&old, &new);
				interrupted = sv_timedwait_sig(&so->so_rcv.sb_waitsem,
					PZERO + 1, &so->so_sem, 0, 0, &ts, &rts);
				assign_cursighold(NULL, &old);
				if (interrupted) {
					p->cku_err.re_status = RPC_INTR;
					p->cku_err.re_errno = EINTR;
					goto done;
				}
			} else {
				sv_timedwait(&so->so_rcv.sb_waitsem, PRIBIO,
					&so->so_sem, 0, 0, &ts, &rts);
			}

			SOCKET_LOCK(so);
			/*
			 * Look for data on the socket.  If there is none, check
			 * for an error, a timeout, or a random wakeup.
			 */
			m = so->so_rcv.sb_mb;
			if (m == NULL) {
				if (so->so_error) {
					p->cku_err.re_status = RPC_CANTRECV;
					p->cku_err.re_errno = so->so_error;
					SOCKET_UNLOCK(so);
					goto done;
				} else if (!rts.tv_sec && !rts.tv_nsec) {
					SOCKET_UNLOCK(so);
					p->cku_err.re_status = RPC_TIMEDOUT;
					p->cku_err.re_errno = ETIMEDOUT;
					RCSTAT.rctimeouts++;
					goto done;
				}
				continue;
			}
		}

		so->so_rcv.sb_mb = m->m_act;
		while (m && m->m_type != MT_DATA && m->m_type != MT_HEADER) {
			sbfree(&so->so_rcv, m);
			m = m_free(m);
		}
		if (m == NULL) {
			continue;
		}
		for (n = m; n; n = n->m_next) {
			sbfree(&so->so_rcv, n);
		}
		if (m->m_len < sizeof (xdr_u_long_t)) {
			m_freem(m);
			continue;
		}
		/*
		 * If reply transaction id matches id sent
		 * we have a good packet, not a delayed duplicate.
		 */
		if (*mtod(m, u_int *) != xid) {
			RCSTAT.rcbadxids++;
			m_freem(m);
			rtries++;	/* keep trying for a good xid */
			reset_timeout = 0; /* but don't try forever */
			continue;
		}
		break;
	}

	SOCKET_UNLOCK(so);
	if (rtries == 0) {
		if (timohz) {
			p->cku_err.re_status = RPC_CANTRECV;
			p->cku_err.re_errno = EIO;
		} else {
			p->cku_err.re_status = RPC_TIMEDOUT;
			p->cku_err.re_errno = 0;
			stries = 0;
		}
		goto done;
	}

	/*
	 * Decode and process reply
	 */
	xdrmbuf_init(&xdr, m, XDR_DECODE);
	{
		struct rpc_msg		   reply_msg;

		reply_msg.acpted_rply.ar_verf = _null_auth;
		reply_msg.acpted_rply.ar_results.where = resultsp;
		reply_msg.acpted_rply.ar_results.proc = xdr_results;

		/*
		 * Decode and validate the response.
		 */
		if (xdr_replymsg(&xdr, &reply_msg)) {
			_seterr_reply(&reply_msg, &(p->cku_err));

			if (p->cku_err.re_status == RPC_SUCCESS) {
				/*
				 * Reply is good, check auth.
				 */
				if (! AUTH_VALIDATE(h->cl_auth,
				    reply_msg.acpted_rply.ar_verf)) {
					p->cku_err.re_status = RPC_AUTHERROR;
					p->cku_err.re_why = AUTH_INVALIDRESP;
					p->cku_err.re_errno = EIO;
					RCSTAT.rcbadverfs++;
				}
				if (reply_msg.acpted_rply.ar_verf.oa_base !=
				    NULL) {
					/* free auth handle */
					xdr.x_op = XDR_FREE;
					(void) xdr_opaque_auth(&xdr,
					    &(reply_msg.acpted_rply.ar_verf));
				}
			} else {
				/*
				 * Maybe our credentials need to be refreshed
				 */
				if (refreshes > 0 && AUTH_REFRESH(h->cl_auth)) {
					refreshes--;
					RCSTAT.rcnewcreds++;
				}
			}
		} else {
			p->cku_err.re_status = RPC_CANTDECODERES;
			p->cku_err.re_errno = EIO;
		}
	}
	m_freem(m);

done:
	if ((p->cku_err.re_status != RPC_SUCCESS) &&
	    (p->cku_err.re_status != RPC_INTR) &&
	    (p->cku_err.re_status != RPC_CANTENCODEARGS) &&
	    (--stries > 0)) {
		RCSTAT.rcretrans++;
		timohz = backoff(timohz);
		if (p->cku_err.re_status == RPC_SYSTEMERROR ||
		    p->cku_err.re_status == RPC_CANTSEND ||
		    p->cku_err.re_status == RPC_CANTRECV) {
			switch (p->cku_err.re_errno) {
			    case ENETUNREACH:
			    case EHOSTUNREACH:
				if (doprint) {
					cmn_err(CE_WARN,
					    "RPC: No route to host %s\n",
					    inet_ntoa(p->cku_iap.iap_faddr));
				}
				delay(timohz);
				break;
			    case ENETDOWN:
				if (doprint) {
					cmn_err(CE_WARN,
					    "RPC: Network down for host %s\n",
					    inet_ntoa(p->cku_iap.iap_faddr));
				}
				delay(timohz);
				break;
			    case EHOSTDOWN:
				if (doprint) {
					cmn_err(CE_WARN,
					    "RPC: host %s down\n",
					    inet_ntoa(p->cku_iap.iap_faddr));
				}
				delay(timohz);
				break;
			    default:
				if (doprint)
					cmn_err(CE_WARN, "RPC error %d\n",
					    p->cku_err.re_errno);
			    case ECONNREFUSED:
				/*
				 * connection refused, host is either
				 * shutting down or comming up, so
				 * wait a bit and try again
				 */
			    case ENOMEM:
			    case ENOBUFS:
			    case EINTR:
			    case EIO:
				/*
				 * Lack of resources, wait a bit and try
				 * again
				 */
				delay(hz);
				break;
			}

			doprint = 0;
               } else if (p->cku_err.re_status == RPC_TIMEDOUT) {
                       struct sockaddr_in addr;
                       /*
                        * Try getting a new route to avoid timeout.
                        * Get the address from p->cku_iap,
                        * disconnect (clear inp->inp_[fl]addr.s_addr),
                        * then try to reconnect.
                        * Errors are reflected in the next send attempt.
                        */
                       bzero(&addr, sizeof(addr));
#ifdef _HAVE_SIN_LEN
                      addr.sin_len = 8;
#endif
                       addr.sin_family = AF_INET;
                       addr.sin_port = p->cku_iap.iap_fport;
                       addr.sin_addr.s_addr = p->cku_iap.iap_faddr.s_addr;
 
                       SOCKET_LOCK(inp->inp_socket);
                       inp->inp_faddr.s_addr = INADDR_ANY;
                       inp->inp_laddr.s_addr = INADDR_ANY;
                       if (in_pcbsetaddr(inp, &addr, &p->cku_iap))
                               inp->inp_socket->so_state &= ~SS_ISCONNECTED;
                       SOCKET_UNLOCK(inp->inp_socket);
		}
		goto call_again;
	}
	set_current_cred(tmpcred);
	if (p->cku_err.re_status != RPC_SUCCESS) {
		RCSTAT.rcbadcalls++;
	}
	return (p->cku_err.re_status);
}

/*
 * Wake up client waiting for a reply.
 *
 * 	A thread can determine why it was awakened [received data,
 *	a timeout, or interrupt] by first checking for interrupts, then
 *	looking for bytes available on the rcv socket, else if there on none,
 *	it timed out.  If the thread is already running there no problem
 *	with an additional wakeup.  If the thread isn't running and data
 *	arrives before the thread restarts from the timeout, the data
 *	is noticed as if there was no timeout, which is ok.
 */
void
ckuwakeup(p)
	register struct cku_private *p;
{
	struct socket	*so = getreal(p->cku_vsock);

	SOCKET_SBWAIT_WAKEALL(&so->so_rcv);
}

/*
 * Return error info on this handle.
 */
void
clntkudp_error(h, err)
	CLIENT *h;
	struct rpc_err *err;
{
	register struct cku_private *p = htop(h);

	*err = p->cku_err;
}

/*
 * free results. Not really necessary to go through the
 * client handle for this one?
 */
/* ARGSUSED */
bool_t
clntkudp_freeres(CLIENT *cl, xdrproc_t xdr_res, void *res_ptr)
{
	XDR xdr;

	xdr.x_op = XDR_FREE;
	return (xdr_res(&xdr, res_ptr));
}

/*
 * Destroy rpc handle.
 * Frees the space used for private data, and handle
 * structure, and closes the socket for this handle.
 */
void
clntkudp_destroy(h)
	CLIENT *h;
{
	struct cku_private *p;
	/* REFERENCED */
	int	error = 0;

	p = htop(h);
	AUTH_DESTROY(h->cl_auth);
#ifdef DEBUG
	mutex_enter(&ckulist_lock);
	if (!p->cku_prev) {
		ASSERT(p == ckulist);
		ckulist = p->cku_next;
		if (ckulist) {
			ckulist->cku_prev = NULL;
		}
	} else if (!p->cku_next) {
		ASSERT(p->cku_prev && (pnum(p->cku_prev) != 0));
		p->cku_prev->cku_next = NULL;
	} else {
		p->cku_next->cku_prev = p->cku_prev;
		p->cku_prev->cku_next = p->cku_next;
	}
	ASSERT(!ckulist || ckulist->cku_prev == NULL);
	mutex_exit(&ckulist_lock);
#endif
	VSOP_CLOSE(p->cku_vsock, 1, 0, error);
	kmem_free(p, sizeof(*p));
}

/*
 * try to bind to a reserved port
 * Aug '96: try harder to not reuse ports.
 */
int
bindresvport(vso)
	struct vsocket *vso;
{
	struct sockaddr_in *sin;
	struct mbuf *m;
	u_short i;
	int error;
	struct cred *tmpcred;
	struct cred *savecred;
#	define MAX_PRIV	(IPPORT_RESERVED-1)
#	define MIN_PRIV	(2*(IPPORT_RESERVED/3))
#	define NUM_PRIVS (MAX_PRIV - MIN_PRIV + 1)
	static int lastport = 0;

	m = m_get(M_WAIT, MT_SONAME);

	sin = mtod(m, struct sockaddr_in *);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_ANY;
	m->m_len = sizeof (struct sockaddr_in);

	/*
	 * Only root can bind to a privileged port number, so
	 * temporarily change the uid to 0 to do the bind.
	 */
	savecred = get_current_cred();
	tmpcred = crdup(savecred);
	tmpcred->cr_cap.cap_effective |= CAP_PRIV_PORT;
	set_current_cred(tmpcred);
	error = EADDRINUSE;
	for (i = NUM_PRIVS; error == EADDRINUSE && i > 0; i--) {
		int curport;

		curport = atomicIncWithWrap(&lastport, NUM_PRIVS);
		sin->sin_port = htons(curport + MIN_PRIV);
		VSOP_BIND(vso, m, error);
	}
	(void) m_freem(m);
	set_current_cred(savecred);
	crfree(tmpcred);
	return (error);
}

/*
 * Set socket attributes for NFS operations.
 * Do this here to maintain the private nature of cku_private.
 */
void
clntkudp_soattr(h, mp)
	CLIENT *h;
	mac_label *mp;
{
	struct cku_private *p;
	extern mac_label *mac_equal_equal_lp;
	extern mac_label *mac_admin_high_lp;
	register struct socket	   *so;

	p = htop(h);
	so = getreal(p->cku_vsock);
	if (so->so_sesmgr_data) {
		sesmgr_set_label(so, mac_equal_equal_lp);
		sesmgr_set_sendlabel(so, mp ? mp : mac_admin_high_lp);
	}
}
