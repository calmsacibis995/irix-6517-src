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

#ident "$Revision: 1.15 $"


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
uint parity_pattern[] = { 0x11111111, 0, 0x10101010, 0xffffffff };
uint fail_parity_bits[] = { 0xf, 0xf, 0x0, 0xf };

/* First level data cache data parity test */
int
dcache_par(void)
{
    int	error = 0;

    msg_printf(VRB, "Data cache data parity test\n");
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
int
icache_par(void)
{
    int	error = 0;
#if IP22
    int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
    int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
    int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
#endif

    msg_printf(VRB, "Instruction cache data parity test\n");

#if IP22
    if (orion || r4700 || r5000) {
	msg_printf(DBG, "skipping icache parity test for orion/r4700/r5000\n");
        okydoky();
	return error;
    }
#endif

    run_uncached();
    invalidate_caches();
    error = icache_parity();
    invalidate_caches();
    run_cached();
    if (error)
	sum_error("instruction cache data parity");
    else
	okydoky();

    return error;
}

#if R4000	    
/*
 * icache_de_bit_test - checks to see if DE bit in Status Reg works
 *                    - also checks real parity errors ie if
 *                      real parity errors occur during this test
 *                      they will appear here
 */

int icache_de_bit_test(uint pattern, k_machreg_t cur_stat) 
{

    register uint i, j;
    register volatile uint *fillptr;
    int error = 0;

    /*
     * words/ipline 
     */
    j = (uint) (PIL_SIZE / sizeof(uint));

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
    i = (uint) (PI_SIZE / sizeof(uint));
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
	    fill_ipline((void *)fillptr);	/* force cache load */
	    fillptr += j;
	  }

	/*
	 * clear error handler  - delay is necessary do work around
	 * pipeline delays in interrupt/exception generation
	 */
	DELAY(2);
	nofault = 0;
      }

    /* no error */
    return 0;
}
#endif

#if R4000
/*
 * icache_parity_test - causes a parity error and makes
 *                      sure that an exception occurs
 *
 */
int icache_parity_test(uint pattern)
{
    register uint i, j;
    register volatile uint *fillptr;
    volatile uint bucket;
    __psunsigned_t cur_err;
    int error = 0;
    uint fail;

    /*
     * words/ipline 
     */
    j = (uint) (PIL_SIZE / sizeof(uint));

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
			   "Did not get expected cache datafield error exception at 0x%08lx, CO_CACHE_ERR: 0x%08x\n",
			   (void *) fillptr, cur_err);
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
	      sd_hitinv((void *)fillptr);
	      bucket = *fillptr;	
	    } else
	      pi_HINV((void *)fillptr);

	    fill_ipline((void *)fillptr);
	    
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
	      sd_hitinv((void *)fillptr);
	      bucket = *fillptr;
	    } else
	      pi_HINV((void *)fillptr);
	    
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
	    fill_ipline((void *)fillptr);	/* force load the cache line */
	    
	    /*
	     * disable force so that errors on writeback are parity, not
	     * secondary ECC
	     */
	    SetSR (SR_FORCE_OFF);
	    
	    /*
	     * write icache data back to secondary. since icache line has
	     * bad parity, this should generate parity exception
	     */
	    write_ipline ((void *)fillptr);	/* force bad parity writeback */
	    
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
		       (void *) fillptr);
	    error++;
	    nofault = 0;
	    break;
	  }
	
	fillptr += j;	/* next line in primary cache */
      }

    return error;
    
}
#elif R10000
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
int
icache_parity_test(void)
{
    register uint i, j;
    register volatile uint *fillptr;
    volatile uint bucket;
    __psunsigned_t cur_err;
    uint fail = 0;
    int error = 0;
    register uint way;

    /*
     * words/ipline 
     */
    j = (uint) (PIL_SIZE / sizeof(uint));

    /*
     * invalidate caches so will force refill
     */
    invalidate_caches();			/* from saio/lib/cache.s */
    flushall_tlb();

    /*
     * force-fill the cache again, but this time we force bad parity, so
     * should see parity errors on every cache line. Error set if NO cache
     * parity errors seen
     */
    fillptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
    i = PI_SIZE / PIL_SIZE;

    while(i--)
      {

	/* check both ways in cache */
	for (way=0 ; way<=1 ; way++) 
	  {

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
		if(!(cur_err & CACHERR_DC_TA))
		  {
		    msg_printf(ERR,
			       "Did not get expected cache tag address error exception at 0x%08lx, CO_CACHE_ERR: 0x%08x\n",
			       (void *) fillptr, cur_err);
		    fail = 1;
		    error++;
		  }
	    		
		/*
		 * force ERET back to next address to clear exception
		 */
		DoEret();

		/*
		 * abort test after error
		 */
		if (fail)
		  goto done;
	      }
	    else
	      {
		nofault = fault_buf;
		
		/* make sure exception is taken on parity errors */
		SetSR(GetSR() & ~SR_DE);
		
		/* set a value */
		*fillptr = 0x0;
		
		/* now write data w/ bad tag parity into cache */
		write_icache_badtag((ulong)fillptr|(ulong)way);

		/* hit invalidate checks tag parity and will cause exception */
		pi_HINV((void*)((ulong)fillptr|(ulong)way));

		/*
		 * if it won't take parity exception in 5 uS, it ain't gonna
		 */
		DELAY(5);
		
		/* NO CACHE ERROR - PRINT LOG MESSAGE */
		msg_printf(ERR,
			   "Icache Parity - no exception at Addr: 0x%x\n",
			   (void *) fillptr);
		error++;
		nofault = 0;
		goto done;
	      }
	    
	  }
	
	fillptr += j;	/* next line in primary cache */
	
      }
    
    /*
     * invalidate caches so will force refill
     */
    invalidate_caches();			/* from saio/lib/cache.s */
    flushall_tlb();

  done:
    return error;
    
}
#endif /* R10000 */

