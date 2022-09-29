/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.11 88/02/08 
 */


/*
 * xdr_array.c, Generic XDR routines impelmentation.
 *
 * These are the "non-trivial" xdr primitives used to serialize and de-serialize
 * arrays.  See xdr.h for more info on the interface to xdr.
 */

#ifdef _KERNEL
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "types.h"
#include "xdr.h"
#include "netinet/in.h"

#else
#ifdef __STDC__
	#pragma weak xdr_array = _xdr_array
	#pragma weak xdr_vector = _xdr_vector
#endif
#include "synonyms.h"
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/errorhandler.h>
#include <bstring.h>		/* prototype for bzero() */
#endif

#define LASTUNSIGNED	((u_int)0-1)


/*
 * XDR an array of arbitrary elements
 * *addrp is a pointer to the array, *sizep is the number of elements.
 * If addrp is NULL (*sizep * elsize) bytes are allocated.
 * elsize is the size (in bytes) of each element, and elproc is the
 * xdr procedure to call to handle each element of the array.
 */
bool_t
xdr_array(
	register XDR *xdrs,
	caddr_t *addrp,		/* array pointer */
	u_int *sizep,		/* number of elements */
	u_int maxsize,		/* max numberof elements */
	u_int elsize,		/* size in bytes of each element */
	xdrproc_t elproc)	/* xdr routine to handle each element */
{
	register u_int i;
	register caddr_t target = *addrp;
	register u_int c;  /* the actual element count */
	register bool_t stat = TRUE;
	register u_int nodesize;

	/* like strings, arrays are really counted arrays */
	if (! xdr_u_int(xdrs, sizep)) {
#ifdef _KERNEL
		printf("xdr_array: size FAILED\n");
#endif
		return (FALSE);
	}
	c = *sizep;
	if ((c > maxsize) && (xdrs->x_op != XDR_FREE)) {
#ifdef _KERNEL
		printf("xdr_array: bad size FAILED\n");
#endif
		return (FALSE);
	}
	nodesize = c * elsize;

	/*
	 * if we are deserializing, we may need to allocate an array.
	 * We also save time by checking for a null array if we are freeing.
	 */
	if (target == NULL)
		switch (xdrs->x_op) {
		case XDR_DECODE:
			if (c == 0)
				return (TRUE);
			*addrp = target = mem_alloc(nodesize);
#ifndef _KERNEL
			if (target == NULL) {
				_rpc_errorhandler(LOG_ERR, 
					"xdr_array: out of memory");
				return (FALSE);
			}
#endif
			bzero(target, (int)nodesize);
			break;

		case XDR_FREE:
			return (TRUE);
	}
	
	/*
	 * now we xdr each element of array
	 */
	for (i = 0; (i < c) && stat; i++) {
		stat = (*(xdrproc3_t)elproc)(xdrs, target, LASTUNSIGNED);
		target += elsize;
	}

	/*
	 * the array may need freeing
	 */
	if (xdrs->x_op == XDR_FREE) {
		mem_free(*addrp, nodesize);
		*addrp = NULL;
	}
	return (stat);
}

#ifndef _KERNEL 
/*
 * xdr_vector():
 *
 * XDR a fixed length array. Unlike variable-length arrays,
 * the storage of fixed length arrays is static and unfreeable.
 * > basep: base of the array
 * > size: size of the array
 * > elemsize: size of each element
 * > xdr_elem: routine to XDR each element
 */
bool_t
xdr_vector(
	register XDR *xdrs,
	register char *basep,
	register u_int nelem,
	register u_int elemsize,
	register xdrproc_t xdr_elem)
{
	register u_int i;
	register char *elptr;

	elptr = basep;
	for (i = 0; i < nelem; i++) {
		if (! (*(xdrproc3_t)xdr_elem)(xdrs, elptr, LASTUNSIGNED)) {
			return(FALSE);
		}
		elptr += elemsize;
	}
	return(TRUE);	
}
#endif /* !_KERNEL */
