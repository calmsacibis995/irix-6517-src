/*
 * mc3/kh.c
 *
 *
 * Copyright 1991, 1992 Silicon Graphics, Inc.
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

#ident "$Id: threeb.c,v 1.4 1996/01/23 08:51:07 ack Exp $"


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
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include <sys/EVEREST/evconfig.h>
#include "fault.h"
#include "uif.h"
#include "prototypes.h"

#define MASK 0xffffffff

extern jmp_buf mem_buf;
extern uint mem_slots;
extern int TimerHandler(int);
extern uint three_l(u_int, u_int);

threebit_driver()
{

    register unsigned int *ptr;
    register unsigned int fail = 0;
    register struct memstr addr;
    register int inc = 1;
    unsigned long long i, himem, memsize, paddr;
    int first_time = 1;
    int val;

    msg_printf(SUM, "Three Bit test\n");


    /*
     * from stand/arcs/lib/libsk/ml/cache.s
     * invalidates all caches. Added because of the R4000 uncached writeback
     * bug - makes *sure* that there are no valid cache lines
     */
    invalidate_caches();
    for (i = 0; i < EV_MAX_SLOTS; i++)
        if (board_type(i) == EVTYPE_MC3)
                setup_err_intr(i, 0); /* 0 is a no op for mc3 */
    mem_slots = mc3slots();
    mem_err_clear(mem_slots); /* clear error regs */
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
        first_time = 1;

        for (i = 0; i < memsize; i += DIRECT_MAP) {
                if (i < PHYS_CHECK_LO)
                        paddr = PHYS_CHECK_LO;
                else
                        paddr = i;
                addr.lomem =
                   (uint *) map_addr(paddr, DIRECT_MAP, UNCACHED);
                if (memsize < (i + DIRECT_MAP)) {
                   himem = ((__psunsigned_t) addr.lomem + (memsize - paddr));
                   addr.himem = (uint *) himem;
                   msg_printf(DBG,"memsize-paddr is 0x%llx, himem is 0x%x\n",
                         memsize-paddr, (uint) himem);
                }
                else {
                   himem = ((__psunsigned_t) addr.lomem +
                        (DIRECT_MAP-(first_time*PHYS_CHECK_LO)));
                   addr.himem = (uint *) himem;
                   first_time = 0;
                }
                addr.mask = MASK;
                msg_printf(DBG, "lo:%x, hi:%x, p: %llx, inc: %d\n",
                         addr.lomem, addr.himem,  paddr, inc);

#ifdef IP19
		/* Enable the bouncing curor via the R4k timer interrupt
	 	 * every four seconds
		 */
    		InstallHandler( INT_Level5, TimerHandler);
    		EnableTimer(4);
#endif

    		/*
    		 *  Kh_l
    		 */
    		fail |= three_l((__psunsigned_t)addr.lomem, (__psunsigned_t)addr.himem);
        }
    } /* else ... setjmp */

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Three Bit test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }

    /* Put back the orginal fault handler */
#ifdef IP19
    RemoveHandler(INT_Level5);
    StopTimer();
#endif
    if (!fail)
    	msg_printf(SUM, "Three Bit test -- PASSED\n");

SLOTFAILED:
#ifdef IP19
    RemoveHandler(INT_Level5);
    StopTimer();
#endif
    mem_err_clear(mem_slots);
    clear_nofault();
    return (fail);
}

