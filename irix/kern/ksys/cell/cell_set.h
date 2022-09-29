
/*
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ifndef	_KSYS_CELL_SET_H
#define	_KSYS_CELL_SET_H

#include "sys/types.h"

/*
 * Data structures used to represent set of cells.
 * Its actually a bit mask with each bit representing a cell.
 */
typedef unsigned long cell_set_t;

extern cell_set_t	set_universe, set_empty, cmsd_set;

#define	CELL_SET_SIZE	(sizeof(cell_set_t) * NBBY)

#define	set_member(n)		(1 << (uint_t)((n)->index))



static __inline void
set_init(cell_set_t *set)
{
	*set = (cell_set_t)0;
}

static __inline void
set_assign(cell_set_t *set0, cell_set_t *set1)
{
	*set0 = *set1;
}


static __inline void
set_add_member(cell_set_t *set, cell_t cell)
{
	*set |= ((cell_set_t)1 << cell);
}


static __inline void
set_del_member(cell_set_t *set, cell_t cell)
{
	*set &= ~((cell_set_t)1 << cell);
}


static __inline boolean_t
set_is_member(cell_set_t *set, cell_t cell)
{
	return ((*set & ((cell_set_t)1 << cell)) != 0 ? B_TRUE : B_FALSE);
}

static __inline boolean_t
set_is_subset(cell_set_t *set, cell_set_t *subset)
{
	return ((*set | *subset) == *set ? B_TRUE : B_FALSE);
}

static __inline boolean_t
set_equal(cell_set_t *set0, cell_set_t *set1)
{
	return (*set0 == *set1 ? B_TRUE : B_FALSE);
}

static __inline boolean_t
set_intersecting(cell_set_t *set0, cell_set_t *set1)
{
	return (boolean_t)((*set0 & *set1) != 0);
}

static __inline boolean_t
set_is_empty(cell_set_t *set)
{
	return ((*set)? B_FALSE : B_TRUE);
}

static __inline void
set_union(cell_set_t *set0, cell_set_t *set1)
{
	*set0 |= *set1;
}

static __inline void
set_intersect(cell_set_t *set0, cell_set_t *set1)
{
	*set0 &= *set1;
}

static __inline void
set_complement(cell_set_t *cset, cell_set_t *set)
{
	*cset = (~(*set) & set_universe);
}

static __inline void
set_exclude(cell_set_t *src_set, cell_set_t *exclude_set)
{
		
	*src_set &= ~(*exclude_set);
}

static __inline void
set_assign_integer(cell_set_t *set, int o)
{
	*set = (cell_set_t)o;
}

static __inline int
set_member_count(cell_set_t *set)
{
	int	count;
	cell_t	index;

	for (index = 0, count =0; index < CELL_SET_SIZE; index++)
		if (set_is_member(set, index))
			count++;
	return count;
}

static __inline int
set_member_min(cell_set_t *set)
{
	cell_t	index;

	for (index = 0; index < CELL_SET_SIZE; index++)
		if (set_is_member(set, index))
			return index;
	return -1;
}

#endif	/* _KSYS_CELL_SET_H */
