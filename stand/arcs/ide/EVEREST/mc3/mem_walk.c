
/*
 * mc3/mem_walk.c
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

#ident "$Revision: 1.18 $"

/*
 * mem_walk -- 	
 *
 *   This is a bubble memory walking test . User can specify
 *   the first address, last address.
 *		
 *   Nearly immediately after detecting a miscompare, the expected data,
 *   actual data and the address at an error occured are displayed.
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
extern void TimerHandler();

#if _MIPS_SIM == _ABI64
extern int walking_1(struct memstr *, register int, unsigned int, enum bitsense);
#else
extern int walking_1(struct memstr *, register int, unsigned int, enum bitsense,
		     unsigned long long paddr);
#endif

int
mem_walk(void)
{
    register unsigned int *ptr;
    register unsigned int fail = 0;
    struct memstr addr;
    register int inc = 1;
    register int bitcount = 32;
#if _MIPS_SIM == _ABI64
    __psunsigned_t paddr, memsize, himem;
    register unsigned int i;
#else
    unsigned long long i, paddr, memsize, himem;
    register int first_time;
#endif
    int val;

    msg_printf(SUM, "Memory walking test\n");
    if (quick_mode()) {
	  msg_printf(SUM, "\nRunning in quick mode...\n");
        inc = QUICK_INC_MEMWLK;
    }

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
#if _MIPS_SIM != _ABI64
        first_time = 1;
#endif

#ifdef IP19
	/* Enable the bouncing cursor via R4k Interrupts */
	/* every four seconds */
	InstallHandler( INT_Level5, TimerHandler);
	EnableTimer(4);
#endif

#if _MIPS_SIM == _ABI64
	paddr = PHYS_TO_K1(PHYS_CHECK_LO);
	addr.lomem = (uint *) paddr;
	himem = PHYS_TO_K1(memsize);
	addr.himem = (uint *) himem;
	addr.mask = MASK;
	msg_printf(DBG, "lo:%llx, hi:%llx, inc: %d, bitcount: %d\n",
                       addr.lomem, addr.himem, inc, bitcount);
	/*
	 * Walking 1 in memory
	 */
	fail |= walking_1(&addr, bitcount, inc, BIT_TRUE);

	/*
	 * Walking 0 in memory
	 */
	fail |= walking_1(&addr, bitcount, inc, BIT_INVERT);
#else
        for (i = 0; i < memsize; i += DIRECT_MAP) {
                if (i < PHYS_CHECK_LO)
                        paddr = PHYS_CHECK_LO;
                else
                        paddr = i;
                addr.lomem =
                   (uint *) map_addr(paddr, DIRECT_MAP, UNCACHED);
                if (memsize < (i + DIRECT_MAP)) {
                   himem = ((uint) addr.lomem + (memsize - paddr));
                   addr.himem = (uint *) himem;
                   msg_printf(DBG,"memsize-paddr is 0x%llx, himem is 0x%llx\n",
                         memsize-paddr, (uint) himem);
                }
                else {
                   himem = ((uint) addr.lomem +
                        (DIRECT_MAP-(first_time*PHYS_CHECK_LO)));
                   addr.himem = (uint *) himem;
                   first_time = 0;
                }
                addr.mask = MASK;
                msg_printf(DBG, "lo:%x, hi:%x, p: %llx\n",
                         addr.lomem, addr.himem,  paddr);
		msg_printf(DBG, "inc is %d and bitcount is %d\n", inc, bitcount);
    		/*
    	 	* Walking 1 in memory
    	 	*/
    		fail |= walking_1(&addr, bitcount, inc, BIT_TRUE, paddr);

    		/*
    	 	 * Walking 0 in memory
    	 	 */
    		fail |= walking_1(&addr, bitcount, inc, BIT_INVERT, paddr);
	} /* for .. i< memsize */
#endif

#ifdef IP19
	/* Put back the orginal vector */
	RemoveHandler( INT_Level5);
	StopTimer();
#endif

    } /* else ... setjmp */

SLOTFAILED:

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Memory walking test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }

    clear_nofault();
    return (fail);
}
