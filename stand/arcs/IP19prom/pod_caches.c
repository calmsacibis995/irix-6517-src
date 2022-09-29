/*
 * IP19prom/pod_caches.c
 *
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/types.h>
#include "pod.h"
#include "pod_failure.h"

static char *dcache_str = "Primary D-cache";
static char *icache_str = "Primary I-cache";
static char *scache_str = "Secondary I+D cache";

/* current revs of the R4K write to caches even if using k1 space if
 * that addr is valid in caches; avoid by invalidating caches.  This
 * should be defined in IP19prom/Makedefs.
 */
#define R4K_UNCACHED_BUG

#define TBD	EVDIAG_TBD

#define PATTERN		0xa5a5a5a5

/* macro to generate a wildly-varying yet unique pattern for cache-tests */
#define PAT_FUNC(x,addr)	((5 * x) + addr + 1)
#define NORM_PAT	1
#define INV_PAT		2

/* The tags of each type of cache (PI, PD, & 2ndary) contain bits that
 * can't be written and read as RAM tests commonly do (the tag_rams()
 * diagnostic performs this test).  */
#define P_D_IGNMASK	0x0000003f
#define P_I_IGNMASK	0x000000ff
#define S_IGNMASK	0x0000007f

#define MASK 	0xffffffff

/* addresses for cacheops must be word-aligned. */
#define WD_ALIGN(addr)	(addr & ~0x03)

#define _1_K	0x00000400
#define _16_K	0x00004000

/* Formerly enumbrated types.  Yeah, I know...  shame on me. */

#define	K0SEG		0
#define K1SEG		1
#define K2SEG		2

#define	INVALIDL	0
#define	CLEAN_EXCL	1
#define	DIRTY_EXCL	2
#define	DIRTY_EXCL_NWB	3

#define BIT_TRUE	0
#define BIT_INVERT	1

typedef struct memstr {
  uint *lomem;	/* base address */
  uint *himem;	/* last address (inclusive) */
  uint mask;
}memstr_t;

/* *********** prototypes for the cache diagnostic routines *****************/

/* the prom cache diags:
 * a) test the primary I, D and secondary tags
 * b) perform stuck-bit and address-uniqueness tests separately for
 *    the primary data and secondary caches, and
 * c) perform a variety of tests on the primary I cache,
 *    relying heavily upon the R4K cache-fill operation.
 */
extern uint tag_rams(int);
extern int pddata(void);
extern int sddata(void);
extern int icachedata(void);


/* the ram tests of the caches perform walking address, read-write,
 * and address-pattern tests for both primary data and secondary
 * (I and D combined) caches */
extern int bwalking_caddr(struct memstr *, int, int);
extern uint addr_pattern(memstr_t *, int, int, int);
extern uint cread_wr(memstr_t *, uint, int);

extern int set_tag_parms(int);
extern int mem_init(uint, uint, uint);
extern int flush2ndline(uint);
extern uint read_word(int, uint);
extern int write_word(int, uint, uint);
extern int set_tag(int, int, uint, int);
extern int flush_pdc(uint, uint);
extern int fill_ipline(uint);
extern int write_ipline(uint);
extern int icache_fill_writeback(uint *, uint);
extern int inval_ctags(int);

/* from cache.s */
extern int _read_tag(int, uint, tag_regs_t *);
extern int _write_tag(int, uint, tag_regs_t *);
extern int pd_hwbinv(uint);

extern int sd_hwb(uint);

#if 0
/* globals set by set_tag_parms */
extern struct reg_desc s_taglo_desc[], p_taglo_desc[];
#endif 

int pod_check_dcache2()
{
	return 0;
}

int pod_check_icache()
{
	return 0;
}

int pod_check_scache2()
{
	return 0;
}

#if 0
init_caches()
{
	int isize, dsize, ssize;
	int iline, dline, sline;

	dsize = get_dcachesize();
	isize = get_icachesize();
	ssize = read_scachesize();	/* read scachesize from EAROM */
	iline = get_icacheline();
	dline = get_dcacheline();
	sline = get_scacheline();

	/*
	 * set all tags of each cache to be invalid.
	 */
	pon_invalidate_icache();
	pon_invalidate_dcache();
	pon_invalidate_scache();

	/*
	 * now go through each cache, and execute a load (or 'fill'
	 * for the icache), to cause the data for that cache line
	 * to be initialized with consistent ECC/parity.
	 */
	init_load_scache(ssize);
	init_load_dcache(dsize);
	init_load_icache(isize);

	/*
	 * Now go back through and invalidate all of the caches.
	 */
	pon_invalidate_icache();
	pon_invalidate_dcache();
	pon_invalidate_scache();
}
#endif

void invalidate_caches() {
	pon_invalidate_icache();
	pon_invalidate_dcache();
	pon_invalidate_scache();
}


/* Check for stuck bits by writing two sequential tags with different
 * (inverse) patterns and reading them back, then write them again
 * with the patterns reversed, and continue throughout cache. */
