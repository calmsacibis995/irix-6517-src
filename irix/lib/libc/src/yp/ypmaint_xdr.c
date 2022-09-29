/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.3 87/08/12 
 */


/*
 * This contains xdr routines used by the NIS/rpc interface
 * for systems and maintenance programs only.  This is a separate module
 * because most NIS clients should not need to link to it.
 */

#ifdef __STDC__
	#pragma weak xdr_ypresp_master = _xdr_ypresp_master
	#pragma weak xdr_ypresp_order = _xdr_ypresp_order
	#pragma weak xdr_ypmaplist_wrap_string = _xdr_ypmaplist_wrap_string
	#pragma weak xdr_ypmaplist = _xdr_ypmaplist
	#pragma weak xdr_ypresp_maplist = _xdr_ypresp_maplist
	#pragma weak xdr_yppushresp_xfr = _xdr_yppushresp_xfr
	#pragma weak xdr_ypreq_xfr = _xdr_ypreq_xfr
	#pragma weak xdr_ypall = _xdr_ypall
	#pragma weak xdr_ypbind_setdom = _xdr_ypbind_setdom
#endif
#include "synonyms.h"

#define NULL 0
#include <rpc/rpc.h>
#include "yp_prot.h"
#include "ypclnt.h"
#include "yp_extern.h"

bool_t xdr_ypmaplist(XDR *, struct ypmaplist **);

/*
 * Serializes/deserializes a ypresp_master structure.
 */
bool_t
xdr_ypresp_master(XDR * xdrs, struct ypresp_master *ps)
{
	return (xdr_u_long(xdrs, &ps->status) &&
	     xdr_ypowner_wrap_string(xdrs, &ps->master) );
}

/*
 * Serializes/deserializes a ypresp_order structure.
 */
bool_t
xdr_ypresp_order(XDR * xdrs, struct ypresp_order *ps)
{
	return (xdr_u_long(xdrs, &ps->status) &&
	     xdr_u_long(xdrs, &ps->ordernum) );
}

/*
 * This is like xdr_ypmap_wrap_string except that it serializes/deserializes
 * an array, instead of a pointer, so xdr_reference can work on the structure
 * containing the char array itself.
 */
bool_t
xdr_ypmaplist_wrap_string(XDR * xdrs, char *pstring)
{
	char *s;

	s = pstring;
	return (xdr_string(xdrs, &s, YPMAXMAP) );
}

/*
 * Serializes/deserializes a ypmaplist.
 */
bool_t
xdr_ypmaplist(XDR *xdrs, struct ypmaplist **lst)
{
	bool_t more_elements;
	int freeing = (xdrs->x_op == XDR_FREE);
	struct ypmaplist **next;

	for(;;) {
		more_elements = (*lst != (struct ypmaplist *) NULL);
		
		if (! xdr_bool(xdrs, &more_elements))
			return (FALSE);
			
		if (! more_elements)
			return (TRUE);  /* All done */
			
		if (freeing)
			next = &((*lst)->ypml_next);

		if (! xdr_reference(xdrs, (caddr_t *)lst, sizeof(struct ypmaplist),
		    (xdrproc_t) xdr_ypmaplist_wrap_string))
			return (FALSE);
			
		lst = (freeing) ? next : &((*lst)->ypml_next);
	}
}


/*
 * Serializes/deserializes a ypresp_maplist.
 */
bool_t
xdr_ypresp_maplist(XDR * xdrs, struct ypresp_maplist *ps)
{
	return (xdr_u_long(xdrs, &ps->status) &&
	   xdr_ypmaplist(xdrs, &ps->list) );
}

/*
 * Serializes/deserializes a yppushresp_xfr structure.
 */
bool_t
xdr_yppushresp_xfr(XDR *xdrs, struct yppushresp_xfr *ps)
{
	return (xdr_u_long(xdrs, &ps->transid) &&
	    xdr_u_long(xdrs, &ps->status));
}


/*
 * Serializes/deserializes a ypreq_xfr structure.
 */
bool_t
xdr_ypreq_xfr(XDR * xdrs, struct ypreq_xfr *ps)
{
	return (xdr_ypmap_parms(xdrs, &ps->map_parms) &&
	    xdr_u_long(xdrs, &ps->transid) &&
	    xdr_u_long(xdrs, &ps->proto) &&
	    xdr_u_short(xdrs, &ps->port) );
}


/*
 * Serializes/deserializes a stream of struct ypresp_key_val's.  This is used
 * only by the client side of the batch enumerate operation.
 */
bool_t
xdr_ypall(XDR * xdrs, struct ypall_callback *callback)
{
	bool_t more;
	struct ypresp_key_val kv;
	bool_t s;
	char keybuf[YPMAXRECORD];
	char valbuf[YPMAXRECORD];

	if (xdrs->x_op == XDR_ENCODE)
		return(FALSE);

	if (xdrs->x_op == XDR_FREE)
		return(TRUE);

	kv.keydat.dptr = keybuf;
	kv.valdat.dptr = valbuf;
	kv.keydat.dsize = YPMAXRECORD;
	kv.valdat.dsize = YPMAXRECORD;
	
	for (;;) {
		if (! xdr_bool(xdrs, &more) )
			return (FALSE);
			
		if (! more)
			return (TRUE);

		s = xdr_ypresp_key_val(xdrs, &kv);
		
		if (s) {
			s = (*callback->foreach)((int)kv.status, kv.keydat.dptr,
			    kv.keydat.dsize, kv.valdat.dptr, kv.valdat.dsize,
			    callback->data);
			
			if (s)
				return (TRUE);
		} else {
			return (FALSE);
		}
	}
}


/*
 * Serializes/deserializes a ypbind_setdom structure.
 */
bool_t
xdr_ypbind_setdom(XDR *xdrs, struct ypbind_setdom *ps)
{
	char *domain = ps->ypsetdom_domain;
	
	return (xdr_ypdomain_wrap_string(xdrs, &domain) &&
	    xdr_yp_binding(xdrs, &ps->ypsetdom_binding) &&
	    xdr_u_short(xdrs, &ps->ypsetdom_vers));
}
