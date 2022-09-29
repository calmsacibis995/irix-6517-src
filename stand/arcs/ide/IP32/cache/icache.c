/*
 * cpu/icache.c
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

#ident "$Revision: 1.5 $"

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include "cache.h"

static jmp_buf fault_buf;

/*
 * defines for use with tag_inf.state
 */
#define PI_INVALID	0
#define PI_CLEAN_EXCL	2


int icachedata(void);
int icacheaddr(void);
int icachetag(void);
int icachefetch(void);
void ifetch_loop(void);
int icache_fill_writeback(uint *, uint);

/*
 * icache    First level instruction cache test
 */
u_int
icache()
{
    int error = 0;
    int r5000 = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT) == C0_IMP_R5000;

    invalidate_caches();

    msg_printf(VRB, "Instruction cache misc tests\n");
    if(!r5000) {
	msg_printf(VRB, "skipping for r10000/r12000\n");
	okydoky();
	return error;
    }
    run_uncached();
    busy (1);
    invalidate_caches();
    error = icachedata() || icachetag() || icachefetch();
    run_cached();
    invalidate_caches();

    if (error)
	sum_error("instruction cache misc");
    else
	okydoky();

    return error;
}

/*
 * icachedata - block mode read/write test
 *
 * all memory locations in the cache are set to the same values, so it
 * will not detect addressing errors well, if at all, but should have
 * very good coverage of single-bit and stuck-bit errors
 */
int icachedata(void)
{
    register uint *fillptr, i, j, pattern;
    uint fail;
    char * mesg;

    mesg = "Data";
    msg_printf(VRB, "icachedata\n");

    /*
     * fill icache with 0's, fill phys address with 0xFFFFFFFF, and
     * try writeback
     */
    pattern = 0;
    invalidate_caches();
    fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
    i = PI_SIZE / sizeof(uint);
    while (i--)
	*fillptr++ = pattern;
    fail = icache_fill_writeback ((uint *)PHYS_CHECK_LO, ~pattern);
    i = PI_SIZE / sizeof(uint);
    fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
    while (i--)
    {
	/*
	 * log error if writeback value is not same as pattern
	 */
	if (*fillptr != pattern)
	{
	    msg_printf(ERR,
		"Cache err: (1) Address:0x%x, Actual:0x%x, Expect:0x%x\n",
		K1_TO_K0((uint)fillptr), *fillptr, pattern);
	    fail = 1;
	}
	fillptr++;
    }

    /*
     * fill icache with 0xFFFFFFFF's, fill phys address with 0, and
     * try writeback
     */
    pattern = ~0;
    fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
    i = PI_SIZE / sizeof(uint);
    while (i--)
	*fillptr++ = pattern;
    fail = icache_fill_writeback ((uint *)PHYS_CHECK_LO, ~pattern);
    i = PI_SIZE / sizeof(uint);
    fillptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
    while (i--)
    {
	/*
	 * log error if writeback value is not same as pattern
	 */
	if (*fillptr != pattern)
	{
	    msg_printf(ERR,
		"Cache err: (2) Address:0x%x, Actual:0x%x, Expect:0x%x\n",
		K1_TO_K0((uint)fillptr), *fillptr, pattern);
	    fail = 1;
	}
	fillptr++;
    }

    /*
     * single-bit marching ones pattern
     */
    pattern = 1;
    j = 8 * sizeof(uint);

    while (j--)
    {
	fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
	i = PI_SIZE / sizeof(uint);
	while (i--)
	    *fillptr++ = pattern;
	fail = icache_fill_writeback ((uint *)PHYS_CHECK_LO, ~pattern);
	i = PI_SIZE / sizeof(uint);
	fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
	while (i--)
	{
	    /*
	     * log error if writeback value is not same as pattern
	     */
	    if (*fillptr != pattern)
	    {
		msg_printf(ERR,
		    "Cache err: (3) Address:0x%x, Actual:0x%x, Expect:0x%x\n",
		    K1_TO_K0((uint)fillptr), *fillptr, pattern);
		fail = 1;
	    }
	    fillptr++;
	}
	pattern <<= 1;
    }

    /*
     * march from 1 to all 1's
     */
    pattern = 1;
    j = 8 * sizeof(uint);

    while (j--)
    {
	fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
	i = PI_SIZE / sizeof(uint);
	while (i--)
	    *fillptr++ = pattern;
	fail = icache_fill_writeback ((uint *)PHYS_CHECK_LO, ~pattern);
	i = PI_SIZE / sizeof(uint);
	fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
	while (i--)
	{
	    /*
	     * log error if writeback value is not same as pattern
	     */
	    if (*fillptr != pattern)
	    {
		msg_printf(ERR,
		    "Cache err: (4) Address:0x%x, Actual:0x%x, Expect:0x%x\n",
		    K1_TO_K0((uint)fillptr), *fillptr, pattern);
		fail = 1;
	    }
	    fillptr++;
	}
	pattern <<= 1;
	pattern |= 1;
    }

    /*
     * march from all 1's to single one
     */
    pattern = ~0;
    j = 8 * sizeof(uint);

    while (j--)
    {
	busy (1);
	fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
	i = PI_SIZE / sizeof(uint);
	while (i--)
	    *fillptr++ = pattern;
	fail = icache_fill_writeback ((uint *)PHYS_CHECK_LO, ~pattern);
	i = PI_SIZE / sizeof(uint);
	fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
	while (i--)
	{
	    /*
	     * log error if writeback value is not same as pattern
	     */
	    if (*fillptr != pattern)
	    {
		msg_printf(ERR,
		    "Cache err: (5) Address:0x%x, Actual:0x%x, Expect:0x%x\n",
		    K1_TO_K0((uint)fillptr), *fillptr, pattern);
		fail = 1;
	    }
	    fillptr++;
	}
	pattern <<= 1;
    }
    busy (0);

    return fail;
}

