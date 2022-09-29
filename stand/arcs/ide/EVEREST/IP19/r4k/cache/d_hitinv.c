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
 *	File Name: d_hitinv.c
 *
 *	Diag name: d_hitinv
 *
 *	Description: This diag tests the Hit Invalidate cache op on the
 *		     data cache.
 *			1. Invalidate the entire cache.
 *			2. The first word of each cache line is written
 *			   to change the cache state to dirty exclusive.
 *			3. Miss addresses are used in the Hit Invalidate 
 *			   cache op to ensure the tag comparator generates
 *			   a miss compare and the cache state is not changed.
 *			4. The correct addresses are used in the Hit
 *			   Invalidate cache op to ensure that the cache lines
 *			   are invalidated on a hit.
 *			
 *===========================================================================*/


#include "sys/types.h"
#include "coher.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <ip19.h>
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

extern u_int error_data[];


dd_hitinv()
{
	register __psunsigned_t addr;
	register u_int 	i;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (d_hitinv) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/* Dirty the d-cache blocks, the cache state should be
	 * dirty exclusive.
	 */
	for(i = 0; i < config_info.dcache_size; i += config_info.dline_size) {
		addr = K0BASE + i;
		* (u_int *)addr = 0xaaaaaaaa;
		if(ChkDTag_CS(addr, 1, PState_Dirty_Exclusive, config_info.dline_size)) {
			/*Eprintf("D-cache state error.\n");*/
			err_msg( DHITINV_ERR1, cpu_loc);
			msg_printf( ERR, "Cache state did not change to valid when filled from memory.\n");
			msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected cache state: 0x%08x Actual cache state: 0x%08x.\n",
				PState_Dirty_Exclusive, error_data[2]);
			retval = FAIL;
		}
	}

	/* Make sure a miss has no effect on the Hit_Invalidate operation
	 */
	for(i = 0; i < config_info.dcache_size; i += config_info.dline_size) {
		addr = K0BASE + config_info.dcache_size + i;
		HitInv_D(addr, 1, config_info.dline_size);
		if(ChkDTag_CS(addr, 1, PState_Dirty_Exclusive, config_info.dline_size)) {
			/*Eprintf("D-cache state error\n");*/
			err_msg( DHITINV_ERR2, cpu_loc);
			msg_printf( ERR, "Hit Invalidate changed the line to invalid on a miss.\n");
			msg_printf( ERR, "Cache line address: 0x%08x\n", K0BASE + i);
			msg_printf( ERR, "Miss address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected cache state: 0x%08x Actual cache state: 0x%08x.\n",
				PState_Dirty_Exclusive, error_data[2]);
			retval = FAIL;
		}
	}

	/*
	 * Check if the Hit Invalidate operation does invalid the cache
	 * block at a hit.
	 */
	for(i = 0; i < config_info.dcache_size; i += config_info.dline_size) {
		addr = K0BASE + i;
		HitInv_D(addr, 1, config_info.dline_size);
		if(ChkDTag_CS(addr, 1, PState_Invalid, config_info.dline_size)) {
			/*Eprintf("D-cache state error\n");*/
			err_msg( DHITINV_ERR3, cpu_loc);
			msg_printf( ERR, "Hit Invalidate did not invalidate the line on a hit.\n");
			msg_printf( ERR, "Cache line address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected cache state: 0x%08x Actual cache state: 0x%08x.\n",
				PState_Invalid, error_data[2]);
			retval = FAIL;
		}
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary data hit invalidate test\n");
	return(retval);
}

