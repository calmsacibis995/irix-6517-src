/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */
#ifdef __STDC__
	#pragma weak yp_all = _yp_all
#endif
#include "synonyms.h"


#define NULL 0
#include <sys/time.h>
#include <rpc/rpc.h>
#include <string.h>		/* prototype for strlen() */
#include <unistd.h>		/* prototype for close() */
#include "yp_prot.h"
#include "ypclnt.h"
#include "yp_extern.h"


static const struct timeval tcp_timout = {
	120,				/* 120 seconds */
	0
	};

/*
 * This does the "glommed enumeration" stuff.  callback->foreach is the name
 * of a function which gets called per decoded key-value pair:
 * 
 * (*callback->foreach)(status, key, keylen, val, vallen, callback->data);
 *
 */
int
yp_all (const char *domain, 
	const char *map, 
	struct ypall_callback *callback)
{
	int domlen;
	int maplen;
	struct ypreq_nokey req;
	int reason;
	struct dom_binding *pdomb;
	struct sockaddr_in sin;
	int socket;
	CLIENT *client;
	enum clnt_stat s;

	if ( (map == NULL) || (domain == NULL) ) {
		return(YPERR_BADARGS);
	}
	
	domlen = (int)strlen(domain);
	maplen = (int)strlen(map);
	
	if ( (domlen == 0) || (domlen > (int)YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > (int)YPMAXMAP) ||
	    (callback == (struct ypall_callback *) NULL) ) {
		return(YPERR_BADARGS);
	}

	if (reason = _yp_dobind(domain, &pdomb) ) {
		return(reason);
	}

	sin = pdomb->dom_server_addr;
	sin.sin_port = 0;
	socket = RPC_ANYSOCK;
	client = clnttcp_create(&sin, YPPROG, YPVERS, &socket, 0, 0);
	if (client == NULL) {
		clnt_pcreateerror("yp_all - TCP channel create failure");
		return(YPERR_RPC);
	}

	clnt_destroy(pdomb->dom_client);
	(void) close(pdomb->dom_socket);
	pdomb->dom_client = client;
	pdomb->dom_socket = socket;

	req.domain = (char *)domain;
	req.map = (char *)map;
	
	s = clnt_call(pdomb->dom_client, YPPROC_ALL, (xdrproc_t)xdr_ypreq_nokey, &req,
		      (xdrproc_t) xdr_ypall, callback, tcp_timout);

	if (s != RPC_SUCCESS) {
		clnt_perror(pdomb->dom_client,
		    "yp_all - RPC clnt_call (TCP) failure");
	}

	if (s != RPC_SUCCESS) {
		return(YPERR_RPC);
	}
	return(0);
}
