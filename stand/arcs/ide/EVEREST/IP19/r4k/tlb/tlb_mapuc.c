
/*
 * tlb/tlb_mapuc.c
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

#ident "$Revision: 1.8 $"

/*
 * tlb_mapuc -- Try check that both cached and uncached mapped access work
 *		without interfering with each other. This was added do detect
 *		the R4000 mapped uncached writeback bug
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

tlb_mapuc()
{
    register int i, j;
    register unsigned cached, uncached;	/* cached & ucached addresses*/
    unsigned mempat = 0x12345678;
    unsigned cachepat = 0x87654321;
    unsigned even_pfn, odd_pfn;
    volatile unsigned rbpat;
    volatile unsigned rbpat1;
    unsigned retval=0;

    volatile __psunsigned_t offset;	/* 0 for even page, NBPP for odd page */

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (tlb_mapuc) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);
    if (console_is_gfx())
    {
	msg_printf(INFO, "Test skipped. Can run only on an ASCII console\n");
	return(0);
    }

    ta_spl();

    /*
     * set up the PFN's that will be used for both cached and uncached access
     */
    even_pfn = (PHYS_CHECK_LO >> TLBHI_VPNSHIFT);
    odd_pfn = even_pfn + 1;
    even_pfn <<= TLBLO_PFNSHIFT;
    odd_pfn <<= TLBLO_PFNSHIFT;


    /*
     * set up the pointer addresses for both cached and uncached access
     */
    cached = 4;
    uncached = (NBPP << 2)+4;

    /*
     * invalidate all tlb entries prior to testing
     */
    flushall_tlb();

    /*
     * set PID to 0
     */
    set_tlbpid(0);

    /* mapped cached - mapped uncached mode */
    for (i = 0, j = 1; i < NTLBENTRIES; i++, j = (j+1) % NTLBENTRIES)
    {
	/* set slot valid, dirty, and cached (noncoherent) */
	settlbslot(i, 0, cached,
		   even_pfn | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT,
		   odd_pfn | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT);

	/* set slot valid, dirty, and uncached */
	settlbslot(j, 0, uncached,
		   even_pfn | TLBLO_V | TLBLO_D | TLBLO_UNCACHED,
		   odd_pfn | TLBLO_V | TLBLO_D | TLBLO_UNCACHED);

	/* offset allows testing of both even & odd page of each tlb slot */
	for (offset = 0; offset <= NBPP; offset += NBPP)
	{
	    if (setjmp(fault_buf))
	    {
		ta_spl();
		err_msg(TLBM_CUWEXCP, cpu_loc, (offset == 0 ? "Even" : "Odd"), i);
		show_fault();
		retval = 1;
	    }
	    else
	    {
		ide_invalidate_caches();

		nofault = fault_buf;
		
		/*
		 * cached write
		 */
		*(unsigned int *)(cached + offset) = cachepat;

		/*
		 * uncached write
		 */
		*(unsigned int *)(uncached + offset) = mempat;
		
		/*
		 * read back the cached value
		 */
		rbpat = *(unsigned int *) (cached + offset);
		DELAY(100);

		/* if we get here, no exception occurred */
		clear_nofault();
		if (rbpat != cachepat)
		{
		    err_msg(TLBM_CUW, cpu_loc, (offset == 0 ? "even" : "odd"), i, cachepat, rbpat);
		    show_fault();
		    retval = 1;
		}
	    }
	}

	/*
	 * invalidate the two tlb entries used during this loop
	 */
	invaltlb(i);
	invaltlb(j);
    }
    if ((continue_on_error() == 0) && (retval))
    {
	msg_printf(INFO, "Aborted due to failure\n");
	return (retval);
    }

    /*
     * invalidate all tlb entries prior to testing
     */
    flushall_tlb();

    /* mapped uncached - mapped cached mode */
    for (i = 0, j = 1; i < NTLBENTRIES; i++, j = (j+1) % NTLBENTRIES)
    {
	/* set slot valid, dirty, and uncached */
	settlbslot(i, 0, uncached,
		   even_pfn | TLBLO_V | TLBLO_D | TLBLO_UNCACHED,
		   odd_pfn | TLBLO_V | TLBLO_D | TLBLO_UNCACHED);

	/* set slot valid, dirty, and cached (noncoherent) */
	settlbslot(j, 0, cached,
		   even_pfn | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT,
		   odd_pfn | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT);

	/* offset allows testing of both even & odd page of each tlb slot */
	for (offset = 0; offset <= NBPP; offset += NBPP)
	{
	    if (setjmp(fault_buf))
	    {
		ta_spl();
		err_msg(TLBM_UCWEXCP, cpu_loc, (offset == 0 ? "Even" : "Odd"), i);
		show_fault();
		retval = 1;
	    }
	    else
	    {
		ide_invalidate_caches();

		nofault = fault_buf;
		
		/*
		 * uncached write
		 */
		*(unsigned int *)(uncached + offset) = mempat;
		
		/*
		 * cached write
		 */
		*(unsigned int *)(cached + offset) = cachepat;

		/*
		 * read back the cached value
		 */
		rbpat = *(unsigned int *) (uncached + offset);
		DELAY(100);

		/* if we get here, no exception occurred */
		clear_nofault();
		if (rbpat != mempat)
		{
		    err_msg(TLBM_UCW, cpu_loc, (offset == 0 ? "even" : "odd"), i, mempat, rbpat);
		    show_fault();
		    retval = 1;
		}
	    }
	}

	/*
	 * invalidate the two tlb entries used during this loop
	 */
	invaltlb(i);
	invaltlb(j);
    }
    if ((continue_on_error() == 0) && (retval))
    {
	msg_printf(INFO, "Aborted due to failure\n");
	return (retval);
    }


    /*
     * invalidate all tlb entries prior to testing
     */
    flushall_tlb();

    /* mapped uncached - mapped readback cached mode */
    for (i = 0, j = 1; i < NTLBENTRIES; i++, j = (j+1) % NTLBENTRIES)
    {
	/* set slot valid, dirty, and uncached */
	settlbslot(i, 0, uncached,
		   even_pfn | TLBLO_V | TLBLO_D | TLBLO_UNCACHED,
		   odd_pfn | TLBLO_V | TLBLO_D | TLBLO_UNCACHED);

	/* set slot valid, dirty, and cached (noncoherent) */
	settlbslot(j, 0, cached,
		   even_pfn | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT,
		   odd_pfn | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT);

	/* offset allows testing of both even & odd page of each tlb slot */
	for (offset = 0; offset <= NBPP; offset += NBPP)
	{
	    if (setjmp(fault_buf))
	    {
		ta_spl();
		err_msg(TLBM_UCWREXCP, cpu_loc, (offset == 0 ? "Even" : "Odd"), i);
		show_fault();
		retval = 1;
	    }
	    else
	    {
		ide_invalidate_caches();

		nofault = fault_buf;
		
		/*
		 * uncached write
		 */
		*(unsigned int *)(uncached + offset) = cachepat;
		
		/*
		 * cached read 
		 */
		rbpat1 = *(unsigned int *)(cached + offset);

		/*
		 * uncached write
		 */
		*(unsigned int *)(uncached + offset) = mempat;
		
		/*
		 * read back the cached value
		 */
		rbpat = *(unsigned int *) (cached + offset);
		DELAY(100);

		/* if we get here, no exception occurred */
		clear_nofault();
		if (rbpat1 != cachepat)
		{
		    err_msg(TLBM_UCWR, cpu_loc, (offset == 0 ? "even" : "odd"), i, cachepat, rbpat1);
		    show_fault();
		    retval = 1;
		}

		if (rbpat != cachepat)
		{
		    err_msg(TLBM_UCRWR, cpu_loc, (offset == 0 ? "even" : "odd"), i, cachepat, rbpat);
		    show_fault();
		    retval = 1;
		}
	    }
	}

	/*
	 * invalidate the two tlb entries used during this loop
	 */
	invaltlb(i);
	invaltlb(j);
    }

    flushall_tlb();
    msg_printf(INFO, "Completed uncached/cached TLB access\n");
    return (retval);
}
