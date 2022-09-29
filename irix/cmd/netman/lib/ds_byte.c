/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Get or put a datastream byte.
 */
#include "datastream.h"

int
ds_byte(DataStream *ds, char *cp)
{
	if (ds->ds_bitoff || ds->ds_count <= 0)
		return 0;
	if (ds->ds_direction == DS_DECODE)
		*cp = *ds->ds_next;
	else
		*ds->ds_next = *cp;
	--ds->ds_count;
	ds->ds_next++;
	return 1;
}
