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
#include "everr_hints.h"
#include "ide_msg.h"
#include "pattern.h"
#include "ip19.h"

static int cpu_loc[2];

#define TESTPAT		0x02

int
fmulsubs()
{
    float f1, f2, f3;
    register long i1;
    uint osr;
    unsigned retval=0;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (fmulsubs) \n");
    msg_printf(DBG, "Running on CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    /* enable cache and fpu - cache ecc errors still enabled */
    osr = GetSR();
    msg_printf(DBG, "Original status reg setting : 0x%x\n", osr);
    SetSR(osr | SR_CU1);
    msg_printf(DBG, "Status reg setting for test : 0x%x\n", GetSR());

    /* clear cause register */
    set_cause(0);

    /* clear fpu status register */
    SetFPSR(0);

    /* try some simple double precision arithmetic operations */
    i1 = TESTPAT;	/* start with fixed point number */
    f1 = i1;		/* convert to floating point */
    f2 = f1 * f1;
    f3 = f2 - f1;
    if (f3 != f1) {
	err_msg(FMULSUBS_RES, cpu_loc, f1, f3);
	retval = 1;
    }

    i1 = f3;		/* convert back to fixed point number */
    if (i1 != TESTPAT){
	err_msg(FMULSUBS_FIX, cpu_loc, TESTPAT, i1);
	retval = 1;
    }

    if (GetFPSR() & ~FPSR_COND)
    {
	err_msg(FMULSUBS_STS, cpu_loc, GetFPSR());
	retval = 1;
    }
    SetSR(osr);
    msg_printf(INFO, "Completed single precision FP mul/sub\n");

    /* report any error */
    return(retval);
}
