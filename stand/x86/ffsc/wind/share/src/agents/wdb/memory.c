/* memory.c - xdr routine for coding/decoding WDB memory structures */

/* Copyright 1988-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01d,10jun95,pad Included rpc/rpc.h
01c,04apr95,ms  new data types.
01b,06nov94,tpr added comments
01a,03oct94,tpr	written.
*/

/*
DESCRIPTION
This module contains the eXternal Data Representation (XDR) routine
for the WDB_MEM_REGION, WDB_MEM_XFER, and WDB_MEM_SCAN_DESC structures.
*/

/* includes */

#include <rpc/rpc.h>

#include "wdbp.h"

/*******************************************************************************
*
* xdr_WDB_MEM_REGION - code, decode or free the WDB_MEM_REGION structure.
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_MEM_REGION
    (
    XDR *		xdrs,	/* xdr handle */
    WDB_MEM_REGION *	objp	/* pointer to the WDB_MEM_REGION structure */
    )
    {
    if (!xdr_TGT_ADDR_T (xdrs, &objp->baseAddr))
	return (FALSE);

    if (!xdr_TGT_INT_T (xdrs, &objp->numBytes))
	return (FALSE);

    if (!xdr_UINT32 (xdrs, &objp->param))
	return (FALSE);

    return (TRUE);
    }

/*******************************************************************************
*
* xdr_WDB_MEM_XFER - code, decode or free the WDB_MEM_XFER structure. 
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_MEM_XFER
    (
    XDR *		xdrs,
    WDB_MEM_XFER *	objp
    )
    {
    if (!xdr_TGT_INT_T (xdrs, &objp->numBytes))
	return (FALSE);

    if (!xdr_TGT_ADDR_T (xdrs, &objp->destination))
	return (FALSE);

    if (!xdr_WDB_OPQ_DATA_T (xdrs, &objp->source, objp->numBytes))
	return (FALSE);

    return (TRUE);
    }

/******************************************************************************
*
* xdr_WDB_MEM_SCAN_DESC
*/ 

BOOL xdr_WDB_MEM_SCAN_DESC
    (
    XDR *		xdrs,
    WDB_MEM_SCAN_DESC *	objp
    )
    {
    if (!xdr_WDB_MEM_REGION (xdrs, &objp->memRegion))
	return (FALSE);

    if (!xdr_WDB_MEM_XFER (xdrs, &objp->memXfer))
	return (FALSE);

    return (TRUE);
    }

