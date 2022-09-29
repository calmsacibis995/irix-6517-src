/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.38 88/02/08
 */

 
/*
 * clnt_tcp.c, Implements a TCP/IP based, client side RPC.
 *
 * TCP based RPC supports 'batched calls'.
 * A sequence of calls may be batched-up in a send buffer.  The rpc call
 * return immediately to the client even though the call was not necessarily
 * sent.  The batching occurs if the results' xdr routine is NULL (0) AND
 * the rpc timeout value is zero (see clnt.h, rpc).
 *
 * Clients should NOT casually batch calls that in fact return results; that is,
 * the server side should be aware that a call is batched and not produce any
 * return message.  Batched calls that produce many result messages can
 * deadlock (netlock) the client and the server....
 *
 * Now go hang yourself.
 */

#ifdef __STDC__
	#pragma weak clnttcp_create = _clnttcp_create
	#pragma weak clnttcp_getreply = _clnttcp_getreply
#endif
#include "synonyms.h"

#include <stdio.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <rpc/pmap_clnt.h>
#include <rpc/errorhandler.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>		/* prototypes for multiple functions */
#include <bstring.h>		/* prototype for bzero() */
#include "priv_extern.h"

#define MCALL_MSG_SIZE 24

static enum clnt_stat clnttcp_call(CLIENT *, u_long, xdrproc_t, void *, 
			xdrproc_t, void *, struct timeval);
static void clnttcp_abort(CLIENT *);
static void clnttcp_geterr(CLIENT *h, struct rpc_err *errp);
static bool_t clnttcp_freeres(CLIENT *cl, xdrproc_t xdr_res, void * res_ptr);
static bool_t clnttcp_control(CLIENT *cl, int request, void *info);
static void clnttcp_destroy(CLIENT *h);

static struct clnt_ops tcp_ops = {
	clnttcp_call,
	clnttcp_abort,
	clnttcp_geterr,
	clnttcp_freeres,
	clnttcp_destroy,
	clnttcp_control
};

struct ct_data {
	int		ct_sock;
	bool_t		ct_closeit;
	struct timeval	ct_wait;
	bool_t          ct_waitset;       /* wait set by clnt_control? */
	bool_t		ct_rtnonerror;	/* if TRUE return RPC_INTR on EINTR */
	struct sockaddr_in ct_addr; 
	struct rpc_err	ct_error;
	char		ct_mcall[MCALL_MSG_SIZE];	/* marshalled callmsg */
	u_int		ct_mpos;			/* pos after marshal */
	XDR		ct_xdrs;
};
static int readtcp(void *, void *, u_int);
static int writetcp(void *, void *, u_int);

/*
 * Create a client handle for a tcp/ip connection.
 * If *sockp<0, *sockp is set to a newly created TCP socket and it is
 * connected to raddr.  If *sockp non-negative then
 * raddr is ignored.  The rpc/tcp package does buffering
 * similar to stdio, so the client must pick send and receive buffer sizes,];
 * 0 => use the default.
 * If raddr->sin_port is 0, then a binder on the remote machine is
 * consulted for the right port number.
 * NB: *sockp is copied into a private area.
 * NB: It is the clients responsibility to close *sockp.
 * NB: The rpch->cl_auth is set null authentication.  Caller may wish to set this
 * something more useful.
 */
