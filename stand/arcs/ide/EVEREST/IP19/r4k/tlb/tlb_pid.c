
/*
 * tlb_pid.c
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
 * tlb_pid -- Test matching and non-matching pid fields in mapped TLB entries.
 *	      Non-matching should cause exception 
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

tlb_pid()
{
    register unsigned char bucket;
    int indx, startent, validpid, testpid;
    volatile int expect_fault;
    __psunsigned_t badvaddr, osr;
    unsigned retval=0;
    volatile __psunsigned_t offset;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (tlb_pid) \n");
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
    for (indx = startent; indx < NTLBENTRIES; indx++)
    {
    	validpid=0;
	/* TLBHI_NPID doesn't count the top slot (special value), but 
	 * we want to test it.  Therefore, use TLBHI_NPID+1.
	 */
    	for (; validpid < TLBHI_NPID+1; validpid += 0x10) {
    	    set_tlbpid(validpid);
    	    testpid = 0;
    	    for (; testpid < TLBHI_NPID; testpid += 0x10) {
    	    	settlbslot(indx, testpid, 0,
    	    		   wpte[indx].evenpg|TLBLO_V|TLBLO_NONCOHRNT,
    	    		   wpte[indx].oddpg|TLBLO_V|TLBLO_NONCOHRNT);
    	    	expect_fault = (testpid != validpid);
		for (offset = 0; offset <= NBPP; offset+=NBPP) {
		    if (setjmp(fault_buf)) {
    	    		ta_spl();
    	    		if (!expect_fault) {
			    err_msg(TLBP_UNEXP, cpu_loc, (offset==0?"even":"odd"), indx, testpid);
			    show_fault();
			    retval = 1;
    	    		} else if ((badvaddr=get_badvaddr())!=(0+offset)) {
			    err_msg(TLBP_BVADDR, cpu_loc, (offset==0?"even":"odd"), indx, badvaddr);
			    show_fault();
			    retval = 1;
    	    		}
    	    	    } else {
    	    	        nofault = fault_buf;
		        /* if expect_fault is true, exception should occur */

    		        bucket = *(unsigned char *)(0+offset);
    		        DELAY(100);		
			clear_nofault();
    		        if (expect_fault) {
			    err_msg(TLBP_NOEXCP, cpu_loc, (offset==0?"even":"odd"), indx);
			    show_fault();
			    retval = 1;
    		        }
		    }
		}
    		invaltlb(indx);
    	    }
    	}
    }
    flushall_tlb();
    SetSR(osr);
    msg_printf(INFO, "Completed TLB refill exception\n");
    return(retval);
}
