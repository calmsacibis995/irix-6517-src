/*
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

/***********************************************************************\
*	File:		Catalog_graphics.c				*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <uif.h>
#include <ide_msg.h>
#include <ide_s1chip.h>
#include <err_hints.h>
#include <everr_hints.h>
#include <sys/EVEREST/dang.h>

extern void graphics_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t graphics_hints [] = {
/* DANG related hints */
{DANG_GENRIC,  "dang_regtest",  "Exception while testing DANG chip adap %d on IO4 %d\n"},
{DANG_BADREG,  "dang_regtest",  "Failed testing DANG register %s. Expected %llx Got %llx\n"},
{DANG_INTSET,  "dang_regtest",  "Failed to set DANG Intr mask. Expected: %llx Got: %llx\n"},
{DANG_ERRCLR,  "dang_regtest",  "DANG error reg clearing FAILED. Expected: %llx Got: %llx\n"},
{DANG_BADRADR, "dang_regtest",  "Failed DANG reg address test. Reg: %s Expected: %llx Got: %llx\n"},
{DANG_SWERR,   "dang_regtest",  "SOFTWARE detected error while testing DANG chip\n"},
{DANG_BADSHRAM, "dang_gr2ram",  "Failed Gr2 shared ram test. Index:%x Expected: 0x%x Got: 0x%x\n"},
{DANG_BADDMA,   "dang_mdma",  "%s (%s) timed out waiting for DANG interrupt\n"},
{DANG_BADDDATA,   "dang_mdma",
    "xfer data, %s (%s), line 0x%x byte 0x%x : src 0x%x dest 0x%x\n"},
{DANG_DMAVECT,   "dang_mdma",  "%s (%s) wrong interrupt level: was %x, sb %x\n"},
{DANG_DMACOMP,   "dang_mdma",  "%s (%s) DMA transfer not complete\n"},
(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0
};


uint	graphics_hintnum[] = { IO4_FCG, IO4_DANG, 0 };

catfunc_t graphics_catfunc = { graphics_hintnum, graphics_printf  };
catalog_t graphics_catalog = { &graphics_catfunc, graphics_hints };


void
graphics_printf(void *loc)
{
        int *l = (int *)loc;
        msg_printf(ERR,"(IO4 slot: %d, GCAM adap: %d ", *l, *(l+1));

	/*
	 * need to correct for DANG chips - do we use the same slot locations
	 * as the F chips?
	 */
        if (*(l+1) == S1_ON_MOTHERBOARD)
                 msg_printf(ERR,"(motherboard))\n");
        else
                 msg_printf(ERR,"(mez card))\n");
}

