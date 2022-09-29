/***********************************************************************\
*	File:		mtest.c						*
*									*
*	Contains code for testing memory (regular and directories)	*
*									*
\***********************************************************************/

#ident "$Revision: 1.15 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/SN/kldiag.h>

#include <libkl.h>
#include <report.h>

#include "ip27prom.h"
#include "mdir.h"
#include "mtest.h"
#include "libasm.h"
#include "hub.h"
#include "rtc.h"
#include "pod.h"
#include "prom_leds.h"

#define VERBOSE			0

#define MTEST_LOADSTORE		1
#define MTEST_A5		1
#define MTEST_C3		1
#define MTEST_RANDOM		1
#define MTEST_ADDRESS		1

/*
 * Test values
 */

#define LEAVE_ECC		(1UL << 63)

#define PAT0			0x0000000000000000
#define PAT1			0x5555555555555555
#define PAT2			0xaaaaaaaaaaaaaaaa
#define PAT3			0xffffffffffffffff
#define PAT4			0xc3c3c3c3c3c3c3c3
#define PAT5			0x3c3c3c3c3c3c3c3c

#define PAT0E			(PAT0 | LEAVE_ECC)
#define PAT1E			(PAT1 | LEAVE_ECC)
#define PAT2E			(PAT2 | LEAVE_ECC)
#define PAT3E			(PAT3 | LEAVE_ECC)
#define PAT4E			(PAT4 | LEAVE_ECC)
#define PAT5E			(PAT5 | LEAVE_ECC)

#define RAND_START		0x9e0de51c090dde55
#define RAND_INCR		0xc0ffee0c0c0a0b19	/* Should be odd */

#define MISCOMPARE()							\
{									\
    fail->reread  = LD(vol_addr);					\
    hub_trigger();							\
    fail->addr	  = vol_addr;						\
    fail->mask	  = mask;						\
    fail->expect  = exp;						\
    fail->actual  = act;						\
    mdir_error_get_clear(NASID_GET(fail->addr), &fail->err);		\
    mtest_print_fail(fail, 1, 0);					\
    rc = -1;								\
    if (++errs >= maxerr) {						\
	hit_max();							\
	goto done;							\
    }									\
    if (kbintr(&next))							\
	goto done;							\
}

#define EXCEPTION()							\
{									\
    hub_trigger();							\
    fail->addr	 = vol_addr;						\
    fail->mask	 = 0;							\
    mdir_error_get_clear(NASID_GET(fail->addr), &fail->err);		\
    mtest_print_fail(fail, 1, 1);					\
    rc = -1;								\
    goto done;								\
}

/*
 * mtest_basic
 *
 *   To be called after memory has already been configured.
 */

static int chk_store(__uint64_t addr, __uint64_t value)
{
    if (mdir_error_check(NASID_GET(addr), 0)) {
	printf("*** Error storing %y into %y:\n", value, addr);
	mdir_error_decode(NASID_GET(addr), 0);
	mdir_error_get_clear(NASID_GET(addr), 0);
	return -1;
    } else
	return 0;
}

static int chk_load(__uint64_t addr)
{
    if (mdir_error_check(NASID_GET(addr), 0)) {
	printf("*** Error loading from %y:\n", addr);
	mdir_error_decode(NASID_GET(addr), 0);
	mdir_error_get_clear(NASID_GET(addr), 0);
	return -1;
    } else
	return 0;
}

/* Print status messages only if running from pod mode */

