/***********************************************************************\
*	File:		hub.c						*
*									*
*	This file contains the IP27 PROM Utility Library routines for	*
*	manipulating miscellaneous features of the hub.			*
*									*
\***********************************************************************/

#ident "$Revision: 1.63 $"

#include <sys/types.h>
#include <sys/nic.h>
#include <sys/SN/agent.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/intr.h>
#include <ksys/elsc.h>
#include <libkl.h>

#include "hub.h"
#include "libc.h"
#include "libasm.h"
#include "prom_leds.h"		/* XXX move to sys/SN */

#define LD(addr)		(*(volatile __uint64_t *) (addr))
#define SD(addr, value)		(LD(addr) = (__uint64_t) (value))

#define PRIMARY_LOCK_TIMEOUT	10000		/* usec */
#define HUB_LOCK_REG		LOCAL_HUB(MD_PERF_CNT0)

#define SET_BITS(reg, bits)	SD(reg, LD(reg) |  (bits))
#define CLR_BITS(reg, bits)	SD(reg, LD(reg) & ~(bits))
#define TST_BITS(reg, bits)	((LD(reg) & (bits)) != 0)

/*
 * Several interrupt levels are used for mutual exclusion between the
 * local CPUs.  These bits are not touched by the Kernel so we can use
 * these locks if the Kernel ever returns to the PROM.  These bits are
 * cleared at the beginning of the PROM before the local arbitration.
 * We have to add 64 to use INT_PEND1.
 */

#define INT_WANT_A		(IP27_INTR_0 + 64)
#define INT_WANT_B		(IP27_INTR_1 + 64)
#define INT_FAVOR_B		(IP27_INTR_2 + 64)

#define INT_BARRIER_A		(IP27_INTR_3 + 64)
#define INT_BARRIER_B		(IP27_INTR_4 + 64)

#define INT_SPARE1		(IP27_INTR_5 + 64)
#define INT_SPARE2		(IP27_INTR_6 + 64)
#define INT_SPARE3		(IP27_INTR_7 + 64)

extern int get_hub_nic_info(nic_data_t, char *) ;

/*
 * hub_halt_remote
 *
 *   Disables a remote CPU's Hub port, effectively taking it out of
 *   the system.  Sets the remote CPU's LEDs to indicate that this
 *   has taken place.
 */

void hub_halt_remote(nasid_t nasid, int cpu)
{
    SD(REMOTE_HUB(nasid, PI_CPU_ENABLE_A) + cpu, 0);
    hub_led_set_remote(nasid, cpu, FLED_KILLED);
}

/*
 * void hub_halt
 *
 *   Disables the current CPU, effectively taking it out of the system.
 *   Hub_barrier() and hub_lock() will continue to work because they
 *   always check the number of CPUs enabled and have timeouts.
 */

void hub_halt(void)
{
    hub_alive_set(LD(LOCAL_HUB(PI_CPU_NUM)), 0);
    SD(LOCAL_HUB(PI_CPU_ENABLE_A) + LD(LOCAL_HUB(PI_CPU_NUM)), 0);
    /* Never reached */
}

/*
 * hub_num_local_cpus
 */

int hub_num_local_cpus(void)
{
    return hub_alive(0) + hub_alive(1);
}

/*
 * hub_local_master
 */

int hub_local_master(void)
{
    return (hub_num_local_cpus() < 2) ? 1 : (hub_cpu_get() == 0);
}

/*
 * hub_led_show
 *
 *   Flashes a value on the LEDs on the local hub for a while.
 */

void hub_led_show(__uint64_t leds)
{
    int		i;

    for (i = 0; i < 10; i++) {		/* 5 sec */
	hub_led_set(leds);
	rtc_sleep(400000);
	hub_led_set(0xff);
	rtc_sleep(100000);
    }
}

/*
 * hub_led_flash
 *
 *   Flashes a specified value on the LEDs until a Control-P is received
 *   on the UART.  It then returns.
 */

#define CTRL(x)		((x) & 0x1f)
#define FLASH_PERIOD	500000	/* microseconds */

void
hub_led_flash(__uint64_t leds)
{
    volatile int	count;
    int			f;
    rtc_time_t		expire;

    expire = rtc_time();

    for (f = 0;
#if 0
	 ! poll() || readc() != CTRL('P')
#endif
	 ; f ^= leds) {
	hub_led_set(f);

	expire += FLASH_PERIOD / 2;

	while (rtc_time() < expire)
	    ;
    }
}

/*
 * hub_led_pause
 *
 *   Puts a specified value on the LEDs and delays for 1 second.
 */

void
hub_led_pause(__uint64_t leds)
{

    hub_led_set(leds);
    rtc_sleep(1000000);
}


/*
 * hub_display_digits
 *
 * This function takes a slot number and an led value. When looking at the 
 * display the results will be printed: 11223344 (where the number is the 
 * node's slot number). So if both processors on a node use this call they
 * will overwrite eachothers values. To help with this the macros 
 * hub_display_taken and hub_set_display_taken check and set a bit used to
 * indicate when the current display value for this slot should not be
 * overwritten.
 */

