/***********************************************************************\
*	File:		entry.s						*
*									*
*	This file contains the exception vectors and startup code for	*
*	the T5 CPU.  Much code was lifted from IP25 prom.               *
*									*
*	- Initializes the registers, TLB, and the caches		*
*	- Initializes the Hub chip					*
*	- Arbitrates for the master status between 2 CPUs on the node	*
*									*
\***********************************************************************/

#ident	"$Revision: 1.115 $"

#include <asm.h>

#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/immu.h>

#include <sys/SN/SN0/IP27.h>
#include <sys/SN/agent.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/gda.h>
#include <sys/SN/launch.h>
#include <sys/SN/nmi.h>

#include "ip27prom.h"
#include "hub.h"
#include "pod.h"
#include "prom_leds.h"
#include "tlb.h"

#ifdef	PANIC
#   undef	PANIC
#endif

#define	PANIC(code, string) MESSAGE(a1, string); li a0,code; j bevPanic; nop

#define HALT	jal hub_halt; nop

/*
 * Define:	LA_EXCEPTION_DEBUG
 * Purpose: 	Dump some C0 registers to a read-only hub pi register.
 *		to aid in debugging using the Logic Analyzer.
 *		Trigger on writes to 01000020.
 * Parameters:	None
 * Notes:	Dont use any registers besides k0, k1.
 */

#define LA_EXCEPTION_DEBUG						\
	dli	k0, LOCAL_HUB(PI_CPU_NUM);				\
	mfc0	k1, C0_CAUSE;	/* Write useful info over SYSAD   */	\
	sd	k1, (k0);	/* for viewing on logic analyzer. */	\
	dmfc0	k1, C0_EPC;	/* The Hub ignores the write.     */	\
	sd	k1, (k0);	/* Trigger on address 01000020.   */	\
	dmfc0	k1, C0_BADVADDR;					\
	sd	k1, (k0);						\

/*
 * DELAY_IF_TRUE(reg)
 *
 *   If reg != 0, inserts a small delay using k1 as a counter.
 */

#define DELAY_IF_TRUE(reg)						\
	beqz	reg, 99f;						\
	 li	k1, 128;						\
98:	bnez	k1, 98b;						\
	 sub	k1, 1;							\
99:

/*
 * Jim Laudon says: "You need to do a write to the UREG0 range
 * before you do any write operations on the MISCIO bus
 * (including FPROM writes) to make sure the part comes up in
 * 80xx mode (as per the note on page 47 of the PCF8584
 * spec)."  That would include LED writes.
 *
 * This must be done at least thirty 12-MHz cycles (2.5 usec) after
 * reset.  Empirically, the entire PCF8584 initialization must be
 * done -- the hub will hang if all we do is store 0x80 followed
 * by 0xc0 into the control register!
 *
 * We assume CPU A and B are synchronized closely after a reset or
 * NMI of both CPUs.  CPU A and B must not reset the I2C simultaneously,
 * and it must still work if only one CPU is present.  This is done by
 * judicious use of delays.
 */

#define RESET_I2C							\
	li	k0, 1;							\
	DELAY_IF_TRUE(k0);		/* Delay > 2.5 usec */		\
	dli	k0, LOCAL_HUB(PI_CPU_NUM);				\
	ld	k0, 0(k0);						\
	DELAY_IF_TRUE(k0);		/* Delay if CPU B */		\
	dli	k0, LOCAL_HUB(KL_I2C_REG);	/* Reset and program */	\
	li	k1, 0x80;						\
	sd	k1, 8(k0);						\
	li	k1, 0x7f;						\
	sd	k1, 0(k0);						\
	li	k1, 0xa0;						\
	sd	k1, 8(k0);						\
	li	k1, 0x1c;						\
	sd	k1, 0(k0);						\
	li	k1, 0xc0;						\
	sd	k1, 8(k0);						\
	dli	k0, LOCAL_HUB(PI_CPU_NUM);				\
	ld	k0, 0(k0);						\
	xor	k0, 1;							\
	DELAY_IF_TRUE(k0);		/* Delay if CPU A */		\
	li	k0, 1;							\
	DELAY_IF_TRUE(k0)		/* Delay on both */


	.text
	.set	noreorder

/*
 * entry -- When the CPU begins executing it jumps
 *	through the power-on vector to this point.
 *	This routine initializes the processor and starts
 *	basic system configuration.
 */

#if defined HUB_ERR_STS_WAR
	/*
	 * This aligns the entry at 2^11 which is 2k and avoids the
	 * uncached addresses at 0x4XX. Otherwise, on NMI, the error
	 * status registers will get cleared when we fetch prom
	 * instructions.
	 */

	.align	11
#endif

LEAF(entry)
	.set	noat

	/*
	 * Do not touch any MD junk bus registers (such as LEDs) until
	 * the I2C dummy write is done!
	 */

	li	k0, 0			/* Paranoid about uninited T5 */
	li	k1, 0			/* Paranoid about uninited T5 */

	/*
	 * Get and clear the SR immediately.  We are in 32-bit mode
	 * until the SR is adjusted below.  Hub regs can't be accessed
	 * in 32-bit mode!
	 */

	MFC0(k0, C0_SR)
	li	k1, PROM_SR

	MTC0(k1, C0_SR)			/* Put in a good SR value */
	MTC0(k0, C0_LLADDR)		/* Save the old SR */

	SET_PROGRESS_SHADOW(PLED_RESET_OK)

