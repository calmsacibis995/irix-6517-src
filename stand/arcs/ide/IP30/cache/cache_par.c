/*
 * cpu/cache_par.c
 *
 *
 * Copyright 1996, Silicon Graphics, Inc.
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

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <setjmp.h>
#include <fault.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include <pon.h>
#include "cache.h"

static int icache_parity(void);
static int dcache_parity(void);

/*
 * patterns to force-load to check parity - currently, all 0's, all 1's, and
 * 1 each of even and odd byte parity
 */
#define NUM_PAR_PATS	4
uint parity_pattern[] = { 0x11111111, 0, 0x10101010, 0xffffffff };
uint fail_parity_bits[] = { 0xf, 0xf, 0x0, 0xf };

/* First level data cache data parity test */
int
dcache_par(void)
{
	int error = 0;

	msg_printf(VRB|PRCPU, "Data cache data parity test\n");

	/* flush stack to uncached space */
	flush_cache();
	/* make sp uncached too */
	setstackseg(K1BASE);
	run_uncached();

	error = dcache_parity();

	setstackseg(K0BASE);
	run_cached();
	flush_cache();

	if (error)
		sum_error("Data cache data parity");
	else
		okydoky();

	return error;
}

/* First level instruction cache data parity test */
int
icache_par(void)
{
	int error = 0;

	msg_printf(VRB|PRCPU, "Instruction cache data parity test\n");

	flush_cache();			/* flush stack so we can be uncached */
	setstackseg(K1BASE);		/* make sp uncached too */
	run_uncached();

	error = icache_parity();

	setstackseg(K0BASE);
	run_cached();
	flush_cache();

	if (error)
		sum_error("Instruction cache data parity");
	else
		okydoky();

	return error;
}

/*
 * write_icache_badtag - simple routine that writes an 
 *                       instruction cache entry with a
 *                       bad parity in the address tag
 *
 */

static void
write_icache_badtag(ulong addr) 
{
	tag_regs_t tag;

	/* Create address part of tag */
	tag.tag_hi = (uint) ((addr & PTAG1) >> TAGHI_PI_PTAG1_SHIFT);
	tag.tag_lo = (uint) ((addr & PTAG0) >> TAGLO_PI_PTAG0_SHIFT);

	/* force bad tag parity */
	if (!calc_r10k_parity(addr & (PTAG0 | PTAG1)))
		tag.tag_lo |= TAGLO_PI_TP;

	/* rest of tag, parity correct */
	tag.tag_lo |= TAGLO_PI_LRU | TAGLO_PI_PSTATE | TAGLO_PI_SP;

	_write_tag(CACH_PI, (ulong)addr, (void*)&tag);
}

/*
 * icache_parity_test - causes a tag address parity error and makes
 *                      sure that an exception occurs
 *
 *                      note: does NOT cause a data error like the
 *                            R4k diags
 */