/*
 * icacheaddr - address-in-address checks
 *
 * does not provide the bit error coverage of icachedata, but by storing
 * address-in-address and ~address-in-address, should find if each
 * cache location is individually addressable
 */
int icacheaddr(void)
{
    register uint *fillptr, *mapptr, i;
    uint fail;
    char * mesg;

    mesg = "Address";
    msg_printf(VRB, "icacheaddr\n");

    /*
     * fill icache with address-in-address data, set phys memory to 0xFFFFFFFF,
     * and try writeback
     */
    fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
    mapptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
    i = PI_SIZE / sizeof(uint);
    while (i--)
	*fillptr++ = (uint) mapptr++;
    fail = icache_fill_writeback ((uint *)PHYS_CHECK_LO, ~0);
    i = PI_SIZE / sizeof(uint);
    fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
    mapptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
    while (i--)
    {
	/*
	 * log error if writeback value is not same as pattern
	 */
	if (*fillptr != (uint) mapptr)
	{
	    msg_printf(ERR,
		"Cache err: (6) Address:0x%x, Actual:0x%x, Expect:0x%x\n",
		K1_TO_K0((uint)fillptr), *fillptr, (uint)mapptr);
	    fail = 1;
	}
	fillptr++;
	mapptr++;
    }

    /*
     * fill icache with ~address-in-address data, set phys memory to
     * 0xFFFFFFFF, and try writeback
     */
    fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
    mapptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
    i = PI_SIZE / sizeof(uint);
    while (i--)
	*fillptr++ = ~((uint) mapptr++);
    fail = icache_fill_writeback ((uint *)PHYS_CHECK_LO, ~0);
    i = PI_SIZE / sizeof(uint);
    fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
    mapptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
    while (i--)
    {
	/*
	 * log error if writeback value is not same as pattern
	 */
	if (*fillptr != ~((uint) mapptr))
	{
	    msg_printf(ERR,
		"Cache err: (7) Address:0x%x, Actual:0x%x, Expect:0x%x\n",
		K1_TO_K0((uint)fillptr), *fillptr, (uint)mapptr);
	    fail = 1;
	}
	fillptr++;
	mapptr++;
    }

    return fail;
}

/*
 * icachetag- icache tag read/write test
 *
 * EXTREMELY simple-minded - essentially, invalidates all tags in icache,
 * reads them back to check that they are invalid, force-loads all cache
 * lines, and reads the tags again to check that they are now valid.
 */
