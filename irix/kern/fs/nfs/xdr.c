/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.36 88/02/08
 */


/*
 * xdr.c, Generic XDR routines implementation.
 *
 * These are the "generic" xdr routines used to serialize and de-serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */

#ifdef _KERNEL
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/kmem.h"
#include "types.h"
#include "xdr.h"
#include "string.h"
#include "nfs.h"
#else
#ifdef __STDC__
	#pragma weak xdr_free       = _xdr_free
	#pragma weak xdr_void       = _xdr_void
	#pragma weak xdr_int        = _xdr_int
	#pragma weak xdr_u_int      = _xdr_u_int
	#pragma weak xdr_long       = _xdr_long
	#pragma weak xdr_longlong   = _xdr_longlong_t
	#pragma weak xdr_longlong_t = _xdr_longlong_t
	#pragma weak xdr_hyper	    = _xdr_longlong_t
	#pragma weak xdr_u_long     = _xdr_u_long
	#pragma weak xdr_short      = _xdr_short
	#pragma weak xdr_u_short    = _xdr_u_short
	#pragma weak xdr_char       = _xdr_char
	#pragma weak xdr_u_char     = _xdr_u_char
	#pragma weak xdr_bool       = _xdr_bool
	#pragma weak xdr_enum       = _xdr_enum
	#pragma weak xdr_opaque     = _xdr_opaque
	#pragma weak xdr_bytes      = _xdr_bytes
	#pragma weak xdr_netobj     = _xdr_netobj
	#pragma weak xdr_union      = _xdr_union
	#pragma weak xdr_string     = _xdr_string
	#pragma weak xdr_time_t     = _xdr_time_t
	#pragma weak xdr_wrapstring = _xdr_wrapstring
	#pragma weak xdr_uint64	    = _xdr_u_longlong_t
	#pragma weak xdr_int64	    = _xdr_longlong_t
	#pragma weak xdr_uint32	    = _xdr_uint32
	#pragma weak xdr_int32	    = _xdr_int32
	#pragma weak xdr_u_longlong	    = _xdr_u_longlong_t
	#pragma weak xdr_u_longlong_t	    = _xdr_u_longlong_t
	#pragma weak xdr_u_hyper    = _xdr_u_longlong_t
#endif
#include "synonyms.h"
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/errorhandler.h>
#include <string.h>		/* prototype for strlen() */
#endif

#ifdef _KERNEL
#define _INITBSS
#define _INITBSSS
#endif

/*
 * constants specific to the xdr "protocol"
 */
#define XDR_FALSE	((long) 0)
#define XDR_TRUE	((long) 1)
#define LASTUNSIGNED	((u_int) 0-1)

/*
 * for unit alignment
 */
static const char xdr_zero[BYTES_PER_XDR_UNIT] = { 0, 0, 0, 0 };

#ifndef _KERNEL
/*
 * Free a data structure using XDR
 * Not a filter, but a convenient utility nonetheless
 */
void
xdr_free(xdrproc_t proc, void *objp)
{
	XDR x;
	
	x.x_op = XDR_FREE;
	(*(p_xdrproc_t)proc)(&x, objp);
}
#endif

/*
 * XDR nothing
 */
/* ARGSUSED */
bool_t
xdr_void(XDR *xdrs, void *addr)
{

	return (TRUE);
}

/*
 * XDR integers
 */
#if (_MIPS_SZLONG != _MIPS_SZINT)
bool_t
xdr_int(register XDR *xdrs, int *ip)
{

	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT(xdrs, ip));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT(xdrs, ip));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

/*
 * XDR unsigned integers
 */
bool_t
xdr_u_int(register XDR *xdrs, u_int *uip)
{

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT(xdrs, (int *)uip));
	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT(xdrs, (int *)uip));
	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

#else	/* _MIPS_SZLONG == _MIPS_SZINT */

