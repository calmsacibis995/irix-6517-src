/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.40 88/02/08
 */


/*
 * clnt_udp.c, Implements a UDP/IP based, client side RPC.
 */

#ifdef __STDC__
	#pragma weak clntudp_bufcreate = _clntudp_bufcreate
	#pragma weak clntudp_create = _clntudp_create
#endif
#include "synonyms.h"

#include <stdio.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#include <rpc/pmap_clnt.h>
#include <rpc/errorhandler.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>             /* prototypes for multiple functions */
#include <bstring.h>            /* prototype for bzero() */
#include "priv_extern.h"
#include <t6net.h>
#include <pthread.h>
#include <signal.h>
#include <mplib.h>
#include <assert.h>
#include <sys/resource.h>

extern int __svc_label_agile;

#define RPC_MAX_BACKOFF 30 /*seconds*/
#define USEC_PER_SEC	1000000   /* number of usecs for 1 second */

/*
 *  This machinery implements per-fd locks for MT-safety.  It is not
 *  sufficient to do per-CLIENT handle locks for MT-safety because a
 *  user may create more than one CLIENT handle with the same fd behind
 *  it.  Therfore, we allocate an array of flags (dg_fd_locks), protected
 *  by the clnt_fd_lock mutex, and an array (dg_cv) of condition variables
 *  similarly protected.  Dg_fd_lock[fd] == 1 => a call is activte on some
 *  CLIENT handle created for that fd.
 *  The current implementation holds locks across the entire RPC and reply,
 *  including retransmissions.  Yes, this is silly, and as soon as this
 *  code is proven to work, this should be the first thing fixed.  One step
 *  at a time.
 */

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
extern int lock_value;
extern pthread_mutex_t clnt_fd_lock;
static int  *dg_fd_locks _INITBSS;
static pthread_cond_t   *dg_cv _INITBSS;

#define release_fd_lock(fd, mask) {	                                              \
	if (MTLIB_ACTIVE()) {                                                         \
	       MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &clnt_fd_lock) );             \
	       if (!allocate_fdarray()) {                                             \
		      dg_fd_locks[fd] = 0;		                              \
               }                                                                      \
               MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &clnt_fd_lock) );           \
	       MTLIB_INSERT( (MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &(mask), NULL) );   \
               if (dg_cv != NULL)                                                     \
                      MTLIB_INSERT( (MTCTL_PTHREAD_COND_SIGNAL, &dg_cv[fd]) );	      \
	}									      \
}
#else
#define release_fd_lock(fd, mask)
#endif

/*
 * synchronouly generated signals like SIGBUS, SIGSEGV can not be masked
 */
#define DELETE_UNMASKABLE_SIGNAL_FROM_SET(s)    { \
                (void) _sigdelset((s), SIGSEGV); \
                (void) _sigdelset((s), SIGBUS); \
                (void) _sigdelset((s), SIGFPE); \
                (void) _sigdelset((s), SIGILL); \
                }
/*
 * UDP bases client side rpc operations
 */
static enum clnt_stat clntudp_call(CLIENT *, u_long, xdrproc_t,
			void *, xdrproc_t, void *, struct timeval);
static void clntudp_abort(CLIENT *);
static void clntudp_geterr(CLIENT *cl, struct rpc_err *errp);
static bool_t clntudp_freeres(CLIENT *cl, xdrproc_t xdr_res, void * res_ptr);
static bool_t clntudp_control(CLIENT *cl, int request, void *info);
static void clntudp_destroy(CLIENT *cl);

static struct clnt_ops udp_ops = {
	clntudp_call,
	clntudp_abort,
	clntudp_geterr,
	clntudp_freeres,
	clntudp_destroy,
	clntudp_control
};

/* 
 * Private data kept per client handle
 */
