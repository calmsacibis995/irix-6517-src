/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Revision: 1.1 $"

/*
 *		altaccess()	
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
 */

#include "sys/types.h"
#include "mem.h"

#define WORD_ADDR_MASK		0xfffffffc
#define RESETFLAGS
#define ONERRCHK
#define LOOPCHK

/*extern volatile u_int clear_latch;*/


Altaccess_l(int first_address, int last_address )
{
	register volatile u_int *last_addr1;
	register volatile u_int *last_addr2;
	register volatile u_int *lo_ptr;
	register volatile u_int *ptr;
	register u_int last_value_read;
	register u_int local_expected_data;
	register u_int lo_local_expected_data;
	volatile u_int *first_addr1;
	volatile u_int *first_addr2;
	volatile int diff;
	u_int err_mode;
	int err_mark;
	int loopflag;
	int error;


	msg_printf(VRB,"Altaccess_l( %x, %x)\n", first_address, last_address);
	/* make sure the addresses are on word boundary
	*/
	if( first_address & 3) {
		first_address &= WORD_ADDR_MASK;
		msg_printf(VRB," Starting address word aligned, 0x%x\n", first_address);
	}
	if( last_address & 3) {
		last_address &= WORD_ADDR_MASK;
		msg_printf(VRB," Ending address word aligned, 0x%x\n", last_address);
	}

	/* Debug message */
	msg_printf(DBG,"Starting address 0x%x, Ending address 0x%x\n",first_address, last_address);


restart:
	diff = ((((int)last_address - (int)first_address) / 2) - 2) & WORD_ADDR_MASK;
	first_addr1 = (u_int *)first_address;
	last_addr1 = (u_int *)((u_int)first_address + (int)(diff - 4));
	first_addr2 = (u_int *)((u_int)first_address + (int)(diff + 4));
	last_addr2 = (u_int *)(last_address - 4);
	error = 0;
busy(0);
/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop:
	/*
	 * Set the upper half locations to x'AAAAAAAA'.
	 */
	fillmemW( first_addr2, last_addr2, 0xaaaaaaaa, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0xaaaaaaaa;
	}*/
busy(1);
	/*
	 * Set all lo-core locations to x'55555555'.
	 */
	fillmemW( first_addr1, last_addr1, 0x55555555, 0);
	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0x55555555;
	}*/
busy(0);
	/*
	 * Address-Complement-In-Address; Ascending Stores;
	 * Read-After-Write; VME Read-After-Write; Write; VME Write;
	 * Decending Checks (VME up; other down).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		lo_local_expected_data = ~(u_int)lo_ptr;
		*ptr = local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(1, err_mark, err_mode, loopflag, loop, done);*/
		}

		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(1, err_mark, err_mode, loopflag, loop, done);*/
		}

		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		--ptr;
		++lo_ptr;
	}
/*	LOOPCHK(1, err_mark, loopflag, loop);*/
busy(1);
	
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(2, err_mark, err_mode, loopflag, loop, done);*/
		}

		++ptr;
	}
/*	LOOPCHK(2, err_mark, loopflag, loop);*/
busy(0);

	for (lo_ptr = last_addr1; lo_ptr >= first_addr1; ) {
		lo_local_expected_data = ~(u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(3, err_mark, err_mode, loopflag, loop, done);*/
		}

		--lo_ptr;
	}
/*	LOOPCHK(3, err_mark, loopflag, loop);*/

/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop1:

	/*
	 * Set all test locations to x'55555555'.
	 */
	fillmemW( first_addr2, last_addr2, 0x55555555, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0x55555555;
	}*/
busy(1);
	/*
	 * Set all lo-core locations to x'AAAAAAAA'.
	 */
	fillmemW( first_addr1, last_addr1, 0xaaaaaaaa, 0);
	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0xaaaaaaaa;
	}*/
busy(0);
	/*
	 * Address-In-Address; Decending Stores; VME Read-After-Write;
	 * Read-After-Write; VME Write; Write; Ascending Checks
	 * (VME decending; other ascending).
	 */
	lo_ptr = last_addr1;
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = (u_int)ptr;
		lo_local_expected_data = (u_int)lo_ptr;
		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(4, err_mark, err_mode, loopflag, loop1, done);*/
		}

		*ptr = local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(4, err_mark, err_mode, loopflag, loop1, done);*/
		}

		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		--lo_ptr;
		++ptr;
	}
