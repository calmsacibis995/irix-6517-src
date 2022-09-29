#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = 	"@(#)clnt_generic.c	1.4 90/07/19 4.1NFSSRC Copyr 1990 Sun Micro";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.5 88/02/08 
 */

#ifdef __STDC__
	#pragma weak clnt_create = _clnt_create
	#pragma weak clnt_create_vers = _clnt_create_vers
#endif
#include "synonyms.h"

#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netdb.h>
#include <sys/time.h>
#include <bstring.h>

static CLIENT * _rpc_clnt_create_both(const char *, unsigned long, unsigned long *,
	unsigned long, unsigned long, const char *, int);
/*
 * Generic client creation: takes (hostname, program-number, protocol) and
 * returns client handle. Default options are set, which the user can 
 * change using the rpc equivalent of ioctl()'s.
 */
CLIENT *
clnt_create(hostname, prog, vers, proto)
	const char *hostname;
	unsigned long prog;
	unsigned long vers;
	const char *proto;
{
	unsigned long vers_out;
	return (_rpc_clnt_create_both(hostname, prog, &vers_out, vers,
		vers, proto, FALSE));

}

/*
 * Generic client creation with version checking the value of
 * vers_out is set to the highest server supported value
 * vers_low <= vers_out <= vers_high  AND an error results
 * if this can not be done. Mount uses this.
 */

CLIENT *
clnt_create_vers(hostname, prog, vers_out, vers_low, vers_high,
	proto)
	const char *hostname;
	unsigned long prog;
	unsigned long *vers_out;
	unsigned long vers_low;
	unsigned long vers_high;
	const char *proto;
{
	return (_rpc_clnt_create_both(hostname, prog, vers_out, vers_low,
		vers_high, proto, TRUE));
}

/*
 * Generic client creation: takes (hostname, program-number, protocol) and
 * returns client handle. Default options are set, which the user can
 * change using the rpc equivalent of ioctl()'s.
 */
static CLIENT *
_rpc_clnt_create_both(hostname, prog, vers_out, vers_low, vers_high,
	proto, doversions)
	const char *hostname;
	unsigned long prog;
	unsigned long *vers_out;
	unsigned long vers_low;
	unsigned long vers_high;
	const char *proto;
	int  doversions;
{
	struct hostent *h, hp;
	struct protoent *p, pp;
	struct sockaddr_in sin;
	int sock;
	struct timeval tv, to;
	unsigned long minvers, maxvers;
	struct rpc_err rpcerr;
	int must_work;
	enum clnt_stat rpc_stat;
	CLIENT *client;
	int  herr;
	char buf[4096];
			

	h=gethostbyname_r(hostname, &hp, buf, sizeof(buf), &herr);
		
	if (h == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNHOST;
		return (NULL);
	}
	if (h->h_addrtype != AF_INET) {
		/*
		 * Only support INET for now
		 */
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = EAFNOSUPPORT; 
		return (NULL);
	}
	sin.sin_family = (short)h->h_addrtype;
	sin.sin_port = 0;
	bzero(sin.sin_zero, sizeof(sin.sin_zero));
	bcopy(h->h_addr, (char*)&sin.sin_addr, h->h_length);
	p = getprotobyname_r(proto, &pp, buf, sizeof(buf));
	if (p == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		rpc_createerr.cf_error.re_errno = EPFNOSUPPORT; 
		return (NULL);
	}
	for (must_work = 0; must_work <= 1; must_work++) {

	sock = RPC_ANYSOCK;
	switch (p->p_proto) {
	case IPPROTO_UDP:
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		client = clntudp_create(&sin, prog, vers_high, tv, &sock);
		if (client == NULL) {
			return (NULL);
		}
		tv.tv_sec = 25;
		clnt_control(client, CLSET_TIMEOUT, &tv);
		break;
	case IPPROTO_TCP:
		client = clnttcp_create(&sin, prog, vers_high, &sock, 0, 0);
		if (client == NULL) {
			return (NULL);
		}
		tv.tv_sec = 25;
		tv.tv_usec = 0;
		clnt_control(client, CLSET_TIMEOUT, &tv);
		break;
	default:
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = EPFNOSUPPORT; 
		return (NULL);
	    }

	    if (doversions) {
		to.tv_sec = 10;
		to.tv_usec = 0;
		rpc_stat = clnt_call(client, NULLPROC, (xdrproc_t) xdr_void, (char *) NULL,
			(xdrproc_t) xdr_void, (char *) NULL, to);
		if (rpc_stat == RPC_SUCCESS) {
			*vers_out = vers_high;
			return (client);
		} else if ((must_work == 0) &&
				(rpc_stat == RPC_PROGVERSMISMATCH)) {
			clnt_geterr(client, &rpcerr);
			minvers = rpcerr.re_vers.low;
			maxvers = rpcerr.re_vers.high;
			if (maxvers < vers_high)
				vers_high = maxvers;
			if (minvers > vers_low)
				vers_low = minvers;
			if (vers_low > vers_high) {
				rpc_createerr.cf_stat = rpc_stat;
				rpc_createerr.cf_error = rpcerr;
				clnt_destroy(client);
				return (NULL);
			}
			clnt_destroy(client);
		} else {
			clnt_geterr(client, &rpcerr);
			rpc_createerr.cf_stat = rpc_stat;
			rpc_createerr.cf_error = rpcerr;
			clnt_destroy(client);
			return (NULL);
		}
	    } else {
		break;
	    }
	}
	return (client);
}
