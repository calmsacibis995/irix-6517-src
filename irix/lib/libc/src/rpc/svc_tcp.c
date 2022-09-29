/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.22 88/02/08
 */


/*
 * svc_tcp.c, Server side for TCP/IP based RPC. 
 *
 * Actually implements two flavors of transporter -
 * a tcp rendezvouser (a listner and connection establisher)
 * and a record/tcp stream.
 */

#ifdef __STDC__
	#pragma weak svctcp_create = _svctcp_create
	#pragma weak svcfd_create = _svcfd_create
#endif
#include "synonyms.h"

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/errorhandler.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>		/* prototype for strerror() */
#include <bstring.h>		/* prototype for bzero() */
#include <unistd.h>		/* prototype for close() */
#include "priv_extern.h"

#include <sys/mac.h>
#include <sys/ioctl.h>
#include <t6net.h>

extern int __svc_label_agile;

/*
 * Ops vector for TCP/IP based rpc service handle
 */

static bool_t svctcp_recv(SVCXPRT *xprt, register struct rpc_msg *msg);
static enum xprt_stat svctcp_stat(SVCXPRT *xprt);
static bool_t svctcp_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, void *args_ptr);
static bool_t svctcp_reply(SVCXPRT *xprt, register struct rpc_msg *msg);
static bool_t svctcp_freeargs(SVCXPRT *, xdrproc_t, void *);
static void svctcp_destroy(register SVCXPRT *xprt);

static struct xp_ops svctcp_op = {
	svctcp_recv,
	svctcp_stat,
	svctcp_getargs,
	svctcp_reply,
	svctcp_freeargs,
	svctcp_destroy
};

/*
 * Ops vector for TCP/IP rendezvous handler
 */
static bool_t		rendezvous_request(SVCXPRT *, struct rpc_msg *);
static enum xprt_stat	rendezvous_stat(SVCXPRT *);
static bool_t		rendezvous_abort(void);

static struct xp_ops svctcp_rendezvous_op = {
	rendezvous_request,
	rendezvous_stat,
	(bool_t (*)(SVCXPRT *, xdrproc_t, void *))rendezvous_abort,
	(bool_t (*)(SVCXPRT *, struct rpc_msg *))rendezvous_abort,
	(bool_t (*)(SVCXPRT *, xdrproc_t, void *))rendezvous_abort,
	svctcp_destroy
};

static int readtcp(void *, void *, u_int);
static int writetcp(void *, void *, u_int);
static SVCXPRT *makefd_xprt(int, u_int, u_int);

struct tcp_rendezvous { /* kept in xprt->xp_p1 */
	u_int sendsize;
	u_int recvsize;
};

struct tcp_conn {  /* kept in xprt->xp_p1 */
	enum xprt_stat strm_stat;
	u_long x_id;
	XDR xdrs;
	char verf_body[MAX_AUTH_BYTES];
	bool_t needflush;
};

/*
 * Usage:
 *	xprt = svctcp_create(sock, send_buf_size, recv_buf_size);
 *
 * Creates, registers, and returns a (rpc) tcp based transporter.
 * Once *xprt is initialized, it is registered as a transporter
 * see (svc.h, xprt_register).  This routine returns
 * a NULL if a problem occurred.
 *
 * If sock<0 then a socket is created, else sock is used.
 * If the socket, sock is not bound to a port then svctcp_create
 * binds it to an arbitrary port.  The routine then starts a tcp
 * listener on the socket's associated port.  In any (successful) case,
 * xprt->xp_sock is the registered socket number and xprt->xp_port is the
 * associated port number.
 *
 * Since tcp streams do buffered io similar to stdio, the caller can specify
 * how big the send and receive buffers are via the second and third parms;
 * 0 => use the system default.
 */
