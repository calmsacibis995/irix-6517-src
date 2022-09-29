/* evtdata.c - xdr routines for coding/decoding WDB event data */

/* Copyright 1988-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01f,27jul95,ms  reworked to handle compiler alignments.
01e,10jun95,pad Included rpc/rpc.h
01d,04apr95,ms  new data types.
01c,06nov94,tpr added comments.
01b,04nov94,tpr changed xdr_int to xdr_TGT_INT_T for objp->eventArg coding.
01a,03oct94,tpr	written.
*/

/*
DESCRIPTION
This module contains the eXternal Data Representation (XDR) routine
for the wdbEvtData_t structure of the WDB protocol.
This data is passed up to the host during a WDB_EVENT_GET call.
*/

/* includes */

#include <rpc/rpc.h>
#include "wdbp.h"

/* data types */

typedef struct
    {
    TGT_INT_T           numInts;
    WDB_CTX             context;
    UINT32              info [WDB_MAX_EVT_DATA];
    } WDB_EVT_CTX_INFO;

/******************************************************************************
*
* xdr_WDB_EVT_CTX_INFO -
*/ 

static BOOL xdr_WDB_EVT_CTX_INFO
    (
    XDR *	    xdrs,		/* xdr handle */
    WDB_EVT_CTX_INFO *  objp		/* pointer to the object to process */
    )
    {
    UINT32	ix;

    if (!xdr_TGT_INT_T (xdrs, &objp->numInts))
	return (FALSE);

    if (!xdr_WDB_CTX (xdrs, &objp->context))
	return (FALSE);

    for (ix = 0; (ix + 2 < objp->numInts) && (ix + 2 < WDB_MAX_EVT_DATA); ix++)
	{
	if (!xdr_UINT32 (xdrs, &objp->info[ix]))
	    return (FALSE);
	}

    return (TRUE);
    }

/*******************************************************************************
*
* xdr_WDB_EVT_INFO - code, decode or free a WDB_EVT_INFO.
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_EVT_INFO
    (
    XDR *	    xdrs,		/* xdr handle */
    WDB_EVT_INFO *  objp		/* pointer to the object to process */
    )
    {
    UINT32	ix;

    if (!xdr_TGT_INT_T (xdrs, &objp->numInts))
	return (FALSE);


    for (ix = 0; (ix < objp->numInts) && (ix < WDB_MAX_EVT_DATA); ix++)
	{
	if (!xdr_UINT32 (xdrs, &objp->info[ix]))
	    return (FALSE);
	}

    return (TRUE);
    }

/******************************************************************************
*
* xdr_WDB_CALL_RETURN_INFO - code/decode data for a WDB_CALL_RETURN event.
*/ 

BOOL xdr_WDB_CALL_RETURN_INFO
    (
    XDR *		xdrs,		/* xdr handle */
    WDB_CALL_RET_INFO *	objp		/* pointer to a WDB_CALL_RET_INFO */
    )
    {
    if (!xdr_UINT32 (xdrs, &objp->callId))
	return (FALSE);

    if (!xdr_enum (xdrs, (enum_t *)&objp->returnType))
	return (FALSE);

    switch (objp->returnType)
	{
	case WDB_CALL_RET_DBL:
	    if (!xdr_double (xdrs, &objp->returnVal.returnValDbl))
		return (FALSE);
	    break;

	case WDB_CALL_RET_INT:
	default:
	    if (!xdr_TGT_INT_T (xdrs, &objp->returnVal.returnValInt))
		return (FALSE);
	}

    if (!xdr_TGT_INT_T (xdrs, &objp->errnoVal))
	return (FALSE);

    return (TRUE);
    }


/*******************************************************************************
*
* xdr_WDB_EVT_DATA - code, decode or free a WDB_EVT_DATA.
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_EVT_DATA
    (
    XDR *		xdrs,		/* xdr handle */
    WDB_EVT_DATA *	objp		/* pointer to a wdbEvtData_t */
    )
    {
    if (!xdr_enum (xdrs, (enum_t *)&objp->evtType))
        return (FALSE);

    switch (objp->evtType)
	{
	case WDB_EVT_VIO_WRITE:
	    if (!xdr_WDB_MEM_XFER (xdrs, &objp->eventInfo.vioWriteInfo))
		return (FALSE);
	    /* fall through */

	case WDB_EVT_NONE:
	    return (TRUE);

	case WDB_EVT_CALL_RET:
	    if (!xdr_WDB_CALL_RETURN_INFO (xdrs, &objp->eventInfo.callRetInfo))
		return (FALSE);
	    break;

	case WDB_EVT_CTX_START:
	case WDB_EVT_CTX_EXIT:
	case WDB_EVT_BP:
	case WDB_EVT_HW_BP:
	case WDB_EVT_WP:
	case WDB_EVT_EXC:
	    if (!xdr_WDB_EVT_CTX_INFO (xdrs,
			(WDB_EVT_CTX_INFO *)&objp->eventInfo.evtInfo))
		return (FALSE);
	    break;

	default:
	    if (!xdr_WDB_EVT_INFO (xdrs, &objp->eventInfo.evtInfo))
		return (FALSE);
	}

    return (TRUE);
    }

