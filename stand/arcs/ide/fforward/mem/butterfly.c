/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Revision: 1.7 $"


#include <sys/types.h>
#include <libsc.h>
#include "mem.h"

static int butterfly_1(__psunsigned_t, __psunsigned_t, __psunsigned_t, u_int *);

static void
DisplayError(u_int errtype, __psunsigned_t exp, __psunsigned_t act,
	     volatile __psunsigned_t *addr)
{
	printf("Failing address %x\n", addr);
	printf("exp = %x, act = %x, XOR %x\n", exp, act, exp ^ act);
	printf("error code %x\n", errtype);
}

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

int
Butterfly_l(__psunsigned_t begadr, __psunsigned_t endadr)
{
	volatile __psunsigned_t range;
	u_int err_mode;
	int r_code;
	int error;


	msg_printf(VRB,"Butterfly_l(0x%08x, 0x%08x)\n", begadr, endadr);
	/* Make sure the addresses are in (double) word boundary.
	 *
	 * Even word start address, odd word end address
	 */
	if (begadr % 16)
		begadr += 16 - (begadr % 16);
	endadr = ( endadr & 8 ) ? ( (endadr - 8) & ~15 ) : (endadr & ~15) | 8;
	range = (endadr - begadr) >> 4;

	msg_printf(DBG,"\t Parameter range = %x\n",range);
	error = 0;

	r_code = butterfly_1(begadr, endadr, range, &err_mode);

	if (r_code == FAIL) {
		error++;
		if (err_mode == ERR_SKIP || err_mode == ERR_ABORT)
			return (FAIL);
	}

	return((error == 0) ? PASS : FAIL);
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
 *
 * 	mid_size - number of word in half of the range
 */
static int
butterfly_1(__psunsigned_t first_address, __psunsigned_t last_address,
	    __psunsigned_t mid_size, u_int *err_flag)
{
	register volatile __psunsigned_t *ptr1;
	register volatile __psunsigned_t *ptr2;
	register volatile __psunsigned_t data1;
	register volatile __psunsigned_t data2;
	register __psunsigned_t pattern1, pattern2;
	register u_int err_mode = 0;
	register int error;
	register int i;

	msg_printf(VRB," Starting address %x, End address %x, words %x\n",
		first_address, last_address, mid_size);

	error = 0;

#if _MIPS_SIM == _ABI64
	first_address &= DWORD_ADDR_MASK;
	last_address &= DWORD_ADDR_MASK;
#else
	first_address &= WORD_ADDR_MASK;
	last_address &= WORD_ADDR_MASK;
#endif

	ptr1 = (__psunsigned_t *)first_address;
	ptr2 = (__psunsigned_t *)last_address;
	msg_printf(VRB,"\tSetting all locations to zero\n");
	for (; ptr1 <= ptr2; ptr1++) {	/* Init all locations to zero */
		*ptr1 = 0;
	}

	pattern1 = FILL_A;
	pattern2 = FILL_5;

	ptr1 = (__psunsigned_t *)first_address;

	if (mid_size % 8) {
		/*
		 * Do the butterfly write word by word
		 *
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
			error++;
			DisplayError(1, pattern2, data2, ptr2);
		}
		if (data1 != pattern1) {
			error++;
			DisplayError(2, pattern1, data1, ptr1);

		}
	}

	msg_printf(DBG,"Last values ptr1 = %016x; ptr2 = %016x\n\n",ptr1,ptr2);

	*err_flag = err_mode;

	return ((error == 0) ? PASS : FAIL);
}
