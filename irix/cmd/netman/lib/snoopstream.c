/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * 	SnoopStream functions
 *
 *	$Revision: 1.17 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <bstring.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include "exception.h"
#include "histogram.h"
#include "snoopstream.h"
#include "snoopd_rpc.h"
#include "protocol.h"
#include "snooper.h"
#include "license.h"
#include <rpc/rpc_msg.h>	/* These two just for RPC_ANYSOCK! */
#include <rpc/svc.h>

/*
 * Open a socket and set up RPC using SnoopStream structure pointed to by ss.
 */
int
ss_open(SnoopStream *ss, char *server, struct sockaddr_in *srv_addr,
	struct timeval *timeout)
{
	struct sockaddr_in sin;

	if (srv_addr == 0) {
		struct hostent *hp;

		hp = gethostbyname(server);
		if (hp == 0 || hp->h_addrtype != AF_INET) {
			exc_raise(0, "cannot find hostname %s", server);
			return 0;
		}
		bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
		sin.sin_family = AF_INET;
		sin.sin_port = 0;
		srv_addr = &sin;
	}

	/* Check licensing */
	if (activateLicense(srv_addr->sin_addr) == 0) {
		exc_raise(0, "snoopd on %s: %s", server, exc_message);
		return 0;
	}

	ss->ss_sock = RPC_ANYSOCK;
	ss->ss_client = clnttcp_create(srv_addr, SNOOPPROG, SNOOPVERS,
				       &ss->ss_sock, 8192, 8192);
	if (ss->ss_client == 0) {
		ss->ss_rpcerr = rpc_createerr.cf_error;
		exc_raise(0, "snoopd on %s", clnt_spcreateerror(server));
		return 0;
	}
	ss->ss_client->cl_auth = authunix_create_default();
	ss->ss_service = SS_NULL;
	if (timeout == 0) {
		ss->ss_timeout.tv_sec = 10;
		ss->ss_timeout.tv_usec = 0;
	} else
		ss->ss_timeout = *timeout;
	ss->ss_server = strdup(server);
	ss->ss_srvaddr = srv_addr->sin_addr;
	ss->ss_rpcerr.re_errno = 0;
	return 1;
}

/*
 * Close a snoopstream.
 */
void
ss_close(SnoopStream *ss)
{
	if (ss->ss_service != SS_NULL)
		(void) ss_unsubscribe(ss);
	auth_destroy(ss->ss_client->cl_auth);
	clnt_destroy(ss->ss_client);
	close(ss->ss_sock);
	deactivateLicense(ss->ss_srvaddr);
	delete(ss->ss_server);
}

/*
 * Exception handling.
 */
static char *svcname[] = { "none", "netsnoop", "netlook", "histogram",
			   "addrlist" };

static u_char snoopstat_to_errno[] = {
	0,		/* SNOOP_OK = 0 */
	EPERM,		/* SNOOPERR_PERM = 1 */
	EINVAL,		/* SNOOPERR_INVAL = 2 */
	ENXIO,		/* SNOOPERR_NOIF = 3 */
	EINVAL,		/* SNOOPERR_NOSVC = 4 */
	EINVAL,		/* SNOOPERR_BADF = 5 */
	EAGAIN,		/* SNOOPERR_AGAIN = 6 */
	EIO,		/* SNOOPERR_IO = 7 */
};

static void
ss_raise_errno(SnoopStream *ss, enum snoopstat status, char *format, ...)
{
	int error;
	va_list ap;

	ss->ss_rpcerr.re_status = RPC_SUCCESS;
	error = snoopstat_to_errno[(int) status];
	ss->ss_rpcerr.re_errno = error;
	va_start(ap, format);
	exc_vraise(error, format, ap);
	va_end(ap);
}

static void
ss_raise_rpcerr(SnoopStream *ss, enum clnt_stat clnt_stat)
{
	char *message;

	clnt_geterr(ss->ss_client, &ss->ss_rpcerr);
	message = clnt_sperrno(clnt_stat);
	switch (clnt_stat) {
	  case RPC_VERSMISMATCH:
	  case RPC_AUTHERROR:
	  case RPC_PROGUNAVAIL:
	  case RPC_PROGVERSMISMATCH:
	  case RPC_PROCUNAVAIL:
	  case RPC_CANTDECODEARGS:
		exc_raise(0, "snoopd on %s: %s", ss->ss_server, message);
		break;
	  case RPC_CANTSEND:
	  case RPC_CANTRECV:
	  case RPC_SYSTEMERROR:
		exc_raise(ss->ss_rpcerr.re_errno, "snoopd on %s: %s",
			  ss->ss_server, message);
		break;
	  case RPC_INTR:
		ss->ss_rpcerr.re_errno = EINTR;
		/* FALL THROUGH */
	  default:
		exc_raise(0, "snoopd on %s: %s", ss->ss_server, message);
	}
}

/*
 * Subscribe to a service.
 */
