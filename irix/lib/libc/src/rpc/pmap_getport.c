/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.10 88/02/08 
 */


/*
 * pmap_getport.c
 * Client interface to pmap rpc service.
 */

#ifdef __STDC__
	#pragma weak pmap_getport = _pmap_getport
#endif
#include "synonyms.h"

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <unistd.h>
#include "rpc_extern.h"

#define	timeout		_pmap_intertry_timeout
#define	tottimeout	_pmap_percall_timeout

/*
 * Find the mapped port for program,version.
 * Calls the pmap service remotely to do the lookup.
 * Returns 0 if no map exists.
 */
u_short
pmap_getport(struct sockaddr_in *address,
	u_long program,
	u_long version,
	u_int protocol)
{
	u_short port = 0;
	int socket = -1;
	register CLIENT *client;
	struct pmap parms;

	address->sin_port = htons(PMAPPORT);
	client = clntudp_bufcreate(address, PMAPPROG,
	    PMAPVERS, timeout, &socket, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client != (CLIENT *)NULL) {
		parms.pm_prog = program;
		parms.pm_vers = version;
		parms.pm_prot = protocol;
		parms.pm_port = 0;  /* not needed or used */
		if (CLNT_CALL(client, PMAPPROC_GETPORT, (xdrproc_t) xdr_pmap, &parms,
		    (xdrproc_t) xdr_u_short, &port, tottimeout) != RPC_SUCCESS){
			rpc_createerr.cf_stat = RPC_PMAPFAILURE;
			clnt_geterr(client, &rpc_createerr.cf_error);
		} else if (port == 0) {
			rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		}
		CLNT_DESTROY(client);
	}
	/* already closed by CLNT_DESTROY */
	/* (void)close(socket); */
	address->sin_port = 0;
	return (port);
}