static int
icache_parity_test(void)
{
	register uint i, j;
	register volatile uint *fillptr;
	volatile uint bucket;
	__psunsigned_t cur_err;
	uint fail = 0;
	int error = 0;
	register uint way;

	j = PIL_SIZE / sizeof(uint);		/* words/ipline */

	invalidate_caches();			/* invalidate to force refill */
	flushall_tlb();

	/*
	 * force-fill the cache again, but this time we force bad parity, so
	 * should see parity errors on every cache line. Error set if NO cache
	 * parity errors seen
	 */
	fillptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
	i = PI_SIZE / PIL_SIZE;

	while(i--) {

		/* check both ways in cache */
		for (way=0 ; way<=1 ; way++) {
			if (setjmp(cache_fault_buf)) {
				if (_exc_save != EXCEPT_ECC) {
					show_fault();
					DoEret();
					return ++error;
				}

				/* current error status */
				cur_err = get_cache_err();

				/*
		 		 * If cache error exception, but not ECC_DFIELD,
				 * set error flag and log it.
		 		 */
				if (!(cur_err & CACHERR_DC_TA)) {
					msg_printf(ERR|PRCPU,
					    "Did not get expected cache tag address error exception at 0x%08lx, CO_CACHE_ERR: 0x%08x\n",
					    (void *) fillptr, cur_err);
					fail = 1;
					error++;
				}

				/*
		 		 * Force ERET back to next address to clear
				 * exception.
		 		 */
				DoEret();

				/* abort test after error */
				if (fail)
					goto done;
			}
			else {
				nofault = cache_fault_buf;

				/* make sure exception is taken on parity
				 * errors
				 */
				SetSR(GetSR() & ~SR_DE);

				/* set a value */
				*fillptr = 0x0;

				/* now write data w/bad tag parity into cache */
				write_icache_badtag((ulong)fillptr|way);

				/* hit invalidate checks tag parity and will
				 * cause exception
				 */
				pi_HINV((void*)((ulong)fillptr|way));

				/*
		 		 * if it won't take parity exception in 5 uS,
				 * it ain't gonna
		 		 */
				DELAY(5);

				/* NO CACHE ERROR - PRINT LOG MESSAGE */
				msg_printf(ERR|PRCPU,
				    "Icache Parity - no exception at Addr: 0x%x\n",
				    (void *) fillptr);
				error++;
				nofault = 0;
				goto done;
			}

		}

		fillptr += j;	/* next line in primary cache */

	}

	invalidate_caches();
	flushall_tlb();

done:
	return error;
}

/*
 * icache_parity - instruction cache block mode parity test
 *
 * all memory locations in the cache are set to the same values, so it
 * will not detect addressing errors well, if at all, but should have
 * very good coverage of cache line parity errors
 */
static int
icache_parity(void)
{
	k_machreg_t oldsr;
	int rc = 0;

	oldsr = GetSR();

	/* test tag address parity */
	rc = icache_parity_test();

	SetSR(oldsr);

	return rc;
}

/*
 * dcache_de_bit_test - checks to see if DE bit in Status Reg works
 *                    - also checks real parity errors ie if
 *                      real parity errors occur during this test
 *                      they will appear here
 */

int dcache_de_bit_test(uint pattern, k_machreg_t cur_stat) 
{
	register uint i;
	register volatile uint *fillptr;

	msg_printf (DBG|PRCPU, "testing data pattern 0x%x\n", pattern);

	invalidate_caches();
	flushall_tlb();

	/* Fill main memory with current test pattern */
	fillptr = (uint *) PHYS_TO_K1(PHYS_CHECK_LO);
	i = PD_SIZE / sizeof(uint);
	while (i--) {
		*fillptr++ = pattern;
	}

	invalidate_caches();			/* force a refill */

	/*
	 * fill the cache from the specified block - normal parity calculation
	 * are enabled, so any parity errors seen should be real
	 */
	fillptr = (uint *)PHYS_TO_K0(PHYS_CHECK_LO);

	i = PD_SIZE / sizeof(uint);

	if (setjmp(cache_fault_buf)) {
		show_fault();
		DoEret();
		SetSR(cur_stat);
		return 1;
		/*NOTREACHED*/
	}
	nofault = cache_fault_buf;	/* enable error handler */

	/*
	 * enable cache interrupts, but not for parity/ecc
	 */
	SetSR(GetSR() | SR_FORCE_OFF | SR_DE);

	while(i--) {
		*fillptr++ = pattern;	/* force cache load */
	}

	/*
	 * clear error handler  - delay is necessary do work around
	 * pipeline delays in interrupt/exception generation
	 */
	DELAY(2);
	nofault = 0;

	return 0;
}

/*
 * dcache_parity_test - causes a parity error and makes
 *                      sure that an exception occurs
 *
 */