/*	LOOPCHK(4, err_mark, loopflag, loop1);*/
busy(1);

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = (u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(5, err_mark, err_mode, loopflag, loop1, done);*/
		}

		++lo_ptr;
	}
/*	LOOPCHK(5, err_mark, loopflag, loop1);*/
busy(0);

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(6, err_mark, err_mode, loopflag, loop1, done);*/
		}

		--ptr;
	}
/*	LOOPCHK(6, err_mark, loopflag, loop1);*/

/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop2:

	/*
	 * Set all test locations to x'CCCCCCCC'.
	 */
	fillmemW( first_addr2, last_addr2, 0xcccccccc, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0xCCCCCCCC;
	}*/

busy(1);
	/*
	 * Set all lo-core locations to x'33333333'.
	 */
	fillmemW( first_addr1, last_addr1, 0x33333333, 0);

	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0x33333333;
	}*/
busy(0);
	/*
	 * Address-Complement-In-Address; Ascending Stores;
	 * Reads-After-Writes; Write; VME Write; Ascending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		lo_local_expected_data = ~(u_int)lo_ptr;

		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;

		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(7, err_mark, err_mode, loopflag, loop2, done);*/
		}

		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(7, err_mark, err_mode, loopflag, loop2, done);*/
		}
		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		--ptr;
		++lo_ptr;
	}
/*	LOOPCHK(7, err_mark, loopflag, loop2);*/
busy(1);

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(8, err_mark, err_mode, loopflag, loop2, done);*/
		}
		--ptr;
	}
/*	LOOPCHK(8, err_mark, loopflag, loop2);*/
busy(0);

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = ~(u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(9, err_mark, err_mode, loopflag, loop2, done);*/
		}

		++lo_ptr;
	}
/*	LOOPCHK(9, err_mark, loopflag, loop2);*/

busy(1);
/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop3:

	/*
	 * Set all test locations to x'33333333'.
	 */
	fillmemW( first_addr2, last_addr2, 0x33333333, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0x33333333;
	}*/
busy(0);
	/*
	 * Set all lo-core locations to x'CCCCCCCC'.
	 */
	fillmemW( first_addr1, last_addr1, 0xcccccccc, 0);
	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0xcccccccc;
	}*/
busy(1);
	/*
	 * Address-In-Address; Ascending Stores; Reads-After-Writes;
	 * VME Write; Write; Decending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (u_int)ptr;
		lo_local_expected_data = (u_int)lo_ptr;
		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(10, err_mark, err_mode, loopflag, loop3, done);*/
		}

		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(10, err_mark, err_mode, loopflag, loop3, done);*/
		}

		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		++lo_ptr;
		--ptr;
	}
/*	LOOPCHK(10, err_mark, loopflag, loop3);*/
busy(0);

	for (lo_ptr = last_addr1; lo_ptr >= first_addr1; ) {
		lo_local_expected_data = (u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(11, err_mark, err_mode, loopflag, loop3, done);*/
		}

		--lo_ptr;
	}
/*	LOOPCHK(11, err_mark, loopflag, loop3);*/
busy(1);

	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = (u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(12, err_mark, err_mode, loopflag, loop3, done);*/
		}

		++ptr;
	}
/*	LOOPCHK(12, err_mark, loopflag, loop3);*/

/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop4:

	/*
	 * Set all test locations to x'EEEEEEEE'.
	 */
	fillmemW( first_addr2, last_addr2, 0xeeeeeeee, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0xeeeeeeee;
	}*/
busy(0);
	/*
	 * Set all lo-core locations to x'77777777'.
	 */
	fillmemW( first_addr1, last_addr1, 0x77777777, 0);
	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0x77777777;
	}*/

	/*
	 * Comp/Address-In-Address; Decending Stores; Reads-After-Writes;
	 * Write; VME Write; Decending Checks;
	 * (VME decending; other ascending).
	 */
	lo_ptr = last_addr1;
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		lo_local_expected_data = (u_int)lo_ptr;
		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(13, err_mark, err_mode, loopflag, loop4, done);*/
		}

		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(13, err_mark, err_mode, loopflag, loop4, done);*/
		}

		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		++ptr;
		--lo_ptr;
	}
