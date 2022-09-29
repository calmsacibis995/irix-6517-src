#include <limits.h>
#include <rpc/rpc.h>
#include "argv.h"

bool_t
xdr_argstring(xdrs, objp)
	XDR *xdrs;
	argstring *objp;
{
	if (!xdr_string(xdrs, objp, ARG_MAX)) {
		return (FALSE);
	}
	return (TRUE);
}



bool_t
xdr_arguments(xdrs, objp)
	XDR *xdrs;
	arguments *objp;
{
	if (!xdr_array(xdrs, (char **)&objp->argv, (u_int *)&objp->argc, ARG_MAX, sizeof(argstring), xdr_argstring)) {
		return (FALSE);
	}
	return (TRUE);
}