struct cu_data {
	int		   cu_sock;
	bool_t		   cu_closeit;
	struct sockaddr_in cu_raddr;
	int		   cu_rlen;
	struct timeval	   cu_wait;
	struct timeval     cu_total;
	struct rpc_err	   cu_error;
	XDR		   cu_outxdrs;
	u_int		   cu_xdrpos;
	u_int		   cu_sendsz;
	char		   *cu_outbuf;
	u_int		   cu_recvsz;
	char		   cu_inbuf[1];
};

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
static int 
allocate_fdarray(void) {
	if (dg_fd_locks == (int *) NULL) {
		int cv_allocsz, fd_allocsz;
		struct rlimit rl;
		int dtbsize = FD_SETSIZE; /* XXX - need a better number */
		
		if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_max > 0)
			dtbsize = (int) rl.rlim_max;
		
		fd_allocsz = dtbsize * (int)sizeof (int);
		dg_fd_locks = (int *) mem_alloc(fd_allocsz);
		if (dg_fd_locks == (int *) NULL) {
			goto err1;
		} else
			memset(dg_fd_locks, '\0', fd_allocsz);
		
		assert(dg_cv == (cond_t *) NULL);
		cv_allocsz = dtbsize * (int)sizeof (pthread_cond_t);
		dg_cv = (pthread_cond_t *) mem_alloc(cv_allocsz);
		if (dg_cv == (pthread_cond_t *) NULL) {
			mem_free(dg_fd_locks, fd_allocsz);
			dg_fd_locks = (int *) NULL;
			goto err1;
		} else {
			int i;
			
			for (i = 0; i < dtbsize; i++)
				MTLIB_INSERT( (MTCTL_PTHREAD_COND_INIT, &dg_cv[i], NULL) );
		}
	} else
		assert(dg_cv != (pthread_cond_t *) NULL);
	return 0;
err1:
	return -1;
}
#endif

/*
 * Create a UDP based client handle.
 * If *sockp<0, *sockp is set to a newly created UPD socket.
 * If raddr->sin_port is 0 a binder on the remote machine
 * is consulted for the correct port number.
 * NB: It is the clients responsibility to close *sockp.
 * NB: The rpch->cl_auth is initialized to null authentication.
 *     Caller may wish to set this something more useful.
 *
 * wait is the amount of time used between retransmitting a call if
 * no response has been heard;  retransmition occurs until the actual
 * rpc call times out.
 *
 * sendsz and recvsz are the maximum allowable packet sizes that can be
 * sent and received.
 */
CLIENT *
clntudp_bufcreate(struct sockaddr_in *raddr,
	u_long program,
	u_long version,
	struct timeval wait,
	register int *sockp,
	u_int sendsz,
	u_int recvsz)
{
	CLIENT *cl = 0;
	register struct cu_data *cu = 0;
	struct timeval now;
	struct rpc_msg call_msg;
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
	sigset_t newmask;
	sigset_t mask;

        if (MTLIB_ACTIVE()) {
		sigfillset(&newmask);
		DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
		MTLIB_INSERT( (MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &newmask, &mask) );
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &clnt_fd_lock) );
		if (allocate_fdarray())	{
			MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &clnt_fd_lock) );
			MTLIB_INSERT( (MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &(mask), NULL) );
			goto err1;
		}
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &clnt_fd_lock) );
		MTLIB_INSERT( (MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &(mask), NULL) );
	}
#endif
	cl = (CLIENT *)mem_alloc(sizeof(CLIENT));
	if (cl == NULL) {
		goto err1;
	}
	sendsz = ((sendsz + 3) / 4) * 4;
	recvsz = ((recvsz + 3) / 4) * 4;
	cu = (struct cu_data *)mem_alloc(sizeof(*cu) + sendsz + recvsz);
	if (cu == NULL) {
		goto err1;
	}
	memset(cu,'\0',sizeof(*cu));
	cu->cu_outbuf = &cu->cu_inbuf[recvsz];

	(void)gettimeofday(&now);
	if (raddr->sin_port == 0) {
		u_short port;
		if ((port =
		    pmap_getport(raddr, program, version, IPPROTO_UDP)) == 0) {
			goto err2;
		}
		raddr->sin_port = htons(port);
	}
	cl->cl_ops = &udp_ops;
	cl->cl_private = (caddr_t)cu;
	cu->cu_raddr = *raddr;
	cu->cu_rlen = sizeof (cu->cu_raddr);
	cu->cu_wait = wait;
	cu->cu_total.tv_sec = -1;
	cu->cu_total.tv_usec = -1;
	cu->cu_sendsz = sendsz;
	cu->cu_recvsz = recvsz;
	call_msg.rm_xid = (unsigned long)(getpid() ^ now.tv_sec ^ now.tv_usec);
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = program;
	call_msg.rm_call.cb_vers = version;
	xdrmem_create(&(cu->cu_outxdrs), cu->cu_outbuf,
	    sendsz, XDR_ENCODE);
	if (! xdr_callhdr(&(cu->cu_outxdrs), &call_msg)) {
		goto err2;
	}
	cu->cu_xdrpos = XDR_GETPOS(&(cu->cu_outxdrs));
	if (*sockp < 0) {
		int dontblock = 1;

		*sockp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (*sockp < 0) {
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
			goto err2;
		}
		/* attempt to bind to prov port */
		(void)bindresvport(*sockp, (struct sockaddr_in *)0);
		/* the sockets rpc controls are non-blocking */
		(void)ioctl(*sockp, FIONBIO, (char *) &dontblock);
		cu->cu_closeit = TRUE;
	} else {
		cu->cu_closeit = FALSE;
	}
	if (__svc_label_agile && tsix_on(*sockp) == -1) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto err2;
	}
	cu->cu_sock = *sockp;
	cl->cl_auth = authnone_create();
	return (cl);