bool_t
xdr_int(XDR *xdrs, int *ip)
{

#ifdef lint
	(void) (xdr_short(xdrs, (short *)ip));
	return (xdr_long(xdrs, (long *)ip));
#else
	return (xdr_long(xdrs, (long *)ip));
#endif
}

/*
 * XDR unsigned integers
 */
bool_t
xdr_u_int(XDR *xdrs, u_int *up)
{

#ifdef lint
	(void) (xdr_short(xdrs, (short *)up));
	return (xdr_u_long(xdrs, (u_long *)up));
#else
	return (xdr_u_long(xdrs, (u_long *)up));
#endif
}
#endif	/* _MIPS_SZLONG == _MIPS_SZINT */

/*
 * XDR long integers
 * same as xdr_u_long - open coded to save a proc call!
 */

/*
 * NOTE that for 64 bit software, the xdr representation of a long
 * and a u_long, is 32 bits. The reasons for this:
 *	All existing protocols expect only 32-bit data. Obviously
 *	we cannot change what is going into rpc packets.
 *
 *	All existing code makes liberal use of xdr_long, XDR_PUTLONG, etc.
 *	To change the behavior of xdr_long to put out 64 bits would require
 *	that all code that is to be compiled for 64 bits would have to be
 *	changed to use xdr_int, XDR_PUTINT, etc.
 *
 *	Moreover, all the data types being used as arguments to those
 *	xdr calls would have to be changed from long/u_long to int/u_int.
 *	That is not desirable, and in many cases would violate our 64bit
 *	abi, or supported API's.
 *
 *	Any 64 bit xdr user that wants to interoperate with 32 bit programs
 *	will be able to without changing any current code.
 */
bool_t
xdr_long(register XDR *xdrs, long *lp)
{
	if (xdrs->x_op == XDR_ENCODE) {
#ifndef _KERNEL
#if _MIPS_SZLONG == 64
	    /*
	     * Check for a long which can be encoded into 32 bits.
	     */
	    if ((*lp & 0xFFFFFFFF00000000) == 0 || 
		(*lp & 0xFFFFFFFF80000000) == 0xFFFFFFFF80000000) 
#endif
#endif
		return (XDR_PUTLONG(xdrs, lp));
	}

#if _MIPS_SZLONG == 64
	if (xdrs->x_op == XDR_DECODE) {
		/* explicitly sign-extend the 32bit value */
		int e;
		int32_t l;

		e = XDR_GETINT(xdrs, &l);
		*lp = l;
		return e;
	}
#else
	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETLONG(xdrs, lp));
#endif

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

/*
 * XDR unsigned long integers
 * same as xdr_long - open coded to save a proc call!
 */
bool_t
xdr_u_long(register XDR *xdrs, u_long *ulp)
{
#if _MIPS_SZLONG == 64
	if (xdrs->x_op == XDR_DECODE) {
		/* explicitly 0-extend the returned value */
		int e;
		u_int32_t ul;

		e = XDR_GETINT(xdrs, (int *)&ul);
		*ulp = ul;
		return e;
	}
#else
	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETLONG(xdrs, (long *)ulp));
#endif
	if (xdrs->x_op == XDR_ENCODE) {
#ifndef _KERNEL
#if _MIPS_SZLONG == 64
	    /*
	     * Check for a ulong which can be encoded into 32 bits.
	     */
	    if ((*ulp & 0xFFFFFFFF00000000) == 0) 
#endif
#endif
		return (XDR_PUTLONG(xdrs, (long *)ulp));
	}
	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

bool_t
xdr_time_t(XDR *xdrs, time_t *tp)
{

#ifdef lint
	(void) (xdr_short(xdrs, (short *)tp));
	(void) (xdr_int(xdrs, (int *)tp));
	return (xdr_long(xdrs, (long *)tp));
#else
#if (_MIPS_SZLONG == 32)
	return (xdr_long(xdrs, (long *)tp));
#elif (_MIPS_SZLONG == 64)
	return (xdr_int(xdrs, (int *)tp));
#else
BOMB!!
#endif

#endif
}