static int
dcache_parity_test(uint pattern, uint parity_bits) 
{
	register uint i;
	register uint way;
	register volatile uint *fillptr;
	__psunsigned_t cur_err;
	volatile uint dummy;
	uint fail = 0;
	int error = 0;
	uint data[2];

	invalidate_caches();
	flushall_tlb();

	/* init data */
	data[0] = pattern;
	data[1] = 0x0;

#define NUM_WORDS_PER_CACHE_LINE 8

	/* we fill the cache with bad-parity data via the index store data
	 * cache-ops. We run in cached space so that later reads catch
	 * the parity errors 
	 */
	fillptr = (uint *)PHYS_TO_K0(PHYS_CHECK_LO);
	i = PD_SIZE / (NUM_WORDS_PER_CACHE_LINE * sizeof(uint));

	while(i--) {
		for (way=0 ; way<=1 ; way++) {
			if (error) {
				msg_printf(VRB|PRCPU,
					"i = %d error = %d\n",i,error);
			}

			/* process exception */
			if (setjmp(cache_fault_buf)) {

				/* get new error error status */
				cur_err = get_cache_err();

		 		/* If cache error exception, but not
				 * ECC_DFIELD, set error flag and log it
		 		 */
				if(!(cur_err & CACHERR_DC_D)) {
					msg_printf(ERR|PRCPU,"Did not get expected cache datafield error exception at 0x%x, CO_CACHE_ERR: 0x%08x\n",
					    (ulong)fillptr, cur_err);
					fail = 1;
					error++;
				}

				/* clear parity for cache line */
				*(fillptr) = pattern;

				/* Force ERET back to next address to clear
				 * exception.
		 		 */
				DoEret();

				/* abort test after error */
				if (fail)
					goto done;
			}

			/* generate parity exception */
			else {

				nofault = cache_fault_buf;

				/* make sure exception is taken on parity
				 * errors
				 */
				SetSR(GetSR() & ~SR_DE);

				/* write pattern to memory - this will create
				 * a correct cache entry
				 */
				*(fillptr) = pattern;

				/* now write bad parity into cache */
				_write_cache_data(CACH_PD,
					(ulong)fillptr|way,
					data, parity_bits);

				/* now read and see if we get exception */
				dummy = *fillptr;

	       			/* if it won't take parity exception in 5 uS,
				 * it ain't gonna
	       			 */
				DELAY(5);

				/* NO CACHE ERROR - PRINT LOG MESSAGE */
				msg_printf(ERR|PRCPU, "Dcache Parity - no exception at Addr: 0x%x\n",
				    (void *)fillptr);

				error++;
				nofault = 0;
				goto done;

			}

		}

		/* next line in primary cache */
		fillptr += NUM_WORDS_PER_CACHE_LINE;
	}

done:
	return error;
}

/*
 * dcache_parity - data cache block mode parity test
 *
 * all memory locations in the cache are set to the same values, so it
 * will not detect addressing errors well, if at all, but should have
 * very good coverage of cache line parity errors
 */
static int
dcache_parity(void)
{
	register uint k,pattern;
	k_machreg_t cur_stat;
	int rc;

	cur_stat = GetSR();

	for (k = 0; k < NUM_PAR_PATS; k++) {
		pattern = parity_pattern[k];

		/* test the DE bit */
		if (rc = dcache_de_bit_test(pattern,cur_stat)) {
			SetSR(cur_stat);
			return(rc);
		}

#ifdef	NEED_REWRITE	/* XXX cannot take exception uncached with cached ide */
		/* test parity */
		if (rc = dcache_parity_test(pattern,fail_parity_bits[k])) {
			SetSR(cur_stat);
			return(rc);
		}
#endif /* NEED_REWRITE */

		/*
		 * restore original diagnostic values for next pass
		 */
		SetSR (cur_stat);
	}

	/*
	 * restore original diagnostic status values in case of error exit
	 */
	SetSR(cur_stat);

	/* no error */
	return 0;
}
