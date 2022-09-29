/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.38 88/02/08 
 */


/*
 * pmap_clnt.c
 * Client interface to pmap rpc service.
 */

#ifdef __STDC__
	#pragma weak pmap_settimeouts = _pmap_settimeouts
	#pragma weak pmap_set = _pmap_set
	#pragma weak pmap_unset = _pmap_unset
#endif
#include "synonyms.h"

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <sys/time.h>
#include <rpc/errorhandler.h>
#include <unistd.h>		/* prototype for close() */
#include <sys/socket.h>
#include "rpc_extern.h"

struct timeval _pmap_intertry_timeout = { 5, 0 };
struct timeval _pmap_percall_timeout = { 60, 0 };
#define	timeout		_pmap_intertry_timeout
#define	tottimeout	_pmap_percall_timeout

void
pmap_settimeouts(struct timeval intertry, struct timeval percall)
{
	_pmap_intertry_timeout = intertry;
	_pmap_percall_timeout = percall;
}




/*
 * Set a mapping between program,version and port.
 * Calls the pmap service remotely to do the mapping.
 */
bool_t
pmap_set(u_long program, u_long version, u_int protocol, u_short port)
{
	struct sockaddr_in myaddress;
	int socket = RPC_ANYSOCK;
	register CLIENT *client;
	struct pmap parms;
	bool_t rslt;

	/* use the loopback address instead of calling get_myaddress */
	myaddress.sin_family = AF_INET;
	myaddress.sin_addr.s_addr = INADDR_LOOPBACK;
	myaddress.sin_port = htons(PMAPPORT);

	client = clntudp_bufcreate(&myaddress, PMAPPROG, PMAPVERS,
	    timeout, &socket, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client == (CLIENT *)NULL)
		return (FALSE);
	parms.pm_prog = program;
	parms.pm_vers = version;
	parms.pm_prot = protocol;
	parms.pm_port = port;
	if (CLNT_CALL(client, PMAPPROC_SET, (xdrproc_t) xdr_pmap, &parms, (xdrproc_t) xdr_bool, &rslt,
	    tottimeout) != RPC_SUCCESS) {
		_rpc_errorhandler(LOG_ERR, "%s",
		    clnt_sperror(client, "pmap_set: Cannot register service")); 
		rslt = FALSE;
	}
	CLNT_DESTROY(client);
	return (rslt);
}

/*
 * Remove the mapping between program,version and port.
 * Calls the pmap service remotely to do the un-mapping.
 */
bool_t
pmap_unset(u_long program, u_long version)
{
	struct sockaddr_in myaddress;
	int socket = -1;
	register CLIENT *client;
	struct pmap parms;
	bool_t rslt;

	/* use the loopback address instead of calling get_myaddress */
	myaddress.sin_family = AF_INET;
	myaddress.sin_addr.s_addr = INADDR_LOOPBACK;
	myaddress.sin_port = htons(PMAPPORT);

	client = clntudp_bufcreate(&myaddress, PMAPPROG, PMAPVERS,
	    timeout, &socket, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client == (CLIENT *)NULL)
		return (FALSE);
	parms.pm_prog = program;
	parms.pm_vers = version;
	parms.pm_port = parms.pm_prot = 0;
	CLNT_CALL(client, PMAPPROC_UNSET, (xdrproc_t) xdr_pmap, &parms, (xdrproc_t) xdr_bool, &rslt,
	    tottimeout);
	CLNT_DESTROY(client);
	(void)close(socket);
	return (rslt);
}
