/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)librpc:rpcb_clnt.c	1.10.6.1"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 
/*
 * rpcb_clnt.c
 * interface to rpcbind rpc service.
 *
 */
#if defined(__STDC__) 
        #pragma weak rpcb_getaddr	= _rpcb_getaddr
        #pragma weak rpcb_getmaps	= _rpcb_getmaps
        #pragma weak rpcb_gettime	= _rpcb_gettime
        #pragma weak rpcb_rmtcall	= _rpcb_rmtcall
        #pragma weak rpcb_set		= _rpcb_set
        #pragma weak rpcb_unset		= _rpcb_unset
#ifndef _LIBNSL_ABI
        #pragma weak rpcb_taddr2uaddr	= _rpcb_taddr2uaddr
        #pragma weak rpcb_uaddr2taddr	= _rpcb_uaddr2taddr
#endif /* _LIBNSL_ABI */
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

#include <rpc/rpc.h>
#include <rpc/rpcb_prot.h>
#include <netconfig.h>
#include <netdir.h>
#include <rpc/nettype.h>
#ifdef PORTMAP
#include <netinet/in.h>		/* FOR IPPROTO_TCP/UDP definitions */
#endif
#ifdef ND_DEBUG
#include <stdio.h>
#endif

static struct timeval tottimeout = { 60, 0 };
static struct timeval rmttimeout = { 3, 0 };

extern int errno;
extern int t_errno;
extern char *strdup();

static char nullstring[] = "\000";

/*
 * This routine will return a client handle that is connected to the
 * rpcbind. Returns NULL on error and free's everything. On success it
 * leaves an open fd.
 */
static CLIENT *
getclnthandle(
	const char *host,
	struct netconfig *nconf)
{
	int fd;
	register CLIENT *client;
	struct netbuf *addr;
	struct nd_addrlist *nas;
	struct nd_hostserv rpcbind_hs;

	if ((fd = t_open(nconf->nc_device, O_RDWR, NULL)) == -1) {
		rpc_createerr.cf_stat = RPC_TLIERROR;
		rpc_createerr.cf_error.re_terrno = t_errno;
		return ((CLIENT *)NULL);
	}

	/* Get the address of the rpcbind */
	rpcbind_hs.h_host = (char *)host; /* XXX h_host should be const char **/
	rpcbind_hs.h_serv = "rpcbind";
#ifdef ND_DEBUG
	fprintf(stderr, "rpcbind client routines: diagnostics :\n");
	fprintf(stderr, "\tGetting address for (%s, %s, %s) ... \n",
		rpcbind_hs.h_host, rpcbind_hs.h_serv, nconf->nc_netid);
#endif

	if (netdir_getbyname(nconf, &rpcbind_hs, &nas)) {
		t_close(fd);
		rpc_createerr.cf_stat = RPC_N2AXLATEFAILURE;
		return ((CLIENT *)NULL);
	}

#ifdef ND_DEBUG
	{
		char *ua;

		ua = taddr2uaddr(nconf, nas->n_addrs);
		fprintf(stderr, "Got it [%s]\n", ua);
		free(ua);
	}
#endif

	addr = nas->n_addrs;

#ifdef ND_DEBUG
	{
		int i;

		fprintf(stderr, "\tnetbuf len = %d, maxlen = %d\n",
			addr->len, addr->maxlen);
		fprintf(stderr, "\tAddress is");
		for (i = 0; i < addr->len; i++)
			fprintf(stderr, "%u.", addr->buf[i]);
		fprintf(stderr, "\n");
	}
#endif
	client = clnt_tli_create(fd, nconf, addr, RPCBPROG,
				RPCBVERS, 0, 0);
	if (! client) {
#ifdef ND_DEBUG
		fprintf(stderr, "rpcbind clnt interface:");
		clnt_pcreateerror();
#endif
		t_close(fd);
	} else {
		/* close the fd when we destroy the handle */
		CLNT_CONTROL(client, CLSET_FD_CLOSE, NULL);
	}
	netdir_free((char *)nas, ND_ADDRLIST);
	return (client);
}

/*
 * This routine will return a client handle that is connected to the local
 * rpcbind. Returns NULL on error and free's everything. On success it
 * leaves an open fd.
 */
