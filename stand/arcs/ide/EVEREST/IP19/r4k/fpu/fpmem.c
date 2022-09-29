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

#ident "$Revision: 1.4 $"

/*
 *					
 *	Floating Point Exerciser - load/store memory
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <regdef.h>
#include <sys/fpu.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "pattern.h"
#include "ip19.h"

static int cpu_loc[2];

unsigned int i_buf[32];
unsigned int o_buf[32];

int
fpmem()
{
    unsigned retval = 0;
    uint osr;
    int i;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (fpmem) \n");
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

    /* Initialize o_buf */
    for (i = 0; i < 32; i++)
	o_buf[i] = i;

    for (i = 0; i < 32; i++)
	LOAD_FPU(i, &o_buf[i]);

    for (i = 0; i < 32; i++) 
	STORE_FPU(i, &i_buf[i]);

    for (i = 0; i < 32; i++) {
	if (i_buf[i] != o_buf[i]) {
	    err_msg(FPMEM_DATA, cpu_loc, i, o_buf[i], i_buf[i]);
	    retval = 1;
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
	    err_msg(FPMEM_INVD, cpu_loc, i, o_buf[i], i_buf[i]);
	retval = 1;
	}
    }
    SetSR(osr);
    msg_printf(INFO, "Completed load/store FP registers\n");
    /* report any error */
    
    return(retval);
}