CLIENT *
clnttcp_create(struct sockaddr_in *raddr, 
	u_long prog,
	u_long vers, 
	register int *sockp, 
	u_int sendsz, 
	u_int recvsz)
{
	CLIENT *h;
	register struct ct_data *ct;
	struct timeval now;
	struct rpc_msg call_msg;

	h  = (CLIENT *)mem_alloc(sizeof(*h));
	if (h == NULL) {
		_rpc_errorhandler(LOG_ERR, "clnttcp_create: out of memory");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}
	ct = (struct ct_data *)mem_alloc(sizeof(*ct));
	if (ct == NULL) {
		_rpc_errorhandler(LOG_ERR, "clnttcp_create: out of memory");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}

	/*
	 * If no port number given ask the pmap for one
	 */
	if (raddr->sin_port == 0) {
		u_short port;
		if ((port = pmap_getport(raddr, prog, vers, IPPROTO_TCP)) == 0) {
			mem_free((caddr_t)ct, sizeof(struct ct_data));
			mem_free((caddr_t)h, sizeof(CLIENT));
			return ((CLIENT *)NULL);
		}
		raddr->sin_port = htons(port);
	}

	/*
	 * If no socket given, open one
	 */
	if (*sockp < 0) {
		*sockp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		(void)bindresvport(*sockp, (struct sockaddr_in *)0);
		if ((*sockp < 0)
		    || (connect(*sockp, (struct sockaddr *)raddr,
		    sizeof(*raddr)) < 0)) {
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
			(void)close(*sockp);
			goto fooy;
		}
		ct->ct_closeit = TRUE;
	} else {
		ct->ct_closeit = FALSE;
	}

	/*
	 * Set up private data struct
	 */
	ct->ct_sock = *sockp;
	ct->ct_wait.tv_usec = 0;
	ct->ct_waitset = FALSE;
	ct->ct_addr = *raddr;
	ct->ct_rtnonerror = FALSE;

	/*
	 * Initialize call message
	 */
	(void)gettimeofday(&now);
	call_msg.rm_xid = (unsigned long)(getpid() ^ now.tv_sec ^ now.tv_usec);
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = prog;
	call_msg.rm_call.cb_vers = vers;

	/*
	 * pre-serialize the staic part of the call msg and stash it away
	 */
	xdrmem_create(&(ct->ct_xdrs), ct->ct_mcall, MCALL_MSG_SIZE,
	    XDR_ENCODE);
	if (! xdr_callhdr(&(ct->ct_xdrs), &call_msg)) {
		if (ct->ct_closeit) {
			(void)close(*sockp);
		}
		goto fooy;
	}
	ct->ct_mpos = XDR_GETPOS(&(ct->ct_xdrs));
	XDR_DESTROY(&(ct->ct_xdrs));

	/*
	 * Create a client handle which uses xdrrec for serialization
	 * and authnone for authentication.
	 */
	xdrrec_create(&(ct->ct_xdrs), sendsz, recvsz,
	    (caddr_t)ct, readtcp, writetcp);
	h->cl_ops = &tcp_ops;
	h->cl_private = (caddr_t) ct;
	h->cl_auth = authnone_create();
	return (h);

fooy:
	/*
	 * Something goofed, free stuff and barf
	 */
	mem_free((caddr_t)ct, sizeof(struct ct_data));
	mem_free((caddr_t)h, sizeof(CLIENT));
	(void)close(*sockp);
	return ((CLIENT *)NULL);
}

static enum clnt_stat
clnttcp_call(register CLIENT *h, 
	u_long proc, 
	xdrproc_t xdr_args, 
	void * args_ptr, 
	xdrproc_t xdr_results, 
	void * results_ptr,
	struct timeval timeout)
{
	register struct ct_data *ct = (struct ct_data *) h->cl_private;
	register XDR *xdrs = &(ct->ct_xdrs);
	struct rpc_msg reply_msg;
	xdr_u_long_t x_id;
	xdr_u_long_t *msg_x_id = (xdr_u_long_t *)(ct->ct_mcall); /* yuk */
	register bool_t shipnow;
	int refreshes = 2;

	if (!ct->ct_waitset) {
		ct->ct_wait = timeout;
	}