int mtest_basic(int premium, __uint64_t addr, int flag)
{
    hub_mderr_t		err;
    __uint64_t		mask, data, result;
    __uint64_t		bdoor;
    volatile __uint64_t	vol_addr;
    jmp_buf		fault_buf;
    mtest_fail_t	fail_buf, *fail = &fail_buf;
    void		*old_buf;
    int			rc	= 0;

#if VERBOSE
    printf("mbt: premium=%d addr=%y\n", premium, addr);
#endif

    if (flag)
        printf("Starting memory/directory tests...\n");

    mask = premium ? MD_PDIR_MASK : MD_SDIR_MASK;

    mdir_error_get_clear(NASID_GET(addr), 0);
    mdir_error_get_clear(NASID_GET(addr), &err);

    if (mdir_error_check(NASID_GET(addr), &err)) {
	printf("*** Unable to clear memory errors:\n");
	mdir_error_decode(NASID_GET(addr), &err);
	rc = -1;
    }

    /*
     * Test for directory data lines stuck at 1 or 0.
     * Then initialize the directory entry.
     */

    if (flag)
        printf("Testing directory now...\n");

    if (setfault(fault_buf, &old_buf)) {
        char            tmp[80];
        __uint64_t      cause;
        int		slot;

        slot = hub_slot_get();
        printf("*** Fatal exception while testing directory on nasid %d in"
		" slot n%d\n", NASID_GET(addr), slot);

        cause = get_cop0(C0_CAUSE);

        xlate_cause(cause, tmp);

        printf("\tCAUSE        %y (%s)\n", cause, tmp);

	result_fail("dirtest", KLDIAG_DIRTEST_FAILED, "caught fatal exception");

        rc = -1;
        goto done;
    }

    bdoor = BDDIR_ENTRY_LO(addr);
    data = 0ULL | LEAVE_ECC;

    SD(bdoor + 0, data);
    /* rc |= chk_store(bdoor + 0, data); */
    SD(bdoor + 8, data);
    /* rc |= chk_store(bdoor + 8, data); */

    result = LD(bdoor + 0) & mask;
    /* rc |= chk_load(bdoor + 0); */

    if (result != 0) {
	printf("*** Dir lo data bits stuck at 1 "
	       "(act: %y, exp: %y)\n", result, data & mask);
	result_fail("dirtest", KLDIAG_DIRTEST_FAILED, "stuck at 1");
	rc = -1;
    }

    result = LD(bdoor + 8) & mask;
    /* rc |= chk_load(bdoor + 8); */

    if (result != 0) {
	printf("*** Dir hi data bits stuck at 1 "
	       "(act: %y, exp: %y)\n", result, data & mask);
	result_fail("dirtest", KLDIAG_DIRTEST_FAILED, "stuck at 1");
	rc = -1;
    }

    data = ~0ULL & mask | LEAVE_ECC;

    SD(bdoor + 0, data);
    /* rc |= chk_store(bdoor + 0, data); */
    SD(bdoor + 8, data);
    /* rc |= chk_store(bdoor + 8, data); */

    result = LD(bdoor + 0) & mask;
    /* rc |= chk_load(bdoor + 0); */

    if (result != mask) {
	printf("*** Dir lo data bits stuck at 0 "
	       "(act: %y, exp: %y)\n", result, data & mask);
	result_fail("dirtest", KLDIAG_DIRTEST_FAILED, "stuck at 0");
	rc = -1;
    }

    result = LD(bdoor + 8) & mask;
    /* rc |= chk_load(bdoor + 8); */

    if (result != mask) {
	printf("*** Dir hi data bits stuck at 0 "
	       "(act: %y, exp: %y)\n", result, data & mask);
	result_fail("dirtest", KLDIAG_DIRTEST_FAILED, "stuck at 0");
	rc = -1;
    }

    data = premium ? MD_PDIR_INIT_LO : MD_SDIR_INIT_LO;
    SD(bdoor + 0, data);
    /* rc |= chk_store(bdoor + 8, data); */

    data = premium ? MD_PDIR_INIT_HI : MD_SDIR_INIT_HI;
    SD(bdoor + 8, data);
    /* rc |= chk_store(bdoor + 8, data); */

    bdoor = BDPRT_ENTRY(addr, 0);
    data = premium ? MD_PDIR_INIT_PROT : MD_SDIR_INIT_PROT;
    SD(bdoor, data);
    /* rc |= chk_store(bdoor, data); */

    /*
     * See if there are any stuck data bits in the memory.
     * ECC will fix these things so we need to check the error registers.
     */
    
    if (flag)
        printf("Testing memory now...\n");

    restorefault(old_buf);

    vol_addr = addr;

    if (setfault(fault_buf, &old_buf))
        EXCEPTION();

    SD(addr, 0xdaedbaefdaedbaef);
    mdir_error_get_clear(NASID_GET(addr), 0);
    SD(addr, 0);
    rc |= chk_store(addr, 0);

    result = LD(addr);
    rc |= chk_load(addr);

    if (result != 0) {
	printf("*** Memory data bits stuck at 1 "
	       "(act: %y, exp: %y)\n", result, 0);
	result_fail("memtest", KLDIAG_MEMTEST_FAILED, "stuck at 1");
	rc = -1;
    }

    SD(addr, ~0ULL);
    rc |= chk_store(addr, 0);

    result = LD(addr);
    rc |= chk_load(addr);

    if (result != ~0ULL) {
	printf("*** Memory data bits stuck at 0 "
	       "(act: %y, exp: %y)\n", result, ~0ULL);
	result_fail("memtest", KLDIAG_MEMTEST_FAILED, "stuck at 0");
	rc = -1;
    }

    mdir_error_get_clear(NASID_GET(addr), 0);

done:

    restorefault(old_buf);

    return rc;
}

