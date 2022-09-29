/* evtpoint.c - xdr routine for coding/decoding a WDB_EVTPT_ADD_DESC */

/* Copyright 1988-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01d,10jun95,pad Included rpc/rpc.h
01c,04apr95,ms  new data types.
01b,06nov94,tpr	added comments
01a,03oct94,tpr	written.
*/

/*
DESCRIPTION
This module contains the eXternal Data Representation (XDR) routine
for the WDB_EVTPT_ADD_DESC structure of the WDB protocol.
*/

/* includes */

#include <rpc/rpc.h>

#include "wdbp.h"

/******************************************************************************
*
* xdr_WDB_ACTION - decode or free a WDB_ACTION.
*/

static BOOL xdr_WDB_ACTION
    (
    XDR *	  xdrs,		/* xdr handle */
    WDB_ACTION * objp		/* pointer to the WDB_EVTPT_ADD_DESC structure */
    )
    {
    if (!xdr_enum (xdrs, (enum_t *)&objp->actionType))
	return (FALSE);

    if (!xdr_UINT32 (xdrs, &objp->actionArg))
	return (FALSE);

    if (!xdr_TGT_ADDR_T (xdrs, &objp->callRtn))
	return (FALSE);

    if (!xdr_TGT_INT_T (xdrs, &objp->callArg))
	return (FALSE);

    return (TRUE);
    }

/*******************************************************************************
*
* xdr_WDB_EVTPT_ADD_DESC - code, decode or free a WDB_EVTPT_ADD_DESC structure
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_EVTPT_ADD_DESC
    (
    XDR *			xdrs,	/* xdr handle */
    WDB_EVTPT_ADD_DESC *	objp	/* object pointer */
    )
    {
    if (!xdr_enum (xdrs, (enum_t *)&objp->evtType))
	return (FALSE);

    if (!xdr_TGT_INT_T (xdrs, &objp->numArgs))
	return (FALSE);

    if (!xdr_ARRAY (xdrs, (char **)&objp->args, &objp->numArgs, objp->numArgs,
		sizeof (UINT32), xdr_UINT32))
	return (FALSE);

    if (!xdr_WDB_CTX (xdrs, &objp->context))
	return (FALSE);

    if (!xdr_WDB_ACTION (xdrs, &objp->action))
	return (FALSE);

    return (TRUE);
    }

/******************************************************************************
*
* xdr_WDB_EVTPT_DEL_DESC -
*/ 

BOOL xdr_WDB_EVTPT_DEL_DESC
    (
    XDR *			xdrs,	/* xdr handle */
    WDB_EVTPT_DEL_DESC *	objp	/* object pointer */
    )
    {
    if (!xdr_enum (xdrs, (enum_t *)&objp->evtType))
	return (FALSE);

    if (!xdr_TGT_ADDR_T (xdrs, &objp->evtptId))
	return (FALSE);

    return (TRUE);
    }