void hub_display_digits(int slot, __uint64_t val) {
    int digit_1, digit_2;
    int led_num = (slot * 2) - 1;
    
    digit_1 = val & 0x0fll;
    digit_2 = (val & 0xf0ll) >> 4;
    
    /* display left to right */
    elsc_display_digit(get_elsc(), led_num++,digit_2,0);
    elsc_display_digit(get_elsc(), led_num,digit_1,0);
}


/*
 * hub_led_elsc_set
 *
 * This function is used to display a pattern on the hub leds and also
 * display the hex value on the elsc. This should only be called after the
 * elsc has been initialized.
 */
void hub_led_elsc_set(__uint64_t val) {
    
    hub_led_set(val);

    /* if we are the master and no error code has been written to the elsc
       write the value the the elsc display digits for our slot */
    if (hub_local_master() && !hub_display_taken())
	hub_display_digits(hub_slot_get(),val);
}


/*
 * hub_int_set_all
 *
 *   Sets or clears each interrupt to match the masks provided.
 *
 *   mask1: values for interrupts 122:64 (INT_PEND0)
 *   mask0: values for interrupts 63:0 (INT_PEND1)
 */

void hub_int_set_all(__uint64_t mask1, __uint64_t mask0)
{
    int		i;

    for (i = 0; i < 64; i++)
	if (mask0 & (1UL << i))
	    hub_int_set(i);
	else
	    hub_int_clear(i);

    for (i = 0; i < 59; i++)
	if (mask1 & (1UL << i))
	    hub_int_set(i + 64);
	else
	    hub_int_clear(i + 64);
}

/*
 * hub_int_get_all
 *
 *   Reads the status of interrupts 122:0 and returns them in two masks.
 *
 *   mask1: values for interrupts 122:64 (INT_PEND0)
 *   mask0: values for interrupts 63:0 (INT_PEND1)
 */

void hub_int_get_all(__uint64_t *mask1, __uint64_t *mask0)
{
    *mask1 = LD(LOCAL_HUB(PI_INT_PEND1));
    *mask0 = LD(LOCAL_HUB(PI_INT_PEND0));
}

/*
 * hub_int_mask_out
 *
 *   Masks out a specified interrupt in the mask of the current processor.
 */

void hub_int_mask_out(int intnum)
{
    if (hub_cpu_get() == 0) {
	if (intnum < 64)
	    CLR_BITS(LOCAL_HUB(PI_INT_MASK0_A), 1UL << intnum);
	else
	    CLR_BITS(LOCAL_HUB(PI_INT_MASK1_A), 1UL << intnum - 64);
    } else {
	if (intnum < 64)
	    CLR_BITS(LOCAL_HUB(PI_INT_MASK0_B), 1UL << intnum);
	else
	    CLR_BITS(LOCAL_HUB(PI_INT_MASK1_B), 1UL << intnum - 64);
    }
}

/*
 * hub_int_mask_in
 *
 *   Masks in a specified interrupt in the mask of the current processor.
 */

void hub_int_mask_in(int intnum)
{
    if (hub_cpu_get() == 0) {
	if (intnum < 64)
	    SET_BITS(LOCAL_HUB(PI_INT_MASK0_A), 1UL << intnum);
	else
	    SET_BITS(LOCAL_HUB(PI_INT_MASK1_A), 1UL << intnum - 64);
    } else {
	if (intnum < 64)
	    SET_BITS(LOCAL_HUB(PI_INT_MASK0_B), 1UL << intnum);
	else
	    SET_BITS(LOCAL_HUB(PI_INT_MASK1_B), 1UL << intnum - 64);
    }
}

/*
 * hub_int_set_mask
 *
 *   Reads the interrupt mask of the current processor.
 */

void hub_int_set_mask(__uint64_t mask1, __uint64_t mask0)
{
    if (hub_cpu_get() == 0) {
	SD(LOCAL_HUB(PI_INT_MASK1_A), mask1);
	SD(LOCAL_HUB(PI_INT_MASK0_A), mask0);
    } else {
	SD(LOCAL_HUB(PI_INT_MASK1_B), mask1);
	SD(LOCAL_HUB(PI_INT_MASK0_B), mask0);
    }
}

/*
 * hub_int_get_mask
 *
 *   Reads the interrupt mask of the current processor.
 */

void hub_int_get_mask(__uint64_t *mask1, __uint64_t *mask0)
{
    if (hub_cpu_get() == 0) {
	*mask1 = LD(LOCAL_HUB(PI_INT_MASK1_A));
	*mask0 = LD(LOCAL_HUB(PI_INT_MASK0_A));
    } else {
	*mask1 = LD(LOCAL_HUB(PI_INT_MASK1_B));
	*mask0 = LD(LOCAL_HUB(PI_INT_MASK0_B));
    }
}

#ifdef HUB_INT_DIAGS

/*
 * test_pattern (INTERNAL)
 *
 *   Sets interrupts 0 and 6 through 122 to a given pattern.
 *   Checks to make sure the values match when read back.
 *   Returns 0 on success, -1 on mismatch (and prints error msg).
 */

