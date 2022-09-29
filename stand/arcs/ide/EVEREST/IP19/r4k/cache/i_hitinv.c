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
 *	File Name: i_hitinv.c
 *
 *	Diag name: i_hitinv()
 *
 *	Description: This diag tests the Hit Invalidate cache op on the
 *		     Instruction cache.
 *			1. Invalidate the primary instruction cache.
 *			2. Fill the I-cache lines from memory with the
 *			   pattern 0xaaaaaaaa. The cache state should be
 *			   valid.
 *			3. Check if Hit Invalidate cache op invalidates a
 *			   cache line on a miss.
 *			4. Check if Hit Invalidate does invalidate the cache
 *			   line on a hit.
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


i_hitinv()
{
	register __psunsigned_t addr;
	register u_int 	i;
	register __psunsigned_t address;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (i_hitinv) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	address = K1_TO_K0(0xa0800000);

	/* Dirty the I-cache blocks, the cache state should be valid
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = address + i;
		* (u_int *) K0_TO_K1(addr) = 0xaaaaaaaa;
		Fill_Icache(addr, 1, config_info.iline_size);

		/* Check if the cache state changed to valid from invalid
		 */
		if(ChkITag_CS(addr, 1, PState_Clean_Exclusive, config_info.iline_size)) {
			/*Eprintf("I-cache state error.\n");*/
			err_msg( IHITINV_ERR1, cpu_loc);
			msg_printf( ERR, "Cache state did not change to valid when filled from memory.\n");
			msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected cache state: 0x%08x Actual cache state: 0x%08x.\n",
				PState_Clean_Exclusive, error_data[2]);
			retval = FAIL;
		}
	}

	/* Make sure a miss has no effect on the Hit_Invalidate operation
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = address + config_info.icache_size + i;
		HitInv_I(addr, 1, config_info.iline_size);

		/* Check if the cache state changed to invalid on a cache
		 * miss.
		 */
		if(ChkITag_CS(addr, 1, PState_Clean_Exclusive, config_info.iline_size)) {
			/*Eprintf("I-cache state error\n");*/
			err_msg( IHITINV_ERR2, cpu_loc);
			msg_printf( ERR, "Hit Invalidate changed the line to invalid on a miss.\n");
			msg_printf( ERR, "Cache line address: 0x%08x\n", address + i);
			msg_printf( ERR, "Miss address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected cache state: 0x%08x Actual cache state: 0x%08x.\n",
				PState_Clean_Exclusive, error_data[2]);
			retval = FAIL;
		}
	}

	/*
	 * Check if the Hit Invalidate operation does invalid the cache
	 * block at a hit.
	 */
	for(i = 0; i < config_info.icache_size; i += config_info.iline_size) {
		addr = address + i;
		HitInv_I(addr, 1, config_info.iline_size);
		if(ChkITag_CS(addr, 1, PState_Invalid, config_info.iline_size)) {
			/*Eprintf("I-cache state error\n");*/
			err_msg( IHITINV_ERR3, cpu_loc);
			msg_printf( ERR, "Hit Invalidate did not invalidate the line on a hit.\n");
			msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected cache state: 0x%08x Actual cache state: 0x%08x.\n",
				PState_Invalid, error_data[2]);
			retval = FAIL;
		}
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary instruction hit invalidate test\n");
	return(retval);
}