/*
 * XDR short integers
 */
bool_t
xdr_short(register XDR *xdrs, short *sp)
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
xdr_u_short(register XDR *xdrs, u_short *usp)
{
	u_long l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (u_long) *usp;
		return (XDR_PUTLONG(xdrs, (long *)&l));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, (long *)&l)) {
			return (FALSE);
		}
		*usp = (u_short) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}


/*
 * XDR a char
 */
bool_t
xdr_char(XDR *xdrs, char *cp)
{
	int i;

	i = (*cp);
	if (!xdr_int(xdrs, &i)) {
		return (FALSE);
	}
	*cp = (char)i;
	return (TRUE);
}

#ifndef _KERNEL
/*
 * XDR an unsigned char
 */
bool_t
xdr_u_char(XDR *xdrs, u_char *cp)
{
	u_int u;

	u = (*cp);
	if (!xdr_u_int(xdrs, &u)) {
		return (FALSE);
	}
	*cp = (u_char)u;
	return (TRUE);
}
#endif /* !KERNEL */

/*
 * XDR booleans
 */
bool_t
xdr_bool(register XDR *xdrs, bool_t *bp)
{
	long lb;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		lb = *bp ? XDR_TRUE : XDR_FALSE;
		return (XDR_PUTLONG(xdrs, &lb));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, &lb)) {
			return (FALSE);
		}
		*bp = (lb == XDR_FALSE) ? FALSE : TRUE;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR enumerations
 */
bool_t
xdr_enum(XDR *xdrs, enum_t *ep)
{
#ifndef lint
#if (_MIPS_SZINT != 32)
BOMB!!
#endif

#if (_MIPS_SZLONG == 32)
	return (xdr_long(xdrs, (long *)ep));
#elif (_MIPS_SZLONG == 64)
	return (xdr_int(xdrs, (int *)ep));
#else
BOMB!!!
#endif
#ifdef NEVER
	enum sizecheck { SIZEVAL };	/* used to find the size of an enum */

	/*
	 * enums are treated as ints
	 */
	if (sizeof (enum sizecheck) == sizeof (long)) {
		return (xdr_long(xdrs, (long *)ep));
	} else if (sizeof (enum sizecheck) == sizeof (short)) {
		return (xdr_short(xdrs, (short *)ep));
#if (_MIPS_SZLONG != _MIPS_SZINT)
	} else if (sizeof (enum sizecheck) == sizeof (int)) {
		return (xdr_int(xdrs, (int *)ep));
#endif
	} else {
		return (FALSE);
	}
#endif /* NEVER */
#else /* lint */
	(void) (xdr_short(xdrs, (short *)ep));
	(void) (xdr_int(xdrs, (int *)ep));
	return (xdr_long(xdrs, (long *)ep));
#endif
}

/*
 * XDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 */
bool_t
xdr_opaque(register XDR *xdrs, void * cp, register u_int cnt)
{
	register u_int rndup;
	char crud[BYTES_PER_XDR_UNIT];			/* 4 bytes */

	/*
	 * if no data we are done
	 */
	if (cnt == 0)
		return (TRUE);

	/*
	 * round byte count to full xdr units
	 */
	rndup = cnt % BYTES_PER_XDR_UNIT;
	if (rndup > 0)
		rndup = BYTES_PER_XDR_UNIT - rndup;

	if (xdrs->x_op == XDR_DECODE) {
		if (!XDR_GETBYTES(xdrs, cp, cnt)) {
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_GETBYTES(xdrs, crud, rndup));
	}

	if (xdrs->x_op == XDR_ENCODE) {
		if (!XDR_PUTBYTES(xdrs, cp, cnt)) {
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_PUTBYTES(xdrs, (char *)xdr_zero, rndup));
	}

	if (xdrs->x_op == XDR_FREE) {
		return (TRUE);
	}

	return (FALSE);
}

