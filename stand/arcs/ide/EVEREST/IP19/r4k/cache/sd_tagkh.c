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

/*
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
 */

#ifdef	TOAD
#include "bup.h"
#include "types.h"

#else
#include "sys/types.h"
#endif	/* TOAD */

#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

extern int error_data[4];

static SdTagKh(int, int);
void DISPLAYERROR(uint, uint, uint, uint);

sd_tagkh()
{
	extern u_int scacherrx;
	uint retval;
	__psunsigned_t osr;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (sd_tagkh) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	osr = GetSR();
	/* Enable cache error exceptions ?? */
	if( scacherrx == NO)
		SetSR(osr | SR_DE);
	else
		SetSR(osr & ~SR_DE);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	if(SdTagKh(0xa0000000, 0xa0000000 + config_info.sidcache_size - 1)) 
		retval = FAIL;
#ifdef	TOAD
		BupHappy();
#endif	/* TOAD */

	SetSR(osr);
	ide_invalidate_caches();
	msg_printf(INFO, "Completed secondary TAG RAM Knaizuk Hartmann test\n");
	return(retval);
}

static SdTagKh(int first_address, int last_address)
{
	register u_int last_addr;
	register u_int last_value_read;
	register u_int local_expected_data;
	register u_int ptr;
	register u_int dmask;
	register u_int oneloc;
	register u_int twolocs;
	register u_int threelocs;

	u_int first_addr;
	uint retval=PASS;

	first_address &= 0xfffffffc;
	last_address &= 0xfffffffc;

	dmask= 0xffffff80;
	
	/*
	 * Load the variables that enable us to jump to the
	 * next sequencial or 2/3 locations away. Since the address for the
	 * tag locations isn't sequencial in the address space.
	 */
	oneloc = config_info.sidline_size;
	twolocs = config_info.sidline_size * 2;
	threelocs = config_info.sidline_size * 3;

	msg_printf( DBG, "Start test\n");
	first_addr = first_address;
	last_addr = last_address;
	first_addr = 0xa0000000;
	last_addr = 0xa00f0000;

	/*
	 * Set partitions 1 and 2 to 0's.
	 */
	local_expected_data = 0xaaaaaaaa & dmask;
	for (ptr = first_addr + oneloc ; ptr <= last_addr; ptr += twolocs) {

		/* *ptr++ = local_expected_data; */
		IndxStorTag_SD(ptr, local_expected_data);

		/*ptr ++;*/
		ptr = ptr + oneloc;

		if (ptr <= last_addr) {
			/* *ptr = local_expected_data;*/
			IndxStorTag_SD( ptr, local_expected_data);
		}
	}

	/*
	 * Set partition 0 to ones.
	 */

	/* ask off bits 0-5 which aren't used */
	local_expected_data = (0x55555555 & dmask);

	for (ptr = first_addr; ptr <= last_addr; ptr += threelocs) {
		/* *ptr = local_expected_data; */
		IndxStorTag_SD( ptr, local_expected_data);
	}

	/*
	 * Verify partition 1 is still 0's.
	 */
	local_expected_data = 0xaaaaaaaa & dmask;
	for (ptr = first_addr + oneloc; ptr <= last_addr; ptr += threelocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read != local_expected_data) {
			DISPLAYERROR(1, local_expected_data, last_value_read, ptr);
			retval = FAIL;
		}
	}

	/*
	 * Set partition 1 to ones.
	 */
	local_expected_data = 0x55555555 & dmask;
	for (ptr = first_addr + oneloc; ptr <= last_addr; ptr += threelocs) {

		/* *ptr = local_expected_data; */
		IndxStorTag_SD( ptr, local_expected_data);
	}

	/*
	 * Verify that partition 2 is zeroes.
	 */
	local_expected_data = 0xaaaaaaaa & dmask;
	for (ptr = first_addr + twolocs; ptr <= last_addr; ptr += threelocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read != local_expected_data) {

			DISPLAYERROR(2, local_expected_data, last_value_read, ptr);
			retval = FAIL;
		}
	}

	/*
	 * Verify that partitions 0 and 1 are still ones.
	 */
	local_expected_data = (0x55555555  & dmask);
	for (ptr = first_addr; ptr <= last_addr; ptr += twolocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read == local_expected_data) {
		   	if((ptr = ptr + oneloc) <= last_addr) {
				last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
				if(last_value_read != local_expected_data) {

					DISPLAYERROR(3, local_expected_data, last_value_read, ptr);
					retval = FAIL;
				}
			}
		}
		else {
			DISPLAYERROR(3, local_expected_data, last_value_read, ptr);
			retval = FAIL;
		}
	}

	/*
	 * Set partition 0 to zeroes.
	 */
	local_expected_data = 0xaaaaaaaa & dmask;
	for (ptr = first_addr; ptr <= last_addr; ptr += threelocs) {

		/* *ptr = local_expected_data;*/
		IndxStorTag_SD( ptr, local_expected_data);
	}

	/*
	 * Check partition 0 for zeroes.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr += threelocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read != local_expected_data) {

			DISPLAYERROR(4, local_expected_data, last_value_read, ptr);
			retval = FAIL;
		}
	}

	/*
	 * Set partition 2 to ones.
	 */
	local_expected_data = 0x55555555 & dmask;
	for (ptr = first_addr + twolocs; ptr <= last_addr; ptr += threelocs) {

		/* *ptr = local_expected_data; */
		IndxStorTag_SD( ptr, local_expected_data);
	}

	/*
	 * Check partition 2 for onees.
	 */
	local_expected_data = (0x55555555 & dmask);
	for (ptr = first_addr + twolocs; ptr <= last_addr; ptr += threelocs) {

		last_value_read = ( IndxLoadTag_SD(ptr) & dmask);
		if (last_value_read != local_expected_data) {

			DISPLAYERROR(5, local_expected_data, last_value_read, ptr);
			retval = FAIL;
		}
	}

	return(retval);
}

void DISPLAYERROR(uint errcode, uint edata, uint adata, uint addr)
{
#ifdef	TOAD
	bupSad();
#else
	err_msg( SDTAGKH_ERR1, cpu_loc);
	msg_printf( ERR, "Address %x, error code %d\n", addr, errcode);
	msg_printf( ERR, "expected %x, actual %x, XOR %x\n",
		edata, adata, (edata ^ adata) );
#endif	/* TOAD */
}
