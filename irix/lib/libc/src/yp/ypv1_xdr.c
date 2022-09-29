/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.5 87/08/12
 */


/*
 * This contains the xdr functions needed by ypserv and the NIS
 * administrative tools to support the previous version of the NIS protocol.
 * Note that many "old" xdr functions are called, with the assumption that
 * they have not changed between the v1 protocol (which this module exists
 * to support) and the current v2 protocol.  
 */

#define NULL 0
#include "synonyms.h"
#include <rpc/rpc.h>
#include "yp_prot.h"
#include "ypv1_prot.h"
#include "ypclnt.h"
#include "yp_extern.h"

typedef struct xdr_discrim XDR_DISCRIM;

/*
 * xdr discriminant/xdr_routine vector for NIS requests.
 */
XDR_DISCRIM _yprequest_arms[] = {
	{(int) YPREQ_KEY, (xdrproc_t) xdr_ypreq_key},
	{(int) YPREQ_NOKEY, (xdrproc_t) xdr_ypreq_nokey},
	{(int) YPREQ_MAP_PARMS, (xdrproc_t) xdr_ypmap_parms},
	{__dontcare__, (xdrproc_t) NULL}
};

/*
 * Serializes/deserializes a yprequest structure.
 */
bool_t
_xdr_yprequest (XDR * xdrs, struct yprequest *ps)
{
	return(xdr_union(xdrs, (enum_t *)&ps->yp_reqtype, &ps->yp_reqbody,
	    _yprequest_arms, NULL) );
}


/*
 * xdr discriminant/xdr_routine vector for NIS responses
 */
XDR_DISCRIM _ypresponse_arms[] = {
	{(int) YPRESP_VAL, (xdrproc_t) xdr_ypresp_val},
	{(int) YPRESP_KEY_VAL, (xdrproc_t) xdr_ypresp_key_val},
	{(int) YPRESP_MAP_PARMS, (xdrproc_t) xdr_ypmap_parms},
	{__dontcare__, (xdrproc_t) NULL}
};

/*
 * Serializes/deserializes a ypresponse structure.
 */
bool_t
_xdr_ypresponse (XDR * xdrs, struct ypresponse *ps)
{
	return(xdr_union(xdrs, (enum_t *)&ps->yp_resptype, &ps->yp_respbody,
	    _ypresponse_arms, NULL) );
}

/*
 * Serializes/deserializes a ypbind_oldsetdom structure.
 */
bool_t
_xdr_ypbind_oldsetdom(XDR *xdrs, struct ypbind_setdom *ps)
{
	char *domain = ps->ypsetdom_domain;
	
	return(xdr_ypdomain_wrap_string(xdrs, &domain) &&
	    xdr_yp_binding(xdrs, &ps->ypsetdom_binding) );
}
