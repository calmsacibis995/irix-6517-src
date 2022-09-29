/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.23 88/02/08 
 */


/*
 * clnt_raw.c
 *
 * Memory based rpc for simple testing and timing.
 * Interface to create an rpc client and server in the same process.
 * This lets us similate rpc and get round trip overhead, without
 * any interference from the kernal.
 */

#ifdef __STDC__
	#pragma weak clntraw_create = _clntraw_create
#endif
#include "synonyms.h"

#include <rpc/rpc.h>
#include <rpc/errorhandler.h>
#include <sys/time.h>

#define MCALL_MSG_SIZE 24

/*
 * This is the "network" we will be moving stuff over.
 */
static CLIENT		*client_object;
static XDR		*xdr_stream;
static char		*_raw_buf;
static char		*mashl_callmsg;
static u_int		mcnt;

static enum clnt_stat	clntraw_call(CLIENT *, u_long, xdrproc_t, void *,
		xdrproc_t, void *, struct timeval);
static void		clntraw_abort();
static void		clntraw_geterr();
static bool_t		clntraw_freeres(CLIENT *, xdrproc_t, void *);
static bool_t		clntraw_control();
static void		clntraw_destroy();

static struct clnt_ops client_ops = {
	clntraw_call,
	clntraw_abort,
	clntraw_geterr,
	clntraw_freeres,
	clntraw_destroy,
	clntraw_control
};

/*
 * Create a client handle for memory based rpc.
 */
CLIENT *
clntraw_create(u_long prog, u_long vers)
{
	struct rpc_msg call_msg;
	XDR *xdrs;
	CLIENT *client;

	if (client_object == NULL) {
		client_object = calloc(1, sizeof(CLIENT));
		xdr_stream = calloc(1, sizeof(XDR));
		_raw_buf = calloc(1, UDPMSGSIZE);
		mashl_callmsg = calloc(1, MCALL_MSG_SIZE);
		if (client_object == NULL ||
		    xdr_stream == NULL ||
		    _raw_buf == NULL ||
		    mashl_callmsg == NULL) {
			client_object = NULL;
			return NULL;
		}
	}
	xdrs = xdr_stream;
	client = client_object;

	/*
	 * pre-serialize the staic part of the call msg and stash it away
	 */
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = prog;
	call_msg.rm_call.cb_vers = vers;
	xdrmem_create(xdrs, mashl_callmsg, MCALL_MSG_SIZE, XDR_ENCODE); 
	if (! xdr_callhdr(xdrs, &call_msg)) {
		_rpc_errorhandler(LOG_ERR, "clntraw_create - Fatal header serialization error.");
	}
	mcnt = XDR_GETPOS(xdrs);
	XDR_DESTROY(xdrs);

	/*
	 * Set xdrmem for client/server shared buffer
	 */
	xdrmem_create(xdrs, _raw_buf, UDPMSGSIZE, XDR_FREE);

	/*
	 * create client handle
	 */
	client->cl_ops = &client_ops;
	client->cl_auth = authnone_create();
	return (client);
}

/* ARGSUSED */
static enum clnt_stat 
clntraw_call(CLIENT *h, 
	u_long proc, 
	xdrproc_t xargs, 
	void * argsp,
	xdrproc_t xresults, 
	void * resultsp, 
	struct timeval timeout)
{
	register XDR *xdrs = xdr_stream;
	struct rpc_msg msg;
	enum clnt_stat status;
	struct rpc_err error;

call_again:
	/*
	 * send request
	 */
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	((struct rpc_msg *)mashl_callmsg)->rm_xid ++ ;
	if ((! XDR_PUTBYTES(xdrs, mashl_callmsg, mcnt)) ||
	    (! XDR_PUTLONG(xdrs, (long *)&proc)) ||
	    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
	    (! (*(p_xdrproc_t)xargs)(xdrs, argsp))) {
		return (RPC_CANTENCODEARGS);
	}
	(void)XDR_GETPOS(xdrs);  /* called just to cause overhead */

	/*
	 * We have to call server input routine here because this is
	 * all going on in one process. Yuk.
	 */
	svc_getreq(0);

	/*
	 * get results
	 */
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	msg.acpted_rply.ar_verf = _null_auth;
	msg.acpted_rply.ar_results.where = resultsp;
	msg.acpted_rply.ar_results.proc = xresults;
	if (! xdr_replymsg(xdrs, &msg))
		return (RPC_CANTDECODERES);
	_seterr_reply(&msg, &error);
	status = error.re_status;

	if (status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, msg.acpted_rply.ar_verf)) {
			status = RPC_AUTHERROR;
		}
	}  /* end successful completion */
	else {
		if (AUTH_REFRESH(h->cl_auth))
			goto call_again;
	}  /* end of unsuccessful completion */

	if (status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, msg.acpted_rply.ar_verf)) {
			status = RPC_AUTHERROR;
		}
		if (msg.acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void)xdr_opaque_auth(xdrs, &(msg.acpted_rply.ar_verf));
		}
	}

	return (status);
	/* NOTREACHED */
	/* this is just here to hush a compiler warning bug... */
	if (timeout.tv_sec == 0) return status;
}

static void
clntraw_geterr()
{
}

/* ARGSUSED */
static bool_t
clntraw_freeres(CLIENT *cl, xdrproc_t xdr_res, void *res_ptr)
{
	register XDR *xdrs = xdr_stream;

	xdrs->x_op = XDR_FREE;
	return ((*(p_xdrproc_t)xdr_res)(xdrs, res_ptr));
}

static void
clntraw_abort()
{
}

static bool_t
clntraw_control()
{
	return (FALSE);
}

static void
clntraw_destroy()
{
}