static int test_pattern(__uint64_t mask1, __uint64_t mask0)
{
    __uint64_t		check1, check0;

    hub_int_set_all(mask1, mask0);

    hub_int_get_all(&check1, &check0);

    if ((mask1 ^ check1) & ~BIT_RANGE(63, 59)) {
#if VERBOSE
	printf("%C: Hub INT_PEND0 mismatch: expect 0x%lx got 0x%lx\n",
	       mask1, check1);
#endif
	return -1;
    }

    if ((mask0 ^ check0) & ~BIT_RANGE(5, 1)) {
#if VERBOSE
	printf("%C: Hub INT_PEND1 mismatch: expect 0x%lx got 0x%lx\n",
	       mask0, check0);
#endif
	return -1;
    }

    return 0;
}

/*
 * test_cause
 *
 *   Poll the CAUSE register until the specified int. level bit (imask)
 *   equals the required value (itest, which is either 0 or imask).
 *   Returns error on time-out or if unrelated interrupt levels are set.
 */

#define CAUSE_TRIES	64

static int test_cause(__uint64_t imask, int itest, char *iname)
{
    __uint64_t		cs;
    int			i;

    for (i = 0; i < CAUSE_TRIES; i++)
	if (((cs = get_cop0(C0_CAUSE)) & imask) == itest)
	    break;

    if (i == CAUSE_TRIES) {
#if VERBOSE
	printf("%C: Failed to %s %s interrupt (cause=0x%lx)\n",
	       itest ? "receive" : "clear", iname, cs);
#endif
	return -1;
    }

    if ((cs & HUB_IP_MASK) != itest) {
#if VERBOSE
	printf("%C: Spurious interrupts (cause=0x%lx, expected 0x%lx)\n",
	       cs, itest);
#endif
	return -1;
    }

    return 0;
}

/*
 * hub_int_diag
 *
 *   Simple diagnostics for interrupts as viewed from one CPU.
 *   Messages are logged for the various errors that may occur.
 *   Returns 0 on success, -1 if there were any errors.
 */

int hub_int_diag(void)
{
    int		rc = 0;

    /*
     * Test functionality and independence of setting/clearing PEND bits.
     */

    if (test_pattern(0x0000000000000000, 0x0000000000000000) < 0)
	rc = -1;

    if (test_pattern(0xaaaaaaaaaaaaaaaa, 0xaaaaaaaaaaaaaaaa) < 0)
	rc = -1;

    if (test_pattern(0x5555555555555555, 0x5555555555555555) < 0)
	rc = -1;

    if (test_pattern(0xffffffffffffffff, 0xffffffffffffffff) < 0)
	rc = -1;

    if (test_pattern(0x0000000000000000, 0x0000000000000000) < 0)
	rc = -1;

    /*
     * Mask in interrupt 63, send it, and verify that the
     * L2 interrupt (IP2) comes on in the CAUSE register.
     * Clear it, and verify the L2 interrupt turns off.
     */

    hub_int_mask_in(63);

    hub_int_set(63);

    if (test_cause(HUB_IP_PEND0, HUB_IP_PEND0, "L2") < 0)
	rc = -1;

    hub_int_clear(63);

    if (test_cause(HUB_IP_PEND0, 0, "L2") < 0)
	rc = -1;

    hub_int_mask_out(63);

    /*
     * Mask in interrupt 64, send it, and verify that the
     * L3 interrupt (IP3) comes on in the CAUSE register.
     * Clear it, and verify the interrupt turns off.
     */

    hub_int_mask_in(64);

    hub_int_set(64);

    if (test_cause(HUB_IP_PEND1_CC, HUB_IP_PEND1_CC, "L3") < 0)
	rc = -1;

    hub_int_clear(64);

    if (test_cause(HUB_IP_PEND1_CC, 0, "L3") < 0)
	rc = -1;

    hub_int_mask_out(64);

    /*
     * All interrupts left off
     */

    return rc;
}

#endif /* HUB_INT_DIAGS */

#ifdef RTL
/*
 * hub_init_regs
 *
 *   Sets all writable hub PI and MD registers to their RESET defaults,
 *   except for the interrupts PEND0, PEND1, and CC.
 *   Error registers should be saved before-hand.
 */

