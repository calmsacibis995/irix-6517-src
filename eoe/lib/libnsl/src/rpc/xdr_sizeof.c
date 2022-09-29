/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

 
#ident	"@(#)libnsl:rpc/xdr_sizeof.c	1.1.1.1"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 
#if defined(__STDC__) 
        #pragma weak xdr_sizeof	= _xdr_sizeof
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
/*
 * General purpose routine to see how much space something will use
 * when serialized using XDR.
 */

#include <rpc/types.h>
#include <rpc/xdr.h>

/* ARGSUSED */
static bool_t
x_putlong(xdrs, longp)
	XDR *xdrs;
	long *longp;
{
	xdrs->x_handy += BYTES_PER_XDR_UNIT;
	return (TRUE);
}

/* ARGSUSED */
static bool_t
x_putbytes(xdrs, bp, len)
	XDR *xdrs;
	void *bp;
	int len;
{
	xdrs->x_handy += RNDUP(len);
	return (TRUE);
}

static u_int
x_getpostn(xdrs)
	XDR *xdrs;
{
	return (xdrs->x_handy);
}

/* ARGSUSED */
static bool_t
x_setpostn(xdrs, pos)
	XDR *xdrs;
	u_int pos;
{
	/* This is not allowed */
	return (FALSE);
}

static long *
x_inline(xdrs, len)
	XDR *xdrs;
	u_int len;
{
	if (len == 0)
		return (NULL);
	if (xdrs->x_op != XDR_ENCODE)
		return (NULL);
	if (len < (int) xdrs->x_base) {
		/* x_private was already allocated */
		xdrs->x_handy += RNDUP (len);
		return ((long *) xdrs->x_private);
	} else {
		/* Free the earlier space and allocate new area */
		if (xdrs->x_private)
			free(xdrs->x_private);
		if ((xdrs->x_private = (caddr_t) malloc(len)) == NULL) {
			xdrs->x_base = 0;
			return (NULL);
		}
		xdrs->x_base = (caddr_t) len;
		xdrs->x_handy += RNDUP (len);
		return ((long *) xdrs->x_private);
	}
}

static
harmless()
{
	/* Always return FALSE/NULL, as the case may be */
	return (0);
}

static void
x_destroy(xdrs)
	XDR *xdrs;
{
	xdrs->x_handy = 0;
	xdrs->x_base = 0;
	if (xdrs->x_private) {
		free(xdrs->x_private);
		xdrs->x_private = NULL;
	}
	return;
}

unsigned long
xdr_sizeof(func, data)
	xdrproc_t func;
	void *data;
{
	XDR x;
	struct xdr_ops ops;
	bool_t stat;

	ops.x_putlong = x_putlong;
	ops.x_putbytes = x_putbytes;
	ops.x_inline = x_inline;
	ops.x_getpostn = x_getpostn;
	ops.x_setpostn = x_setpostn;
	ops.x_destroy = x_destroy;

	/* the other harmless ones */
	ops.x_getlong = harmless;
	ops.x_getbytes = harmless;

	x.x_op = XDR_ENCODE;
	x.x_ops = &ops;
	x.x_handy = 0;
	x.x_private = (caddr_t) NULL;
	x.x_base = (caddr_t) 0;

	stat = func(&x, data);
	if (x.x_private)
		free(x.x_private);
	return (stat == TRUE ? (unsigned) x.x_handy: 0);
}