int
ss_subscribe(SnoopStream *ss, enum svctype service, char *interface,
	     u_int buffer, u_int count, u_int interval)
{
	enum clnt_stat clnt_stat;
	struct subreq sr;
	struct intres ir;

	/* Fill in the arguments */
	sr.sr_interface = interface;
	sr.sr_service = service;
	sr.sr_buffer = buffer;
	sr.sr_count = count;
	sr.sr_interval = interval;

	/* Do the RPC */
	clnt_stat = clnt_call(ss->ss_client, SNOOPPROC_SUBSCRIBE,
			      xdr_subreq, &sr, xdr_intres, &ir, ss->ss_timeout);

	/* Process the reply */
	if (clnt_stat != RPC_SUCCESS) {
		ss_raise_rpcerr(ss, clnt_stat);
		return 0;
	}
	if (ir.ir_status != SNOOP_OK) {
		ss_raise_errno(ss, ir.ir_status,
			       "snoopd on %s: Error in subscribe",
			       ss->ss_server);
		return 0;
	}
	ss->ss_rawproto = findprotobyid(ir.ir_value);
	ss->ss_service = service;
	return 1;
}

/*
 * Common client RPC stub code.
 */
static char *procname[] = {
	"null",
	"subscribe",
	"unsubscribe",
	"setsnooplen",
	"seterrflags",
	"addfilter",
	"deletefilter",
	"startcapture",
	"stopcapture",
	"getstats",
	"getaddr",
};

static int
ss_call(SnoopStream *ss, u_long proc, xdrproc_t xdrarg, void *arg,
	xdrproc_t xdrres, void *res)
{
	enum clnt_stat clnt_stat;

	if (ss->ss_service == SS_NULL) {
		ss_raise_errno(ss, SNOOPERR_INVAL, "%s before subscribe",
			       procname[proc]);
		return 0;
	}
	clnt_stat = clnt_call(ss->ss_client, proc, xdrarg, arg,
			      xdrres, res, ss->ss_timeout);
	if (clnt_stat != RPC_SUCCESS) {
		ss_raise_rpcerr(ss, clnt_stat);
		return 0;
	}
	return 1;
}

/*
 * Unsubscribe.
 */
int
ss_unsubscribe(SnoopStream *ss)
{
	enum snoopstat status;

	if (!ss_call(ss, SNOOPPROC_UNSUBSCRIBE, xdr_void, 0,
		     xdr_enum, &status)) {
		return 0;
	}
	if (status != SNOOP_OK) {
		ss_raise_errno(ss, status,
			       "snoopd %s service on %s: error in unsubscribe",
			       svcname[(int) ss->ss_service], ss->ss_server);
		return 0;
	}
	ss->ss_service = SS_NULL;
	return 1;
}

/*
 * Set the snoop length.
 */
int
ss_setsnooplen(SnoopStream *ss, u_int snooplen)
{
	enum snoopstat status;

	if (!ss_call(ss, SNOOPPROC_SETSNOOPLEN, xdr_u_int, &snooplen,
		     xdr_enum, &status)) {
		return 0;
	}
	if (status != SNOOP_OK) {
		ss_raise_errno(ss, status,
			       "snoopd on %s: could not set snoop length to %u",
			       ss->ss_server, snooplen);
		return 0;
	}
	return 1;
}

/*
 * Set the interval, this is important for histogram service
 */
int
ss_setinterval(SnoopStream *ss, u_int interval)
{
	enum snoopstat status;

	if (!ss_call(ss, SNOOPPROC_SETINTERVAL, xdr_u_int, &interval,
		     xdr_enum, &status)) {
		return 0;
	}
	if (status != SNOOP_OK) {
		ss_raise_errno(ss, status,
			       "snoopd on %s: could not set interval",
			       ss->ss_server);
		return 0;
	}
	return 1;
}

/*
 * Snoop for errors.
 */
int
ss_seterrflags(SnoopStream *ss, u_int flags)
{
	enum snoopstat status;

	if (!ss_call(ss, SNOOPPROC_SETERRFLAGS, xdr_u_int, &flags,
		     xdr_enum, &status)) {
		return 0;
	}
	if (status != SNOOP_OK) {
		ss_raise_errno(ss, status,
			       "snoopd on %s: could not set error flags",
				ss->ss_server);
		return 0;
	}
	return 1;
}

/*
 * Compile and add a filter to a subscribed service.
 *
 * The return value is defined by the service:
 *	NETSNOOP: < 0 on error;
 *	NETLOOK: < 0 on error;
 *	HISTOGRAM: int bin; ( < 0 on error)
 */
int
ss_compile(SnoopStream *ss, ExprSource *src, ExprError *err)
{
	Snooper *sn;
	Expr *ex;
	int bin;

	if (src == 0 || src->src_buf == 0)
		return ss_add(ss, 0, err);

	sn = nullsnooper(ss->ss_rawproto);
	ex = ex_parse(src, sn, ss->ss_rawproto, err);
	sn_destroy(sn);

	if (ex == 0) {
		ss_raise_errno(ss, SNOOPERR_BADF, "snoopd on %s: %s",
			       ss->ss_server, err->err_message);
		return -1;
	}

	bin = ss_add(ss, ex, err);
	ex_destroy(ex);
	return bin;
}

/*
 * Add a filter to a subscribed service.
 */
