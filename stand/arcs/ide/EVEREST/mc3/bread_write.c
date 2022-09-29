
/*
 * mc2/bread_write.c
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

#ident "$Revision: 1.11 $"

/*
 *  	bread_write - 
 *
 * NOTICE: Reserved 1M memory for diagnostics.
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
#include "mc3_reg.h"
#include "exp_parser.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include <sys/EVEREST/evconfig.h>
#include "fault.h"
#include "uif.h"
#include "prototypes.h"

extern jmp_buf mem_buf;
extern uint mem_slots;
extern void TimerHandler();
extern int bread_wr(struct memstr *, unsigned int, unsigned long long);

bread_write()
{
    register int fail = 0;
    struct memstr addr;
    register int inc = 1;
    register uint mem_slots;
    unsigned long long i, himem, memsize, paddr;
    register int first_time = 1;
    int val;

    msg_printf(SUM,"Byte Read-Write data test\n");
    if (quick_mode()) {
	msg_printf(SUM, "\nRunning in quick mode...\n");
	/*
       msg_printf(SUM, "\nQuick mode takes approximately 40 seconds");
       msg_printf(SUM, " for every 1GB of memory\n");
	*/
        inc = QUICK_INC_BRW;
    }
    else {
/*
    msg_printf(SUM, "\nWARNING: This test takes approximately 15 minutes ");
    msg_printf(SUM, "for every 32MB of memory\n");
*/
    }

    /*
     * from saio/lib/cache.s
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
        /* everest_error_show();*/
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
                msg_printf(DBG, "lo:%x, hi:%x, p: %llx, inc: %d\n",
                         addr.lomem, addr.himem,  paddr, inc);
    		/*
    		 * Write and read 0's and 1's
    		 */

    		addr.mask = 0xff;

#ifdef IP19
		/* Enable the bouncing cursor via R4k Interrupts */
		/* every four seconds */
		InstallHandler( INT_Level5, TimerHandler);
		EnableTimer(4);
#endif

    		fail |= bread_wr(&addr, inc, paddr);

    	} /* for i < memsize */
    } /* else ... setjmp */

SLOTFAILED:
#ifdef IP19
    /* Put back the orginal vector */
    RemoveHandler( INT_Level5);
    StopTimer();
#endif

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Byte Read-Write data test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }

    clear_nofault();

    return (fail);
}
