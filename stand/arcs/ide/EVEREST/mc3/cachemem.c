
/*
 * mc3/cachemem.c
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

#ident "$Revision: 1.25 $"

/*
 *  	CACHEMEM - Cached Address Uniqueness
 *
 *   This is a traditional, hueristic, rule-of-thumb, "address-in-address"
 *   memory test.  It also puts the complement of the address in the address,
 *   making passes in ascending order only. All of memory is stored and then
 *   checked.  All reads and writes are made through K0 seg, so the the reads
 *   and writes are cached. However, since the size of main memory exceeds
 *   the cache sizes, all data will be written to main memory and then read
 *   back.
 *
 *   This is not a particularly thorough test, and it depends upon a good
 *   cache to function correctly, but it is fast, at least compared to the
 *   other full-memory tests.
 *
 *   Nearly immediately after detecting a miscompare, the expected data,
 *   actual data and the address at an error occured are displayed.
 *
 *
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/invent.h>
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
#include "uif.h"
#include "prototypes.h"

#define INVERT	1
#define NONINVERT	0

jmp_buf cache_buf;
extern uint mem_slots;

extern long long vir2phys(__psunsigned_t);

int
cachemem(void)
{
    register __psunsigned_t first_address, last_address;
    unsigned long long memsize, i;
    unsigned int fail = 0;
    register __psunsigned_t osr;
    int val;
    int inc = 4;
#ifdef IP19
    int first_time = 1;
    __psunsigned_t paddr;
#endif

    msg_printf(SUM, "Memory with cache write-through test\n");
    if (quick_mode()) {
	 msg_printf(SUM, "\nRunning in quick mode...\n");
       inc = QUICK_INC_CACHEMEM;
    }

    osr = get_sr();

    mem_slots = mc3slots();
    mem_err_clear(mem_slots); /* clear error regs */
    memtest_init();
    if (val = setjmp(cache_buf)) {
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
	date_stamp();
        set_nofault(cache_buf);

    /* since this test is doing cached-data-accesses,
     * turn off DE bit so that ECC errors will not be automatically corrected
     */
    msg_printf(INFO, "Turning off ECC correction\n");
    osr = get_sr();
    set_sr(osr & ~SR_DE); 

    memsize = readconfig_reg(RDCONFIG_MEMSIZE,1);

    /* Enable the bouncing cursor via the R4k timer interrupt
     * every four seconds
     */
    InstallHandler( INT_Level5, TimerHandler);
    EnableTimer(4);

#if _MIPS_SIM == _ABI64
    first_address = (__psunsigned_t)(PHYS_TO_MEM(PHYS_CHECK_LO));
    last_address = (__psunsigned_t)(PHYS_TO_MEM(memsize));
    msg_printf(DBG,"start: %x, end: %x, inc:%d\n", 
		first_address, last_address, inc);

    	fail |= memaddr(first_address, last_address, NONINVERT, inc);
    	fail |= memaddr(first_address, last_address, INVERT, inc);
#else
    for (i = 0; i < memsize; i += DIRECT_MAP) {
        if (i < PHYS_CHECK_LO)
                paddr = PHYS_CHECK_LO;
        else
                paddr = i;
        first_address =
                map_addr(paddr, DIRECT_MAP, CACHED);
        if (memsize < (i + DIRECT_MAP)) {
                last_address = first_address + (memsize - paddr);
        }
        else {
                last_address = first_address+(DIRECT_MAP-(first_time*paddr));
                first_time = 0;
        }
	msg_printf(DBG,"start: %x, end: %x, p:0x%llx, inc:%d\n", 
		first_address, last_address, paddr, inc);

    	fail |= memaddr(first_address, last_address, NONINVERT, inc);
    	fail |= memaddr(first_address, last_address, INVERT, inc);
    } /* for ... */
#endif
    /* Put back the orginal fault handler */
    RemoveHandler( INT_Level5);
    StopTimer();
    } /* else... setjmp */