void
hub_init_regs(int mask)
{
#ifndef SABLE
#if 1
    if (mask & HUB_REGS_PI) {
	SD(LOCAL_HUB(PI_CPU_PROTECT), -1);
	/*
	 * FIXME - 0 is not the right value here... it prevents
	 * IO widgets from interrupting CPU.
	 */
#if 0
	SD(LOCAL_HUB(PI_IO_PROTECT), 0);
#endif
	SD(LOCAL_HUB(PI_REGION_PRESENT), -1);
	/* This isn't the reset default, but it's what we want. */
	SD(LOCAL_HUB(PI_CALIAS_SIZE), PI_CALIAS_SIZE_4K);
	SD(LOCAL_HUB(PI_MAX_CRB_TIMEOUT), 128);
	SD(LOCAL_HUB(PI_CRB_SFACTOR), 100);
	SD(LOCAL_HUB(PI_CPU_ENABLE_A), 1);
	SD(LOCAL_HUB(PI_CPU_ENABLE_B), 1);
	SD(LOCAL_HUB(PI_INT_MASK0_A), 0);
	SD(LOCAL_HUB(PI_INT_MASK1_A), 0);
	SD(LOCAL_HUB(PI_INT_MASK0_B), 0);
	SD(LOCAL_HUB(PI_INT_MASK1_B), 0);
	SD(LOCAL_HUB(PI_CC_PEND_CLR_A), -1);
	SD(LOCAL_HUB(PI_CC_PEND_CLR_B), -1);
	SD(LOCAL_HUB(PI_CC_MASK), 0);

#if 0					/* Don't screw over the clock! */
	SD(LOCAL_HUB(PI_RT_LOCAL_CTRL), 0);	/* Turn clock off first */
	SD(LOCAL_HUB(PI_RT_FILTER_CTRL), 0);
	SD(LOCAL_HUB(PI_RT_COUNT), 0);
	SD(LOCAL_HUB(PI_RT_COMPARE_A), 0);
	SD(LOCAL_HUB(PI_RT_COMPARE_B), 0);
	SD(LOCAL_HUB(PI_PROFILE_COMPARE), 0);
#endif

	SD(LOCAL_HUB(PI_RT_PEND_A), 0);
	SD(LOCAL_HUB(PI_RT_PEND_B), 0);
	SD(LOCAL_HUB(PI_PROF_PEND_A), 0);
	SD(LOCAL_HUB(PI_PROF_PEND_B), 0);
	SD(LOCAL_HUB(PI_RT_EN_A), 0);
	SD(LOCAL_HUB(PI_RT_EN_B), 0);
	SD(LOCAL_HUB(PI_PROF_EN_A), 0);
	SD(LOCAL_HUB(PI_PROF_EN_B), 0);

	SD(LOCAL_HUB(PI_ERR_INT_PEND), -1);
	SD(LOCAL_HUB(PI_ERR_INT_MASK_A), 0);
	SD(LOCAL_HUB(PI_ERR_INT_MASK_B), 0);
	SD(LOCAL_HUB(PI_ERR_STACK_ADDR_A), 0);
	SD(LOCAL_HUB(PI_ERR_STACK_ADDR_B), 0);
	SD(LOCAL_HUB(PI_ERR_STACK_SIZE), 0);
	SD(LOCAL_HUB(PI_SPOOL_CMP_A), 0);
	SD(LOCAL_HUB(PI_SPOOL_CMP_B), 0);
	SD(LOCAL_HUB(PI_CRB_TIMEOUT_A), 0);
	SD(LOCAL_HUB(PI_CRB_TIMEOUT_B), 0);
	SD(LOCAL_HUB(PI_SYSAD_ERRCHK_EN), 0);
	SD(LOCAL_HUB(PI_BAD_CHECK_BIT_A), 0);
	SD(LOCAL_HUB(PI_BAD_CHECK_BIT_B), 0);
	SD(LOCAL_HUB(PI_NACK_CNT_A), 0);
	SD(LOCAL_HUB(PI_NACK_CNT_B), 0);
	SD(LOCAL_HUB(PI_NACK_CMP), 0);
    }

    if (mask & HUB_REGS_MD) {
	SD(LOCAL_HUB(MD_IO_PROTECT), -1);
	SD(LOCAL_HUB(MD_HSPEC_PROTECT), -1);
#if 0
	SD(LOCAL_HUB(MD_MEMORY_CONFIG), MMC_RESET_DEFAULTS);
#endif
	SD(LOCAL_HUB(MD_REFRESH_CONTROL), MRC_RESET_DEFAULTS);
#if 0	/* Using these for scratch regs */
	SD(LOCAL_HUB(MD_MIG_DIFF_THRESH), 0);
	SD(LOCAL_HUB(MD_MIG_VALUE_THRESH), 0);
#endif
	LD(LOCAL_HUB(MD_MIG_CANDIDATE_CLR));
	LD(LOCAL_HUB(MD_DIR_ERROR_CLR));
	LD(LOCAL_HUB(MD_PROTOCOL_ERROR_CLR));
	LD(LOCAL_HUB(MD_MEM_ERROR_CLR));
	LD(LOCAL_HUB(MD_MISC_ERROR_CLR));
	SD(LOCAL_HUB(MD_MOQ_SIZE), MMS_RESET_DEFAULTS);
	SD(LOCAL_HUB(MD_MLAN_CTL), MLAN_RESET_DEFAULTS);
    }
#endif

    /*
     * The MD_PERF_SEL and MD_PERF_CNTx registers are cleared at
     * the very beginning of the PROM boot so they can be used
     * as scratch registers.  Must not clear them here.
     */

#if 0
    /* System was dying at graphics PAGE_A store */

    if (mask & HUB_REGS_GFX) {
	SD(LOCAL_HUB(PI_GFX_PAGE_A), 0);
	SD(LOCAL_HUB(PI_GFX_CREDIT_CNTR_A), 0);
	SD(LOCAL_HUB(PI_GFX_BIAS_A), 0);
	SD(LOCAL_HUB(PI_GFX_INT_CNTR_A), 0);
	SD(LOCAL_HUB(PI_GFX_INT_CMP_A), 0x10000);
	SD(LOCAL_HUB(PI_GFX_PAGE_B), 0);
	SD(LOCAL_HUB(PI_GFX_CREDIT_CNTR_B), 0);
	SD(LOCAL_HUB(PI_GFX_BIAS_B), 0);
	SD(LOCAL_HUB(PI_GFX_INT_CNTR_B), 0);
	SD(LOCAL_HUB(PI_GFX_INT_CMP_B), 0x10000);
    }
#endif
#endif
}

