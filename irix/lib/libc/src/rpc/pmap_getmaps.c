/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.11 88/02/08 
 */


/*
 * pmap_getmap.c
 * Client interface to pmap rpc service.
 * contains pmap_getmaps, which is only tcp service involved
 */

#ifdef __STDC__
	#pragma weak pmap_getmaps = _pmap_getmaps
#endif
#include "synonyms.h"

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <rpc/errorhandler.h>
#include <unistd.h>
#define NAMELEN 255
#define MAX_BROADCAST_SIZE 1400


/*
 * Get a copy of the current port maps.
 * Calls the pmap service remotely to do get the maps.
 */
struct pmaplist *
pmap_getmaps(address)
	 struct sockaddr_in *address;
{
	struct pmaplist *head = (struct pmaplist *)NULL;
	int socket = -1;
	struct timeval minutetimeout;
	register CLIENT *client;

	minutetimeout.tv_sec = 60;
	minutetimeout.tv_usec = 0;
	address->sin_port = htons(PMAPPORT);
	client = clnttcp_create(address, PMAPPROG,
	    PMAPVERS, &socket, 50, 500);
	if (client != (CLIENT *)NULL) {
		if (CLNT_CALL(client, PMAPPROC_DUMP, (xdrproc_t) xdr_void, NULL, 
			(xdrproc_t) xdr_pmaplist, &head, minutetimeout) != RPC_SUCCESS) {
			_rpc_errorhandler(LOG_ERR, "%s",
			    clnt_sperror(client, "pmap_getmaps: rpc problem")); 
		}
		CLNT_DESTROY(client);
	}
	(void)close(socket);
	address->sin_port = 0;
	return (head);
}
