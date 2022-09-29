static char rcsid[] = "$Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP32/mem/RCS/butterfly.c,v 1.2 1997/05/15 16:08:16 philw Exp $";

/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */


#include "sys/types.h"
/*#include "mips/cpu.h"*/
#include "mem.h"
/*
#include "error.h"
#include "menu.h"
*/


#define Tprintf2	printf
#define Tprintf		printf
#define Dprintf2	printf
#define WORD_ADDR_MASK	0xfffffffc
#define FAIL	1
#define PASS	0
#define ERR_ABORT 99
#define ERR_SKIP  99
/*
 * Butterfly()
 *
 *	This test tests the memory in a bank by bank basis. The first_address
 *	and last_address arguments will be used to determine the boundary
 *	within a bank.
 *	The extern struct bank_info[] provides the beginning address and the
 *	size (in byte) of the banks.
 *	Memory locations are accessed in a butterfly fashion, i.e. alternating
 * 	high and low memory locations are access in sequence. The data pattern 
 * 	is also complemented for each memory store.
 *	The effect of this test is to stress the address line drivers and the 
 * 	data line transcievers. It also stress the R4000 on-chip write buffer.
 */

Butterfly_l(u_int begadr, u_int endadr)
{
	register volatile int range;
	u_int err_mode;
	int r_code;
	int error;


	Tprintf2("Butterfly_l(0x%08x, 0x%08x)\n", begadr, endadr);
	/* make sure the addresses are in word boundary
	*/
	/* Even word start address, odd word end address */
	begadr &= ~7;
	endadr = (endadr &4) ? ((endadr -4) & ~7) | 4 : (endadr & ~7) | 4;

	range = (endadr - begadr) >> 3;

	error = 0;


	r_code = butterfly_1(begadr, endadr, range, &err_mode);

	if (r_code == FAIL) {
		error = 1;
		if(err_mode == ERR_SKIP || err_mode == ERR_ABORT)
			return (FAIL);
	}

	if(error == 0)
		return(PASS);

	return(FAIL);
}


/*
 * butterfly_1()
 *	This routine is specially designed to test the address line drivers
 *	for the memory subsystem in a bank by bank basis. 
 *	The arguments first_address and last_address
 *	are WORD addresses. The last_address should be the one's
 *	compliment of the first_address to achieve the address line stressing
 *	effect. If the mid_size is a modulo of 8, the test will write to
 *	memory in blocks of eight words. This will put 16 store word
 *	instructions all together to achieve a higher stress effect.
 */

int
butterfly_1(first_address, last_address, mid_size, err_flag)
u_int first_address;
u_int last_address;
int mid_size;		/* number of word in half of the range */
u_int *err_flag;
{
	register volatile u_int *ptr1;
	register volatile u_int *ptr2;
	register volatile u_int data1;
	register volatile u_int data2;
	register u_int pattern1, pattern2;
	register int i;
	register int loopflag;
	register u_int err_mode;
	register int error;
	int err_mark;

	Dprintf2(" Starting address %x, End address %x, words %x\n",
		first_address, last_address, mid_size);

	error = 0;
	first_address &= WORD_ADDR_MASK;
	last_address &= WORD_ADDR_MASK;

	/*RESETFLAGS(err_mark, loopflag, err_mode);*/
loop:
	ptr1 = (u_int *)first_address;
	ptr2 = (u_int *)last_address;

	for(; ptr1 <= ptr2; ptr1++) {	/* Init all location to zero */
		*ptr1 = 0;
	}

	pattern1 = 0xaaaaaaaa;
	pattern2 = 0x55555555;

	ptr1 = (u_int *)first_address;

	if (mid_size % 8) {
		/*
		 * Do the butterfly write word by word
		 */
		for(i=0; i < mid_size; i++) {
			*ptr1 = pattern1;
			*ptr2 = pattern2;
			ptr1++;
			ptr2--;
		}
	}
	else {
		/*
		 * Do the butterfly write in blocks of 8 words. This will put 16
		 * store word operations together consecutively.
		 */
		for(i=0; i < mid_size; i += 8) {
			*ptr1 = pattern1;
			*ptr2 = pattern2;
			*(ptr1 + 1) = pattern1;
			*(ptr2 - 1) = pattern2;
			*(ptr1 + 2) = pattern1;
			*(ptr2 - 2) = pattern2;
			*(ptr1 + 3) = pattern1;
			*(ptr2 - 3) = pattern2;
			*(ptr1 + 4) = pattern1;
			*(ptr2 - 4) = pattern2;
			*(ptr1 + 5) = pattern1;
			*(ptr2 - 5) = pattern2;
			*(ptr1 + 6) = pattern1;
			*(ptr2 - 6) = pattern2;
			*(ptr1 + 7) = pattern1;
			*(ptr2 - 7) = pattern2;
			ptr1 += 8;
			ptr2 -= 8;
		}
	}

	/*
	 * Do the butterfly read word by word in the opposite order as the
	 * write.
	 */
	for(i=0; i < mid_size; i++) {
		data2 = *++ptr2;
		data1 = *--ptr1;
		if(data2 != pattern2) {
			error = 1;
			DisplayError(1, pattern2, data2, ptr2);
			/*ONERRCHK(1, err_mark, err_mode, loopflag, loop, done);*/
		}
		if (data1 != pattern1) {
			error = 1;
			DisplayError(2, pattern1, data1, ptr1);
			/*ONERRCHK(1, err_mark, err_mode, loopflag, loop, done);*/

		}
	}
	/*LOOPCHK(1, err_mark, loopflag, loop);*/

done:
	*err_flag = err_mode;
	if(error == 0)
		return(PASS);

	return(FAIL);
}
DisplayError(u_int errtype, u_int exp, u_int act, u_int addr)
{
	printf("Failing address %x\n", addr);
	printf("exp = %x, act = %x, XOR %x\n", exp, act, exp ^ act);
	printf("error code %x\n", errtype);
}