/*
 * hub_access_regs
 *
 *   Accesses all of the regs and throws away the results.
 *   By looking at the RTL trace, this can be used to verify the
 *   reset defaults for all of the registers.
 */

void
hub_access_regs(int mask)
{
#ifndef SABLE
    if (mask & HUB_REGS_PI) {
	LD(LOCAL_HUB(PI_CPU_PROTECT));
	LD(LOCAL_HUB(PI_IO_PROTECT));
	LD(LOCAL_HUB(PI_REGION_PRESENT));
	LD(LOCAL_HUB(PI_CPU_NUM));
	LD(LOCAL_HUB(PI_CALIAS_SIZE));
	LD(LOCAL_HUB(PI_MAX_CRB_TIMEOUT));
	LD(LOCAL_HUB(PI_CRB_SFACTOR));
	LD(LOCAL_HUB(PI_CPU_PRESENT_A));
	LD(LOCAL_HUB(PI_CPU_PRESENT_B));
	LD(LOCAL_HUB(PI_CPU_ENABLE_A));
	LD(LOCAL_HUB(PI_CPU_ENABLE_B));
	LD(LOCAL_HUB(PI_INT_PEND0));
	LD(LOCAL_HUB(PI_INT_PEND1));
	LD(LOCAL_HUB(PI_INT_MASK0_A));
	LD(LOCAL_HUB(PI_INT_MASK1_A));
	LD(LOCAL_HUB(PI_INT_MASK0_B));
	LD(LOCAL_HUB(PI_INT_MASK1_B));
	LD(LOCAL_HUB(PI_CC_PEND_CLR_A));
	LD(LOCAL_HUB(PI_CC_PEND_CLR_B));
	LD(LOCAL_HUB(PI_CC_MASK));

	LD(LOCAL_HUB(PI_RT_LOCAL_CTRL));
	LD(LOCAL_HUB(PI_RT_FILTER_CTRL));
	LD(LOCAL_HUB(PI_RT_COUNT));
	LD(LOCAL_HUB(PI_RT_COMPARE_A));
	LD(LOCAL_HUB(PI_RT_COMPARE_B));
	LD(LOCAL_HUB(PI_PROFILE_COMPARE));

	LD(LOCAL_HUB(PI_RT_PEND_A));
	LD(LOCAL_HUB(PI_RT_PEND_B));
	LD(LOCAL_HUB(PI_PROF_PEND_A));
	LD(LOCAL_HUB(PI_PROF_PEND_B));
	LD(LOCAL_HUB(PI_RT_EN_A));
	LD(LOCAL_HUB(PI_RT_EN_B));
	LD(LOCAL_HUB(PI_PROF_EN_A));
	LD(LOCAL_HUB(PI_PROF_EN_B));

	LD(LOCAL_HUB(PI_ERR_INT_PEND));
	LD(LOCAL_HUB(PI_ERR_INT_MASK_A));
	LD(LOCAL_HUB(PI_ERR_INT_MASK_B));
	LD(LOCAL_HUB(PI_ERR_STACK_ADDR_A));
	LD(LOCAL_HUB(PI_ERR_STACK_ADDR_B));
	LD(LOCAL_HUB(PI_ERR_STACK_SIZE));
	LD(LOCAL_HUB(PI_ERR_STATUS0_A));
	LD(LOCAL_HUB(PI_ERR_STATUS1_A));
	LD(LOCAL_HUB(PI_ERR_STATUS0_B));
	LD(LOCAL_HUB(PI_ERR_STATUS1_B));
	LD(LOCAL_HUB(PI_SPOOL_CMP_A));
	LD(LOCAL_HUB(PI_SPOOL_CMP_B));
	LD(LOCAL_HUB(PI_CRB_TIMEOUT_A));
	LD(LOCAL_HUB(PI_CRB_TIMEOUT_B));
	LD(LOCAL_HUB(PI_SYSAD_ERRCHK_EN));
	LD(LOCAL_HUB(PI_BAD_CHECK_BIT_A));
	LD(LOCAL_HUB(PI_BAD_CHECK_BIT_B));
	LD(LOCAL_HUB(PI_NACK_CNT_A));
	LD(LOCAL_HUB(PI_NACK_CNT_B));
	LD(LOCAL_HUB(PI_NACK_CMP));
    }

    if (mask & HUB_REGS_MD) {
	LD(LOCAL_HUB(MD_IO_PROTECT));
	LD(LOCAL_HUB(MD_HSPEC_PROTECT));
	LD(LOCAL_HUB(MD_MEMORY_CONFIG));
	LD(LOCAL_HUB(MD_REFRESH_CONTROL));
	LD(LOCAL_HUB(MD_FANDOP_CAC_STAT));
	LD(LOCAL_HUB(MD_MIG_DIFF_THRESH));
	LD(LOCAL_HUB(MD_MIG_VALUE_THRESH));
	LD(LOCAL_HUB(MD_MIG_CANDIDATE));
	LD(LOCAL_HUB(MD_MIG_CANDIDATE_CLR));
	LD(LOCAL_HUB(MD_DIR_ERROR));
	LD(LOCAL_HUB(MD_DIR_ERROR_CLR));
	LD(LOCAL_HUB(MD_PROTOCOL_ERROR));
	LD(LOCAL_HUB(MD_PROTOCOL_ERROR_CLR));
	LD(LOCAL_HUB(MD_MEM_ERROR));
	LD(LOCAL_HUB(MD_MEM_ERROR_CLR));
	LD(LOCAL_HUB(MD_MISC_ERROR));
	LD(LOCAL_HUB(MD_MISC_ERROR_CLR));
	LD(LOCAL_HUB(MD_MOQ_SIZE));
	LD(LOCAL_HUB(MD_MLAN_CTL));

	LD(LOCAL_HUB(MD_PERF_SEL));
	LD(LOCAL_HUB(MD_PERF_CNT0));
	LD(LOCAL_HUB(MD_PERF_CNT1));
	LD(LOCAL_HUB(MD_PERF_CNT2));
	LD(LOCAL_HUB(MD_PERF_CNT3));
	LD(LOCAL_HUB(MD_PERF_CNT4));
	LD(LOCAL_HUB(MD_PERF_CNT5));
    }

#if 0
    if (mask & HUB_REGS_GFX) {
	LD(LOCAL_HUB(PI_GFX_PAGE_A));
	LD(LOCAL_HUB(PI_GFX_CREDIT_CNTR_A));
	LD(LOCAL_HUB(PI_GFX_BIAS_A));
	LD(LOCAL_HUB(PI_GFX_INT_CNTR_A));
	LD(LOCAL_HUB(PI_GFX_INT_CMP_A));
	LD(LOCAL_HUB(PI_GFX_PAGE_B));
	LD(LOCAL_HUB(PI_GFX_CREDIT_CNTR_B));
	LD(LOCAL_HUB(PI_GFX_BIAS_B));
	LD(LOCAL_HUB(PI_GFX_INT_CNTR_B));
	LD(LOCAL_HUB(PI_GFX_INT_CMP_B));
    }
#endif

    if (mask & HUB_REGS_NI) {
	LD(LOCAL_HUB(NI_STATUS_REV_ID));
	LD(LOCAL_HUB(NI_PORT_RESET));
	LD(LOCAL_HUB(NI_PROTECTION));
	LD(LOCAL_HUB(NI_GLOBAL_PARMS));
	LD(LOCAL_HUB(NI_SCRATCH_REG0));
	LD(LOCAL_HUB(NI_SCRATCH_REG1));
	LD(LOCAL_HUB(NI_DIAG_PARMS));
	LD(LOCAL_HUB(NI_VECTOR_PARMS));
	LD(LOCAL_HUB(NI_VECTOR));
	LD(LOCAL_HUB(NI_VECTOR_DATA));
	LD(LOCAL_HUB(NI_VECTOR_STATUS));
	LD(LOCAL_HUB(NI_RETURN_VECTOR));
	LD(LOCAL_HUB(NI_VECTOR_READ_DATA));
	LD(LOCAL_HUB(NI_IO_PROTECT));
	LD(LOCAL_HUB(NI_AGE_CPU0_MEMORY));
	LD(LOCAL_HUB(NI_AGE_CPU0_PIO));
	LD(LOCAL_HUB(NI_AGE_CPU1_MEMORY));
	LD(LOCAL_HUB(NI_AGE_CPU1_PIO));
	LD(LOCAL_HUB(NI_AGE_GBR_MEMORY));
	LD(LOCAL_HUB(NI_AGE_GBR_PIO));
	LD(LOCAL_HUB(NI_AGE_IO_MEMORY));
	LD(LOCAL_HUB(NI_AGE_IO_PIO));
	LD(LOCAL_HUB(NI_PORT_PARMS));
	LD(LOCAL_HUB(NI_PORT_ERROR));
	LD(LOCAL_HUB(NI_PORT_ERROR_CLEAR));
    }

    if (mask & HUB_REGS_CORE) {
	LD(LOCAL_HUB(CORE_MD_ALLOC_BW));
	LD(LOCAL_HUB(CORE_ND_ALLOC_BW));
	LD(LOCAL_HUB(CORE_ID_ALLOC_BW));
	LD(LOCAL_HUB(CORE_ARB_CTRL));
	LD(LOCAL_HUB(CORE_REQQ_DEPTH));
	LD(LOCAL_HUB(CORE_REPQ_DEPTH));
	LD(LOCAL_HUB(CORE_FIFO_DEPTH));
    }
#endif
}
#endif /* RTL */