static CLIENT *
local_rpcb()
{
	CLIENT *client;
	struct netconfig *nconf;
	static char *local_netid;

	if (!local_netid) {
		NCONF_HANDLE *nc_handle;	/* Net config handle */
		char *tmpid = NULL;

		nc_handle = setnetconfig();
		if (nc_handle == NULL) {
			/* fails to open netconfig file */
			rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			return (NULL);
		}
		while (nconf = getnetconfig(nc_handle)) {
			if (strcmp(nconf->nc_protofmly,
				NC_LOOPBACK) == 0) {
				tmpid = nconf->nc_netid;
				if (nconf->nc_semantics == NC_TPI_CLTS)
					break;
			}
		}
		if (tmpid)
			local_netid = strdup(tmpid);
		endnetconfig(nc_handle);
	}

	if (!local_netid) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (NULL);
	}
	if (!(nconf = getnetconfigent(local_netid))) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (NULL);
	}
	client = getclnthandle(_rpc_gethostname(), nconf);
	freenetconfigent(nconf);
	return (client);
}
/*
 * Set a mapping between program, version and address.
 * Calls the rpcbind service remotely to do the mapping.
 */
bool_t
rpcb_set(
	u_long program,
	u_long version,
	struct netconfig *nconf,	/* Network structure of transport */
	struct netbuf *address)		/* Services netconfig address */
{
	register CLIENT *client;
	bool_t rslt = FALSE;
	RPCB parms;
	char uidbuf[32];

	/* parameter checking */
	if (nconf == (struct netconfig *)NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (FALSE);
	}
	if (address == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (FALSE);
	}
	client = local_rpcb();
	if (! client)
		return (FALSE);

	parms.r_addr = taddr2uaddr(nconf, address); /* convert to universal */
	if (!parms.r_addr) {
		rpc_createerr.cf_stat = RPC_N2AXLATEFAILURE;
		return (FALSE); /* no universal address */
	}
	parms.r_prog = program;
	parms.r_vers = version;
	parms.r_netid = nconf->nc_netid;
	/*
	 * Though uid is not being used directly, we still send it for
	 * completeness.  For non-unix platforms, perhaps some other
	 * string or an empty string can be sent.
	 */
	(void) sprintf(uidbuf, "%d", geteuid());
	parms.r_owner = uidbuf;

	CLNT_CALL(client, RPCBPROC_SET, xdr_rpcb, &parms,
			xdr_bool, &rslt, tottimeout);

	CLNT_DESTROY(client);
	free(parms.r_addr);
	return (rslt);
}

/*
 * Remove the mapping between program, version and netbuf address.
 * Calls the rpcbind service to do the un-mapping.
 * If netbuf is NULL, unset for all the transports, otherwise unset
 * only for the given transport.
 */
bool_t
rpcb_unset(
	u_long program,
	u_long version,
	struct netconfig *nconf)
{
	register CLIENT *client;
	bool_t rslt = FALSE;
	RPCB parms;
	char uidbuf[32];

	client = local_rpcb();

	if (! client)
		return (FALSE);

	parms.r_prog = program;
	parms.r_vers = version;
	if (nconf)
		parms.r_netid = nconf->nc_netid;
	else
		parms.r_netid = nullstring; /* unsets  all*/
	parms.r_addr = nullstring;
	(void) sprintf(uidbuf, "%d", geteuid());
	parms.r_owner = uidbuf;

	CLNT_CALL(client, RPCBPROC_UNSET, xdr_rpcb, &parms,
			xdr_bool, &rslt, tottimeout);

	CLNT_DESTROY(client);
	return (rslt);
}

/*
 * Find the mapped address for program, version.
 * Calls the rpcbind service remotely to do the lookup.
 * Uses the transport specified in nconf.
 * Returns FALSE (0) if no map exists, else returns 1.
 *
 * Assuming that the address is all properly allocated
 */
