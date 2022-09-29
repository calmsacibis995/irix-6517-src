/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.14 87/08/12 
 */

/*
 * This contains xdr routines used by the NIS/rpc interface.
 */

#ifdef __STDC__
	#pragma weak xdr_datum = _xdr_datum
	#pragma weak xdr_ypdomain_wrap_string = _xdr_ypdomain_wrap_string
	#pragma weak xdr_ypmap_wrap_string = _xdr_ypmap_wrap_string
	#pragma weak xdr_ypreq_key = _xdr_ypreq_key
	#pragma weak xdr_ypreq_nokey = _xdr_ypreq_nokey
	#pragma weak xdr_ypresp_val = _xdr_ypresp_val
	#pragma weak xdr_ypresp_key_val = _xdr_ypresp_key_val
	#pragma weak xdr_yp_inaddr = _xdr_yp_inaddr
	#pragma weak xdr_yp_binding = _xdr_yp_binding
	#pragma weak xdr_ypbind_resp = _xdr_ypbind_resp
	#pragma weak xdr_ypowner_wrap_string = _xdr_ypowner_wrap_string
	#pragma weak xdr_ypmap_parms = _xdr_ypmap_parms
	#pragma weak ypbind_resp_arms = _ypbind_resp_arms
#endif
#include "synonyms.h"

#define NULL 0
#include <rpc/rpc.h>
#include "yp_prot.h"
#include "ypclnt.h"

typedef struct xdr_discrim XDR_DISCRIM;
bool_t xdr_yp_binding();
bool_t xdr_ypref();

/*
 * Serializes/deserializes a dbm datum data structure.
 */
bool_t
xdr_datum(xdrs, pdatum)
	XDR * xdrs;
	datum * pdatum;

{
	return (xdr_bytes(xdrs, &(pdatum->dptr), (u_int *)&(pdatum->dsize),
	    YPMAXRECORD));
}


/*
 * Serializes/deserializes a domain name string.  This is a "wrapper" for
 * xdr_string which knows about the maximum domain name size.  
 */
bool_t
xdr_ypdomain_wrap_string(xdrs, ppstring)
	XDR * xdrs;
	char **ppstring;
{
	return (xdr_string(xdrs, ppstring, YPMAXDOMAIN) );
}

/*
 * Serializes/deserializes a map name string.  This is a "wrapper" for
 * xdr_string which knows about the maximum map name size.  
 */
bool_t
xdr_ypmap_wrap_string(xdrs, ppstring)
	XDR * xdrs;
	char **ppstring;
{
	return (xdr_string(xdrs, ppstring, YPMAXMAP) );
}

/*
 * Serializes/deserializes a ypreq_key structure.
 */
bool_t
xdr_ypreq_key(xdrs, ps)
	XDR *xdrs;
	struct ypreq_key *ps;

{
	return (xdr_ypdomain_wrap_string(xdrs, &ps->domain) &&
	    xdr_ypmap_wrap_string(xdrs, &ps->map) &&
	    xdr_datum(xdrs, &ps->keydat) );
}

/*
 * Serializes/deserializes a ypreq_nokey structure.
 */
bool_t
xdr_ypreq_nokey(xdrs, ps)
	XDR * xdrs;
	struct ypreq_nokey *ps;
{
	return (xdr_ypdomain_wrap_string(xdrs, &ps->domain) &&
	    xdr_ypmap_wrap_string(xdrs, &ps->map) );
}

/*
 * Serializes/deserializes a ypresp_val structure.
 */

bool_t
xdr_ypresp_val(xdrs, ps)
	XDR * xdrs;
	struct ypresp_val *ps;
{
	return (xdr_u_long(xdrs, &ps->status) &&
	    xdr_datum(xdrs, &ps->valdat) );
}

/*
 * Serializes/deserializes a ypresp_key_val structure.
 */
bool_t
xdr_ypresp_key_val(xdrs, ps)
	XDR * xdrs;
	struct ypresp_key_val *ps;
{
	return (xdr_u_long(xdrs, &ps->status) &&
	    xdr_datum(xdrs, &ps->valdat) &&
	    xdr_datum(xdrs, &ps->keydat) );
}


/*
 * Serializes/deserializes an in_addr struct.
 * 
 * Note:  There is a data coupling between the "definition" of a struct
 * in_addr implicit in this xdr routine, and the true data definition in
 * <netinet/in.h>.  
 */
bool_t
xdr_yp_inaddr(xdrs, ps)
	XDR * xdrs;
	struct in_addr *ps;

{
	return (xdr_opaque(xdrs, &ps->s_addr, 4));
}

/*
 * Serializes/deserializes a ypbind_binding struct.
 */
bool_t
xdr_yp_binding(xdrs, ps)
	XDR * xdrs;
	struct ypbind_binding *ps;

{
	return (xdr_yp_inaddr(xdrs, &ps->ypbind_binding_addr) &&
            xdr_opaque(xdrs, &ps->ypbind_binding_port, 2));
}

/*
 * xdr discriminant/xdr_routine vector for NIS binder responses
 */
XDR_DISCRIM ypbind_resp_arms[] = {
	{(int) YPBIND_SUCC_VAL, (xdrproc_t) xdr_yp_binding},
	{(int) YPBIND_FAIL_VAL, (xdrproc_t) xdr_u_long},
	{__dontcare__, (xdrproc_t) NULL}
};

/*
 * Serializes/deserializes a ypbind_resp structure.
 */
bool_t
xdr_ypbind_resp(xdrs, ps)
	XDR * xdrs;
	struct ypbind_resp *ps;

{
	return (xdr_union(xdrs, (enum_t *)&ps->ypbind_status, &ps->ypbind_respbody,
	    ypbind_resp_arms, NULL) );
}

/*
 * Serializes/deserializes a peer server's node name
 */
bool_t
xdr_ypowner_wrap_string(xdrs, ppstring)
	XDR * xdrs;
	char **ppstring;

{
	return (xdr_string(xdrs, ppstring, YPMAXPEER) );
}

/*
 * Serializes/deserializes a ypmap_parms structure.
 */
bool_t
xdr_ypmap_parms(xdrs, ps)
	XDR *xdrs;
	struct ypmap_parms *ps;

{
	return (xdr_ypdomain_wrap_string(xdrs, &ps->domain) &&
	    xdr_ypmap_wrap_string(xdrs, &ps->map) &&
	    xdr_u_long(xdrs, &ps->ordernum) &&
	    xdr_ypowner_wrap_string(xdrs, &ps->owner) );
}