	shipnow =
	    (xdr_results == (xdrproc_t)0 && timeout.tv_sec == 0
	    && timeout.tv_usec == 0) ? FALSE : TRUE;

call_again:
	xdrs->x_op = XDR_ENCODE;
	ct->ct_error.re_status = RPC_SUCCESS;
	x_id = ntohl(--(*msg_x_id));
	if ((! XDR_PUTBYTES(xdrs, ct->ct_mcall, ct->ct_mpos)) ||
	    (! XDR_PUTLONG(xdrs, (long *)&proc)) ||
	    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
	    (! (*(p_xdrproc_t)xdr_args)(xdrs, args_ptr))) {
		if (ct->ct_error.re_status == RPC_SUCCESS)
			ct->ct_error.re_status = RPC_CANTENCODEARGS;
		(void)xdrrec_endofrecord(xdrs, TRUE);
		return (ct->ct_error.re_status);
	}
	if (! xdrrec_endofrecord(xdrs, shipnow))
		return (ct->ct_error.re_status = RPC_CANTSEND);
	if (! shipnow)
		return (RPC_SUCCESS);
	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		return(ct->ct_error.re_status = RPC_TIMEDOUT);
	}


	/*
	 * Keep receiving until we get a valid transaction id
	 */
	xdrs->x_op = XDR_DECODE;
	for (;;) {
		reply_msg.acpted_rply.ar_verf = _null_auth;
		reply_msg.acpted_rply.ar_results.where = NULL;
		reply_msg.acpted_rply.ar_results.proc = (xdrproc_t) xdr_void;
		if (! xdrrec_skiprecord(xdrs))
			return (ct->ct_error.re_status);
		/* now decode and validate the response header */
		if (! xdr_replymsg(xdrs, &reply_msg)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				continue;
			return (ct->ct_error.re_status);
		}
		if (reply_msg.rm_xid == x_id)
			break;
	}

	/*
	 * process header
	 */
	_seterr_reply(&reply_msg, &(ct->ct_error));
	if (ct->ct_error.re_status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, reply_msg.acpted_rply.ar_verf)) {
			ct->ct_error.re_status = RPC_AUTHERROR;
			ct->ct_error.re_why = AUTH_INVALIDRESP;
		} else if (! (*(p_xdrproc_t)xdr_results)(xdrs, results_ptr)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				ct->ct_error.re_status = RPC_CANTDECODERES;
		}
		/* free verifier ... */
		if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void)xdr_opaque_auth(xdrs, &(reply_msg.acpted_rply.ar_verf));
		}
	}  /* end successful completion */
	else {
		/* maybe our credentials need to be refreshed ... */
		if (refreshes-- && AUTH_REFRESH(h->cl_auth))
			goto call_again;
	}  /* end of unsuccessful completion */
	return (ct->ct_error.re_status);
}

static void
clnttcp_geterr(CLIENT *h, struct rpc_err *errp)
{
	register struct ct_data *ct =
	    (struct ct_data *) h->cl_private;

	*errp = ct->ct_error;
}

static bool_t
clnttcp_freeres(CLIENT *cl, xdrproc_t xdr_res, void * res_ptr)
{
	register struct ct_data *ct = (struct ct_data *)cl->cl_private;
	register XDR *xdrs = &(ct->ct_xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*(p_xdrproc_t)xdr_res)(xdrs, res_ptr));
}

/* ARGSUSED */
static void
clnttcp_abort(CLIENT *cl)
{
}

