/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Revision: 1.1 $"

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
 *	first_address = address of the first meory location to be tested
 *	last_address  = address of the last 32 bit word to be tested.
 *	
 */

#include "sys/types.h"
#include "mem.h"

#define WORD_ADDR_MASK	0xfffffffc
#define RESETFLAGS
#define ONERRCHK
#define LOOPCHK

March_l(int first_address, int last_address)
{
	register u_int *first_addr;
	register u_int *last_addr;
	register u_int *ptr; 
	register u_int last_value_read; 
	register u_int err_mode; 
	register int err_mark;
	register int a;		/* data pattern and looping control */
	register int b; 
	register int loopflag;
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
	msg_printf(DBG,"March_l (%08x, %08x)\n", first_address, last_address);
	

start:
	first_addr = (u_int *)first_address;
	last_addr = (u_int *)last_address;
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
		fillmemW( first_addr, last_addr, b, 0);
		/*for (ptr = first_addr; ptr <= last_addr; ) {
			*ptr++ = b;
		}*/

		/*
		 * Ascend thru memory checking for last setting, resetting 
		 * and then verifying.
		 */
		for (ptr = first_addr; ptr <= last_addr; ) { 
			/*
			 * Verify previously set value set ok.
			 */

			if ((last_value_read = *ptr) != b) {
				error = 1;
				memerr(ptr,b,last_value_read, sizeof(u_int));
/*				ONERRCHK(1, err_mark, err_mode,loopflag,loop,done);*/
			}
			*ptr = a;		/* change current value to its complement */

			/*
			 * Insure current value has been changed.
			 */
			if ((last_value_read = *ptr) != a) {
				error = 1;
				memerr(ptr,a,last_value_read, sizeof(u_int));
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
		for (ptr = last_addr; ptr >= first_addr; ptr--) {
			/*
			 * Verify previously set value set ok.
			 */
			if ((last_value_read = *ptr) != a) {
				error = 1;
				memerr(ptr,a,last_value_read, sizeof(u_int));
/*				ONERRCHK(2, err_mark, err_mode,loopflag,loop,done);*/
			}
			*ptr = b;		/* change current value to its complement */

			/*
			 * Insure current value has been changed.
			 */
			if ((last_value_read = *ptr) != b) {
				error = 1;
				memerr(ptr,b,last_value_read, sizeof(u_int));
/*				ONERRCHK(2, err_mark, err_mode,loopflag,loop,done);*/
			}
		}
/*		LOOPCHK(2, err_mark, loopflag, loop);*/

		b = ~(a = b);			/* switch ones and zeroes */
	} while (a == 0);

done:
	if ((error == 0) ) {
		return(TRUE);
	}

	return(FALSE);
}






