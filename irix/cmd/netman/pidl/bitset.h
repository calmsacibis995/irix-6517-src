#ifndef BITSET_H
#define BITSET_H
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Bitsets are used in dataflow analysis to represent sets of fields.
 */

typedef struct bitset {
	unsigned int	bs_len;		/* maximum cardinality */
	unsigned int	bs_veclen;	/* number of words in bs_vec */
	unsigned int	bs_vec[1];	/* actually, 1 or more */
} BitSet;

extern BitSet	*bitset(unsigned int);
extern int	memberbit(BitSet *, unsigned int);
extern int	firstbit(BitSet *);
extern int	insertbit(BitSet *, unsigned int);
extern int	removebit(BitSet *, unsigned int);
extern BitSet	*assignbits(BitSet *, BitSet *);
extern BitSet	*unionbits(BitSet *, BitSet *, BitSet *);
extern BitSet	*intersectbits(BitSet *, BitSet *, BitSet *);
extern int	equalbits(BitSet *, BitSet *);
extern void	destroybitset(BitSet *);

#endif
