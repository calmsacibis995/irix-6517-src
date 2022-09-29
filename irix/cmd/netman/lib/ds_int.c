/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Get or put a datastream integer.
 */
#include "datastream.h"

int
ds_int(DataStream *ds, long *lp, int size, enum dsextendhow how)
{
	switch (size) {
	  case sizeof(long):
		if (!ds_long(ds, lp))
			return 0;
		break;
	  case sizeof(short): {
		short s;

		if (!ds_short(ds, &s))
			return 0;
		*lp = (how == DS_ZERO_EXTEND) ? (unsigned short) s : s;
		break;
	  }
	  case sizeof(char): {
		signed char c;

		if (!ds_byte(ds, (char *) &c))
			return 0;
		*lp = (how == DS_ZERO_EXTEND) ? (unsigned char) c : c;
		break;
	  }
	  default:
		return 0;
	}
	return 1;
}