static bool_t
clnttcp_control(CLIENT *cl, int request, void *info)
{
	register struct ct_data *ct = (struct ct_data *)cl->cl_private;

	switch (request) {
	case CLSET_TIMEOUT:
		ct->ct_wait = *(struct timeval *)info;
		ct->ct_waitset = TRUE;
		break;
	case CLGET_TIMEOUT:
		*(struct timeval *)info = ct->ct_wait;
		break;
	case CLGET_SERVER_ADDR:
		*(struct sockaddr_in *)info = ct->ct_addr;
		break;
	case CLGET_FD:
		*(int *)info = ct->ct_sock;
		break;
	case CLSET_FD_CLOSE:
		ct->ct_closeit = TRUE;
		break;
	case CLSET_FD_NCLOSE:
		ct->ct_closeit = FALSE;
		break;
	case CLSET_EINTR_RETURN:
		ct->ct_rtnonerror = *(bool_t *)info;
		break;
	case CLGET_EINTR_RETURN:
		*(bool_t *)info = ct->ct_rtnonerror;
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}


static void
clnttcp_destroy(CLIENT *h)
{
	register struct ct_data *ct =
	    (struct ct_data *) h->cl_private;

	if (ct->ct_closeit) {
		(void)close(ct->ct_sock);
	}
	XDR_DESTROY(&(ct->ct_xdrs));
	mem_free((caddr_t)ct, sizeof(struct ct_data));
	mem_free((caddr_t)h, sizeof(CLIENT));
}

/*
 * Interface between xdr serializer and tcp connection.
 * Behaves like the system calls, read & write, but keeps some error state
 * around for the rpc level.
 */
static int
readtcp(void *v, void *buf, register u_int len)
{
	fd_set mask;
	fd_set readfds;
	struct ct_data *ct = (struct ct_data *)v;

	if (len == 0)
		return (0);
	FD_ZERO(&mask);
	FD_SET(ct->ct_sock, &mask);
	for (;;) {
		readfds = mask;
		switch (select(FD_SETSIZE, &readfds, (fd_set*)NULL, (fd_set*)NULL,
			       &(ct->ct_wait))) {
		case 0:
			ct->ct_error.re_status = RPC_TIMEDOUT;
			return (-1);

		case -1:
			if (errno == EINTR) {
				if (ct->ct_rtnonerror != 0) {
					ct->ct_error.re_status = RPC_INTR;
					ct->ct_error.re_errno = errno;
					return (-1);
				}
				continue;
			}
			ct->ct_error.re_status = RPC_CANTRECV;
			ct->ct_error.re_errno = errno;
			return (-1);
		}
		break;
	}
	switch (len = (int)read(ct->ct_sock, buf, (size_t)len)) {

	case 0:
		/* premature eof */
		ct->ct_error.re_errno = ECONNRESET;
		ct->ct_error.re_status = RPC_CANTRECV;
		len = -1;  /* it's really an error */
		break;

	case -1:
		ct->ct_error.re_errno = errno;
		ct->ct_error.re_status = RPC_CANTRECV;
		break;
	}
	return (len);
}

static int
writetcp(void *v, void *buf, u_int len)
{
	register int i, cnt;
	struct ct_data *ct = (struct ct_data *)v;

	for (cnt = (int)len; cnt > 0; cnt -= i, buf = (char *)buf + i) {
		if ((i = (int)write(ct->ct_sock, buf, (size_t)cnt)) == -1) {
			ct->ct_error.re_errno = errno;
			ct->ct_error.re_status = RPC_CANTSEND;
			return (-1);
		}
	}
	return (len);
}

enum clnt_stat
clnttcp_getreply(register CLIENT *h,
	xdrproc_t xdr_results,
	caddr_t results_ptr,
	struct timeval timeout)
{
	register struct ct_data *ct = (struct ct_data *) h->cl_private;
	register XDR *xdrs = &(ct->ct_xdrs);
	struct rpc_msg reply_msg;

	/*
	 * Receive a message
	 */
	if (!ct->ct_waitset) {
		ct->ct_wait = timeout;
	}
	ct->ct_error.re_status = RPC_SUCCESS;
	xdrs->x_op = XDR_DECODE;
	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = NULL;
	reply_msg.acpted_rply.ar_results.proc = (xdrproc_t) xdr_void;
	if (! xdrrec_skiprecord(xdrs))
		return (ct->ct_error.re_status);
	/* now decode and validate the response header */
	if (! xdr_replymsg(xdrs, &reply_msg))
		return (ct->ct_error.re_status);

	/*
	 * process header
	 */
	_seterr_reply(&reply_msg, &(ct->ct_error));
	if (ct->ct_error.re_status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, reply_msg.acpted_rply.ar_verf)) {
			ct->ct_error.re_status = RPC_AUTHERROR;
			ct->ct_error.re_why = AUTH_INVALIDRESP;
		} else if (! (*(p_xdrproc_t)xdr_results)(xdrs, results_ptr)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				ct->ct_error.re_status = RPC_CANTDECODERES;
		}
		/* free verifier ... */
		if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void)xdr_opaque_auth(xdrs, &(reply_msg.acpted_rply.ar_verf));
		}
	}  /* end successful completion */

	return (ct->ct_error.re_status);
}
