/* xdrcore.c - xdr routine for coding/decoding the "core" WDB data types */

/* Copyright 1988-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01g,10jun95,pad Included rpc/rpc.h
01f,07jun95,tpr removed xdr_TGT_ADDR_T for the host. function defines by WTX.
01e,14apr95,ms	merged all basic filters into one file.
01d,04apr95,ms  new data types + made it call xdr_WDB_OPQ_DATA_T.
01c,06feb95,ms	made host side use malloc/free.
01b,31jan95,ms  handle NULL strings.
01a,24jan95,ms	written.
*/

/*
DESCRIPTION
This module contains the eXternal Data Representation (XDR) routine
for manipulating core WDB agent data types.
These XDR filters are always part of the agent, regardless of
how it is scalled. It currently contains:

	xdr_WDB_STRING_T	/@ code/decode a string @/
	xdr_WDB_OPQ_DATA_T	/@ code/decode a block of memory @/
	xdr_ARRAY		/@ code/decode an array @/
	xdr_TGT_ADDR_T		/@ code/decode a target address @/
	xdr_TGT_INT_T		/@ code/decode a target address @/
	xdr_UINT32		/@ code/decode a 32 bit unsigned int @/
*/

/* includes */

#include <rpc/rpc.h>

#include "wdbp.h"
#include "string.h"

/*******************************************************************************
*
* xdr_WDB_STRING_T - code, decode a string.
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_STRING_T
    (
    XDR *	xdrs,		/* XDR stream */
    WDB_STRING_T *	pStr		/* pointer to the string */
    )
    {
    UINT32	size = ~0;

    if (xdrs->x_op == XDR_ENCODE)
	{
	if (*pStr == NULL)
	    size = 0;
	else
	    size = strlen(*pStr) + 1;
	}

    if (!xdr_WDB_OPQ_DATA_T (xdrs, pStr, size))
	return (FALSE);

    return (TRUE);
    }

/*******************************************************************************
*
* xdr_WDB_OPQ_DATA_T - code, decode opaque data.
*
* RETURNS: TRUE if it succeeds, FALSE otherwise.
*/

BOOL xdr_WDB_OPQ_DATA_T
    (
    XDR *	xdrs,		/* XDR stream */
    WDB_OPQ_DATA_T *	ppData,		/* address of the data pointer */
    UINT32	size		/* number of bytes to process */
    )
    {
#ifndef HOST
    int		rndup;
#endif	/* !HOST */

    switch (xdrs->x_op)
	{
	case XDR_FREE:
#ifdef HOST
	    if (*ppData != NULL)
		free (*ppData);
#endif
	    return (TRUE);

	case XDR_ENCODE:
	    if (! xdr_UINT32 (xdrs, &size))
		return (FALSE);
	    return (xdr_opaque (xdrs, *ppData, size));

	case XDR_DECODE:
	    if (! xdr_UINT32 (xdrs, &size))
                return (FALSE);

	    if (size == 0)
		{
		*ppData = NULL;
		return (TRUE);
		}

#ifndef HOST
	    *ppData = (char *)xdr_inline (xdrs, size);
	    if (*ppData == NULL)
		return (FALSE);

	    rndup = size % BYTES_PER_XDR_UNIT;
	    if (rndup > 0)
		{
		rndup = BYTES_PER_XDR_UNIT - rndup;
		(void) xdr_inline (xdrs, rndup);
		}
#else
            if (*ppData == NULL)
                if ((*ppData = (char *) malloc (size)) == NULL)
                    return (FALSE);

            if (!xdr_opaque (xdrs, *ppData, size))
                return (FALSE);
#endif

	    return (TRUE);
	}

    return (FALSE);
    }

/******************************************************************************
*
* xdr_UINT32 - code/decode a 32 bit integer.
*/ 

BOOL xdr_UINT32
    (
    XDR *	xdrs,		/* XDR stream */
    UINT32 *	pInt		/* integer to code */
    )
    {
    switch (xdrs->x_op)
	{
	case XDR_DECODE:
	    return (XDR_GETLONG(xdrs, (long *)pInt));

	case XDR_ENCODE:
	    return (XDR_PUTLONG(xdrs, (long *)pInt));

	case XDR_FREE:
	default:
	    return (TRUE);
	}
    }

/******************************************************************************
*
* xdr_TGT_INT_T - code/decode a target integer.
*/ 

BOOL xdr_TGT_INT_T
    (
    XDR *	xdrs,		/* XDR stream */
    TGT_INT_T *	pTgtInt		/* target address */
    )
    {
    if (!xdr_UINT32 (xdrs, (UINT32 *)pTgtInt))
	return (FALSE);

    return (TRUE);
    }

#ifndef HOST
/******************************************************************************
*
* xdr_TGT_ADDR_T - code/decode a target address.
*/ 

BOOL xdr_TGT_ADDR_T
    (
    XDR *		xdrs,		/* XDR stream */
    TGT_ADDR_T *	pTgtAddr	/* target address */
    )
    {
    if (!xdr_UINT32 (xdrs, (UINT32 *)pTgtAddr))
	return (FALSE);

    return (TRUE);
    }
#endif /* HOST */

/******************************************************************************
*
* xdr_ARRAY - code/decode an array without using malloc.
*/ 

BOOL xdr_ARRAY
    (
    XDR *	xdrs,		/* XDR stream */
    caddr_t *	addrp,		/* array pointer */
    TGT_INT_T *	sizep,		/* number of elements */
    TGT_INT_T 	maxsize,	/* max numberof elements */
    TGT_INT_T 	elsize,		/* size in bytes of each element */
    xdrproc_t 	elproc		/* xdr routine to handle each element */
    )
    {
#ifdef	HOST
    return (xdr_array (xdrs, addrp, sizep, maxsize, elsize, elproc));
#else	/* !HOST */
    TGT_INT_T 	i;
    caddr_t 	target = *addrp;
    TGT_INT_T 	c;		/* the actual element count */
    BOOL 	stat = TRUE;
    TGT_INT_T 	nodesize;

    /* like strings, arrays are really counted arrays */

    if (! xdr_UINT32 (xdrs, (UINT32 *)sizep))
        return (FALSE);

    c = *sizep;
    if ((c > maxsize) && (xdrs->x_op != XDR_FREE))
        return (FALSE);

    nodesize = c * elsize;

    /*
     * if we are deserializing, we may need to allocate an array.
     * We also save time by checking for a null array if we are freeing.
     */

    if (target == NULL)
        {
        switch (xdrs->x_op)
            {
            case XDR_DECODE:
                if (c == 0)
                    return (TRUE);

                *addrp = target = (char *)xdr_inline (xdrs, 0);
                break;

            case XDR_FREE:
                return (TRUE);

            case XDR_ENCODE:
                break;
            }
        }

    /*
     * now we xdr each element of the array
     */

    for (i = 0; (i < c) && stat; i++)
        {
        stat = (*elproc)(xdrs, target, ~0);
        target += elsize;
        }

    return (stat);
#endif	/* !HOST */
    }

