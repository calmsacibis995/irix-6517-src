/*
 * cpu/cache_par.c
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

#ident "$Revision: 1.3 $"

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <setjmp.h>
#include <fault.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include "cache.h"

static jmp_buf fault_buf;

int icache_parity(void);
int dcache_parity(void);

#ifdef DEBUG
unsigned int i_tags[512];
unsigned int d_tags[512];
unsigned int s_tags[8192];
#endif	/* DEBUG */

/*
 * patterns to force-load to check parity - currently, all 0's, all 1's, and
 * 1 each of even and odd byte parity
 */
#define NUM_PAR_PATS	4
uint parity_pattern[] = { 0, 0x11111111, 0x10101010, 0xffffffff };

/* First level data cache data parity test */
u_int
dcache_par()
{
    int	error = 0;
    int r5000 = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT) == C0_IMP_R5000;

    msg_printf(VRB, "Data cache data parity test\n");
    if (!r5000) {
	msg_printf(DBG, "skipping dcache parity test for r10000/r12000\n");
        okydoky();
	return error;
    }
    run_uncached();
    invalidate_caches();
    error = dcache_parity();
    run_cached();
    invalidate_caches();
    if (error)
	sum_error("data cache data parity");
    else
	okydoky();

    return error;
}

/* First level instruction cache data parity test */
u_int
icache_par()
{
    int	error = 0;
    int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
    int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
    int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_TRITON;
    int r10000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R10000;
    msg_printf(VRB, "Instruction cache data parity test\n");

#if 0
    XXX these all possible O2 CPUs, skip this test for IP32
    if (orion || r4700 || r5000 || r10000) {
#else
    {
#endif
	msg_printf(DBG, "skipping icache parity test for orion/r4700/r5000/r10000/r12000\n");
        okydoky();
	return error;
    }

    run_uncached();
    invalidate_caches();
    error = icache_parity();
    run_cached();
    invalidate_caches();
    if (error)
	sum_error("instruction cache data parity");
    else
	okydoky();

    return error;
}

/*
 * icache_parity - instruction cache block mode parity test
 *
 * all memory locations in the cache are set to the same values, so it
 * will not detect addressing errors well, if at all, but should have
 * very good coverage of cache line parity errors
 */
int icache_parity(void)
{
    register uint i, j, k, pattern;
    register volatile uint *fillptr;
    uint fail, cur_stat, cur_err;
    int error = 0;
    uint bucket;

    /*
     * words/ipline will be used a lot in this diag, so set it once
     */
    j = PIL_SIZE / sizeof(uint);

    /*
     * save current status register contents for restoration later
     */
    cur_stat = GetSR();

    for (k = 0; k < NUM_PAR_PATS; k++)
    {
        fail = 0;

	/*
	 * get current parity test pattern
	 */
	pattern = parity_pattern[k];
	msg_printf(DBG, "testing data pattern 0x%x\n", pattern);

	/*
	 * invalidate caches so will force refill
	 */
	invalidate_caches();			/* from saio/lib/cache.s */
	flushall_tlb();

	/*
	 * Fill main memory current test pattern
	 */
	fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
	i = PI_SIZE / sizeof(uint);
	while (i--)
	    *fillptr++ = pattern;

	/*
	 * fill the icache from the specified block - normal cache error
	 * are enabled, so any fill errors should be seen
	 */
	fillptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
	i = PI_SIZE / PIL_SIZE;
	if (setjmp(fault_buf))
	{
	    show_fault();
	    DoEret();
	    SetSR(cur_stat);
	    return ++error;
	}
	else
	{
	    nofault = fault_buf;	/* enable error handler */

	    /*
	     * enable cache interrupts, but not for parity/ecc
	     */
	    SetSR(SR_FORCE_OFF | SR_DE);

	    while(i--)
	    {
		fill_ipline(fillptr);	/* force cache load */
		fillptr += j;
	    }

	    /*
	     * clear error handler  - delay is necessary do work around
	     * pipeline delays in interrupt/exception generation
	     */
	    DELAY(2);
	    nofault = 0;
	}


	/*
	 * invalidate caches so will force refill
	 */
	invalidate_caches();			/* from saio/lib/cache.s */
	flushall_tlb();

	/*
	 * set parity pattern guaranteed to fail for ever other byte in cache
	 * line for any uniform pattern - half of the bytes are set to odd
	 * parity, half to even
	 */
	set_ecc(0x55);

	/*
	 * force-fill the cache again, but this time we force bad parity, so
	 * should see parity errors on every cache line. Error set if NO cache
	 * parity errors seen
	 */
	fillptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
	i = PI_SIZE / PIL_SIZE;
	while(i--)
	{
	    msg_printf(DBG, "fillptr 0x%x\n", fillptr);

	    if (setjmp(fault_buf))
	    {
	        if (_exc_save != EXCEPT_ECC) {
		    show_fault();
		    DoEret();
		    return ++error;
		}

		cur_err = get_cache_err();	/* get new error status */

		/*
		 * if cache error exception, but not ECC_DFIELD, set error
		 * flag and log it
		 */
		if(!(cur_err & CACHERR_ED))
		{
		    msg_printf(ERR,
			"Did not get expected cache datafield error exception at 0x%08x, CO_CACHE_ERR: 0x%08x\n",
			(uint) fillptr, cur_err);
		    fail = 1;
		    error++;
		}

		/*
		 * write error address again with force disable to
		 * clear cache parity error
		 */
		SetSR(GetSR() & ~SR_CE);

		/*
		 * invalidate cache line
		 */
		if (_sidcache_size) {
			sd_hitinv((uint)fillptr);
			bucket = *fillptr;	
		} else
			pi_HINV((uint)fillptr);

		fill_ipline(fillptr);

		/*
		 * force ERET back to next address to clear exception
		 */
		DoEret();

		/*
		 * abort test after error
		 */
		if (fail)
		    break;
	    }
	    else
	    {
		nofault = fault_buf;

		/*
		 * invalidate cache line
		 */
		if (_sidcache_size) {
			sd_hitinv((uint)fillptr);
			bucket = *fillptr;
		} else
			pi_HINV((uint)fillptr);

		/*
		 * enable interrupts for cache parity/ECC while parity force is
		 * enabled
		 */
		SetSR (SR_FORCE_ON);
		set_cause(0);

		/*
		 * write pattern into cache line.  this violate the cache
		 * subset rule if the 2nd cache line is invalid.  thus the
		 * 'bucket = *fillptr' line up there
		 */
		fill_ipline(fillptr);	/* force load the cache line */

		/*
		 * disable force so that errors on writeback are parity, not
		 * secondary ECC
		 */
		SetSR (SR_FORCE_OFF);

		/*
		 * write icache data back to secondary. since icache line has
		 * bad parity, this should generate parity exception
		 */
		write_ipline (fillptr);	/* force bad parity writeback */

		/*
		 * if it won't take parity exception in 5 uS, it ain't gonna
		 */
		DELAY(5);

#ifdef DEBUG
{
	unsigned int i;

	/*
	 * don't want to print the tags within the tags-reading loops since
	 * doing so may alter the icache even though we are running uncached.
	 * this may happen if the printf routine call some routines through
	 * function pointers and the addresses in the pointers are cached
	 */ 
	for (i = 0x88800000; i < 0x88802000; i += 16)
		read_tag(PRIMARYI, i, &i_tags[(i - 0x88800000) / 16]);
	for (i = 0x88800000; i < 0x88802000; i += 16)
		read_tag(PRIMARYD, i, &d_tags[(i - 0x88800000) / 16]);
	for (i = 0x88800000; i < 0x88900000; i += 128)
		read_tag(SECONDARY, i, &s_tags[(i - 0x88800000) / 128]);

	for (i = 0x88800000; i < 0x88802000; i += 16)
		if (i_tags[(i - 0x88800000) / 16] & PSTATEMASK)
			printf("ICACHE: 0x%08x, 0x%08x\n", i, i_tags[(i - 0x88800000) / 16]);
	for (i = 0x88800000; i < 0x88802000; i += 16)
		if (d_tags[(i - 0x88800000) / 16] & PSTATEMASK)
			printf("DCACHE: 0x%08x, 0x%08x\n", i, d_tags[(i - 0x88800000) / 16]);
	for (i = 0x88800000; i < 0x88900000; i += 128)
		if (s_tags[(i - 0x88800000) / 128] & SSTATEMASK)
			printf("SCACHE: 0x%08x, 0x%08x\n", i, s_tags[(i - 0x88800000) / 128]);
}
#endif	/* DEBUG */

		/* NO CACHE ERROR - PRINT LOG MESSAGE */
		msg_printf(ERR,
		    "Icache Parity - no exception at Addr: 0x%x\n",
		    (uint) fillptr);
		error++;
		nofault = 0;
		break;
	    }
		    
	    fillptr += j;	/* next line in primary cache */
	}

	/*
	 * restore original diagnostic status values
	 */
	SetSR (cur_stat);
    }

    /*
     * restore original diagnostic status values in case of error exit
     */
    SetSR (cur_stat);

    return error;
}

/*
 * dcache_parity - data cache block mode parity test
 *
 * all memory locations in the cache are set to the same values, so it
 * will not detect addressing errors well, if at all, but should have
 * very good coverage of cache line parity errors
 */
int dcache_parity(void)
{
    register uint i, j, k, pattern;
    register volatile uint *fillptr, *ptr;
    uint fail, cur_stat, cur_err;
    volatile uint dummy;
    int error = 0;

    cur_stat = GetSR();

    for (k = 0; k < NUM_PAR_PATS; k++)
    {
        fail = 0;
	/*
	 * get current parity test pattern
	 */
	pattern = parity_pattern[k];
	msg_printf (DBG, "testing data pattern 0x%x\n", pattern);

	/*
	 * invalidate caches and tlb just in case they are valid
	 */
	invalidate_caches();			/* from saio/lib/cache.s */
	flushall_tlb();

	/*
	 * Fill main memory with current test pattern
	 */
	fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
	i = PD_SIZE / sizeof(uint);
	while (i--)
	    *fillptr++ = pattern;

	/*
	 * invalidate caches so will force refill
	 */
	invalidate_caches();			/* from saio/lib/cache.s */

	/*
	 * fill the cache from the specified block - normal parity calculation
	 * are enabled, so any parity errors seen should be real
	 */
	fillptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
	i = PD_SIZE / sizeof(uint);

	if (setjmp(fault_buf))
	{
	    show_fault();
	    DoEret();
	    SetSR(cur_stat);
	    return ++error;
	}
	else
	{
	    nofault = fault_buf;	/* enable error handler */

	    /*
	     * enable cache interrupts, but not for parity/ecc
	     */
	    SetSR(SR_FORCE_OFF | SR_DE);

	    while(i--)
	    {
		*fillptr++ = pattern;	/* force cache load */
	    }

	    /*
	     * clear error handler  - delay is necessary do work around
	     * pipeline delays in interrupt/exception generation
	     */
	    DELAY(2);
	    nofault = 0;
	}

	/*
	 * invalidate caches so will force refill
	 */
	invalidate_caches();			/* from saio/lib/cache.s */
	flushall_tlb();

	/*
	 * set parity pattern guaranteed to fail for ever other byte in cache
	 * line for any uniform pattern - half of the bytes are set to odd
	 * parity, half to even
	 */
	set_ecc(0x55);

	/*
	 * force-fill the cache again, but this time we force bad parity, so
	 * should see parity errors on every cache dword. Error set if NO cache
	 * parity errors seen
	 */
	fillptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
	i = PD_SIZE / (2 * sizeof(uint)) ;
	while(i--)
	{
	    if (setjmp(fault_buf))
	    {
		cur_err = get_cache_err();	/* get new error status */

		/*
		 * if cache error exception, but not ECC_DFIELD, set error
		 * flag and log it
		 */
		if(!(cur_err & CACHERR_ED))
		{
		    msg_printf(ERR,
			"Did not get expected cache datafield error exception at 0x%08x, CO_CACHE_ERR: 0x%08x\n",
			(uint) fillptr, cur_err);
		    fail = 1;
		    error++;
		}

		/*
		 * write error address again with force disable to
		 * clear cache parity error
		 */
		SetSR(GetSR() & ~SR_CE);

		/*
		 * invalidate cache line
		 */
		if (_sidcache_size)
			sd_hitinv((uint)fillptr);
		else
			pd_HINV((uint)fillptr);

		/*
		 * clear parity for cache line
		 */
		*fillptr = pattern;	/* force cache store */

		/*
		 * force ERET back to next address to clear exception
		 */
		DoEret();

		/*
		 * abort test after error
		 */
		if (fail)
		    break;
	    }
	    else
	    {
		msg_printf(DBG, "testing location 0x%08x\n", (uint)fillptr);
		nofault = fault_buf;

		/*
		 * invalidate cache line
		 */
		if (_sidcache_size)
			sd_hitinv((uint)fillptr);
		else
			pd_HINV((uint)fillptr);

		/*
		 * enable interrupts for cache parity/ECC while parity force is
		 * enabled
		 */
		SetSR (SR_FORCE_ON);
		set_cause(0);

		/*
		 * write pattern into cache line
		 */
		*fillptr = pattern;	/* force cache store */
		dummy = *fillptr;	/* load from cache line again */

		/*
		 * if it won't take parity exception in 5 uS, it ain't gonna
		 */
		DELAY(5);

		/* NO CACHE ERROR - PRINT LOG MESSAGE */
		msg_printf(ERR, "Dcache Parity - no exception at Addr: 0x%x\n",
		    (uint) fillptr);
		error++;
		nofault = 0;
		break;
	    }
		    
	    fillptr += 2;	/* next dword in primary cache */
	}

	/*
	 * restore original diagnostic values for next pass
	 */
	SetSR (cur_stat);
    }

    /*
     * restore original diagnostic status values in case of error exit
     */
    SetSR (cur_stat);

    return error;
}
