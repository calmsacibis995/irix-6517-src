/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Remote network interface snooper implementation.
 */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include "exception.h"
#include "expr.h"
#include "heap.h"
#include "macros.h"
#include "snooper.h"
#include "snoopstream.h"

DefineSnooperOperations(remote_snops,rsn)

/*
 * Remote network interface snooper.
 */
typedef struct {
	char		*hostname;
	char		*interface;
	SnoopStream	ss;
	Snooper		snooper;	/* base class state */
} RemoteSnooper;

#define	RSN(sn)	((RemoteSnooper *) (sn)->sn_private)

static int
rsn_geterror(RemoteSnooper *rsn)
{
	switch (rsn->ss.ss_rpcerr.re_status) {
	  case RPC_SUCCESS:
	  case RPC_CANTSEND:
	  case RPC_CANTRECV:
	  case RPC_SYSTEMERROR:
		return rsn->ss.ss_rpcerr.re_errno;
	  case RPC_TIMEDOUT:
		return ETIMEDOUT;
	  case RPC_INTR:
		return EINTR;
	  case RPC_VERSMISMATCH:
	  case RPC_PROGUNAVAIL:
	  case RPC_PROGVERSMISMATCH:
	  case RPC_PROCUNAVAIL:
		return EINVAL;
	  case RPC_AUTHERROR:
		return EACCES;
	  default:
		return EIO;
	}
}

Snooper *
remotesnooper(char *hostname, char *interface, struct timeval *to,
	      int buffer, int count, int interval)
{
	RemoteSnooper *rsn;
	struct sockaddr_in sin;
	struct hostent* hp;
	int error, on = 1;

	/*
	 * Get named host's address.
	 */
	hp = gethostbyname(hostname);
	if (hp == 0 || hp->h_addrtype != AF_INET) {
		exc_raise(0, "cannot find hostname %s", hostname);
		return 0;
	}
	bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
	sin.sin_family = AF_INET;
	sin.sin_port = 0;

	/*
	 * Allocate and set private and ops.
	 */
	rsn = new(RemoteSnooper);
	rsn->snooper.sn_private = (char *) rsn;
	rsn->snooper.sn_ops = &remote_snops;

	/*
	 * Create a snoopstream to the server
	 */
	if (!ss_open(&rsn->ss, hostname, &sin, to)) {
		error = rsn_geterror(rsn);
		goto fail;
	}

	/*
	 * Set the snoopstream to return on EINTR
	 */
	clnt_control(rsn->ss.ss_client, CLSET_EINTR_RETURN, &on);

	if (!ss_subscribe(&rsn->ss, SS_NETSNOOP, interface,
			  buffer, count, interval)) {
		error = rsn_geterror(rsn);
		goto close;
	}

	if (!sn_init(&rsn->snooper, rsn, &remote_snops, hostname,
		     rsn->ss.ss_sock, rsn->ss.ss_rawproto)) {
		error = rsn->snooper.sn_error;
		goto close;
	}

	rsn->hostname = (hostname) ? strdup(hostname) : 0;
	rsn->interface = (interface) ? strdup(interface) : 0;
	return &rsn->snooper;

close:
	ss_close(&rsn->ss);
fail:
	delete(rsn);
	errno = error;
	return 0;
}

static int
rsn_add(Snooper *sn, Expr **exp, ExprError *err)
{
	RemoteSnooper *rsn = RSN(sn);
	Expr *ex = (exp == 0) ? 0 : *exp;

	exc_defer++;
	if (ss_add(&rsn->ss, ex, err) < 0) {
		sn->sn_error = rsn_geterror(rsn);
		return 0;
	}
	--exc_defer;
	if (ex) {
		ex_destroy(ex);
		*exp = 0;
	}
	return 1;
}

static int
rsn_delete(Snooper *sn)
{
	RemoteSnooper *rsn = RSN(sn);

	exc_defer++;
	if (!ss_delete(&rsn->ss, 0))
		return 0;
	--exc_defer;
	return 1;
}

static int
rsn_read(Snooper *sn, SnoopPacket *sp, int len)
{
	SnoopPacketWrap spw;
	RemoteSnooper *rsn = RSN(sn);

	spw.spw_sp = sp;
	spw.spw_len = (unsigned int *) &len;
	spw.spw_maxlen = len;

	exc_defer++;
	if (!ss_read(&rsn->ss, (xdrproc_t) xdr_snooppacketwrap, &spw)) {
		sn->sn_error = rsn_geterror(rsn);
		return -1;
	}
	--exc_defer;
	return len;
}

static int
rsn_write(Snooper *sn, SnoopPacket *sp, int len)
{
	return 0;
}

static int
rsn_ioctl(Snooper *sn, int cmd, void *arg)
{
	RemoteSnooper *rsn = RSN(sn);

	switch (cmd) {
	  case FIONBIO:
		if (ioctl(rsn->ss.ss_sock, cmd, arg) < 0) {
			sn->sn_error = errno;
			return 0;
		}
		break;

	  case SIOCSNOOPLEN:
		exc_defer++;
		if (!ss_setsnooplen(&rsn->ss, *(unsigned int *)arg)) {
			sn->sn_error = rsn_geterror(rsn);
			return 0;
		}
		--exc_defer;
		break;

	  case SIOCSNOOPING:
		exc_defer++;
		if (!(*(int *)arg ? ss_start(&rsn->ss) : ss_stop(&rsn->ss))) {
			sn->sn_error = rsn_geterror(rsn);
			return 0;
		}
		--exc_defer;
		break;

	  case SIOCERRSNOOP:
		exc_defer++;
		if (!ss_seterrflags(&rsn->ss, *(int *)arg)) {
			sn->sn_error = rsn_geterror(rsn);
			return 0;
		}
		--exc_defer;
		break;

	  case SIOCRAWSTATS:
		exc_defer++;
		if (!ss_getstats(&rsn->ss, arg)) {
			sn->sn_error = rsn_geterror(rsn);
			return 0;
		}
		--exc_defer;
		break;

	  default:
		sn->sn_error = EINVAL;
		return 0;
	}
	return 1;
}

static int
rsn_getaddr(Snooper *sn, int cmd, struct sockaddr *sa)
{
	RemoteSnooper *rsn = RSN(sn);

	exc_defer++;
	if (!ss_getaddr(&rsn->ss, cmd, sa)) {
		sn->sn_error = rsn_geterror(rsn);
		return 0;
	}
	--exc_defer;
	return 1;
}

static int
rsn_shutdown(Snooper *sn, enum snshutdownhow how)
{
	RemoteSnooper *rsn = RSN(sn);

	exc_defer++;
	if (!ss_unsubscribe(&rsn->ss)) {
		sn->sn_error = rsn_geterror(rsn);
		return 0;
	}
	--exc_defer;
	if (shutdown(rsn->ss.ss_sock, (int) how) < 0) {
		sn->sn_error = errno;
		return 0;
	}
	return 1;
}

static void
rsn_destroy(Snooper *sn)
{
	RemoteSnooper *rsn = RSN(sn);

	ss_close(&rsn->ss);
	delete(rsn->hostname);
	delete(rsn->interface);
	delete(rsn);
}
