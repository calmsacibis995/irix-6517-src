#if 0
static char sccsid[] = 	"@(#)rstatxdr.c	1.2 88/05/08 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.12
 */

#include <rpc/rpc.h>
#include <rpcsvc/rstat.h>

rstat(host, statp)
	const char *host;
	struct statstime *statp;
{
	return (callrpc(host, RSTATPROG, RSTATVERS_TIME, RSTATPROC_STATS,
	    xdr_void, (char *) NULL, xdr_statstime, (char *) statp));
}

havedisk(host)
	const char *host;
{
	long have;
	
	if (callrpc(host, RSTATPROG, RSTATVERS_SWTCH, RSTATPROC_HAVEDISK,
	    xdr_void, (char *) NULL, xdr_long, (char *) &have) != 0)
		return (-1);
	else
		return (have);
}

xdr_stats(xdrs, statp)
	XDR *xdrs;
	struct stats *statp;
{
	int i;
	
	for (i = 0; i < CPUSTATES; i++)
		if (xdr_int(xdrs, (int *) &statp->cp_time[i]) == 0)
			return (0);
	for (i = 0; i < DK_NDRIVE; i++)
		if (xdr_int(xdrs, (int *) &statp->dk_xfer[i]) == 0)
			return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pgpgin) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pgpgout) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pswpin) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pswpout) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_intr) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_ipackets) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_ierrors) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_oerrors) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_collisions) == 0)
		return (0);
	return (1);
}

xdr_statsswtch(xdrs, statp)		/* version 2 */
	XDR *xdrs;
	struct statsswtch *statp;
{
	int i;
	
	for (i = 0; i < CPUSTATES; i++)
		if (xdr_int(xdrs, (int *) &statp->cp_time[i]) == 0)
			return (0);
	for (i = 0; i < DK_NDRIVE; i++)
		if (xdr_int(xdrs, (int *) &statp->dk_xfer[i]) == 0)
			return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pgpgin) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pgpgout) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pswpin) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pswpout) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_intr) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_ipackets) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_ierrors) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_oerrors) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_collisions) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_swtch) == 0)
		return (0);
	for (i = 0; i < 3; i++)
		if (xdr_long(xdrs, &statp->avenrun[i]) == 0)
			return (0);
	if (xdr_timeval(xdrs, &statp->boottime) == 0)
		return (0);
	return (1);
}

xdr_statstime(xdrs, statp)		/* version 3 */
	XDR *xdrs;
	struct statstime *statp;
{
	int i;
	
	for (i = 0; i < CPUSTATES; i++)
		if (xdr_int(xdrs, (int *) &statp->cp_time[i]) == 0)
			return (0);
	for (i = 0; i < DK_NDRIVE; i++)
		if (xdr_int(xdrs, (int *) &statp->dk_xfer[i]) == 0)
			return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pgpgin) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pgpgout) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pswpin) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_pswpout) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_intr) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_ipackets) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_ierrors) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_oerrors) == 0)
		return (0);
	if (xdr_int(xdrs, &statp->if_collisions) == 0)
		return (0);
	if (xdr_int(xdrs, (int *) &statp->v_swtch) == 0)
		return (0);
	for (i = 0; i < 3; i++)
		if (xdr_long(xdrs, &statp->avenrun[i]) == 0)
			return (0);
	if (xdr_timeval(xdrs, &statp->boottime) == 0)
		return (0);
	if (xdr_timeval(xdrs, &statp->curtime) == 0)
		return (0);
	/* 
	 * Many implementations of this protocol incorrectly left out
	 * if_opackets.  This value is included here for compatibility 
	 * with these buggy implementations.
	 */
	(void) xdr_int (xdrs, &statp->if_opackets);
	return (1);
}

xdr_timeval(xdrs, tvp)
	XDR *xdrs;
	struct timeval *tvp;
{
#if _MIPS_SZLONG == 64
	/*
	 * for compatibility w/ 6.1 where tv_sec was a long
	 * we need to be sure to zero out the top 32 bits on decode
	 */
	if (xdrs->x_op == XDR_DECODE)
		*(uint *)tvp = 0;
	/* in either case, 32 bits is pushed across/retrieved */
	if (xdr_int(xdrs, &tvp->tv_sec) == 0)
#else
	if (xdr_long(xdrs, &tvp->tv_sec) == 0)
#endif
		return (0);
	if (xdr_long(xdrs, &tvp->tv_usec) == 0)
		return (0);
	return (1);
}
