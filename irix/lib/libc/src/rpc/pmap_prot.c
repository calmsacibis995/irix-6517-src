/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.18 88/02/08 
 */


/*
 * pmap_prot.c
 * Protocol for the local binder service, or pmap.
 */

#ifdef _KERNEL
#include "types.h"
#include "xdr.h"
#include "pmap_prot.h"
#else
#ifdef __STDC__
	#pragma weak xdr_pmap = _xdr_pmap
#endif
#include "synonyms.h"
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/pmap_prot.h>
#endif 


bool_t
xdr_pmap(xdrs, regs)
	XDR *xdrs;
	struct pmap *regs;
{

	if (xdr_u_long(xdrs, &regs->pm_prog) && 
		xdr_u_long(xdrs, &regs->pm_vers) && 
		xdr_u_long(xdrs, &regs->pm_prot))
		return (xdr_u_long(xdrs, &regs->pm_port));
	return (FALSE);
}