#if defined(RTL) || defined(GATE)
	LA_EXCEPTION_DEBUG
	li	k0, 0
#endif

	/*
	 * Check if we're here because of an NMI (both NMI and SR are on).
	 */

	MFC0(k0, C0_LLADDR)		/* Get saved C0_SR */
	li	k1, SR_NMI|SR_SR
	and	k0, k1
	bne	k0, k1, bevNmiNone	/* Skip if not */
	 nop

	dli	k0, LOCAL_HUB(MD_PERF_CNT0)	/* Clear hub_locks */
	sd	zero, 0(k0)

	/*
	 * Reset the I2C in case we were NMI'd while holding the bus.
	 */

#if !defined(SABLE) && !defined(RTL) && !defined(GATE)
	RESET_I2C
#endif

	/*
	 * Indicate NMI on LEDS, using k0/k1 only.
	 */

#define NMI_LED_DELAY	27746		/* 1/8 second (1/4 if two CPUs) */

	HUB_LED_SET(0x3f)

	dli	k0, NMI_LED_DELAY
1:	bnez	k0, 1b
	 add	k0, -1

	HUB_LED_SET(0xcf)		/* Show next 2 LEDs */

	dli	k0, NMI_LED_DELAY
1:	bnez	k0, 1b
	 add	k0, -1

	HUB_LED_SET(0xf3)		/* Show next 2 LEDs */

	dli	k0, NMI_LED_DELAY
1:	bnez	k0, 1b
	 add	k0, -1

	HUB_LED_SET(0xfc)		/* Show next 2 LEDs */

	dli	k0, NMI_LED_DELAY
1:	bnez	k0, 1b
	 add	k0, -1

	HUB_LED_SET(PLED_NMI)		/* Show PLED_NMI */

	/*
	 * Jump to the CPU PROM NMI handler if the EPC was in range of the
	 * CPU PROM in COMPAT K1 space, the CPU PROM in mapped space, or
	 * the IO6 PROM in mapped space.  Otherwise, jump to Kernel handler.
	 *
	 * The CPU PROM uses a distinctive mapped address range
	 * (IP27PROM_BASE_MAPPED, c00000001fc00000).
	 */

	DMFC0(k0, C0_ERROR_EPC)
	dli	k1, 0xffffffffffe00000		/* Check 2 MB range */
	and	k0, k1
	dli	k1, 0xffffffffbfc00000
	beq	k0, k1, bevNmiProm
	 nop
	dli	k1, IP27PROM_BASE_MAPPED
	beq	k0, k1, bevNmiProm
	 nop
	dli	k1, IO6PROM_BASE_MAPPED
	beq	k0, k1, bevNmiProm
	 nop

	b	bevNmiKernel
	 nop

bevNmiNone:

	/*
	 * If we're here because of a soft reset, jump to soft reset handler.
	 */

	li	k1, SR_SR
	and	k0, k1
	bnez	k0, bevSr
	 nop

	SET_PROGRESS_SHADOW(PLED_PRE_I2C_RESET)
        /*
         * Check if the R10K config registers match with the proms config
         * register bits
         * N.B.: We write it once and check to see if the bits are fine.
         * This writes the K0segcc bits atleast and prevents the config
         * bits mismatch problem
         */

         dli    k0, IP27CONFIG_ADDR
         daddu  k0, ip27c_r10k_mode
         lw     k1,  0(k0)
         MTC0(k1, C0_CONFIG)

         andi   k1,  k1, 0xffff
         MFC0(k0, C0_CONFIG)
         andi   k0,  k0, 0xffff
         bne	k0, k1, diff_mode
	 nop

         dli    k0, IP27CONFIG_ADDR
         daddu  k0, ip27c_r10k_mode
         lw     k1,  0(k0)
	 srl	k1,  k1, 16
         andi   k1,  k1, 0x1ff
         MFC0(k0, C0_CONFIG)
	 srl	k0,  k0, 16
         andi   k0,  k0, 0x1ff
         beq	  k0, k1, ok_mode
	 nop


diff_mode:

	 SET_PROGRESS(FLED_MODEBIT)
	 HALT

ok_mode :

#if !defined(SABLE) && !defined(RTL) && !defined(GATE)
	RESET_I2C
