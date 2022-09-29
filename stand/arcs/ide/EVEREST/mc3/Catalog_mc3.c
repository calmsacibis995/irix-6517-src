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
*	File:		Catalog_mc3.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <err_hints.h>
#include <everr_hints.h>
#include <ide_msg.h>
#include "uif.h"
#include "prototypes.h"

#ident	"arcs/ide/EVEREST/mc3/Catalog_mc3.c $Revision: 1.14 $"

extern void mc3_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t mc3_hints [] = {
{ADDRU_ERR, "mem5", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{CACHEMEM_ERR, "mem10", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{CONNECT_ERR, "mem2", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{DECODE_ERR, "mem12", "*** Unable to decode address 0x%x!\n"},
{DECODE_Y_ERR, "mem12", "*** Unable to decode address 0x%x!\n"},
{DECODE_RANGE_ERR, "mem12", "Address 0x%x is out of range\n"},
/*
{FECC_IBIT, "mem10", "Can't clear level 6 IP\n"},
{FECC_CACHEWR, "mem10", "Can't write new cached data\n"},
{FECC_SET_ECC, "mem10", "Can't set ECC register to be bad\n"},
{FECC_NOIP6ERR, "mem10", "IP6 error not passed to cpu\n"},
{FECC_ERTOIP, "mem10", "Double bit error not registered\n"},
{FECC_IBITOFF, "mem10", "Can't turn level 6 IP off after writing\n"},
{FECC_CERTOIP, "mem10", "Can't clear error in ERTOIP register\n"},
*/
{MARCHX_ERR, "mem7", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{MARCHY_ERR, "mem8", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{DBLMARCHY_ERR, "mem15", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{MC3_INTR_ERR, "MC3 test", "CPU interrupt during memory write to 0x%x\n"},
{MC3_REG_ERR, "MC3 test",
	 "Test passed, but got an unexpected error in a register"},
{MEMADDR_ERR, "mem3", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{MEMADDR_HINT, "mem3", "\t\tAddress line 0x%x might be bad\n"},
{MEM_ECC_ERR, "mem9", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{MEM_PAT_ERR, "mem11", "Virtual Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{MEMWLK_ERR, "mem6", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{MEMBWRRD_ERR, "mem13", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{MEMWRRD_ERR, "mem4", "Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{MEM_KH_ERR, "mem16", "Virtual Addr: 0x%x, Expected: 0x%x, Actual: 0x%x\n"},
{MEMERR_REG, "MC3 test", "%s test detected non-zero MC3 error register contents\n"},
{MC3INTR_IP6, "memutils", "Level 3 interrupt pending not detected in CAUSE\n", 0},
{MC3INTR_IP6NC, "memutils", "Level 3 interrupt pending not cleared in Cause : Cause 0x%x\n", 0},
{MC3INTR_ERTOIP, "memutils", "ECC error detected in ERTOIP\n", 0},
{MC3INTR_CERTOIP, "memutils", "ERTOIP not cleared via write to CERTOIP : ERTOIP 0xllx\n", 0},
(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0
};


uint	mc3_hintnum[] = {MC3_MA_HINT, MC3_MD_HINT, MC3_BANK_HINT, 
			 MC3_LEAF_HINT, MC3_SIMM_HINT, 0 };

catfunc_t mc3_catfunc = { mc3_hintnum, mc3_printf  };
catalog_t mc3_catalog = { &mc3_catfunc, mc3_hints };

void
mc3_printf(void *ptr)
{
    msg_printf(ERR, "(MC3 slot (hex): %x)\n", *(int *)ptr);
}
