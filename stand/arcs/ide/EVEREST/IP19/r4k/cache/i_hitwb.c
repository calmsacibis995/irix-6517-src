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
 *	File Name: i_hitwb.c
 *
 *	Diag name: i_hitwb
 *
 *	Description: This diag tests the Hit Writeback cache op on the 
 *		     instruction cache.
 *			1. Invalidate the entire I-cache.
 *			2. Initialize memory buffer A with pattern 1.
 *			3. Load the cache with the memory buffer so the
 *			   cache lines change from invalid to clean exclusive.
 *			4. Change the memory buffer to pattern 2.
 *			5. Initialize memory buffer B with pattern 3.
 *			6. Generate miss addresses based on memory buffer B
 *			   address and check if write back happens on a
 *			   cache miss.
 *			7. Do Hit Writeback with the correct address to check
 *			   if write back happens.
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

extern u_int error_data[];

i_hitwb()
{
	register __psunsigned_t addr;
	register u_int 	i;
	register __psunsigned_t buffer_base;
	volatile u_int temp;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (i_hitwb) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/* find the first free memory location at 8K byte boundary */
	buffer_base = PHYS_TO_K1(0x800000);

	/*
	 * Initialize the memory buffer with pattern 0x11111111
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = buffer_base + i;
		* (u_int *)addr = 0x11111111;
#ifdef DEBUG
		msg_printf( DBG, "location %x data %x expect %x\n", addr, *(u_int*)addr,
				0x11111111);
#endif /* DEBUG */
	}

	/*
	 * Load memory into cache. The cache line should change from
	 * invalid to clean exclusive.
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = K1_TO_K0(buffer_base + i);
		
		/* Init the secondary cache, if present */
		temp = *(u_int *)addr;

		Fill_Icache(addr, 1, config_info.iline_size);
		if(ChkITag_CS(addr, 1, PState_Clean_Exclusive, config_info.iline_size)) {
			/*Eprintf("I-cache state error.\n");*/
			err_msg( IHITWB_ERR1, cpu_loc);
			msg_printf( ERR, "Cache state did not change to valid when filled from memory.\n");
			msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected cache state: 0x%08x Actual cache state: 0x%08x.\n",
				PState_Clean_Exclusive, error_data[2]);
			retval = FAIL;
		}
	}

	/*
	 * Change the memory buffer to a background pattern 0x22222222
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = buffer_base + i;
		* (u_int *)addr = 0x22222222;
#ifdef DEBUG
		msg_printf( DBG, "location %x data %x expect %x\n", addr, *(u_int*)addr,
				0x22222222);
#endif /* DEBUG */
	}

	/*
	 * Initialize memory (buffer + 8k) with pattern 0x33333333
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = buffer_base + config_info.icache_size + i;
		* (u_int *)addr = 0x33333333;
#ifdef DEBUG
		msg_printf( DBG, "location %x data %x expect %x\n", addr, *(u_int*)addr,
				0x33333333);
#endif /* DEBUG */
	}

	/* Make sure that Hit Writeback operation does not write back on
	 * a cache miss.
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = K1_TO_K0(buffer_base + config_info.icache_size + i);
		HitWB_I(addr, 1, config_info.iline_size);

		addr = buffer_base + i;
		temp = * (u_int *)addr;
#ifdef DEBUG
		msg_printf( DBG, "location %x data %x\n", addr, temp);
#endif /* DEBUG */
		if (temp != 0x22222222) {
			/*Eprintf("Hit Writeback happened on a miss.\n");*/
			err_msg( IHITWB_ERR2, cpu_loc);
			msg_printf( ERR, "Cache line address: 0x%08x\n", K1_TO_K0(addr));
			msg_printf( ERR, "Miss address: 0x%08x\n", 
				K1_TO_K0(buffer_base + config_info.icache_size + i));
			retval = FAIL;
		}
	}

	/* Make sure that Hit Writeback operation does write back a I-cache
	 * line on a hit.
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = K1_TO_K0(buffer_base + i);
		HitWB_I(addr, 1, config_info.iline_size);
		CreateDE_SD(K1_TO_K0(addr), 1, config_info.sidline_size);
		temp = *(u_int *)addr;

#ifdef DEBUG
		msg_printf( DBG, "location %x data %x\n", K0_TO_K1(addr), temp);
#endif /* DEBUG */
		if (temp != 0x11111111) {
			/*Eprintf("Hit Writeback did not happened on a hit\n");*/
			err_msg( IHITWB_ERR3, cpu_loc);
			msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
			msg_printf( ERR, "expected %x, actual %x, XOR %x\n", 0x11111111,
				temp, (temp ^ 0x11111111) );
			retval = FAIL;
		}
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction hit writeback test\n");
	return(retval);
}