/*
 * msg_sleep
 *
 *   Makes the LEDs alternate a value and its complement to indicate
 *   the CPU is waiting on something.  Should be called the first time
 *   with (*next == 0) to initialize.
 */

#define MSG_CHK_INTERVAL	1000		/* usec */
#define MSG_LED_INTERVAL	100000		/* usec */

static void msg_sleep(__uint64_t *led, rtc_time_t *next)
{
    rtc_sleep(MSG_CHK_INTERVAL);

    if (*next == 0)
	*next = rtc_time();

    if (rtc_time() > *next) {
	*next += MSG_LED_INTERVAL;
	hub_led_set(*led);
	*led ^= 0xff;
    }
}

/*
 * hub_barrier
 *
 *   Closely synchronizes the CPUs on the local node at a barrier point.
 *   The algorithm follows.  The INT_BARRIER_A and INT_BARRIER_B interrupt bits
 *   are used for synchronization variables A and B.  They are both assumed
 *   to be 0 before using barriers.
 *
 *	A = 1;			while (A == 0);
 *	while (B == 0);		B = 1;
 *	A = 0;			while (A == 1);
 *	while (B == 1);		B = 0;
 *
 *   Does nothing if only one CPU is active, and breaks out if either
 *   CPU becomes disabled during the barrier.
 */

