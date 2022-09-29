/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Bitset implementation.
 */
#include <bstring.h>
#include <values.h>
#include "bitset.h"
#include "heap.h"
#include "macros.h"

#define	UIBITS	BITS(unsigned int)

BitSet *
bitset(unsigned int len)
{
	unsigned int veclen, size;
	BitSet *bs;

	veclen = HOWMANY(len, UIBITS);
	size = veclen * sizeof bs->bs_vec[0];
	bs = xnew(BitSet, size - sizeof bs->bs_vec[0]);
	bs->bs_len = len;
	bs->bs_veclen = veclen;
	bzero(bs->bs_vec, size);
	return bs;
}

int
memberbit(BitSet *bs, unsigned int bit)
{
	if (bit >= bs->bs_len)
		return 0;
	return bs->bs_vec[bit / UIBITS] & (1 << (bit % UIBITS));
}

int
firstbit(BitSet *bs)
{
	int n;
	unsigned int *u, w;

	n = 0;
	for (u = bs->bs_vec; *u == 0; u++) {
		if (++n == bs->bs_veclen)
			return -1;
	}
	n *= UIBITS;
	for (w = *u; (w & 1) == 0; w >>= 1)
		n++;
	return n;
}

int
insertbit(BitSet *bs, unsigned int bit)
{
	if (bit >= bs->bs_len)
		return 0;
	bs->bs_vec[bit / UIBITS] |= (1 << (bit % UIBITS));
	return 1;
}

int
removebit(BitSet *bs, unsigned int bit)
{
	if (bit >= bs->bs_len)
		return 0;
	bs->bs_vec[bit / UIBITS] &= ~(1 << (bit % UIBITS));
	return 1;
}

BitSet *
assignbits(BitSet *lbs, BitSet *rbs)
{
	unsigned int i;

	if (lbs->bs_len != rbs->bs_len)
		return 0;
	for (i = 0; i < lbs->bs_veclen; i++)
		lbs->bs_vec[i] = rbs->bs_vec[i];
	return lbs;
}

BitSet *
unionbits(BitSet *bs, BitSet *lbs, BitSet *rbs)
{
	unsigned int i;

	if (bs->bs_len != lbs->bs_len || bs->bs_len != rbs->bs_len)
		return 0;
	for (i = 0; i < bs->bs_veclen; i++)
		bs->bs_vec[i] = lbs->bs_vec[i] | rbs->bs_vec[i];
	return bs;
}

BitSet *
intersectbits(BitSet *bs, BitSet *lbs, BitSet *rbs)
{
	unsigned int i;

	if (bs->bs_len != lbs->bs_len || bs->bs_len != rbs->bs_len)
		return 0;
	for (i = 0; i < bs->bs_veclen; i++)
		bs->bs_vec[i] = lbs->bs_vec[i] & rbs->bs_vec[i];
	return bs;
}

int
equalbits(BitSet *lbs, BitSet *rbs)
{
	unsigned int i;

	if (lbs->bs_len != rbs->bs_len)
		return 0;
	for (i = 0; i < lbs->bs_veclen; i++) {
		if (lbs->bs_vec[i] != rbs->bs_vec[i])
			return 0;
	}
	return 1;
}

void
destroybitset(BitSet *bs)
{
	delete(bs);
}
