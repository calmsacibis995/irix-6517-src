/*
 * fpu/fpmem.c
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

#ident "$Revision: 1.7 $"

/*
 *					
 *	Floating Point Exerciser - load/store memory
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

unsigned int i_buf[32];
unsigned int o_buf[32];

extern void LOAD_FPU(int, unsigned int *);
extern void STORE_FPU(int, unsigned int *);

int
fpmem()
{
    register int fail = 0;
    register ulong status;
    int i;

    /* enable cache and fpu - cache ecc errors still enabled */
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

    /* Initialize o_buf */
    for (i = 0; i < 32; i++)
	o_buf[i] = i;

    for (i = 0; i < 32; i++)
	LOAD_FPU(i, &o_buf[i]);

    for (i = 0; i < 32; i++) 
	STORE_FPU(i, &i_buf[i]);

    for (i = 0; i < 32; i++) {
	if (i_buf[i] != o_buf[i]) {
	    msg_printf(DBG,"fpmem: failed at %d, expect 0x%x, got 0x%x",\
					i, o_buf[i], i_buf[i]);
	    fail = 1;
	}
    }

    /* Initialize o_buf */
    for (i = 0; i < 32; i++)
	o_buf[i] = ~i;

    for (i = 0; i < 32; i++)
	LOAD_FPU(i, &o_buf[i]);

    for (i = 0; i < 32; i++) 
	STORE_FPU(i, &i_buf[i]);

    for (i = 0; i < 32; i++) {
	if (i_buf[i] != o_buf[i]) {
	    msg_printf(DBG,"fpmem: failed at %d, expect 0x%x, got 0x%x\n",\
					i, o_buf[i], i_buf[i]);
	    fail = 1;
	}
    }
    /* report any error */
    
    return(fail);
}