/*	LOOPCHK(13, err_mark, loopflag, loop4);*/
busy(1);
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(14, err_mark, err_mode, loopflag, loop4, done);*/
		}

		++ptr;
	}
/*	LOOPCHK(14, err_mark, loopflag, loop4);*/
busy(0);

	for (lo_ptr = last_addr1; lo_ptr >= first_addr1; ) {
		lo_local_expected_data = (u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(15, err_mark, err_mode, loopflag, loop4, done);*/
		}

		--lo_ptr;
	}
/*	LOOPCHK(15, err_mark, loopflag, loop4);*/

/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop5:
busy(1);
	/*
	 * Set all test locations to x'77777777'.
	 */
	fillmemW( first_addr2, last_addr2, 0x77777777, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0x77777777;
	}*/
busy(0);
	/*
	 * Set all lo-core locations to x'EEEEEEEE'.
	 */
	fillmemW( first_addr1, last_addr1, 0xeeeeeeee, 0);
	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0xeeeeeeee;
	}*/
busy(1);
	/*
	 * Address/Comp-In-Address; Ascending Stores; VME Read-After-Write;
	 * Read-After-Write; VME Write; Write; Decending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (u_int)ptr;
		lo_local_expected_data = ~(u_int)lo_ptr;
		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(16, err_mark, err_mode, loopflag, loop5, done);*/
		}

		*ptr = local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(16, err_mark, err_mode, loopflag, loop5, done);*/
		}

		*lo_ptr = lo_local_expected_data;
		*ptr = local_expected_data;
		++lo_ptr;
		--ptr;
	}
/*	LOOPCHK(16, err_mark, loopflag, loop5);*/
busy(0);

	for (lo_ptr = last_addr1; lo_ptr >= first_addr1; ) {
		lo_local_expected_data = ~(u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(17, err_mark, err_mode, loopflag, loop5, done);*/
		}

		--lo_ptr;
	}
/*	LOOPCHK(17, err_mark, loopflag, loop5);*/
busy(1);

	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = (u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(18, err_mark, err_mode, loopflag, loop5, done);*/
		}

		++ptr;
	}
/*	LOOPCHK(18, err_mark, loopflag, loop5);*/
busy(0);

/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop6:

	/*
	 * Set all test locations to x'99999999'.
	 */
	fillmemW( first_addr2, last_addr2, 0x99999999, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0x99999999;
	}*/
busy(1);
	/*
	 * Set all lo-core locations to x'66666666'.
	 */
	fillmemW( first_addr1, last_addr1, 0x66666666, 0);
	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0x66666666;
	}*/
busy(0);
	/*
	 * Comp/Address-In-Address; Ascending Stores; Read-After-Write;
	 * VME Read-After-Write; Write; VME Write; Ascending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		lo_local_expected_data = (u_int)lo_ptr;
		*ptr = local_expected_data;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(19, err_mark, err_mode, loopflag, loop6, done);*/
		}

		*lo_ptr = lo_local_expected_data;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(19, err_mark, err_mode, loopflag, loop6, done);*/
		}

		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		--ptr;
		++lo_ptr;
	}
/*	LOOPCHK(19, err_mark, loopflag, loop6);*/
busy(1);
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(20, err_mark, err_mode, loopflag, loop6, done);*/
		}

		--ptr;
	}
/*	LOOPCHK(20, err_mark, loopflag, loop6);*/
busy(0);
	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = (u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(21, err_mark, err_mode, loopflag, loop6, done);*/
		}

		++lo_ptr;
	}
/*	LOOPCHK(21, err_mark, loopflag, loop6);*/
busy(0);
/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop7:
	/*
	 * Set all test locations to x'66666666'.
	 */
	fillmemW( first_addr2, last_addr2, 0x66666666, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0x66666666;
	}*/
