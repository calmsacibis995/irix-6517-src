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
 *	File Name: d_kh.c
 *
 *	Diag name: d_kh
 *
 *	Description: This diag tests the data integrity of the D-cache with
 *		     the Knaizuk Hartmann algorithm. Data pattern 0x55555555
 *		     and 0xaaaaaaaa are used.
 *
 *			1. Invalidate the entire D-cache.
 *			2. Init memory with a background pattern of 
 *			   0xdeadbeef.
 *			3. Set partition 1 & 2 to 5's.
 *			4. Set partition 0 to a's.
 *			5. Verity partition 1 is still 5's.
 *			6. Set partition 1 to a's.
 *			7. Verify partition 2 is 5's.
 *			8. Verify partition 0 and 1 are still a's.
 *			9. Set partition 0 to 5's.
 *			10. Verify partition 0 is 5's.
 *			11. Set partition 2 to a's.
 *			12. Verify partition 2 is a's.
 *
 *	Routines: d_kh()
 *		  PdKh()
 *
 *  Knaizuk Hartmann Memory Test
 *
 *  This algorithm is used to perform a fast but non-ehaustive memory test.
 *  It will test a memory subsystem for stuck-at faults in both the address
 *  lines as well as the data locations.  Wired or memory array behavior and
 *  non creative decoder design are assummed.  It makes only 4n memory accesses
 *  where n is the number of locations to be tested.  This alogorithm trades
 *  completeness for speed. No free lunch around here.  Hence this algorithm
 *  is not able to isolate pattern sensitivity or intermittent errors.  C'est
 *  la vie. This algorithm is excellent when a quick memory test is needed.
 *
 *  The algorithm breaks up the memory to be tested into 3 partitions.  Partion
 *  0 consists of memory locations 0, 3, 6, ... partition 1 consists of
 *  memory locations 1,4,7,...  partition 2 consists oflocations 2,5,8...
 *  The partitions are filled with either an all ones pattern or an all
 *  zeroes pattern.  By varying the order in which the partitions are filled
 *  and then checked, this algorithm manages to check all combinations
 *  of possible stuck at faults.  If you don't believe me, you can find
 *  a rigorous mathematical proof (set notation and every thing) in
 *  a correspondence called "An Optimal Algorithm for Testing Stuck-at Faults
 *  in Random Access Memories" in the November 1977 issue of IEEE Transactions
 *  on Computing, volume C-26 #11 page 1141.
 *
 *  INPUTS:  first_address - address of the first memory location to be tested
 *           last_address - address of the last 32 bit word to be tested.
 *
 *===========================================================================*/

#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <ip19.h>
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

#define WORD_ADDR_MASK 0xfffffffc

void d_kh_error(__psunsigned_t, u_int, u_int);
int PdKh(__psunsigned_t, __psunsigned_t);

int
d_kh()
{
	register __psunsigned_t first_addr;
	register __psunsigned_t last_addr;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (d_kh) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	first_addr = K0BASE;
	last_addr = (first_addr + config_info.dcache_size - 4);

	retval = PdKh(first_addr, last_addr);

	ide_invalidate_caches();
	msg_printf(INFO, "Completed primary data RAM Knaizuk Hartmann test\n");
	return(retval);
}


