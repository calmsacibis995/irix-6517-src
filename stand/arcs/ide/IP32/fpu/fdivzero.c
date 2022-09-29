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

#ident "$Revision: 1.1 $"

/*
 *					
 *	Floating Point Exerciser - basic functions with simple single
 *	devided by 0 
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include "libsc.h"
#include "uif.h"

#define DIVIDEND	333333
#define DIVISOR		0
#define DIVZERO		0x8020

int
fdivzero()
{
    volatile float f1, f2, f3;
    register long i1, i2;
    register long status;
    register int fail;

    fail = 0;

    /* enable cache and fpu - cache ecc errors still enabled */
    status = GetSR();
    status |= SR_CU0|SR_CU1;
    SetSR(status);

    /* clear cause register */
    set_cause(0);

    /* clear fpu status register */
    SetFPSR(0);

    /* Convert to floating point */
    f1 = DIVIDEND;
    f2 = DIVISOR;
    f3 = f1/f2;

    if (GetFPSR() != DIVZERO) {
        fail = 1;
    }

    i1 = f1;
    if (i1 != DIVIDEND){
	fail = 1;
    }
    i2 = f2;
    if (i2 != DIVISOR){
	fail = 1;
    }

    /* report any error */
    
    return(fail);
}
