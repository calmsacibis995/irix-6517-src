/* regs.c - xdr filters for WDB_REG_READ_DESC and WDB_REG_WRITE_DESC structures */

/* Copyright 1988-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01c,10jun95,pad Included rpc/rpc.h
01b,04apr95,ms  new data types.
01a,20jan95,ms	written.
*/

/*
DESCRIPTION
This module contains the eXternal Data Representation (XDR) routine
for the WDB_REG_READ_DESC and WDB_REG_READ_DESC structures of the WDB protocol.
*/

/* includes */

#include <rpc/rpc.h>

#include "wdbp.h"

/*******************************************************************************
*
* xdr_WDB_REG_READ_DESC - code, decode or free the WDB_REG_READ_DESC structure. 
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_REG_READ_DESC
    (
    XDR *		xdrs,
    WDB_REG_READ_DESC *	objp
    )
    {
    if (!xdr_enum (xdrs, (enum_t *)&objp->regSetType))
	return (FALSE);

    if (!xdr_WDB_CTX (xdrs, &objp->context))
	return (FALSE);

    if (!xdr_WDB_MEM_REGION (xdrs, &objp->memRegion))
	return (FALSE);

    return (TRUE);
    }

/*******************************************************************************
*
* xdr_WDB_REG_WRITE_DESC - code, decode or free the WDB_REG_WRITE_DESC structure. 
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_REG_WRITE_DESC
    (
    XDR *		xdrs,
    WDB_REG_WRITE_DESC *	objp
    )
    {
    if (!xdr_enum (xdrs, (enum_t *)&objp->regSetType))
	return (FALSE);

    if (!xdr_WDB_CTX (xdrs, &objp->context))
	return (FALSE);

    if (!xdr_WDB_MEM_XFER (xdrs, &objp->memXfer))
	return (FALSE);

    return (TRUE);
    }

