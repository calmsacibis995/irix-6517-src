/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Seek and tell for datastreams.
 */
#include "datastream.h"

int
ds_seek(DataStream *ds, int offset, enum dsseekbasis basis)
{
	int newcount;
	int exact;

	if (basis == DS_ABSOLUTE)
		offset -= DS_TELL(ds);
	newcount = ds->ds_count - offset;
	if (newcount < 0) {
		newcount = 0;
		offset = ds->ds_count;
		exact = 0;
	} else if (newcount > ds->ds_size) {
		newcount = ds->ds_size;
		offset = ds->ds_count - newcount;
		exact = 0;
	} else
		exact = 1;
	ds->ds_count = newcount;
	ds->ds_next += offset;
	return exact;
}

int
ds_tell(DataStream *ds)
{
	return DS_TELL(ds);
}
