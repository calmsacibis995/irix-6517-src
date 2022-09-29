/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)librpc:xdr.c	1.2.4.1"

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
        #pragma weak xdr_bool	= _xdr_bool
        #pragma weak xdr_bytes	= _xdr_bytes
        #pragma weak xdr_char	= _xdr_char
        #pragma weak xdr_enum	= _xdr_enum
        #pragma weak xdr_free	= _xdr_free
        #pragma weak xdr_int	= _xdr_int
        #pragma weak xdr_long	= _xdr_long
        #pragma weak xdr_opaque	= _xdr_opaque
        #pragma weak xdr_short	= _xdr_short
        #pragma weak xdr_string	= _xdr_string
        #pragma weak xdr_u_char	= _xdr_u_char
        #pragma weak xdr_u_long	= _xdr_u_long
        #pragma weak xdr_u_short	= _xdr_u_short
        #pragma weak xdr_union	= _xdr_union
        #pragma weak xdr_void	= _xdr_void
        #pragma weak xdr_wrapstring	= _xdr_wrapstring
#ifndef _LIBNSL_ABI
        #pragma weak xdr_netobj	= _xdr_netobj
        #pragma weak xdr_u_int	= _xdr_u_int
#endif /* _LIBNSL_ABI */
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>
/*
 * xdr.c, Generic XDR routines implementation.
 *
 * These are the "generic" xdr routines used to serialize and de-serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */

#ifdef KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#else
#ifdef SYSLOG
#include <sys/syslog.h>
#else
#define	LOG_ERR 3
#endif /* SYSLOG */
#include <stdio.h>
#endif

#include <rpc/types.h>
#include <rpc/xdr.h>

/*
 * constants specific to the xdr "protocol"
 */
#define	XDR_FALSE	((long) 0)
#define	XDR_TRUE	((long) 1)
#define	LASTUNSIGNED	((u_int) 0-1)

/*
 * for unit alignment
 */
static char xdr_zero[BYTES_PER_XDR_UNIT] = { 0, 0, 0, 0 };

/*
 * MACRO definitions for the more commonly used XDR_routines
 */
#define	XDR_LONG(xdrs, lp)	\
	((xdrs->x_op == XDR_ENCODE) ? XDR_PUTLONG(xdrs, lp) : \
	(xdrs->x_op == XDR_DECODE) ? XDR_GETLONG(xdrs, lp) : \
	(xdrs->x_op == XDR_FREE) ? TRUE : FALSE)

#define	XDR_U_LONG(xdrs, ulp)	XDR_LONG(xdrs, (long *)(ulp))

#define	XDR_INT(xdrs, ip)	XDR_LONG(xdrs, (long *)(ip)) 

#define	XDR_U_INT(xdrs, ip)	XDR_U_LONG(xdrs, (u_long *)(ip))

#ifndef KERNEL
/*
 * Free a data structure using XDR
 * Not a filter, but a convenient utility nonetheless
 */
void
xdr_free(
	xdrproc_t proc,
	void *objp)
{
	XDR x;

	x.x_op = XDR_FREE;
	(*proc)(&x, objp);
}
#endif

/*
 * XDR nothing
 */
bool_t
xdr_void(
	XDR *xdrs,
	void * addr)
{
	return (TRUE);
}

/*
 * XDR integers
 */
bool_t
xdr_int(xdrs, ip)
	XDR *xdrs;
	int *ip;
{

#ifdef lint
	(void) (xdr_short(xdrs, (short *)ip));
	return (xdr_long(xdrs, (long *)ip));
#else
	return (XDR_INT(xdrs, ip));
#endif
}

/*
 * XDR unsigned integers
 */
bool_t
xdr_u_int(xdrs, up)
	XDR *xdrs;
	u_int *up;
{

#ifdef lint
	(void) (xdr_short(xdrs, (short *)up));
	return (xdr_u_long(xdrs, (u_long *)up));
#else
	return (XDR_U_INT(xdrs, up));
#endif
}

/*
 * XDR long integers
 * same as xdr_u_long
 */
bool_t
xdr_long(xdrs, lp)
	register XDR *xdrs;
	long *lp;
{
	return (XDR_LONG(xdrs, lp));
}

/*
 * XDR unsigned long integers
 * same as xdr_long
 */
bool_t
xdr_u_long(xdrs, ulp)
	register XDR *xdrs;
	u_long *ulp;
{
	return (XDR_U_LONG(xdrs, ulp));
}

/*
 * XDR short integers
 */
bool_t
xdr_short(xdrs, sp)
	register XDR *xdrs;
	short *sp;
{
	long l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (long) *sp;
		return (XDR_PUTLONG(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, &l)) {
			return (FALSE);
		}
		*sp = (short) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR unsigned short integers
 */
bool_t
xdr_u_short(xdrs, usp)
	register XDR *xdrs;
	u_short *usp;
{
	u_long l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (u_long) *usp;
		return (XDR_PUTLONG(xdrs, (long *)&l));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, (long *)&l)) {
#ifdef KERNEL
			printf("xdr_u_short: decode FAILED\n");
#endif
			return (FALSE);
		}
		*usp = (u_short) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