static uint tag_rams(cache)
	int cache;	/* CACH_PD, CACH_PI, or CACH_S{D,I} */
{
	tag_regs_t Tag0In, Tag1In, Tag0Out, Tag1Out;
	uint start_addr = POD_STACKADDR;
	uint hi_addr, linesize, ignmask;
	register uint caddr;
	register uint loopincr;
	register uint pattern;
	uint badcaddr = 0;
	int i, numtags = 0;

	switch(cache) {
	case CACH_PD:
		linesize = get_dcacheline();
		hi_addr = POD_STACKADDR + get_dcachesize();
						/* 1 PD-cache sized chunk */
		ignmask = P_D_IGNMASK;
		break;

	case CACH_PI:
		linesize = get_icacheline();
		hi_addr = POD_STACKADDR + get_icachesize();
						/* 1 PI-cache sized chunk */
		ignmask = P_I_IGNMASK;
		break;

	case CACH_SD:
	case CACH_SI:
		linesize = get_scacheline();
		hi_addr = POD_STACKADDR + read_scachesize();
						/* 1 2nd-cache sized chunk */
		ignmask = S_IGNMASK;
		break;

	default:
		return TBD;	/* Parameter error */
	}

	loopincr = linesize * 2;	/* process two tags at a time */

	pattern = PATTERN;

	for (caddr = start_addr; caddr < hi_addr; caddr += loopincr) {
	    for (i = 0; i < 2; i++) {
		Tag0In.tag_lo = pattern;
		Tag1In.tag_lo = ~pattern;
		_write_tag(cache, caddr, &Tag0In);
		_write_tag(cache, (caddr+linesize), &Tag1In);

		_read_tag(cache, caddr, &Tag0Out);
		_read_tag(cache, (caddr+linesize), &Tag1Out);

		if (Tag0Out.tag_lo&(~ignmask) != Tag0In.tag_lo&(~ignmask))
			badcaddr = caddr;
		else if (Tag1Out.tag_lo&(~ignmask) != Tag1In.tag_lo&(~ignmask))
			badcaddr = (caddr+linesize);
		if (badcaddr)
			return TBD;

		pattern = ~pattern;
	    }
	    numtags += 2;
	}

	return 0;
} /* tag_rams */


/*
 * pddata -- Tests primary data cache data-rams for addressing and stuck-at 
 *	     faults.  In order to prevent any memory<-->secondary-cache
 *	     interaction (fills or writebacks), all PD tags are dummied-up
 *	     (i.e. the tags are constructed but the data rams are not
 *	     filled).  All are set to dirty exclusive, with line 0
 *	     vaddr beginning at POD_STACKADDR and rising contiguously, so
 *	     all accesses within this range will hit the PD cache,
 *	     thereby isolating it from the rest of the system.
 *	     Since the data in the cache is often bogus, ECC errors may
 *	     occur; disable the exception by setting the DE bit in the SR.
 */
static
pddata()
{
	struct memstr addr;
	int fail = 0;
	register uint lpaddr, caddr, paddr;
	uint lophys, topcaddr;
	uint dsize, dline;

	dsize = get_dcachesize();
	dline = get_dcacheline();

	addr.lomem = (uint *)POD_STACKADDR;
	addr.himem = (uint *)(POD_STACKADDR + dsize);
	addr.mask  = MASK;

	lophys = K0_TO_PHYS(POD_STACKADDR);
	caddr = POD_STACKADDR;
	topcaddr = caddr+dsize;

	/* set all PD tags DE and contiguous addrs beginning at POD_STACKADDR */
	for (lpaddr=caddr,paddr=lophys; lpaddr < topcaddr; lpaddr += dline) {
		set_tag(CACH_PD, DIRTY_EXCL, paddr,0);
		paddr += dline;
	}
	/* the tests should now always hit the primary dcache */

	/* Walking address-lines by bytes */
	if (bwalking_caddr(&addr, 1, CACH_PD)) {
		return TBD;	
	}

	/* Test for stuck ram-bits: Write & read 0's & 1's in wd incrs */
	if (cread_wr(&addr, 1, CACH_PD)) {
		return TBD;	
	}

	/* check for unique-addressing of entire cache by writing 
	 * a pdcache chunk of address-patterns and then verifying
	 * it from beginning to end. */
	if (addr_pattern(&addr, 1, BIT_TRUE, CACH_PD)) {
		return TBD;
	}

	/* repeating the addr_pattern test using the ones-complement
	 * of the address as the pattern checks for stuck-bits:
	 * together the two tests tested all bits set and cleared. */
	if (addr_pattern(&addr, 1, BIT_INVERT, CACH_PD)) {
		return TBD;	
	}
} /* pddata */

		
/*
 * sddata -- Tests secondary I&D cache data-rams for addressing and stuck-at 
 *	     faults.  In order to prevent any memory<-->2ndary interaction, 
 *	     all tags are dummied-up (i.e. the data rams are not filled):
 *	     PD lines are set to invalid, 2ndary lines to dirty exclusive.
 *	     The tags' vaddrs begin at POD_STACKADDR and rise contiguously.
 *	     This ensures 2ndary hits for all accesses in the range of
 *	     POD_STACKADDR to POD_STACKADDR + scachesize - 1, effectively
 *	     isolating the cache.  sddata must not be run
 *	     until pddata() has verified the integrity of the primary data
 *	     cache: sddata() attributes any errors to the secondary,
 *	     assuming the PD cache to be operating correctly.  Since the
 *	     data in the caches is bogus, ECC errors may occur, but exceptions
 *	     are suppressed by setting the DE bit in the SR.
 */
