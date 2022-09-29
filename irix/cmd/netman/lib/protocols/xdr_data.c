/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * XDR/datastream hybrid mutant monster.
 */
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "datastream.h"

bool_t	xdrdata_long(XDR *, long *);
bool_t	xdrdata_bytes(XDR *, void *, u_int);
u_int	xdrdata_getpos(XDR *);
bool_t	xdrdata_setpos(XDR *, u_int);
long	*xdrdata_inline(XDR *, int);
void	xdrdata_destroy(XDR *);

static struct xdr_ops xdrdata_ops = {
	xdrdata_long,
	xdrdata_long,
	xdrdata_bytes,
	xdrdata_bytes,
	xdrdata_getpos,
	xdrdata_setpos,
	xdrdata_inline,
	xdrdata_destroy,
};

void
xdrdata_create(XDR *xdr, DataStream *ds)
{
	switch (ds->ds_direction) {
	  case DS_DECODE:
		xdr->x_op = XDR_DECODE;
		break;
	  case DS_ENCODE:
		xdr->x_op = XDR_ENCODE;
	}
	xdr->x_ops = &xdrdata_ops;
	xdr->x_private = (caddr_t) ds;
}

#define	DS(xdr)	((DataStream *) (xdr)->x_private)

static bool_t
xdrdata_long(XDR *xdr, long *lp)
{
	return ds_long(DS(xdr), lp);
}

static bool_t
xdrdata_bytes(XDR *xdr, void *p, u_int len)
{
	return ds_bytes(DS(xdr), p, len);
}

static u_int
xdrdata_getpos(XDR *xdr)
{
	return DS_TELL(DS(xdr));
}

static bool_t
xdrdata_setpos(XDR *xdr, u_int pos)
{
	return ds_seek(DS(xdr), pos, DS_ABSOLUTE);
}

static long *
xdrdata_inline(XDR *xdr, int len)
{
	return (long *) ds_inline(DS(xdr), len, BYTES_PER_XDR_UNIT);
}

/* ARGSUSED */
static void
xdrdata_destroy(XDR *xdr)
{
}
