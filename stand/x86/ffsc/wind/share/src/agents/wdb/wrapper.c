/* wrapper.c - wrapper xdr filters for the WDB RPC-backend */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,14jun95,tpr added checkun capability.
01b,10jun95,pad included rpc/rpc.h
01a,18apr94,ms  written.
*/

/*
DESCPRIPTION

*/

/* includes */

#include <rpc/rpc.h>

#include "wdbp.h"

/*******************************************************************************
*
* xdr_WDB_PARAM_WRAPPER -
*/ 

BOOL xdr_WDB_PARAM_WRAPPER
    (
    XDR * 	         xdrs,
    WDB_PARAM_WRAPPER *  objp
    )
    {
    UINT32	xdrStreamSize = 0x00;
    UINT32	cksumVal = 0x00;
    UINT32	cksumPos;
    UINT32	sizePos;	

    cksumPos = XDR_GETPOS (xdrs);

    if (!xdr_UINT32 (xdrs, &cksumVal))
	return (FALSE);

    sizePos = XDR_GETPOS (xdrs);

    if (!xdr_UINT32 (xdrs, &xdrStreamSize))
	return (FALSE);

#ifndef HOST
    if (!xdr_CHECKSUM (xdrs, cksumVal, xdrStreamSize, cksumPos, sizePos))
	    return (FALSE);
#endif 	/* !HOST */

    if (! xdr_UINT32 (xdrs, &objp->seqNum))
	return (FALSE);

    if (! (*objp->xdr) (xdrs, objp->pParams))
	return (FALSE);

#ifdef	HOST
    if (!xdr_CHECKSUM (xdrs, cksumVal, xdrStreamSize, cksumPos, sizePos))
	return (FALSE);
#endif	/* HOST */

    return (TRUE);
    }

/******************************************************************************
*
* xdr_WDB_REPLY_WRAPPER -
*/ 

BOOL xdr_WDB_REPLY_WRAPPER
    (
    XDR *  	         xdrs,
    WDB_REPLY_WRAPPER *  objp
    )
    {
    UINT32	xdrStreamSize = 0x00;
    UINT32	cksumVal = 0x00;
    UINT32	cksumPos;
    UINT32	sizePos;	

    cksumPos = XDR_GETPOS (xdrs);

    if (!xdr_UINT32 (xdrs, &cksumVal))
	return (FALSE);

    sizePos = XDR_GETPOS (xdrs);

    if (!xdr_UINT32 (xdrs, &xdrStreamSize))
	return (FALSE);

#ifdef	HOST
    if (!xdr_CHECKSUM (xdrs, cksumVal, xdrStreamSize, cksumPos, sizePos))
	return (FALSE);
#endif	/* HOST */

    if (! xdr_UINT32 (xdrs, &objp->errCode))
	return (FALSE);

    /* encode/decode the rest of the reply if there is no error */

    if ((objp->errCode & (~WDB_EVENT_NOTIFY)) == OK)
	if (! (*objp->xdr) (xdrs, objp->pReply))
	    return (FALSE);

#ifndef HOST
    if (!xdr_CHECKSUM (xdrs, cksumVal, xdrStreamSize, cksumPos, sizePos))
	    return (FALSE);
#endif 	/* !HOST */

    return (TRUE);
    }

