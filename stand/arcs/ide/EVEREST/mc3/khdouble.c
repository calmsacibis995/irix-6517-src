
/*
 * mc3/khdouble.c
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

#ident "$Id: khdouble.c,v 1.6 1996/05/08 23:55:07 olson Exp $"


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

#ifdef IP19
extern u_int	dwd_rdbuf[2];
extern u_int	dwd_wrtbuf[2];
extern u_int	dwd_tmpbuf[2];
#endif

extern jmp_buf mem_buf;
extern uint mem_slots;
extern int TimerHandler(int);
extern int Kh_dbl(__psunsigned_t, __psunsigned_t);


khdouble_drv()
{

    register unsigned int *ptr;
    register unsigned int fail = 0;
    register struct memstr addr;
    register int inc = 1;
#if _MIPS_SIM == _ABI64
    __psunsigned_t himem, memsize, paddr;
    unsigned int i;
#else
    unsigned long long i, himem, memsize, paddr;
#endif
#ifdef IP19
    int first_time = 1;
#endif
    int val;

    msg_printf(SUM, "Knaizuk Hartmann Doubleword Test\n");

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
	/* Enable the bouncing cursor via the R4k timer interrupt
	 * every four seconds
	 */
	InstallHandler( INT_Level5, TimerHandler);
	EnableTimer(4);
#endif

#if _MIPS_SIM == _ABI64
	paddr = PHYS_TO_K1(PHYS_CHECK_LO);
	addr.lomem = (uint *) paddr;
	himem = PHYS_TO_K1(memsize);
	addr.himem = (uint *) himem;
	addr.mask = MASK;
	msg_printf(DBG, "lo:%llx, hi:%llx, p: %llx, inc: %d\n",
                         addr.lomem, addr.himem,  paddr, inc);

	/*
	 *  Kh_l
	 */
	fail = Kh_dbl((__psunsigned_t)addr.lomem, (__psunsigned_t)addr.himem - 0x100);
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
                   msg_printf(DBG,"memsize-paddr is 0x%llx, himem is 0x%x\n",
                         memsize-paddr, (uint) himem);
                }
                else {
                   himem = ((uint) addr.lomem +
                        (DIRECT_MAP-(first_time*PHYS_CHECK_LO)));
                   addr.himem = (uint *) himem;
                   first_time = 0;
                }
                addr.mask = MASK;
                msg_printf(DBG, "lo:%x, hi:%x, p: %llx, inc: %d\n",
                         addr.lomem, addr.himem,  paddr, inc);
    		/*
    		 *  Kh_l
    		 */
    		fail = kh_dw(addr.lomem, addr.himem - 0x100);
       		if( fail) {
                	msg_printf(ERR, "Failing Address = %08x\n", dwd_tmpbuf[0]);
                	msg_printf( ERR, "Expected = %08x %08x\n", dwd_wrtbuf[1], dwd_wrtbuf[0]);
                	msg_printf( ERR, "  Actual = %08x %08x\n", dwd_rdbuf[1], dwd_rdbuf[0]);
                	msg_printf( ERR, "failing step %d\n", fail);
        	}

        }
#endif

#ifdef IP19
	/* Put back the orginal fault handler */
	RemoveHandler( INT_Level5);
	StopTimer();
#endif

    } /* else ... setjmp */

SLOTFAILED:

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Knaizuk Hartmann Doubleword test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }

    clear_nofault();
    return (fail);
}
