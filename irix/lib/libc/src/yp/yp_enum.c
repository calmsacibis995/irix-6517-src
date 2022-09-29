/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.8 87/08/12 
 */

#ifdef __STDC__
	#pragma weak yp_first = _yp_first
	#pragma weak yp_next = _yp_next
#endif
#include "synonyms.h"

#include <sys/time.h>
#include <rpc/rpc.h>
#include <string.h>		/* prototype for strlen() */
#include <bstring.h>		/* prototype for bcopy() */
#include <unistd.h>		/* prototype for sleep() */
#include "yp_prot.h"
#include "ypclnt.h"
#include "yp_extern.h"

#define ypsymbol_prefix "YP_"
#define ypsymbol_prefix_length 3

static int v2dofirst(const char *, const char *, struct dom_binding *, struct timeval ,
			char **, int  *, char **, int  *);
static int v2donext(const char *, const char *,const char *, int, struct dom_binding *, 
			struct timeval , char **, int  *, char **, int  *);

/*
 * This requests the NIS server associated with a given domain to return the
 * first key/value pair from the map data base.  The returned key should be
 * used as an input to the call to ypclnt_next.  This part does the parameter
 * checking, and the do-until-success loop.
 */
int
yp_first (const char *domain,
	const char *map,
	char **key,		/* return: key array */
	int  *keylen,		/* return: bytes in key */
	char **val,		/* return: value array */
	int  *vallen)		/* return: bytes in val */
{
	int domlen;
	int maplen;
	struct dom_binding *pdomb;
	int reason;

	if ( (map == NULL) || (domain == NULL) ) {
		return (YPERR_BADARGS);
	}
	
	domlen = (int)strlen(domain);
	maplen = (int)strlen(map);
	
	if ( (domlen == 0) || (domlen > (int)YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > (int)YPMAXMAP) ) {
		return (YPERR_BADARGS);
	}

	for (;;) {
		
		if (reason = _yp_dobind(domain, &pdomb) ) {
			return (reason);
		}

		reason = v2dofirst(domain, map, pdomb, _ypserv_timeout,
		    key, keylen, val, vallen);

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
 * This part of the "get first" interface talks to ypserv.
 */

static int
v2dofirst (const char *domain,
	const char *map,
	struct dom_binding *pdomb,
	struct timeval timeout,
	char **key,
	int  *keylen,
	char **val,
	int  *vallen)
{
	struct ypreq_nokey req;
	struct ypresp_key_val resp;
	unsigned int retval = 0;

	req.domain = (char *)domain;
	req.map = (char *)map;
	resp.keydat.dptr = resp.valdat.dptr = NULL;
	resp.keydat.dsize = resp.valdat.dsize = 0;

	/*
	 * Do the get first request.  If the rpc call failed, return with status
	 * from this point.
	 */
	
	if(clnt_call(pdomb->dom_client, YPPROC_FIRST, (xdrproc_t) xdr_ypreq_nokey,
	    &req, (xdrproc_t) xdr_ypresp_key_val, &resp, timeout) != RPC_SUCCESS) {
		return (YPERR_RPC);
	}

	/* See if the request succeeded */
	
	if (resp.status != YP_TRUE) {
		retval = (unsigned int)ypprot_err((unsigned) resp.status);
	}

	/* Get some memory which the user can get rid of as he likes */

	if (!retval) {

		if ((*key =
		    (char *) malloc((unsigned)
		        resp.keydat.dsize + 2)) != NULL) {

			if ((*val = (char *) malloc(
			    (unsigned) resp.valdat.dsize + 2) ) == NULL) {
				free((char *) *key);
				retval = YPERR_RESRC;
			}
		
		} else {
			retval = YPERR_RESRC;
		}
	}

	/* Copy the returned key and value byte strings into the new memory */

	if (!retval) {
		*keylen = resp.keydat.dsize;
		bcopy(resp.keydat.dptr, *key, resp.keydat.dsize);
		(*key)[resp.keydat.dsize] = '\n';
		(*key)[resp.keydat.dsize + 1] = '\0';
		
		*vallen = resp.valdat.dsize;
		bcopy(resp.valdat.dptr, *val, resp.valdat.dsize);
		(*val)[resp.valdat.dsize] = '\n';
		(*val)[resp.valdat.dsize + 1] = '\0';
	}
	
	CLNT_FREERES(pdomb->dom_client, (xdrproc_t) xdr_ypresp_key_val, &resp); 
	return ((int)retval);
}

/*
 * This requests the NIS server associated with a given domain to return the
 * "next" key/value pair from the map data base.  The input key should be
 * one returned by ypclnt_first or a previous call to ypclnt_next.  The
 * returned key should be used as an input to the next call to ypclnt_next.
 * This part does the parameter checking, and the do-until-success loop.
 */
int
yp_next (const char *domain,
	const char *map,
	const char *inkey,
	int  inkeylen,
	char **outkey,		/* return: key array associated with val */
	int  *outkeylen,	/* return: bytes in key */
	char **val,		/* return: value array associated with outkey */
	int  *vallen)		/* return: bytes in val */
{
	int domlen;
	int maplen;
	struct dom_binding *pdomb;
	int reason;


	if ( (map == NULL) || (domain == NULL) || (inkey == NULL) ) {
		return(YPERR_BADARGS);
	}
	
	domlen = (int)strlen(domain);
	maplen = (int)strlen(map);
	
	if ( (domlen == 0) || (domlen > (int)YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > (int)YPMAXMAP) ) {
		return(YPERR_BADARGS);
	}

	for (;;) {
		if (reason = _yp_dobind(domain, &pdomb) ) {
			return(reason);
		}

		reason = v2donext(domain, map, inkey, inkeylen, pdomb,
		    _ypserv_timeout, outkey, outkeylen, val, vallen);

		if (reason == YPERR_RPC) {
			yp_unbind(domain);
			(void) sleep(_ypsleeptime);
		} else {
			break;
		}
	}
	
	return(reason);
}

/*
 * This part of the "get next" interface talks to ypserv.
 */
static int
v2donext (const char *domain,
	const char *map,
	const char *inkey,
	int  inkeylen,
	struct dom_binding *pdomb,
	struct timeval timeout,
	char **outkey,		/* return: key array associated with val */
	int  *outkeylen,	/* return: bytes in key */
	char **val,		/* return: value array associated with outkey */
	int  *vallen)		/* return: bytes in val */
{
	struct ypreq_key req;
	struct ypresp_key_val resp;
	unsigned int retval = 0;

	req.domain = (char *)domain;
	req.map = (char *)map;
	req.keydat.dptr = (char *)inkey;
	req.keydat.dsize = inkeylen;
	
	resp.keydat.dptr = resp.valdat.dptr = NULL;
	resp.keydat.dsize = resp.valdat.dsize = 0;

	/*
	 * Do the get next request.  If the rpc call failed, return with status
	 * from this point.
	 */
	
	if(clnt_call(pdomb->dom_client,
	    YPPROC_NEXT, (xdrproc_t) xdr_ypreq_key, &req, (xdrproc_t) xdr_ypresp_key_val, &resp,
	    timeout) != RPC_SUCCESS) {
		return(YPERR_RPC);
	}

	/* See if the request succeeded */
	
	if (resp.status != YP_TRUE) {
		retval = (unsigned int)ypprot_err((unsigned) resp.status);
	}

	/* Get some memory which the user can get rid of as he likes */

	if (!retval) {
		if ( (*outkey = (char *) malloc((unsigned)
		    resp.keydat.dsize + 2) ) != NULL) {

			if ( (*val = (char *) malloc((unsigned)
			    resp.valdat.dsize + 2) ) == NULL) {
				free((char *) *outkey);
				retval = YPERR_RESRC;
			}
		
		} else {
			retval = YPERR_RESRC;
		}
	}

	/* Copy the returned key and value byte strings into the new memory */

	if (!retval) {
		*outkeylen = resp.keydat.dsize;
		bcopy(resp.keydat.dptr, *outkey,
		    resp.keydat.dsize);
		(*outkey)[resp.keydat.dsize] = '\n';
		(*outkey)[resp.keydat.dsize + 1] = '\0';
		
		*vallen = resp.valdat.dsize;
		bcopy(resp.valdat.dptr, *val, resp.valdat.dsize);
		(*val)[resp.valdat.dsize] = '\n';
		(*val)[resp.valdat.dsize + 1] = '\0';
	}
	
	CLNT_FREERES(pdomb->dom_client, (xdrproc_t) xdr_ypresp_key_val, &resp);
	return((int)retval);
}