#ifdef KERNEL
	printf("xdr_u_short: bad op FAILED\n");
#endif
	return (FALSE);
}


/*
 * XDR a char
 */
bool_t
xdr_char(xdrs, cp)
	XDR *xdrs;
	char *cp;
{
	int i;

	i = (*cp);
	if (! XDR_INT(xdrs, &i)) {
		return (FALSE);
	}
	*cp = i;
	return (TRUE);
}

#ifndef KERNEL
/*
 * XDR an unsigned char
 */
bool_t
xdr_u_char(
	XDR *xdrs,
	u_char *cp)
{
	u_int u;

	u = (*cp);
	if (! XDR_U_INT(xdrs, &u)) {
		return (FALSE);
	}
	*cp = u;
	return (TRUE);
}
#endif /* !KERNEL */

/*
 * XDR booleans
 */
bool_t
xdr_bool(xdrs, bp)
	register XDR *xdrs;
	bool_t *bp;
{
	long lb;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		lb = *bp ? XDR_TRUE : XDR_FALSE;
		return (XDR_PUTLONG(xdrs, &lb));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, &lb)) {
#ifdef KERNEL
			printf("xdr_bool: decode FAILED\n");
#endif
			return (FALSE);
		}
		*bp = (lb == XDR_FALSE) ? FALSE : TRUE;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
#ifdef KERNEL
	printf("xdr_bool: bad op FAILED\n");
#endif
	return (FALSE);
}

/*
 * XDR enumerations
 */
bool_t
xdr_enum(xdrs, ep)
	XDR *xdrs;
	enum_t *ep;
{
#ifndef lint
	enum sizecheck { SIZEVAL };	/* used to find the size of an enum */

	/*
	 * enums are treated as ints
	 */
	if (sizeof (enum sizecheck) == sizeof (long)) {
		return (XDR_LONG(xdrs, (long *)ep));
	} else if (sizeof (enum sizecheck) == sizeof (short)) {
		return (xdr_short(xdrs, (short *)ep));
	} else {
		return (FALSE);
	}
#else
	(void) (xdr_short(xdrs, (short *)ep));
	return (xdr_long(xdrs, (long *)ep));
#endif
}

/*
 * XDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 */
bool_t
xdr_opaque(
	register XDR *xdrs,
	void * cp,
	register u_int cnt)
{
	register u_int rndup;
	static crud[BYTES_PER_XDR_UNIT];

	/*
	 * if no data we are done
	 */
	if (cnt == 0)
		return (TRUE);

	/*
	 * round byte count to full xdr units
	 */
	rndup = cnt % BYTES_PER_XDR_UNIT;
	if ((int) rndup > 0)
		rndup = BYTES_PER_XDR_UNIT - rndup;

	if (xdrs->x_op == XDR_DECODE) {
		if (!XDR_GETBYTES(xdrs, cp, cnt)) {
#ifdef KERNEL
			printf("xdr_opaque: decode FAILED\n");
#endif
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_GETBYTES(xdrs, crud, rndup));
	}

	if (xdrs->x_op == XDR_ENCODE) {
		if (!XDR_PUTBYTES(xdrs, cp, cnt)) {
#ifdef KERNEL
			printf("xdr_opaque: encode FAILED\n");
#endif
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_PUTBYTES(xdrs, xdr_zero, rndup));
	}

	if (xdrs->x_op == XDR_FREE) {
		return (TRUE);
	}

#ifdef KERNEL
	printf("xdr_opaque: bad op FAILED\n");
#endif
	return (FALSE);
}

/*
 * XDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL maxsize bytes are allocated
 */
bool_t
xdr_bytes(xdrs, cpp, sizep, maxsize)
	register XDR *xdrs;
	char **cpp;
	register u_int *sizep;
	u_int maxsize;
{
	register char *sp = *cpp;  /* sp is the actual string pointer */
	register u_int nodesize;

	/*
	 * first deal with the length since xdr bytes are counted
	 * We decided not to use MACRO XDR_U_INT here, because the
	 * advantages here will be miniscule compared to xdr_bytes.
	 * This saved us 100 bytes in the library size.
	 */
	if (! xdr_u_int(xdrs, sizep)) {
#ifdef KERNEL
		printf("xdr_bytes: size FAILED\n");
#endif
		return (FALSE);
	}
	nodesize = *sizep;
	if ((nodesize > maxsize) && (xdrs->x_op != XDR_FREE)) {
#ifdef KERNEL
		printf("xdr_bytes: bad size FAILED\n");
#endif
		return (FALSE);
	}

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {

	case XDR_DECODE:
		if (nodesize == 0) {
			return (TRUE);
		}
		if (sp == NULL) {
			*cpp = sp = (char *)mem_alloc(nodesize);
		}
#ifndef KERNEL
		if (sp == NULL) {
			(void) syslog(LOG_ERR, "xdr_bytes: out of memory");
			return (FALSE);
		}
#endif
		/* fall into ... */

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, nodesize));

	case XDR_FREE:
		if (sp != NULL) {
			mem_free(sp, nodesize);
			*cpp = NULL;
		}
		return (TRUE);
	}
