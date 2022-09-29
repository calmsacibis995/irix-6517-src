/*
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.1 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern heart_byte_blast(volatile char *, char);
static int check_blast(volatile char *bp, char data,int n);

/*ARGSUSED*/
int
heart_byte(__uint32_t argc, char **argv)
{
	volatile char *bp = (volatile char *)PHYS_TO_K1_RAM(0x01000000);
	char *testname = "Heart uncached byte write";
	void (*cached_blast)(volatile char *, char);
	int errors = 0;
	int n = 1000000;

	msg_printf(VRB,"%s test @ 0x%x\n",testname,bp);

	cached_blast = (void (*)())PHYS_TO_K0(KDM_TO_PHYS(heart_byte_blast));
	while (n > 0) {
		cached_blast(bp,0);
		cached_blast(bp+128,128);
		errors += check_blast(bp,0,n);
		errors += check_blast(bp+128,128,n);
		cached_blast(bp,128);
		cached_blast(bp+128,0);
		errors += check_blast(bp,128,n);
		errors += check_blast(bp+128,0,n);
		if ((n%100) == 0) msg_printf(VRB,".");
		n--;
	}

	if (errors)
		msg_printf(SUM,"%s test FAILED! (%d errors)\n",testname,errors);

	return errors;
}

static int
check_blast(volatile char *bp, char data, int n)
{
#define datum(n)	((unsigned long)(data+n)<<(8*(7-n)))
	volatile unsigned long *dp = (volatile unsigned long *)bp;
	unsigned long exp;
	int i,errors = 0;

	for(i=0; i < 16; i++) {
		exp = datum(0)|datum(1)|datum(2)|datum(3)|
		      datum(4)|datum(5)|datum(6)|datum(7);
		if (*dp != exp) {
			msg_printf(VRB,
				"\nERROR! 0x%x pass %d %x (mem) vs %x!\n",
				dp,n,*dp,exp);
			errors++;
		}
		data += 8;
		dp++;
	} 

	return 0;
}
