/* ctxdesc.c - xdr routines for coding/decoding WDB_CTX_CREATE_DESC struct */

/* Copyright 1988-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01d,10jun95,pad Included rpc/rpc.h
01c,04apr95,ms	new data types.
01b,06nov94,tpr added comments
		changed xdr function for the redirIn, redirOut and isTask fields
01a,03oct94,tpr	written.
*/

/*
DESCRIPTION
This module contains the eXternal Data Representation (XDR) routine
for the WDB_CTX_CREATE_DESC structure of the WDB protocol.
*/

/* includes */

#include <rpc/rpc.h>

#include "wdbp.h"

/*******************************************************************************
*
* xdr_WDB_CTX_CREATE_DESC - code, decode od free the WDB_CTX_CREATE_DESC structure.
*
* This functions codes, decodes or frees the WDB_CTX_CREATE_DESC structure of the
* WDB protocol.
*
* RETURNS:  TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_CTX_CREATE_DESC
    (
    XDR *		xdrs,	/* xdr handle */
    WDB_CTX_CREATE_DESC * 	objp	/* pointer to the WDB_CTX_CREATE_DESC structure */
    )
    {
    TGT_INT_T	ix;

    if (!xdr_enum (xdrs, (enum_t *)&objp->contextType))
	return (FALSE);

    if (!xdr_TGT_ADDR_T (xdrs, &objp->stackBase))
	return (FALSE);

    if (!xdr_UINT32 (xdrs, &objp->stackSize))
	return (FALSE);

    if (!xdr_TGT_ADDR_T (xdrs, &objp->entry))
	return (FALSE);

    for (ix = 0; ix < 10; ix++)
	{
	if (!xdr_TGT_INT_T (xdrs, &objp->args[ix]))
	    return (FALSE);
	}

    if (!xdr_WDB_STRING_T (xdrs, &objp->name))
	return (FALSE);

    if (!xdr_TGT_INT_T (xdrs, &objp->priority))
	return (FALSE);

    if (!xdr_TGT_INT_T (xdrs, &objp->options))
	return (FALSE);

    if (!xdr_TGT_INT_T (xdrs, &objp->redirIn))
	return (FALSE);

    if (!xdr_TGT_INT_T (xdrs, &objp->redirOut))
	return (FALSE);

    if (!xdr_TGT_INT_T (xdrs, &objp->redirErr))
	return (FALSE);

    return (TRUE);
    }

