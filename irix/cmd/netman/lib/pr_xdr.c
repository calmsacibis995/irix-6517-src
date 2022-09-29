/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Xdr_matchresult is an allocating xdr filter used by fragment caches
 * that hold the result expressions of network protocol match routines.
 * Calls to match later fragments find the cached result.  Upon hitting
 * the cache for the last fragment, or if the cached entry times out,
 * xdr_matchresult is called to free memory.  Network protocols should
 * use savematchresult to make a cache copy of the result expression.
 *
 * Xdr_decodestate is an allocating xdr filter used to decode, encode,
 * and free protocol-specific decoding state used by higher layers to
 * keep track of fragments.  Network layer protocols store decode state
 * records in caches between decode invocations.  After the last frag
 * is decoded or when the cache entry expires, cache remove code calls
 * xdr_decodestate to free memory.
 */
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "debug.h"
#include "expr.h"
#include "heap.h"
#include "protocol.h"
#include "protostack.h"
#include "strings.h"

Expr *
savematchresult(Expr *rex)
{
	Expr *mex;

	mex = expr(rex->ex_op, rex->ex_arity, rex->ex_token);
	mex->ex_u = rex->ex_u;
	if (mex->ex_op == EXOP_STRING) {
		mex->ex_str.s_ptr =
			strndup(rex->ex_str.s_ptr, rex->ex_str.s_len);
	}
	return mex;
}

int
xdr_matchresult(XDR *xdr, Expr **exp)
{
	Expr *ex, rex;

	ex = *exp;
	if (xdr->x_op == XDR_FREE) {
		if (ex->ex_op == EXOP_STRING)
			delete(ex->ex_str.s_ptr);
		ex_destroy(ex);
		*exp = 0;
		return 1;
	}

	assert(ex || xdr->x_op == XDR_DECODE);
	if (xdr->x_op == XDR_DECODE && ex == 0) {
		ex = &rex;
		ex_null(ex);
	}

	if (!xdr_enum(xdr, (enum_t *) &ex->ex_op))
		return 0;
	switch (ex->ex_op) {
	  case EXOP_NUMBER:
		if (!xdr_long(xdr, &ex->ex_val))
			return 0;
		break;
	  case EXOP_ADDRESS:
		if (!xdr_opaque(xdr, (char *) &ex->ex_addr,
				sizeof ex->ex_addr)) {
			return 0;
		}
		break;
	  case EXOP_STRING:
		if (!xdr_bytes(xdr, &ex->ex_str.s_ptr,
			       (u_int *) &ex->ex_str.s_len, (u_int) -1)) {
			return 0;
		}
	}

	if (ex == &rex) {
		*exp = ex = new(Expr);
		*ex = rex;
	}
	return 1;
}

int
xdr_decodestate(XDR *xdr, ProtoDecodeState **pdsp)
{
	ProtoDecodeState *pds;
	unsigned int size, prid;
	Protocol *pr;

	pds = *pdsp;
	if (xdr->x_op == XDR_FREE) {
		delete(pds);
		*pdsp = 0;
		return 1;
	}

	assert(pds || xdr->x_op == XDR_DECODE);
	if (xdr->x_op == XDR_ENCODE) {
		size = pds->pds_mysize;
		pr = pds->pds_proto;
		prid = pr->pr_id;
	}
	if (!xdr_u_int(xdr, &size) || !xdr_u_int(xdr, &prid))
		return 0;
	if (xdr->x_op == XDR_DECODE) {
		if (size <= sizeof *pds)
			return 0;
		pr = findprotobyid(prid);
		if (pr == 0)
			return 0;
		if (pds == 0)
			*pdsp = pds = tnew(ProtoDecodeState, size);
		pds->pds_mysize = size;
		pds->pds_proto = pr;
	}

	return (*pr->pr_xdrstate)(xdr, pds);
}
