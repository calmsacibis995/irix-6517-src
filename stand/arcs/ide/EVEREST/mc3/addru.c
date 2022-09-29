
/*
 * mc3/addru.c
 *
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
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

#ident "$Revision: 1.20 $"

/*
 *  	ADDRU - Address Uniqueness
 *
 *   This is a traditional, hueristic, rule-of-thumb, "address-in-address"
 *   memory test.  It also puts the complement of the address in the address,
 *   and makes passes in both ascending and decending addressing order.
 *   There are both full memory store then check passes, as well as read-
 *   after-write passes (with complementing).
 *
 *   Nearly immediately after detecting a miscompare, the expected data,
 *   actual data and the address at an error occured are displayed.
 *
 *
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#ifdef IP19
#include "ip19.h"
#elif IP21
#include "ip21.h"
#elif IP25
#include "ip25.h"
#endif
#include "pattern.h"
#include "memstr.h"
#include "exp_parser.h"
#include "mc3_reg.h"
#include "libsk.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include <sys/EVEREST/evconfig.h>
#include "fault.h"
#include "prototypes.h"
#include "uif.h"


#define MASK		0xffffffffffffffff

extern jmp_buf mem_buf;
extern uint mem_slots;
extern int addr_pattern(struct memstr *, int, enum bitsense);

addru()
{
    __psunsigned_t himem, paddr;
    unsigned long long i, memsize;
    register unsigned int fail = 0;
    struct memstr addr;
    register int inc = 4;
    int val;
#ifdef IP19
    int first_time = 1;
#endif

    msg_printf(SUM, "Memory address in address test\n");
    if (quick_mode()) {
	msg_printf(SUM,"\nRunning in quick mode...\n");
	inc = QUICK_INC;
    }

    mem_slots = mc3slots();
    mem_err_clear(mem_slots); /* clear error regs */
    memtest_init();
    if (val = setjmp(mem_buf)) {
        if (val == 1){
                msg_printf(ERR, "Unexpected exception during memory test\n");
                show_fault();
        }
	mem_err_log();
   	mem_err_show();
	/* everest_error_show(); */
	fail = 1;
	goto SLOTFAILED;
    }
    else {
	set_nofault(mem_buf);
	memsize = readconfig_reg(RDCONFIG_MEMSIZE,0);
	
	date_stamp();

#if _MIPS_SIM == _ABI64					/* 64-bits avoids the tlb */
	paddr = PHYS_TO_MEM(PHYS_CHECK_LO);
	addr.lomem = paddr;
	himem = PHYS_TO_MEM(memsize);
	addr.himem = himem;
	addr.mask = (__psunsigned_t)MASK;
	msg_printf(DBG, "lo:%llx, hi:%llx, p: %llx, inc: %d\n",
		   addr.lomem, addr.himem, paddr, inc);

	fail |= addr_pattern(&addr, inc, BIT_TRUE);

	/* Complement address pattern test */
	fail |= addr_pattern(&addr, inc, BIT_INVERT);

#else
	/* Enable the bouncing cursor via R4k Interrupts */
	/* every four seconds */
	InstallHandler( INT_Level5, TimerHandler);
	EnableTimer(4);

	for (i = 0; i < memsize; i += DIRECT_MAP) {
		if (i < PHYS_CHECK_LO)
			paddr = PHYS_CHECK_LO;
		else
			paddr = i;
		addr.lomem = 
		   map_addr(paddr, DIRECT_MAP, UNCACHED);
		if (memsize < (i + DIRECT_MAP)) {
    		   himem = ((uint) addr.lomem + (memsize - paddr));
    		   addr.himem = himem;
		   msg_printf(DBG,"memsize-paddr is 0x%llx, himem is 0x%x\n",
			 memsize-paddr, (uint) himem);
		}
		else {
		   himem = ((uint) addr.lomem +
			(DIRECT_MAP-(first_time*PHYS_CHECK_LO)));
		   addr.himem = himem;
	   	   first_time = 0;
		}
    		addr.mask = (__psunsigned_t)MASK;
        	msg_printf(DBG, "lo:%x, hi:%x, paddr: %x, inc: %d\n",
			 addr.lomem, addr.himem, paddr, inc);

		fail |= addr_pattern(&addr, inc, BIT_TRUE);

		/* Complement address pattern test */
		fail |= addr_pattern(&addr, inc, BIT_INVERT);
	}

	/* Put back the orginal vector */
	RemoveHandler( INT_Level5);
	StopTimer();
#endif

    } /* else... setjmp */

SLOTFAILED:

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Memory address in address test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }

    clear_nofault();
    date_stamp();

    return (fail);
}
