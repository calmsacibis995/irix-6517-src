/*
 * fpu/finvalid.c
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

#ident "$Revision: 1.5 $"

/*
 *					
 *	Floating Point Exerciser - basic functions with simple single
 *	invalid
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include <setjmp.h>
#include <fault.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "pattern.h"
#include "ip19.h"

static int cpu_loc[2];
static jmp_buf fault_buf;

#define DIVIDEND	0
#define FPUINVALID	0x10f80		/* all exceptions enabled , V cause */
#define	SR_EXCEPT	(SR_CU0 | SR_CU1 | SR_IEC & ~SR_BEV)

int
finvalid()
{
    volatile float f1;
    register long i1, i2;
    uint osr;
    unsigned retval=0;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (finvalid) \n");
    msg_printf(DBG, "Running on CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    if (setjmp(fault_buf))
    {
	msg_printf(DBG, "Floating point status : 0x%x\n", GetFPSR());
	if ((GetFPSR() & ~FPSR_COND) != FPUINVALID) 
	{
	    err_msg(FINV_STS, cpu_loc, GetFPSR());
	    retval = 1;
	}
	SetFPSR(0);

        i1 = f1;
        if (i1 != DIVIDEND){
            err_msg(FINV_FIX, cpu_loc, DIVIDEND, i1);
            retval = 1;
	}
    }
    else
    {
	nofault = fault_buf;

	/* clear cause register */
	set_cause(0);

	/* enable cache and fpu - cache ecc errors still enabled */
	osr = GetSR();
	msg_printf(DBG, "Original status reg setting : 0x%x\n", osr);
	SetSR(osr | SR_CU1);
	msg_printf(DBG, "Status reg setting for test : 0x%x\n", GetSR());

	/* clear the FPSR */
	SetFPSR(0);

	/* set up fpu status register for exception */
	SetFPSR(FPSR_ENABLE);

	/* Convert to floating point */
	f1 = 0.0;
	f1 = f1/f1;

	DELAY(10);

	/*
	 * if the error does not generate an exception, print the error
	 * message and exit
	 */
	err_msg(FINV_NOEXCP, cpu_loc);
	retval = 1;
	clear_nofault();
    }
    SetSR(osr);
    msg_printf(INFO, "Completed FP invalid exception\n");

    /* report any error */
    return(retval);
}