#endif

	SET_PROGRESS_SHADOW(PLED_POST_I2C_RESET)

	.set	at

	/*
	 * Power up sequence or hard reset.
	 * Need to perform full initialization.
	 *
	 * NI_SCRATCH_REG0 advertises our NIC to the outside world.
	 * If this value is left at 0 (reset default), other nodes will
	 * assume we're a headless node.  We set this value to -1 to
	 * indicate we're in the process of coming up.  Other Hubs will
	 * wait for us to set our NIC (see main.c).
	 */

	dli	v0, LOCAL_HUB(NI_SCRATCH_REG0)
	li	v1, -1
	sd	v1, 0(v0)

	/*
	 * Due to a problem with reset propagation outside a module,
	 * we need to do two hub resets whenever the first one comes from
	 * the system controller.  We can't tell which resets came from the
	 * SN0net and which came from the system controller so we always
	 * do two resets.
  	 *
	 * We use the PI_ERR_STACK_ADDR_B register to determine if this is
	 * the first or second reset.  After the first reset, we set
	 * PI_ERR_STACK_ADDR_B to "Reset0" and after the second one, we
	 * clear it again.
	 *
	 * It doesn't matter which CPU does this and there's no race here.
	 *
	 */

	dli	v0, LOCAL_HUB_ADDR(PI_ERR_STACK_ADDR_B)
	ld	a0, (v0)
	dli	a1, 0x52737430		/* "Rst0" */
	beq	a0, a1, 1f
	 nop

	/*
 	 * It wasn't Reset1 so we're on the first reset.  Do another.
	 * First store Reset1 into the register, then do a reset
	 */
	sd	a1, (v0)
	sync				/* Make sure this gets out. */

	dli	v0, LOCAL_HUB_ADDR(NI_PORT_RESET)
	li	v1, NPR_PORTRESET | NPR_LOCALRESET
	sd	v1, (v0)		/* Reset! */

	SET_PROGRESS(FLED_RESETWAIT)
	HALT

1:

	/*
	 * Go ahead with normal initialization. We'll clear PI_ERR_STACK_ADDR_B
	 * after local arbitration so we don't end up with extra resets
	 */

#if 0
	/* Turn on debug signals */

	dli	a0, LOCAL_HUB(0x2000c0)
	li	a1, 0xb
	sd	a1, 0(a0)
	ld	a2, 0(a0)
	andi	a2, 0x0f
	beq	a1, a2, 2f
	 nop

	SET_PROGRESS(FLED_COREDEBUG)
	HALT
2:
#endif

#ifdef RTL				/* Hang CPU B if present */
	dli	v0, LOCAL_HUB(0)
	ld	v0, PI_CPU_NUM(v0)
1:	bnez	v0, 1b
	 nop
#endif /* RTL */

#if !defined(RTL) && !defined(GATE)
	jal	initializeCPU		/* Clears BSR */
	 nop
#endif

	DMTBR(zero, BR_BSR)

	DMFC0(k0, C0_ERROR_EPC)		/* Squirrel away ERROR_EPC */
	DMTC1(k0, $f31)			/* on hard reset */

	SET_PROGRESS(PLED_IORESET)
	/*
	 * We need to reset the ii asap, otherwise we need to wait for
	 * a long period of time later.
	 * WARNING: moving this will possibly screw up things later on.
	 */
	jal	ii_early_init
	 nop

	SET_PROGRESS(PLED_RUNTLB)
#if !defined(RTL) && !defined(GATE)
	jal	tlb_prom
	 nop
#endif

	/*
	 * Assign CALIAS size
	 */

	dli	v0, LOCAL_HUB(0)
	li	v1, 1			/* CALIAS_SIZE = 4 KB */
	sd	v1, PI_CALIAS_SIZE(v0)

	/*
	 * Start the real-time clock going ASAP since it's used for
	 * timeouts in I2C driver, etc.  This is done by both CPUs.
	 * There should be no ill effects if they collide.
	 */

	SET_PROGRESS(PLED_RTCINIT)
	jal	rtc_init
	 nop
	SET_PROGRESS(PLED_RTCINITDONE)

	/*
	 * Test and invalidate primary caches
	 */

#if !defined(RTL) && !defined(GATE)
	dli	v0, LOCAL_HUB_ADDR(PROMOP_REG)
	ld	v0, (v0)
	 nop

	/*
 	 * Don't use the promop bits if the magic number's not right.
	 */

	li	a0, PROMOP_MAGIC_MASK
	and	a0, v0
	li	a1, PROMOP_MAGIC
	bne	a0, a1, 1f
	 nop

	and	v0, PROMOP_SKIP_DIAGS
	bnez	v0, end_of_pcache_diags
	 nop

1:

	SET_PROGRESS(PLED_TESTICACHE)
	jal	cache_test_i
	 nop
	beqz	v0, 1f
	 nop
	SET_PROGRESS(FLED_ICACHE)	/* Can't continue -- no UART */
	HALT
1:
	SET_PROGRESS(PLED_TESTDCACHE)
	jal	cache_test_d
	 nop
	beqz	v0,1f
	 nop
	SET_PROGRESS(FLED_DCACHE)	/* Can't continue -- no UART */
	HALT

1:
end_of_pcache_diags:
	SET_PROGRESS(PLED_INVICACHE)
	jal	cache_inval_i
	 nop
	SET_PROGRESS(PLED_INVDCACHE)
	jal	cache_inval_d
	 nop
	SET_PROGRESS(PLED_INVSCACHE)
	jal	cache_inval_s
	 nop

	/*
	 * Get the cache stack ready for use.
	 */

	SET_PROGRESS(PLED_MAKESTACK)
	dli	a0, PROMDATA_VADDR
	dli	a1, PROMDATA_PADDR
	dli	a2, PROMDATA_SIZE
	jal	cache_dirty
	 nop

	DMFBR(v0, BR_BSR)
	or	v0, BSR_DEX
	DMTBR(v0, BR_BSR)

	dli	sp, POD_STACKVADDR + POD_STACKSIZE

#else /* !RTL */

	dla	gp, _gp
	dli	sp, 0xa800000000100000

	HUB_CPU_GET()
	beqz	v0, 1f
	 nop
	daddu	sp, -(POD_STACKSIZE / 2)
1:

#endif /* !RTL */

	/*
	 * Transfer control to C
	 */

	SET_PROGRESS(PLED_MAIN)
	jal	main
	 nop

	HUB_LED_SET(FLED_MAINRET)
	HALT

	END(entry)

/*
 * redo_bsrs
 *
 *   If we are coming from the Kernel and want to enter POD mode,
 *   we have to make sure the BSRs are cleared to something
 *   reasonable.
 */

LEAF(redo_bsrs)
	move	s0, ra

	/*
	 * XXX fix setting BSR global master, uart parms, etc.
	 */

	DMTBR(zero, BR_BSR)
	DMTBR(zero, BR_NOFAULT)
	DMTBR(zero, BR_ASM_HANDLER)
	DMTBR(zero, BR_LIBC_DEVICE)
	DMTBR(zero, BR_IOC3UART_BASE)

	j	s0
	 nop
	END(redo_bsrs)

/*
 * Function:	bevRestartMaster
 * Purpose:	entry point for restart from BEV vector. This is
 *		called when the OS wants to reboot or return
 *		from the prom monitor.
 * Parameters:	none
 * Returns:	Doesn't
 */

LEAF(bevRestartMaster)
	jal	initializeCPU
	 nop
	jal	tlb_prom
	 nop

	jal	initializeIP27
	 nop

	jal	run_dex
	 nop

#if 0
	jal	io6_initmaster
	 nop
	jal	init_ioc3_uart
	 nop
#endif

	MESSAGE(a0, DEFAULT_SEGMENT)
	jal	load_execute_segment
	 li	a1, 1

	HUB_LED_FLASH(FLED_RESTART)

	li	a0, -POD_MODE_DEX
	li	a1, KLDIAG_DEBUG
	MESSAGE(a2, "RestartMaster can't load IO6prom")

	jal	pod_mode
	 nop

	HALT

	END(bevRestartMaster)

/*
 * Function:	bevSlaveLoop
 * Parameters:	entry point for restart from BEV vector. This is
 *		called by the slave CPUs.
 * Parameters:	none
 * Returns:	nothing
 */

LEAF(bevSlaveLoop)
	jal	initializeCPU
	 nop
	jal	tlb_prom
	 nop
#if 0
	jal	initializeIP27
	 nop
#endif

	j	launch_loop
	 nop				/* Does not return */
	END(bevSlaveLoop)

/*
 * Function:	bevLaunchSlave
 * Parameters:	entry point for launch from BEV vector. This is
 *		called by anyone: IP27prom, IO6prom, Kernel, MDK, etc.
 * Parameters:	none
 * Returns:	nothing
 */

LEAF(bevLaunchSlave)
	j	launch_slave		/* Transfer control & return */
	 nop
	END(bevLaunchSlave)

/*
 * Function:	bevWaitSlave
 * Parameters:	entry point for wait from BEV vector. This is
 *		called by anyone: IP27prom, IO6prom, Kernel, MDK, etc.
 * Parameters:	none
 * Returns:	nothing
 */

LEAF(bevWaitSlave)
	j	launch_wait		/* Transfer control & return */
	 nop
	END(bevWaitSlave)

/*
 * Function:	bevPollSlave
 * Parameters:	entry point for poll from BEV vector. This is
 *		called by anyone: IP27prom, IO6prom, Kernel, MDK, etc.
 * Parameters:	none
 * Returns:	nothing
 */

LEAF(bevPollSlave)
	j	launch_poll		/* Transfer control & return */
	 nop
	END(bevPollSlave)

/*
 * Function: 	bevRestartMasterEPC
 * Purpose:	To restart POD using the IOC3, ignoring all the other
 *		stuff.
 * Parameters:	none
 * Returns:	Never.
 */

LEAF(bevRestartMasterEPC)
	jal	initializeCPU
	 nop
	jal	tlb_prom
	 nop
	jal	initializeIP27
	 nop

	DMFBR(t0, BR_BSR)
	or	t0,BSR_PODDEX+BSR_NODIAG
	DMTBR(t0, BR_BSR)

	li	a0, POD_MODE_DEX
	li	a1, KLDIAG_DEBUG
	MESSAGE(a2, "Software entry into POD mode from IO6 POD mode")

	jal	pod_mode
	 nop

	HALT

	END(bevRestartMasterEPC)

/*
 * Function:	bevPrintf
 * Purpose:	Print a string to the console.
 * Parameters:	string
 * Returns:	Nothing
 */

LEAF(bevPrintf)
	j	printf
	 nop
	END(bevPrintf)

/*
 * Function:	bevPodMode
 * Purpose:	To put the current CPU into POD.  Allows symmon or
 *		whatever to jump back into pod.
 * Parameters:	None
 * Returns:	Nothing
 */

LEAF(bevPodMode)
	jal	initializeCPU
	 nop
	jal	initializeIP27
	 nop

	li	a0, POD_MODE_DEX
	li	a1, KLDIAG_DEBUG
	MESSAGE(a2, "Software entry into POD mode")

	jal	pod_mode
	 nop

	HALT

	END(bevPodMode)

/*
 * Function: 	initializeCPU
 * Purpose:	Initializes the T5 registers to a known state.
 * Parameters:	None
 * Returns:	Nothing
 */

