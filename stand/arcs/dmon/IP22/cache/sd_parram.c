#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
/*
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include "cache.h"
*/


#define	NUM_XOR_PATS	4
#define PHYS_RAMBASE	0x08000000
static uint ecc_xor_pats[] = {0x55, 0xaa, 0xff, 0 };
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
    uint fail, i, j, cur_err, is_rev1, clear_pat;
    u_int scache_size, dcache_linesize;
    volatile uint dummy, dummy2;
    u_int stat_reg;

    fail = 0;

    stat_reg = GetSR();
    /*
     * start with caches known invalid so that uncached writes go to memory
     */
    invalidate_caches();

    /*
     * Get the data cache line size
     */
    dcache_linesize = dcache_ln_size();

    /*
     * checks to see if this is a revision 1.x chip that has the uncached
     * writeback bug - if rev 1.x, must invert the ecc's written out
     */
/*
    if ((get_prid() & 0xFF) == 1)
    {
	is_rev1 = 1;
	clear_pat = ~0;
    }
    else
    {
*/
	is_rev1 = 0;
	clear_pat = 0;

    /*
     * disable ECC exceptions
     */
    SetSR(SR_DE);

    puts("\r\nInitializing Memory");

    /*
     * fill an area of phys ram the size of the scache with all 0's
     */
    scache_size = size_2nd_cache();
    j = scache_size/ sizeof(uint);
    
    fillptr = (uint *)PHYS_TO_K1(PHYS_RAMBASE +0x00100000);
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

	puts("\r\nECC Ram - Writing ");
	puthex( r_pattern);

	/*
	 * run current test patterns for all double words in the scache
	 */
	i = scache_size/ (2 * sizeof(uint));	/* # dwords in scache */
	fillptr = (uint *)PHYS_TO_K0(PHYS_RAMBASE + 0x00100000);
	while (i--)
	{
    	    *fillptr = 0;		/* force cache load and modify */
	    SetSR(SR_CE | SR_DE);	/* set FORCE bit, no exceptions */
	    SetEcc(w_pattern);		/* set selected write ECC value */

	    /* writeback to secondary */
	    IndxWBInv_D((uint)fillptr, 1, dcache_ln_size);

	    SetSR(SR_DE);		/* clear FORCE bit */
	    dummy = *fillptr;		/* reload primary cache line */
	    dummy2 = *(fillptr+1);

	    /*
	     * get ECC for selected dword by reading the tags, which does
	     * an indexed load tag cache op
	     */
	    SetEcc(0);			/* clear ECC register */
	    IndxLoadTag_SD( PHYS_TO_K0((uint)fillptr) );
	    readin = GetEcc();
	    if (readin != r_pattern)
	    {
		/*
		 * clean out the bad ECC pattern BEFORE calling errlog
		 * or the exception handler goes off into the weeds . . .
		 */
		*fillptr = 0;			/* cache load and modify */
		SetSR(SR_CE | SR_DE);		/* set FORCE , no exceptions */
		SetEcc(clear_pat);		/* forces calculated ecc */
	        /* writeback to secondary */
	        IndxWBInv_D((uint)fillptr, 1, dcache_ln_size);
		SetSR(SR_DE);			/* clear FORCE bit */

		puts("\r\nBad ECC at scache dword 0x");
		puthex(K0_TO_PHYS( fillptr) );
		puts("\r\nexpected 0x");
		puthex(r_pattern);
		puts("\r\nactual 0x");
		puthex(readin);
		puts("\r\nProbable failure at -> u10");
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
    SetEcc(0);
    invalidate_caches();

    /*
     * disable interrupts
     */
    SetSR( stat_reg);
    puts("done\r\n");

    return (fail);
}
