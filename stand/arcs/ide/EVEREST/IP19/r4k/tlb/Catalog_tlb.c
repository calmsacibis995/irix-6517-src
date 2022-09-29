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
*	File:		Catalog_tlb.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <everr_hints.h>
#include <ide_msg.h>
#include "prototypes.h"

#ident	"arcs/ide/EVEREST/IP19/r4k/tlb/Catalog_tlb.c $Revision: 1.6 $"

extern void tlb_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t tlb_hints [] = {
{TLBR_HI,     "tlb_ram", "TLBHI      entry %d R/W error: Wrote 0x%x Read 0x%x\n"},
{TLBR_EVENLO, "tlb_ram", "TLBLO even entry %d R/W error: Wrote 0x%x Read 0x%x\n"},
{TLBR_ODDLO,  "tlb_ram", "TLBLO odd  entry %d R/W error: Wrote 0x%x Read 0x%x\n"},
{TLBM_CUWEXCP, "tlb_mapuc", "TLB %s entry %d cached/uncached W exception\n"},
{TLBM_CUW,    "tlb_mapuc", "TLB %s entry %d cached/uncached W error : Wrote 0x%x Read 0x%x\n"},
{TLBM_UCWEXCP, "tlb_mapuc", "TLB %s entry %d uncached/cached W execption\n"},
{TLBM_UCW,    "tlb_mapuc", "TLB %s entry %d uncached/cached W error : Wrote 0x%x Read 0x%x\n"},
{TLBM_UCWREXCP, "tlb_mapuc", "TLB %s entry %d uncached/cached RW execption\n"},
{TLBM_UCWR,    "tlb_mapuc", "TLB %s entry %d uncached/cached RW error : Wrote 0x%x Read 0x%x\n"},
{TLBM_UCRWR,    "tlb_mapuc", "TLB %s entry %d uncached/cached RWR error : Wrote 0x%x Read 0x%x\n"},
{TLBMOD_BVADDR, "tlb_mod", "TLB %s entry %d mod exception VADDR error : Expected 0x%x Got 0x%x\n"},
{TLBMOD_NOEXCP, "tlb_mod", "TLB %s entry %d mod exception didn't occur\n"},
{TLBMOD_UNEXP, "tlb_mod", "TLB %s entry %d unexpected exception during mod\n"},
{TLBMOD_MOD, "tlb_mod", "TLB %s entry %d mod error : Wrote 0x%x Read 0x%x\n"},
{TLBC_PERR, "tlb_c", "Exception during cached write to 0x%x\n"},
{TLBC_WR, "tlb_c", "Cached write to 0x%x failed\n"},
{TLBC_CEXCP, "tlb_c", "TLB %s entry %d cached mode exception\n"},
{TLBC_CRW, "tlb_c", "TLB %s entry %d cached R/W error : Wrote 0x%x Read 0x%x\n"},
{TLBC_UEXCP, "tlb_c", "TLB %s entry %d uncached mode exception\n"},
{TLBC_URW, "tlb_c", "TLB %s entry %d uncached R/W error : Wrote 0x%x Read 0x%x\n"},
{TLBP_UNEXP, "tlb_pid", "TLB %s entry %d unexpected exception with matching pid 0x%x\n"},
{TLBP_BVADDR, "tlb_pid", "TLB %s entry %d refill exception VADDR error : Expected 0x%x Got 0x%x\n"},
{TLBP_NOEXCP, "tlb_pid", "TLB %s entry %d refill exception didn't occur\n"},
{TLBPR_ERR, "tlb_probe", "TLB probe error : Expected entry %d Got entry %d vpnum %d addr 0x%x\n"},
{TLBV_BVADDR, "tlb_valid", "TLB entry %d invalid exception VADDR error : Expected 0x%x Got 0x%x\n"},
{TLBV_NOEXCP, "tlb_valid", "TLB entry %d invalid exception didn't occur\n"},
{TLBX_UNEXP, "tlb_xlate", "TLB entry %d unexpected exception for addr 0x%x\n"},
{TLBX_ERR, "tlb_xlate", "TLB entry %d translation error at addr 0x%x : Wrote %d Read %d\n"},
{TLBG_UNEXP, "tlb_g", "Unexpected exception occurred during global access\n"},
{(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0}
};


uint	tlb_hintnum[] = { IP19_TLB, 0 };

catfunc_t tlb_catfunc = { tlb_hintnum, tlb_printf  };
catalog_t tlb_catalog = { &tlb_catfunc, tlb_hints };

void
tlb_printf(void *loc)
{
	int *l = (int *)loc;
	msg_printf(ERR, "(IP19 slot:%d cpu:%d)\n", *l, *(l+1));
}