void
hub_barrier(void)
{
    rtc_time_t		next	= 0;
    __uint64_t		led	= 0x55;

    if (hub_cpu_get() == 0) {
	hub_int_set(INT_BARRIER_A);

	while (! hub_int_test(INT_BARRIER_B) && hub_alive(1))
	    msg_sleep(&led, &next);

	hub_int_clear(INT_BARRIER_A);

	while (hub_int_test(INT_BARRIER_B) && hub_alive(1))
	    msg_sleep(&led, &next);
    } else {
	while (! hub_int_test(INT_BARRIER_A) && hub_alive(0))
	    msg_sleep(&led, &next);

	hub_int_set(INT_BARRIER_B);

	while (hub_int_test(INT_BARRIER_A) && hub_alive(0))
	    msg_sleep(&led, &next);

	hub_int_clear(INT_BARRIER_B);
    }
}

/*
 * primary_lock (internal)
 *
 *   Allows CPU A and CPU B to mutually exclude the hub from one another by
 *   obtaining a blocking lock.  Does nothing if only one CPU is active.
 *
 *   Implements Peterson's Algorithm for two-process exclusion.  Utilizes
 *   three interrupt bits as communication variables.
 *
 *   This lock should be held just long enough to set or clear a global
 *   lock bit.  After a relatively short timeout period, this routine
 *   figures something is wrong, steals the lock, and sets the other CPU
 *   to "dead" so barriers and locks will no longer wait for it.
 */

static void
primary_lock(void)
{
    rtc_time_t		next	= 0;
    __uint64_t		led	= 0xf0;
    rtc_time_t		expire;

    if (hub_num_local_cpus() > 1) {
	expire = rtc_time() + PRIMARY_LOCK_TIMEOUT;

	if (hub_cpu_get() == 0) {
	    hub_int_set(INT_WANT_A);
	    hub_int_set(INT_FAVOR_B);

	    while (hub_int_test(INT_WANT_B) && hub_int_test(INT_FAVOR_B)) {
		msg_sleep(&led, &next);
		if (rtc_time() > expire) {
		    hub_alive_set(1, 0);
		    hub_int_clear(INT_FAVOR_B);
		}
	    }
	} else {
	    hub_int_set(INT_WANT_B);
	    hub_int_clear(INT_FAVOR_B);

	    while (hub_int_test(INT_WANT_A) && ! hub_int_test(INT_FAVOR_B)) {
		msg_sleep(&led, &next);
		if (rtc_time() > expire) {
		    hub_alive_set(0, 0);
		    hub_int_set(INT_FAVOR_B);
		}
	    }
	}
    }
}

/*
 * primary_unlock (internal)
 *
 *   Counterpart to primary_lock
 */

static void
primary_unlock(void)
{
    if (hub_cpu_get() == 0)
	hub_int_clear(INT_WANT_A);
    else
	hub_int_clear(INT_WANT_B);
}

/*
 * hub_lock_timeout
 *
 *   Uses primary_lock to implement multiple lock levels.
 *
 *   There are 20 lock levels from 0 to 19 (limited by the number of bits
 *   in HUB_LOCK_REG).  To prevent deadlock, multiple locks should be
 *   obtained in order of increasingly higher level, and released in the
 *   reverse order.
 *
 *   A timeout value of 0 may be used for no timeout.
 *
 *   Returns 0 if successful, -1 if lock times out.
 */

