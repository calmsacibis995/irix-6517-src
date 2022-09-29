/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Seek pf->pf_off bytes from the current stream location, decode pf into ex,
 * and advance skipto bytes from pf's origin.
 */
#include <values.h>
#include "datastream.h"
#include "expr.h"
#include "protocol.h"

int
ds_field(DataStream *ds, ProtoField *pf, int skipto, Expr *ex)
{
	int size;
	int (*seeker)(DataStream *, int, enum dsseekbasis);

	size = pf->pf_size;
	seeker = (size < 0) ? ds_seekbit : ds_seek;
	if (pf->pf_off && !(*seeker)(ds, pf->pf_off, DS_RELATIVE))
		return 0;
	switch (pf->pf_type) {
	  case EXOP_NUMBER:
		if (size > 0) {
			if (!ds_int(ds, &ex->ex_val, size, pf_extendhow(pf)))
				return 0;
		} else {
			size = -size;
			if (!ds_bits(ds, &ex->ex_val, size, pf_extendhow(pf)))
				return 0;
			skipto *= BITSPERBYTE;
		}
		ex->ex_op = EXOP_NUMBER;
		break;
	  case EXOP_ADDRESS:
		if (!ds_bytes(ds, A_BASE(&ex->ex_addr, size), size))
			return 0;
		ex->ex_op = EXOP_ADDRESS;
		break;
	  case EXOP_STRING:
		ex->ex_str.s_ptr = (char *) ds->ds_next;
		if (size == PF_VARIABLE)
			ex->ex_str.s_len = strlen((char *) ds->ds_next);
		else {
			if (!ds_seek(ds, size, DS_RELATIVE))
				return 0;
			ex->ex_str.s_len = size;
		}
		ex->ex_op = EXOP_STRING;
	}
	(void) (*seeker)(ds, skipto - (pf->pf_off + size), DS_RELATIVE);
	return 1;
}