int
PdKh(__psunsigned_t first_address, __psunsigned_t last_address)
{
	register volatile u_int * last_addr;
	register volatile u_int last_value_read;
	register volatile u_int local_expected_data;
	register volatile u_int *ptr;

	volatile u_int *first_addr, *tmp;
	u_int retval=PASS;

	first_address &= WORD_ADDR_MASK;
	last_address &= WORD_ADDR_MASK;

	msg_printf( DBG, "first_address %x, last_address %x\n", first_address, last_address);

	first_addr = (u_int *)first_address;
	last_addr = (u_int *)last_address;
	msg_printf( DBG, "first_addr %x, last_addr %x\n", first_addr, last_addr);

	/* Init memory with a background pattern 
	*/
	tmp = last_addr;
	for(ptr = first_addr; ptr <= tmp; ) {
		*ptr++ = 0xdeadbeef;
	}

#ifdef FUTURESHOCK
	/* Get a background pattern into the secondary */
	LoadCacheW(0x80000000, 0x200);
#endif /* FUTURESHOCK */

	/*
	 * Set partitions 1 and 2 to 5's.
	 */
	local_expected_data = 0x55555555;
	for (ptr = first_addr + 1; ptr <= last_addr; ptr += 2) {
		*ptr++ = local_expected_data;
		if (ptr <= last_addr) {
			*ptr = local_expected_data;
		}
	}

	/*
	 * Set partition 0 to a's.
	 */
	local_expected_data = 0xaaaaaaaa;
	for (ptr = first_addr; ptr <= last_addr; ptr += 3) {
		*ptr = local_expected_data;
	}

	/*
	 * Verify partition 1 is still 5's.
	 */
	local_expected_data = 0x55555555;
	for (ptr = first_addr + 1; ptr <= last_addr; ptr += 3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			/*Eprintf("Partition 1 error after partition 0 set to 0xaaaaaaaa.\n");*/
			err_msg( DKH_ERR1, cpu_loc);
			d_kh_error((__psunsigned_t)ptr, local_expected_data, last_value_read);
			retval = FAIL;
		}
	}

	/*
	 * Set partition 1 to a's.
	 */
	local_expected_data = 0xaaaaaaaa;
	for (ptr = first_addr + 1; ptr <= last_addr; ptr += 3) {
		*ptr = local_expected_data;
	}

	/*
	 * Verify that partition 2 is 5's.
	 */
	local_expected_data = 0x55555555;
	for (ptr = first_addr + 2; ptr <= last_addr; ptr += 3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			/*Eprintf("Partition 2 error after partition 1 set to 0xaaaaaaaa.\n");*/
			err_msg( DKH_ERR2, cpu_loc);
			d_kh_error((__psunsigned_t)ptr, local_expected_data, last_value_read);
			retval = FAIL;
		}
	}

	/*
	 * Verify that partitions 0 and 1 are still a's.
	 */
	local_expected_data = 0xaaaaaaaa;
	for (ptr = first_addr; ptr <= last_addr; ptr += 2) {
		if ((last_value_read = *ptr) != local_expected_data) {
			/*Eprintf("Partition 0 error after partition 1 set to 0xaaaaaaaa.\n");*/
			err_msg( DKH_ERR3, cpu_loc);
			d_kh_error((__psunsigned_t)ptr, local_expected_data, last_value_read);
			retval = FAIL;
		}

		if (++ptr <= last_addr && ((last_value_read = *ptr) != local_expected_data)) {
			/*Eprintf("Partition 1 error after partition 1 set to 0xaaaaaaaa.\n");*/
			err_msg( DKH_ERR4, cpu_loc);
			d_kh_error((__psunsigned_t)ptr, local_expected_data, last_value_read);
			retval = FAIL;
		}
	}

	/*
	 * Set partition 0 to 5's.
	 */
	local_expected_data = 0x55555555;
	for (ptr = first_addr; ptr <= last_addr; ptr += 3) {
		*ptr = local_expected_data;
	}

	/*
	 * Check partition 0 for 5's.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr += 3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			/*Eprintf("Partition 0 error after partition 0 set to 0x55555555.\n");*/
			err_msg( DKH_ERR5, cpu_loc);
			d_kh_error((__psunsigned_t)ptr, local_expected_data, last_value_read);
			retval = FAIL;
		}
	}

	/*
	 * Set partition 2 to a's.
	 */
	local_expected_data = 0xaaaaaaaa;
	for (ptr = first_addr + 2; ptr <= last_addr; ptr += 3) {
		*ptr = local_expected_data;
	}

	/*
	 * Check partition 2 for a's.
	 */
	local_expected_data = 0xaaaaaaaa;
	for (ptr = first_addr + 2; ptr <= last_addr; ptr += 3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			/*Eprintf("Partition 2 error after partition 2 set to 0xaaaaaaaa.\n");*/
			err_msg( DKH_ERR6, cpu_loc);
			d_kh_error((__psunsigned_t)ptr, local_expected_data, last_value_read);
			retval = FAIL;
		}
	}
	return(retval);
}


void d_kh_error(__psunsigned_t address, u_int expected, u_int actual)
{
	msg_printf( ERR, "Cache address: 0x%08x\n", address);
	msg_printf( ERR, "Expected: 0x%08x Actual: 0x%08x Xor: 0x%08x.\n",
			expected, actual, expected ^ actual);
}
