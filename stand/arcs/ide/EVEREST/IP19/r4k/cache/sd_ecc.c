
/*
 * cpu/scache_ecc.c
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
/*#include <sys/sbd.h>*/
#include <sys/cpu.h>
/*#include <fault.h>*/
/*#include <setjmp.h>*/
#include <ide_msg.h>
#include <libsc.h>
#include <libsk.h>
#include "cache.h"
#include "ip19.h"
#include "prototypes.h"

static jmp_buf fault_buf;

/*
 * number of data patterns to fed through
 */
#define NUM_DATA_PATS	4
#define NUM_XOR_PATS	4
static uint data_pats[] = { 0, 0x55555555, 0xaaaaaaaa, 0xffffffff };
static uint ecc_xor_pats[] = {0x55, 0xaa, 0xff, 0 };

#define SR_FORCE_ON     (SR_CU0 | SR_CU1 | SR_CE | SR_IEC)
#define SR_FORCE_OFF    (SR_CU0 | SR_CU1 | SR_IEC)
#define SR_FORCE_NOERR  (SR_CU0 | SR_CU1 | SR_CE | SR_DE )
#define SR_NOERR        (SR_CU0 | SR_CU1 | SR_DE )

extern void sd_hitinv(uint);
extern uint get_cache_err(void);

