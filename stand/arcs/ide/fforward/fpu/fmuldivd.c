/*
 * fpu/fmuldivd.c
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

#ident "$Revision: 1.9 $"

/*
 *					
 *	Floating Point Exerciser - basic functions with double precision
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
fmuldivd()
{
    volatile double f1, f2, f3, f4, f5, f6;
    register ulong status;
    register int i1;
    register int fail = 0;
    long long *xf2, *xf6;
    xf2 = (long long *)&f2;
    xf6 = (long long *)&f6;

    /* enable cache and fpu - cache ecc errors still enabled*/
    status = GetSR();
#ifdef TFP
    status |= SR_CU0|SR_CU1|SR_FR|SR_KX;
#else
    status |= SR_CU0|SR_CU1;
#endif
    SetSR(status);

    /* clear cause register */
    set_cause(0);

    /* clear fpu status register */
    SetFPSR(0);

    /* let's try some double precision division and multiplication */
    i1 = TESTVAL;	/* start with fixed point number */
    f1 = (double)i1;	/* convert to floating point number */
    i1 = 0x10000;	/* this is our divide test divisor */
    f3 = (double)i1;	/* convert to floating point number */
    f4 = f1 / f3;
    i1 = (int)f4;	/* convert back to fixed point number */
	if (i1 != TESTBAK) {
		msg_printf(DBG, "fmuldivd simple multiply/divide failed\n" );
		msg_printf(DBG, "expected 0x%x, got 0x%x\n", TESTBAK, i1 );
		fail = 1;
	}

    /* note: we're about to assume addition is working */
    f2 = f1 + f1;	/* we'll also need 2 times our test value */
    i1 = 2;
    f5 = (double)i1;
    f6 = f5 * f1;
	if (f6 != f2) {
		msg_printf(DBG, "fmuldivd simple multiply/divide failed\n" );
		msg_printf(DBG, "expected %llx, got %llx\n", *xf2, *xf6 );
		fail = 1;
	}

    /* report any error */
    return(fail);
}