static
sddata()
{
	struct memstr addr;
	int fail = 0;
	register uint lpaddr, caddr, paddr;
	uint lophys;
	uint topsdcaddr, toppdcaddr;	/* POD_STACKADDR + <cachesize> */
	uint ssize, dsize, sline, dline;

	ssize = read_scachesize();
	dsize = get_dcachesize();
	sline = get_scacheline();
	dline = get_dcacheline();

	addr.lomem = (uint *)POD_STACKADDR;
	addr.himem = (uint *)(POD_STACKADDR + ssize);
	addr.mask  = MASK;

	lophys = K0_TO_PHYS(POD_STACKADDR);
	caddr = POD_STACKADDR;
	topsdcaddr = caddr + ssize;
	toppdcaddr = caddr + dsize;

	/* 
	 * for the 2nd-level cache tests, set all primary dtags to 
	 * invalid--the initial writes will miss without writebacks
	 * and the lines become DE; after a pd-sized chunk of writes
	 * all misses will cause writebacks to the desired part of
	 * 2ndary cache (which is exactly what we want).
	 */
	for (lpaddr=caddr, paddr=lophys; lpaddr<toppdcaddr; lpaddr += dline) {
		set_tag(CACH_PD, INVALIDL, paddr,0);
		paddr += dline;
	}

	/* 
	 * set all 2ndary tags to DE with contiguous, ascending addresses,
	 * beginning at POD_STACKADDR.  All further accesses by the tests will
	 * then hit the secondary, removing the complexity of 
	 * memory--scache interaction.
	 */
	for (lpaddr=caddr,paddr=lophys; lpaddr < topsdcaddr; lpaddr += sline) {
		set_tag(CACH_SD, DIRTY_EXCL, paddr,0);
		paddr += sline;
	}

	/* Walking address-lines in byte-increments */
	if (bwalking_caddr(&addr, 1, CACH_SD)) {
		return TBD;
	}

	/* verifying writes of all 1's and 0's in the scache would be
	 * nice, but running addr_pattern with both address and ~address
	 * as patterns will do nearly as well, and simultaneously verify
	 * that the entire cache is being used (i.e. unique addresses).
	 * For now, nuke cread_wr() for the 2ndary cache. */
#ifdef TOO_SLOW
	/* Test for stuck ram-bits: Write & read 0's & 1's in wd incrs */
	if (cread_wr(&addr, 1, CACH_SD)) {
		return TBD;
	}
#endif /* TOO_SLOW */

	/* verifies unique-addressing and--when subsequently run with
	 * ~address as the pattern--checks that each data-bit can be
	 * set and cleared */
	if (addr_pattern(&addr, 1, BIT_TRUE, CACH_SD)) {
		 return TBD;
	}

	if (addr_pattern(&addr, 1, BIT_INVERT, CACH_SD)) {
		 return TBD;
	}

	return(0);

} /* sddata */


/* At this point we have verified that primary data and 
 * secondary caches work correctly, as well as the 
 * interactions between them (fills and writebacks).  Now
 * see if writebacks and fills between memory and 2ndary
 * work.  Since we are testing the memory and cache as rams
 * elsewhere, just write and verify one word per 2ndary line
 * to speed up these power-on tests. */