int scache_ecc_intr(void);
int scache_ecc_ram(void);

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
scache_ecc()
{
    int error = 0;

    msg_printf(VRB, "Secondary Cache ECC RAM/Interrupt Test\n");

    /* Size and set the cache structure */
    setconfig_info();

    /*run_uncached();*/
    ide_invalidate_caches();
    error = scache_ecc_ram(); 
    /*run_cached();*/
    ide_invalidate_caches();
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
int scache_ecc_ram (void)
{
    register uint *fillptr, r_pattern, w_pattern, readin;
    uint fail, i, j, is_rev1, clear_pat;
    volatile uint dummy, dummy2;

    fail = 0;

    /*
     * start with caches known invalid so that uncached writes go to memory
     */
    ide_invalidate_caches();

    is_rev1 =0;
    clear_pat = 0;
    /*
     * disable ECC exceptions
     */
    SetSR(SR_NOERR);

    msg_printf (VRB, "Initializing Memory\n");

    /*
     * fill an area of phys ram the size of the scache with all 0's
     */
    j = SID_SIZE / sizeof(uint);
    fillptr = (uint *)PHYS_TO_K1(PHYS_CHECK_LO);
    msg_printf (VRB, "j 0x%x  fillptr 0x%x\n", j, fillptr);
    while (j--)
	*fillptr++ = 0;

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

	msg_printf(VRB, "ECC Ram - Writing 0x%x\n", r_pattern);

	/*
	 * run current test patterns for all double words in the scache
	 */
	i = SID_SIZE / (2 * sizeof(uint));	/* # dwords in scache */
	fillptr = (uint *)PHYS_TO_K0(PHYS_CHECK_LO);
	msg_printf (VRB, "i 0x%x  fillptr 0x%x\n", i, fillptr);
	while (i--)
	{
    	    *fillptr = 0;	/* force cache load and modify */
	    SetSR(SR_FORCE_NOERR);	/* set FORCE bit, no exceptions */
	    set_ecc(w_pattern);		/* set selected write ECC value */
	    IndxWBInv_D((uint)fillptr, 1, config_info.dline_size);	/* writeback to secondary */
	    SetSR(SR_NOERR);		/* clear FORCE bit */
	    dummy = *fillptr;		/* reload primary cache line */
	    dummy2 = *(fillptr+1);

	    /*
	     * get ECC for selected dword by calling read_tag, which does
	     * an indexed load tag cache op
	     */
	    set_ecc(0);			/* clear ECC register */
	    IndxLoadTag_SD(PHYS_TO_K0((uint)fillptr));
	    readin = (uint)get_ecc();
	    if (readin != r_pattern)
	    {
		/*
		 * clean out the bad ECC pattern BEFORE calling errlog
		 * or the exception handler goes off into the weeds . . .
		 */
		*fillptr = 0;		/* cache load and modify */
		SetSR(SR_FORCE_NOERR);		/* set FORCE , no exceptions */
		set_ecc(clear_pat);		/* forces calculated ecc */
		IndxWBInv_D((uint)fillptr, 1, config_info.dline_size);	/* writeback to secondary */
		SetSR(SR_NOERR);		/* clear FORCE bit */

		msg_printf(ERR,
		   "Bad ECC at scache dword 0x%x - expected 0x%x, got 0x%x\n",
		    K0_TO_PHYS((uint)fillptr), r_pattern, readin);
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
    ide_invalidate_caches();

    /*
     * disable interrupts
     */
    SetSR (SR_FORCE_OFF & ~SR_IEC);

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
    uint fail, cur_err, datacount, ecc_bshift;
    volatile uint dummy;

    fail = 0;

    /*
     * make sure we start with the caches in a known state
     */
    ide_invalidate_caches();

    /*
     * start at the base of the scratch address area for each pattern
     */
    fillptr = (uint *)PHYS_TO_K0(PHYS_CHECK_LO);

    /*
     * use each one of the data test patterns
     */
    for (datacount = 0; datacount < NUM_DATA_PATS; datacount++)
    {
	d_pattern = data_pats[datacount];

	msg_printf(DBG, "Testing pattern 0x%x\n", d_pattern);

	/*
	 * test each bit of scache ecc separately
	 */
	for (ecc_bshift = 0; ecc_bshift < 8; ecc_bshift++)
	{
	    u_int old_cache_err;

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
	     * set ECC force pattern to XOR in a single bit per pass
	     * this will walk a single ECC bit error through the
	     * secondary cache line ECC
	     */
	    set_ecc(~(1 << ecc_bshift));

	    /*
	     * force the dirty data in the primary dcache back into the
	     * secondary cache. Since the force bit is set in the status
	     * register, the writeback will XOR the value in the ECC
	     * register with the calculated ECC value
	     */
	    if (setjmp(fault_buf))
	    {
		cur_err = get_cache_err();	/* get error status */

		/*
		 * if cache error exception, but not ECC_DFIELD,
		 * set error flag and log it
		 */
		if(!(cur_err & CACHERR_ED))
		{
		    msg_printf(ERR,
			"Did not get expected cache datafield error exception at 0x%08x, CO_CACHE_ERR: 0x%08x\n",
			fillptr, cur_err);
		    fail = 1;
		}

		/*
		 * write error address again with force disable to
		 * clear cache parity error
		 */
		SetSR(GetSR() & ~SR_CE);

		/*
		 * invalidate cache line
		 */
		sd_hitinv((uint)fillptr);

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
		nofault = fault_buf;

		/*
		 * load valid cache line and mark as dirty in
		 * primary dcache
		 */
		*fillptr = d_pattern;

		/*
		 * enable interrupts for cache parity/ECC while
		 * parity force is enabled
		 */
		SetSR (SR_FORCE_ON);
		set_cause(0);

		/*
		 * writeback primary dcache to secondary, which
		 * should generate ECC errors
		 */
		HitWB_D((uint)fillptr, 1, config_info.dline_size);

		/*
		 * clear the force bit now
		 */
		SetSR (SR_FORCE_OFF);

		/*
		 * invalidate the primary cache line
		 */
		HitInv_D((uint)fillptr, 1, config_info.dline_size);

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
	    	msg_printf(ERR, 
	    	    "Scache ECC - no exception for pattern 0x%x, ECC bit %d\n",
		    d_pattern, ecc_bshift);
		fail = 1;
		nofault = 0;
		break;
	    }

	    /*
	     * disable interrupts
	     */
	    SetSR (SR_FORCE_OFF & ~SR_IEC);

	}				/* END of ECC bitwalk loop */
    }					/* END of data pattern loop */

    return fail;
}


