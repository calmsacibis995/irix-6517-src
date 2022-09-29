/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Raw filter compilation.
 */
#include <values.h>
#include <sys/types.h>
#include <net/raw.h>
#include "datastream.h"
#include "expr.h"
#include "protocol.h"

void
pc_init(ProtoCompiler *pc,
	struct snoopfilter *sf,
	int offset,
	enum dsbyteorder order,
	unsigned short *errflags,
	ExprError *err)
{
	ds_init(&pc->pc_mask, (unsigned char *) sf->sf_mask + offset,
		sizeof sf->sf_mask - offset, DS_ENCODE, order);
	ds_init(&pc->pc_match, (unsigned char *) sf->sf_match + offset,
		sizeof sf->sf_match - offset, DS_ENCODE, order);
	pc->pc_errflags = errflags;
	pc->pc_error = err;
}

void
pc_stop(ProtoCompiler *pc)
{
	(void) ds_seekbit(&pc->pc_mask, pc->pc_mask.ds_size * BITSPERBYTE,
		DS_ABSOLUTE);
	(void) ds_seekbit(&pc->pc_match, pc->pc_match.ds_size * BITSPERBYTE,
		DS_ABSOLUTE);
}

enum dsbyteorder
pc_setbyteorder(ProtoCompiler *pc, enum dsbyteorder order)
{
	(void) ds_setbyteorder(&pc->pc_mask, order);
	return ds_setbyteorder(&pc->pc_match, order);
}

int
pc_byte(ProtoCompiler *pc, char mask, char match)
{
	return ds_byte(&pc->pc_match, &match)
	    && ds_byte(&pc->pc_mask, &mask);
}

int
pc_bytes(ProtoCompiler *pc, char *mask, char *match, int len)
{
	char allones;

	if (!ds_bytes(&pc->pc_match, match, len))
		return 0;
	if (mask != PC_ALLBYTES)
		return ds_bytes(&pc->pc_mask, mask, len);
	allones = 0xff;
	while (--len >= 0)
		if (!ds_byte(&pc->pc_mask, &allones))
			return 0;
	return 1;
}

int
pc_bits(ProtoCompiler *pc, long mask, long match, int len)
{
	return ds_bits(&pc->pc_match, &match, len, DS_ZERO_EXTEND)
	    && ds_bits(&pc->pc_mask, &mask, len, DS_ZERO_EXTEND);
}

int
pc_short(ProtoCompiler *pc, short mask, short match)
{
	return ds_short(&pc->pc_match, &match)
	    && ds_short(&pc->pc_mask, &mask);
}

int
pc_long(ProtoCompiler *pc, long mask, long match)
{
	return ds_long(&pc->pc_match, &match)
	    && ds_long(&pc->pc_mask, &mask);
}

int
pc_int(ProtoCompiler *pc, long mask, long match, int size)
{
	switch (size) {
	  case sizeof(long):
		if (!pc_long(pc, mask, match))
			return 0;
		break;
	  case sizeof(short):
		if (!pc_short(pc, (short) mask, (short) match))
			return 0;
		break;
	  case sizeof(char):
		if (!pc_byte(pc, (char) mask, (char) match))
			return 0;
		break;
	  default:
		return 0;
	}
	return 1;
}

/*
 * Skip to pf->pf_off bits or bytes, depending on whether pf is a bitfield
 * or an integer; compile a comparison using mask and match; and advance
 * at least skipto bytes from the field's origin.
 */
int
pc_intfield(ProtoCompiler *pc, ProtoField *pf, long mask, long match,
	    int skipto)
{
	int size, skip;
	int (*skipper)(), (*compiler)();

	if (pf->pf_size == PF_VARIABLE)
		return 0;
	size = pf->pf_size;
	if (size > 0) {
		skipper = pc_skipbytes;
		compiler = pc_int;
	} else {
		skipper = pc_skipbits;
		compiler = pc_bits;
		size = -size;
		skipto *= BITSPERBYTE;
	}
	if (pf->pf_off && !(*skipper)(pc, pf->pf_off))
		return 0;
	if (!(*compiler)(pc, mask, match, size))
		return 0;
	skip = skipto - (pf->pf_off + size);
	if (skip > 0)
		(void) (*skipper)(pc, skip);
	return 1;
}

/*
 * Skip over pf->pf_off bytes, mask the field described by pf and compare
 * against match, and advance skipto bytes from pf's origin.
 */
int
pc_bytefield(ProtoCompiler *pc, ProtoField *pf, char *mask, char *match,
	     int skipto)
{
	int skip;

	if (pf->pf_size == PF_VARIABLE || pf->pf_size < 0)
		return 0;
	if (pf->pf_off && !pc_skipbytes(pc, pf->pf_off))
		return 0;
	if (!pc_bytes(pc, mask, match, pf->pf_size))
		return 0;
	skip = skipto - (pf->pf_off + pf->pf_size);
	if (skip > 0)
		(void) pc_skipbytes(pc, skip);
	return 1;
}

int
pc_skipbytes(ProtoCompiler *pc, int len)
{
	return ds_seek(&pc->pc_match, len, DS_RELATIVE)
	    && ds_seek(&pc->pc_mask, len, DS_RELATIVE);
}

int
pc_skipbits(ProtoCompiler *pc, int len)
{
	return ds_seekbit(&pc->pc_match, len, DS_RELATIVE)
	    && ds_seekbit(&pc->pc_mask, len, DS_RELATIVE);
}

/*
 * Given a mask expression (null means exact match), validate and return
 * via maskp the integer mask value.
 */
int
pc_intmask(ProtoCompiler *pc, Expr *mex, long *maskp)
{
	if (mex == 0) {
		*maskp = PC_ALLINTBITS;
		return 1;
	}
	if (mex->ex_op == EXOP_NUMBER) {
		*maskp = mex->ex_val;
		return 1;
	}
	ex_error(mex, "non-integer mask", pc->pc_error);
	return 0;
}

/*
 * Common error code for protocol compile functions.
 */
void
pc_badop(ProtoCompiler *pc, Expr *ex)
{
	ex_badop(ex, pc->pc_error);
}

/*
 * Compilation error reporter.
 */
void
pc_error(ProtoCompiler *pc, Expr *ex, char *message)
{
	ex_error(ex, message, pc->pc_error);
}
