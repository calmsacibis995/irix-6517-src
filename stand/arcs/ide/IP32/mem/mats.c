/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Revision: 1.1 $"

/*
 *			M a t s() 
 *
 *  Modified Algorithmic Test Sequence Memory Test
 *
 *  This algorithm is used to perform a fast but non-ehaustive memory test.  It
 *  will test a memory subsystem for stuck-at faults in both the address lines
 *  as well as the data locations. Wired or memory array behavior and arbitrary
 *  decoder design is assummed.  It makes only 4n memory accesses where n is
 *  the number of locations to be tested.  This algorithm trades completeness
 *  for speed.  No free lunch around here.  Hence this algorithm is not able to
 *  isolate pattern sensitivity or intermittent errors.  C'est la vie.  This
 *  algorithm is excellent when a quick memory test is needed.  Such as at PON
 *  (Power ON).  It should be slightly faster than the Knaizuk-Hartmann
 *  algorithm (on which it is based) due to its simpler looping structure.  It
 *  will definitely take less space than Knaizuk-Hartmann.  It provides
 *  slightly better test coverage than Knaizuk-Hartmann since it can detect
 *  errors in aribitrary decoder designs.
 *
 *  Three passes are made thru memory.  In the first pass the all zeroes
 *  pattern is written to every location.  In the second pass each location is
 *  verifieid that it is zeroes then it is written to ones.  The final pass
 *  verifies that the memory is now all ones.  The proof that this algorithm
 *  detects all stuck at faults for wired or memory arrays can be found in
 *  "Comments on an optimal algorithm for testing struck-at faults in random
 *  access memories." Nair, R., IEEE Transactions on Computers C-28 3(Mar) 1979.
 *
 *  Inputs:  
 *	first_address = address of the first meory location to be tested
 *	last_address  = address of the last 32 bit word to be tested.
 */

#include "sys/types.h"
#include "mem.h"
/* #include "error.h" #include "menu.h" */
#define   WORD_ADDR_MASK          0xfffffffc
#define  RESETFLAGS
#define ONERRCHK
#define LOOPCHK

Mats_l(int first_address, int last_address )
{
	register volatile u_int *first_addr;
	register volatile u_int *last_addr;
	register volatile u_int *ptr;
	register u_int last_value_read;
	register u_int local_expected_data;
	register u_int not_local_expected_data;
	register u_int err_mode;
	register u_int err_mark;
	register u_int loopflag;
	int error;

	/* make sure the addresses are on word boundary
	*/
	if( first_address & 3) {
		first_address &= WORD_ADDR_MASK;
		msg_printf(" Starting address word aligned, 0x%x\n", first_address);
	}
	if( last_address & 3) {
		last_address &= WORD_ADDR_MASK;
		msg_printf(VRB," Ending address word aligned, 0x%x\n", last_address);
	}

	/* Debug message */
	msg_printf(DBG,"mats( %08x, %08x)\n", first_address, last_address);

start:
	first_addr = (u_int *)first_address;
	last_addr = (u_int *)last_address;
	error = 0;
	local_expected_data = 0xaaaaaaaa;
	not_local_expected_data = ~local_expected_data;
	busy(0);
	/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop:
	/*
	 *  Set memory to 0xaaaaaaaa.
	 */
	fillmemW( first_addr, last_addr, local_expected_data, 0);
	/*for (ptr = first_addr; ptr <= last_addr; ) {
		*ptr++ = local_expected_data;
        }*/
#if defined(SABLE) && defined(DEBUG)
	*(first_addr + 1) = 0x44445555;
#endif

	/*
	 *  Verify that each location is still a's then change it to 5's.
	 */
	for (ptr = first_addr; ptr <= last_addr; ) {

		if ((last_value_read = *ptr) != local_expected_data) {
			error = 1;
			memerr(ptr,local_expected_data, last_value_read,sizeof(u_int));
			/*ONERRCHK(1, err_mark, err_mode, loopflag, loop, done);*/
		}

		*ptr++ = not_local_expected_data;

	}
	/*	LOOPCHK(1, err_mark, loopflag, loop);*/

#if defined(SABLE) && defined(DEBUG)
	*(first_addr + 2) = 0x77778888;
#endif

	/*
	 *  Verify that memory is still 5's.
	 */
	for (ptr = first_addr; ptr <= last_addr; ) {
		if ((last_value_read = *ptr) != not_local_expected_data) {
			error = 1;
			memerr(ptr,not_local_expected_data, last_value_read,sizeof(u_int));
			/*			ONERRCHK(2, err_mark, err_mode, loopflag, loop, done);*/
		}

		++ptr;
	}
	/*	LOOPCHK(2, err_mark, loopflag, loop);*/
busy(1);
done:
	if (error == 0)  {
		return(TRUE);
	}
	return(FALSE);
}

