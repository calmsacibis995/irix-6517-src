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

#ident "ide/IP30/cache/icache.c: $Revision: 1.20 $"

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include <pon.h>
#include "cache.h"

/* takes care of fact that R10K icache line is 16 words long and not 8 words */
#define FACTOR 2

/*
 * defines for use with tag_inf.state
 */
#define PI_INVALID	0
#define PI_VALID        1
#define PI_CLEAN_EXCL	2

static int icachedata(void);
static int icacheaddr(void);
static int icachetag(void);
static int icachefetch(void);
static void ifetch_loop(void);
static int icache_fill_writeback(uint *, uint);

/*
 * icache    First level instruction cache test
 */
int
icache(void)
{
	int error = 0;

	msg_printf(VRB|PRCPU, "Instruction cache misc tests\n");

	flush_cache();
	setstackseg(K1BASE);
	run_uncached();
	invalidate_caches();

	error = icachedata() || icacheaddr() || icachetag() || icachefetch();

	invalidate_caches();
	setstackseg(K0BASE);
	run_cached();
	flush_cache();

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
static int
icachedata_pattern(uint *ptr, unsigned long pattern, int op)/* 36-bit pattern */
{
	unsigned long pattern1 = pattern;
	unsigned long result0, result1;
	unsigned long mask;
	uint *saveptr;
	int fail = 0;
	int i;

	msg_printf(DBG|PRCPU, "Instruction cache data pattern=0x%x op %d\n",
		   pattern,op);

	saveptr = ptr;
	ptr = saveptr;
	i =  (PI_SIZE / PIL_SIZE) / FACTOR;
	while (i--) {
		if (op != 0) {
			pattern = (__psunsigned_t)ptr &
				(0xffffffffff & ((~(PIL_SIZE-1))|1));
			pattern1 = pattern | 1;
			if (op == 2) {
				pattern =  (~pattern ) & 0xffffffffff;
				pattern1 = (~pattern1) & 0xffffffffff;
			}
		}

		write_icache_line(ptr,0,pattern);
		write_icache_line(ptr,1,pattern1);
		ptr += (PIL_SIZE/sizeof(uint));
	}

	/* Read back Icache and make sure it matches.
	 */
	ptr = saveptr;
	i =  (PI_SIZE / sizeof(uint)) / FACTOR;
	while (i--) {
		result0 = read_icache_word(ptr,0);
		result1 = read_icache_word(ptr,1);

		if (op != 0) {
			pattern = (__psunsigned_t)ptr &
				(0xffffffffff & ((~(PIL_SIZE-1))|1));
			pattern1 = pattern | 1;
			if (op == 2) {
				pattern =  (~pattern ) & 0xfffffffff;
				pattern1 = (~pattern1) & 0xfffffffff;
			}
		}

		if (result0 != pattern || result1 != pattern1) {
			msg_printf(ERR|PRCPU,
				"I Cache Error: Addr 0x%x Expected 0x%x\n"
				"    Actual: way0 0x%x way1 0x%x\n",
				ptr,pattern,result0,result1);
			fail = 1;
		}

		ptr++;
	}

	return fail;
}

int
icachedata(void)
{
	unsigned long pattern;
	register uint *ptr;
	int fail = 0;

	ptr = (uint *)PHYS_TO_K0(PHYS_CHECK_LO);
	invalidate_caches();

	fail |= icachedata_pattern(ptr,0,0);
	fail |= icachedata_pattern(ptr,0xfffffffff,0);
	fail |= icachedata_pattern(ptr,0x555555555,0);
	fail |= icachedata_pattern(ptr,0xaaaaaaaaa,0);

	/* march a 1 down
	 */
	pattern = 1L << 35;
	while (pattern != 0) {
		fail |= icachedata_pattern(ptr,pattern,0);
		pattern >>= 1;
	}

#if 0	/* test takes a while as i$ not used, use for "extended" diags? */
	/* march a sticky 1 up.
	 */
	pattern = 3;		/* 0x1 already tested above */
	while (pattern != 0xfffffffff) {
		fail |= icachedata_pattern(ptr,pattern,0);
		pattern = (pattern << 1) | 1;
	}

	/* march from all 1s down to a single 1.
	 */
	pattern = 0xfffffffff;
	while (pattern != 0) {
		fail |= icachedata_pattern(ptr,pattern,0);
		pattern >>= 1;
	}
#endif

	return fail;
}

/*
 * icacheaddr - address-in-address checks
 *
 * does not provide the bit error coverage of icachedata, but by storing
 * address-in-address and ~address-in-address, should find if each
 * cache location is individually addressable
 */
static int
icacheaddr(void)
{
	uint *ptr = (uint *)PHYS_TO_K0(PHYS_CHECK_LO);
	int fail = 0;

	fail |= icachedata_pattern(ptr,0,1);	/*  ptr */
	fail |= icachedata_pattern(ptr,0,2);	/* ~ptr */

	return fail;
}

/*
 * icachetag- icache tag read/write test
 *
 * EXTREMELY simple-minded - essentially, invalidates all tags in icache,
 * reads them back to check that they are invalid, force-loads all cache
 * lines, and reads the tags again to check that they are now valid.
 */
int
icachetag(void)
{
	register uint *ptr, i, j;
	tag_info_t tag_info0; /* and store the state and addr here */
	tag_info_t tag_info1;
	int fail = 0;

	j = PIL_SIZE / sizeof(int);
	invalidate_caches();

	/*
	 * check the primary icache tags for invalid
	 */
	ptr = (uint *) K0_TO_PHYS(PHYS_CHECK_LO);
	i = PI_SIZE / PIL_SIZE;
	while (i--) {
		get_tag_info(PRIMARYI,(__psunsigned_t)ptr,0,&tag_info0);
		get_tag_info(PRIMARYI,(__psunsigned_t)ptr,1,&tag_info1);

		/*
		 * error if cache line not invalid
		 */
		if (tag_info0.state != PI_INVALID) {
			msg_printf(ERR|PRCPU,
			    "Cache tag err: icache not invalid at 0x%x\n"
			    "State: 0x%x, Phys Address: 0x%x\n",
			    ptr,tag_info0.state, tag_info0.physaddr);
			fail = 1;
		}
		if (tag_info1.state != PI_INVALID) {
			msg_printf(ERR|PRCPU,
			    "Cache tag err: icache not invalid at 0x%x\n"
			    "State: 0x%x, Phys Address: 0x%x\n",
			    ptr,tag_info1.state, tag_info1.physaddr);
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
	ptr = (uint *)PHYS_TO_K0(PHYS_CHECK_LO);
	i = (PI_SIZE / PIL_SIZE) / FACTOR;
	if (setjmp(cache_fault_buf)) {
		msg_printf(ERR|PRCPU, "exception on cache fill at 0x%x\n", ptr);
		if (_exc_save != EXCEPT_ECC) {
			msg_printf(ERR|PRCPU, "no cache error seen - dumping status\n");
			show_fault();
		}
		fail = 1;
		DoErrorEret();
		/*NOTREACHED*/
	}

	nofault = cache_fault_buf;	/* enable error handler */

	/*
	 * enable cache interrupts, but not for parity/ecc
	 */
	SetSR(GetSR() | SR_DE);

	while (i--) {
		write_icache_line(ptr,0,0x055555555); /* load icache */
		write_icache_line(ptr,1,0x1aaaaaaaa); /* load icache */
		ptr += j;
	}

	/*
	 * clear error handler  - delay is necessary do work around
	 * pipeline delays in interrupt/exception generation
	 */
	DELAY(2);
	nofault = 0;

	/*
	 * check the primary icache tags for invalid
	 */
	ptr = (uint *)K0_TO_PHYS(PHYS_CHECK_LO);
	i = (PI_SIZE / PIL_SIZE) / FACTOR;
	while (i--) {
		get_tag_info(PRIMARYI,(__psunsigned_t)ptr,0,&tag_info0);
		get_tag_info(PRIMARYI,(__psunsigned_t)ptr,1,&tag_info1);
		/*
		 * error if valid bit is not set
	 	 */
		if (tag_info0.state != PI_VALID) {
			msg_printf(ERR|PRCPU,
			    "Cache tag err: icache not valid at 0x%lx\n"
			    "State: 0x%x, Phys Address: 0x%lx\n",
			    ptr,tag_info0.state, tag_info0.physaddr);
			fail = 1;
		}
		if (tag_info1.state != PI_VALID) {
			msg_printf(ERR|PRCPU,
			    "Cache tag err: icache not valid at 0x%lx\n"
			    "State: 0x%x, Phys Address: 0x%lx\n",
			    ptr,tag_info1.state, tag_info1.physaddr);
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
int
icachefetch(void)
{
	uint tcached, tuncached, fail;

	fail = 0;

	/*
	 * make sure caches are invalid to force icache fetch
	 */
	invalidate_caches();

	/*
	 * time ifetch_loop() in both cached and uncached modes
	 */
	tcached = time_function(PHYS_TO_K0(KDM_TO_PHYS(ifetch_loop)));

	tuncached = time_function(PHYS_TO_K1(KDM_TO_PHYS(ifetch_loop)));

	/*
	 * very simpleminded test - if it did not run faster in cached mode,
	 * the cache is assumed to be bad
	 */
	msg_printf(DBG|PRCPU,
	    "Cached Time: %x, Uncached Time: %x\n", tcached, tuncached);
	if (tcached >= tuncached) {
		msg_printf(ERR|PRCPU,
		    "Cache err: no icache speedup in icache fetch test\n"
		    "Cached Time: %x, Uncached Time: %x\n", tcached, tuncached);
		fail = 1;
	}

	return fail;
}

/*
 * dummy routine to use for simple icache functionality test
 */
void
ifetch_loop(void)
{
	register volatile i,j;

	i = 0;
	j = 10000;
	while (j--) {
		i++;
	}
}
