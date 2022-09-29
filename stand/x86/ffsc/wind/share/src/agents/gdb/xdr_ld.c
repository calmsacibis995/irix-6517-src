/* xdr_ld.c  - xdr routines for remote dbx interface to VxWorks  */

/* Copyright 1984,1985,1986,1987,1988,1989 Wind River Systems, Inc. */
/*extern char copyright_wind_river[]; static char *copyright=copyright_wind_river;*/

/*
modification history
--------------------
01e,07sep92,maf  replaced NULL with 0, to work around missing definition in
		   HP-UX RPC headers.  This is acceptable practice under ANSI,
		   which provides a special semantic for 0 in a pointer context.
01d,06sep92,maf  removed include of "vxWorks.h."
                 changed "UINT" to "u_int", as specified in RPC headers.
01c,29aug92,maf  minor cleanup.
01b,31jan92,maf  updated comments to xdr_String ().
01a,05jun90,llk  extracted from xdr_dbx.c.
*/

/*
DESCRIPTION
This module contains the eXternal Data Representation (XDR) routines
for object files that are downloaded to VxWorks.  They are used by
remote debuggers that use RPC (such as dbxWorks and vxGdb).
*/

#include "rpc/rpc.h"
#include "xdr_ld.h"

/* forward declarations */

bool_t xdr_String();   	/* xdr routine for argument list */


/*******************************************************************************
*
* xdr_String - xdr routine for strings.
* 
* Used by xdr_ldfile to handle the actual argument
* strings.  normally calls xdr_string - but does something 
* reasonable encode of null pointer.
*/

bool_t xdr_String (xdrs, strp)
    XDR	*xdrs;
    char **strp;

    {
    if ((*strp == 0) & (xdrs->x_op == XDR_ENCODE)) 
	return(FALSE);
    else 
	return(xdr_string(xdrs, strp, MAXSTRLEN));
    }
/*******************************************************************************
*
* xdr_ldfile - xdr routine for a single element in the load table 
*/

bool_t xdr_ldfile (xdrs, objp)
    XDR *xdrs;
    ldfile *objp;

    {
    if (! xdr_String(xdrs, &objp->name)) 
	return(FALSE);
    if (! xdr_int(xdrs, &objp->txt_addr)) 
	return(FALSE);
    if (! xdr_int(xdrs, &objp->data_addr)) 
	return(FALSE);
    if (! xdr_int(xdrs, &objp->bss_addr)) 
	return(FALSE);

    return(TRUE);
    }
/*******************************************************************************
*
* xdr_ldtabl -
*
* xdr routine for a list of files and load addresses loaded into VxWorks.
*/

bool_t xdr_ldtabl (xdrs,objp)
    XDR *xdrs;
    ldtabl *objp;

    {
    return (xdr_array (xdrs, (caddr_t *) &objp->tbl_ent,
	    (u_int *) &objp->tbl_size, MAXTBLSZ, sizeof(ldfile), xdr_ldfile));
    }
