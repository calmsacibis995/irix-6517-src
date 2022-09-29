
/*
 * ide/IP30/cache/scache_ecc.c  Secondary Cache ECC RAM/Interrupt Test
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

#ident "ide/IP30/cache/scache_ecc.c: $Revision: 1.17 $"

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include <pon.h>	/* for run_cached & run_uncached */
#include "cache.h"

/*
 * number of data patterns to fed through
 */
#define NUM_DATA_PATS	4
#define NUM_XOR_PATS	4
static uint data_pats[] = { 0, 0x55555555, 0xbbbbbbbb, 0xffffffff };
static uint ecc_xor_pats[] = {0x55, 0xaa, 0xff, 0 };

int scache_ecc_intr(void);
int scache_ecc_ram(void);
void setstackseg(__psunsigned_t);


/*
 * scache_ecc - secondary cache ecc test called function. set up this
 *		way to allow for more subtests later if needed
 *
 * NOTE - both of these tests are based on the assumption that on cache
 * data loads or instruction fills, the R4000 ECC register contents are
 * only forced into the PRIMARY cache parity fields. If errors crop up
 * here, I will have to rewrite parts of these diagnostics
 */
u_int
scache_ecc(void)
{
    extern int _sidcache_size;
    int error = 0;

    /* Check for a secondary cache, skip if a PC version */
    if(! _sidcache_size) {
	msg_printf(VRB|PRCPU,
		"*** No Secondary Cache detected, test skipped\n");
	return(0);
    }

    msg_printf(VRB|PRCPU, "Secondary Cache ECC RAM/Interrupt Test\n");
    /* flush stack to uncached space */
    flush_cache();
    /* make sp uncached too */
    setstackseg(K1BASE);
    run_uncached();
    invalidate_caches();
	
    error = scache_ecc_ram() || scache_ecc_intr();

    invalidate_caches();
    setstackseg(K0BASE);
    run_cached();
    flush_cache();
    if (error)
	sum_error("secondary cache");
    else
	okydoky();

    return error;
}

/*
 * scache_ecc_ram - secondary cache ecc ram test
 *
 * all memory locations in the cache are set to 0, so normal ECC for all
 * dwords will be 0. Different data patterns are XOR'd into the calculated
 * ECC and verified for all dwords in the scache.
 */
int
scache_ecc_ram(void)
{
    register uint *fillptr, r_pattern, w_pattern, readin;
    uint fail, i, j, is_rev1, clear_pat;
    struct tag_regs tags;
    volatile uint dummy, dummy2;
    k_machreg_t cur_stat;
#if R10000
    int way,way_size;
#endif

    fail = 0;

    /*
     * save current status register contents for restoration later
     */
    cur_stat = GetSR();

    /*
     * start with caches known invalid so that uncached writes go to memory
     */
    invalidate_caches();

    is_rev1 = 0;
    clear_pat = 0;

    /*
     * disable ECC exceptions
     */    
    SetSR(cur_stat | SR_DE);

    msg_printf(DBG|PRCPU, "Initializing Memory\n");

    /*
     * fill an area of phys ram the size of the scache with all 0's
     */
    j = (uint) (SID_SIZE / sizeof(uint));
    fillptr = (uint *)PHYS_TO_K1(PHYS_CHECK_LO);
    while (j--)
	*fillptr++ = 0;

#if R10000
    tags.tag_lo = 0x0;
    tags.tag_hi = 0x0;
#endif
    
    /*
     * for each chosen set of XOR patterns, read in and verify current ECC
     * contents and write next pattern. since we do read and write in the
     * same pass, the patterns are jimmied so that the last XOR write will
     * restore original ECC value (0)
     */
    for (j = 0; j < NUM_XOR_PATS; j++)
    {
	/*
	 * get current readback and write data patterns
	 */
	r_pattern = ecc_xor_pats[j];
	if (is_rev1)
	    w_pattern = ~r_pattern;
	else
	    w_pattern = r_pattern;

	msg_printf(DBG|PRCPU, "ECC Ram - Writing 0x%x\n", r_pattern);

	/*
	 * run current test patterns for all double words in the scache
	 */
	i = (uint) (SID_SIZE / (2 * sizeof(uint)));	/*# dwords in scache*/
	way_size = i / 2; /* # dwords in 1 way of scache */
	fillptr = (uint *)PHYS_TO_K0(PHYS_CHECK_LO);
	while (i--)
	{
    	    *fillptr = 0;		/* force cache load and modify */
	    way = i / way_size;

	    _write_cache_data(CACH_SD, (ulong)fillptr | way, (uint*)&tags, 
			      w_pattern);

	    dummy = *fillptr;		/* reload primary cache line */
	    dummy2 = *(fillptr+1);

	    /*
	     * get ECC for selected dword by calling read_tag, which does
	     * an indexed load tag cache op
	     */
	    readin = _read_cache_data(CACH_SD, (ulong)fillptr | way, 
				      (uint*)&tags);
	    if (readin != r_pattern) {
		/*
		 * clean out the bad ECC pattern BEFORE calling errlog
		 * or the exception handler goes off into the weeds . . .
		 */
		*fillptr = 0;			/* cache load and modify */
		set_ecc(clear_pat);		/* forces calculated ecc */
		pd_iwbinv((void *)fillptr);	/* writeback to secondary */

		msg_printf(ERR|PRCPU,
		   "Bad ECC at scache dword 0x%x - expected 0x%x, got 0x%x\n"
		   "\tSRAM: %s\n",
		   K0_TO_PHYS(fillptr), ip30_ssram_swizzle(r_pattern),
		   ip30_ssram_swizzle(readin),
		   ip30_ssram(fillptr,
			ip30_ssram_swizzle(readin),
			ip30_ssram_swizzle(r_pattern), cpuid()));

		fail = 1;
		break;
	    }

	    /*
	     * next dword address
	     */
	    fillptr += 2;
	}
    }

    /*
     * make sure that the caches and ecc register are clean
     */
    set_ecc(0);
    invalidate_caches();

    /*
     * disable interrupts
     */
    SetSR(cur_stat);

    return (fail);
}

