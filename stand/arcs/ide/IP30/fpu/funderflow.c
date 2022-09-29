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

#ident "$Revision: 1.3 $"

/*
 *					
 *	Floating Point Exerciser - basic functions with simple single
 *	underflow
 *					
 */

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include <fault.h>
#include <setjmp.h>
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

static jmp_buf fault_buf;

#define DIVUND		0x20000
#define	SR_EXCEPT	(SR_CU0 | SR_CU1 | SR_IEC | SR_FR | SR_KX)

extern void UNDERFLOW_OP(void);

int
funderflow()
{
    register int fail;
    register ulong status;

    fail = 0;
    status = GetSR();

    if (setjmp(fault_buf)) {
	if (!(GetFPSR() & DIVUND)) 
	    fail = 1;
	SetFPSR(0);

    } else {
	set_nofault(fault_buf);

	/* clear cause register */
	set_cause(0);

	/* enable cache and fpu - cache ecc errors still enabled */
	SetSR(SR_EXCEPT);

	/* clear the FPSR */
	SetFPSR(0);

	/* set up fpu status register for exception */
	SetFPSR(FP_ENABLE);

	UNDERFLOW_OP();

	DELAY(10);

	/* error if it does not generate an exception */
	fail = 1;
	nofault = 0;
    }

    /* return the status register to original state */
    SetSR(status);

    /* report any error */
    
    return(fail);
}
