
/*
 * tlb_valid.c
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
 * tlb_valid -- Map each TLB entry as invalid which should cause an exception 
 *       	when an access is attempted.
 *		Setup the virtual address to k2 and uncached.
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

tlb_valid()
{
	register unsigned char bucket, *addr;
	register int i, startent;
	volatile uint offset;	/* must not end up in an s-register */
	volatile __psunsigned_t expected; /* must not end up in an s-register */
	__psunsigned_t badvaddr, osr;
	unsigned retval=0;
	register vpnum;		/* virtual page num to set/fetch to/from slot */

	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (tlb_valid) \n");
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
	pte_setup();
	flushall_tlb();
	set_tlbpid(0);
	for (i = startent,vpnum = 0; i < NTLBENTRIES; i++,vpnum+=2) {
	    addr = (unsigned char *)((0x20000000|K2BASE)+(vpnum*NBPP));
	    settlbslot(i, 0, (__psunsigned_t)addr, wpte[i].evenpg, wpte[i].oddpg);
	    for (offset = 0; offset <= NBPP; offset += NBPP) {
		if (setjmp(fault_buf)) {
			ta_spl();
			expected = (__psunsigned_t)addr+offset;
			badvaddr = get_badvaddr();
			if (badvaddr != expected) {
			    err_msg(TLBV_BVADDR, cpu_loc, i, expected, badvaddr);
			    show_fault();
			    retval = 1;
			}
		} else {
			nofault = &fault_buf[0];

			/* exception occurs here, fault handler
			   will resume to next (offset) loop iteration */

			bucket = *(unsigned char *)(addr+offset);
			DELAY(0x100);
			clear_nofault();

			/* if we are here, the exception didn't occur. */

			err_msg(TLBV_NOEXCP, cpu_loc, i); 
			show_fault();
			retval = 1;
		}
		invaltlb(i);
	    }
	}
	flushall_tlb();
	SetSR(osr);
	msg_printf(INFO, "Completed TLB invalid exception\n");
	return(retval);
}
