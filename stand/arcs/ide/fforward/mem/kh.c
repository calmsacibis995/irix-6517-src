/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Revision: 1.17 $"

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
 *  Inputs:  
 *	first_address = address of the first meory location to be tested
 *	last_address  = address of the last 32 bit word to be tested.
 *	tstparms = Address of test parameter struct, see struct definition
 */

/*  Log of Changes from original frias version  */
/*  Substituted TRUE for PASS FALSE for FAIL	*/
/*  Removed tstparms-> calls                    */
/*  Remove mem_err variable condition           */
/*  Removed ONERRCKH LOOPCHK calls              */

#include <sys/types.h>
#include <libsk.h>
#include "mem.h"

Kh_l(__psunsigned_t first_address, __psunsigned_t last_address)
{
	register __psunsigned_t *first_addr;
	register __psunsigned_t *last_addr;
	register __psunsigned_t last_value_read;
	register __psunsigned_t local_expected_data;
	register __psunsigned_t *ptr;
	register int error;

	/* make sure the addresses are aligned 
	*/

#if _MIPS_SIM == _ABI64
        if( first_address & ~DWORD_ADDR_MASK) {
                first_address &= DWORD_ADDR_MASK;
                msg_printf(DBG," Starting address double word aligned, 0x%x\n",
			   first_address);
        }
        if( last_address & ~DWORD_ADDR_MASK) {
                last_address &= DWORD_ADDR_MASK;
                msg_printf(DBG," Ending address double word aligned, 0x%x\n",
			   last_address);
        }
#else
	if( first_address & ~WORD_ADDR_MASK) {
		first_address &= WORD_ADDR_MASK;
		msg_printf(DBG," Starting address word aligned, 0x%x\n",
			   first_address);
	}
	if( last_address & ~WORD_ADDR_MASK) {
		last_address &= WORD_ADDR_MASK;
		msg_printf(DBG," Ending address word aligned, 0x%x\n",
			   last_address);
	}
#endif

	/* Debug message */
	msg_printf(VRB,"Kh_l( %x, %x)\n", first_address, last_address);

#if IP26
	flush_cache();
#endif

	error = 0;
	first_addr = (__psunsigned_t *)first_address;
	last_addr = (__psunsigned_t *)last_address;

	/*
	 * Set partitions 1 and 2 to 0's.
	 */
	msg_printf(DBG,"...P1 P2 ==> 0\n");
	busy(1);

	local_expected_data = 0;
	for (ptr = first_addr + 1; ptr <= last_addr; ptr += 2) {
		*ptr++ = local_expected_data;
		if (ptr <= last_addr) {
			*ptr = local_expected_data;
		}
	}

	/*
	 * Set partition 0 to ones.
	 */
	msg_printf(DBG,"...P0 ==> 1\n");
	busy(1);
	/*	scope_trigger();*/
	local_expected_data = -1;
	for (ptr = first_addr; ptr <= last_addr; ptr += 3) {
		*ptr = local_expected_data;
	}

	msg_printf(DBG,"...check P1=0\n");
	busy(1);

	/*
	 * Verify partition 1 is still 0's.
	 */
	local_expected_data = 0;
	for (ptr = first_addr + 1; ptr <= last_addr; ptr += 3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t) );
		}
	}

	msg_printf(DBG,"...P1==>1\n");
	busy(1);

	/*
	 * Set partition 1 to ones.
	 */
	local_expected_data = -1;
	for (ptr = first_addr + 1; ptr <= last_addr; ptr += 3) {
		*ptr = local_expected_data;
	}

#if defined(SABLE) && defined(DEBUG)
	*(first_addr + 2) = 0x12345678;
#endif

	msg_printf(DBG,"...check P2=0\n");
	busy(1);

	/*
	 * Verify that partition 2 is zeroes.
	 */
	local_expected_data = 0;
	for (ptr = first_addr + 2; ptr <= last_addr; ptr += 3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}
	}

#if defined(SABLE) && defined(DEBUG)
	*(first_addr + 1) = 0xaa55aa55;
#endif

	msg_printf(DBG,"...check P0 P1 = 1\n");
	busy(1);
	/*
	 * Verify that partitions 0 and 1 are still ones.
	 */
	local_expected_data = -1;
	for (ptr = first_addr; ptr <= last_addr; ptr += 2) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}
		if( ++ptr <= last_addr && (last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}
	}

	msg_printf(DBG,"...P0==>0\n");
	busy(1);
	/*
	 * Set partition 0 to zeroes.
	 */
	local_expected_data = 0;
	for (ptr = first_addr; ptr <= last_addr; ptr += 3) {
		*ptr = local_expected_data;
	}

	msg_printf(DBG,"...check P0=0\n");
	busy(1);
	/*
	 * Check partition 0 for zeroes.
	 */
	for (ptr = first_addr; ptr <= last_addr; ptr += 3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}
	}

	msg_printf(DBG,"...P2==>1\n");
	busy(1);
	/*
	 * Set partition 2 to ones.
	 */
	local_expected_data = -1;
	for (ptr = first_addr + 2; ptr <= last_addr; ptr += 3) {
		*ptr = local_expected_data;
	}


	msg_printf(DBG,"...check P2 = 1\n");
	busy(1);
	/*
	 * Check partition 2 for onees.
	 */
	local_expected_data = -1;
	for (ptr = first_addr + 2; ptr <= last_addr; ptr += 3) {
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;

			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}
	}

	busy(0);
	msg_printf(DBG,"...done\n");
	/*	if (error == 0)  { */
	if (error == 0) {
		return(TRUE);
	} else {
		return(FALSE);
	}
}
