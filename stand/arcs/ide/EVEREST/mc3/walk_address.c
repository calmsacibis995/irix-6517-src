
/*
 * mc3/walk_address.c
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

#ident "$Revision: 1.19 $"

/*
 *  	addr_walk
 *
 *   This is a traditional test that check for the short and open
 *   of the address lines. Address lines that are greater or equal to
 *   the most significant address lines of the limits are not testsed.
 *   Test is done by doing byte read/write from first_address up to 
 *    last_address.
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
#include "mc3_reg.h"
#include "libsk.h"
#include "exp_parser.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include <sys/EVEREST/evconfig.h>
#include "uif.h"
#include "prototypes.h"

#define MASK 0xffffffff

extern jmp_buf mem_buf;
extern uint mem_slots;

#if _MIPS_SIM == _ABI64
extern int bwalking_addr(struct memstr *, int);
#else
extern int bwalking_addr(struct memstr *, int, unsigned long long);
#endif

int
walk_address(void)
{
    struct memstr addr;
    register int inc = 4;
    int fail = 0;
#if _MIPS_SIM == _ABI64
    int i;
    __psunsigned_t himem, memsize, paddr;
#else
    unsigned long long i, himem, memsize, paddr;
    register int first_time =1 ;
#endif
    int val;

    msg_printf(SUM,"Walking address test\n");

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
	date_stamp();
        set_nofault(mem_buf);

        memsize = readconfig_reg(RDCONFIG_MEMSIZE,0);

#if _MIPS_SIM == _ABI64
	paddr = PHYS_TO_MEM(PHYS_CHECK_LO);
	addr.lomem = paddr;
	himem = PHYS_TO_MEM(memsize);
	addr.himem = himem;
	addr.mask = MASK;
	msg_printf(DBG, "size:%llx, lo:%llx, hi:%llx\n",
                       memsize, addr.lomem, addr.himem);
	fail |= bwalking_addr(&addr, inc);
#else
        first_time = 1;
        for (i = 0; i < memsize; i += DIRECT_MAP) {
                if (i < PHYS_CHECK_LO)
                        paddr = PHYS_CHECK_LO;
                else
                        paddr = i;
                addr.lomem = map_addr(paddr, DIRECT_MAP, UNCACHED);
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
                addr.mask = MASK;
                msg_printf(DBG, "lo:%x, hi:%x, p: %llx\n",
                         addr.lomem, addr.himem,  paddr);

    		fail |= bwalking_addr(&addr, inc, paddr);
	} /* for i < memsize */
#endif
    } /* else ... setjmp */

SLOTFAILED:

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Walking address test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }

    date_stamp();
    clear_nofault();
    return (fail);
}
