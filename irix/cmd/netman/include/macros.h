#ifndef MACROS_H
#define MACROS_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Common macros.
 */

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define	HOWMANY(n, u)	(((n)+(u)-1)/(u))
#define	ROUNDUP(n, u)	(HOWMANY(n, u) * (u))

/*
 * Make a single compile-time identifier from two identifiers.
 */
#define	makeident2(x,y)	x##y

/*
 * Compute the length (number of elements) of a vector of known size.
 */
#define	lengthof(v)	(sizeof(v) / sizeof((v)[0]))

/*
 * Bit-sizeof type or variable.  Requires <values.h>.
 */
#define	bitsizeof(t)	(sizeof(t) * BITSPERBYTE)

/*
 * Compute the length of a string constant or a char array of known size.
 */
#define	constrlen(s)	(sizeof(s) - 1)

/*
 * Given a struct tag and member name, return the member's byte offset.
 */
#define	structoff(tag,mem) \
	((char *) &((struct tag *) 0)->mem - (char *) 0)

/*
 * Macro to calculate the ceiling-log2 of an integer.
 */
#define	LOG2CEIL(n, c) { \
	unsigned long x = n; \
	c = (x & x-1) ? 1 : 0; \
	if (x & 0xffff0000) \
		c += 16, x &= 0xffff0000; \
	if (x & 0xff00ff00) \
		c += 8, x &= 0xff00ff00; \
	if (x & 0xf0f0f0f0) \
		c += 4, x &= 0xf0f0f0f0; \
	if (x & 0xcccccccc) \
		c += 2, x &= 0xcccccccc; \
	if (x & 0xaaaaaaaa) \
		c += 1; \
}

#endif