int
rpcb_getaddr(
	u_long program,
	u_long version,
	struct netconfig *nconf,
	struct netbuf *address,
	const char *host)
{
	register CLIENT *client;
	RPCB parms;
	bool_t status = FALSE;
	enum clnt_stat clnt_st;
	char *ua = NULL;

	/* parameter checking */
	if (nconf == (struct netconfig *)NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (FALSE);
	}
	if (address == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (FALSE);
	}
	client = getclnthandle(host, nconf);
	if (client == (CLIENT *)NULL)
		return (FALSE);

	parms.r_prog = program;
	parms.r_vers = version;
	parms.r_netid = nconf->nc_netid;	/* not needed */
	parms.r_addr = nullstring;	/* not needed; just for xdring */
	parms.r_owner = nullstring;	/* not needed; just for xdring */

	clnt_st = CLNT_CALL(client, RPCBPROC_GETADDR, xdr_rpcb, &parms,
				xdr_wrapstring, &ua, tottimeout);
	if (clnt_st == RPC_SUCCESS) {
		struct netbuf *na;

		if ((ua == NULL) || (ua[0] == NULL)) {
			/* address unknown */
			rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
			goto error;
		}
		na = uaddr2taddr(nconf, ua);
#ifdef ND_DEBUG
		fprintf(stderr, "\tRemote address is [%s].\n", ua);
		if (!na)
			fprintf(stderr, "\tCouldn't resolve remote address!\n");
#endif
		xdr_free(xdr_wrapstring, &ua);

		if (! na) {
			/* We don't know about your universal address */
			rpc_createerr.cf_stat = RPC_N2AXLATEFAILURE;
			goto error;
		}
		if (na->len > address->maxlen) {
			/* Too long address */
			netdir_free((char *)na, ND_ADDR);
			rpc_createerr.cf_stat = RPC_FAILED;
			goto error;
		}

		memcpy(address->buf, na->buf, (int)na->len);
		address->len = na->len;
		netdir_free((char *)na, ND_ADDR);
	} else if (clnt_st != RPC_SUCCESS) {
#ifdef PORTMAP
		/*
		 * rpcbind/portmapper do not return PROGVERSMISMATCH because
		 * of svc_versquiet, and hence this
		 */
		if (((clnt_st == RPC_PROGVERSMISMATCH) ||
			(clnt_st == RPC_PROGUNAVAIL)) &&
			(strcmp(nconf->nc_protofmly, NC_INET) == 0)) {
			/*
			 * version 3 not available. Try version 2
			 * The assumption here is that the netbuf
			 * is arranged in the sockaddr_in
			 * style for IP cases.
			 */
			u_short port;
			u_int protocol;
			struct netbuf remote;

			CLNT_CONTROL(client, CLGET_SVC_ADDR, &remote);
			protocol = strcmp(nconf->nc_proto, NC_TCP) ?
					IPPROTO_UDP : IPPROTO_TCP;
			port = (u_short)pmap_getport((struct sockaddr_in *)remote.buf,
					program, version, protocol);
			if (port == 0) {
				/* error number set by pmap_getport() itself */
				goto error;
			}
			port = htons(port);
			if (address->maxlen <= remote.len) {
				memcpy(address->buf, remote.buf,
						remote.len);
				memcpy((char *)&address->buf[sizeof (short)],
					(char *)&port, sizeof (short));
				address->len = remote.len;
			} else {
				/* XXX: No suitable error number here */
				rpc_createerr.cf_stat = RPC_PMAPFAILURE;
				clnt_geterr(client, &rpc_createerr.cf_error);
			}
		} else {
#endif
			rpc_createerr.cf_stat = clnt_st;
			clnt_geterr(client, &rpc_createerr.cf_error);
#ifndef PORTMAP
			goto error;
#else
		}
#endif /* PORTMAP */
	}
	if (address->len == 0) {
		rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		clnt_geterr(client, &rpc_createerr.cf_error);
	} else {
		status = TRUE;
	}
error:
	CLNT_DESTROY(client);
#ifdef ND_DEBUG
	if (status)
		fprintf(stderr, "\tSUCCESS.\n");
	else
		fprintf(stderr, "\tFAILED.\n");
#endif
	return (status);
}

/*
 * Get a copy of the current maps.
 * Calls the rpcbind service remotely to get the maps.
 *
 * It returns only a list of the services
 * It returns NULL on failure.
 */
RPCBLIST *
rpcb_getmaps(
	struct netconfig *nconf,
	const char *host)
{
	RPCBLIST *head = (RPCBLIST *)NULL;
	register CLIENT *client;

	client = getclnthandle(host, nconf);
	if (client != (CLIENT *)NULL) {
		if (CLNT_CALL(client, RPCBPROC_DUMP, xdr_void, NULL,
			xdr_rpcblist, &head, tottimeout) != RPC_SUCCESS) {
			rpc_createerr.cf_stat = RPC_RPCBFAILURE;
			clnt_geterr(client, &rpc_createerr.cf_error);
		}
		CLNT_DESTROY(client);
	}
	return (head);
}

/*
 * rpcbinder remote-call-service interface.
 * This routine is used to call the rpcbind remote call service
 * which will look up a service program in the address maps, and then
 * remotely call that routine with the given parameters. This allows
 * programs to do a lookup and call in one step.
*/
enum clnt_stat
rpcb_rmtcall(
	struct netconfig *nconf,	/* Netconfig structure */
	const char *host,		/* Remote host name */
	u_long prog, 			/* Remote proc identifiers */
	u_long vers, 			/* Remote proc identifiers */
	u_long proc,			/* Remote proc identifiers */
	xdrproc_t xdrargs, 		/* XDR routines */
	caddr_t argsp, 			/* Argument and Result */
	xdrproc_t xdrres,		/* XDR routines */
	caddr_t resp,			/* Argument and Result */
	struct timeval tout,		/* Timeout value for this call */
	struct netbuf *addr_ptr)	/* Preallocated netbuf address */
{
	register CLIENT *client;
	enum clnt_stat stat;
	struct rpcb_rmtcallargs a;
	struct rpcb_rmtcallres r;

