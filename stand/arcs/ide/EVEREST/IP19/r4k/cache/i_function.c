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

#ident "$Revision: 1.6 $"


/*=============================================================================
 *
 *	File Name: i_function.c
 *
 *	Diag name: i_function
 *
 *	Description: This diag tests the functionality of the entire 
 *		     instruction cache. It checks the block fill and 
 *		     hit write back of the instruction cache lines.
 *			1. Invalidate the entire I-cache.
 *			2. Write memory buffer A locations with their
 *			   addresses.
 *			3. Fill the cache from memory buffer A with block
 *			   fill operation.
 *			4. Change memory buffer A to pattern 0xdeadbeef.
 *			5. Write back the cache lines with the Hit
 *			   Writeback cache op.
 *			6. Check if the cache lines are written back to
 *			   memory buffer A with the correct data.
 *
 *===========================================================================*/


#include "sys/types.h"
#include "coher.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

i_function()
{
	register __psunsigned_t addr;
	register u_int i;
	volatile u_int temp;
	register __psunsigned_t address;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (i_function) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	address = PHYS_TO_K1(0x800000);

	/*
	 * Fill memory buffer A based at "address" with address as 
	 * background pattern
	 */
	for(i = 0; i < config_info.icache_size; i += 4) {
		addr = address + i;
		* (u_int *) addr = (uint)(K1_TO_K0( addr));
	}

	/*
	 * Load memory buffer A into cache. This should cause the cache lines
	 * to become valid. Note Secondary if present also gets primed.
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = K1_TO_K0(address + i);

		temp = * (u_int *)addr;

		Fill_Icache(addr, 1, config_info.iline_size);
	}

	ide_invalidate_caches();
	/*
	 * Fill memory buffer A based at "address" with pattern 0xdeadbeef
	 */
	for(i = 0; i < config_info.icache_size; i += 4) {
		addr = address + i;
		* (u_int *) addr = 0xdeadbeef;
	}

	/*
	 * Write back cache to memory buffer A by using Hit Writeback
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = K1_TO_K0(address + i);
		HitWB_I(addr, 1, config_info.iline_size);

		/* Get the secondary in a clean exclusve state after HITWB
		 * Note, create_de does not work with config = non-conherent
		 * instead the line is made clean exclusive. BUG in chip
		 */
		CreateDE_SD( addr, 1, config_info.sidline_size);
	}

	/* 
	 * If a secondary cache exist, then read from Kseq0 which should
	 * hit in the secondary and not memory. Else check the data in
	 * memory when no SC exists.
	 */

	address = K1_TO_K0( address);
	/*
	 * Check if the entire cache is written back to memory buffer A.
	 */
	for(i = 0; i < config_info.icache_size; i += 4) {
		addr = address + i;
		temp = * (u_int *) addr;
		if (temp != addr) {
			/*Eprintf("I-cache block write back error.\n");*/
			err_msg( IFUNCT_ERR1, cpu_loc);
			msg_printf( ERR, "Memory contains incorrect data.\n");
			msg_printf( ERR, "Cache address: 0x%08x\n", K1_TO_K0(addr));
			msg_printf( ERR, "Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x.\n",
				K1_TO_K0(addr), temp, K1_TO_K0(addr) ^ temp);
			msg_printf( ERR, "Icache TAG = %x\n", IndxLoadTag_I( addr) );
			
			msg_printf( ERR, "Scache TAG = %x\n", IndxLoadTag_SD( addr) );
			retval = FAIL;
		}
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction functionality test\n");
	return(retval);
}

