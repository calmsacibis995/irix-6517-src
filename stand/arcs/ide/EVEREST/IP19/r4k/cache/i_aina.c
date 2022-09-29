/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.7 $"


#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];


/* Use the compliment of the address as data */
#define COMPLIMENT 1

/* write data from low to high */
#define FORWARD 0
i_aina()
{
	register __psunsigned_t last_addr;
	register u_int last_value_read;
	register __psunsigned_t ptr;
	register __psunsigned_t first_addr;
	register u_int expected;
	register volatile u_int tmp;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (i_aina) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	first_addr = PHYS_TO_K1(0x800000);

	last_addr = (first_addr + config_info.icache_size - 4);

	/*
	 * Address tag memory Kseg1
	 */
	filltagW_l((u_int *)first_addr, (u_int *)last_addr, COMPLIMENT, FORWARD);

	/* init secondary cache to clean exclusive, if one exists */
	/* Must be done for fill I to load the data */
	for(ptr = K1_TO_K0(first_addr); ptr < K1_TO_K0(last_addr); ptr +=4) {
			tmp = *(u_int *)ptr;
			msg_printf( DBG, "addr %x mem data %x\n", ptr, *(u_int *)ptr);
	}

#ifdef RICHARD
	for(ptr = K1_TO_K0(first_addr); ptr < K1_TO_K0(first_addr + 0x200); ptr +=4) {
		tmp= IndxLoadTag_SD( ptr);
		msg_printf( DBG, " addr %x  sdtag %x\n", ptr, tmp);
	}
#endif /* RICHARD */

	/*
	 * Fill the I-cache from the secondary/memory
	 */
	Fill_Icache(K1_TO_K0(first_addr), config_info.icache_size / config_info.iline_size, config_info.iline_size);

	/* Fill memory with a background pattern Kseg1 */
	fillmemW( first_addr, last_addr, 0xdeadbeef, 0);

	/* Invalidate the dcache since the read above put it in a clean 
	 * exclusive state
	 */
	InvCache_SD(K0BASE, config_info.dcache_size/ config_info.dline_size, config_info.dline_size);

	/* Write back Icache to memory or Scache if one exists */
	/* Note this sets the Scache state to invalid */
	HitWB_I(K1_TO_K0(first_addr), config_info.icache_size / config_info.iline_size, config_info.iline_size);

	/* Since the above doesn't set the state to dirty/clean in the
	 * secondary we have to dirty it so that the Icache data comes
	 * from the secondary. Otherwise we will cause a miss and the
	 * data will come from memory (deadbeef).
	 */
	CreateDE_SD(K1_TO_K0(first_addr), config_info.sidcache_size/config_info.sidline_size, config_info.sidline_size);
	

#ifdef RICHARD
	for(ptr= first_addr; ptr < first_addr + 0x100; ptr += 4) {
		msg_printf( DBG, "after create addr %x, data %x", ptr, *(u_int *)ptr);
		tmp= IndxLoadTag_SD( ptr);
		msg_printf( DBG, "  sdtag %x\n", tmp);
	}
#endif /* RICHARD */
	
	/*
	 * Verify address tag pattern 
	 */
	expected = (uint)first_addr ;
	for (ptr = K1_TO_K0(first_addr) ; ptr <= K1_TO_K0(last_addr); ptr += 4) {
		if ((last_value_read = *(u_int *)ptr) != ~expected) {
			err_msg( IAINA_ERR1, cpu_loc);
			msg_printf( ERR, "addr %x, exp %x, act %x, XOR %x\n",
				ptr, ~expected, last_value_read,
				~expected ^ last_value_read );

			retval = FAIL;
		}
		expected += 4;
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction data RAM test\n");
	return(retval);

}

