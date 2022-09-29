/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Get or put datastream bytes.
 */
#include <bstring.h>
#include "datastream.h"

int
ds_bytes(DataStream *ds, void *p, unsigned int len)
{
	int newcount;

	if (ds->ds_bitoff)
		return 0;
	newcount = ds->ds_count - len;
	if (newcount < 0)
		return 0;
	if (ds->ds_direction == DS_DECODE)
		bcopy(ds->ds_next, p, len);
	else
		bcopy(p, ds->ds_next, len);
	ds->ds_count = newcount;
	ds->ds_next += len;
	return 1;
}
