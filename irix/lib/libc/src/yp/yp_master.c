/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#ifdef __STDC__
	#pragma weak yp_master = _yp_master
#endif
#include "synonyms.h"

#define NULL 0
#include <sys/time.h>
#include <rpc/rpc.h>
#include <string.h>		/* prototype for strlen() */
#include <unistd.h>		/* prototype for sleep() */
#include "yp_prot.h"
#include "ypclnt.h"
#include "yp_extern.h"

static int v2domaster(const char *, const char *, struct dom_binding *,
		struct timeval, char **);

/*
 * This checks parameters, and implements the outer "until binding success"
 * loop.
 */
int
yp_master (const char *domain, const char *map, char **master)
{
	int domlen;
	int maplen;
	int reason;
	struct dom_binding *pdomb;

	if ( (map == NULL) || (domain == NULL) ) {
		return (YPERR_BADARGS);
	}
	
	domlen = (int)strlen(domain);
	maplen = (int)strlen(map);
	
	if ( (domlen == 0) || (domlen > (int)YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > (int)YPMAXMAP) ||
	    (master == NULL) ) {
		return (YPERR_BADARGS);
	}

	for (;;) {
		
		if (reason = _yp_dobind(domain, &pdomb) ) {
			return (reason);
		}

		reason = v2domaster(domain, map, pdomb, _ypserv_timeout,
		    master);

		if (reason == YPERR_RPC) {
			yp_unbind(domain);
			(void) sleep(_ypsleeptime);
		} else {
			break;
		}
	}
	
	return (reason);
}

/*
 * This talks v2 to ypserv
 */
static int
v2domaster (const char *domain, 
	const char *map,
	struct dom_binding *pdomb,
	struct timeval timeout,
	char **master)
{
	struct ypreq_nokey req;
	struct ypresp_master resp;
	unsigned int retval = 0;

	req.domain = (char *)domain;
	req.map = (char *)map;
	resp.master = NULL;

	/*
	 * Do the get_master request.  If the rpc call failed, return with
	 * status from this point.  
	 */
	
	if(clnt_call(pdomb->dom_client, YPPROC_MASTER, (xdrproc_t) xdr_ypreq_nokey, &req,
	    (xdrproc_t) xdr_ypresp_master, &resp, timeout) != RPC_SUCCESS) {
		return (YPERR_RPC);
	}

	/* See if the request succeeded */
	
	if (resp.status != YP_TRUE) {
		retval = (unsigned int)ypprot_err((unsigned) resp.status);
	}

	/* Get some memory which the user can get rid of as he likes */

	if (!retval && ((*master = malloc(
	    (unsigned) strlen(resp.master) + 1)) == NULL)) {
		retval = YPERR_RESRC;

	}

	if (!retval) {
		strcpy(*master, resp.master);
	}
	
	CLNT_FREERES(pdomb->dom_client, (xdrproc_t) xdr_ypresp_master, &resp);
	return ((int)retval);
}
