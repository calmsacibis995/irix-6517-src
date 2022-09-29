/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.16 88/02/08 
 */


/*
 * svc_raw.c,   This is a toy for simple testing and timing.
 * Interface to create an rpc client and server in the same UNIX process.
 * This lets us similate rpc and get rpc (round trip) overhead, without
 * any interference from the kernal.
 */

#ifdef __STDC__
	#pragma weak svcraw_create = _svcraw_create
#endif
#include "synonyms.h"

#include <rpc/rpc.h>


/*
 * This is the "network" that we will be moving data over
 */
static struct svcraw_private {
	char	_raw_buf[UDPMSGSIZE];
	SVCXPRT	server;
	XDR	xdr_stream;
	char	verf_body[MAX_AUTH_BYTES];
} *svcraw_private _INITBSS;

static bool_t svcraw_recv(SVCXPRT *xprt, struct rpc_msg *msg);
static enum xprt_stat svcraw_stat(SVCXPRT *);
static bool_t svcraw_getargs(SVCXPRT *, xdrproc_t, void *);
static bool_t svcraw_reply(SVCXPRT *xprt, struct rpc_msg *msg);
static bool_t svcraw_freeargs(SVCXPRT *, xdrproc_t, void *);
static void svcraw_destroy(SVCXPRT *);

static struct xp_ops server_ops = {
	svcraw_recv,
	svcraw_stat,
	svcraw_getargs,
	svcraw_reply,
	svcraw_freeargs,
	svcraw_destroy
};

SVCXPRT *
svcraw_create(void)
{
	register struct svcraw_private *srp = svcraw_private;

	if (srp == 0) {
		srp = (struct svcraw_private *)calloc(1, sizeof (*srp));
		if (srp == 0)
			return (0);
	}
	srp->server.xp_sock = 0;
	srp->server.xp_port = 0;
	srp->server.xp_p3 = NULL;
	srp->server.xp_ops = &server_ops;
	srp->server.xp_verf.oa_base = srp->verf_body;
	xdrmem_create(&srp->xdr_stream, srp->_raw_buf, UDPMSGSIZE, XDR_FREE);
	return (&srp->server);
}

/* ARGSUSED */
static enum xprt_stat
svcraw_stat(SVCXPRT *xprt)
{

	return (XPRT_IDLE);
	/* NOTREACHED */
	/* this is just here to hush a compiler warning bug... */
	xprt->xp_p3 = NULL;
}

/* ARGSUSED */
static bool_t
svcraw_recv(SVCXPRT *xprt, struct rpc_msg *msg)
{
	register struct svcraw_private *srp = svcraw_private;
	register XDR *xdrs;

	if (srp == 0)
		return (0);
	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	if (! xdr_callmsg(xdrs, msg))
	       return (FALSE);
	return (TRUE);
}

/* ARGSUSED */
static bool_t
svcraw_reply(SVCXPRT *xprt, struct rpc_msg *msg)
{
	register struct svcraw_private *srp = svcraw_private;
	register XDR *xdrs;

	if (srp == 0)
		return (FALSE);
	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	if (! xdr_replymsg(xdrs, msg))
	       return (FALSE);
	(void)XDR_GETPOS(xdrs);  /* called just for overhead */
	return (TRUE);
}

/* ARGSUSED */
static bool_t
svcraw_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, void * args_ptr)
{
	register struct svcraw_private *srp = svcraw_private;

	if (srp == 0)
		return (FALSE);
	return ((*(p_xdrproc_t)xdr_args)(&srp->xdr_stream, args_ptr));
}

/* ARGSUSED */
static bool_t
svcraw_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args, void * args_ptr)
{ 
	register struct svcraw_private *srp = svcraw_private;
	register XDR *xdrs;

	if (srp == 0)
		return (FALSE);
	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_FREE;
	return ((*(p_xdrproc_t)xdr_args)(xdrs, args_ptr));
} 

/* ARGSUSED */
static void
svcraw_destroy(SVCXPRT *xprt)
{
}