LEAF(initializeCPU)
	move	t1,ra
	li	v0,PROM_SR		/* Our expected SR */
	MTC0(v0, C0_SR)			/* In known state */

	SET_PROGRESS(PLED_INITCPU)	/* set startup leds */

	/*
	 * Initialize the busy-bit table in 2 steps:
	 * Step 1: Write all registers
	 */

	.set	noat
	add	$at, $0, $0	;	  mtc1	  $0, $f0
	add	$2, $0, $0	;	  mtc1	  $0, $f1
	add	$3, $0, $0	;	  mtc1	  $0, $f2
	add	$4, $0, $0	;	  mtc1	  $0, $f3
	add	$5, $0, $0	;	  mtc1	  $0, $f4
	add	$6, $0, $0	;	  mtc1	  $0, $f5
	add	$7, $0, $0	;	  mtc1	  $0, $f6
	add	$8, $0, $0	;	  mtc1	  $0, $f7
	add	$9, $0, $0	;	  mtc1	  $0, $f8
	add	$10, $0, $0	;	  mtc1	  $0, $f9
	add	$11, $0, $0	;	  mtc1	  $0, $f10
	add	$12, $0, $0	;	  mtc1	  $0, $f11
	/* $13 = ret. addr. */	;	  mtc1	  $0, $f12
	add	$14, $0, $0	;	  mtc1	  $0, $f13
	add	$15, $0, $0	;	  mtc1	  $0, $f14
	add	$16, $0, $0	;	  mtc1	  $0, $f15
	add	$17, $0, $0	;	  mtc1	  $0, $f16
	add	$18, $0, $0	;	  mtc1	  $0, $f17
	add	$19, $0, $0	;	  mtc1	  $0, $f18
	add	$20, $0, $0	;	  mtc1	  $0, $f19
	add	$21, $0, $0	;	  mtc1	  $0, $f20
	add	$22, $0, $0	;	  mtc1	  $0, $f21
	add	$23, $0, $0	;	  mtc1	  $0, $f22
	add	$24, $0, $0	;	  mtc1	  $0, $f23
	add	$25, $0, $0	;	  mtc1	  $0, $f24
	add	$26, $0, $0	;	  mtc1	  $0, $f25
	add	$27, $0, $0	;	  mtc1	  $0, $f26
	add	$28, $0, $0	;	  mtc1	  $0, $f27
	add	$29, $0, $0	;	  mtc1	  $0, $f28
	add	$30, $0, $0	;	  mtc1	  $0, $f29
	add	$31, $0, $0	;	  mtc1	  $0, $f30
	mtc1	$0, $f31

	mult	$1, v0		# for hi lo registers;
	.set	at

	/*
	 * Step 2: Initialize other half of busy bit table.
	 */

	li	v0, 32
1:
	mtc1	zero, $f0
	sub	v0, 1
	bgez	v0, 1b
	 nop

	/* Initialize the co-proc 0 registers */

	MTC0(zero, C0_CAUSE)		/* 32-bit */
	MTC0(zero, C0_WATCHLO)
	MTC0(zero, C0_WATCHHI)
	MTC0(zero, C0_TLBWIRED)
	MTC0(zero, C0_ECC)

	DMTC0(zero, C0_TLBLO_0)		/* 64 bit */
	DMTC0(zero, C0_TLBLO_1)
	DMTC0(zero, C0_CTXT)
	DMTC0(zero, C0_COMPARE)
	DMTC0(zero, C0_EXTCTXT)

	/*
	 * Test coprocessor 1 registers that we plan to use...
	 */
	SET_PROGRESS(PLED_TESTCP1)
	DMTBR(zero, BR_BSR)		/* Boot status register */
	DMFBR(v0, BR_BSR)		/* Read it back */
	beq	v0,zero,1f		/* skip panic if OK */
	 nop
	SET_PROGRESS(FLED_CP1)		/* Dead coproc # 1 */
	HALT
1:	dli	v0,0xa5a5a5a5a5a5a5a5	/* Another try */
	DMTBR(v0, BR_BSR)
	DMFBR(t0, BR_BSR)
	beq	v0,t0,1f		/* checks out ok */
	 nop
	SET_PROGRESS(FLED_CP1)
	HALT
1:

	/*
	 * Assume Coproc #1 (boot registers) is working, and initialize
	 * all the registers.
	 */

	DMTBR(zero, BR_BSR)
	DMTBR(zero, BR_NOFAULT)
	DMTBR(zero, BR_ASM_HANDLER)
	DMTBR(zero, BR_IOC3UART_BASE)
	DMTBR(zero, BR_ELSC_SPACE)

	jal	tlb_flush
	 nop

	dla	gp, _gp			/* Set up for calling C */

	j	t1			/* t1 contains ra */
	 nop
	END(initializeCPU)

/*
 * Function: 	initializeIP27
 * Purpose:	To initialize the IP27 to a known state after diags
 *		have been run or the kernel returned control to
 *		us.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses a, t, v, s0
 */

LEAF(initializeIP27)
	move	s0,ra

	HUB_LED_SET(PLED_INVICACHE)
	jal	cache_inval_i
	 nop
	HUB_LED_SET(PLED_INVDCACHE)
	jal	cache_inval_d
	 nop
	HUB_LED_SET(PLED_INVSCACHE)
	jal	cache_inval_s
	 nop
