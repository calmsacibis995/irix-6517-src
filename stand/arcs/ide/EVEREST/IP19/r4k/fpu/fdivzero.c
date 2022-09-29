/*
 * fpu/fdivzero.c
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
 *	devided by 0 
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

#define DIVIDEND	333333
#define DIVISOR		0
#define DIVZERO		0x8020

int
fdivzero()
{
    volatile float f1, f2, f3;
    register long i1, i2;
    uint osr;
    unsigned retval=0;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (fdivzero) \n");
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

    /* Convert to floating point */
    f1 = DIVIDEND;
    f2 = DIVISOR;
    f3 = f1/f2;

    if ((GetFPSR() & ~FPSR_COND) != DIVZERO) {
	err_msg(FDIVZRO_STS, cpu_loc, GetFPSR());
	retval = 1;
    }

    i1 = f1;
    if (i1 != DIVIDEND){
	err_msg(FDIVZRO_DIVD, cpu_loc, DIVIDEND, i1);
	retval = 1;
    }
    i2 = f2;
    if (i2 != DIVISOR){
	err_msg(FDIVZRO_DIVS, cpu_loc, DIVISOR, i2);
	retval = 1;
    }
    SetSR(osr);
    msg_printf(INFO, "Completed FP divide by zero exception\n");

    /* report any error */
    
    return(retval);
}