busy(1);
	/*
	 * Set all lo-core locations to x'99999999'.
	 */
	fillmemW( first_addr1, last_addr1, 0x99999999, 0);
	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0x99999999;
	}*/
busy(0);
	/*
	 * Address-Complement-In-Address; Decending Stores;
	 * Write; VME Write; Ascending Checks;
	 * (VME decending; other ascending).
	 */
	lo_ptr = last_addr1;
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		lo_local_expected_data = ~(u_int)lo_ptr;
		*ptr = local_expected_data;
		*lo_ptr = lo_local_expected_data;
		++ptr;
		--lo_ptr;
	}

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = ~(u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(22, err_mark, err_mode, loopflag, loop7, done);*/
		}

		--ptr;
	}
/*	LOOPCHK(22, err_mark, loopflag, loop7);*/
busy(1);

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = ~(u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(23, err_mark, err_mode, loopflag, loop7, done);*/
		}

		++lo_ptr;
	}
/*	LOOPCHK(23, err_mark, loopflag, loop7);*/
busy(0);

/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop8:
	/*
	 * Set all test locations to x'11111111'.
	 */
	fillmemW( first_addr2, last_addr2, 0x11111111, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0x11111111;
	}*/
busy(1);
	/*
	 * Set all lo-core locations to x'88888888'.
	 */
	fillmemW( first_addr1, last_addr1, 0x88888888, 0);
	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0x88888888;
	}*/
busy(0);
	/*
	 * Address-In-Address; Ascending Stores; Multiple Write-Wrong-Firsts;
	 * VME Write; Write; Ascending Checks;
	 * (VME ascending; other decending).
	 */
	lo_ptr = first_addr1;
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (u_int)ptr;
		lo_local_expected_data = (u_int)lo_ptr;
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

	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = (u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(24, err_mark, err_mode, loopflag, loop8, done);*/
		}

		++lo_ptr;
	}
/*	LOOPCHK(24, err_mark, loopflag, loop8);*/
busy(1);
	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));	
/*			ONERRCHK(25, err_mark, err_mode, loopflag, loop8, done);*/
		}

		--ptr;
	}
/*	LOOPCHK(25, err_mark, loopflag, loop8);*/
busy(0);

	RESETFLAGS(err_mark, loopflag, err_mode);
loop9:
	/*
	 * Set all test locations to x'88888888'.
	 */
	fillmemW( first_addr2, last_addr2, 0x88888888, 0);
	/*for (ptr = first_addr2; ptr <= last_addr2; ) {
		*ptr++ = 0x88888888;
	}*/
busy(1);
	/*
	 * Set all lo-core locations to x'11111111'.
	 */
	fillmemW( first_addr2, last_addr2, 0x11111111, 0);
	/*for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		*lo_ptr++ = 0x11111111;
	}*/
busy(0);
	/*
	 * Address/Comp-In-Address; Ascending/Decending Stores;
	 * Multiple Write-Wrong-Firsts; Write; VME Write; Ascending Checks;
	 * (VME decending; other ascending).
	 */
	lo_ptr = last_addr1;
	for (ptr = first_addr2; ptr <= last_addr2; ) {
		local_expected_data = (u_int)ptr;
		lo_local_expected_data = ~(u_int)lo_ptr;
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

	for (ptr = last_addr2; ptr >= first_addr2; ) {
		local_expected_data = (u_int)ptr;
		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(26, err_mark, err_mode, loopflag, loop9, done);*/
		}

		--ptr;
	}
/*	LOOPCHK(26, err_mark, loopflag, loop9);*/
busy(1);
	for (lo_ptr = first_addr1; lo_ptr <= last_addr1; ) {
		lo_local_expected_data = ~(u_int)lo_ptr;
		if ((last_value_read = *lo_ptr) != lo_local_expected_data) {
			error = 1;
			memerr(lo_ptr,lo_local_expected_data, last_value_read, sizeof(u_int));
/*			ONERRCHK(27, err_mark, err_mode, loopflag, loop9, done);*/
		}

		++lo_ptr;
	}
/*	LOOPCHK(27, err_mark, loopflag, loop9);*/
busy(0);

done:
	if (error == 0) {
		return(TRUE);
	}

	return(FALSE);
}