int icachetag(void)
{
    register uint *ptr, i, j;
    tag_info_t tag_info; /* and store the state and addr here */
    uint fail;

    fail = 0;
    j = PIL_SIZE / sizeof(int);
    msg_printf(VRB, "icachetag\n");

    /*
     * invalidate the caches
     */
    invalidate_caches();

    /*
     * check the primary icache tags for invalid
     */
    ptr = (uint *) PHYS_CHECK_LO;
    i = PI_SIZE / PIL_SIZE;
    while (i--)
    {
	get_tag_info(PRIMARYI, K0_TO_PHYS(ptr), &tag_info);

	/*
	 * error if cache line not invalid
	 */
	if (tag_info.state != PI_INVALID)
	{
	    msg_printf(ERR, "Cache tag err: icache not invalid at 0x%x\n",
		    K0_TO_PHYS(ptr));
	    msg_printf(ERR, "State: 0x%x, Phys Address: 0x%x\n",
		    tag_info.state, tag_info.physaddr);
	    fail = 1;
 	}
	ptr += j;
    }

    /*
     * invalidate the caches again - just in case
     */
    invalidate_caches();

    /*
     * fill the icache from the specified block - normal cache error
     * are enabled, so any fill errors should be seen
     */
    ptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
    i = PI_SIZE / PIL_SIZE;
    if (setjmp(fault_buf))
    {
	msg_printf(ERR, "exception on cache fill at 0x%x\n", (uint) ptr);
	if (_exc_save != EXCEPT_ECC)
	{
	    msg_printf(ERR, "no cache error seen - dumping status\n");
	    show_fault();
	}
	DoErrorEret();
	fail = 1;
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
	    fill_ipline(ptr);	/* force cache load */
	    ptr += j;
	}

	/*
	 * clear error handler  - delay is necessary do work around
	 * pipeline delays in interrupt/exception generation
	 */
	DELAY(2);
	nofault = 0;
    }

    /*
     * check the primary icache tags for invalid
     */
    ptr = (uint *) PHYS_CHECK_LO;
    i = PI_SIZE / PIL_SIZE;
    while (i--)
    {
	get_tag_info(PRIMARYI, K0_TO_PHYS(ptr), &tag_info);
	/*
	 * error if valid bit is not set
	 */
	if (tag_info.state != PI_CLEAN_EXCL)
	{
	    msg_printf(ERR, "Cache tag err: icache not valid at 0x%x\n",
		    K0_TO_PHYS(ptr));
	    msg_printf(ERR, "State: 0x%x, Phys Address: 0x%x\n",
		    tag_info.state, tag_info.physaddr);
	    fail = 1;
 	}
	ptr += j;
    }

    return fail;
}

/*
 * icachefetch - icache instruction fetch test
 *
 * rather simple - runs the same routine in both cached and uncached modes;
 * if the elapsed clock cycles are the same, the icache is not functioning
 * properly. One nice feature is the the same primitives can be used to
 * calculate the percent speed improvement of cached vs uncached routines
 */
int icachefetch(void)
{
    register uint *ptr, i, j;
    uint tcached, tuncached, fail;

    fail = 0;
    msg_printf(VRB, "icachefetch\n");

    /*
     * make sure caches are invalid to force icache fetch
     */
    invalidate_caches();

    /*
     * time ifetch_loop() in both cached and uncached modes
     */
    tcached = time_function(PHYS_TO_K0(KDM_TO_PHYS((uint)ifetch_loop)));
    tuncached = time_function(PHYS_TO_K1(KDM_TO_PHYS((uint)ifetch_loop)));

    /*
     * very simpleminded test - if it did not run faster in cached mode,
     * the cache is assumed to be bad
     */
    if (tcached >= tuncached)
    {
	msg_printf(ERR, "Cache err: no icache speedup in icache fetch test\n");
	msg_printf(ERR, 
	    "Cached Time: %x, Uncached Time: %x\n", tcached, tuncached);
	fail = 1;
    }

    return fail;
}

/*
 * dummy routine to use for simple icache functionality test
 */
void ifetch_loop(void)
{
    register	i,j;

    i = 0;
    j = 10000;
    while (j--)
    {
	i++;
    }
}

/*
 * fills icache with data whose PHYSICAL address is given, sets the
 * memory at the physical address to fillpat, then attempts to write
 * back the correct information from the icache onto the physical address
 *
 * returns 0 for success, 1 if cache errors occured
 */
