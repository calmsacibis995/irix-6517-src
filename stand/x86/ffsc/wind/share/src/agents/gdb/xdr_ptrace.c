/* xdr_ptrace.c  - xdr routines for remote ptrace calls */
 
/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01f,10mar93,maf  changed #include <> to #include "" for consistency.
01e,07sep92,maf  replaced NULL with 0, to work around missing definition in
		   HP-UX RPC headers.  This is acceptable practice under ANSI,
		   which provides a special semantic for 0 in a pointer context.
01d,06sep92,maf  removed include of "vxWorks.h"
                 changed "LOCAL" to "static."
01c,29aug92,maf  references to errno member of Ptrace_return changed to
		   errno_val, since errno is now a macro in VxWorks 5.1.
01b,25may91,maf  now uses counted bytes struct to transfer registers.
		 removed xdr_regs_ptr (), xdr_fp_status_ptr (), and
		   xdr_fpa_regs_ptr ().
01a,05jun90,llk  extracted from xdr_ptrace.h, version 01c.
*/

#include "rpc/rpc.h"
#include "xdr_ptrace.h"

#define MAX_LEN 32000

/********************************************************************
*
* xdr_c_bytes -
*
* xdr routine for counted bytes  
*
*/
bool_t xdr_c_bytes(xdrs,objp)
    XDR *xdrs;
    C_bytes *objp;
    {
    return(xdr_bytes(xdrs, &objp->bytes, (u_int *) &objp->len, MAX_LEN));
    } /* xdr_c_bytes */

/********************************************************************
*
* xdr_c_bytes_ptr -
*
* xdr routine for counted bytes branch of ptrace_info
*
*/

static bool_t xdr_c_bytes_ptr(xdrs,objp)
    XDR *xdrs;
    C_bytes **objp;
    {
    return(xdr_pointer(xdrs, (char **) objp, sizeof(C_bytes), xdr_c_bytes));
    } /* xdr_c_bytes_ptr */

/********************************************************************
*
* xdr_ptrace_info -
*
* xdr routine for discriminated union ptrace_info
*
*/

bool_t xdr_ptrace_info(xdrs,objp)
    XDR *xdrs;
    Ptrace_info *objp;
    {
    static struct xdr_discrim choices[] = 
	{
	    { (int) DATA, xdr_c_bytes_ptr },
	    { __dontcare__, 0 }
	};

    return(xdr_union(xdrs, (enum_t *) &objp->ttype, 
	(char *) &objp->more_data, choices, xdr_void));
    } /* xdr_ptrace_info */

/********************************************************************
*
* xdr_rptrace - 
*
* xdr routine for remote ptrace data into server
*
*/

bool_t xdr_rptrace(xdrs,objp)
    XDR *xdrs;
    Rptrace *objp;
    {
    if (! xdr_int(xdrs, &objp->pid)) 
	return(FALSE);
    if (! xdr_int(xdrs, &objp->data)) 
	return(FALSE);
    if (! xdr_int(xdrs, &objp->addr)) 
	return(FALSE);
    if (! xdr_ptrace_info(xdrs, &objp->info)) 
	return(FALSE);

    return(TRUE);
    } /* xdr_rptrace */

/********************************************************************
*
* xdr_ptrace_return - 
*
* xdr routine for remote ptrace data returned by server
*
*/

bool_t xdr_ptrace_return(xdrs, objp)
    XDR *xdrs;
    Ptrace_return *objp;
    {
    if (! xdr_int(xdrs, &objp->status)) 
	return(FALSE);
    if (! xdr_int(xdrs, &objp->errno_val)) 
	return(FALSE);
    if (! xdr_ptrace_info(xdrs, &objp->info)) 
	return(FALSE);

    return(TRUE);
    } /* xdr_ptrace_return */	
