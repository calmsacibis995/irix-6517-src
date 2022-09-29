/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * mclance.h --
 *
 *	Definitions for the LANCE multicast filters used by the ENP 
 *	and EN1 drivers.
 *
 */
#ident "$Revision: 1.5 $"

#ifndef __mclance__
#define __mclance__

/*
 * 64-bit logical address filter to accept or reject multicast addresses.
 */
typedef unsigned short Mcast_Filter[4];

#define SET_MCAST_FILTER_BIT(filter, bit) \
	(filter)[bit >> 4] |= (1 << ((bit) & 0xF))
#define CLR_MCAST_FILTER_BIT(filter, bit) \
	(filter)[bit >> 4] &= ~(1 << ((bit) & 0xF))

extern int AddMulti(union mkey *, struct mfilter *, Mcast_Filter, int *);
extern int DeleteMulti(union mkey *, struct mfilter *, Mcast_Filter);
extern mval_t CalcIndex(unsigned char *addr, int len);

#endif /* __mclance__ */