int
ss_add(SnoopStream *ss, Expr *ex, ExprError *err)
{
	struct expression exp;
	struct intres ir;

	exp.exp_expr = ex;
	exp.exp_proto = ss->ss_rawproto;

	if (!ss_call(ss, SNOOPPROC_ADDFILTER, xdr_expression, &exp,
		     xdr_intres, &ir)) {
		return -1;
	}
	if (ir.ir_status != SNOOP_OK) {
		ss_raise_errno(ss, ir.ir_status,
			       (ir.ir_status == SNOOPERR_BADF) ? ir.ir_message
				: "snoopd on %s: cannot add %s filter",
			       ss->ss_server, svcname[(int) ss->ss_service]);
		bzero(err, sizeof *err);
		err->err_message = exc_message;
		return -1;
	}
	return ir.ir_value;
}

/*
 * Remove a filter from a subscribed service.  The second argument is the
 * bin number to delete (0 for netlook and netsnoop).
 */
int
ss_delete(SnoopStream *ss, int bin)
{
	enum snoopstat status;

	if (!ss_call(ss, SNOOPPROC_DELETEFILTER, xdr_int, &bin,
		     xdr_enum, &status)) {
		return 0;
	}
	if (status != SNOOP_OK) {
		ss_raise_errno(ss, status,
			       "snoopd on %s: could not delete %s filter",
			       ss->ss_server, svcname[(int) ss->ss_service]);
		return 0;
	}
	return 1;
}

/*
 * Start a service.
 */
int
ss_start(SnoopStream *ss)
{
	enum snoopstat status;

	if (!ss_call(ss, SNOOPPROC_STARTCAPTURE, xdr_void, 0,
		     xdr_enum, &status)) {
		return 0;
	}
	if (status != SNOOP_OK) {
		ss_raise_errno(ss, status,
			       "snoopd on %s: could not start %s service",
			       ss->ss_server, svcname[(int) ss->ss_service]);
		return 0;
	}
	return 1;
}

/*
 * Stop a service.
 */
int
ss_stop(SnoopStream *ss)
{
	enum snoopstat status;

	if (!ss_call(ss, SNOOPPROC_STOPCAPTURE, xdr_void, 0,
		     xdr_enum, &status)) {
		return 0;
	}
	if (status != SNOOP_OK) {
		ss_raise_errno(ss, status,
			       "snoopd on %s: could not stop %s service",
			       ss->ss_server, svcname[(int) ss->ss_service]);
		return 0;
	}
	return 1;
}

/*
 * Read service-specific data.
 */
int
ss_read(SnoopStream *ss, xdrproc_t xdrres, void *res)
{
	enum clnt_stat clnt_stat;
	struct timeval tv;
	extern enum clnt_stat clnttcp_getreply(CLIENT *h, xdrproc_t,
					       caddr_t, struct timeval);


	/* Check if subscribed to a service */
	if (ss->ss_service == SS_NULL) {
		ss_raise_errno(ss, SNOOPERR_INVAL, "read before subscribe");
		return 0;
	}

	/* Do the appropriate getreply */
	tv.tv_sec = 1000000;		/* XXX should select indefinitely */
	tv.tv_usec = 0;
	clnt_stat = clnttcp_getreply(ss->ss_client, xdrres, res, tv);

	/* Process the reply */
	if (clnt_stat != RPC_SUCCESS) {
		ss_raise_rpcerr(ss, clnt_stat);
		return 0;
	}
	return 1;
}

/*
 * Get snoop statistics.
 */
int
ss_getstats(SnoopStream *ss, struct snoopstats *stats)
{
	return ss_call(ss, SNOOPPROC_GETSTATS, xdr_void, 0,
		       xdr_snoopstats, stats);
}

/*
 * Get address.
 */
int
ss_getaddr(SnoopStream *ss, int cmd, struct sockaddr *sa)
{
	struct getaddrarg gaa;
	struct getaddrres gar;

	/* Translate cmd to an OS-independent encoding */
	switch (cmd) {
	  case SIOCGIFADDR:
		gaa.gaa_cmd = GET_IFADDR;
		break;
	  case SIOCGIFDSTADDR:
		gaa.gaa_cmd = GET_IFDSTADDR;
		break;
	  case SIOCGIFBRDADDR:
		gaa.gaa_cmd = GET_IFBRDADDR;
		break;
	  case SIOCGIFNETMASK:
		gaa.gaa_cmd = GET_IFNETMASK;
		break;
	  default:
		ss_raise_errno(ss, SNOOPERR_INVAL, "unknown command %#x", cmd);
		return 0;
	}
	gaa.gaa_addr = *sa;

	/* Do the RPC */
	if (!ss_call(ss, SNOOPPROC_GETADDR, xdr_getaddrarg, &gaa,
		     xdr_getaddrres, &gar)) {
		return 0;
	}

	/* Process the reply */
	if (gar.gar_status != SNOOP_OK) {
		ss_raise_errno(ss, gar.gar_status,
			       "snoopd on %s: could not get address",
			       ss->ss_server);
		return 0;
	}
	*sa = gar.gar_addr;
	return 1;
}