#if 0
	move	a0, zero
	move	a1, zero
	jal	hub_int_set_all		/* Clear hub interrupts */
	 nop
	jal	clear_hub_ccs		/* Clear hub cross-CPU ints */
	 nop
#endif

	j	s0
	 nop
	END(initializeIP27)


/*
 * A reset does not propogate to the ii. This will send a warm reset to
 * the ii llp from one of the cpus on the node.
 */
LEAF(ii_early_init)
	/*
         * Need to reset the ii. Do this on all cpus on this node since
	 * we haven't done arbitration yet.
	 */
	dli	v0, LOCAL_HUB(IIO_ILCSR);
	ld	v1, 0(v0)
	ori	v1, ILCSR_WARM_RESET
	sd	v1, 0(v0)
	jr	ra
	 nop
	END(ii_early_init)

/*
 * Function:	bevSr
 * Purpose:	Process a soft reset
 * Parameters:	none, assumes entered from boot vector.
 * Returns:	doesn't
 */

LEAF(bevSr)
	/* XXX fixme */

#if !defined(SABLE) && !defined(RTL) && !defined(GATE)
	RESET_I2C
#endif

	j	ra
	 nop
	END(bevSr)

/*
 * The other cpu wants us to take over pod mode. Presumably, this
 * is after an exception or NMI, so we have some useful data to
 * print out.
 */

LEAF(SwitchHandler)
	li	a0, POD_MODE_CAC
	li	a1, KLDIAG_PASSED
	MESSAGE(a2, "Taking over pod mode")

	jal	pod_mode
	 nop

	HALT

	END(SwitchHandler)

/*
 * Function:	bevNmiProm
 * Purpose:	Process an NMI interrupt received while in the CPU PROM.
 * Parameters:	none, assumes entered from boot vector.
 * Returns:	doesn't
 */

LEAF(bevNmiProm)
	.set	noat

	/*
	 * Be sure FPU is enabled
	 */

	DMFC0(a0, C0_SR)

	# Save SR to LED
	HUB_LED_SET(a0)

	dli	k0, PROM_SR
	#
	# Don't OR with existing SR.
	# or	k0, k1

	DMTC0(k0, C0_SR)
	nop; nop;

	dmtc1	ra, RA_FP
	jal	saveGprs
	 nop

	.set	at

	DMFBR(a0, BR_BSR)
	and	a0, BSR_DEX	/* Don't flush cache if already in DEX mode */
	bnez	a0, 1f
	 nop
	jal	cache_flush	/* run_dex will inval caches to set up stack */
	 nop			/* we don't want to lose our cache contents  */
1:

	jal	tlb_prom
	 nop

	li	a0, POD_MODE_DEX	/* Better to always go to DEX? */
	li	a1, KLDIAG_NMIPROM
	MESSAGE(a2, "NMI while in PROM")

	jal	pod_mode
	 nop

	HALT

	END(bevNmiProm)

/*
 * Function:	bevNmiKernel
 * Purpose:	Process an NMI interrupt received while not in the CPU PROM.
 * Parameters:	none, assumes entered from boot vector.
 * Returns:	doesn't
 */

LEAF(bevNmiKernel)
	.set	noat

	/*
	 * Be sure FPU is enabled XXX should save old SR
	 */

	dli	k0, PROM_SR
	MTC0(k0, C0_SR)

	dmtc1	ra, RA_FP
	jal	saveGprs
	 nop

	/*
	 * We're going to try to access memory, so we set up to trap
	 * any exception that may occur.
	 */

	DMTBR(zero, BR_NOFAULT)		/* Turn off no fault */
	dla	k0, bevNmiFailedAccess	/* Jump-to on exception */
	dli	k1, COMPAT_K1BASE	/* Make sure we jump into the real*/
	or	k0, k1			/* prom			*/
	DMTBR(k0, BR_ASM_HANDLER)	/* Set up assembler handler */

	/*
	 * Compute the address of our NMI structure (using only k0/k1/AT_FP).
	 */

	dli	k0, LOCAL_HUB(NI_STATUS_REV_ID)	/* Get node ID in k0 */
	ld	k0, (k0)
	dsrl	k0, NSRI_NODEID_SHFT
	and	k0, (NSRI_NODEID_MASK >> NSRI_NODEID_SHFT)
	dsll	k0, NASID_SHFT		/* Point to node's NMI structure */
	dli	k1, K1BASE | IP27_NMI_OFFSET
	or	k0, k1
	dli	k1, LOCAL_HUB(PI_CPU_NUM)	/* Add stride for CPU B */
	ld	k1, 0(k1)
	beqz	k1, 1f
	 nop
	dli	k1, IP27_NMI_STRIDE
1:
	daddu	k0, k1
	ld	k1, NMI_OFF_MAGIC(k0)	/* Verify magic number */

	dli	AT, NMI_MAGIC
	xor	k1, AT

	DMTBR(zero, BR_ASM_HANDLER)

	bnez	k1, bevNmiCorrupt
	 nop
	ld	k1, NMI_OFF_CALL(k0)	/* Call address */

	ld	AT, NMI_OFF_CALLC(k0)	/* Call address complement */
	xor	AT, k1
	daddiu	AT, 1
	bnez	AT, bevNmiSecond	/* Verify complement is correct */
	 nop
	beqz	k1, 1f			/* No vector set up */
	 nop

	dli	a0, LOCAL_HUB(NI_STATUS_REV_ID)	/* Get node ID in a0 */
	ld	a0, (a0)
	dsrl	a0, NSRI_NODEID_SHFT
	and	a0, (NSRI_NODEID_MASK >> NSRI_NODEID_SHFT)
	dsll	a0, NASID_SHFT
	dli	a1, K1BASE | IP27_NMI_KREGS_OFFSET
	or	a0, a1
	dli	a1, LOCAL_HUB(PI_CPU_NUM)
	ld	a1, 0(a1)
	beq	a1, zero, 2f
	 nop
	daddiu	a0, IP27_NMI_KREGS_CPU_SIZE