/*
 * mtest_print_fail
 */

void mtest_print_fail(mtest_fail_t *fail, int mderrs, int exception)
{
    hub_lock(HUB_LOCK_PRINT);

    printf("----- MEMORY FAILURE (%s, node slot %d) -----\n",
	   exception ? "caught exception" : "miscompare",
	   hub_slot_get());

    if (mderrs) {
	if (! mdir_error_check(NASID_GET(fail->addr), &fail->err))
	    printf("No hub MD error registered\n");
	else
	    mdir_error_decode(NASID_GET(fail->addr), &fail->err);
    }

    if (exception) {
	char		tmp[80];
	__uint64_t	cause;

	cause = get_cop0(C0_CAUSE);

	xlate_cause(cause, tmp);

	printf("CAUSE        %y (%s)\n", cause, tmp);
    }

    printf("Address      %y\n", fail->addr);

    if (fail->mask) {
	ulong		syn;

	if (fail->mask != ~0ULL)
	    printf("Mask         %y\n", fail->mask);

	printf("Actual       %y\n", fail->actual & fail->mask);

	syn = (fail->actual ^ fail->expect) & fail->mask;

	if (syn) {
	    printf("Expected     %y\n", fail->expect & fail->mask);
	    printf("Difference   %y\n", syn);

	    if (bitcount(syn) == 1) {
		char		tmp[32];
		ulong		mc;
		int		loc, premium, dimm0_sel;

		mc	  = LD(REMOTE_HUB(NASID_GET(fail->addr),
					  MD_MEMORY_CONFIG));
		premium	  = (mc & MMC_DIR_PREMIUM) != 0;
		dimm0_sel = (int) (mc >> MMC_DIMM0_SEL_SHFT) & 3;
		loc	  = firstone(syn);

		if ((fail->addr >> 56) != 0x90)
		    mdir_xlate_addr_mem(tmp, fail->addr, 8 + loc);
		else if (premium)
		    mdir_xlate_addr_pdir(tmp, fail->addr, 7 + loc, dimm0_sel);
		else
		    mdir_xlate_addr_sdir(tmp, fail->addr, 5 + loc, dimm0_sel);

		printf("Single Bit   %s\n", tmp);
	    }
	}

	syn = (fail->reread ^ fail->actual) & fail->mask;

	if (syn)
	    printf("Reread       %y\n", fail->reread & fail->mask);

    }

    hub_unlock(HUB_LOCK_PRINT);
}

/*
 * Private utility routines
 *
 *   Ragnarok does a very good job of compiling these particular routines.
 *   It's just about as good as a hand-written assembly version.
 */

#define POLL_LEN		0x10000