	client = getclnthandle(host, nconf);
	if (client == (CLIENT *)NULL)
		return (RPC_FAILED);
	CLNT_CONTROL(client, CLSET_RETRY_TIMEOUT, &rmttimeout);
	a.prog = prog;
	a.vers = vers;
	a.proc = proc;
	a.args_ptr = argsp;
	a.xdr_args = xdrargs;
	r.addr_ptr = NULL;
	r.results_ptr = resp;
	r.xdr_results = xdrres;
	stat = CLNT_CALL(client, RPCBPROC_CALLIT,
			xdr_rpcb_rmtcallargs, &a,
			xdr_rpcb_rmtcallres, &r, tout);
	if ((stat == RPC_SUCCESS) && (addr_ptr != NULL)) {
		struct netbuf *na;

		na = uaddr2taddr(nconf, r.addr_ptr);
		if (! na) {
			stat = RPC_N2AXLATEFAILURE;
			goto error;
		}
		if (na->len > addr_ptr->maxlen) {
			/* Too long address */
			stat = RPC_FAILED; /* XXX A better error no */
			netdir_free((char *)na, ND_ADDR);
			goto error;
		}
		memcpy(addr_ptr->buf, na->buf, (int)na->len);
		addr_ptr->len = na->len;
		netdir_free((char *)na, ND_ADDR);
	}
error:
	CLNT_DESTROY(client);
	if (r.addr_ptr)
		xdr_free(xdr_wrapstring, &r.addr_ptr);
	return (stat);
}

/*
 * Gets the time on the remote host.
 * Returns 1 if succeeds else 0.
 */
bool_t
rpcb_gettime(
	const char *host,
	time_t *timep)
{
	CLIENT *client = NULL;
	void *handle;
	struct netconfig *nconf;

	if ((host == NULL) || (host[0] == NULL)) {
		time(timep);
		return (TRUE);
	}

	if ((handle = _rpc_setconf("netpath")) == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (FALSE);
	}
	rpc_createerr.cf_stat = RPC_SUCCESS;
	while (client == (CLIENT *)NULL) {
		if ((nconf = _rpc_getconf(handle)) == NULL) {
			if (rpc_createerr.cf_stat == RPC_SUCCESS)
				rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			break;
		}
		client = getclnthandle(host, nconf);
		if (client)
			break;
	}
	_rpc_endconf(handle);
	if (client) {
		enum clnt_stat st;

		st = CLNT_CALL(client, RPCBPROC_GETTIME, xdr_void, NULL,
				xdr_int, timep, tottimeout);
		CLNT_DESTROY(client);
		if (st == RPC_SUCCESS)
			return (TRUE);
	}
	return (FALSE);
}

/*
 * Converts taddr to universal address.  This routine should never
 * really be called because local n2a libraries are always provided.
 */
char *
rpcb_taddr2uaddr(
	struct netconfig *nconf,
	struct netbuf *taddr)
{
	CLIENT *client;
	char *uaddr = NULL;

	/* parameter checking */
	if (nconf == (struct netconfig *)NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (NULL);
	}
	if (taddr == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (NULL);
	}
	client = local_rpcb();
	if (! client)
		return (NULL);

	CLNT_CALL(client, RPCBPROC_TADDR2UADDR, xdr_netbuf, taddr,
			xdr_wrapstring, &uaddr, tottimeout);
	CLNT_DESTROY(client);
	return (uaddr);
}

/*
 * Converts universal address to netbuf.  This routine should never
 * really be called because local n2a libraries are always provided.
 */
struct netbuf *
rpcb_uaddr2taddr(
	struct netconfig *nconf,
	char *uaddr)
{
	CLIENT *client;
	struct netbuf nbuf;
	struct netbuf *taddr;

	/* parameter checking */
	if (nconf == (struct netconfig *)NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (NULL);
	}
	if (uaddr == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (NULL);
	}
	client = local_rpcb();
	if (! client)
		return (NULL);

	taddr = (struct netbuf *)calloc(1, sizeof (struct netbuf));
	if (taddr == NULL) {
		CLNT_DESTROY(client);
		return (NULL);
	}
	if (CLNT_CALL(client, RPCBPROC_UADDR2TADDR, xdr_wrapstring, &uaddr,
		xdr_netbuf, taddr, tottimeout) != RPC_SUCCESS) {
		free(taddr);
		taddr = NULL;
	}
	CLNT_DESTROY(client);
	return (taddr);
}
