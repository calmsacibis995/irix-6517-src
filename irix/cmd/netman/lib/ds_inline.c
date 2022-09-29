/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Fast access to datastream buffers.
 */
#include <sys/types.h>
#include "datastream.h"

char *
ds_inline(DataStream *ds, unsigned int len, int align)
{
	char *p;
	int trymask, newcount;

	if (ds->ds_bitoff)
		return 0;
	p = (char *) ds->ds_next;
	trymask = align - 1;
	if (!(align & trymask) ? (u_long) p & trymask : (u_long) p % align)
		return 0;
	newcount = ds->ds_count - len;
	if (newcount < 0)
		return 0;
	ds->ds_count = newcount;
	ds->ds_next += len;
	return p;
}
