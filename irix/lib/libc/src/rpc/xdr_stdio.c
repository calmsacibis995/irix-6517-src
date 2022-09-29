/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.17 88/02/08 
 */


/*
 * xdr_stdio.c, XDR implementation on standard i/o file.
 *
 * This set of routines implements a XDR on a stdio stream.
 * XDR_ENCODE serializes onto the stream, XDR_DECODE de-serializes
 * from the stream.
 */

#ifdef __STDC__
	#pragma weak xdrstdio_create = _xdrstdio_create
#endif
#include "synonyms.h"

#include <rpc/types.h>
#include <stdio.h>
#include <rpc/xdr.h>
#include <sys/endian.h>

static void xdrstdio_destroy(register XDR *xdrs);
static bool_t xdrstdio_getlong(XDR *xdrs, register long *lp);
static bool_t xdrstdio_putlong(XDR *xdrs, long *lp);
#if (_MIPS_SZLONG != _MIPS_SZINT)
static bool_t xdrstdio_getint(XDR *xdrs, register int *ip);
static bool_t xdrstdio_putint(XDR *xdrs, int *ip);
#endif
static bool_t xdrstdio_getbytes(XDR *xdrs, void *addr, u_int len);
static bool_t xdrstdio_putbytes(XDR *xdrs, void *addr, u_int len);
static u_int xdrstdio_getpos(XDR *xdrs);
static bool_t xdrstdio_setpos(XDR *xdrs, u_int pos);
static long * xdrstdio_inline(XDR *xdrs, int len);

/*
 * Ops vector for stdio type XDR
 */
static struct xdr_ops	xdrstdio_ops = {
	xdrstdio_getlong,	/* deseraialize a long int */
	xdrstdio_putlong,	/* seraialize a long int */
#if (_MIPS_SZLONG != _MIPS_SZINT)
	xdrstdio_getint,	/* deseraialize an int */
	xdrstdio_putint,	/* seraialize an int */
#endif
	xdrstdio_getbytes,	/* deserialize counted bytes */
	xdrstdio_putbytes,	/* serialize counted bytes */
	xdrstdio_getpos,	/* get offset in the stream */
	xdrstdio_setpos,	/* set offset in the stream */
	xdrstdio_inline,	/* prime stream for inline macros */
	xdrstdio_destroy	/* destroy stream */
};

/*
 * Initialize a stdio xdr stream.
 * Sets the xdr stream handle xdrs for use on the stream file.
 * Operation flag is set to op.
 */
void
xdrstdio_create(register XDR *xdrs, FILE *file, enum xdr_op op)
{

	xdrs->x_op = op;
	xdrs->x_ops = &xdrstdio_ops;
	xdrs->x_private = (caddr_t)file;
	xdrs->x_handy = 0;
	xdrs->x_base = 0;
}

/*
 * Destroy a stdio xdr stream.
 * Cleans up the xdr stream handle xdrs previously set up by xdrstdio_create.
 */
static void
xdrstdio_destroy(register XDR *xdrs)
{
	(void)fflush((FILE *)xdrs->x_private);
	/* xx should we close the file ?? */
}

static bool_t
xdrstdio_getlong(XDR *xdrs, register long *lp)
{
#if (_MIPS_SZLONG != _MIPS_SZINT)
	xdr_long_t mylong;

	if (fread((caddr_t)&mylong, sizeof(xdr_long_t), 1,
			(FILE *)xdrs->x_private) != 1)
		return (FALSE);
	*lp = mylong;
	return (TRUE);
#else
	if (fread((caddr_t)lp, sizeof(long), 1, (FILE *)xdrs->x_private) != 1)
		return (FALSE);
#if BYTE_ORDER == LITTLE_ENDIAN 
	*lp = ntohl(*lp);
#endif
	return (TRUE);
#endif	/* _MIPS_SZLONG == _MIPS_SZINT */
}

static bool_t
xdrstdio_putlong(XDR *xdrs, long *lp)
{

#if (BYTE_ORDER == LITTLE_ENDIAN) || (_MIPS_SZLONG != _MIPS_SZINT)
	xdr_long_t mycopy = (xdr_long_t)htonl(*lp);
	lp = (long *)&mycopy;
#endif
	if (fwrite((caddr_t)lp, sizeof(xdr_long_t), 1, (FILE *)xdrs->x_private) != 1)
		return (FALSE);
	return (TRUE);
}

#if (_MIPS_SZLONG != _MIPS_SZINT)
static bool_t
xdrstdio_getint(XDR *xdrs, register int *ip)
{

	if (fread((caddr_t)ip, sizeof(int), 1, (FILE *)xdrs->x_private) != 1)
		return (FALSE);
#if BYTE_ORDER == LITTLE_ENDIAN 
	*ip = ntohl(*ip);
#endif
	return (TRUE);
}

static bool_t
xdrstdio_putint(XDR *xdrs, int *ip)
{

#if BYTE_ORDER == LITTLE_ENDIAN 
	int mycopy = htonl(*ip);
	ip = &mycopy;
#endif
	if (fwrite((caddr_t)ip, sizeof(int), 1, (FILE *)xdrs->x_private) != 1)
		return (FALSE);
	return (TRUE);
}
#endif	/* _MIPS_SZLONG != _MIPS_SZINT */

static bool_t
xdrstdio_getbytes(XDR *xdrs, void *addr, u_int len)
{

	if ((len != 0) && (fread(addr, (size_t)len, 1, (FILE *)xdrs->x_private) != 1))
		return (FALSE);
	return (TRUE);
}

static bool_t
xdrstdio_putbytes(XDR *xdrs, void *addr, u_int len)
{

	if ((len != 0) && (fwrite(addr, (size_t)len, 1, (FILE *)xdrs->x_private) != 1))
		return (FALSE);
	return (TRUE);
}

static u_int
xdrstdio_getpos(XDR *xdrs)
{

	return ((u_int) ftell((FILE *)xdrs->x_private));
}

static bool_t
xdrstdio_setpos(XDR *xdrs, u_int pos)
{

	return ((fseek((FILE *)xdrs->x_private, (long)pos, 0) < 0) ?
		FALSE : TRUE);
}

/* ARGSUSED */
static long *
xdrstdio_inline(XDR *xdrs, int len)
{

	/*
	 * Must do some work to implement this: must insure
	 * enough data in the underlying stdio buffer,
	 * that the buffer is aligned so that we can indirect through a
	 * long *, and stuff this pointer in xdrs->x_buf.  Doing
	 * a fread or fwrite to a scratch buffer would defeat
	 * most of the gains to be had here and require storage
	 * management on this buffer, so we don't do this.
	 */
	return (NULL);
}
