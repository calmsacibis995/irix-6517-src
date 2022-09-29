/*
 * new3bit.c - new 3 bit memory test
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
 *
 * NOTE: for IP30,
 *           what needs to be done for IP30:  XXX
 *              -remove non-IP30 code
 *              -verify the IP30 defs
 */
#ident  "ide/godzilla/mem/new3bit.c  $Revision: 1.3 $"

#include "sys/types.h"
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include "libsk.h"
#include "d_mem.h"
#include "d_godzilla.h"
#include "libsc.h"

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


static u_int
threebit(u_int pat1, u_int pat2, u_int pat3, u_int *wrkp, u_int *endptr,
	 u_int mask)
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
	u_char xx[80];

	begptr = wrkptr;

	do {
		wrkptr= begptr;
		msg_printf(DBG,"  3-Bit test running  ");
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
		msg_printf(DBG,"    starting read back \n");

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
		gets((char *)xx);
	}
	goto bit3_ret1;

bit3_err2:
	if ((received ) != (pattern2 ) )  {
		printf("Test Error 2\n");
		printf("Failing address %x\n", wrkptr-1);
		printf("expected %x, actual %x, Xor %x\n", 
			pattern2, received, (pattern2 ^ received) );
		gets((char *)xx);
	}
	goto bit3_ret2;

bit3_err3:
	if ((received ) != (pattern3 ) )  {
		printf("Test Error 3\n");
		printf("Failing address %x\n", wrkptr-1);
		printf("expected %x, actual %x, Xor %x\n", 
			pattern3, received, (pattern3 ^ received) );
		gets((char *)xx);
	}
	goto bit3_ret3;
}

static u_int
dwd_3bit(__psunsigned_t begadr, __psunsigned_t endadr, u_int pat1,
	 u_int pat2, u_int pat3, u_int mask, u_int errlimit, u_int ecc_on)
{
	u_int	r_code;
	u_int	rtn_errors= 0;
	u_int	max_errors;

	rtn_errors = 0;
	max_errors = errlimit;

	/* Ensure word aligned */
	begadr &= (u_int) ~(sizeof(long) -1);
	endadr &= (u_int) ~(sizeof(long) -1);

	/* Do a sanity check */
	if (endadr <= begadr)
		return(rtn_errors);

	/*if (setjmp(buffer) )
		printf("Hello\n");
	*/	

	r_code = threebit(pat1, pat2, pat3, (u_int *)begadr, (u_int *)endadr, mask);
	msg_printf(DBG," r_code = %x, max_errors = %x \n", r_code, max_errors);
	msg_printf(DBG," ecc_on = %x\n", ecc_on);

	return 0 ; /* should return value */
}

int
three_ll(__psunsigned_t first, __psunsigned_t last/*, struct tstvar *tstparms*/)
{
	u_int i;
	u_int j;
	u_int ecc_on = 0;	/* Someday I will write this version */
	u_int errors;
	u_int error_limit = 1;	 /* XXX */

	msg_printf(DBG," ecc_on = %x\n", ecc_on);
	j= 0;
	last = last - 0x10;
	first = 0xaa00000c;
	for(i=0; i < 18; i += 3) {
		j++;
		msg_printf(DBG,"starting test pass %x\n", j);
		errors = dwd_3bit(first, last, tbl[i], tbl[i+1], tbl[i+2], (u_int)0, error_limit,  ecc_on);
		return(errors);
	}
	msg_printf(DBG," errors =  %x\n", errors);

	return 0;			/* pass */
}
