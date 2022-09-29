/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Revision: 1.12 $"

/*
 *			M a r c h i n g( )  O n e s  and  Z e r o e s
 *
 *  This algorithm is a classic, a real oldie but goodie.  It tests for
 *  stuck-at faults and catches some patern sensitivity problems.  It takes 14n
 *  memory acceses.
 *
 *  This algorithm makes eight passes through memory.  The first pass sets all
 *  memory to zeroes.  The next pass ascends thru memory checking each location
 *  for zeroes then sets the location to ones and verifies that it is still
 *  ones.  The next pass decends thru memory checking for ones, setting to
 *  zeroes and verifying zeroes are still present.  The last four passes are
 *  analogous to the first four except that ones and zeroes are inverted.
 *
 *  Inputs:  
 *	first_address = address of the first memory location to be tested
 *	last_address  = address of the last location to be tested.
 *	
 */

#include <sys/types.h>
#include <libsk.h>
#include "mem.h"

#define RESETFLAGS
#define ONERRCHK
#define LOOPCHK

int
March_l(__psunsigned_t first_address, __psunsigned_t last_address)
{
	register __psunsigned_t	 *first_addr;
	register __psunsigned_t	 *last_addr;
	register __psunsigned_t	 *ptr;
	register __psunsigned_t	 last_value_read;
	register u_int err_mode;
	register int err_mark;
	register __psunsigned_t	 a;		/* data pattern and looping control */
	register __psunsigned_t	 b;
	register int loopflag;
	int error;

	/* make sure the addresses are on word boundary
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
	msg_printf(DBG,"March_l (%08x, %08x)\n", first_address, last_address);

#if IP26
	flush_cache();
#endif

start:
	first_addr = (__psunsigned_t *)first_address;
	last_addr = (__psunsigned_t *)last_address;
	error = 0;
	/*
	 * Set looping control for each major pass.
	 */
	a = ~( b = 0 );

	RESETFLAGS(err_mark, loopflag, err_mode);
loop:

	do {
		/*
		 * Set memory to initial value (x'00000000').
		 */
		msg_printf(VRB,"^ Fill %x\n",b);
		fillmemW(first_addr, last_addr, b, 0);

		/*
		 * Ascend thru memory checking for last setting, resetting 
		 * and then verifying.
		 */
		msg_printf(VRB,"^ Check %x, fill %x\n",b,a);
		for (ptr = first_addr; ptr <= last_addr; ) {
			/*
			 * Verify previously set value set ok.
			 */
			if ((last_value_read = *ptr) != b) {
				error++;
				memerr((caddr_t)ptr,b,last_value_read, sizeof(__psunsigned_t));
				/*				ONERRCHK(1, err_mark, err_mode,loopflag,loop,done);*/
			}
			*ptr = a;		/* change current value to its complement */
			msg_printf(DBG,"%x in %x\n", *ptr,ptr);
			/*
			 * Ensure current value has been changed.
			 */
			if ((last_value_read = *ptr) != a) {
				error++;
				memerr((caddr_t)ptr,a,last_value_read, sizeof(__psunsigned_t));
				/*				ONERRCHK(1, err_mark, err_mode,loopflag,loop,done);*/
			}
			++ptr;
		}
		/*		LOOPCHK(1, err_mark, loopflag, loop);*/
#if defined(SABLE) && defined(DEBUG)
		*(last_addr - 1) = 0xabababab;
#endif
		/*
		 * Decend thru memory checking for last setting, resetting 
		 * and then verifying.
		 */
		msg_printf(VRB,"v Check %x, fill %x\n",a,b);

		for (ptr = last_addr; ptr >= first_addr; ptr--) {

			/*
			 * Verify previously set value set ok.
			 */
			if ((last_value_read = *ptr) != a) {
				error++;
				memerr((caddr_t)ptr,a,last_value_read, sizeof(__psunsigned_t));
				/*				ONERRCHK(2, err_mark, err_mode,loopflag,loop,done);*/
			}
			*ptr = b;		/* change current value to its complement */
			msg_printf(DBG,"%x in %x\n", *ptr,ptr);
			/*
			 * Insure current value has been changed.
			 	*/
			if ((last_value_read = *ptr) != b) {
				error++;
				memerr((caddr_t)ptr,b,last_value_read, sizeof(__psunsigned_t));
				/*				ONERRCHK(2, err_mark, err_mode,loopflag,loop,done);*/
			}
		}
		/*		LOOPCHK(2, err_mark, loopflag, loop);*/
		b = ~(a = b);			/* switch ones and zeroes */
	} while (a == 0);

done:
	if (error == 0) {
		return(TRUE);
	} else {
		return(FALSE);
	}
}