SLOTFAILED:

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Memory with cache write-through test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }

    msg_printf(INFO, "Restoring ECC correction\n");
    set_sr(osr);    /* restore old setting of Status register */
    clear_nofault();

    return (fail);
}

int
memaddr(__psunsigned_t first_address, __psunsigned_t last_address, int invert, int inc)
{
    register unsigned long long memsize;
    register unsigned actual;
    __psunsigned_t p;
    register int i;
    int passed = 0;
    unsigned int n_bk, bk;
    __psunsigned_t badaddr;
    int slot;
    uint badbits = 0;

    if (invert)
    msg_printf(INFO,"Writing complemented cached data from 0x%x to 0x%x\n", 
     first_address, last_address);
    else
	msg_printf(INFO,"Writing cached data from 0x%x to 0x%x\n", 
     first_address, last_address);

    msg_printf(DBG,"inc inside of memaddr is %d\n", inc);

    memsize = (last_address - first_address) + 4;
    p = first_address; 
    n_bk = (uint)(memsize / _dcache_size);
    msg_printf(DBG,"memsize is %llx and n_bk is %x\n", memsize, n_bk);

    for (bk = 0; bk < n_bk; bk++)
    {
	i = (uint)(_dcache_size / sizeof(int));
	while ((i--) && (p < last_address))
	{
	    c_wwrite(p, (invert ? ~(int)p : (int)p));
/*            if ((p % 0x80) == 0)
            	if (cpu_intr_pending()) {
			badaddr = vir2phys(p);
                        decode_address(badaddr,0, SW_WORD, &slot);
                        err_msg(MC3_INTR_ERR, &slot, badaddr);
			if (chk_memereg(mem_slots)) {
			    check_memereg(mem_slots);
			    mem_err_clear(mem_slots);
			}
		}
*/
	    p += inc;
	}
    }

    p = first_address;
    i = (uint)(_dcache_size / sizeof(int));
    while ((i--) && (p < last_address))
    {
	c_wwrite(p, (invert ? ~(int)p : (int)p)); 
/*        if ((p % 0x80) == 0)
        	if (cpu_intr_pending()) {
			badaddr = vir2phys(p);
                        decode_address(badaddr,0, SW_WORD, &slot);
                        err_msg(MC3_INTR_ERR, &slot, badaddr);
			if (chk_memereg(mem_slots)) {
			    check_memereg(mem_slots);
			    mem_err_clear(mem_slots);
			}
		}
*/
	p += inc;
    }
    msg_printf(VRB, "\n");

    if (invert)
    msg_printf(INFO,"Verifying complemented data from 0x%x to 0x%x\n", 
     first_address, last_address);
    else
    msg_printf(INFO,"Verifying data from 0x%x to 0x%x\n", 
     first_address, last_address);
    p = first_address;
    n_bk = (uint)(memsize / _dcache_size);
    for (bk = 0; bk < n_bk; bk++)
    {

	i = (uint)(_dcache_size / sizeof(int));
	while ((i--) && (p < last_address))
	{
	    actual = c_wread(p);
	    if (actual != (invert ? ~(int)p : (int)p))
	    {
		msg_printf(VRB, "\n");
		msg_printf(DBG,"addr: 0x%x, actual: 0x%x, expt: 0x%x\n",
			p, actual, (invert ? ~(int)p : (int)p));
		badbits = actual ^ (uint)p;
		badaddr = K1_TO_PHYS(p);
		decode_address(K1_TO_PHYS(badaddr), badbits, SW_WORD, &slot);
		err_msg(CACHEMEM_ERR, &slot, badaddr, 
			(invert ? ~(int)p : (int)p), actual);
		if (chk_memereg(mem_slots)) {
		    check_memereg(mem_slots);
		    mem_err_clear(mem_slots);
		}
		passed = 1;
	    }
	    p += inc;
	}
    }
    msg_printf(VRB, "\n");
    date_stamp();
    return (passed);
}