int
hub_lock_timeout(int level, rtc_time_t timeout)
{
    __uint64_t		mask	= 1ULL << level;
    rtc_time_t		expire	= (timeout ?
				   rtc_time() + timeout :
				   RTC_TIME_MAX);
    int			done	= 0;

    while (! done) {
	while (TST_BITS(HUB_LOCK_REG, mask)) {
	    if (rtc_time() > expire)
		return -1;
	    if (! hub_alive(1 - hub_cpu_get()))
		return 0;
	}

	primary_lock();

	if (! TST_BITS(HUB_LOCK_REG, mask)) {
	    SET_BITS(HUB_LOCK_REG, mask);
	    done = 1;
	}

	primary_unlock();
    }

    return 0;
}

/*
 * hub_lock
 *
 *   Like hub_lock_timeout, with infinite timeout value.
 */

void
hub_lock(int level)
{
    (void) hub_lock_timeout(level, 0);
}

/*
 * hub_unlock
 *
 *   Counterpart to hub_lock_timeout and hub_lock
 */

void
hub_unlock(int level)
{
    __uint64_t		mask	= 1ULL << level;

    primary_lock();

    CLR_BITS(HUB_LOCK_REG, mask);

    primary_unlock();
}

/*
 * PROM's access routine for Hub
 */

static int hub_nic_access(nic_data_t data,
			  int pulse, int sample, int delay)
{
    __uint64_t		value;

    *(volatile __uint64_t *) data =
	nic_get_phase_bits() | MCR_PACK(pulse, sample);

    do
	value = *(volatile __uint64_t *) data;
    while (! MCR_DONE(value));

    rtc_sleep(delay);

    return MCR_DATA(value);
}

/*
 * hub_nic_get
 *
 *   Reads a Hub's NIC and stores the result in nicp.  The caller
 *   must insure multiple CPUs can't try to read the NIC at once.
 *
 *   Returns 0 on success, -1 on failure (and stores ~0UL into nicp).
 */

int
hub_nic_get(nasid_t nasid, int verbose, __uint64_t *nicp)
{
    nic_state_t		ns;
    uchar_t		pageA[32];
    uchar_t		pageB[32];
    char		family[1];
    char		serial[6];
    char		crc[1];
    char		buf[64];
    int			r = -1;
	char		hub_nic_info[1024] ;

        char *nic_string;
        *hub_nic_info = 0;

    hub_lock(HUB_LOCK_NIC);

#if 0
    if (nic_setup(&ns,
		  hub_nic_access,
		  (nic_data_t) REMOTE_HUB(nasid, MD_MLAN_CTL)))
	goto done;

    *nicp = 0;	    /* Clear two MS bytes */

    if (nic_next(&ns, (char *) nicp + 2, family, crc)) {
	*nicp = ~0UL;
	goto done;
    }

#endif
	get_hub_nic_info((nic_data_t)REMOTE_HUB(nasid, MD_MLAN_CTL), 
			  hub_nic_info) ;
		if ((nic_string = strstr(hub_nic_info,"IP27")) ||
			(nic_string = strstr(hub_nic_info,"IP31")))
		{
			if(nic_string = strstr(nic_string,"Laser:"))
			{
					
					nic_string += 6;
					*nicp = strtoull(nic_string, 0, 16);
			}
	        }
		else
                {
			*nicp = ~0UL;
			goto done;
		}

    if (verbose) {
#if 0
	/* Since family is not used before this point */
	printf("Family 0x%02x\n", family[0]);
#endif
    if (nic_setup(&ns,
		  hub_nic_access,
		  (nic_data_t) REMOTE_HUB(nasid, MD_MLAN_CTL)))
	goto done;

    *nicp = 0;	    /* Clear two MS bytes */

    if (nic_next(&ns, (char *) nicp + 2, family, crc)) {
	*nicp = ~0UL;
	goto done;
    }


	if (nic_read_mfg(&ns,family,serial,crc,pageA,pageB))
	    printf("NIC mfg. data not available\n");
	else {
	    printf("--> Found family 0x0b Mfg. Board data:\n");
	    if (pageA[0] != 0x01) {
		printf("NIC mfg. page 'A' data not available\n");
	    } else {
		memcpy(buf, pageA + 1, 10);
		buf[10] = 0;
		printf("Serial #: %10.10s\n", buf);
		memcpy(buf, pageA + 11, 19);
		buf[19] = 0;
		printf("Part #: %s", buf);
	    }
	    if (pageB[0] != 0x20) {
		printf("\nNIC mfg. page 'B' data not available\n");
	    } else {
		memcpy(buf, pageB + 0, 6);
		buf[6] = 0;
		printf("%s  ", buf);
		memcpy(buf, pageB + 6, 6);
		buf[6] = 0;
		printf("Revision: %s\n", buf);
		printf("Group: 0x%02x  Capability: 0x%02x%02x%02x%02x "
		       "Variety: 0x%02x\n",
		       pageB[10],pageB[11],pageB[12],pageB[13],pageB[14],
		       pageB[15]);
		memcpy(buf, pageB + 16, 14);
		buf[14] = 0;
		printf("Part Name: %s\n", buf);
	    }
	}
    }

    r = 0;

 done:

    hub_unlock(HUB_LOCK_NIC);

    return r;
}
