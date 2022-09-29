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
#include "everr_hints.h"
#include "ide_msg.h"
#include "pattern.h"
#include "ip19.h"

static int cpu_loc[2];

#define TESTVAL		0xdeadbeef
#define TESTBAK		0xffffdeae

int
fmuldivs()
{
    float f1, f2, f3, f4, f5, f6;
    uint osr;
    register long i1;
    unsigned retval = 0;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (fmuldivs) \n");
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

    /* let's try some single precision division and multiplication */
    i1 = TESTVAL;	/* start with fixed point number */
    f1 = i1;		/* convert to floating point number */
    i1 = 0x10000;	/* this is our divide test divisor */
    f3 = i1;		/* convert to floating point number */
    f4 = f1 / f3;
    i1 = f4;		/* convert back to fixed point number */
    if (i1 != TESTBAK){
	err_msg(FMULDIVS_DIV, cpu_loc, TESTBAK, i1);
	retval = 1;
    }

    /* note: we're about to assume addition is working */
    f2 = f1 + f1;	/* we'll also need 2 times our test value */
    i1 = 2;
    f5 = i1;
    f6 = f5 * f1;
    if (f6 != f2){
	err_msg(FMULDIVS_MUL, cpu_loc, f2, f6);
	retval = 1;
    }
    SetSR(osr);
    msg_printf(INFO, "Completed single precision FP mul/div\n");

    /* report any error */
    
    return(retval);
}
