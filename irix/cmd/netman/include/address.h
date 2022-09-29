#ifndef ADDRESS_H
#define ADDRESS_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Generic protocol address.  Machine Dependent: We assume that the
 * native byte order is big-endian.
 */

typedef union address {
	unsigned char		a_vec[2 * sizeof(long)];
	struct {
		unsigned long	ai_high;
		unsigned long	ai_low;
	} a_int;
} Address;

#define	a_high	a_int.ai_high
#define	a_low	a_int.ai_low

#define	A_CAST(a, type) \
	((type *) A_BASE(a, sizeof(type)))

#define	A_BASE(a, len) \
	(&(a)->a_vec[sizeof (a)->a_vec - (len)])

#define	A_INIT(a, vec, len) { \
	int off = sizeof (a)->a_vec - (len); \
	bcopy((char *)(vec), (a)->a_vec + off, (len)); \
	if (off) \
		bzero((a)->a_vec, off); \
}

#endif
