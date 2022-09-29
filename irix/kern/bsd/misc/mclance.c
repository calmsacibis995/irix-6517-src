/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * mclance.c --
 *
 *	Common routines shared by the SGI EN1 (if_en.c) and 
 *	CMC ENP-10 (if_enp.c) drivers that manipulate the AMD 7990 LANCE 
 *	ethernet chip's logical address filter.
 *
 */
#ident "$Revision: 1.6 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <net/multi.h>
#include "mclance.h"

#define TRUE  1
#define FALSE 0

/*
 * CalcIndex --
 *
 *	Given a multicast ethernet address, this routine calculates the 
 *	address's bit index in the logical address filter mask for the
 *	LANCE chip.
 *
 *	(Modified from CMC "User's Guide K1 Kernel Software For Ethernet
 *	Node Processors", May 1986, Appendix D)
 */

mval_t
CalcIndex(unsigned char *addr,
    int len)
{
    unsigned int  crc = ~0;
    unsigned char x;
    int i;
    int j;
#define CRC_MASK	0xEDB88320

    for (i = 0; i < len; i++, addr++) {
	x = *addr;
	for (j = 0; j < 8; j++) {
	    if ((x ^ crc) & 1) {
		crc = (crc >> 1) ^ CRC_MASK;
	    } else {
		crc = (crc >> 1);
	    }
	    x >>= 1;
	}
    }
    return((mval_t) (crc >> 26));
}


/*
 * AddMulti --
 *
 *	Determines if the multicast address needs to be added to the 
 *	active mcast address table and if the LANCE chip's mcast filter 
 *	needs to be updated.
 *
 *	Returns:
 *	 0	if successful
 *	 ENOMEM if the address table is full.
 *
 *	 If *updateChip is TRUE, then the filter has been updated and 
 *	 the caller must copy it to the chip.
 */

int
AddMulti(keyPtr, table, filter, updateChip)
    register union mkey *keyPtr;	/* contains the mcast address. */
    struct mfilter	*table;		/* active mcast addresses. */
    Mcast_Filter	filter;		/* copy of chip's address filter. */
    int			*updateChip;	/* Set TRUE if the key is first one
					 * to map to calcuated index bit. */
{
    mval_t index;

    /*
     * If the address is not already in use, then add it to the
     * table of addresses. Calculate the bit # for the logical
     * address filter and if it's not already set, set *updateChip to TRUE
     * so the filter's value on the board will be changed by caller.
     */

    *updateChip = FALSE;
    if (!mfmatch(table, keyPtr, &index)) {
	index = CalcIndex(keyPtr->mk_dhost, sizeof(keyPtr->mk_dhost));
	if (!mfhasvalue(table, index)) {
	    SET_MCAST_FILTER_BIT(filter, index);
	    *updateChip = TRUE;
	}
	if (!mfadd(table, keyPtr, index)) {
	    return(ENOMEM);
	} 
    }
    return(0);
}


/*
 * DeleteMulti --
 *
 *	Inverse of AddMulti: deletes the address from the active table
 *	and determines if the LANCE chip's filter should be changed.
 *
 *	Returns:
 *	 TRUE	if the filter should be givn to the board by the caller.
 *	 FALSE  the board does not have to be updated.
 */

int
DeleteMulti(keyPtr, table, filter)
    register union mkey *keyPtr;
    struct mfilter	*table;
    Mcast_Filter	filter;
{
    mval_t index;

    if (mfmatch(table, keyPtr, &index)) {
	ASSERT(index == CalcIndex(keyPtr->mk_dhost,sizeof(keyPtr->mk_dhost)));
	mfdel(table, keyPtr);

	/*
	 * If this multicast address is the last one to map the
	 * bit index in the filter, clear the bit and indicate that the
	 * board's filter needs to be changed.
	 */
	if (!mfhasvalue(table, index)) {
	    CLR_MCAST_FILTER_BIT(filter, index);
	    return(TRUE);
	}
    }
    return(FALSE);
}
