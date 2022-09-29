/*
 * fmuldivs.c
 *
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.4 $"

/*
 *					
 *	Floating Point Exerciser - basic functions with single precision
 *	arithmetic (multiply and divide)
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

#define TESTVAL		0xdeadbeef
#define TESTBAK		0xffffdeae

int
fmuldivs(void)
{
    float f1, f2, f3, f4, f5, f6;
    register ulong status;
    register int i1;
    register int fail = 0;
    int *xf2, *xf6;
    xf2 = (int *)&f2;
    xf6 = (int *)&f6;

    /* enable cache and fpu - cache ecc errors still enabled */
    status = GetSR();
    status |= SR_CU0|SR_CU1|SR_FR;
    SetSR(status);

    /* clear cause register */
    set_cause(0);

    /* clear fpu status register */
    SetFPSR(0);

    /* let's try some single precision division and multiplication */
    i1 = TESTVAL;	/* start with fixed point number */
    f1 = (float)i1;	/* convert to floating point number */
    i1 = 0x10000;	/* this is our divide test divisor */
    f3 = (float)i1;	/* convert to floating point number */
    f4 = f1 / f3;
    i1 = (int)f4;	/* convert back to fixed point number */
    if (i1 != TESTBAK) {
	msg_printf(DBG|PRCPU,
		"fmuldivs simple multiply/divide failed\n"
		"expected 0x%x, got 0x%x\n", TESTBAK, i1 );
	fail = 1;
    }

    /* note: we're about to assume addition is working */
    f2 = f1 + f1;	/* we'll also need 2 times our test value */
    i1 = 2;
    f5 = (float)i1;
    f6 = f5 * f1;
    if (f6 != f2) {
	msg_printf(DBG|PRCPU,
		"fmuldivs simple multiply/divide failed\n"
		"expected %lx, got %lx\n", *xf2, *xf6 );
	fail = 1;
    }

    /* report any error */
    return(fail);
}
