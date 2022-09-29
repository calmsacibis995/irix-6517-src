/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#ifdef __STDC__
	#pragma weak yp_order = _yp_order
#endif
#include "synonyms.h"

#define NULL 0
#include <sys/time.h>
#include <rpc/rpc.h>
#include <string.h>		/* prototype for strlen() */
#include <unistd.h>		/* prototype for sleeop() */
#include "yp_prot.h"
#include "ypclnt.h"
#include "yp_extern.h"

static int v2doorder(const char *, const char *, struct dom_binding *, 
			struct timeval, int *);

/*
 * This checks parameters, and implements the outer "until binding success"
 * loop.
 */
int
yp_order (const char *domain, const char *map, int *order)
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
	    (order == NULL) ) {
		return (YPERR_BADARGS);
	}

	for (;;) {
		
		if (reason = _yp_dobind(domain, &pdomb) ) {
			return (reason);
		}

		reason = v2doorder(domain, map, pdomb, _ypserv_timeout,
		    order);

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
v2doorder (const char *domain,
	const char *map,
	struct dom_binding *pdomb,
	struct timeval timeout,
	int *order)
{
	struct ypreq_nokey req;
	struct ypresp_order resp;
	unsigned int retval = 0;

	req.domain = (char *)domain;
	req.map = (char *)map;
	resp.ordernum = 0;

	/*
	 * Do the get_order request.  If the rpc call failed, return with
	 * status from this point.  
	 */
	
	if(clnt_call(pdomb->dom_client, YPPROC_ORDER, (xdrproc_t) xdr_ypreq_nokey, &req,
	    (xdrproc_t) xdr_ypresp_order, &resp, timeout) != RPC_SUCCESS) {
		return (YPERR_RPC);
	}

	/* See if the request succeeded */
	
	if (resp.status != YP_TRUE) {
		retval = (unsigned int)ypprot_err((unsigned) resp.status);
	}

	*order = (int) resp.ordernum;
	CLNT_FREERES(pdomb->dom_client, (xdrproc_t) xdr_ypresp_order, &resp);
	return ((int)retval);

}
