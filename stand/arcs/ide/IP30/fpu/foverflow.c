/*
 * fpu/foverflow.c
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
 *	overflow  
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

#ifdef TFP
#define OVERFLOW	0x14
#else
#define OVERFLOW	0x5014
#endif

extern void OVERFLOW_OP(void);
int
foverflow(void)
{
    register ulong status;
    register int fail;

    fail = 0;

    /* enable cache and fpu - cache ecc errors still enabled */
    status = GetSR();
    status |= SR_CU0|SR_CU1|SR_FR;
    SetSR(status);

    /* clear cause register */
    set_cause(0);

    /* clear fpu status register */
    SetFPSR(0);

    OVERFLOW_OP();

    if (GetFPSR() != OVERFLOW)
        fail = 1;

    /* report any error */
    
    return(fail);
}
