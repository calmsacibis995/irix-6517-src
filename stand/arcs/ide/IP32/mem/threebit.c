static char rcsid[] = "$Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP32/mem/RCS/threebit.c,v 1.2 1997/05/15 16:08:58 philw Exp $";

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
#include "uif.h"
#include <stdio.h>

#define Dprintf2	printf
#define Dprintf		printf


extern u_int *WriteLoop14();

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
		0xe7fe7fe7, 0x7fe7fe7f, 0xfe7fe7fe,
		0x0fb0fb0f, 0xfb0fb0fb, 0xb0fb0fb0,
		0x08f08f08, 0x8f08f08f, 0xf80f80f8,
		0x01201201, 0x20120120, 0x12012012,
		0x80380380, 0x38038038, 0x03803803,
		0x3e13e13e, 0x13e13e13, 0xe13e13e1,
		0xf7ef7ef7, 0xef7ef7ef, 0x7ef7ef7e,
		0x7fd7fd7f, 0xd7fd7fd7, 0xfd7fd7fd,
		0xb7fb7fb7, 0xfb7fb7fb, 0x7fb7fb7f
		};

static int threebit();
static int dwd_3bit();
three_l(u_int first, u_int last, struct tstvar *tstparms )
{
	u_int i;
	u_int j;
	u_int ecc_on = 0;	/* Someday I will write this version */

	j= 0;
	last = last - 0x10;
	for(i=0; i < 18; i += 3) {
		j++;
		Dprintf2("starting test pass %d\n", j);
		dwd_3bit(first, last, tbl[i], tbl[i+1], tbl[i+2], 0, ecc_on);
		}

	last = last - 0x10;
	for(i=0; i < 18; i += 3) {
		j++;
		Dprintf2("Inverted data, starting test pass %d\n", j);
		dwd_3bit(first, last, ~tbl[i], ~tbl[i+1], ~tbl[i+2], 0, ecc_on);
		}
}

int
static dwd_3bit(begadr, endadr, pat1, pat2, pat3, mask, errlimit, ecc_on)
u_int	begadr, endadr;
u_int	pat1, pat2, pat3;
u_int	mask, errlimit;
int	ecc_on;
{
	u_int	r_code;
	u_int	rtn_errors= 0;
	u_int	max_errors;

	rtn_errors = 0;
	max_errors = errlimit;

	/* Ensure word aligned *
	begadr &= (u_int) ~(sizeof(long) -1);
	endadr &= (u_int) ~(sizeof(long) -1);

	/* Do a sanity check */
	if (endadr <= begadr)
		return(rtn_errors);

	/*if (setjmp(buffer) )
		msg_printf(VRB,"Hello\n");
	*/	

	r_code = threebit(pat1, pat2, pat3, (u_int *)begadr, (u_int *)endadr, mask);

}


static int threebit(u_int pat1, u_int pat2, u_int pat3, u_int *wrkp, u_int *endptr, u_int mask)
{
	register u_int pattern1= pat1;
	register u_int pattern2= pat2;
	register u_int pattern3= pat3;
	register u_int *wrkptr= wrkp;
	register u_int received;
	register u_int *nearlydone = endptr -14;
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
		msg_printf(VRB,"wrkptr = %x\n", wrkptr);

		/* Fill most of memory up */
		wrkptr = WriteLoop14(wrkptr, nearlydone, pattern1, pattern2, pattern3);

		/* Finish up the last locations */
		while (1) {
			if (wrkptr > endptr)
				break;
			*wrkptr++ = pattern1;			
			if (wrkptr > endptr)
				break;
			*wrkptr++ = pattern2;			
			if (wrkptr > endptr)
				break;
			*wrkptr++ = pattern3;			
		}


		/* Now start the read back */
		Dprintf2("    starting read back \n");

		wrkptr = begptr;
	
		while (wrkptr < nearlydone ) {
			received = *wrkptr++;		/* READ */
			if ( received != pattern1)	
				goto bit3_err1;

			received2 = *wrkptr++;		/* READ */
			if ( received2 != pattern2)	
				goto bit3_err2;

			received3 = *wrkptr++;		/* READ */
			if ( received3 != pattern3)	
				goto bit3_err3;

			received = *wrkptr++;		/* READ */
			if ( received != pattern1)	
				goto bit3_err1;

			received2 = *wrkptr++;		/* READ */
			if ( received2 != pattern2)	
				goto bit3_err2;

			received3 = *wrkptr++;		/* READ */
			if ( received3 != pattern3)	
				goto bit3_err3;
		}

		msg_printf(VRB,"    begin slow reads %x\n", wrkptr);
		/* 
		 * Finish up the last reads here
		 */
		while (1) {
			if (wrkptr > endptr)
				break;
			received = *wrkptr++;		/* READ */
			if (received != pattern1)
				goto bit3_err1;

bit3_ret1:
			if (wrkptr > endptr)
				break;
			received2 = *wrkptr++;		/* READ */
			if (received2 != pattern2)
				goto bit3_err2;
bit3_ret2:
			if (wrkptr > endptr)
				break;
			received3 = *wrkptr++;		/* READ */
			if (received3 != pattern3)
				goto bit3_err3;
bit3_ret3:
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
		msg_printf(ERR,"Test Error 1\n");
		msg_printf(ERR,"Failing address %x\n", wrkptr-1);
		msg_printf(ERR,"expected %x, actual %x, Xor %x\n", 
			pattern1, received, (pattern1 ^ received) );
		/* gets(xx); */
	}
	goto bit3_ret1;

bit3_err2:
	if ((received2 ) != (pattern2 ) )  {
		msg_printf(ERR,"Test Error 2\n");
		msg_printf(ERR,"Failing address %x\n", wrkptr-1);
		msg_printf(ERR,"expected %x, actual %x, Xor %x\n", 
			pattern2, received2, (pattern2 ^ received2) );
		/* gets(xx); */
	}
	goto bit3_ret2;

bit3_err3:
	if ((received3 ) != (pattern3 ) )  {
		msg_printf(ERR,"Test Error 3\n");
		msg_printf(ERR,"Failing address %x\n", wrkptr-1);
		msg_printf(ERR,"expected %x, actual %x, Xor %x\n", 
			pattern3, received3, (pattern3 ^ received3) );
		/* gets(xx); */
	}
	goto bit3_ret3;
			
}