#ifdef KERNEL
	printf("xdr_bytes: bad op FAILED\n");
#endif
	return (FALSE);
}

/*
 * Implemented here due to commonality of the object.
 */
bool_t
xdr_netobj(xdrs, np)
	XDR *xdrs;
	struct netobj *np;
{

	return (xdr_bytes(xdrs, &np->n_bytes, &np->n_len, MAX_NETOBJ_SZ));
}

/*
 * XDR a descriminated union
 * Support routine for discriminated unions.
 * You create an array of xdrdiscrim structures, terminated with
 * an entry with a null procedure pointer.  The routine gets
 * the discriminant value and then searches the array of xdrdiscrims
 * looking for that value.  It calls the procedure given in the xdrdiscrim
 * to handle the discriminant.  If there is no specific routine a default
 * routine may be called.
 * If there is no specific or default routine an error is returned.
 */
bool_t
xdr_union(
	register XDR *xdrs,
	enum_t *dscmp,			/* to decide which arm to work on */
	void *unp,			/* the union itself */
	struct xdr_discrim *choices,	/* [value, xdr proc] for each arm */
	xdrproc_t dfault)		/* default xdr routine */
{
	register enum_t dscm;

	/*
	 * we deal with the discriminator;  it's an enum
	 */
	if (! xdr_enum(xdrs, dscmp)) {
#ifdef KERNEL
		printf("xdr_enum: dscmp FAILED\n");
#endif
		return (FALSE);
	}
	dscm = *dscmp;

	/*
	 * search choices for a value that matches the discriminator.
	 * if we find one, execute the xdr routine for that value.
	 */
	for (; choices->proc != NULL_xdrproc_t; choices++) {
		if (choices->value == dscm)
			return ((*(choices->proc))(xdrs, unp, LASTUNSIGNED));
	}

	/*
	 * no match - execute the default xdr routine if there is one
	 */
	return ((dfault == NULL_xdrproc_t) ? FALSE :
	    (*dfault)(xdrs, unp, LASTUNSIGNED));
}


/*
 * Non-portable xdr primitives.
 * Care should be taken when moving these routines to new architectures.
 */


/*
 * XDR null terminated ASCII strings
 * xdr_string deals with "C strings" - arrays of bytes that are
 * terminated by a NULL character.  The parameter cpp references a
 * pointer to storage; If the pointer is null, then the necessary
 * storage is allocated.  The last parameter is the max allowed length
 * of the string as specified by a protocol.
 */
bool_t
xdr_string(xdrs, cpp, maxsize)
	register XDR *xdrs;
	char **cpp;
	u_int maxsize;
{
	register char *sp = *cpp;  /* sp is the actual string pointer */
	u_int size;
	u_int nodesize;

	/*
	 * first deal with the length since xdr strings are counted-strings
	 */
	switch (xdrs->x_op) {
	case XDR_FREE:
		if (sp == NULL) {
			return (TRUE);	/* already free */
		}
		/* fall through... */
	case XDR_ENCODE:
		size = strlen(sp);
		break;
	}
	/*
	 * We decided not to use MACRO XDR_U_INT here, because the
	 * advantages here will be miniscule compared to xdr_string.
	 * This saved us 100 bytes in the library size.
	 */
	if (! xdr_u_int(xdrs, &size)) {
#ifdef KERNEL
		printf("xdr_string: size FAILED\n");
#endif
		return (FALSE);
	}
	if (size > maxsize) {
#ifdef KERNEL
		printf("xdr_string: bad size FAILED\n");
#endif
		return (FALSE);
	}
	nodesize = size + 1;

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {

	case XDR_DECODE:
		if (nodesize == 0) {
			return (TRUE);
		}
		if (sp == NULL)
			*cpp = sp = (char *)mem_alloc(nodesize);
#ifndef KERNEL
		if (sp == NULL) {
			(void) syslog(LOG_ERR, "xdr_string: out of memory");
			return (FALSE);
		}
#endif
		sp[size] = 0;
		/* fall into ... */

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, size));

	case XDR_FREE:
		mem_free(sp, nodesize);
		*cpp = NULL;
		return (TRUE);
	}
#ifdef KERNEL
	printf("xdr_string: bad op FAILED\n");
#endif
	return (FALSE);
}

#ifndef KERNEL
/*
 * Wrapper for xdr_string that can be called directly from
 * routines like clnt_call
 */
bool_t
xdr_wrapstring(xdrs, cpp)
	XDR *xdrs;
	char **cpp;
{
	if (xdr_string(xdrs, cpp, LASTUNSIGNED)) {
		return (TRUE);
	}
	return (FALSE);
}
#endif /* !KERNEL */