static
c_to_mem()
{
	register uint lo_caddr, lo_ucaddr, lo_paddr;
	register uint caddr, paddr;
	register volatile uint ucaddr;
	uint hi_sdcaddr, hi_pdcaddr;	/* POD_STACKADDR + <cachesize> */
	uint hi_oneup;			/* POD_STACKADDR + <2*ssize> */
	uint expval;
	volatile uint gotval;	/* volatile since dummy-read for flush */
	tag_regs_t ptag, stag;
	uint ssize, dsize, sline, dline;

	lo_caddr = POD_STACKADDR;
	lo_ucaddr = K0_TO_K1(POD_STACKADDR);
	lo_paddr = K0_TO_PHYS(POD_STACKADDR);
	hi_sdcaddr = lo_caddr+ssize;
	hi_pdcaddr = lo_caddr+dsize;

	dsize = get_dcachesize();
	ssize = read_scachesize();
	sline = get_scacheline();
	dline = get_dcacheline();

	/* 
	 * Start with writebacks, because we've already verified that
	 * the 2ndary can be set without relying on memory: now see
	 * if it can be written-back.  Then see if the 2ndary fills
	 * with correct memory values.
	 * Set all primary dtags to invalid--the initial writes will
	 * miss without writebacks and the lines become DE; after a
	 * pd-sized chunk of writes, all misses will cause writebacks
	 * to the desired part of 2ndary cache (which is exactly what
	 * we want).
	 */
	for (caddr=lo_caddr,paddr=lo_paddr;caddr<hi_pdcaddr;caddr += dline) {
		set_tag(CACH_PD, INVALIDL, paddr,0);
		paddr += dline;
	}

	/* 
	 * set all 2ndary tags to DE with contiguous, ascending addresses,
	 * beginning at POD_STACKADDR.  All the writes in this 1meg section
	 * will then hit the 2ndary; we'll then read the next 1meg to force
	 * them out, and check (via K1seg) whether they got there ok
	 */
	for (caddr=lo_caddr,paddr=lo_paddr; caddr < hi_sdcaddr; caddr+=sline) {
		set_tag(CACH_SD, DIRTY_EXCL, paddr,0);
		paddr += sline;
	}

	/* now fill 0th word per line in 2ndary with K0address-pattern */
	for (caddr = lo_caddr; caddr < hi_sdcaddr; caddr += sline) {
		*(uint *)caddr = caddr;
	}

	/* and force it to write-back by reading a 1 meg above dirty stuff.
	 * Note that the read will be optimized-out if gotval is not
	 * declared volatile!
	 */
	caddr = hi_sdcaddr;
	hi_oneup = hi_sdcaddr + ssize;
	for (; caddr < hi_oneup; caddr += sline) {
		gotval = *(uint *)caddr;
	}

	invalidate_caches();	/* no misleading results--especially if
				 * 1.2 R4K */

	/* verify it, and write ~k1addr in prep for fill test */
	expval = lo_caddr;	/* we wrote the CACHED-addr pattern */
	for (ucaddr=lo_ucaddr;ucaddr<(lo_ucaddr+ssize);ucaddr+=sline) {
		gotval = *(uint *)ucaddr;
		if (gotval != expval)	 {
			/* 2ndary-to-mem wb error: K1addr 0x%x\n" ucaddr*/
			/* Wrote 0x%x to 2ndary, read 0x%x, expval, gotval); */
			return TBD;
		}
		*(uint *)ucaddr = ~ucaddr;
		expval += sline;
	}


	/* now test 2ndary-fills from memory.  We used ~k1addr to ensure
	 * that neither cache or mem values used by wb-test would match */
	
	/* read via k0 and verify it */
	expval = lo_ucaddr;	/* we wrote the UNCACHED addr pattern */
	for (caddr = lo_caddr; caddr < hi_sdcaddr; caddr += sline) {
		if ((gotval = *(uint *)caddr) != ~expval)	 {
			/* "\n\tMem-to-2ndary fill error: K1addr 0x%x\n",
				K0_TO_K1(caddr) */
			/* "\t\tWrote 0x%x to 0x%x, read 0x%x from 0x%x\n",
				~expval, expval, gotval, caddr */
			return TBD;
		}
		expval += sline;
	}

	return(0);


} /* c_to_mem */ 


/*
 * icachedata - block mode read/write test
 *
 * uses a wildly-varying but unique pattern to test the data-rams of
 * the icache, then repeats the algorithm with its inverse.  This
 * should therefore detect stuck bits plus addressing errors quite well.
 */
static
icachedata()
{
    register uint *fillptr, i, j, pattern;
    uint caddr = POD_STACKADDR;
    uint ucaddr = K0_TO_K1(caddr);
    uint paddr = K0_TO_PHYS(caddr);
    int fail = 0;
    uint isize;

    isize = get_icachesize();

    /* fill physaddr with pattern, icache with ~pattern, and try writeback */
    pattern = 1;
    invalidate_caches();
    fillptr = (uint *) ucaddr;
    i = isize / sizeof(uint);
    while (i--) {
	pattern = PAT_FUNC(pattern, (int)fillptr);
	*fillptr++ = pattern;
    }
    if (fail = icache_fill_writeback ((uint *)paddr, INV_PAT )) {
	    /* clean up before we leave */
	    zero_icache();
	    invalidate_caches();
	    return fail;	/* fail comes from icache_fill_writeback */
    }

    /* Now do it again with everything reversed: fill physaddr 
     * with ~pattern, icache with ~pattern, and try writeback */
    pattern = 1;
    invalidate_caches();
    fillptr = (uint *) ucaddr;
    i = isize / sizeof(uint);
    while (i--) {
	pattern = PAT_FUNC(pattern, (int)fillptr);
	*fillptr++ = ~pattern;
    }
    fail = icache_fill_writeback ((uint *)paddr, NORM_PAT );

    /* clean up before we leave */
    zero_icache();
    invalidate_caches();

    return fail;	/* return value comes from icache_fill_writeback */

} /* icachedata */


/* ++++++++++++++++++++ begin cache-testing primitives ++++++++++++++++++++++ */

/*
 * fills icache with data whose PHYSICAL address is given, sets the
 * memory at the physical address to fillpat, then attempts to write
 * back the correct information from the icache into the physical address
 *
 * returns 0 for success, nonzero if errors: 1 if pd_hwbinv didn't 
 * invalidate pdcache tags, 2 if refetch into primary shows that the
 * modified value in pdcache didn't get written to secondary, and
 * 3 if the icache fill and flush result--when fetched into the pdcache
 * for confirmation--isn't correct.
 */
