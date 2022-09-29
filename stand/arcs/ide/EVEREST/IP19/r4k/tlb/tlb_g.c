
/*
 * tlb/tlb_g.c
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
 * tlb_g -- Test the global bit of the mapped TLB entry
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

#define BY_SIXTEENS	0x10

static jmp_buf fault_buf;

tlb_g()
{
    register unsigned char bucket;
    int retval=0, indx, startent, validpid, testpid;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (tlb_g) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    if (console_is_gfx())
    {
	startent = 2;
	msg_printf(INFO, "TLB entries 0 and 1 skipped. Can be tested only when run on an ASCII console\n");
    }
    else
	startent = 0;
    ta_spl();
    pte_setup();
    flushall_tlb();

    for (indx = startent; indx < NTLBENTRIES; indx++) 
    {
	/* TLBHI_NPID doesn't count the top slot (special value), but 
	 * we want to test it.  Therefore, use TLBHI_NPID+1.
	 */
    	for (validpid = 0; validpid < TLBHI_NPID+1; validpid += BY_SIXTEENS) 
	{
    	    set_tlbpid(validpid);

    	    for (testpid = 0; testpid < TLBHI_NPID+1; testpid += BY_SIXTEENS) 
	    {
		settlbslot(indx, testpid, 0,
			wpte[indx].evenpg|TLBLO_V|TLBLO_G|TLBLO_NONCOHRNT,
			wpte[indx].oddpg|TLBLO_V|TLBLO_G|TLBLO_NONCOHRNT);

		if (setjmp(fault_buf))
		{
		    err_msg(TLBG_UNEXP, cpu_loc);
		    show_fault();
		    retval = 1;
		}
		else
		{
		    nofault = fault_buf;

		    /* exception should not occur. */

		    bucket = *(unsigned char *)0;	/* even page access */
		    bucket = *(unsigned char *)(0+(__psunsigned_t)NBPP); /* odd page access */
		    nofault = 0;
		}
		invaltlb(indx);
    	    }
    	}
    }
    flushall_tlb();
    msg_printf(INFO, "Completed TLB global bit test\n");
    return(retval);
}
