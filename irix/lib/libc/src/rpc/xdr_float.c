/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.13 88/02/08 
 */


/*
 * xdr_float.c, Generic XDR routines impelmentation.
 *
 * These are the "floating point" xdr routines used to (de)serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */

#ifdef _KERNEL
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "types.h"
#include "xdr.h"

#else
#ifdef __STDC__
	#pragma weak xdr_float = _xdr_float
	#pragma weak xdr_double = _xdr_double
#if 0
#if (_MIPS_SIM == _ABIN32) || (_MIPS_SIM == _ABI64)
	#pragma weak xdr_quadruple = _xdr_quadruple
#endif
#endif
#endif
#include "synonyms.h"
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#endif

/*
 * NB: Not portable.
 * This routine works on Suns (Sky / 68000's) and Vaxen.
 */

bool_t
xdr_float(xdrs, fp)
	register XDR *xdrs;
	register float *fp;
{
	switch (xdrs->x_op) {

#if (_MIPS_SZLONG != _MIPS_SZINT)
	case XDR_ENCODE:
		return (XDR_PUTINT(xdrs, (int *)fp));

	case XDR_DECODE:
		return (XDR_GETINT(xdrs, (int *)fp));
#else
	case XDR_ENCODE:
		return (XDR_PUTLONG(xdrs, (long *)fp));

	case XDR_DECODE:
		return (XDR_GETLONG(xdrs, (long *)fp));
#endif	/* _MIPS_SZLONG == _MIPS_SZINT */

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}


bool_t
xdr_double(xdrs, dp)
	register XDR *xdrs;
	double *dp;
{
#if (_MIPS_SZLONG != _MIPS_SZINT)
	register int *ip;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		ip = (int *)dp;
		return (XDR_PUTINT(xdrs, ip++) && XDR_PUTINT(xdrs, ip));

	case XDR_DECODE:
		ip = (int *)dp;
		return (XDR_GETINT(xdrs, ip++) && XDR_GETINT(xdrs, ip));

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
#else
	register long *lp;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		lp = (long *)dp;
		return (XDR_PUTLONG(xdrs, lp++) && XDR_PUTLONG(xdrs, lp));

	case XDR_DECODE:
		lp = (long *)dp;
		return (XDR_GETLONG(xdrs, lp++) && XDR_GETLONG(xdrs, lp));

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
#endif	/* _MIPS_SZLONG == _MIPS_SZINT */
}

#if 0
/*
 * This routine is stubbed out due to the lack of an interoperable format for
 * 128 bit floating point values.
 */
#if (_MIPS_SIM == _ABIN32) || (_MIPS_SIM == _ABI64)
bool_t
_xdr_quadruple(xdrs, ldp)
	register XDR *xdrs;
	long double *ldp;
{
	switch (xdrs->x_op) {

	case XDR_ENCODE:
		return (XDR_PUTBYTES(xdrs, (char *)ldp, sizeof(*ldp)));

	case XDR_DECODE:
		return (XDR_GETBYTES(xdrs, (char *)ldp, sizeof(*ldp)));

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}
#endif
#endif