static int
icache_fill_writeback(uint *physaddr, uint fillpat)
{
    register uint *fillptr, i, j;
    register uint readpat, pattern, exppat;
    volatile uint dummy;
    struct tag_regs tags;
    uint isize, iline, dline, dsize;

    isize = get_icachesize();
    iline = get_icacheline();
    dsize = get_dcachesize();
    dline = get_dcacheline();
	
    /* words/ipline will be used a lot in this diag, so set it once */
    j = iline / sizeof(uint);

    /* clear out the current cache contents */
    invalidate_caches();

    /* At this point:  mem = k1-pattern, pd = pi = sc = invalid. */

    /* Do a cached-write: this will fill secondary cache from memory 
     * and change primary dcache to new pattern. */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = isize / sizeof(uint);
    pattern = 1;
    while(i--) {
	pattern = PAT_FUNC(pattern, (int)fillptr);
	*fillptr++ = (fillpat==INV_PAT ? ~pattern : pattern);
    }
    /* mem = sc = k1-pattern, pd = new-pattern, pi = invalid. */

    /* fill icache from secondary */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = isize / iline;
    while(i--) {
	fill_ipline((uint)fillptr);
	fillptr += j;
    }
    /* mem = sc = pi = k1pat, pd = newpat. */

    /* flush modified pattern in dcache to secondary.  Now 2ndary value
     * is different from icache value.  Use the invalidate form of
     * the writeback to ensure that a subsequent read (confirming that
     * the secondary values actually changed) will miss the pdcache.
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = isize / iline;
    while(i--) {
	pd_hwbinv((uint)fillptr);
	fillptr += j;
    }

    /* (mem = pi = k1pat, sc = newpat, pd = invalid).

    /* fetch a random pd tag to prove that the pd-lines are now invalid.  */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    fillptr += ((dline/sizeof(int)) * 5);	/* get tag from dcache line 5 */
    _read_tag(CACH_PD, (uint)fillptr, &tags);
    if (tags.tag_lo & PSTATEMASK) {
	/* printf("ifwb: pd_hwbinv didn't invalidate tag:\n\tp-taglo %R\n",
		tags.tag_lo, p_taglo_desc); */
	return TBD;
    }

    /* 
     * fetch back into primary-d to ensure that the 2ndary contents
     * was actually modified by the pd flush (pd_hwbinv) above.
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = dsize / sizeof(uint);
    pattern = 1;
    while(i--) {
	pattern = PAT_FUNC(pattern, (int)fillptr);
	exppat = (fillpat==INV_PAT ? ~pattern : pattern);
	readpat = *fillptr++;
	if (readpat != exppat) {
		return TBD;
	}
    }
    /* (mem = pi = k1pat, sc = pd = newpat). */

    /* 
     * now do the pd_hwbinv again (icache won't write-back 
     * unless p-dcache is invalid).
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = isize / iline;
    while(i--) {
	pd_hwbinv((uint)fillptr);
	fillptr += j;
    }
    /* (mem = pi = k1pat, sc = newpat, pd = invalid). */

    /*
     * flush the contents of the p-icache out to the secondary.  Note
     * that the scache line must be valid and map to the addresses in
     * the icache for the pi_flush to work.
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = isize / iline;
    while (i--) {
	write_ipline ((uint)fillptr);
	fillptr += j;
    }
    /* (mem = pi = sc = k1pat, pd = invalid). */

    /* 
     * read secondary into primary-d: should == value that calling 
     * routine originally initialized memory to via k1seg write (k1pat).
     * Note that the pattern-generator must use K1 (not K0) adrs.
     */
    fillptr = (uint *) PHYS_TO_K0(physaddr);
    i = dsize / sizeof(uint);
    pattern = 1;
    while(i--) {
	pattern = PAT_FUNC(pattern, K0_TO_K1((int)fillptr));
	/* if we were called with INV_PAT, expect NORM_PAT (i.e. k1pat) */
	exppat = (fillpat==INV_PAT ? pattern : ~pattern);
	readpat = *fillptr++;
	if (readpat != exppat) {
		/* printf("\n    ifwb: final pd-refill of icvals failed:\n\t");
		printf("addr 0x%x exp 0x%x read 0x%x\n",
			(int)fillptr, exppat, readpat); */
		return TBD;
	}
    }
    /* (mem = pi = sc = pd = k1pat). */

    return 0;

} /* icache_fill_writeback */


static
inval_ctags(int cache)
{
	switch(cache) {
	case CACH_PI:
		pon_invalidate_icache();
		return 0;
	case CACH_PD:
		pon_invalidate_dcache();
		return 0;
	case CACH_SI:
	case CACH_SD:
		pon_invalidate_scache();
		return 0;
	default:
		/*
		printf("inval_ctags: invalid cache parameter (%d)\n",cache);
		*/
		return TBD; /* parameter error */
	}
}



static
mem_init(lo_addr, hi_addr, pattern)
uint lo_addr, hi_addr;
uint pattern;
{
	register uint *firstp, *lastp, *ptr;

	firstp = (uint *)PHYS_TO_K1(lo_addr);
	lastp = (uint *)PHYS_TO_K1(hi_addr);

	for (ptr = firstp; ptr <= lastp; ptr++) {
		*ptr = pattern;
		pattern = ~pattern;
	}

} /* mem_init */


