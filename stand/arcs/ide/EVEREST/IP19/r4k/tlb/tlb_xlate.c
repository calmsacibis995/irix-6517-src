
/*
 * cpu/tlb_xlate.c
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

#ident "$Revision: 1.9 $"

/*
 * tlb_xlate -- Test for correct virtual to physical translation via a
 *		mapped TLB entry. Setup the virtual address to user segment
 *		and uncached.
 *
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "pattern.h"
#include "tlb.h"
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];
static jmp_buf fault_buf;

tlb_xlate()
{
    register unsigned char bucket;
    register unsigned physpnum;
    register int i, startent;
    volatile uint offset;
    volatile __psunsigned_t vaddr;
    tlbptepairs_t rtlbpairs;
    int physval;
    unsigned retval=0;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (tlb_xlate) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);
    if (console_is_gfx())
    {
	startent = 2;
	msg_printf(INFO, "TLB entries 0 and 1 skipped. Can be tested only when run on an ASCII console\n");
    }
    else
	startent = 0;
    ide_invalidate_caches();

    /*
     * set up the wpte[] array with good PFNs for all TLB entries
     * and write page number as first byte of the referenced memory
     * location
     */
    pte_setup();

    /*
     * invalidate current TLB entries
     */
    flushall_tlb();

    /*
     * set PID to 0
     */
    set_tlbpid(0);

    /*
     * check that each TLB slot can be mapped to ALL user segment addresses
     * this is a *LONG* test
     */
    for (i = startent, physpnum = startent * 2; i < NTLBENTRIES; i++, physpnum += 2)
    {
	vaddr = startent * NBPP;
	do
	{
	    msg_printf(DBG, "vaddr = 0x%x\n", vaddr);
	    settlbslot(i, 0, vaddr,
		       wpte[i].evenpg | TLBLO_V | TLBLO_UNCACHED,
		       wpte[i].oddpg | TLBLO_V | TLBLO_UNCACHED);

	    for (offset = 0; offset <= NBPP; offset += NBPP)
	    {
		if (setjmp(fault_buf))
		{
		    err_msg(TLBX_UNEXP, cpu_loc, i, vaddr);
		    read_indexed_lo(i, &rtlbpairs);
		    msg_printf(DBG, "Entryhi 0x%x Entrylo0 0x%x Entrylo1 0x%x\n", 
			read_indexed_hi(i), rtlbpairs.evenpg, rtlbpairs.oddpg);
		    show_fault();
		    retval = 1;
		}
		else
		{
		    nofault = &fault_buf[0];

		    /* fetch contents from virtual (mapped) addr */
		    bucket = *(unsigned char *)(vaddr + offset);

		    clear_nofault();

		    /*
		     * compare the contents of virtual address and physical
		     * address 
		     */

		    physval = uc_bread(k1a + (physpnum * NBPP) + offset);
		    if (bucket != physval)
		    {
			err_msg(TLBX_ERR, cpu_loc, i, (vaddr + offset), physval, bucket);
			show_fault();
			retval = 1;
		    }
		}
	    }
	    invaltlb(i);
	    vaddr = (vaddr << 1) + (NBPP << 1);
	} while (vaddr < K0BASE);
    }

    flushall_tlb();
    msg_printf(INFO, "Completed TLB address translation\n");
    return (retval);
}
