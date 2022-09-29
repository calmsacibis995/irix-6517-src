/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.20 88/02/08 
 */


/*
 * xdr_mem.h, XDR implementation using memory buffers.
 *
 * If you have some data to be interpreted as external data representation
 * or to be converted to external data representation in a memory buffer,
 * then this is the package for you.
 *
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
	#pragma weak xdrmem_create = _xdrmem_create
#endif
#include "synonyms.h"
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <netinet/in.h>
#include <bstring.h>		/* prototype for bcopy() */
#endif


static bool_t xdrmem_getlong(register XDR *xdrs, long *lp);
static bool_t xdrmem_putlong(register XDR *xdrs, long *lp);
static bool_t xdrmem_getbytes(XDR *, void *, u_int);
static bool_t xdrmem_putbytes(XDR *, void *, u_int);
static u_int xdrmem_getpos(register XDR *xdrs);
static bool_t xdrmem_setpos(register XDR *xdrs, u_int pos);
static long * xdrmem_inline(register XDR *xdrs, int len);
static void xdrmem_destroy(XDR *xdrs);
#if (_MIPS_SZLONG != _MIPS_SZINT)
static bool_t xdrmem_getint(register XDR *xdrs, int *ip);
static bool_t xdrmem_putint(register XDR *xdrs, int *ip);
#endif

static struct	xdr_ops xdrmem_ops = {
	xdrmem_getlong,
	xdrmem_putlong,
#if (_MIPS_SZLONG != _MIPS_SZINT)
	xdrmem_getint,
	xdrmem_putint,
#endif
	xdrmem_getbytes,
	xdrmem_putbytes,
	xdrmem_getpos,
	xdrmem_setpos,
	xdrmem_inline,
	xdrmem_destroy
};

/*
 * The procedure xdrmem_create initializes a stream descriptor for a
 * memory buffer.  
 */
void
xdrmem_create(register XDR *xdrs, void * addr, u_int size, enum xdr_op op)
{

	xdrs->x_op = op;
	xdrs->x_ops = &xdrmem_ops;
	xdrs->x_private = xdrs->x_base = addr;
	xdrs->x_handy = (int)size;
}

/*ARGSUSED*/
static void
xdrmem_destroy(XDR *xdrs)
{
}

static bool_t
xdrmem_getlong(register XDR *xdrs, long *lp)
{

	if ((xdrs->x_handy -= sizeof(xdr_long_t)) < 0)
		return (FALSE);
	*lp = (long)ntohl((u_long)(*((xdr_long_t *)(xdrs->x_private))));
	xdrs->x_private += sizeof(xdr_long_t);
	return (TRUE);
}

static bool_t
xdrmem_putlong(register XDR *xdrs, long *lp)
{

	if ((xdrs->x_handy -= sizeof(xdr_long_t)) < 0)
		return (FALSE);
	*(xdr_long_t *)xdrs->x_private = (xdr_long_t)htonl((u_long)(*lp));
	xdrs->x_private += sizeof(xdr_long_t);
	return (TRUE);
}

#if (_MIPS_SZLONG != _MIPS_SZINT)
static bool_t
xdrmem_getint(
	register XDR *xdrs,
	int *ip)
{

	if ((xdrs->x_handy -= sizeof(int)) < 0)
		return (FALSE);
	*ip = (int)ntohl((u_int)(*((int *)(xdrs->x_private))));
	xdrs->x_private += sizeof(int);
	return (TRUE);
}

static bool_t
xdrmem_putint(
	register XDR *xdrs,
	int *ip)
{

	if ((xdrs->x_handy -= sizeof(int)) < 0)
		return (FALSE);
	*(int *)xdrs->x_private = (int)htonl((u_int)(*ip));
	xdrs->x_private += sizeof(int);
	return (TRUE);
}
#endif	/* _MIPS_SZLONG != _MIPS_SZINT */

static bool_t
xdrmem_getbytes(register XDR *xdrs, void *addr, register u_int len)
{

	if ((xdrs->x_handy -= len) < 0)
		return (FALSE);
	bcopy(xdrs->x_private, addr, (int)len);
	xdrs->x_private += len;
	return (TRUE);
}

static bool_t
xdrmem_putbytes(register XDR *xdrs, void *addr, register u_int len)
{

	if ((xdrs->x_handy -= len) < 0)
		return (FALSE);
	bcopy(addr, xdrs->x_private, (int)len);
	xdrs->x_private += len;
	return (TRUE);
}

static u_int
xdrmem_getpos(register XDR *xdrs)
{

	return (u_int)((caddr_t)xdrs->x_private - (caddr_t)xdrs->x_base);
}

static bool_t
xdrmem_setpos(register XDR *xdrs, u_int pos)
{
	register caddr_t newaddr = xdrs->x_base + pos;
	register caddr_t lastaddr = xdrs->x_private + xdrs->x_handy;

	if ((__psint_t)newaddr > (__psint_t)lastaddr)
		return (FALSE);
	xdrs->x_private = newaddr;
	xdrs->x_handy = (int)(lastaddr - newaddr);
	return (TRUE);
}

static long *
xdrmem_inline(register XDR *xdrs, int len)
{
	long *buf = 0;

	if (xdrs->x_handy >= len) {
		xdrs->x_handy -= len;
		buf = (long *) xdrs->x_private;
		xdrs->x_private += len;
	}
	return (buf);
}
