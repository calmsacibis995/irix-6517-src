/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)librpc:rpc_generic.c	1.11.7.1"

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
#if defined(__STDC__) 
#ifndef _LIBNSL_ABI
        #pragma weak rpc_nullproc	= _rpc_nullproc
#endif /* _LIBNSL_ABI */
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

/*
 * rpc_generic.c, Miscl routines for RPC.
 *
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/nettype.h>
#include <sys/param.h>
#include <ctype.h>
#include <sys/resource.h>
#include <netconfig.h>
#include <malloc.h>

#ifndef MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN 64
#endif

static struct handle {
	NCONF_HANDLE *nhandle;
	int nflag;		/* Whether NETPATH or NETCONFIG */
	int nettype;
};

static struct _rpcnettype {
	char *name;
	int type;
} _rpctypelist[] = {
	"netpath", _RPC_NETPATH,
	"visible", _RPC_VISIBLE,
	"circuit_v", _RPC_CIRCUIT_V,
	"datagram_v", _RPC_DATAGRAM_V,
	"circuit_n", _RPC_CIRCUIT_N,
	"datagram_n", _RPC_DATAGRAM_N,
	"tcp", _RPC_TCP,
	"udp", _RPC_UDP,
	0, _RPC_NONE
};

int
_rpc_dtbsize( void )
{
	/*
	 * fd_set supports at most FD_SETSIZE of open
	 * file descriptors -- there's no sense in
	 * trying to support more here.
	 */
	return (FD_SETSIZE);
}

/*
 * Cache the result of gethostname(), so we don't have to do an
 * expensive system call every time.
 */
char *
_rpc_gethostname( void )
{
	static char *host;

	if (host == (char *)NULL) {
		host = (char *)calloc(1, MAXHOSTNAMELEN);
		if (host == (char *)NULL)
			return ((char *)NULL);
		gethostname(host, MAXHOSTNAMELEN);
	}
	return (host);
}

/*
 * Find the appropriate buffer size
 */
u_int
_rpc_get_t_size(
	int size,	/* Size requested */
	long bufsize)	/* Supported by the transport */
{
	if (bufsize == -2)
		/* transfer of data unsupported */
		return ((u_int)0);
	if (size == 0) {
		if ((bufsize == -1) || (bufsize == 0))
			/*
			 * bufsize == -1 : No limit on the size
			 * bufsize == 0 : Concept of tsdu foreign. Choose
			 *			a value.
			 */
			return ((u_int)MAXTR_BSIZE);
		else
			return ((u_int)bufsize);
	}
	if ((bufsize == -1) || (bufsize == 0))
		return ((u_int)size);
	/* Check whether the value is within the upper max limit */
	return (size > bufsize ? (u_int)bufsize : (u_int)size);
}

/*
 * Find the appropriate address buffer size
 */
u_int
_rpc_get_a_size(
	long size)	/* normally tinfo.addr */
{
	if (size >= 0)
		return ((u_int)size);
	if (size <= -2)
		return ((u_int)0);
	/*
	 * (size == -1) No limit on the size. we impose a limit here.
	 */
	return ((u_int)MAXTR_BSIZE);
}

/*
 * Returns the type of the network as defined in <rpc/nettype.h>
 * If nettype is NULL, it defaults to NETPATH.
 */
static int
getnettype( const char *nettype)
{
	int i;

	if ((nettype == NULL) || (nettype[0] == NULL))
		return (_RPC_NETPATH);	/* Default */

	for (i = 0; _rpctypelist[i].name; i++)
		if (strcasecmp(nettype, _rpctypelist[i].name) == 0)
			return (_rpctypelist[i].type);
	return (_rpctypelist[i].type);
}

/*
 * For the given nettype (tcp or udp only), return the first structure found.
 * This should be freed by calling freenetconfigent()
 */
struct netconfig *
_rpc_getconfip(nettype)
	char *nettype;
{
	char *netid;
	static char *netid_tcp;
	static char *netid_udp;

	if (!netid_udp && !netid_tcp) {
		struct netconfig *nconf;
		extern char *strdup();
		void *confighandle;

		if (!(confighandle = setnetconfig()))
			return (NULL);
		while (nconf = getnetconfig(confighandle)) {
			if (strcmp(nconf->nc_protofmly, NC_INET) == 0) {
				if (strcmp(nconf->nc_proto, NC_TCP) == 0)
					netid_tcp = strdup(nconf->nc_netid);
				else if (strcmp(nconf->nc_proto, NC_UDP) == 0)
					netid_udp = strdup(nconf->nc_netid);
			}
		}
		endnetconfig(confighandle);
	}
	if (strcmp(nettype, "udp") == 0)
		netid = netid_udp;
	else if (strcmp(nettype, "tcp") == 0)
		netid = netid_tcp;
	else
		return ((struct netconfig *)NULL);
	if ((netid == NULL) || (netid[0] == NULL))
		return ((struct netconfig *)NULL);
	return (getnetconfigent(netid));
}