static void hit_max(void)
{
    printf("Exceeded maximum error count (stopping)\n");
}

static int two_pat(__uint64_t saddr, __uint64_t eaddr,
		   __uint64_t pat0, __uint64_t pat1,
		   __uint64_t mask,
		   mtest_fail_t *fail, int maxerr)
{
    jmp_buf		fault_buf;
    void	       *old_buf;
    __uint64_t		exp, act;
    int			rc = 0;
    __uint64_t		taddr;
    volatile __uint64_t	vol_addr;
    volatile int	errs = 0;
    rtc_time_t		next = 0;

#if VERBOSE
    printf("two_pat(s=%016lx e=%016lx m=%016lx)\n", saddr, eaddr, mask);
    printf("two_pat(%016lx %016lx)\n", pat0, pat1);
#endif

    if (setfault(fault_buf, &old_buf))
	EXCEPTION();

    while (saddr < eaddr) {
	taddr = MIN(saddr + POLL_LEN, eaddr);

	while (saddr < taddr) {
	    /*
	     * It's necessary to save the address in a volatile location
	     * because the EXCEPTION handler is unable to see register
	     * variables.
	     */

	    vol_addr = saddr;

	    exp = pat0;
	    SD(saddr, exp);
	    act = LD(saddr);
	    if ((act ^ exp) & mask)
		MISCOMPARE();

	    exp = pat1;
	    SD(saddr, exp);
	    act = LD(saddr);
	    if ((act ^ exp) & mask)
		MISCOMPARE();

	    saddr += 8;
	}

	if (kbintr(&next)) {
	    rc = -1;
	    break;
	}
    }

 done:

    restorefault(old_buf);

#if VERBOSE
    printf("two_pat: rc=%d\n", rc);
#endif

    return rc;
}

static int fill(__uint64_t saddr, __uint64_t eaddr,
		__uint64_t pat0, __uint64_t pat1,
		__uint64_t ecc,
		mtest_fail_t *fail, int maxerr)
{
    jmp_buf		fault_buf;
    void	       *old_buf;
    int			rc = 0;
    __uint64_t		taddr;
    volatile __uint64_t	vol_addr;
    volatile int	errs = 0;
    rtc_time_t		next = 0;

#if VERBOSE
    printf("fill(s=%016lx e=%016lx)\n", saddr, eaddr);
    printf("fill(%016lx %016lx)\n", pat0, pat1);
#endif

    if (setfault(fault_buf, &old_buf))
	EXCEPTION();

    while (saddr < eaddr) {
	taddr = MIN(saddr + POLL_LEN, eaddr);

	while (saddr < taddr) {
	    /*
	     * It's necessary to save the address in a volatile location
	     * because the EXCEPTION handler is unable to see register
	     * variables.
	     */

	    vol_addr = saddr + 0;		/* 1st unroll */
	    SD(saddr +	0, pat0 | ecc);
	    vol_addr = saddr + 8;
	    SD(saddr +	8, pat1 | ecc);
	    vol_addr = saddr + 16;		/* 2nd unroll */
	    SD(saddr + 16, pat0 | ecc);
	    vol_addr = saddr + 24;
	    SD(saddr + 24, pat1 | ecc);

	    saddr += 32;
	}

	if (kbintr(&next)) {
	    rc = -1;
	    break;
	}
    }

 done:

    restorefault(old_buf);

#if VERBOSE
    printf("fill: rc=%d\n", rc);
#endif

    return rc;
}

