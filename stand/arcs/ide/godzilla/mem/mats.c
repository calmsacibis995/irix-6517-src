/*
 * mats.c
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
 *
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
 *	first_address = address of the first memory location to be tested
 *	last_address  = address of the last location to be tested.
 *
 *	NOTE: for IP30, not hardware dependency.
 *		- remove ifdef'ed code	XXX
 */
#ident "ide/godzilla/mem/mats.c:  $Revision: 1.6 $"

#include <sys/types.h>
#include <libsk.h>
#include "d_mem.h"

#define RESETFLAGS
#define ONERRCHK
#define LOOPCHK

Mats_l(__psunsigned_t first_address, __psunsigned_t last_address )
{
	register volatile __psunsigned_t *first_addr;
	register volatile __psunsigned_t *last_addr;
	register volatile __psunsigned_t	 *ptr;
	register __psunsigned_t	 last_value_read;
	register __psunsigned_t	 local_expected_data;
	register __psunsigned_t	 not_local_expected_data;
#if NOTDEF /* causes compiler remarks */
	register u_int err_mode;
	register u_int err_mark;
	register u_int loopflag;
#endif /* NOTDEF */
	int error;

#if _MIPS_SIM == _ABI64
	/* make sure the addresses are on word boundary
        */
	if (first_address & ~DWORD_ADDR_MASK) {
		first_address &= DWORD_ADDR_MASK;
		msg_printf(VRB," Starting address word aligned, 0x%x\n",
		    first_address);
	}
	if (last_address & ~DWORD_ADDR_MASK) {
		last_address &= DWORD_ADDR_MASK;
		msg_printf(VRB," Ending address word aligned, 0x%x\n",
		    last_address);
	}
#else
	/* make sure the addresses are on word boundary
	*/
	if (first_address & ~WORD_ADDR_MASK) {
		first_address &= WORD_ADDR_MASK;
		msg_printf(VRB," Starting address word aligned, 0x%x\n",
		    first_address);
	}
	if (last_address & ~WORD_ADDR_MASK) {
		last_address &= WORD_ADDR_MASK;
		msg_printf(VRB," Ending address word aligned, 0x%x\n",
		    last_address);
	}
#endif
	/* Debug message */
	msg_printf(DBG,"mats( %08x, %08x)\n", first_address, last_address);

	/* to prevent write-back since ide is cached */
	flush_cache();

#if NOTDEF /* causes compiler remarks */
start:
#endif /* NOTDEF */
	first_addr = (__psunsigned_t *)first_address;
	last_addr = (__psunsigned_t *)last_address;
	error = 0;
	local_expected_data = FILL_A;
	not_local_expected_data = ~local_expected_data;
	/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
#if NOTDEF /* causes compiler remarks */
loop:
#endif /* NOTDEF */
	/*
	 *  Set memory to 0xaaaaaaaa.
	 */
	fillmemW((__psunsigned_t *)first_addr, (__psunsigned_t *)last_addr,
		 local_expected_data, 0);
	msg_printf(VRB,"Fill %x\n",local_expected_data);

	/*
	 *  Verify that each location is still a's then change it to 5's.
	 */
	msg_printf(VRB,"Check %x, fill %x\n", local_expected_data,not_local_expected_data);
	for (ptr = first_addr; ptr <= last_addr; ) {

		if ((last_value_read = *ptr) != local_expected_data) {
			error++;
			memerr((caddr_t)ptr,local_expected_data, last_value_read,sizeof(u_int));
			/*ONERRCHK(1, err_mark, err_mode, loopflag, loop, done);*/
		}

		*ptr++ = not_local_expected_data;

	}
	/*	LOOPCHK(1, err_mark, loopflag, loop);*/

	/*
	 *  Verify that memory is still 5's.
	 */
	msg_printf(VRB,"Check %x\n",not_local_expected_data);
	for (ptr = first_addr; ptr <= last_addr; ) {
		if ((last_value_read = *ptr) != not_local_expected_data) {
			error++;
			memerr((caddr_t)ptr,not_local_expected_data, last_value_read,sizeof(u_int));
			/*			ONERRCHK(2, err_mark, err_mode, loopflag, loop, done);*/
		}

		++ptr;
	}


	/*	LOOPCHK(2, err_mark, loopflag, loop);*/
#if NOTDEF /* causes compiler remarks */
done:
#endif /* NOTDEF */
	if (error == 0) {
		return(TRUE);
	} else {
		return(FALSE);
	}
}