/*
 * icache_parity - instruction cache block mode parity test
 *
 * all memory locations in the cache are set to the same values, so it
 * will not detect addressing errors well, if at all, but should have
 * very good coverage of cache line parity errors
 */
int icache_parity(void)
{
#ifdef R4000
    register uint k, pattern;
#endif
    k_machreg_t cur_stat;
    int rc = 0;

    /*
     * save current status register contents for restoration later
     */
    cur_stat = GetSR();

#ifdef R4000
    for (k = 0; k < NUM_PAR_PATS; k++)
    {

        pattern = parity_pattern[k];        

	/* test the DE bit */
	if (rc = icache_de_bit_test(pattern,cur_stat))
	  break;

	/* test tag address parity */
	rc = icache_parity_test(pattern);

	/*
	 * restore original diagnostic status values
	 */
	SetSR(cur_stat);
    }
#endif

#ifdef R10000
    /* test tag address parity */
    rc = icache_parity_test();
#endif

    /*
     * restore original diagnostic status values in case of error exit
     */
    SetSR(cur_stat);

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
    i = (uint)(PD_SIZE / sizeof(uint));
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
    
    i = (uint) (PD_SIZE / sizeof(uint));
    
    if (setjmp(fault_buf))
      {
	show_fault();
	DoEret();
	SetSR(cur_stat);
	return 1;
      }
    else
      {
	nofault = fault_buf;	/* enable error handler */
	
	/*
	 * enable cache interrupts, but not for parity/ecc
	 */
#if R4000	    
	SetSR(SR_FORCE_OFF | SR_DE);
#elif R10000
	SetSR(GetSR() | SR_FORCE_OFF | SR_DE);
#endif
	
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

    /* no error */
    return 0;

}

/*
 * dcache_parity_test - causes a parity error and makes
 *                      sure that an exception occurs
 *
 */
#if R4000
int dcache_parity_test(uint pattern,uint not_used)
{
    register uint i;
    __psunsigned_t cur_err;
    register volatile uint *fillptr;
    volatile uint dummy;
    uint fail = 0;
    int error = 0;

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
    i =(uint) ( PD_SIZE / (2 * sizeof(uint))) ;
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
			   "Did not get expected cache datafield error exception at 0x%x, CO_CACHE_ERR: 0x%08x\n",
			   fillptr, cur_err);
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
	      sd_hitinv((void *)fillptr);
	    else
	      pd_HINV((void *)fillptr);
	    
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
	    msg_printf(DBG, "testing location 0x%08lx\n", fillptr);
	    nofault = fault_buf;
	    
	    /*
	     * invalidate cache line
	     */
	    if (_sidcache_size)
	      sd_hitinv((void *)fillptr);
	    else
	      pd_HINV((void *)fillptr);
	    
	    /*
	     * enable interrupts for cache parity/ECC while parity force is
	     * enabled
	     */
	    SetSR(SR_FORCE_ON);

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
		       (void *) fillptr);
	    error++;
	    nofault = 0;
	    break;
	  }
	
	fillptr += 2;	/* next dword in primary cache */
    }

    return error;

}
#elif R10000
int dcache_parity_test(uint pattern, uint parity_bits) 
{

    register uint i;
    register uint way;
    register volatile uint *fillptr;
    ulong offset;
    __psunsigned_t cur_err;
    volatile uint dummy;
    uint fail = 0;
    int error = 0;
    uint data[2];

    /*
     * invalidate caches so will force refill
     */
    invalidate_caches();			/* from saio/lib/cache.s */
    flushall_tlb();

    /* init data */
    data[0] = pattern;
    data[1] = 0x0;

#define NUM_WORDS_PER_CACHE_LINE 8

    /* we fill the cache with bad-parity data via the index store data
     * cache-ops. We run in cached space so that later reads catch
     * the parity errors 
     */
    fillptr = (uint *) PHYS_TO_K0(PHYS_CHECK_LO);
    i =(uint) (PD_SIZE / (NUM_WORDS_PER_CACHE_LINE * sizeof(uint))) ;

    while(i--)
      {

	for (way=0 ; way<=1 ; way++) 
	  {

	    if (error) {
	      msg_printf(VRB,"i = %d error = %d\n",i,error);
	    }

	    /* offset needed to select way 0 or way 1 */
	    if (way)
	      offset = PD_SIZE / 2; /* with offset causes a way 1 access */
	    else                    /* after a way 0 access has occurred */
	      offset = 0x0;

	    /* process exception */
	    if (setjmp(fault_buf))
	      {

		cur_err = get_cache_err();	/* get new error status */
	
		/*
		 * if cache error exception, but not ECC_DFIELD, set error
		 * flag and log it
		 */
		if(!(cur_err & CACHERR_DC_D))
		  {
		    msg_printf(ERR,"Did not get expected cache datafield error exception at 0x%x, CO_CACHE_ERR: 0x%08x\n", (ulong)fillptr+(ulong)offset, cur_err);
		    fail = 1;
		    error++;
		  }
		
		/*
		 * clear parity for cache line
		 */
		*(fillptr+offset) = pattern;
	    
		/*
		 * force ERET back to next address to clear exception
		 */
		DoEret();
	    
		/*
		 * abort test after error
		 */
		if (fail)
		  goto done;
	      }
	
	    /* generate parity exception */
	    else {

	      nofault = fault_buf;

	      /* make sure exception is taken on parity errors */
	      SetSR(GetSR() & ~SR_DE);

	      /* write pattern to memory - this will create a correct
		 cache entry */
	      *(fillptr+offset) = pattern;

	      /* now write bad parity into cache */
	      _write_cache_data(CACH_PD, (ulong)(fillptr+offset)|(ulong)way, data, 
				parity_bits);

	      /* now read and see if we get exception */
	      dummy = *(fillptr+offset);	

	      /*
	       * if it won't take parity exception in 5 uS, it ain't gonna
	       */
	      DELAY(5);

	      /* NO CACHE ERROR - PRINT LOG MESSAGE */
	      msg_printf(ERR, "Dcache Parity - no exception at Addr: 0x%x\n",
			 (void *)(fillptr+offset));

	      error++;
	      nofault = 0;
	      goto done;
	      
	    }
	   
	}
	
	fillptr += NUM_WORDS_PER_CACHE_LINE;	/* next line in primary cache */
	
    }
    
  done:
    return error;
}
#endif
     
     
/*
 * dcache_parity - data cache block mode parity test
 *
 * all memory locations in the cache are set to the same values, so it
 * will not detect addressing errors well, if at all, but should have
 * very good coverage of cache line parity errors
 */
int dcache_parity(void)
{
    register uint k,pattern;
    k_machreg_t cur_stat;
    int rc;

    cur_stat = GetSR();

    for (k = 0; k < NUM_PAR_PATS; k++)
    {

	pattern = parity_pattern[k];

	/* test the DE bit */
	if (rc = dcache_de_bit_test(pattern,cur_stat))
	  return(rc);

	/* test parity */
	if (rc = dcache_parity_test(pattern,fail_parity_bits[k]))
	  return(rc);

	/*
	 * restore original diagnostic values for next pass
	 */
	SetSR (cur_stat);
    }

    /*
     * restore original diagnostic status values in case of error exit
     */
    SetSR (cur_stat);

    /* no error */
    return 0;
}
