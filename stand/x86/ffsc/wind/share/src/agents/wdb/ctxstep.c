/* ctxstep.c  - xdr rtn for coding/decoding the WDB_CTX_STEP_DESC structure */

/* Copyright 1988-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01e,10jun95,pad Included rpc/rpc.h
01d,04apr95,ms  new data types.
01c,06nov94,tpr added comments.
		changed xdr function of the context field.
01b,26oct94,tpr changed all CONTEXT_SOURCE_STEP by WDB_CTX_STEP_DESC.
01a,03oct94,tpr	written.
*/

/*
DESCRIPTION
This module contains the eXternal Data Representation (XDR) routine
for the WDB_CTX_STEP_DESC structure of the WDB protocol.
*/

/* includes */

#include <rpc/rpc.h>

#include "wdbp.h"

/*******************************************************************************
*
* xdr_WDB_CTX_STEP_DESC - code, decode or free the WDB_CTX_STEP_DESC structure.
*
* This function codes, decodes or frees the WDB_CTX_STEP_DESC structure of
* the protocol WDB.
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_CTX_STEP_DESC
    (
    XDR *		xdrs,		/* xdr handle */
    WDB_CTX_STEP_DESC *	objp		/* pointer to WDB_CTX_STEP_DESC */
    )
    {
    if (!xdr_WDB_CTX (xdrs, &objp->context))
	return (FALSE);

    if (!xdr_TGT_ADDR_T (xdrs, &objp->startAddr))
	return (FALSE);

    if (!xdr_TGT_ADDR_T (xdrs, &objp->endAddr))
	return (FALSE);

    return (TRUE);
    }