/*
 * Returns the type of the nettype, which should then be used with
 * _rpc_getconf().
 */
void *
_rpc_setconf( const char *nettype)
{
	struct handle *handle;

	handle = (struct handle *) malloc(sizeof (struct handle));
	if (handle == NULL)
		return (NULL);
	switch (handle->nettype = getnettype(nettype)) {
	case _RPC_NETPATH:
	case _RPC_CIRCUIT_N:
	case _RPC_DATAGRAM_N:
		if (!(handle->nhandle = setnetpath())) {
			free(handle);
			return (NULL);
		}
		handle->nflag = TRUE;
		break;
	case _RPC_VISIBLE:
	case _RPC_CIRCUIT_V:
	case _RPC_DATAGRAM_V:
	case _RPC_TCP:
	case _RPC_UDP:
		if (!(handle->nhandle = setnetconfig())) {
			free(handle);
			return (NULL);
		}
		handle->nflag = FALSE;
		break;
	default:
		return (NULL);
	}

	return (handle);
}

/*
 * Returns the next netconfig struct for the given "net" type.
 * _rpc_setconf() should have been called previously.
 */
struct netconfig *
_rpc_getconf(handle)
	struct handle *handle;
{
	struct netconfig *nconf;

	if (handle == NULL)
		return (NULL);
	while (1) {
		if (handle->nflag)
			nconf = getnetpath(handle->nhandle);
		else
			nconf = getnetconfig(handle->nhandle);
		if (nconf == (struct netconfig *)NULL)
			break;
		if ((nconf->nc_semantics != NC_TPI_CLTS) &&
			(nconf->nc_semantics != NC_TPI_COTS) &&
			(nconf->nc_semantics != NC_TPI_COTS_ORD))
			continue;
		switch (handle->nettype) {
		case _RPC_VISIBLE:
			if (!(nconf->nc_flag & NC_VISIBLE))
				continue;
			/* falls through */
		case _RPC_NETPATH:	/* Be happy */
			break;
		case _RPC_CIRCUIT_V:
			if (!(nconf->nc_flag & NC_VISIBLE))
				continue;
			/* falls through */
		case _RPC_CIRCUIT_N:
			if ((nconf->nc_semantics != NC_TPI_COTS) &&
				(nconf->nc_semantics != NC_TPI_COTS_ORD))
				continue;
			break;
		case _RPC_DATAGRAM_V:
			if (!(nconf->nc_flag & NC_VISIBLE))
				continue;
			/* falls through */
		case _RPC_DATAGRAM_N:
			if (nconf->nc_semantics != NC_TPI_CLTS)
				continue;
			break;
		case _RPC_TCP:
			if (((nconf->nc_semantics != NC_TPI_COTS) &&
				(nconf->nc_semantics != NC_TPI_COTS_ORD)) ||
				strcmp(nconf->nc_protofmly, NC_INET) ||
				strcmp(nconf->nc_proto, NC_TCP))
				continue;
			break;
		case _RPC_UDP:
			if ((nconf->nc_semantics != NC_TPI_CLTS) ||
				strcmp(nconf->nc_protofmly, NC_INET) ||
				strcmp(nconf->nc_proto, NC_UDP))
				continue;
			break;
		}
		break;
	}
	return (nconf);
}

void
_rpc_endconf(handle)
	struct handle *handle;
{
	if (handle == NULL)
		return;
	if (handle->nflag) {
		endnetpath(handle->nhandle);
	} else {
		endnetconfig(handle->nhandle);
	}
	free(handle);
}

/*
 * Used to ping the NULL procedure for clnt handle.
 * Returns NULL if fails, else a non-NULL pointer.
 */
void *
rpc_nullproc(clnt)
	CLIENT *clnt;
{
	static char res;
	struct timeval TIMEOUT = {25, 0};

	if (clnt_call(clnt, NULLPROC, xdr_void, (char *)NULL,
		xdr_void, (char *)NULL, TIMEOUT) != RPC_SUCCESS)
		return (NULL);
	return ((void *)&res);
}