SVCXPRT *
svctcp_create(register int sock, u_int sendsize, u_int recvsize)
{
	bool_t madesock = FALSE;
	register SVCXPRT *xprt;
	register struct tcp_rendezvous *r;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);

	if (sock == RPC_ANYSOCK) {
		if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			_rpc_errorhandler(LOG_ERR, "svctcp_create - tcp socket creation problem: %s", strerror(errno));
			return ((SVCXPRT *)NULL);
		}
		madesock = TRUE;
	}
	if (__svc_label_agile && tsix_on (sock) == -1) {
		_rpc_errorhandler(LOG_ERR, "svcudp_create: tsix_on failed: %s", strerror(errno));
		if (madesock)
			(void)close(sock);
		return ((SVCXPRT *)NULL);
	}
	bzero((char *)&addr, sizeof (addr));
	addr.sin_family = AF_INET;
	if (bindresvport(sock, &addr)) {
		addr.sin_port = 0;
		(void)bind(sock, (struct sockaddr *)&addr, len);
	}
	if ((getsockname(sock, (struct sockaddr *)&addr, &len) != 0)  ||
	    (listen(sock, 2) != 0)) {
		_rpc_errorhandler(LOG_ERR, "svctcp_create - cannot getsockname or listen: %s", strerror(errno));
		if (madesock)
		       (void)close(sock);
		return ((SVCXPRT *)NULL);
	}
	r = (struct tcp_rendezvous *)mem_alloc(sizeof(*r));
	if (r == NULL) {
		_rpc_errorhandler(LOG_ERR, "svctcp_create: out of memory");
		if (madesock)
			(void)close(sock);
		return (NULL);
	}
	r->sendsize = sendsize;
	r->recvsize = recvsize;
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL) {
		_rpc_errorhandler(LOG_ERR, "svctcp_create: out of memory");
		mem_free((char *) r, sizeof(*r));
		if (madesock)
			(void)close(sock);
		return (NULL);
	}
	xprt->xp_p2 = NULL;
	xprt->xp_p3 = NULL;
	xprt->xp_p1 = (caddr_t)r;
	xprt->xp_verf = _null_auth;
	xprt->xp_ops = &svctcp_rendezvous_op;
	xprt->xp_port = ntohs(addr.sin_port);
	xprt->xp_sock = sock;
	xprt_register(xprt);
	return (xprt);
}

/*
 * Like svtcp_create(), except the routine takes any *open* UNIX file
 * descriptor as its first input.
 */
SVCXPRT *
svcfd_create(int fd, u_int sendsize, u_int recvsize)
{

	return (makefd_xprt(fd, sendsize, recvsize));
}

static SVCXPRT *
makefd_xprt(int fd, u_int sendsize, u_int recvsize)
{
	register SVCXPRT *xprt;
	register struct tcp_conn *cd;
	mac_t lbl = NULL;
 
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == (SVCXPRT *)NULL) {
		_rpc_errorhandler(LOG_ERR, "svc_tcp: makefd_xprt: out of memory");
		goto done;
	}
	cd = (struct tcp_conn *)mem_alloc(sizeof(struct tcp_conn));
	if (cd == (struct tcp_conn *)NULL) {
		_rpc_errorhandler(LOG_ERR, "svc_tcp: makefd_xprt: out of memory");
		mem_free((char *) xprt, sizeof(SVCXPRT));
		xprt = (SVCXPRT *)NULL;
		goto done;
	}
	if (__svc_label_agile &&
	    (tsix_get_mac(fd, &lbl) == -1 || tsix_set_mac(fd, lbl) == -1)) {
		_rpc_errorhandler(LOG_ERR, "svc_tcp: makefd_xprt: tsix_{get|set}_mac failed: %s", strerror(errno));
		mac_free(lbl);
		mem_free((caddr_t)cd, sizeof(struct tcp_conn));
		mem_free((caddr_t)xprt, sizeof(SVCXPRT));
		xprt = (SVCXPRT *)NULL;
		goto done;
	}
	mac_free(lbl);
	cd->strm_stat = XPRT_IDLE;
	cd->needflush = FALSE;
	xdrrec_create(&(cd->xdrs), sendsize, recvsize,
	    (caddr_t)xprt, readtcp, writetcp);
	xprt->xp_p2 = NULL;
	xprt->xp_p1 = (caddr_t)cd;
	xprt->xp_verf.oa_base = cd->verf_body;
	xprt->xp_addrlen = 0;
	xprt->xp_ops = &svctcp_op;  /* truely deals with calls */
	xprt->xp_port = 0;  /* this is a connection, not a rendezvouser */
	xprt->xp_sock = fd;
	xprt_register(xprt);
    done:
	return (xprt);
}

/* ARGSUSED */
static bool_t
rendezvous_request(register SVCXPRT *xprt, struct rpc_msg *m)
{
	int sock;
	struct tcp_rendezvous *r;
	struct sockaddr_in addr;
	int len;

	r = (struct tcp_rendezvous *)xprt->xp_p1;
    again:
	len = sizeof(struct sockaddr_in);
	if ((sock = accept(xprt->xp_sock, (struct sockaddr *)&addr,
	    &len)) < 0) {
		if (errno == EINTR)
			goto again;
	       return (FALSE);
	}
	/*
	 * make a new transporter (re-uses xprt)
	 */
	xprt = makefd_xprt(sock, r->sendsize, r->recvsize);
	xprt->xp_raddr = addr;
	xprt->xp_addrlen = len;
	return (FALSE); /* there is never an rpc msg to be processed */
}

/* ARGSUSED */
static enum xprt_stat
rendezvous_stat(SVCXPRT *xprt)
{
	return (XPRT_IDLE);
	/* NOTREACHED */
	/* this is just here to hush a compiler warning bug... */
	xprt->xp_p3 = NULL;
}

static bool_t
rendezvous_abort(void)
{
	_rpc_errorhandler(LOG_ERR, "svctcp: rendezvous: invalid function");
	exit(1);
	/* NOTREACHED */
}