static int check(__uint64_t saddr, __uint64_t eaddr,
		 __uint64_t pat0, __uint64_t pat1,
		 __uint64_t mask,
		 mtest_fail_t *fail, int maxerr)
{
    jmp_buf		fault_buf;
    void	       *old_buf;
    __uint64_t		exp, act;
    int			rc = 0;
    __uint64_t		taddr;
    volatile __uint64_t	vol_addr;
    volatile int	errs = 0;
    rtc_time_t		next = 0;

#if VERBOSE
    printf("check(s=%016lx e=%016lx m=%016lx)\n", saddr, eaddr, mask);
    printf("check(%016lx %016lx)\n", pat0, pat1);
#endif

    if (setfault(fault_buf, &old_buf))
	EXCEPTION();

    while (saddr < eaddr) {
	taddr = MIN(saddr + POLL_LEN, eaddr);

	while (saddr < taddr) {
	    /*
	     * It's necessary to save the address in a volatile location
	     * because the EXCEPTION handler is unable to see register
	     * variables.
	     */

	    vol_addr = saddr + 0;	/* 1st unroll */
	    exp = pat0;
	    act = LD(saddr +  0);
	    if ((act ^ exp) & mask)
		MISCOMPARE();

	    vol_addr = saddr + 8;
	    exp = pat1;
	    act = LD(saddr +  8);
	    if ((act ^ exp) & mask)
		MISCOMPARE();

	    vol_addr = saddr + 16;	/* 2nd unroll */
	    exp = pat0;
	    act = LD(saddr + 16);
	    if ((act ^ exp) & mask)
		MISCOMPARE();

	    vol_addr = saddr + 24;
	    exp = pat1;
	    act = LD(saddr + 24);
	    if ((act ^ exp) & mask)
		MISCOMPARE();

	    saddr += 32;
	}

	if (kbintr(&next)) {
	    rc = -1;
	    break;
	}
    }

 done:

    restorefault(old_buf);

#if VERBOSE
    printf("check(rc=%d)\n", rc);
#endif

    return rc;
}

static int fill_linear(__uint64_t saddr, __uint64_t eaddr,
		       __uint64_t rstart, __uint64_t rincr,
		       __uint64_t ecc,
		       mtest_fail_t *fail, int maxerr)
{
    jmp_buf		fault_buf;
    void	       *old_buf;
    int			rc = 0;
    __uint64_t		taddr;
    volatile __uint64_t	vol_addr;
    volatile int	errs = 0;
    rtc_time_t		next = 0;

#if VERBOSE
    printf("fill_linear(s=%016lx e=%016lx)\n", saddr, eaddr);
    printf("fill_linear(rs=%016lx ri=%016lx)\n", rstart, rincr);
#endif

    if (setfault(fault_buf, &old_buf))
	EXCEPTION();

    while (saddr < eaddr) {
	taddr = MIN(saddr + POLL_LEN, eaddr);

	while (saddr < taddr) {
	    /*
	     * It's necessary to save the address in a volatile location
	     * because the EXCEPTION handler is unable to see register
	     * variables.
	     */

	    vol_addr = saddr;		/* 1st unroll */
	    SD(saddr, rstart | ecc);
	    saddr += 8;
	    rstart += rincr;

	    vol_addr = saddr;		/* 2nd unroll */
	    SD(saddr, rstart | ecc);
	    saddr += 8;
	    rstart += rincr;
	}

	if (kbintr(&next)) {
	    rc = -1;
	    break;
	}
    }

 done:

    restorefault(old_buf);

#if VERBOSE
    printf("fill_linear: rc=%d\n", rc);
#endif

    return rc;
}

