static char rcsid[] = "$Header: /proj/irix6.5.7m/isms/stand/arcs/ide/fforward/mem/RCS/new3bit.c,v 1.3 1995/06/09 20:34:57 jeffs Exp $";

/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

/*
 *		t h r e e b i t ()
 *
 * The three bit test checks that all
 *
 *  This test supports looping and continue on error. When a failure is
 *  detected the error function address passed in is called. The macro
 *  that supports looping makes a call to a routine 'scope-trigger'. This
 *  subroutine is machine specific and can be used to provide a scope
 *  trigger when an error is detected an looping is enabled.
 *
 *  Inputs:  
 *	first_address = address of the first meory location to be tested
 *	last_address  = address of the last 32 bit word to be tested.
 *	tstparms = Address of test parameter struct, see struct definition
 */

#include "sys/types.h"
#include "libsc.h"

#define Dprintf2	printf
#define Dprintf		printf


extern u_int *WriteLoop16(u_int *, u_int *, u_int, u_int, u_int);

/*
 * Define data pattern. Note, every three data patterns
 * constitues a new pass of the test.
 */
static	u_int tbl[] = {
		0xb6db6db6, 0xdb6db6db, 0x6db6db6d,
		0x24924924, 0x92492492, 0x49249249,
		0x6db6db6d, 0xb6db6db6, 0xdb6db6db,
		0x49249249, 0x24924924, 0x92492492,
		0xbd6bd6bd, 0x6bd6bd6b, 0xd6bd6bd6,
		0x92492492, 0x49249249, 0x24924924,
		};

static dwd_3bit(u_int, u_int, u_int, u_int, u_int, u_int, int);
static int threebit(u_int, u_int, u_int, u_int *, u_int *, u_int);

int
three_ll(__psunsigned_t first, __psunsigned_t last)
{
	u_int i;
	u_int j;
	u_int ecc_on = 0;	/* Someday I will write this version */

	j= 0;
	last = last - 0x10;
	first = 0xaa00000c;
	for(i=0; i < 18; i += 3) {
		j++;
		Dprintf2("starting test pass %x\n", j);
		dwd_3bit(first, last, tbl[i], tbl[i+1], tbl[i+2], 0, ecc_on);
	}
}

int
static dwd_3bit(u_int begadr, u_int endadr, u_int pat1, u_int pat2, u_int pat3,
		u_int mask, int ecc_on)
{
	u_int	r_code;
	u_int	rtn_errors= 0;
	u_int	max_errors;

	rtn_errors = 0;

	/* Ensure word aligned *
	begadr &= (u_int) ~(sizeof(long) -1);
	endadr &= (u_int) ~(sizeof(long) -1);

	/* Do a sanity check */
	if (endadr <= begadr)
		return(rtn_errors);

	/*if (setjmp(buffer) )
		printf("Hello\n");
	*/	

	r_code = threebit(pat1, pat2, pat3, (u_int *)begadr, (u_int *)endadr, mask);

}


static int threebit(u_int pat1, u_int pat2, u_int pat3, u_int *wrkp,
		    u_int *endptr, u_int mask)
{
	register u_int pattern1= pat1;
	register u_int pattern2= pat2;
	register u_int pattern3= pat3;
	register u_int *wrkptr= wrkp;
	register u_int received;
	register u_int *nearlydone = endptr -(16 *14);
	register u_int received2;
	register u_int received3;
	u_int *begptr ;
	u_int pattern4 = pattern1;
	u_int i;
	u_char xx[80];

	begptr = wrkptr;

	do {
		wrkptr= begptr;
		Dprintf2("  3-Bit test running  ");
		printf("wrkptr = %x\n", wrkptr);

		/* Fill most of memory up */
		wrkptr = WriteLoop16(wrkptr, nearlydone, pattern1, pattern2, pattern3);

		/* Finish up the last locations */
		while (1) {
			if (wrkptr > endptr)
				break;
			*wrkptr = pattern1;			
			wrkptr += 4;
			if (wrkptr > endptr)
				break;
			*wrkptr = pattern2;			
			wrkptr += 4;
			if (wrkptr > endptr)
				break;
			*wrkptr = pattern3;			
			wrkptr += 4;
		}


		/* Now start the read back */
		Dprintf2("    starting read back \n");

		wrkptr = begptr;
	
		while (wrkptr < nearlydone ) {
			received = *wrkptr ;		/* READ */
			if ( received != pattern1)	
				goto bit3_err1;

			received2 = *(wrkptr +4);		/* READ */
			if ( received2 != pattern2)	
				goto bit3_err2;

			received3 = *(wrkptr +8);		/* READ */
			if ( received3 != pattern3)	
				goto bit3_err3;

			received = *(wrkptr +12);		/* READ */
			if ( received != pattern1)	
				goto bit3_err1;

			received2 = *(wrkptr +16);		/* READ */
			if ( received2 != pattern2)	
				goto bit3_err2;

			received3 = *(wrkptr +20);		/* READ */
			if ( received3 != pattern3)	
				goto bit3_err3;
			wrkptr +=24;
		}

		printf("    begin slow reads %x\n", wrkptr);
		/* 
		 * Finish up the last reads here
		 */
		while (1) {
			if (wrkptr > endptr)
				break;
			received = *wrkptr;		/* READ */
			if (received != pattern1)
				goto bit3_err1;

bit3_ret1:
			wrkptr +=4;
			if (wrkptr > endptr)
				break;
			received = *wrkptr;		/* READ */
			if (received != pattern2)
				goto bit3_err2;
bit3_ret2:
			wrkptr +=4;
			if (wrkptr > endptr)
				break;
			received = *wrkptr;		/* READ */
			if (received != pattern3)
				goto bit3_err3;
bit3_ret3:
			wrkptr +=4;
			continue;
		}
		/* Rotate patterns */
		received = pattern1 ^ mask;
		pattern1 = pattern2 ^ mask;
		pattern2 = pattern3 ^ mask;
		pattern3 = received ^ mask;
	} while (pattern1 != pattern4);

	return(0);

/***************************************/


bit3_err1:
	if ((received ) != (pattern1 ) ) {
		printf("Test Error 1\n");
		printf("Failing address %x\n", wrkptr-1);
		printf("expected %x, actual %x, Xor %x\n", 
			pattern1, received, (pattern1 ^ received) );
		gets(xx);
	}
	goto bit3_ret1;

bit3_err2:
	if ((received ) != (pattern2 ) )  {
		printf("Test Error 2\n");
		printf("Failing address %x\n", wrkptr-1);
		printf("expected %x, actual %x, Xor %x\n", 
			pattern2, received, (pattern2 ^ received) );
		gets(xx);
	}
	goto bit3_ret2;

bit3_err3:
	if ((received ) != (pattern3 ) )  {
		printf("Test Error 3\n");
		printf("Failing address %x\n", wrkptr-1);
		printf("expected %x, actual %x, Xor %x\n", 
			pattern3, received, (pattern3 ^ received) );
		gets(xx);
	}
	goto bit3_ret3;
			
}