#define STAG_LO_MASK	0xfffe0000	/* paddr 31..17 */
#define PTAG_LO_MASK	0xfffff000	/* paddr 31..12 */

/* bits 14..12 of virtual addr become the vindex field in 
 * s_taglo register (bits 9..7)
 */
#define SVINDEX_BITS	0x00007000
#define SVINDEX_ROLL	5
#define VADDR_TO_VIDX(vaddr)	((vaddr & SVINDEX_BITS) >> SVINDEX_ROLL)

/* set_tag either reads a tag from 'phys_addr' of 'which' cache or
 * synthesizes the relevant fields (depending on 'rd_n_set'), sets the
 * state to 'state', sets the vindex field to map phys_addr == virtual addr
 * to avoid VCE's (if it is an scache tag), then writes it back to the cache.
 * As an optimization, if called to invalidate a tag it just zeroes, writes,
 * then returns.
 */
static
set_tag(which, state, phys_addr, rd_n_set)
  int which;		/* CACHE_PD, CACHE_PI, or CACH_SD */
  int state;		/* INVALIDL, CLEAN_EXCL, DIRTY_EXCL, DIRTY_EXCL_NWB */
  uint phys_addr;	/* < MAX_PHYS_MEM */
  int rd_n_set;		/* 0 read and modify; else create tag from scratch */
{
	struct tag_regs tags;
	uint caddr = PHYS_TO_K0(phys_addr);
	uint tlo_val;
	uint value;

	tags.tag_lo = 0;
	tags.tag_hi = 0;

	if (state == INVALIDL) {	/* frequently used and simple case */
		_write_tag(which, caddr, &tags);
		return 0;
	}

	if (rd_n_set) {	/* grab current tag then modify specified fields */
		_read_tag(which, caddr, &tags);
	} else {
		/* first set the addr field */
		if (which == CACH_PD || which == CACH_PI) {
			tags.tag_lo = ((phys_addr&PTAG_LO_MASK) >> PADDR_SHIFT);
	
		} else {	/* 2ndary I+D tag */
			tags.tag_lo = ((phys_addr&STAG_LO_MASK) >> SADDR_SHIFT);
			/* vindex page-color is set to match paddr bits
			 * 14..12 to avoid VCE exceptions
			 */
			tags.tag_lo &= ~SVINDEXMASK;
			tags.tag_lo |= VADDR_TO_VIDX(phys_addr);
		}
	}

	/* clear old state bits */
	if (which == CACH_SD)
		tags.tag_lo &= ~SSTATEMASK;
	else
		tags.tag_lo &= ~PSTATEMASK;

	switch(state) {
		case INVALIDL:
			break;	/* above clearing did the trick */
		case CLEAN_EXCL:
			if (which == CACH_SD) {
				tags.tag_lo |= SCLEANEXCL;
			}
			else {
				/* PCLEANEXCL sets V bit in P I cache */
				tags.tag_lo |= PCLEANEXCL;
			}
			break;
		case DIRTY_EXCL:
		case DIRTY_EXCL_NWB:
			if (which == CACH_PI) {
				/* printf("set_tag: PI & DE invalid combo\n");*/
				return TBD; /* parameter error */
			}
			if (which == CACH_SD)
				tags.tag_lo |= SDIRTYEXCL;
			else
				tags.tag_lo |= PDIRTYEXCL;
			break;
		default:
			/*printf("set_tag: invalid cache-spec (%d)\n",which);*/
			return TBD; /* parameter error */
	}
	_write_tag(which, caddr, &tags);

	/* if primary data cache and need dirty exclusive state, we must
	 * set the writeback bit.  Note that this routine expects to be
	 * invoked with the DE bit set, so even if the tag is dummied-up
	 * (with invalid data) this should work. */
	if ( (state == DIRTY_EXCL) && (which == CACH_PD) ) {  
		/* read then write it--this sets 'W' bit w/o changing value */
		value = read_word(K0SEG, phys_addr);
		write_word(K0SEG, phys_addr, value);
	}

	return 0;

} /* set_tag */


/* the primary data and secondary (I+D) data-ram tests generate
 * accesses only to the target cache in order to test the component
 * without interaction with the rest of the system.  The pd cache is
 * verified first, then it is used to test the secondary. During the
 * 2ndary tests all writes must therefore be forced to the 2ndary.
 * flush_pdc() does this for the range of primary-D lines specified by
 * lo_caddr and hi_caddr (K0-space addrs).
 */
static
flush_pdc(lo_caddr, hi_caddr)
  uint lo_caddr;
  register uint hi_caddr;
{
	register uint loopaddr = WD_ALIGN(lo_caddr);
	uint dline;

	dline = get_dcacheline();

	for ( ; loopaddr < hi_caddr; loopaddr += dline) {
		pd_hwbinv(loopaddr);
	}

} /* flush_pdc */