static int check_linear(__uint64_t saddr, __uint64_t eaddr,
			__uint64_t rstart, __uint64_t rincr,
			__uint64_t mask,
			mtest_fail_t *fail, int maxerr)
{
    jmp_buf		fault_buf;
    void	       *old_buf;
    __uint64_t		act, exp;
    int			rc = 0;
    __uint64_t		taddr;
    volatile __uint64_t	vol_addr;
    volatile int	errs = 0;
    rtc_time_t		next = 0;

#if VERBOSE
    printf("check_linear(s=%016lx e=%016lx)\n", saddr, eaddr);
    printf("check_linear(rs=%016lx ri=%016lx)\n", rstart, rincr);
#endif

    if (setfault(fault_buf, &old_buf))
	EXCEPTION();

    exp = rstart;

    while (saddr < eaddr) {
	taddr = MIN(saddr + POLL_LEN, eaddr);

	while (saddr < taddr) {
	    /*
	     * It's necessary to save the address in a volatile location
	     * because the EXCEPTION handler is unable to see register
	     * variables.
	     */

	    vol_addr = saddr;		/* 1st unroll */
	    act = LD(saddr);
	    if ((act ^ exp) & mask)
		MISCOMPARE();
	    saddr += 8;
	    exp += rincr;

	    vol_addr = saddr;		/* 2nd unroll */
	    act = LD(saddr);
	    if ((act ^ exp) & mask)
		MISCOMPARE();
	    saddr += 8;
	    exp += rincr;
	}

	if (kbintr(&next)) {
	    rc = -1;
	    break;
	}
    }

 done:

    restorefault(old_buf);

#if VERBOSE
    printf("check_linear(rc=%d)\n", rc);
#endif

    return rc;
}

/*
 * mtest_dir_backdoor
 *
 *   Test an area of the back door directory/prot/ref. count memory.
 *   The entire area is tested with double-word accesses at 48 bits
 *   for premium SIMMs or 16 bits for standard.  Bit 63 is always written
 *   so the directory ECC memory is also tested (but not ECC generation).
 *
 *   Note:  Spurious errors occur in the MD_DIR_ERROR register when
 *   writing to back door memory if back door memory already contains
 *   illegal ECC.  This routine does not check MD_DIR_ERROR.
 *
 *   premium: true if predetermined directory type is premium
 *   sback: start address in back door space, multiple of 4096
 *   eback: end address in back door space, multiple of 4096
 *   fail: pointer to structure where failure info is stored
 *   maxerr: maximum number of errors to print before giving up
 *
 *   Return 0 for success, -1 for failure.
 */

int mtest_dir_backdoor(int premium,
		       __uint64_t sback,
		       __uint64_t eback,
		       int diag_mode,
		       mtest_fail_t *fail,
		       int maxerr)
{
    __uint64_t	mask;
    int		rc = -1;
    __uint64_t	ecc;

#if VERBOSE
    printf("mbdt(%d,%lx,%lx,%d)\n", premium, sback, eback, diag_mode);
#endif

    if (diag_mode == DIAG_MODE_NONE) {
	rc = 0;
	goto done;
    }

    /*
     * Exceptions will be generated if quality checking is on.
     * See the "qual" command.
     */

    mdir_error_get_clear(NASID_GET(sback), 0);

    mask = premium ? MD_PDIR_MASK : MD_SDIR_MASK;

    ecc = LEAVE_ECC;

#if MTEST_LOADSTORE
    if (diag_mode != DIAG_MODE_NORMAL) {
	/*
	 * Fast load after store test
	 */
	
	db_printf("\t\tStore/load\n");
	
	if (two_pat(sback, eback, PAT1E, PAT2E, mask, fail, maxerr) < 0)
	    goto done;
    }
#endif

#if MTEST_A5
    /*
     * Check alternating a's and 5's
     */

    db_printf("\t\tA5 fill\n");

    if (fill(sback, eback, PAT1E, PAT2E, ecc, fail, maxerr) < 0)
	goto done;

    db_printf("\t\tA5 verify\n");

    if (check(sback, eback, PAT1, PAT2, mask, fail, maxerr) < 0)
	goto done;

    db_printf("\t\t5A fill\n");

    if (fill(sback, eback, PAT2E, PAT1E, ecc, fail, maxerr) < 0)
	goto done;

    db_printf("\t\t5A verify\n");

    if (check(sback, eback, PAT2, PAT1, mask, fail, maxerr) < 0)
	goto done;
#endif

#if MTEST_C3
    if (diag_mode != DIAG_MODE_NORMAL) {
	/*
	 * Check alternating c's and 3's
	 */

	db_printf("\t\tC3 fill\n");

	if (fill(sback, eback, PAT4E, PAT5E, ecc, fail, maxerr) < 0)
	    goto done;

	db_printf("\t\tC3 verify\n");

	if (check(sback, eback, PAT4, PAT5, mask, fail, maxerr) < 0)
	    goto done;

	db_printf("\t\t3C fill\n");

	if (fill(sback, eback, PAT5E, PAT4E, ecc, fail, maxerr) < 0)
	    goto done;

	db_printf("\t\t3C verify\n");

	if (check(sback, eback, PAT5, PAT4, mask, fail, maxerr) < 0)
	    goto done;
    }
#endif

#if MTEST_RANDOM
    /*
     * Check a pseudo-random pattern (address bit test)
     */
    
    db_printf("\t\tRandom fill\n");
    
    if (fill_linear(sback, eback, RAND_START, RAND_INCR,
		    ecc, fail, maxerr) < 0)
	goto done;
    
    db_printf("\t\tRandom verify\n");
    
    if (check_linear(sback, eback, RAND_START, RAND_INCR,
		     mask, fail, maxerr) < 0)
	goto done;
#endif

#if MTEST_ADDRESS
    if (diag_mode != DIAG_MODE_NORMAL) {
	/*
	 * Check a linear fill with address (address bit test)
	 */
	
	db_printf("\t\tAddress fill\n");
	
	if (fill_linear(sback, eback, sback, 8, ecc, fail, maxerr) < 0)
	    goto done;
	
	db_printf("\t\tAddress verify\n");
	
	if (check_linear(sback, eback, sback, 8, mask, fail, maxerr) < 0)
	    goto done;
    }
#endif

    rc = 0;

 done:

    mdir_error_get_clear(NASID_GET(sback), 0);

    return rc;
}

