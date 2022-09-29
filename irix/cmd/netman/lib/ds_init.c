/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Datastream initialization.
 */
#include "datastream.h"

extern struct dsops _big_endian_dsops;
extern struct dsops _little_endian_dsops;

static void
setbyteorder(DataStream *ds, enum dsbyteorder neworder)
{
	switch (neworder) {
	  case DS_BIG_ENDIAN:
		ds->ds_ops = &_big_endian_dsops;
		break;
	  case DS_LITTLE_ENDIAN:
		ds->ds_ops = &_little_endian_dsops;
	}
}

void
ds_init(DataStream *ds, unsigned char *p, int len, enum dsdirection direction,
	enum dsbyteorder order)
{
	setbyteorder(ds, order);
	ds->ds_direction = direction;
	ds->ds_size = ds->ds_count = len;
	ds->ds_bitoff = 0;
	ds->ds_next = (unsigned char *) p;
}

enum dsbyteorder
ds_setbyteorder(DataStream *ds, enum dsbyteorder neworder)
{
	enum dsbyteorder oldorder;

	oldorder = ds->ds_ops->dso_byteorder;
	if (neworder != oldorder)
		setbyteorder(ds, neworder);
	return oldorder;
}