#ifdef R4K_UNCACHED_BUG
/* XXXXXX in these cut-down diags, read_word and write_word
 * are only necessary until the 2.0 R4K parts arrive and are
 * verified to not exhibit the bug in which uncached reads and
 * writes go to the cache if the addr is in the cache and valid.
 * When it is fixed we can nuke the next two routines. XXXXXXXX
 */
/* given a physical address, read_word converts it to its equivalent
 * in the specified kernel space and reads the value there.
 */
static uint
read_word(accessmode, phys_addr)
  int accessmode;		/* K0SEG, K1SEG, or K2SEG */
  uint phys_addr;		/* read from this address */
{
	uint vaddr;
	tag_regs_t PITagSave,PDTagSave,STagSave,TagZero;
	uint caddr = PHYS_TO_K0(phys_addr);
	uint uval;

	/* if reading through k1seg, must invalidate line to
	 * force R4K to read from memory, not cache, due to bug */
	if (accessmode == K1SEG) {
		TagZero.tag_lo = 0;
/*		_read_tag(CACH_PI, caddr, &PITagSave); */
/*		_write_tag(CACH_PI, caddr, &TagZero); */
		_read_tag(CACH_PD, caddr, &PDTagSave);
		_write_tag(CACH_PD, caddr, &TagZero);
		_read_tag(CACH_SD, caddr, &STagSave);
		_write_tag(CACH_SD, caddr, &TagZero);
	}

	switch(accessmode) {
	case K0SEG:
			vaddr = PHYS_TO_K0(phys_addr);
			break;
	case K1SEG:
			vaddr = PHYS_TO_K1(phys_addr);
			break;
	case K2SEG:
			vaddr = phys_addr+K2BASE;
			break;
	}

	uval = *(uint *)vaddr;
	if (accessmode == K1SEG) {
/*		_write_tag(CACH_PI, caddr, &PITagSave); */
		_write_tag(CACH_PD, caddr, &PDTagSave);
		_write_tag(CACH_SD, caddr, &STagSave);
	}
	return (uval);

} /* end fn read_word */


/* given a physical address, write_word converts it to its equivalent
 * in the specified kernel space and stores value there.
 */
static
write_word(accessmode, phys_addr, value)
  int accessmode;		/* K0SEG, K1SEG, or K2SEG */
  uint phys_addr;		/* write to this address */
  uint value;
{
	uint vaddr;
	tag_regs_t PITagSave,PDTagSave,STagSave,TagZero;
	uint caddr = PHYS_TO_K0(phys_addr);

	/* if writing through k1seg, must invalidate line to
	 * force R4K to write to memory, not cache, due to bug */
	if (accessmode == K1SEG) {
		TagZero.tag_lo = 0;
/*		_read_tag(CACH_PI, caddr, &PITagSave); */
/*		_write_tag(CACH_PI, caddr, &TagZero); */
		_read_tag(CACH_PD, caddr, &PDTagSave);
		_write_tag(CACH_PD, caddr, &TagZero);
		_read_tag(CACH_SD, caddr, &STagSave);
		_write_tag(CACH_SD, caddr, &TagZero);
	}

	switch(accessmode) {
	case K0SEG:
			vaddr = PHYS_TO_K0(phys_addr);
			break;
	case K1SEG:
			vaddr = PHYS_TO_K1(phys_addr);
			break;
	case K2SEG:
			vaddr = phys_addr+K2BASE;
			break;
	}
	*(uint *)vaddr = value;

	if (accessmode == K1SEG) {
/*		_write_tag(CACH_PI, caddr, &PITagSave); */
		_write_tag(CACH_PD, caddr, &PDTagSave);
		_write_tag(CACH_SD, caddr, &STagSave);
	}

} /* end fn write_word */

#endif /* R4K_UNCACHED_BUG */


/* bwalking_caddr checks primary or secondary caches for shorts
 * and open address lines.  Address lines that are >= the most
 * significant address lines of the limits are not tested.  The
 * test is conducted by doing byte reads/writes beginning at lomem.
 * If testing the 2ndary, all writes must be flushed from the PD.
 * bwalking_caddr expects its caller to set the cache-tags as needed
 * for the test requested: generally DE primary d-tags for pddata,
 * invalid PD tags and DE s-tags for sddata.  It doesn't clean the
 * tags before exiting either.
 */

/* STEVE- Check the old version to figure out what this really does */
static
bwalking_caddr(memstrp, startbit, cache)
  struct memstr *memstrp;
  register int startbit;
  int cache;	/* CACH_SD or CACH_PD */
{
	register uint lomem = (uint)memstrp->lomem;
	register uint himem = (uint)memstrp->himem;
	uint mask = memstrp->mask;
	register uint k;
	register char *pmem,*refval;
	register uint testline;
	tag_regs_t chktag;
	int do2ndary = 0;
	int doneit = 0;
	int doflush = 0;

	if (cache == CACH_SD || cache == CACH_SI) /* flush writes to 2ndary */
		doflush = 1;

	refval = (char *)lomem;
	for (testline = startbit; (lomem|testline) < himem; testline <<= 1) {
		*refval = 0x55;
		if (doflush)
			pd_hwbinv(WD_ALIGN((uint)refval));

		pmem = (char *)(lomem | testline);
		if( pmem == refval )
			continue;
		*pmem = 0xaa;
		if (doflush)
			pd_hwbinv(WD_ALIGN((uint)pmem));

		k = *refval; 
		if ( (k  & mask) != 0x55 ) {
			/*
			printf("%s\n\t%s walking-addr error\n",
			    (cache==CACH_PD?dcache_str:scache_str));
			printf(
			  "\trefaddr 0x%x caddr 0x%x; read 0x%x expect 0x55\n",
		    	    (int)refval, (int)pmem, (int)k);
			printf("\tAddress line 0x%x may be bad\n",testline);
			*/
			return TBD;
		}
    	}

	return 0;

} /* bwalking_caddr */
	