int icache_fill_writeback(uint *physaddr, uint fillpat)
{
    register uint *fillptr, i, j;
    volatile uint dummy;
    uint fail;

    /*
     * words/ipline will be used a lot in this diag, so set it once
     */
    j = PIL_SIZE / sizeof(uint);

    /*
     * failure flag - will set if parse_cacherr detects problems
     */
    fail = 0;

    /*
     * clear out the current cache contents
     */
    invalidate_caches();

    /*
     * fill secondary cache and set primary dcache to fill pattern
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = PI_SIZE / sizeof(uint);
    while(i--)
	*fillptr++ = fillpat;

    /*
     * fill the icache from the specified block (in secondary)
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = PI_SIZE / PIL_SIZE;
    while(i--)
    {
	u_int old_cache_err = get_cache_err();
	tag_regs_t tag_regs;
	int bit, parity;
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;

	/*
	 * fill current icache line from the specified block
	 */
	fill_ipline(fillptr);
	if (old_cache_err != (old_cache_err =  get_cache_err()))
	    fail = 1;

	/*
	 * if we are on an R4600/R4700/R5000 we need to read the tag and then
	 * force it into the correct member of the set by using an
	 * index write which chooses the slot based on bit 13 of the
	 * vaddr.  This is a ugly hack, but it does ensure that 
	 * addr + 0x2000 goes into the other slot.
	 */
	if (orion || r4700 || r5000) {
	    read_tag(PRIMARYI, K0_TO_PHYS(fillptr), &tag_regs);
	    pi_HINV(fillptr);
	    write_tag(PRIMARYI, K0_TO_PHYS(fillptr), &tag_regs);
	    fill_ipline(fillptr);
        }
	if (old_cache_err !=  get_cache_err())
	    fail = 1;
	fillptr += j;
    }


    /*
     * invalidate data in primary dcache and flush fill pattern to
     * secondary cache - if primary data cache is still valid, the
     * icache writeback will fail
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);

    i = PI_SIZE / PDL_SIZE;
    while(i--)
    {
	pd_HWBINV((uint *)fillptr);
	fillptr += j;
    }

    /*
     * flush data in secondary cache to main memory and confirm via
     * uncached readback
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);

    /*
     * XXX: a quick hack to get it to work.  What really needs to happen is that
     * SIDL_SIZE needs to be set to the same value as PIL_SIZE for the 4600; this 
     * loop only works if SIDL_SIZE is the same as or greater than PIL_SIZE, 
     * which is true for the R4000, but not for the R4600.
     */
    i = PI_SIZE / (_sidcache_size ? SIDL_SIZE : PIL_SIZE);
    while(i--)
    {
	/* check for a secondary cache, note the above code that
	 * mentions secondary cache is ok if one doesn't exist. The Pdata
	 * will just go straight through to memory.
	 */
	if( _sidcache_size) {
		sd_HWB((uint)fillptr);
	}
	if ((dummy = *(uint *)K0_TO_K1((uint)fillptr)) != fillpat)
	{
	    msg_printf(ERR, 
		"Uncached readback at 0x%x - expected 0x%x, got 0x%x\n",
		(uint) K0_TO_K1((uint)fillptr), fillpat, dummy);
	    fail = 1;
	}
	fillptr += j;
    }

    /*
     * write back the data in the primary icache to the secondary
     * or to memory if secondary doesn't exist.
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = PI_SIZE / PIL_SIZE;
    while (i--)
    {
	u_int old_cache_err = get_cache_err();

	write_ipline (fillptr);
	if (old_cache_err !=  get_cache_err())
	    fail = 1;
	fillptr += j;
    }

    /* R.Frias added check for SC */
    if( _sidcache_size) {

    	/*
     	 * force writeback of data in secondary cache to phys memory
     	 */
    	fillptr = (uint *) PHYS_TO_K0(physaddr);
    	j = SIDL_SIZE / sizeof(uint);
    	i = PI_SIZE / SIDL_SIZE;
    	while (i--)
    	{
		flush2ndline(K0_TO_PHYS((uint)fillptr));
		fillptr += j;
    	}
    }

    return (fail);
}