/*
 * scache_ecc_intr - secondary cache ecc interrupt test
 *
 * all memory locations in the cache are set to the same values, so it
 * will not detect addressing errors well, but forces ECC single-bit errors
 * for every each bit in the ECC for every line in the secondary cache
 */
int scache_ecc_intr(void)
{
    register uint *fillptr, d_pattern;
    uint fail, datacount, ecc_bshift;
    __psunsigned_t cur_err;
    volatile uint dummy;
#if R10000
    struct tag_regs tags;   
    int way;
#endif

    fail = 0;

#if R10000
    /* test all ways */
    for (way = 0; way < 2 ; way++) {
#endif

    /*
     * make sure we start with the caches in a known state
     */
    invalidate_caches();

    /*
     * start at the base of the scratch address area for each pattern
     */
    fillptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);

    /*
     * use each one of the data test patterns
     */
    for (datacount = 0; datacount < NUM_DATA_PATS; datacount++)
    {
	d_pattern = data_pats[datacount];

	msg_printf(DBG|PRCPU, "Testing pattern 0x%x\n", d_pattern);

	/*
	 * test each bit of scache ecc separately
	 */
	for (ecc_bshift = 0; ecc_bshift < 9; ecc_bshift++) {
	    __psunsigned_t old_cache_err;

	    /*
	     * invalidate secondary cache (and primary dcache) so will
	     * force refill - this should also work around the R4k
	     * uncached writeback bug
	     */
	    invalidate_dcache(PD_SIZE, PDL_SIZE);
	    invalidate_scache(SID_SIZE, SIDL_SIZE);

	    /*
	     * store currently selected data pattern into primary
	     * data cache
	     */
	    old_cache_err = get_cache_err();
	    *fillptr = d_pattern;

	    if (old_cache_err != get_cache_err())
		fail = 1;

	    /*
	     * force the dirty data in the primary dcache back into the
	     * secondary cache. Since the force bit is set in the status
	     * register, the writeback will XOR the value in the ECC
	     * register with the calculated ECC value
	     */
	    if (setjmp(cache_fault_buf))
	    {
		cur_err = get_cache_err();	/* get error status */

		/*
		 * if cache error exception, but not ECC_DFIELD,
		 * set error flag and log it
		 */
		if(!(cur_err & CACHERR_DC_D)) {
		    msg_printf(ERR|PRCPU,
			"Did not get expected cache datafield error exception at 0x%08lx, CO_CACHE_ERR: 0x%08x\n",
			(void *) fillptr, cur_err);
		    fail = 1;
		}

		/*
		 * invalidate cache line
		 */
		sd_hitinv((void *)((ulong)fillptr|way));

		/*
		 * load valid cache line
		 */
		dummy = *fillptr;

		/*
		 * force ERET back to next address to clear exception
		 */
		DoErrorEret();

		/*
		 * abort test after error
		 */
		if (fail)
		    break;
	    }
	    else
	    {
		nofault = cache_fault_buf;
		tags.tag_lo = d_pattern;
		tags.tag_hi = d_pattern;

		/*
		 * enable interrupts for cache parity/ECC while
		 * parity force is enabled
		 */
		SetSR (GetSR() & ~SR_DE);

		/* fool w/ ecc bits in 2ndary cache */
		_write_cache_data(CACH_SD, (ulong)fillptr|way, (uint*)&tags, 
				  ~(1 << ecc_bshift));

		/* write to memory */
		sd_HWBINV((uint*)((ulong)fillptr|way));

		/*
		 * read the data back into the primay dcache
		 */
		dummy = *fillptr;

		/*
		 * if it won't take ECC exception in 10 uS, it ain't
		 * gonna
		 */
		DELAY(10);

		/* NO CACHE ERROR - PRINT LOG MESSAGE */
	    	msg_printf(ERR|PRCPU,
	    	    "Scache ECC - no exception for pattern 0x%x, ECC bit %d\n",
		    d_pattern, ecc_bshift);
		fail = 1;
		break;
	    }

	    SetSR (GetSR() & ~SR_IEC);

	}				/* END of ECC bitwalk loop */
    }					/* END of data pattern loop */

#if R10000
    } /* end of for way loop */
#endif

    return fail;
}
