/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Little endian datastream implementation.  Machine dependent: assumes
 * that native byte order is big endian.
 */
#include <values.h>
#include "datastream.h"
#include "macros.h"

DefineDataStreamOperations(_little_endian_dsops, leds, DS_LITTLE_ENDIAN);

static int
leds_bits(DataStream *ds, long *lp, unsigned int len, enum dsextendhow how)
{
	long word;			/* longword containing the bits */
	int minbytes, maxbytes;		/* floor and ceiling byte lengths */
	int newcount, newbitoff;	/* updated ds members */
	int justify;			/* shift to justify field in word */
	unsigned char *from, *to;	/* byte copy pointers */

	if (ds->ds_bitoff + len > bitsizeof(word))
		return 0;

	minbytes = (ds->ds_bitoff + len) / BITSPERBYTE;
	maxbytes = HOWMANY(ds->ds_bitoff + len, BITSPERBYTE);
	newcount = ds->ds_count - minbytes;
	if (newcount < 0)
		return 0;
	newbitoff = (ds->ds_bitoff + len) % BITSPERBYTE;
	if (newcount == 0 && newbitoff)
		return 0;

	justify = bitsizeof(word) - len;
	if (ds->ds_direction == DS_DECODE) {
		from = ds->ds_next, to = (unsigned char *) &word + maxbytes;
		while (--maxbytes >= 0)
			*--to = *from++;
		word <<= ds->ds_bitoff;
		if (how == DS_ZERO_EXTEND)
			word = (unsigned long) word >> justify;
		else
			word >>= justify;
		*lp = word;
	} else {
		word = ((unsigned long)*lp << justify) >> ds->ds_bitoff;
		from = (unsigned char *) &word + maxbytes, to = ds->ds_next;
		if (maxbytes > 1) {
			*to++ |= *--from;
			--maxbytes;
		}
		while (--maxbytes > 0)
			*to++ = *--from;
		*to |= *from;
	}

	ds->ds_count = newcount;
	ds->ds_next += minbytes;
	ds->ds_bitoff = newbitoff;
	return 1;
}

static int
leds_short(DataStream *ds, short *sp)
{
	int newcount;

	if (ds->ds_bitoff)
		return 0;
	newcount = ds->ds_count - sizeof *sp;
	if (newcount < 0)
		return 0;
	if (ds->ds_direction == DS_DECODE)
		*sp = ds->ds_next[1] << BITSPERBYTE | ds->ds_next[0];
	else {
		ds->ds_next[0] = ((char *)sp)[1];
		ds->ds_next[1] = ((char *)sp)[0];
	}
	ds->ds_count = newcount;
	ds->ds_next += sizeof *sp;
	return 1;
}

static int
leds_long(DataStream *ds, long *lp)
{
	int newcount;

	if (ds->ds_bitoff)
		return 0;
	newcount = ds->ds_count - sizeof *lp;
	if (newcount < 0)
		return 0;
	if (ds->ds_direction == DS_DECODE) {
		*lp = ((ds->ds_next[3] << BITSPERBYTE
			| ds->ds_next[2]) << BITSPERBYTE
			| ds->ds_next[1]) << BITSPERBYTE
			| ds->ds_next[0];
	} else {
		ds->ds_next[0] = ((char *)lp)[3];
		ds->ds_next[1] = ((char *)lp)[2];
		ds->ds_next[2] = ((char *)lp)[1];
		ds->ds_next[3] = ((char *)lp)[0];
	}
	ds->ds_count = newcount;
	ds->ds_next += sizeof *lp;
	return 1;
}

static int
leds_float(DataStream *ds, float *fp)
{
	return leds_long(ds, (long *) fp);
}

static int
leds_double(DataStream *ds, double *dp)
{
	return leds_long(ds, (long *)dp + 1)
	    && leds_long(ds, (long *)dp);
}