err1:
	_rpc_errorhandler(LOG_ERR, "clntudp_create: out of memory");
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;
err2:
	if (cu)
		mem_free((caddr_t)cu, sizeof(*cu) + sendsz + recvsz);
	if (cl)
		mem_free((caddr_t)cl, sizeof(CLIENT));
	return ((CLIENT *)NULL);
}

CLIENT *
clntudp_create(struct sockaddr_in *raddr,
	u_long program,
	u_long version,
	struct timeval wait,
	register int *sockp)
{

	return(clntudp_bufcreate(raddr, program, version, wait, sockp,
	    UDPMSGSIZE, UDPMSGSIZE));
}

static enum clnt_stat 
clntudp_call(register CLIENT	*cl,	/* client handle */
	u_long		proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void *		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void *		resultsp,	/* pointer to results */
	struct timeval	utimeout)	/* seconds to wait before giving up */
{
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;
	register XDR *xdrs;
	register int outlen;
	register int inlen;
	int fromlen;
	fd_set readfds;
	fd_set mask;
	struct sockaddr_in from;
	struct rpc_msg reply_msg;
	XDR reply_xdrs;
	struct timeval time_waited;
	struct timeval retransmit_time;
	bool_t ok;
	int nrefreshes = 2;	/* number of times to refresh cred */
	struct timeval timeout;
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
	sigset_t newmask;
	sigset_t oldmask;
	if (MTLIB_ACTIVE()) {
		sigfillset(&newmask);
		DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
		MTLIB_INSERT( (MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &newmask, &oldmask) );
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &clnt_fd_lock) );
		if (!allocate_fdarray()) {
			while (dg_fd_locks[cu->cu_sock]) {
				MTLIB_INSERT((MTCTL_PTHREAD_COND_WAIT, &dg_cv[cu->cu_sock], 
					      &clnt_fd_lock));
				
			}
			dg_fd_locks[cu->cu_sock] = lock_value;
		}
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &clnt_fd_lock) );
	}
#endif
	if (cu->cu_total.tv_usec == -1) {
		timeout = utimeout;     /* use supplied timeout */
	} else {
		timeout = cu->cu_total; /* use default timeout */
	}

	time_waited.tv_sec = 0;
	time_waited.tv_usec = 0;
	retransmit_time=cu->cu_wait;
 
call_again:
	xdrs = &(cu->cu_outxdrs);
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, cu->cu_xdrpos);
	/*
	 * the transaction is the first thing in the out buffer
	 */
	(*(u_short *)(cu->cu_outbuf))++;
	if ((! XDR_PUTLONG(xdrs, (long *)&proc)) ||
	    (! AUTH_MARSHALL(cl->cl_auth, xdrs)) ||
	    (! (*(p_xdrproc_t)xargs)(xdrs, argsp))) {
		release_fd_lock(cu->cu_sock, oldmask);
		return (cu->cu_error.re_status = RPC_CANTENCODEARGS);
	}
	outlen = (int)XDR_GETPOS(xdrs);

	if (__svc_label_agile) {
		struct in_addr ia;

		ia.s_addr = cu->cu_raddr.sin_addr.s_addr;
		if (tsix_set_mac_byrhost(cu->cu_sock, &ia, NULL) == -1) {
			cu->cu_error.re_errno = errno;
			release_fd_lock(cu->cu_sock, oldmask);
			return (cu->cu_error.re_status = RPC_CANTSEND);
		}
	}
