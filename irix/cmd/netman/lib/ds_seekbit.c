/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Bitwise seek and tell for datastreams.
 */
#include <values.h>
#include "datastream.h"
#include "macros.h"

int
ds_seekbit(DataStream *ds, int bitoff, enum dsseekbasis basis)
{
	int bitrem, bitcount, bitsize;
	int exact;

	if (basis == DS_ABSOLUTE)
		bitoff -= DS_TELLBIT(ds);
	bitrem = ds->ds_count * BITSPERBYTE - ds->ds_bitoff;
	bitcount = bitrem - bitoff;
	bitsize = ds->ds_size * BITSPERBYTE;
	if (bitcount < 0) {
		bitcount = 0;
		bitoff = bitrem;
		exact = 0;
	} else if (bitcount > bitsize) {
		bitcount = bitsize;
		bitoff = bitrem - bitsize;
		exact = 0;
	} else
		exact = 1;
	ds->ds_count = HOWMANY(bitcount, BITSPERBYTE);
	ds->ds_next += bitoff / BITSPERBYTE;
	ds->ds_bitoff += bitoff % BITSPERBYTE;
	if (ds->ds_bitoff < 0) {
		ds->ds_bitoff += BITSPERBYTE;
		--ds->ds_next;
	} else if (ds->ds_bitoff >= BITSPERBYTE) {
		ds->ds_bitoff -= BITSPERBYTE;
		ds->ds_next++;
	}
	return exact;
}

int
ds_tellbit(DataStream *ds)
{
	return DS_TELLBIT(ds);
}