2:
	jal	store_gprs
	 nop

	jal	restoreGprs
	 nop

	dmfc1	ra, RA_FP
	sd	zero, NMI_OFF_CALLC(k0)
	jr	k1			/* Off to OS land.... */
	 nop

1:
	/*
	 * No NMI vector set.  Send the slaves to the slave loop and the
	 * master to POD mode.  The NMI area conveniently tells us if
	 * we're the global master.
	 */

	jal	restoreGprs
	 nop
#if 0
	dmfc1	ra, RA_FP
	ld	k1, NMI_OFF_GMASTER(k0)
	bnez	k1, bevNmiMaster
	 nop
	j	launch_loop
	 nop
#else
	b	bevNmiMaster
	 nop
#endif

	.set	at

bevNmiFailedAccess:

	jal	tlb_prom
	 nop
	jal	redo_bsrs
	 nop

	/*
	 * pod_loop() determines console availability and sends the
	 * appropriate processors to the slave loop.
	 */

#if 1
	/* DPRINT("fixme: clear memory access error") */
#endif

	li	a0, POD_MODE_DEX
	li	a1, KLDIAG_NMIBADMEM
	MESSAGE(a2, "NMI while in Kernel and memory inaccessible")

	jal	pod_mode
	 nop

	HALT

bevNmiCorrupt:

	jal	tlb_prom
	 nop
	jal	redo_bsrs
	 nop

	li	a0, POD_MODE_DEX
	li	a1, KLDIAG_NMICORRUPT
	MESSAGE(a2, "NMI while in Kernel and NMI vectors corrupt")

	jal	pod_mode
	 nop

	HALT

bevNmiMaster:
	jal	tlb_prom
	 nop
	jal	redo_bsrs
	 nop

	li	a0, POD_MODE_DEX
	li	a1, KLDIAG_NMI
	MESSAGE(a2, "NMI while in Kernel and no NMI vector installed")

	jal	pod_mode
	 nop

	HALT

	END(bevNmiKernel)

/*
 * Function:    bevNmiSecond
 * Purpose:     Process a second NMI or a corrupt NMI complement
 * Parameters:  none, assumes entered from boot vector.
 * Returns:     doesn't
 */

LEAF(bevNmiSecond)
	jal	tlb_prom
	 nop
	jal	redo_bsrs
	 nop
	jal	cache_flush	/* run_dex will inval caches to set up stack */
	 nop			/* we don't want to lose our cache contents  */

	li	a0, POD_MODE_DEX
	li	a1, KLDIAG_NMISECOND
	MESSAGE(a2, "Second NMI received")

	jal	pod_mode
	 nop

	HALT

	END(bevNmiSecond)

/*
 * Function:	bevPanic
 * Purpose:	An exception occurred without a nofault handler in place.
 * Parameters:	a0 - panic code (KLDIAG_*)
 *		a1 - pointer to panic string
 *
 * Exception handlers (besides NMI) may only be reached from the PROM
 * once the Kernel clears the BEV bit and installs its own handlers.
 * This is the usual case, so all handlers assume the BSRs are valid.
 * There are occasions when the kernel may crash and come here before
 * it's installed its handlers.
 */

LEAF(bevPanic)
	move	s1, a0			/* Save panic code */
	move	s2, a1			/* Save panic string */

	/*
	 * If the exception happened in the PROM, go to dirty exclusive
	 * (DEX) mode and display the POD prompt.  Note that this trashes
	 * the cache.  It was deemed more important to actually get to
	 * POD mode than to keep the cache.
	 *
	 * If the exception happened in the kernel, go to uncached POD
	 * mode and display the POD prompt.  The method for determining
	 * we're coming from the kernel is the same as used by bevNmi.
	 *
	 * pod_loop() determines console availability and sends the
	 * appropriate processors to the slave loop.
	 */

	DMFC0(k0, C0_EPC)
	dli	k1, 0xffffffffffe00000		/* Check 2 MB range */
	and	k0, k1
	dli	k1, 0xffffffffbfc00000
	beq	k0, k1, bevPanicProm
	 nop
	dli	k1, IP27PROM_BASE_MAPPED
	beq	k0, k1, bevPanicProm
	 nop
	dli	k1, IO6PROM_BASE_MAPPED
	beq	k0, k1, bevPanicProm
	 nop

bevPanicKernel:
	li	a0, POD_MODE_UNC
	move	a1, s1
	move	a2, s2

	jal	pod_mode
	 nop

	HALT

bevPanicProm:
	li	a0, POD_MODE_DEX
	move	a1, s1
	move	a2, s2

	jal	pod_mode
	 nop

	HALT

	END(bevPanic)

