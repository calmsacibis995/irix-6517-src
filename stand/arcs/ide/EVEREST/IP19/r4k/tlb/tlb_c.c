
/*
 * tlb/tlb_c.c
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
 * tlb_c -- Positive and negative testing with regard to marking mapped
 *	    pages either cacheable or non-cacheable via use of the c bits(3).
 *	    NOTE: only 2 values are checked here: 2 - uncached, 3 - coherent.
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

#define NUMADDRS	4

static jmp_buf fault_buf;

tlb_c()
{
    __psunsigned_t addr;
    register int i, startent;
    register unsigned *cachep, *nocachep;	/* cached & ucached ptrs */
    unsigned mempat = 0x12345678;
    unsigned cachepat = 0x87654321;
    __psunsigned_t osr;
    unsigned tlbpat, retval=0;
    static __psunsigned_t n_addr[NUMADDRS] = {
					    KUBASE,
					    KUBASE | 0x20000000,
					    K2BASE | 0x10000000,
					    K2BASE | 0x20000000
    };
    volatile __psunsigned_t offset;	/* 0 for even page, NBPP for odd page */

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (tlb_c) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    if (console_is_gfx())
    {
	startent = 2;
	msg_printf(INFO, "TLB entries 0 and 1 skipped. Can be tested only when run on an ASCII console\n");
    }
    else
	startent = 0;
    ta_spl();
    pte_setup();		/* set up page tables, k0a, and k1a */
    flushall_tlb();

    /*
     * First sanity check the correct functioning of the cache without adding
     * the complexity of the tlb.  Write one pattern to a cache line (via
     * k0seg) and a different one to the memory that corresponds to that line
     * (via k1seg), then (reading back through the same pointers) ensure that
     * the values are indeed different (no writeback should occur).  If this
     * test is successful (and no cache parity error occurs) we disable the
     * parity exception and assume that the cache is functioning correctly,
     * and test the tlb in cached (noncoherent) and uncached modes with that
     * potential variable removed. 
     */
    cachep = (unsigned *) (k0a + 4);	/* ptr into cached space */
    nocachep = (unsigned *) (k1a + 4);	/* uncached */

    osr = GetSR();
    msg_printf(DBG, "Original status reg setting : 0x%x\n", osr);
			    /* turn off BEV to disable boot mode vectors */
    SetSR(osr & ~SR_DE);	/* enable exceptions for cache parity errors */
    msg_printf(DBG, "Status reg setting for test : 0x%x\n", GetSR());

    if (setjmp(fault_buf))
    {				/* parity error! */
	err_msg(TLBC_PERR, cpu_loc, (__psunsigned_t) cachep);
	show_fault();
	SetSR(GetSR() | SR_DE);		/* set DE bit again */
	retval = 1;
    }
    else
    {
	nofault = &fault_buf[0];
	*cachep = cachepat;
	*nocachep = mempat;
	if (*cachep == mempat)
	{
	    err_msg(TLBC_WR, cpu_loc, (__psunsigned_t) cachep);
	    show_fault();
	    SetSR(GetSR() | SR_DE);		/* set DE bit again */
	    retval = 1;
	}
    }
    if ((continue_on_error() == 0) && (retval))
    {
	msg_printf(INFO, "Aborted due to failure\n");
	SetSR(osr);
	return (retval);
    }

    flushall_tlb();
    set_tlbpid(0);

    /*
     * for remaining tests we ignore parity and ECC cache errors, which are
     * automatically corrected anyway 
     */
    SetSR(GetSR() | SR_DE);

    /* first test cached mode */
    for (i = startent; i < NTLBENTRIES; i++)
    {
	addr = n_addr[i & (NUMADDRS - 1)];
	/* set slot valid, dirty, and cached (noncoherent) */
	settlbslot(i, 0, addr,
		   wpte[0].evenpg | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT,
		   wpte[0].oddpg | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT);
	/* offset allows testing of both even & odd page of each tlb slot */
	for (offset = 0; offset <= NBPP; offset += NBPP)
	{
	    if (setjmp(fault_buf))
	    {
		ta_spl();
		err_msg(TLBC_CEXCP, cpu_loc, (offset == 0 ? "Even" : "Odd"), i);
		show_fault();
		retval = 1;
	    }
	    else
	    {
		nofault = fault_buf;
		
		/*
		 * cached write
		 */
		*(cachep + (offset/sizeof(int))) = cachepat;

		/*
		 * uncached write
		 */
		*(nocachep + (offset/sizeof(int))) = mempat;
		
		/*
		 * mapped & cached
		 */
		tlbpat = *(unsigned *)(addr + 4 + offset);
		DELAY(100);

		/* if we get here, no exception occurred */
		nofault = 0;
		if (tlbpat != cachepat)
		{
		    err_msg(TLBC_CRW, cpu_loc, (offset == 0 ? "even" : "odd"), i, cachepat, tlbpat);
		    show_fault();
		    retval = 1;
		}
	    }
	}
	invaltlb(i);
    }
    if ((continue_on_error() == 0) && (retval))
    {
	msg_printf(INFO, "Aborted due to failure\n");
	SetSR(osr);
	return (retval);
    }

    ide_invalidate_caches();

    /* uncached mode */
    for (i = startent; i < NTLBENTRIES; i++)
    {
	settlbslot(i, 0, 0, wpte[0].evenpg | TLBLO_V | TLBLO_D | TLBLO_UNCACHED,
		   wpte[0].oddpg | TLBLO_V | TLBLO_D | TLBLO_UNCACHED);
	for (offset = 0; offset <= NBPP; offset += NBPP)
	{
	    if (setjmp(fault_buf))
	    {
		ta_spl();
		err_msg(TLBC_UEXCP, cpu_loc, (offset == 0 ? "Even" : "Odd"), i);
		show_fault();
		retval = 1;
	    }
	    else
	    {
		nofault = fault_buf;
		*(cachep + offset) = cachepat;
		*(nocachep + offset) = mempat;
		tlbpat = *(unsigned *) (4 + offset);
		nofault = 0;
		if (tlbpat != mempat)
		{
		    err_msg(TLBC_URW, cpu_loc, (offset == 0 ? "even" : "odd"), i, mempat, tlbpat);
		    show_fault();
		    retval = 1;
		}
	    }
	}
	invaltlb(i);
    }
    flushall_tlb();
    SetSR(osr);
    msg_printf(INFO, "Completed TLB C-bits test\n");
    return (retval);
}