send_again:
	if (sendto(cu->cu_sock, cu->cu_outbuf, outlen, 0,
	    (struct sockaddr *)&(cu->cu_raddr), cu->cu_rlen)
	    != outlen) {
		if (errno == ENOBUFS &&
		    time_waited.tv_sec <= timeout.tv_sec) {
			/* Special case to avoid temporary congestion */
			time_waited.tv_sec += 1;
			sleep(1);
			goto send_again;
		}
		cu->cu_error.re_errno = errno;
		release_fd_lock(cu->cu_sock, oldmask);
		return (cu->cu_error.re_status = RPC_CANTSEND);
	}

	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		release_fd_lock(cu->cu_sock, oldmask);
		return (cu->cu_error.re_status = RPC_TIMEDOUT);
	}
	/*
	 * sub-optimal code appears here because we have
	 * some clock time to spare while the packets are in flight.
	 * (We assume that this is actually only executed once.)
	 */
	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = resultsp;
	reply_msg.acpted_rply.ar_results.proc = xresults;
	FD_ZERO(&mask);
	FD_SET(cu->cu_sock, &mask);
	for (;;) {
		readfds = mask;
		switch (select(FD_SETSIZE, &readfds, NULL, 
			       NULL, &(retransmit_time))) {

		case 0:
			time_waited.tv_sec += retransmit_time.tv_sec;
			time_waited.tv_usec += retransmit_time.tv_usec;
			while (time_waited.tv_usec >= USEC_PER_SEC) {
				time_waited.tv_sec++;
				time_waited.tv_usec -= USEC_PER_SEC;
			}
 
 			/*update retransmit_time*/
 
 			if (retransmit_time.tv_sec < RPC_MAX_BACKOFF) {
			    retransmit_time.tv_usec += retransmit_time.tv_usec;
			    retransmit_time.tv_sec += retransmit_time.tv_sec;
			    while (retransmit_time.tv_usec >= USEC_PER_SEC) {
				    retransmit_time.tv_sec++;
				    retransmit_time.tv_usec -= USEC_PER_SEC;
			    }
 			}

			if ((time_waited.tv_sec < timeout.tv_sec) ||
				((time_waited.tv_sec == timeout.tv_sec) &&
				(time_waited.tv_usec < timeout.tv_usec)))
				goto send_again;	
			release_fd_lock(cu->cu_sock, oldmask);
			return (cu->cu_error.re_status = RPC_TIMEDOUT);

		/*
		 * buggy in other cases because time_waited is not being
		 * updated.
		 */
		case -1:
			if (errno == EINTR)
				continue;	
			cu->cu_error.re_errno = errno;
			release_fd_lock(cu->cu_sock, oldmask);
			return (cu->cu_error.re_status = RPC_CANTRECV);
		}
		do {
			fromlen = sizeof(struct sockaddr);
			inlen = recvfrom(cu->cu_sock, cu->cu_inbuf, 
				(int) cu->cu_recvsz, 0,
				(struct sockaddr *)&from, &fromlen);
		} while (inlen < 0 && errno == EINTR);
		if (inlen < 0) {
			if (errno == EWOULDBLOCK)
				continue;	
			cu->cu_error.re_errno = errno;
			release_fd_lock(cu->cu_sock, oldmask);
			return (cu->cu_error.re_status = RPC_CANTRECV);
		}
		if (inlen < (int)sizeof(xdr_u_long_t))
			continue;	
		/* see if reply transaction id matches sent id */
		if (*((xdr_u_long_t *)(cu->cu_inbuf)) != 
					*((xdr_u_long_t *)(cu->cu_outbuf)))
			continue;	
		/* we now assume we have the proper reply */
		break;
	}

	/*
	 * now decode and validate the response
	 */
	xdrmem_create(&reply_xdrs, cu->cu_inbuf, (u_int)inlen, XDR_DECODE);
	ok = xdr_replymsg(&reply_xdrs, &reply_msg);
	/* XDR_DESTROY(&reply_xdrs);  save a few cycles on noop destroy */
	if (ok) {
		_seterr_reply(&reply_msg, &(cu->cu_error));
		if (cu->cu_error.re_status == RPC_SUCCESS) {
			if (! AUTH_VALIDATE(cl->cl_auth,
				reply_msg.acpted_rply.ar_verf)) {
				cu->cu_error.re_status = RPC_AUTHERROR;
				cu->cu_error.re_why = AUTH_INVALIDRESP;
			}
			if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
				xdrs->x_op = XDR_FREE;
				(void)xdr_opaque_auth(xdrs,
				    &(reply_msg.acpted_rply.ar_verf));
			} 
		}  /* end successful completion */
		else {
			/* maybe our credentials need to be refreshed ... */
			if (nrefreshes > 0 && AUTH_REFRESH(cl->cl_auth)) {
				nrefreshes--;
				goto call_again;
			}
		}  /* end of unsuccessful completion */
	}  /* end of valid reply message */
	else {
		cu->cu_error.re_status = RPC_CANTDECODERES;
	}
	release_fd_lock(cu->cu_sock, oldmask);
	return (cu->cu_error.re_status);
}

static void
clntudp_geterr(CLIENT *cl, struct rpc_err *errp)
{
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;

	*errp = cu->cu_error;
}


