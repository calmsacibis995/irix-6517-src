/*
 * fpu/funderflow.c
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
 *	underflow
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include <fault.h>
#include <setjmp.h>
#include "pattern.h"
#include "ip19.h"
#include <ide_msg.h>
#include <everr_hints.h>

/*
 * externs defined over in saio/lib, mostly in faultasm.s
 */
extern uint _rmpstatus_save;
extern uint _rmperadhi_save;
extern uint _rmperadlo_save;

static int cpu_loc[2];
static jmp_buf fault_buf;

#define DIVUND		0x20000	/*underflow is never set; tininess sets the
					unimplemented bit*/
#define	SR_EXCEPT	(SR_CU0 | SR_CU1 | SR_IEC & ~SR_BEV)

int
funderflow()
{
    uint osr, temp;
    unsigned retval=0;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (funderflow) \n");
    msg_printf(DBG, "Running on CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    if (setjmp(fault_buf))
    {
	temp = GetFPSR();
	msg_printf(DBG, "Floating point status : 0x%x\n", temp);
	if (!(temp & DIVUND)) {
	    err_msg(FUNDER_EXCP, cpu_loc, temp);
	    retval = 1;
	}
	SetFPSR(0);
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
	msg_printf(DBG, "Floating point status setting for test : 0x%x\n", GetFPSR());

	UNDERFLOW_OP();
    	GetFPSR();			/* let fp operation finish */

	DELAY(10);

	/*
	 * if the error does not generate an exception, print the error
	 * message and exit
	 */
	err_msg(FUNDER_NOEXCP, cpu_loc);
	retval = 1;
	clear_nofault();
    }
    SetSR(osr);
    msg_printf(INFO, "Completed FPU Underflow Exception\n");

    /* report any error */
    
    return(retval);
}
