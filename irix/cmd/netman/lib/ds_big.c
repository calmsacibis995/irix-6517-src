/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Big endian datastream implementation.  Machine Dependent: assumes
 * native representations are big endian, IEEE floating point.
 */
#include <values.h>
#include "datastream.h"
#include "macros.h"

DefineDataStreamOperations(_big_endian_dsops, beds, DS_BIG_ENDIAN);

static int
beds_bits(DataStream *ds, long *lp, unsigned int len, enum dsextendhow how)
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
		from = ds->ds_next, to = (unsigned char *) &word;
		while (--maxbytes >= 0)
			*to++ = *from++;
		word <<= ds->ds_bitoff;
		if (how == DS_ZERO_EXTEND)
			word = (unsigned long) word >> justify;
		else
			word >>= justify;
		*lp = word;
	} else {
		word = ((unsigned long)*lp << justify) >> ds->ds_bitoff;
		from = (unsigned char *) &word, to = ds->ds_next;
		if (maxbytes > 1) {
			*to++ |= *from++;
			--maxbytes;
		}
		while (--maxbytes > 0)
			*to++ = *from++;
		*to |= *from;
	}

	ds->ds_count = newcount;
	ds->ds_next += minbytes;
	ds->ds_bitoff = newbitoff;
	return 1;
}

static int
beds_short(DataStream *ds, short *sp)
{
	int newcount;

	if (ds->ds_bitoff)
		return 0;
	if ((unsigned long) ds->ds_next % sizeof *sp)
		return ds_bytes(ds, (char *) sp, sizeof *sp);
	newcount = ds->ds_count - sizeof *sp;
	if (newcount < 0)
		return 0;
	if (ds->ds_direction == DS_DECODE)
		*sp = *(short *)ds->ds_next;
	else
		*(short *)ds->ds_next = *sp;
	ds->ds_count = newcount;
	ds->ds_next += sizeof *sp;
	return 1;
}

static int
beds_long(DataStream *ds, long *lp)
{
	int newcount;

	if (ds->ds_bitoff)
		return 0;
#if defined(__mips) || defined(sparc)
	if ((unsigned long) ds->ds_next % sizeof *lp)
#endif
#if defined(m68000) || (defined(sun) && !defined(sparc))
	if ((int) ds->ds_next % sizeof(short))
#endif
		return ds_bytes(ds, (char *) lp, sizeof *lp);
	newcount = ds->ds_count - sizeof *lp;
	if (newcount < 0)
		return 0;
	if (ds->ds_direction == DS_DECODE)
		*lp = *(long *)ds->ds_next;
	else
		*(long *)ds->ds_next = *lp;
	ds->ds_count = newcount;
	ds->ds_next += sizeof *lp;
	return 1;
}

static int
beds_float(DataStream *ds, float *fp)
{
	return beds_long(ds, (long *) fp);
}

static int
beds_double(DataStream *ds, double *dp)
{
	int newcount;

	if (ds->ds_bitoff)
		return 0;
#if defined(__mips) || defined(sparc)
	if ((unsigned long) ds->ds_next % sizeof *dp)
#endif
#if defined(m68000) || (defined(sun) && !defined(sparc))
	if ((int) ds->ds_next % sizeof(short))
#endif
		return ds_bytes(ds, (char *) dp, sizeof *dp);
	newcount = ds->ds_count - sizeof *dp;
	if (newcount < 0)
		return 0;
	if (ds->ds_direction == DS_DECODE)
		*dp = *(double *)ds->ds_next;
	else
		*(double *)ds->ds_next = *dp;
	ds->ds_count = newcount;
	ds->ds_next += sizeof *dp;
	return 1;
}
