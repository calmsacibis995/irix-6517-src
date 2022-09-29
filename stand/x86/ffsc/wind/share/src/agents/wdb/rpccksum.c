/* rpcCksum.c - xdr checksum filter for UDP trame */

/* Copyright 1992-1995 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01b,21jun95,tpr changed code inside xdrCksum().
01a,12jun95,tpr	created.
*/

/*
DESCRIPTION

This XDR filter either computes the XDR stream checksun and put this
value inside the stream or checks that the xdr stream checksum match the
checksum inside the stream.
*/

#include <rpc/rpc.h>

#include "wdbp.h"
#ifdef	HOST
#include "wpwrutil.h"
#endif	/* HOST */

#define WDB_CKSUM_FLAG	0xffff0000
#define WDB_XDR_STREAM_MAX_SIZE	0x1000

LOCAL BOOL	xdr_wdb_cksum_enabled = TRUE;

/******************************************************************************
*
* xdrCksum - returns the ones compliment checksum of some memory.
*
* NOMANUAL
*/

UINT32 xdrCksum
    (
    unsigned short *	pAddr,			/* start of buffer */
    int			len			/* size of buffer in bytes */
    )
    {
    UINT32	sum = 0;
    BOOL	swap = FALSE;

    /* take care of unaligned buffer address */

    if ((UINT32)pAddr & 0x1)
	{
	sum += *((unsigned char *) pAddr);
	pAddr = (unsigned short *)(((unsigned char *)pAddr) + 1);
	len--;
	swap = TRUE;
	}

    /* evaluate checksum */

    while (len > 1)
	{
	sum += * (pAddr ++);
	len -= 2;
	}

    /* take care of last byte */

    if (len > 0)
	sum = sum + ((*(unsigned char *) pAddr) << 8);

    /* fold to 16 bits */

    sum = (sum & 0xffff) + (sum >> 16);
    sum = (sum & 0xffff) + (sum >> 16);

    /* swap if we started on unaligned address */

    if (swap)
	sum = ((sum & 0x00ff) << 8) | ((sum & 0xff00) >> 8);

    return (~sum);
    }

/*******************************************************************************
*
* xdr_CHECKSUM - checksum an XDR stream
*
* When data are encoded in the XDR stream, this function computes the
* checksum of the XDR stream pointer to by <xdr>. The checksum is performed
* between the start point of the XDR stream and the current position in the
* stream. The XDR stream size and checksum value are placed in the stream
* at the <xdrStreamSizePos> and <xdrCksumValPos> position.
*
* When data are decoded from the stream, the filter verifies that the stream
* checksum value match the checkum value inside the stream. One of the checksum
* feature is when the checksum value of a buffer is included inside this buffer,
* its checksum value is equal to zero. This feature is used by this function
* to check if checksum values are equal.
*
* RETURN: TRUE no error was detected otherwise FALSE.
*/

BOOL xdr_CHECKSUM
    (
    XDR *	xdrs,			/* xdr to compute/check the checksum */
    UINT32	xdrCksumVal,		/* xdr stream checksum value */
    UINT32	xdrStreamSize,		/* xdr stream size */
    UINT32	xdrCksumValPos,		/* checksum value inside the steam */
    UINT32	xdrStreamSizePos	/* stream size inside the xdr steam */
    )
    {
    long *		xdrStreamAddr;	/* xdr stream buffer address */
    u_int		xdrStreamPos;	/* xdr strean current position */
    u_int		cksum = 0x00;  	/* checksum() return value */

    switch (xdrs->x_op)
	{
	case XDR_ENCODE:
	    /* return if the xdr stream checksum capability is not enabled */

	    if (!xdr_wdb_cksum_enabled)
		return (TRUE);

	    /* save the current position in the xdr stream */

	    xdrStreamPos = XDR_GETPOS (xdrs);

	    /* get the xdr stream address */

	    if ((xdrStreamAddr = xdr_inline (xdrs, 0)) == NULL)
		return (FALSE);

	    /* compute the xdr stream start address */

	    xdrStreamAddr = (long *) ((char *) xdrStreamAddr - xdrStreamPos +
	    					sizeof (UINT32));

	    /* set the current position at the xdr stream size value */

	    if (!XDR_SETPOS (xdrs, xdrStreamSizePos))
		return (FALSE);

	    /* compute the XDR stream size */

	    xdrStreamSize = xdrStreamPos - sizeof (UINT32);

	    /* save the xdr stream size into the xdr stream */

	    if (!xdr_UINT32 (xdrs, &xdrStreamSize))
		return (FALSE);

	    /* checksum the xdr stream */

	    cksum = (u_int) xdrCksum ((unsigned short *) xdrStreamAddr,
								xdrStreamSize);

	    cksum = htons (cksum);

	    /* set the flag to signal the xdr stream checksum is available */

	    cksum |= WDB_CKSUM_FLAG;

	    /* set the current position at the xdr stream checksum value */

	    if (!XDR_SETPOS (xdrs, xdrCksumValPos))
		return (FALSE);

	    /* put the xdr stream checksum value in the xdr steam */

	    if (!xdr_UINT32 (xdrs, &cksum))
		return (FALSE);

	    /* restore the current position in the xdr stream */

	    if (!XDR_SETPOS (xdrs, xdrStreamPos))
		return (FALSE);

            break;

	case XDR_DECODE:
	    if (!xdr_wdb_cksum_enabled) 
		{
	        /* 
	         * if the xdr stream checksum capability is desabled and the
	         * checksum is performed on the reveiced stream then turn on
	         * the checksum capability and control checkum coherency.
	         */

		if (xdrCksumVal & WDB_CKSUM_FLAG)
		    xdr_wdb_cksum_enabled = TRUE;
		else 
		    return (TRUE);
		}
	    else 
		if (!(xdrCksumVal & WDB_CKSUM_FLAG))
		    {
		    /*
		     * Turn off checksum capability because the reveiced
		     * stream is sent without its checksum.
		     */
#ifdef	HOST
		    wpwrLogWarn ("The agent doesn't provide XDR stream checksum\n");
#endif
		    xdr_wdb_cksum_enabled = FALSE;
		    return (TRUE);
		    }

	    /* save the current position in the xdr stream */

	    xdrStreamPos = XDR_GETPOS (xdrs);

	    /* get the xdr stream address */

	    if ((xdrStreamAddr = xdr_inline (xdrs, 0)) == NULL)
		return (FALSE);

	    /* compute the xdr stream start address */

	    xdrStreamAddr = (long *) ((char *) xdrStreamAddr - xdrStreamPos +
						sizeof (UINT32));

	    /*
	     * check the xdr stream size is not too big. The size
	     * field may be corrupted.
	     */

	    if (WDB_XDR_STREAM_MAX_SIZE < xdrStreamSize)
		return (FALSE);

	    /* checksum the xdr stream */

	    cksum = (u_int) xdrCksum ((unsigned short *) xdrStreamAddr,
								xdrStreamSize);

	    /* 
	     * if the received stream is not corrupted its checksun value 
	     * should be equal to zero because its checksum value is inside
	     * the stream.
	     * if checksum is different from 0 then the received stream is 
	     * corrupted. So return FLASE .
	     */

	    if ((cksum & ~WDB_CKSUM_FLAG) != 0)
		return (FALSE);

            break;

	default:
	    break;
	}

     return (TRUE);
     }
