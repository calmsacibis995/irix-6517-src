/* @(#)ypupdate_prot.x	1.2 88/03/31 4.0NFSSRC; from 1.4 88/02/08 Copyr 1986, Sun Micro */

/*
 * NIS update service protocol
 */

#ifdef __STDC__
	#pragma weak xdr_ypupdate_args = _xdr_ypupdate_args
	#pragma weak xdr_ypdelete_args = _xdr_ypdelete_args
	#pragma weak xdr_yp_buf = _xdr_yp_buf
#endif
#include "synonyms.h"

#include	"ypupdate_prot.h"
#include	<rpc/rpc.h>

int xdr_yp_buf(XDR *, struct yp_buf *);

int
xdr_ypupdate_args(xdrs, args)
XDR			*xdrs;
struct ypupdate_args	*args;
{
	if ( xdr_string(xdrs, &args->mapname, MAXMAPNAMELEN) )
		return ( xdr_yp_buf(xdrs, &args->key) &&
			xdr_yp_buf(xdrs, &args->datum) );
	return (FALSE);
}

int
xdr_ypdelete_args(xdrs, args)
XDR			*xdrs;
struct ypdelete_args	*args;
{
	if ( xdr_string(xdrs, &args->mapname, MAXMAPNAMELEN) )
		return ( xdr_yp_buf(xdrs, &args->key) );
	return (FALSE);
}

int
xdr_yp_buf(xdrs, args)
XDR		*xdrs;
struct yp_buf	*args;
{
	return ( xdr_int(xdrs, &args->yp_buf_len) &&
		xdr_bytes(xdrs, &args->yp_buf_val, (u_int *)&args->yp_buf_len,
			MAXYPDATALEN) );
}
