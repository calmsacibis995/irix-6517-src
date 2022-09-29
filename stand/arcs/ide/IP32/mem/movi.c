/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Revision: 1.1 $"

/*
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
 */

#include "sys/types.h"
#include "mem.h"
/*#include "error.h" #include "menu.h" */
#define   WORD_ADDR_MASK          0xfffffffc
#define  RESETFLAGS
#define ONERRCHK
#define LOOPCHK

Movi_l(int first_address, int last_address)
{
	register volatile u_int *error_addr;
	register volatile u_int *last_addr;
	register u_int err_mode;
	register u_int err_mark;
	register u_int msw;
	register u_int delta;
	register u_int max_delta;
	register u_int local_actual_data;
	register int lsw;
	register int pattern;
	register int inverse_pattern;
	volatile u_int *first_addr;
	int loopflag;
	int error;


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
	msg_printf(DBG,"Movi_l( %08x, %08x )\n", first_address, last_address);

start:
	busy(0);
	first_addr = (u_int *)first_address;
	last_addr = (u_int *)last_address;
	error = 0;
	/*
	 * Initialize all memory to 5's.
	 */
	/*	RESETFLAGS(err_mark, loopflag, err_mode);*/
loop:
	fillmemW( first_addr, last_addr, 0x55555555, 0);
	/*for (error_addr = first_addr; error_addr <= last_addr; ) {
		*error_addr++ = 0x55555555;
	}*/
busy(1);
	pattern = 0x55555555;
	inverse_pattern = ~pattern;
	/*
	 * Compute the number of bits required to represent the highest
	 * address then turn it into a bit mask.
	 */
	max_delta = 1 << ((4 * 8) - 1);
	for (msw = (((char *)last_addr - (char *)first_addr) + 1) << 1; (int) msw > 0; msw <<= 1) {
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
				for (msw = (u_int)first_addr; msw <= ((u_int)last_addr & ~(delta - 1));
				    msw = ((msw + delta) < msw ? (int)last_addr + 1 : msw + delta)) {
					if ((msw + lsw) > (u_int)last_addr) {
						break;
					}

					error_addr = ((u_int *)(msw + lsw));
					if ((local_actual_data = *error_addr) != pattern) {
						error = 1;
						msg_printf(DBG,"Error on first forward read\n");
						msg_printf(DBG,"Address increment= %0x Delta= %0x\n",lsw, delta);
						memerr(error_addr,pattern,local_actual_data,sizeof(u_int));
/*						ONERRCHK(1, err_mark, err_mode, loopflag, loop, done);*/
					}

					*error_addr = inverse_pattern;
					if ((local_actual_data = *error_addr) != inverse_pattern) {
						error = 1;
						msg_printf(DBG,"Error on second forward read\n");
						msg_printf(DBG,"Address increment= %0x Delta= %0x\n",lsw, delta);
						memerr(error_addr,inverse_pattern,local_actual_data,sizeof(u_int));
/*						ONERRCHK(1, err_mark, err_mode, loopflag, loop, done);*/

					}
				}
				/*				LOOPCHK(1, err_mark, loopflag, loop);*/

				/* This may be a bug.
				if ((msw + lsw) > (u_int)last_addr) {
					break;
				}
*/
			}

			pattern = inverse_pattern;
			inverse_pattern = ~pattern;
		} while (pattern != 0x55555555);
		/*
		 * Reverse moving inversions.
		 */
busy(0);
		do {
			for (lsw = delta - 4; lsw >= 0; lsw -= 4) {
				for (msw = (u_int)(last_addr + 1) - delta; msw >= (u_int)first_addr;
				    msw = delta > msw ? 0 : msw - delta) {
					if ((msw + lsw) < (u_int)first_addr) {
						break;
					}

					error_addr = ((u_int *)(msw + lsw));
repeat_2:
					if ((local_actual_data = *error_addr) != pattern) {
						error = 1;
						msg_printf(DBG,"Error on first reverse read\n");
						msg_printf(DBG,"Address increment= %0x Delta= %0x\n",lsw, delta);
						memerr(error_addr,pattern,local_actual_data,sizeof(u_int));

						/*						ONERRCHK(2, err_mark, err_mode, loopflag, loop, done);*/
					}

					*error_addr = inverse_pattern;
repeat_3:
					if ((local_actual_data = *error_addr) != inverse_pattern ) {
						error = 1;
						msg_printf(DBG,"Error on second reverse read\n");
						msg_printf(DBG,"Address increment= %0x Delta= %0x\n",lsw, delta);
						memerr(error_addr,inverse_pattern,local_actual_data,sizeof(u_int));
						/*						ONERRCHK(2, err_mark, err_mode, loopflag, loop, done);*/
					}
				}
				/*				LOOPCHK(2, err_mark, loopflag, loop);*/

				/* This may be a bug.
				if ((msw + lsw) < (u_int)first_addr) {
					break;
				}
*/
			}

			pattern = inverse_pattern;
			inverse_pattern = ~pattern;
		} while (pattern != 0x55555555);
	}

busy(1);
done:
	if (error == 0)  {
		return(TRUE);
	}

	return(FALSE);
}







