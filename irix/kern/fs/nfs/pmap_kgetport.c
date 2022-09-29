/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.10 88/02/08 
 */


/*
 * pmap_kgetport.c
 * Kernel interface to pmap rpc service.
 */

#include "types.h"
#include <net/if.h>
#include <netinet/in.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/time.h>
#include "auth.h"
#include "clnt.h"
#include "pmap_prot.h"
#include "rpc_msg.h"
#include "xdr.h"

#define RETRIES 4

/*
 * Find the mapped port for program,version.
 * Calls the pmap service remotely to do the lookup.
 *
 * The 'address' argument is used to locate the portmapper, then
 * modified to contain the port number, if one was found.  If no
 * port number was found, 'address'->sin_port returns unchanged.
 *
 * Returns:	0	if port number successfully found for 'program'
 *		ENOENT	if 'program' was not registered
 *		errno	if there was an error contacting the portmapper
 */
int pmap_kgetport(struct sockaddr_in *, u_long, u_long, u_long);

extern int portmap_timeout;

int
pmap_kgetport(address, program, version, protocol)
	struct sockaddr_in *address;
	u_long program;
	u_long version;
	u_long protocol;
{
	struct sockaddr_in tmpaddr;
	register CLIENT *client;
	int error;
	struct pmap parms;
	u_short port;
	struct timeval tottimeout;

	/* copy 'address' so that it doesn't get trashed */
	tmpaddr = *address;
	tmpaddr.sin_port = htons(PMAPPORT);

	client = clntkudp_create(&tmpaddr, PMAPPROG, PMAPVERS, RETRIES,
				 KUDP_INTR, KUDP_XID_CREATE, get_current_cred());
	if (client == NULL) {
		return rpc_createerr.cf_error.re_errno;
	}

	error = 0;
	parms.pm_prog = program;
	parms.pm_vers = version;
	parms.pm_prot = protocol;
	parms.pm_port = 0;  /* not needed or used */
	port = 0;
	tottimeout.tv_sec = portmap_timeout / 10;
	tottimeout.tv_usec = 100000 * (portmap_timeout % 10);
	if (CLNT_CALL(client, PMAPPROC_GETPORT, xdr_pmap, &parms,
		      xdr_u_short, &port, tottimeout) == RPC_SUCCESS) {
		if (port != 0)
			address->sin_port = htons(port); /* save the port # */
		else
			error = ENOENT;	/* program not registered */
	} else {
		struct rpc_err rpcerr;

		CLNT_GETERR(client, &rpcerr);
		error = rpcerr.re_errno;
	}
	CLNT_DESTROY(client);
	return (error);
}

/*
 * getport_loop -- kernel interface to pmap_kgetport()
 *
 * Talks to the portmapper using the sockaddr_in supplied by 'address',
 * to lookup the specified 'program'.
 *
 * Modifies 'address'->sin_port by rewriting the port number, if one
 * was found.  If a port number was not found (ie, return value != 0),
 * then 'address'->sin_port is left unchanged.
 *
 * If the portmapper does not respond, prints console message (once).
 * If retry is true, retries forever, unless a signal is received.
 * If retry is false, only tries once.
 *
 * Returns:	0	the port number was successfully put into 'address'
 *		ENOENT	the requested process is not registered.
 *		errno	the portmapper did not respond and a signal occurred.
 */
int
getport_loop(address, program, version, protocol, retry)
	struct sockaddr_in *address;
	u_long program;
	u_long version;
	u_long protocol;
	int retry;
{
	int error;
	int printed = 0;

	/* sit in a tight loop until the portmapper responds */
	do {
		error = pmap_kgetport(address, program, version, protocol);
		if (!error || error == ENOENT)
			break;

		/* test to see if a signal has come in */
		if (error == EINTR) {
			return (error);
		}
		/* print this message only once */
		if (retry && (printed++ == 0)) {
			printf("Portmapper not responding; still trying\n");
		}
	} while(retry);				/* go try the portmapper again */

	/* got a response...print message if there was a delay */
	if (printed) {
		printf("Portmapper ok\n");
	}
	return (error);
}