/*
 * Function: 	bevFlashLeds
 * Purpose:	Entry point to allow kernel to request leds flash
 * Parameters:	None
 * Returns:	Never
 */

LEAF(bevFlashLeds)
	HUB_LED_FLASH(FLED_OS)

	jal	initializeCPU
	 nop

	li	a0, POD_MODE_DEX
	li	a1, KLDIAG_DEBUG
	MESSAGE(a2, "Entering POD mode after flashing LEDs");

	jal	pod_mode
	 nop

	HALT

	END(bevFlashLeds)

/*
 * Define:	BEV_PROLOGUE
 * Purpose: 	Defines the initial instructions of the vectored handlers.
 * Notes:	In order to avoid trashing the registers saved in the fprs,
 * 		we need to check for an asm fault handler immediately when
 * 		we enter each of the exception handlers.  If one isn't set,
 * 		we save the ra and then proceed.  This stuff is written as
 * 		macro so that we won't lose the ra.
 */

#define	BEV_PROLOGUE							\
	.set	noat;							\
	MFC0(k0, C0_SR);						\
	li	k1, SR_CU1|SR_FR|SR_EXL;				\
	or	k0, k1;		/* EXL keeps us from ruining EPC in */	\
	MTC0(k0, C0_SR);	/* the event of a second exception! */	\
	LA_EXCEPTION_DEBUG;						\
	DMFBR(k0, BR_ASM_HANDLER);					\
	DMTBR(zero, BR_ASM_HANDLER);					\
	beqz	k0, 90f;						\
	 nop;								\
	j	k0;							\
	 nop;								\
90:	DMTC1(ra, RA_FP);						\
	DMFBR(k0, BR_NOFAULT);						\
	beqz	k0, 93f;						\
	 nop;								\
	.set	at;							\
92:	move	a0, k0;							\
	MFC0(k0, C0_SR);						\
	ori	k0, SR_EXL;						\
	xori	k0, SR_EXL;						\
	MTC0(k0, C0_SR);						\
	j	longjmp;						\
	 li	a1, 1;		/* Tell setjmp where we came from */	\
	.set	noat;							\
93:	jal	saveGprs;						\
	nop;								\
	.set	at;							\
	ld	k1,	LOCAL_HUB(NI_SCRATCH_REG1);			\
	dli	k0,	ADVERT_EXCP_MASK;				\
	or	k1,	k0;						\
	sd	k1,	LOCAL_HUB(NI_SCRATCH_REG1);			\
94:	jal	tlb_prom;						\
	 nop

/*
 * Function:	bevGeneral
 * Purpose:	Vector entry point for PROM general exception vector.
 * Parameters:	As per exceptions
 * Returns:	Nothing
 */

LEAF(bevGeneral)
	BEV_PROLOGUE
	PANIC(KLDIAG_EXC_GENERAL, "General Exception")
	END(bevGeneral)

/*
 * Function:	bevECC
 * Purpose:	BEV exception handler for PROM ECC exception
 * Parameters:	none
 * Returns:	Does not return, unless ASM handler is set.
 */

LEAF(bevECC)
	BEV_PROLOGUE
	PANIC(KLDIAG_EXC_ECC, "ECC Exception")
	END(bevECC)

/*
 * Function:	bevTlbRefill
 * Purpose:	BEV exception handler for PROM TLB refill expection
 * Parameters:	none
 * Returns:	Panics, unless ASM handler set.
 */

LEAF(bevTlbRefill)
	BEV_PROLOGUE
	PANIC(KLDIAG_EXC_TLB, "TLB Refill Exception")
	END(bevTlbRefill)

/*
 * Function:	bevXtlbRefill
 * Purpose:	BEV exception handler for PROM XTLB refill expection
 * Parameters:	none
 * Returns:	Panics, unless ASM handler set.
 */

LEAF(bevXtlbRefill)
	BEV_PROLOGUE
	PANIC(KLDIAG_EXC_XTLB, "XTLB Refill Exception")
	END(bevXtlbRefill)

/*
 * Function:	notimplemented
 * Purpose:	process a an unimplemented exception
 * Parameters:	none
 * Returns:	does not return
 */

LEAF(notimplemented)
	BEV_PROLOGUE
	PANIC(KLDIAG_EXC_UNIMPL, "Unimplemented Exception")
	END(notimplemented)

/*
 * Function:	bevRePod
 * Purpose:	To return to pod Uncached
 * Parameters:	none
 * Returns:	nothing
 */

LEAF(bevRePod)
	BEV_PROLOGUE
	li	a0, POD_MODE_DEX
	li	a1, KLDIAG_DEBUG
	MESSAGE(a2, "bevRePod entry into POD mode")
	jal	pod_mode
	 nop
	HALT
END(bevRePod)

/*
 * Function:	bevCache
 * Purpose:	PROM BEV cache error handler
 * Parameters:	none
 * Returns:	does not return
 */

LEAF(bevCache)
	BEV_PROLOGUE
	MFC0(a0, C0_CACHE_ERR)
	dli	a1,CE_TYPE_MASK
	and	a0,a1
	dli	a1,CE_TYPE_SIE
	beq	a0,a1,1f
	nop
	PANIC(KLDIAG_EXC_CACHE, "Cache Error Exception")
1:	PANIC(KLDIAG_EXC_CACHE, "System Interface Error")
	END(bevCache)