static void
svctcp_destroy(register SVCXPRT *xprt)
{
	register struct tcp_conn *cd = (struct tcp_conn *)xprt->xp_p1;

	xprt_unregister(xprt);
	(void)close(xprt->xp_sock);
	if (xprt->xp_port != 0) {
		/* a rendezvouser socket */
		xprt->xp_port = 0;
	} else {
		/* an actual connection socket */
		XDR_DESTROY(&(cd->xdrs));
	}
	mem_free((caddr_t)cd, sizeof(struct tcp_conn));
	mem_free((caddr_t)xprt, sizeof(SVCXPRT));
}

/*
 * All read operations timeout after 35 seconds.
 * A timeout is fatal for the connection.
 */
static const struct timeval wait_per_try = { 35, 0 };

/*
 * reads data from the tcp conection.
 * Return -1 on EINTR or EWOULDBLOCK.  Any other error is fatal.
 * (And a read of zero bytes is a half closed stream => error.)
 */
static int
readtcp(void *v, void *buf, register u_int len)
{
	register SVCXPRT *xprt = (SVCXPRT *)v;
	register int sock = xprt->xp_sock;
	fd_set readfds;

	FD_ZERO(&readfds);

	do {
		FD_SET(sock, &readfds);
		if (select(sock+1, &readfds, (fd_set *)NULL, (fd_set *)NULL, 
			   (struct timeval *)&wait_per_try) <= 0) {
			switch (errno) {
			    case EINTR:
			    case EWOULDBLOCK:
				return (-1);

			    default:
				goto fatal_err;
			}
		}
	} while (!FD_ISSET(sock, &readfds));
	if ((len = (int)read(sock, buf, (size_t)len)) > 0)
		return (len);
fatal_err:
	((struct tcp_conn *)(xprt->xp_p1))->strm_stat = XPRT_DIED;
	return (-1);
}

/*
 * writes data to the tcp connection.
 * Any error is fatal and the connection is closed.
 */
static int
writetcp(void *v, void *buf, u_int len)
{
	register int i, cnt;
	register SVCXPRT *xprt = (SVCXPRT *)v;
	register struct tcp_conn *cd = (struct tcp_conn *)xprt->xp_p1;

	for (cnt = len; cnt > 0; cnt -= i, buf = (char *)buf + i) {
		if ((i = (int)write(xprt->xp_sock, buf, (size_t)cnt)) < 0) {
			switch (errno) {
			    case EWOULDBLOCK:
			    case EINTR:
				cd->needflush = TRUE;
				return (len - cnt);

			    default:
				cd->strm_stat = XPRT_DIED;
				return (-1);
			}
		}
	}
	cd->needflush = FALSE;
	return (len);
}

static enum xprt_stat
svctcp_stat(SVCXPRT *xprt)
{
	register struct tcp_conn *cd =
	    (struct tcp_conn *)(xprt->xp_p1);

	if (cd->strm_stat == XPRT_DIED)
		return (XPRT_DIED);
	if (! xdrrec_eof(&(cd->xdrs)))
		return (XPRT_MOREREQS);
	return (XPRT_IDLE);
}

static bool_t
svctcp_recv(SVCXPRT *xprt, register struct rpc_msg *msg)
{
	register struct tcp_conn *cd =
	    (struct tcp_conn *)(xprt->xp_p1);
	register XDR *xdrs = &(cd->xdrs);

	xdrs->x_op = XDR_DECODE;
	(void)xdrrec_skiprecord(xdrs);
	if (xdr_callmsg(xdrs, msg)) {
		cd->x_id = msg->rm_xid;
		return (TRUE);
	}
	return (FALSE);
}

static bool_t
svctcp_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, void *args_ptr)
{

	return ((*(p_xdrproc_t)xdr_args)(&(((struct tcp_conn *)(xprt->xp_p1))->xdrs), args_ptr));
}

static bool_t
svctcp_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args, void *args_ptr)
{
	register XDR *xdrs =
	    &(((struct tcp_conn *)(xprt->xp_p1))->xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*(p_xdrproc_t)xdr_args)(xdrs, args_ptr));
}

static bool_t
svctcp_reply(SVCXPRT *xprt, register struct rpc_msg *msg)
{
	register struct tcp_conn *cd =
	    (struct tcp_conn *)(xprt->xp_p1);
	register XDR *xdrs = &(cd->xdrs);
	register bool_t stat;

	xdrs->x_op = XDR_ENCODE;
	if (cd->needflush) {
		if (!xdrrec_endofrecord(xdrs, TRUE))
			return (FALSE);
	}
	msg->rm_xid = cd->x_id;
	stat = xdr_replymsg(xdrs, msg);
	return (xdrrec_endofrecord(xdrs, TRUE) && stat);
}
