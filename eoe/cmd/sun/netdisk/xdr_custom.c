#ifndef lint
static  char sccsid[] = "@(#)xdr_custom.c 1.7 88/02/08 Copyr 1987 Sun Micro";
#endif
/*
 * @(#)xdr_custom.c	1.1 88/06/08 4.0NFSSRC; from 1.6 88/03/01 D/NFS
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

/*
 *
 */

#include "toc.h"

bool_t
xdr_futureslop(xdrs, objp)
XDR *xdrs;
struct futureslop *objp;
{
#ifdef	lint
	objp = objp;
#endif
	if(xdrs->x_op == XDR_DECODE)
		return(xdrrec_skiprecord(xdrs));
	(void) fprintf(stderr,"error in xdr_futureslop\n");
	return(FALSE);
}

bool_t
xdr_nomedia(xdrs, objp)
XDR *xdrs;
struct nomedia *objp;
{
#ifdef	lint
	xdrs = xdrs;
	objp = objp;
#endif
	(void) fprintf(stderr,"xdr_nomedia- No such media supported\n");
	return(FALSE);
}
