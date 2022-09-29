/*
 * movi.c -	  Moving Inversions memory test
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
 *  Moving Inversions memory test
 *
 *  The moving inversions algorithm will test all memory in B * (12A + 1) * N
 *  memory acceses where 
 *	N is the number of of memory locations to be tested.
 *	A is the total number of address bits
 *	B is the number of bits in the RAM chip
 *
 *  This algorithm can detect all stuck-at faults, all coupling faults, and
 *  some pattern sensitivety problems that can be modeled as coupling between
 *  rows and columns of the memory array.  It has the nice side effect of
 *  giving the address decoding logic a real workout.  
 *
 *  The basic algorithm reads the location under test, writes it, then verifies
 *  it.  It uses as a sliding one or zero pattern.  Memory is processed in
 *  jumps of log2(n).  One drawback of this algorithm is that it is sentive to
 *  RAM chip organization. However this is a property of all of the faster
 *  pattern sensitivity tests.  This implemenr=tation assumes a by 1 RAM.
 *
 *  Inputs:
 *	first_address - address of the first memory location to be tested
 *	last_address - address of the last 32 bit word to be tested.
 *	tstparms = Address of test parameter struct, see struct definition
 *
 */
#ident "ide/IP30/mem/movi.c:  $Revision: 1.3 $"

#include <sys/types.h>
#include <libsk.h>
#include "d_mem.h"

#define RESETFLAGS

#if _MIPS_SIM == _ABI64
#define ALIGN_MASK	DWORD_ADDR_MASK
#define ALIGN_STR	"double word"
#else
#define ALIGN_MASK	WORD_ADDR_MASK
#define ALIGN_STR	"word"
#endif

int
Movi_l(__psunsigned_t first_address, __psunsigned_t last_address)
{
	register volatile u_int *error_addr;
	register volatile u_int *last_addr;
	register __psunsigned_t msw;
	register u_int delta;
	register u_int max_delta;
	register u_int local_actual_data;
	register int lsw;
	register int pattern;
	register int inverse_pattern;
	volatile u_int *first_addr;
	int error;


	/* make sure the addresses are on (double) word boundary
	*/
	if(first_address & ~ALIGN_MASK) {
		first_address &= ALIGN_MASK;
		msg_printf(VRB," Starting address %s aligned, 0x%x\n",
			   ALIGN_STR, first_address);
	}
	if(last_address & ~ALIGN_MASK) {
		last_address &= ALIGN_MASK;
		msg_printf(VRB," Ending address %s aligned, 0x%x\n",
			   ALIGN_STR, last_address);
	}

	/* Debug message */
	msg_printf(DBG,"Movi_l( %08x, %08x )\n", first_address, last_address);

#if IP26
	flush_cache();
#endif

	if(first_address & ~ALIGN_MASK) {
		first_address &= ALIGN_MASK;
		msg_printf(VRB," Starting address %s aligned, 0x%x\n",
			   ALIGN_STR, first_address);
	}
	if(last_address & ~ALIGN_MASK) {
		last_address &= ALIGN_MASK;
		msg_printf(VRB," Ending address %s aligned, 0x%x\n",
			   ALIGN_STR, last_address);
	}
	busy(0);
	first_addr = (u_int *)first_address;
	last_addr = (u_int *)last_address;
	error = 0;
	/*
	 * Initialize all memory to 5's.
	 */
	fillmemW((__psunsigned_t *)first_addr, (__psunsigned_t *)last_addr, FILL_5, 0);
	busy(1);
	pattern = 0x55555555;
	inverse_pattern = ~pattern;
	/*
	 * Compute the number of bits required to represent the highest
	 * address then turn it into a bit mask.
	 */
	max_delta = 1 << ((4 * 8) - 1);
	for (msw = (((char *)last_addr - (char *)first_addr) + 1) << 1; (__psint_t) msw > 0; msw <<= 1) {
		max_delta >>= 1;
	}

	/*
	 * Address incrementing loop.
	 */
	for (delta = 4; delta != max_delta; delta <<= 1) {
		/*
		 * Forward moving inversions test.
		 */
		do {
			for (lsw = 0; lsw <= (delta - 1); lsw += 4) {
				for (msw = (__psunsigned_t)first_addr; msw <= ((__psunsigned_t)last_addr & ~(delta - 1));
				    msw = ((msw + delta) < msw ? (__psunsigned_t)last_addr + 1 : msw + delta)) {
					if ((msw + lsw) > (__psunsigned_t)last_addr) {
						break;
					}

					error_addr = ((u_int *)(msw + lsw));
					if ((local_actual_data = *error_addr) != pattern) {
						error = 1;
						msg_printf(DBG,"Error on first forward read\n");
						msg_printf(DBG,"Address increment= %0x Delta= %0x\n",lsw, delta);
						memerr((caddr_t)error_addr,pattern,local_actual_data,sizeof(u_int));
					}

					*error_addr = inverse_pattern;
					if ((local_actual_data = *error_addr) != inverse_pattern) {
						error = 1;
						msg_printf(DBG,"Error on second forward read\n");
						msg_printf(DBG,"Address increment= %0x Delta= %0x\n",lsw, delta);
						memerr((caddr_t)error_addr,inverse_pattern,local_actual_data,sizeof(u_int));

					}
				}

				/* This may be a bug.
				if ((msw + lsw) > (__psunsigned_t)last_addr) {
					break;
				}
*/
				busy(1);
			}

			pattern = inverse_pattern;
			inverse_pattern = ~pattern;
		} while (pattern != 0x55555555);
		/*
		 * Reverse moving inversions.
		 */
		do {
			for (lsw = delta - 4; lsw >= 0; lsw -= 4) {
				for (msw = (__psunsigned_t)(last_addr + 1) - delta; msw >= (__psunsigned_t)first_addr;
				    msw = delta > msw ? 0 : msw - delta) {
					if ((msw + lsw) < (__psunsigned_t)first_addr) {
						break;
					}

					error_addr = ((u_int *)(msw + lsw));
					if ((local_actual_data = *error_addr) != pattern) {
						error = 1;
						msg_printf(DBG,"Error on first reverse read\n");
						msg_printf(DBG,"Address increment= %0x Delta= %0x\n",lsw, delta);
						memerr((caddr_t)error_addr,pattern,local_actual_data,sizeof(u_int));

					}

					*error_addr = inverse_pattern;
					if ((local_actual_data = *error_addr) != inverse_pattern ) {
						error = 1;
						msg_printf(DBG,"Error on second reverse read\n");
						msg_printf(DBG,"Address increment= %0x Delta= %0x\n",lsw, delta);
						memerr((caddr_t)error_addr,inverse_pattern,local_actual_data,sizeof(u_int));
					}
				}

				/* This may be a bug.
				if ((msw + lsw) < (__psunsigned_t)first_addr) {
					break;
				}
*/
				busy(1);
			}

			pattern = inverse_pattern;
			inverse_pattern = ~pattern;
		} while (pattern != 0x55555555);
	}

	if (error == 0)  {
		return(TRUE);
	}

	return(FALSE);
}