/*
 * mtest_ecc_backdoor
 *
 *   Not clear what this should do
 */

int mtest_ecc_backdoor(int premium,
		       __uint64_t sback,
		       __uint64_t eback,
		       int diag_mode,
		       mtest_fail_t *fail,
		       int maxerr)
{
    mdir_error_get_clear(NASID_GET(sback), 0);

    return 0;
}

/*
 * mtest_dir
 *
 *   Tests the dir/prot memory through the back door, for a range of memory.
 *
 *   premium: true if predetermined directory type is premium
 *   saddr: page-aligned start physical address in back door memory
 *   eaddr: page-aligned end physical address (exclusive) in back door memory
 *
 *   Returns 0 for success, -1 for failure.
 */

int
mtest_dir(int premium, __uint64_t saddr, __uint64_t eaddr,
	  int diag_mode, mtest_fail_t *fail, int maxerr)
{
    int		rc = 0;

#if VERBOSE
    printf("mcbd(%d,%lx,%lx)\n", premium, saddr, eaddr);
#endif

    TRACE(premium);
    TRACE(saddr);
    TRACE(eaddr);

    if (mtest_dir_backdoor(premium,
			   (__uint64_t) BDPRT_ENTRY(saddr, 0),
			   (__uint64_t) BDPRT_ENTRY(eaddr - 0x1000, 0) + 0x400,
			   diag_mode, fail, maxerr) < 0)
	rc = -1;

    TRACE(rc);

    return rc;
}

/*
 * mtest_ecc
 *
 *   Tests the ECC through the back door, for a range of memory.
 *
 *   premium: true if predetermined directory type is premium
 *   saddr: page-aligned start physical address in back door memory
 *   eaddr: page-aligned end physical address (exclusive) in back door memory
 *
 *   Returns 0 for success, -1 for failure.
 */

int
mtest_ecc(int premium, __uint64_t saddr, __uint64_t eaddr,
	  int diag_mode, mtest_fail_t *fail, int maxerr)
{
    int		rc = 0;

#if VERBOSE
    printf("mcbe(%d,%lx,%lx)\n", premium, saddr, eaddr);
#endif

    TRACE(premium);
    TRACE(saddr);
    TRACE(eaddr);

    if (mtest_ecc_backdoor(premium,
			   (__uint64_t) BDECC_ENTRY(saddr),
			   (__uint64_t) BDECC_ENTRY(eaddr - 0x20) + 8,
			   diag_mode, fail, maxerr) < 0)
	rc = -1;

    TRACE(rc);

    return rc;
}