/*
 * XDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL maxsize bytes are allocated
 */
bool_t
xdr_bytes(register XDR *xdrs, char **cpp, register u_int *sizep, u_int maxsize)
{
	register char *sp = *cpp;  /* sp is the actual string pointer */
	register u_int nodesize;

	/*
	 * first deal with the length since xdr bytes are counted
	 */
	if (! xdr_u_int(xdrs, sizep)) {
		return (FALSE);
	}
	nodesize = *sizep;
	if ((nodesize > maxsize) && (xdrs->x_op != XDR_FREE)) {
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
#ifndef _KERNEL
		if (sp == NULL) {
			_rpc_errorhandler(LOG_ERR, "xdr_bytes: out of memory");
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
	return (FALSE);
}

/*
 * Implemented here due to commonality of the object.
 */
bool_t
xdr_netobj(XDR *xdrs, struct netobj *np)
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
	enum_t *dscmp,		/* enum to decide which arm to work on */
	void *unp,		/* the union itself */
	struct xdr_discrim *choices,	/* [value, xdr proc] for each arm */
	xdrproc_t dfault)	/* default xdr routine */
{
	register enum_t dscm;

	/*
	 * we deal with the discriminator;  it's an enum
	 */
	if (! xdr_enum(xdrs, dscmp)) {
		return (FALSE);
	}
	dscm = *dscmp;

	/*
	 * search choices for a value that matches the discriminator.
	 * if we find one, execute the xdr routine for that value.
	 */
	for (; choices->proc != NULL_xdrproc_t; choices++) {
		if (choices->value == dscm)
			return ((*((xdrproc3_t)choices->proc))(xdrs, unp, LASTUNSIGNED));
	}

	/*
	 * no match - execute the default xdr routine if there is one
	 */
	return ((dfault == NULL_xdrproc_t) ? FALSE :
	    (*(xdrproc3_t)dfault)(xdrs, unp, LASTUNSIGNED));
}


/*
 * Non-portable xdr primitives.
 * Care should be taken when moving these routines to new architectures.
 */

#ifdef _KERNEL
zone_t	*nfs_xdr_string_zone;
#endif

/*
 * XDR null terminated ASCII strings
 * xdr_string deals with "C strings" - arrays of bytes that are
 * terminated by a NULL character.  The parameter cpp references a
 * pointer to storage; If the pointer is null, then the necessary
 * storage is allocated.  The last parameter is the max allowed length
 * of the string as specified by a protocol.
 */
bool_t
xdr_string(register XDR *xdrs, char **cpp, u_int maxsize)
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
			return(TRUE);	/* already free */
		}
		/* fall through... */
	case XDR_ENCODE:
		size = (int)strlen(sp);
		break;
	}
	if (! xdr_u_int(xdrs, &size)) {
		return (FALSE);
	}
	if (size > maxsize) {
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
		if (sp == NULL) {
#ifdef _KERNEL
			if (nodesize <= NFS_MAXNAMLEN) {
				*cpp = sp = (char *)kmem_zone_alloc(nfs_xdr_string_zone, KM_SLEEP);
			} else {
				*cpp = sp = (char *)mem_alloc(nodesize);
			}
#else
			*cpp = sp = (char *)mem_alloc(nodesize);
#endif
		}
#ifndef _KERNEL
		if (sp == NULL) {
			_rpc_errorhandler(LOG_ERR, "xdr_string: out of memory");
			return (FALSE);
		}
#endif
		sp[size] = 0;
		/* fall into ... */

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, size));

	case XDR_FREE:
#ifdef _KERNEL
		if (nodesize <= NFS_MAXNAMLEN) {
			kmem_zone_free(nfs_xdr_string_zone, sp);
		} else {
			mem_free(sp, nodesize);
		}
#else
		mem_free(sp, nodesize);