static bool_t
clntudp_freeres(CLIENT *cl, xdrproc_t xdr_res, void * res_ptr)
{
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;
	register XDR *xdrs = &(cu->cu_outxdrs);
	bool_t dummy;
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
	sigset_t newmask;
	sigset_t mask;

	if (MTLIB_ACTIVE()) {
		sigfillset(&newmask);
		DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
		MTLIB_INSERT( (MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &newmask, &mask) );
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &clnt_fd_lock) );
		if (!allocate_fdarray()) {
			while (dg_fd_locks[cu->cu_sock]) {
				MTLIB_INSERT((MTCTL_PTHREAD_COND_WAIT, &dg_cv[cu->cu_sock], &clnt_fd_lock));
			}
		}
	}
#endif
	xdrs->x_op = XDR_FREE;
	dummy = (*(p_xdrproc_t)xdr_res)(xdrs, res_ptr);
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
	if (MTLIB_ACTIVE()) {
		MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, &clnt_fd_lock));
		MTLIB_INSERT((MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &mask, NULL));
		if (dg_cv != NULL)
			MTLIB_INSERT((MTCTL_PTHREAD_COND_SIGNAL, &dg_cv[cu->cu_sock]));
	}
#endif
	return (dummy);
}

/* ARGSUSED */
static void 
clntudp_abort(CLIENT *h)
{
}

static bool_t
clntudp_control(CLIENT *cl, int request, void *info)
{
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
	sigset_t newmask;
	sigset_t mask;

	if (MTLIB_ACTIVE()) {
		sigfillset(&newmask);
		DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
		MTLIB_INSERT( (MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &newmask, &mask) );
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &clnt_fd_lock) );
		if (!allocate_fdarray()) {
			while (dg_fd_locks[cu->cu_sock]) {
				MTLIB_INSERT((MTCTL_PTHREAD_COND_WAIT, &dg_cv[cu->cu_sock], 
					      &clnt_fd_lock));
			}
			dg_fd_locks[cu->cu_sock] = lock_value;
		}
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &clnt_fd_lock) );
	}
#endif
	switch (request) {
	case CLSET_TIMEOUT:
		cu->cu_total = *(struct timeval *)info;
		break;
	case CLGET_TIMEOUT:
		*(struct timeval *)info = cu->cu_total;
		break;
	case CLSET_RETRY_TIMEOUT:
		cu->cu_wait = *(struct timeval *)info;
		break;
	case CLGET_RETRY_TIMEOUT:
		*(struct timeval *)info = cu->cu_wait;
		break;
	case CLGET_SERVER_ADDR:
		*(struct sockaddr_in *)info = cu->cu_raddr;
		break;
	case CLGET_FD:
		*(int *)info = cu->cu_sock;
		break;
	case CLSET_FD_CLOSE:
		cu->cu_closeit = TRUE;
		break;
	case CLSET_FD_NCLOSE:
		cu->cu_closeit = FALSE;
		break;
	default:
		release_fd_lock(cu->cu_sock, mask);
		return (FALSE);
	}	
	release_fd_lock(cu->cu_sock, mask);
	return (TRUE);
}
	
static void
clntudp_destroy(CLIENT *cl)
{
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;
	int cu_sock = cu->cu_sock;
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
	sigset_t newmask;
	sigset_t mask;

	if (MTLIB_ACTIVE()) {
		sigfillset(&newmask);
		DELETE_UNMASKABLE_SIGNAL_FROM_SET(&newmask);
		MTLIB_INSERT( (MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &newmask, &mask) );
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &clnt_fd_lock) );
		if (!allocate_fdarray()) {
			while (dg_fd_locks[cu_sock]) {
				MTLIB_INSERT((MTCTL_PTHREAD_COND_WAIT, &dg_cv[cu_sock], 
					      &clnt_fd_lock));
			}
		}
	}
#endif
	if (cu->cu_closeit) {
		(void)close(cu_sock);
	}
	XDR_DESTROY(&(cu->cu_outxdrs));
	mem_free((caddr_t)cu, (sizeof(*cu) + cu->cu_sendsz + cu->cu_recvsz));
	mem_free((caddr_t)cl, sizeof(CLIENT));

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
	if (MTLIB_ACTIVE()) {
		MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, &clnt_fd_lock));
		MTLIB_INSERT((MTCTL_PTHREAD_SIGMASK, SIG_SETMASK, &mask, NULL));
		if (dg_cv != NULL)
			MTLIB_INSERT((MTCTL_PTHREAD_COND_SIGNAL, &dg_cv[cu_sock]));
	}
#endif
}