/*
 * mtest
 */

int mtest(__uint64_t saddr, __uint64_t eaddr,
	  int diag_mode, mtest_fail_t *fail, int maxerr)
{
    int		rc = -1;

#if VERBOSE
    printf("mmt(%lx,%lx,%d)\n", saddr, eaddr, diag_mode);
#endif

    if (diag_mode == DIAG_MODE_NONE) {
	rc = 0;
	goto done;
    }

    /*
     * Exceptions will be generated if quality checking is on.
     * See the "qual" command.
     */

    mdir_error_get_clear(NASID_GET(saddr), 0);

#if MTEST_LOADSTORE
    if (diag_mode != DIAG_MODE_NORMAL) {
	/*
	 * Fast load after store test
	 */
	
	db_printf("\t\tStore/load\n");
	
	if (two_pat(saddr, eaddr, PAT1E, PAT2E, ~0ULL, fail, maxerr) < 0)
	    goto done;
    }
#endif

#if MTEST_A5
    /*
     * Check alternating a's and 5's
     */

    db_printf("\t\tA5 fill\n");

    if (fill(saddr, eaddr, PAT1, PAT2, 0, fail, maxerr) < 0)
	goto done;

    db_printf("\t\tA5 verify\n");

    if (check(saddr, eaddr, PAT1, PAT2, ~0ULL, fail, maxerr) < 0)
	goto done;

    db_printf("\t\t5A fill\n");

    if (fill(saddr, eaddr, PAT2, PAT1, 0, fail, maxerr) < 0)
	goto done;

    db_printf("\t\t5A verify\n");

    if (check(saddr, eaddr, PAT2, PAT1, ~0ULL, fail, maxerr) < 0)
	goto done;
#endif

#if MTEST_C3
    if (diag_mode != DIAG_MODE_NORMAL) {
	/*
	 * Check alternating c's and 3's
	 */

	db_printf("\t\tC3 fill\n");

	if (fill(saddr, eaddr, PAT4, PAT5, 0, fail, maxerr) < 0)
	    goto done;

	db_printf("\t\tC3 verify\n");

	if (check(saddr, eaddr, PAT4, PAT5, ~0ULL, fail, maxerr) < 0)
	    goto done;

	db_printf("\t\t3C fill\n");

	if (fill(saddr, eaddr, PAT5, PAT4, 0, fail, maxerr) < 0)
	    goto done;

	db_printf("\t\t3C verify\n");

	if (check(saddr, eaddr, PAT5, PAT4, ~0ULL, fail, maxerr) < 0)
	    goto done;
    }
#endif

#if MTEST_RANDOM
    /*
     * Check a pseudo-random pattern (address bit test)
     */
    
    db_printf("\t\tRandom fill\n");
    
    if (fill_linear(saddr, eaddr, RAND_START, RAND_INCR,
		    0, fail, maxerr) < 0)
	goto done;
    
    db_printf("\t\tRandom verify\n");
    
    if (check_linear(saddr, eaddr, RAND_START, RAND_INCR,
		     ~0ULL, fail, maxerr) < 0)
	goto done;
#endif

#if MTEST_ADDRESS
    if (diag_mode != DIAG_MODE_NORMAL) {
	/*
	 * Check a linear fill with address (address bit test)
	 */
	
	db_printf("\t\tAddress fill\n");
	
	if (fill_linear(saddr, eaddr, saddr, 8, 0, fail, maxerr) < 0)
	    goto done;
	
	db_printf("\t\tAddress verify\n");
	
	if (check_linear(saddr, eaddr, saddr, 8, ~0ULL, fail, maxerr) < 0)
	    goto done;
    }
#endif

    rc = 0;

 done:

    mdir_error_get_clear(NASID_GET(saddr), 0);

    return rc;
}
