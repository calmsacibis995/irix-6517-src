
/*
 * IP19/r4k/tlb/tlb_ram.c
 *
 *
 * Copyright 1992, Silicon Graphics, Inc.
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
 * tlb_ram -- 	Test the tlb as a small memory array. Just see if all the 
 * 		read/write bits can be toggled and that all undefined bits 
 *		read back zero.
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "pattern.h"
#include "tlb.h"
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

#define WPATTERN	0xa5a5a5a5

tlb_ram()
{
    unsigned i, j, startent, wpat, rpat, retval=0;
    tlbptepairs_t wtlbpairs, rtlbpairs;	/* read & written tlblo_0 & _1 */

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (tlb_ram) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);
    if (console_is_gfx())
    {
	startent = 2;
	msg_printf(INFO, "TLB entries 0 and 1 skipped. Can be tested only when run on an ASCII console\n");
    }
    else
	startent = 0;
    wpat = WPATTERN;

    /*
     * invalidate the current TLB contents
     */
    flushall_tlb();	

    /*
     * test the TLB high registers
     */
    i = startent;
    do
    {
	j = 0;
	do
	{
	    write_indexed_hi(i, wpat);
	    rpat = read_indexed_hi(i);
	    if (rpat != (wpat & TLBHI_DATA))
	    {			/* failed */
		err_msg(TLBR_HI, cpu_loc, i, wpat & TLBHI_DATA, rpat);
		show_fault();	
		retval = 1;
	    }
	    wpat = ~wpat;
	    j++;
	} while (j < 2);
	i++;
    } while (i < NTLBENTRIES);

    /*
     * invalidate the current TLB contents again
     */
    flushall_tlb();

    /*
     * test the even-odd tlb low registers
     */
    /* the flushall_tlb zeroed tlbhi; writes to tlblo{0,1} will fail 
     * unless some non-zero value is moved there
     */
    for (i = startent; i < NTLBENTRIES; i++)
	write_indexed_hi(i, (0xBAD0000+(i << 6)));

    i = startent;
    do
    {
	j = 0;
	do
	{
	    /*
	     * invalidate the current slot contents
	     */
	    invaltlb(i);
	    
	    /* must ram-test the bits of both even and odd pages */
	    wtlbpairs.evenpg = wpat & TLBLO_DATA;
	    /*
	     * added this weirdness to make sure that both pages are
	     * set to global access or not in unison
	     */
	    wtlbpairs.oddpg =
		(((~wpat & ~TLBLO_G) | ((wpat & TLBLO_G) ? TLBLO_G : 0)) &
		  TLBLO_DATA);
	    write_indexed_lo(i, &wtlbpairs);
	    read_indexed_lo(i, &rtlbpairs);
	    DELAY(100);
	    if (rtlbpairs.evenpg != wtlbpairs.evenpg)
	    {
		err_msg(TLBR_EVENLO, cpu_loc, i, wtlbpairs.evenpg, rtlbpairs.evenpg);
		show_fault();	
		retval = 1;
	    }
	    if (rtlbpairs.oddpg != wtlbpairs.oddpg)
	    {
		err_msg(TLBR_ODDLO, cpu_loc, i, wtlbpairs.oddpg, rtlbpairs.oddpg);
		show_fault();	
		retval = 1;
	    }
	    wpat = (~wpat);
	    j++;
	} while (j < 2);
	i++;
    } while (i < NTLBENTRIES);

    /*
     * invalidate the current TLB contents before exiting
     */
    flushall_tlb();

    /* successful */

    msg_printf(INFO, "Completed TLB RAM test\n");
    return (retval);
}
