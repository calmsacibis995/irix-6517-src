/*
 * scache.c
 *
 *
 * Copyright 1993, Silicon Graphics, Inc.
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

#ident "$Revision: 1.16 $"

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

static void
scache_error(char *type, void *addr, __psunsigned_t act, __psunsigned_t exp)
{
	act = ip30_ssram_swizzle(act);
	exp = ip30_ssram_swizzle(exp);
	msg_printf(ERR|PRCPU,
		"Secondary cache address error: Address 0x%x,"
		" Actual 0x%x, Expected 0x%x (SRAM=%s)\n",
		type,addr,act,exp,ip30_ssram(addr,act,exp,cpuid()));
}

/*
 * scache - secondary cache SRAM test
 */
u_int
scache(void)
{
	/* Secondary cache size, 0 if PC version */
	extern int _sidcache_size;
	int error = 0;


	if(!_sidcache_size) {
		msg_printf(VRB|PRCPU,
			"*** No Secondary Cache detected, test skipped\n");
		return(0);
	}
		
	msg_printf(VRB|PRCPU, "Secondary cache data and address tests\n");

	FlushAllCaches();
	setstackseg(K1BASE);
	run_uncached();
	
	error = scachedata() || scacheaddr();

	flush_cache();
	setstackseg(K0BASE);
	run_cached();
	FlushAllCaches();

	if (error) {
		sum_error("secondary cache data or address");
	} else {
		okydoky();
	}
	return error;
}

/*
 * scachedata - secondary walking bit test
 *
 */
int
scachedata(void)
{
	register __psunsigned_t *fillptr;
	register uint i, pattern;
	__psunsigned_t *k0_base = (__psunsigned_t *) PHYS_TO_K0(PHYS_CHECK_LO);
	uint fail = 0, odd = 0;
	uint sid_size = (uint) (SID_SIZE / sizeof (__psunsigned_t));
	uint pd_size = (uint) (PD_SIZE / sizeof (__psunsigned_t));

	pattern = 0x1;
	while (pattern != 0) {

		busy (1);
		invalidate_caches();

		/* Fill secondary through primary */
		fillptr = k0_base;
		for (i = sid_size; i > 0; --i) {
			*fillptr++ = pattern;
		}

		/* write final values to secondary and invalidate primary */
		fillptr = k0_base;
		for (i = pd_size; i > 0; --i) {
			pd_HWBINV ((volatile uint *)fillptr++);
		}

		/* Read back and check */
		fillptr = k0_base;
		for (i = sid_size; i > 0; --i) {
			if ((uint)*fillptr != pattern) {
				scache_error("",fillptr,*fillptr,pattern);
				fail = 1;
			}
			fillptr++;
		}

		pattern = ~pattern;		/* invert */
		odd = !odd;
		if (!odd) {			/* every other time... */
			pattern <<= 1;		/* shift bit left */
		}
	}
	busy (0);
	return (fail);
}

/*
 * scacheaddr - secondary address uniqueness test
 *
 * does not provide the bit error coverage of icachedata, but by storing
 * address-in-address and ~address-in-address, should find if each
 * cache location is individually addressable
 */
int
scacheaddr(void)
{
	register __psunsigned_t *fillptr;
	uint i;
	__psunsigned_t *k0_base = (__psunsigned_t *) PHYS_TO_K0(PHYS_CHECK_LO);
	uint fail = 0;
	uint sid_size = (uint) (SID_SIZE / sizeof (__psunsigned_t));
	uint pd_size = (uint) (PD_SIZE / sizeof (__psunsigned_t));

	invalidate_caches();

	/* Fill secondary through primary */

	fillptr = k0_base;
	for (i = sid_size; i > 0; --i) {
		*fillptr = (__psunsigned_t) fillptr;
		fillptr++;
	}

	/* write final values to secondary and invalidate primary */

	fillptr = k0_base;
	for (i = pd_size; i > 0; --i) {
		pd_HWBINV ((volatile uint *)fillptr);
		fillptr++;
	}

	/* Read back and check */

	fillptr = k0_base;
	for (i = sid_size; i > 0; --i) {
		if (*fillptr != (__psunsigned_t) fillptr) {
				scache_error("data",fillptr,*fillptr,
					(__psunsigned_t) fillptr);
			fail = 1;
		}
		fillptr++;
	}

	/* again, with address inverted */

	invalidate_caches();

	/* Fill secondary through primary */

	fillptr = k0_base;
	for (i = sid_size; i > 0; --i) {
		*fillptr = ~(__psunsigned_t)fillptr;
		fillptr++;
	}

	/* write final values to secondary and invalidate primary */

	fillptr = k0_base;
	for (i = pd_size; i > 0; --i) {
		pd_HWBINV ((volatile uint *)fillptr);
		fillptr++;
	}

	/* Read back and check */

	fillptr = k0_base;
	for (i = sid_size; i > 0; --i) {
		if (*fillptr != ~(__psunsigned_t)fillptr) {
			scache_error("address",fillptr,*fillptr,
				~(__psunsigned_t)fillptr);
			fail = 1;
		}
		fillptr++;
	}

	return (fail);
}