static uint
addr_pattern(memstrp, inc, sense, cache)
  struct memstr *memstrp;
  register int inc;
  register int sense;		/* use address/~address pattern */
  int cache;
{
	register uint *lomem = memstrp->lomem;
	register uint *himem = memstrp->himem;
	register uint mask = memstrp->mask;
	register uint *pmem = (uint *)lomem;
	register uint *pmemhi = (uint *)himem;
	int fail = 0;
	register uint k;
	register uint data;
	int interval;

	while( pmem < pmemhi ) {
	        data = (sense == BIT_INVERT ? ~(uint)pmem : (uint)pmem);
		*pmem = data & mask;
		pmem += inc;
	}

	pmem = (uint *)lomem;
	while( pmem < pmemhi ) {
	        data = (sense == BIT_INVERT ? ~(uint)pmem : (uint)pmem);
		if( (k = *pmem & mask) != (data & mask)) {
			/* printf("\n\t%s address_pattern error: caddr 0x%x\n",
			    (cache==CACH_PD?dcache_str:scache_str),(uint)pmem);
			printf("\t\tread 0x%x, expected 0x%x\n",k,(data&mask));
			*/
			return TBD;
			/* return((uint)pmem); store failure data */
		}
		pmem += inc;
	}

	return 0;

} /* addr_pattern */


/* write a primary- or 2ndary-cache-sized (depending on 'cache') chunk
 * of 0xa5a5a5a5 pattern, then read it back to verify; repeat for
 * inverse pattern 0x5a5a5a5a.  When testing the secondary cache, the
 * pd must be flushed after each writing-pass (all but the last pd
 * cache's worth of writes were already forced-out due to primary misses). 
 */
static uint
cread_wr(memstrp, inc, cache)
  struct memstr *memstrp;
  uint inc;
  int cache;
{
	register uint *lomem = memstrp->lomem;
	register uint *himem = memstrp->himem;
	register uint mask = memstrp->mask;
	register uint data;
	register uint *ptr;

	/* fill s-cache with 0xa5a5a5a5 pattern */
	for (ptr = lomem; ptr < himem; ptr += inc )
		*ptr = PATTERN;

	/* read and verify PATTERN, then write ~PATTERN */
	for (ptr = lomem; ptr < himem; ptr += inc) {
		if ((data = *ptr & mask) != (PATTERN & mask)) {
			/* printf("\n\t%s read/write error: caddr 0x%x\n", 
			    (cache==CACH_PD?dcache_str:scache_str),(uint)ptr);
			printf("\t\tread 0x%x, expected 0x%x\n", (data & mask),
				(PATTERN & mask));
			*/
			return TBD;
			/* return((uint)ptr);  store failure info */
		}
		*ptr = ~PATTERN;	/* set up for next pass */
	}

	/* verify 0x5a5a5a5a */
	for (ptr = lomem ; ptr < himem; ptr += inc) {
		if ((data = *ptr & mask) != (~PATTERN & mask)) {
			/*
			printf("\n\t%s read/write error: caddr 0x%x\n",
			    (cache==CACH_PD?dcache_str:scache_str),(uint)ptr);
			printf("\t\tread 0x%x, expected 0x%x\n", data, mask);
			*/
			/*return((uint)ptr); store fialure info */
			return TBD;
		}
	}

	return 0;

} /* cread_wr */


/* put zeros into icache--it does horrible things if the pattern used to
 * check it is left in the rams, even with invalid tags. */
zero_icache()
{
    register uint *fillptr, i, j;
    volatile uint dummy;
    uint isize, iline; 

    iline = get_icacheline();
    isize = get_icachesize();

    j = iline / sizeof(uint);

    /* fill dcache with 0 */
    fillptr = (uint *) PHYS_TO_K0(POD_STACKADDR);
    i = isize / sizeof(uint);	/* total # of wds in pdcache */
    while(i--) {
	*fillptr++ = 0;
    }

    /* flush dcache to scache */
    fillptr = (uint *) PHYS_TO_K0(POD_STACKADDR);
    i = isize / iline;	/* total # of lines in pdcache */
    while(i--) {
	pd_hwbinv((uint)fillptr);
	fillptr += j;
    }

    /* and fill icache from scache */
    fillptr = (uint *) PHYS_TO_K0(POD_STACKADDR);
    i = isize / iline;
    while(i--) {
	fill_ipline((uint)fillptr);
	fillptr += j;
    }

}
