/*
 * fpu/fmulsubs.c
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
 *	Floating Point Exerciser - basic functions with simple single
 *	precision arithmetic (multiply and subtraction)
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

#define TESTPAT		0x02
#define FPU_COND_BIT	0x800000

int
fmulsubs(void)
{
    float f1, f2, f3;
    register long i1;
    register ulong status;
    register int fail;
    int *xf1, *xf3;
    unsigned long fpsr = 0;

    fail = 0;
    xf1 = (int *)&f1;
    xf3 = (int *)&f3;

    /* enable cache and fpu - cache ecc errors still enabled */
    status = GetSR();
    status |= SR_CU0|SR_CU1|SR_FR;
    SetSR(status);

    /* clear cause register */
    set_cause(0);

    /* clear fpu status register */
    SetFPSR(0);

    /* try some simple double precision arithmetic operations */
    i1 = TESTPAT;	/* start with fixed point number */
    f1 = (float)i1;	/* convert to floating point */
    f2 = f1 * f1;
    f3 = f2 - f1;
    if (f3 != f1) {
	msg_printf(DBG|PRCPU,
		"fmulsubs simple multiply/subtract failed\n"
		"expected %lx, got %lx\n", *xf1, *xf3 );
	fail = 1;
    }

    if (f3 == f2) {
	msg_printf(DBG|PRCPU,
		"fmulsubs simple multiply/subtract failed\n"
		"expected %lx, got %lx\n", *xf1, *xf3 );
	fail = 1;
    }

    i1 = (long)f3;	/* convert back to fixed point number */
    if (i1 != TESTPAT) {
	msg_printf(DBG|PRCPU,
		"fmulsubs simple multiply/subtract failed\n"
		"expected 0x%x, got 0x%x\n", TESTPAT, i1 );
		fail = 1;
    }

    if (fpsr = (GetFPSR()&(~FPU_COND_BIT))) {
	msg_printf(DBG|PRCPU,
		"fmulsubs simple multiply/subtract failed\n"
		"GetFPSR returned non-zero (0x%x)\n", fpsr);
        fail = 1;
    }

    /* report any error */
    return(fail);
}
