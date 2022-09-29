
/*
 * tlb/tlb_mod.c
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

#ident "$Revision: 1.10 $"

/*
 * tlb_mod -- Map the page "non-writable" and then do a write 
 *	      access to a mapped address. An exception should
 *	      occur.
 *
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/reg.h>
#include <setjmp.h>
#include <fault.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "pattern.h"
#include "tlb.h"
#include "ip19.h"
#include "prototypes.h"

__psunsigned_t get_badvaddr(void);

static __psunsigned_t cpu_loc[2];
static jmp_buf fault_buf;

tlb_mod()
{
    unsigned retval = 0;
    volatile uint i, startent;
    __psunsigned_t badvaddr, osr;
    volatile __psunsigned_t offset;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (tlb_mod) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);
    if (console_is_gfx())
    {
	startent = 2;
	msg_printf(INFO, "TLB entries 0 and 1 skipped. Can be tested only when run on an ASCII console\n");
    }
    else
	startent = 0;
    osr = GetSR();
    msg_printf(DBG, "Original status reg setting : 0x%x\n", osr);
			/* turn off BEV to disable boot mode vectors */
    SetSR(osr & ~SR_BEV);
    msg_printf(DBG, "Status reg setting for test : 0x%x\n", GetSR());

    ta_spl();
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
     * first test checks that each slot, when marked valid but read-only,
     * causes an exception when an attempt is made to write to either of the
     * page-pairs mapped by the slot. 
     */
    for (i = startent + 1; i < NTLBENTRIES; i++)
    {

	/* set page-pair to valid but not writable */
	settlbslot(i, 0, 0, wpte[i].evenpg | TLBLO_V | TLBLO_NONCOHRNT,
		   wpte[i].oddpg | TLBLO_V | TLBLO_NONCOHRNT);

	/* test even and odd page of slot */
	for (offset = 0; offset <= NBPP; offset += NBPP)
	{

	    if (setjmp(fault_buf))
	    {
		ta_spl();
		if ((badvaddr = get_badvaddr()) != (1 + offset))
		{
		    err_msg(TLBMOD_BVADDR, cpu_loc, (offset ? "ODD page" : "EVEN page"), i, (1 + offset), badvaddr);
		    show_fault();
		    retval = 1;
		}
	    }
	    else
	    {
		nofault = &fault_buf[0];

		/*
		 * this write should cause exception here; fault handler will
		 * resume to next iteration after awakening in the setjmp
		 * code above 
		 */
		*(unsigned volatile char *) (1 + offset) = i;

		DELAY(10);

		clear_nofault();

		/* if we're here, the exception didn't occur. Fail!! */
		err_msg(TLBMOD_NOEXCP, cpu_loc, (offset ? "ODD page" : "EVEN page"), i);
		show_fault();
		retval = 1;
	    }
	    invaltlb(i);
	}
    }
    if ((continue_on_error() == 0) && (retval))
    {
	msg_printf(INFO, "Aborted due to failure\n");
	SetSR(osr);
	return (retval);
    }

    /*
     * this test checks that each slot, when marked valid, writeable, and
     * dirty, does NOT cause an exception when an attempt is made to write to
     * either of the page-pairs mapped by the slot. 
     */
    for (i = startent + 1; i < NTLBENTRIES; i++)
    {
	/* set the page-pair to valid, writeable and dirty */
       settlbslot(i, 0, 0, wpte[i].evenpg | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT,
		   wpte[i].oddpg | TLBLO_V | TLBLO_D | TLBLO_NONCOHRNT);

	if (setjmp(fault_buf))
	{
	    ta_spl();
	    err_msg(TLBMOD_UNEXP, cpu_loc, (offset ? "ODD page" : "EVEN page"), i);
	    show_fault();
	    retval = 1;
	}
	else
	{
	    nofault = &fault_buf[0];

	    /* write to both even and (contiguous) odd page */
	    *(unsigned volatile char *) 1 = i;

	    /* different val for fun */
	    *(unsigned volatile char *) (1 + NBPP) = i + NTLBENTRIES;

	    DELAY(100);
	    clear_nofault();
	    if (*(unsigned char *) 1 != i)
	    {
		err_msg(TLBMOD_MOD, cpu_loc, (offset ? "ODD page" : "EVEN page"), i, i, *(unsigned char *) 1);
		show_fault();
		retval = 1;
	    }
	    if (*(unsigned char *) (1 + NBPP) != (i + NTLBENTRIES))
	    {
		err_msg(TLBMOD_MOD, cpu_loc, (offset ? "ODD page" : "EVEN page"), i, i + NTLBENTRIES, *(unsigned char *) (1 + NBPP));
		show_fault();
		retval = 1;
	    }
	}
	invaltlb(i);
    }
    flushall_tlb();
    SetSR(osr);
    msg_printf(INFO, "Completed TLB modification exception\n");
    return (retval);
}
