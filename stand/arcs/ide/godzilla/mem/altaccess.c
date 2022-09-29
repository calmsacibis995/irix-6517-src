/*
 * altaccess.c - "address-in-address" type of test
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * altaccess()	-
 *
 *  This is an "address-in-address" type of test that makes alternating
 *  accesses in a variety of ways to two address spaces.
 *
 *  This version does memory addressing in opposite directions for the two
 *  address spaces under test.
 *
 *  Inputs:  
 *	first_address = address of the first memory location to be tested
 *	last_address  = address of the last 32 bit word to be tested.
 *
 * NOTES:
 *		- K64U64 is true
 */
#ident "ide/godzilla/mem/altaccess.c   $Revision: 1.3 $"

#include <sys/types.h>
#include <libsk.h>
#include "d_mem.h"

int
Altaccess_l(__psunsigned_t first_address, __psunsigned_t last_address)
{
	register volatile __psunsigned_t *last_addr1;
	register volatile __psunsigned_t *last_addr2;
	register volatile __psunsigned_t *lo_ptr;
	register volatile __psunsigned_t *ptr;
	register __psunsigned_t last_value_read;
	__psunsigned_t local_expected_data;
	__psunsigned_t lo_local_expected_data;
	volatile __psunsigned_t *first_addr1;
	volatile __psunsigned_t *first_addr2;
	volatile __psunsigned_t diff;
	int error;

	msg_printf(VRB,"Altaccess_l( %x, %x)\n", first_address, last_address);
	/* make sure the addresses are on word boundary
	 * by testing low order bits
	 */
#if _MIPS_SIM == _ABI64
	if( first_address & ~DWORD_ADDR_MASK) {
		first_address &= DWORD_ADDR_MASK;
		msg_printf(VRB," Starting address double word aligned, 0x%x\n", first_address);
	}
	if( last_address & ~DWORD_ADDR_MASK) {
		last_address &= DWORD_ADDR_MASK;
		msg_printf(VRB," Ending address double word aligned, 0x%x\n", last_address);
	}

#else
        if( first_address & ~WORD_ADDR_MASK) {
                first_address &= WORD_ADDR_MASK;
                msg_printf(VRB," Starting address word aligned, 0x%x\n", first_address);
        }
        if( last_address & ~WORD_ADDR_MASK) {
                last_address &= WORD_ADDR_MASK;
                msg_printf(VRB," Ending address word aligned, 0x%x\n", last_address);
        }



#endif
	/* Debug message */
	msg_printf(DBG,"Starting address 0x%x, Ending address 0x%x\n",first_address, last_address);

#if IP26
	flush_cache();
#endif

#if _MIPS_SIM == _ABI64
	diff = (((last_address - first_address) / 2) - 2) & DWORD_ADDR_MASK;
#else
	diff = (((last_address - first_address) / 2) - 2) & WORD_ADDR_MASK;
#endif

	first_addr1 = (__psunsigned_t *)first_address;
	last_addr1 = (__psunsigned_t *)((__psunsigned_t)first_address + diff) - 1;
	first_addr2 = (__psunsigned_t *)((__psunsigned_t)first_address + diff) + 1;
	last_addr2 = (__psunsigned_t *)(last_address) - 1;
	error = 0;

	busy(0);

	msg_printf(VRB,"a's and 5's\n");
	/*
	 * Set the upper half locations to x'AAAAAAAA'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_A, 0);

	busy(1);

	/*
	 * Set all lo-core locations to x'55555555'.
	 */
	fillmemW((__psunsigned_t *)first_addr1, (__psunsigned_t *)last_addr1,
		 FILL_5, 0);

	busy(0);

	/*
	 * Address-Complement-In-Address; Ascending Stores;
	 * Read-After-Write; VME Read-After-Write; Write; VME Write;
	 * Decending Checks (VME up; other down).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;
		*ptr = local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		--ptr;
		++lo_ptr;
	}

	busy(1);
	
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		++ptr;
	}

	busy(0);

	for (lo_ptr = last_addr1; lo_ptr >= first_addr1; ) {
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		--lo_ptr;
	}

	msg_printf(VRB,"5's and a's\n");
	/*
	 * Set all test locations to x'55555555'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_5, 0);

	busy(1);

	/*
	 * Set all lo-core locations to x'AAAAAAAA'.
	 */
	fillmemW((__psunsigned_t *)first_addr1, (__psunsigned_t *)last_addr1,
		 FILL_A, 0);

	busy(0);

	/*
	 * Address-In-Address; Decending Stores; VME Read-After-Write;
	 * Read-After-Write; VME Write; Write; Ascending Checks
	 * (VME decending; other ascending).
	 */
	lo_ptr = last_addr1;
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		*ptr = local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		--lo_ptr;
		++ptr;
	}

	busy(1);

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		++lo_ptr;
	}

	busy(0);

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		--ptr;
	}

	msg_printf(VRB,"c's and 3's\n");
	/*
	 * Set all test locations to x'CCCCCCCC'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_C, 0);

	busy(1);

	/*
	 * Set all lo-core locations to x'33333333'.
	 */
	fillmemW((__psunsigned_t *)first_addr1, (__psunsigned_t *)last_addr1,
		 FILL_3, 0);

	busy(0);

	/*
	 * Address-Complement-In-Address; Ascending Stores;
	 * Reads-After-Writes; Write; VME Write; Ascending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;

		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;

		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}
		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		--ptr;
		++lo_ptr;
	}

	busy(1);

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}
		--ptr;
	}

	busy(0);

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		++lo_ptr;
	}

	busy(1);

	msg_printf(VRB,"3's and c's\n");
	/*
	 * Set all test locations to x'33333333'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_3, 0);

	busy(0);

	/*
	 * Set all lo-core locations to x'CCCCCCCC'.
	 */
	fillmemW((__psunsigned_t *)first_addr1, (__psunsigned_t *)last_addr1,
		 FILL_C, 0);

	busy(1);

	/*
	 * Address-In-Address; Ascending Stores; Reads-After-Writes;
	 * VME Write; Write; Decending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		++lo_ptr;
		--ptr;
	}

	busy(0);

	for (lo_ptr = last_addr1; lo_ptr >= first_addr1; ) {
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		--lo_ptr;
	}

	busy(1);

	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		++ptr;
	}

	msg_printf(VRB,"e's and 7's\n");
	/*
	 * Set all test locations to x'EEEEEEEE'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_C, 0);

	busy(0);

	/*
	 * Set all lo-core locations to x'77777777'.
	 */
	fillmemW((__psunsigned_t *)first_addr1, (__psunsigned_t *)last_addr1,
		 FILL_7, 0);

	busy(1);

	/*
	 * Comp/Address-In-Address; Decending Stores; Reads-After-Writes;
	 * Write; VME Write; Decending Checks;
	 * (VME decending; other ascending).
	 */
	lo_ptr = last_addr1;
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		++ptr;
		--lo_ptr;
	}

	busy(0);

	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		++ptr;
	}

	busy(1);

	for (lo_ptr = last_addr1; lo_ptr >= first_addr1; ) {
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		--lo_ptr;
	}

	msg_printf(VRB,"7's and e's\n");

	/*
	 * Set all test locations to x'77777777'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_7, 0);

	busy(0);

	/*
	 * Set all lo-core locations to x'EEEEEEEE'.
	 */
	fillmemW((__psunsigned_t *)first_addr1, (__psunsigned_t *)last_addr1,
		 FILL_E, 0);

	busy(1);

	/*
	 * Address/Comp-In-Address; Ascending Stores; VME Read-After-Write;
	 * Read-After-Write; VME Write; Write; Decending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;
		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		*ptr = local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		++lo_ptr;
		--ptr;
	}

	for (lo_ptr = last_addr1; lo_ptr >= first_addr1; ) {
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		--lo_ptr;
	}

	busy(0);

	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		++ptr;
	}

	msg_printf(VRB,"9's and 6's\n");
	/*
	 * Set all test locations to x'99999999'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_9, 0);

	busy(1);

	/*
	 * Set all lo-core locations to x'66666666'.
	 */
	fillmemW((__psunsigned_t *)first_addr1, (__psunsigned_t *)last_addr1,
		 FILL_6, 0);

	busy(0);

	/*
	 * Comp/Address-In-Address; Ascending Stores; Read-After-Write;
	 * VME Read-After-Write; Write; VME Write; Ascending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		*ptr = local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		--ptr;
		++lo_ptr;
	}

	busy(0);

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		--ptr;
	}

	busy(1);

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		++lo_ptr;
	}

	msg_printf(VRB,"6's and 9's\n");
	/*
	 * Set all test locations to x'66666666'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_6, 0);

	busy(0);

	/*
	 * Set all lo-core locations to x'99999999'.
	 */
	fillmemW((__psunsigned_t *)first_addr1, (__psunsigned_t *)last_addr1,
		 FILL_9, 0);

	busy(1);

	/*
	 * Address-Complement-In-Address; Decending Stores;
	 * Write; VME Write; Ascending Checks;
	 * (VME decending; other ascending).
	 */
	lo_ptr = last_addr1;
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;
		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		++ptr;
		--lo_ptr;
	}

	busy(0);

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		--ptr;
	}

	busy(1);

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		++lo_ptr;
	}

	msg_printf(VRB,"1's and 8's\n");
	/*
	 * Set all test locations to x'11111111'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_1, 0);

	busy(0);

	/*
	 * Set all lo-core locations to x'88888888'.
	 */
	fillmemW((__psunsigned_t *)first_addr1, (__psunsigned_t *)last_addr1,
		 FILL_8, 0);

	busy(1);

	/*
	 * Address-In-Address; Ascending Stores; Multiple Write-Wrong-Firsts;
	 * VME Write; Write; Ascending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		*lo_ptr = local_expected_data;
		*ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		*ptr = lo_local_expected_data;
		*lo_ptr = local_expected_data;
		*ptr = lo_local_expected_data;
		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		++lo_ptr;
		--ptr;
	}

	busy(0);

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = (__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		++lo_ptr;
	}

	busy(1);

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));	
		}

		--ptr;
	}

	msg_printf(VRB,"8's and 1's\n");
	/*
	 * Set all test locations to x'88888888'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_8, 0);

	busy(0);

	/*
	 * Set all lo-core locations to x'11111111'.
	 */
	fillmemW((__psunsigned_t *)first_addr2, (__psunsigned_t *)last_addr2,
		 FILL_1, 0);

	busy(1);

	/*
	 * Address/Comp-In-Address; Ascending/Decending Stores;
	 * Multiple Write-Wrong-Firsts; Write; VME Write; Ascending Checks;
	 * (VME decending; other ascending).
	 */
	lo_ptr = last_addr1;
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;
		*ptr = lo_local_expected_data;
		*lo_ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		*lo_ptr = local_expected_data;
		*ptr = lo_local_expected_data;
		*lo_ptr = local_expected_data;
		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		--lo_ptr;
		++ptr;
	}

	busy(0);

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (__psunsigned_t)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr((caddr_t)ptr,local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		--ptr;
	}

	busy(1);

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = ~(__psunsigned_t)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr((caddr_t)lo_ptr,lo_local_expected_data, last_value_read, sizeof(__psunsigned_t));
		}

		++lo_ptr;
	}

	busy(0);

	if (error == 0) {
		return(TRUE);
	}

	return(FALSE);
}





