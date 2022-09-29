/* ctx.c - xdr routine for coding/decoding a WDB_CTX */

/* Copyright 1988-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01d,10jun95,pad Included rpc/rpc.h
01c,04apr95,ms  new data types.
01b,06nov94,tpr added comments.
01a,03oct94,tpr	written.
*/

/*
DESCRIPTION

This module contains the eXternal Data Representation (XDR) routine
for the WDB_CTX structure of the WDB protocol.
*/

/* includes */

#include <rpc/rpc.h>

#include "wdbp.h"

/*******************************************************************************
*
* xdr_WDB_CTX - code, decode or free a WDB_CTX structure.
*
* RETURNS:  TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_CTX
    (
    XDR *	xdrs,			/* xdr handle */
    WDB_CTX *	objp			/* pointer to the WDB_CTX */
    )
    {
    if (!xdr_enum (xdrs, (enum_t *)&objp->contextType))
	return (FALSE);

    if (!xdr_UINT32 (xdrs, &objp->contextId))
	return (FALSE);

    return (TRUE);
    }