#endif
		*cpp = NULL;
		return (TRUE);
	}
	return (FALSE);
}

/*
 * These routines were written for the 32-bit kernel and are being
 * included in the 32/64 bit merge.
 */

bool_t
xdr_uint32(register XDR *xdrs, u_int *objp)
{
    return (xdr_u_int(xdrs, objp));
}

bool_t
xdr_int32(register XDR *xdrs, int *objp)
{
    return (xdr_int(xdrs, objp));
}

bool_t
xdr_u_longlong_t(register XDR *xdrs, __uint64_t *objp)
{
	uint	*objp1 = (uint *)objp;
	int	status;

/* Slightly wierd - there is no XDR_PUTINT if MIPS_SZLONG == MIPS_SZINT. */
#if (_MIPS_SZLONG != _MIPS_SZINT)
#define _XDR_PUTHALF(xdrs, ptr)		XDR_PUTINT(xdrs, (int *)ptr)
#define _XDR_GETHALF(xdrs, ptr)		XDR_GETINT(xdrs, (int *)ptr)
#else
#define _XDR_PUTHALF(xdrs, ptr)		XDR_PUTLONG(xdrs, (long *)(ptr))
#define _XDR_GETHALF(xdrs, ptr)		XDR_GETLONG(xdrs, (long *)(ptr))
#endif

	switch (xdrs->x_op) {

	case XDR_ENCODE:
	    if (!_XDR_PUTHALF(xdrs, objp1)) {
		return (FALSE);
	    }
	    objp1++;
	    status = _XDR_PUTHALF(xdrs, objp1);
	    return (status);

	case XDR_DECODE:
	    if (!_XDR_GETHALF(xdrs, objp1)) {
		return (FALSE);
	    }
	    objp1++;
	    status = _XDR_GETHALF(xdrs, objp1);
	    return (status);

	case XDR_FREE:
	    return (TRUE);
	}
	return (FALSE);

#undef _XDR_PUTHALF
#undef _XDR_GETHALF
}


bool_t
xdr_longlong_t(register XDR *xdrs, __int64_t *objp)
{
	int	*objp1 = (int *)objp;
	int	status;

/* Slightly wierd - there is no XDR_PUTINT if MIPS_SZLONG == MIPS_SZINT. */
#if (_MIPS_SZLONG != _MIPS_SZINT)
#define _XDR_PUTHALF(xdrs, ptr)		XDR_PUTINT(xdrs, (int *)ptr)
#define _XDR_GETHALF(xdrs, ptr)		XDR_GETINT(xdrs, (int *)ptr)
#else
#define _XDR_PUTHALF(xdrs, ptr)		XDR_PUTLONG(xdrs, (long *)(ptr))
#define _XDR_GETHALF(xdrs, ptr)		XDR_GETLONG(xdrs, (long *)(ptr))
#endif

	switch (xdrs->x_op) {

	case XDR_ENCODE:
	    if (!_XDR_PUTHALF(xdrs, objp1)) {
		return (FALSE);
	    }
	    objp1++;
	    status = _XDR_PUTHALF(xdrs, objp1);
	    return (status);

	case XDR_DECODE:
	    if (!_XDR_GETHALF(xdrs, objp1)) {
		return (FALSE);
	    }
	    objp1++;
	    status = _XDR_GETHALF(xdrs, objp1);
	    return (status);

	case XDR_FREE:
	    return (TRUE);
	}
	return (FALSE);

#undef _XDR_PUTHALF
#undef _XDR_GETHALF
}

#ifndef _KERNEL
/* 
 * Wrapper for xdr_string that can be called directly from 
 * routines like clnt_call
 */
bool_t
xdr_wrapstring(XDR *xdrs, char **cpp)
{
	if (xdr_string(xdrs, cpp, LASTUNSIGNED)) {
		return (TRUE);
	}
	return (FALSE);
}
#endif /* !_KERNEL */
