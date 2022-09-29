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

#ident "$Revision: 1.5 $"

/*=============================================================================
 *
 *	File Name: d_function.c
 *
 *	Diag name: d_function
 *
 *	Description: This diag tests the functionality of the entire data
 *		     cache. It checks the block fill, write back on a dirty
 *		     line replacement, and no write back on a clean line
 *		     replacement function of the data cache lines.
 *			1. Invalidate the entire D-cache.
 *			2. Write memory buffer A locations with their
 *			   addresses.
 *			3. Fill the cache from memory buffer A with block
 *			   fill operation.
 *			4. Check if the cache lines are filled with the
 *			   expected data. Modify each location with its
 *			   Kseg0 address. This should dirty the line and
 *			   set the writeback bit.
 *			5. Write memory buffer B locations with their
 *			   addresses.
 *			6. Fill the cache from memory buffer B with block
 *			   fill operation. This should cause the dirtied
 *			   cache lines being written back to memory buffer A,
 *			   and the cache lines being replaced by memory
 *			   buffer B and the write back bit being cleared.
 *			7. Check if the entire cache does being filled with
 *			   the data from memory buffer B.
 *			8. Check if the entire cache is written back to
 *			   memory buffer A. Memory buffer A locations now
 *			   should contain the Kseg0 addresses.
 *			9. Check the memory buffer B with the pattern
 *			   0xdeadbeef.
 *			10. Fill the cache from memory buffer A with block
 *			    fill operation. This should cause NO write back
 *			    to the memory buffer B because the cache lines
 *			    should be clean and the write back bits should
 *			    be unset.
 *			11. Check if the entire cache is filled with memory
 *			    buffer A.
 *			12. Check if memory buffer B still contain 0xdeadbeef.
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

d_function()
{
	register __psunsigned_t addr, address;
	register u_int i;
	register u_int temp;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (d_function) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	address = PHYS_TO_K1(0x800000);

	/*
	 * Fill memory buffer A based at "address" with address as 
	 * background pattern
	 */
	for(i = 0; i < config_info.dcache_size; i += 4) {
		addr = address + i;
		* (u_int *) addr = (uint)addr;
	}

	/*
	 * Load memory buffer A into cache. This should cause the cache lines
	 * to become valid. 
	 */
	for(i = 0; i < config_info.dcache_size; i += config_info.dline_size) {
		addr = K1_TO_K0(address + i);
		temp = * (u_int *) addr;
	}

	/*
	 * Check if the entire block is filled with the expected data from
	 * memory buffer A.
	 * Change the data to the Kseg0 address of that location for later
	 * checking in memory. This should cause the Writeback bit to be set.
	 */
	for(i = 0; i < config_info.dcache_size; i += 4) {
		addr = K1_TO_K0(address + i);
		temp = * (u_int *) addr;
		if (temp != K0_TO_K1(addr)) {
			/*Eprintf("D-cache block fill error1.\n");*/
			err_msg( DFUNCT_ERR1, cpu_loc);	
			msg_printf( ERR, "Cache contains incorrect data.\n");
			msg_printf( ERR, "Cache Address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x\n",
				K0_TO_K1(addr), temp, K0_TO_K1(addr) ^ temp);
			retval = FAIL;
		}
		* (u_int *)addr = (uint)addr;
	}

	/*
	 * Fill memory buffer B based at "address + dcache_size" with 
	 * address as background pattern. Note Make the address alias
	 * the secondary inorder to force the writeback.
	 */
	address = address + config_info.sidcache_size;

	for(i = 0; i < config_info.dcache_size; i += 4) {
		addr = address + i;
		* (u_int *) addr = (uint)addr;
	}

	/*
	 * Load memory buffer B into cache. This will cause the current cache
	 * lines to be written back to memory buffer A and the Writeback 
	 * bit should be cleared. 
	 */
	for(i = 0; i < config_info.dcache_size; i += config_info.dline_size) {
		addr = K1_TO_K0(address + i);
		temp = * (u_int *) addr;
	}

	/*
	 * Check if the entire block is filled with the expected data from
	 * memory buffer B.
	 */
	for(i = 0; i < config_info.dcache_size; i += 4) {
		addr = K1_TO_K0(address + i);
		temp = * (u_int *) addr;
		if (temp != K0_TO_K1(addr)) {
			/*Eprintf("D-cache block fill error2.\n");*/
			err_msg( DFUNCT_ERR2, cpu_loc);	
			msg_printf( ERR, "Cache contains incorrect data.\n");
			msg_printf( ERR, "Cache Address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x\n",
				K0_TO_K1(addr), temp, K0_TO_K1(addr) ^ temp);
			retval = FAIL;
		}
	}
	address = address - config_info.sidcache_size;

	/*
	 * Check if the entire cache is written back to memory buffer A.
	 */
	for(i = 0; i < config_info.dcache_size; i += 4) {
		addr = address + i;
		temp = * (u_int *) addr;
		if (temp != K1_TO_K0(addr)) {
			/*Eprintf("D-cache block write back error.\n");*/
			err_msg( DFUNCT_ERR3, cpu_loc);	
			msg_printf( ERR, "Memory contains incorrect data.\n");
			msg_printf( ERR, "Cache Address: 0x%08x\n", K1_TO_K0(addr));
			msg_printf( ERR, "Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x\n",
				K1_TO_K0(addr), temp, K1_TO_K0(addr) ^ temp);
			retval = FAIL;
		}
	}

	/*
	 * Fill memory buffer B based at "address + dcache_size" with 
	 * 0xdeadbeef as background pattern
	 */
	for(address = address + config_info.dcache_size, i = 0; i < config_info.dcache_size; i += 4) {
		addr = address + i;
		* (u_int *) addr = 0xdeadbeef;
	}

	/*
	 * Load memory biffer A into cache. This should NOT cause the cache
	 * lines to be written back to memory buffer B because the Writeback
	 * bit is not set.
	 */
	for(address = address - config_info.dcache_size, i = 0;
		i < config_info.dcache_size; i += config_info.dline_size) {
		addr = K1_TO_K0(address + i);
		temp = * (u_int *) addr;
	}

	/*
	 * Check if the entire block is filled with the expected data from
	 * memory buffer A.
	 */
	for(i = 0; i < config_info.dcache_size; i += 4) {
		addr = K1_TO_K0(address + i);
		temp = * (u_int *) addr;
		if (temp != addr) {
			/*Eprintf("D-cache block fill error3.\n");*/
			err_msg( DFUNCT_ERR4, cpu_loc);	
			msg_printf( ERR, "Cache contains incorrect data.\n");
			msg_printf( ERR, "Cache Address: 0x%08x\n", addr);
			msg_printf( ERR, "Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x\n",
				addr, temp, addr ^ temp);
			retval = FAIL;
		}
	}


	/*
	 * Check if the memory buffer B still contains the pattern
	 * 0xdeadbeff.
	 */

	for(address = address + config_info.dcache_size, i = 0; i < config_info.dcache_size; i += 4) {
		addr = address + i;
		temp = * (u_int *) addr;
		if (temp != 0xdeadbeef) {
			/*Eprintf("D-cache block write back error2.\n");*/
			err_msg( DFUNCT_ERR5, cpu_loc);	
			msg_printf( ERR, "Memory content is altered.\n");
			msg_printf( ERR, "Write back happened on a clean line.\n");
			msg_printf( ERR, "Cache Address: 0x%08x\n", K1_TO_K0(addr));
			msg_printf( ERR, "Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x\n",
				0xdeadbeef, temp, 0xdeadbeef ^ temp);
			retval = FAIL;
		}
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary data functionality test\n");
	return(retval);
}

