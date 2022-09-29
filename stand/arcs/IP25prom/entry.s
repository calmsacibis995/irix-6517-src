/*
 * File: entry.s
 * Purpose: Entry points for boot mode vector - BEV vector jumps to 
 * 	    routines in this file.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident	"$Revision: 1.10 $"

#include <asm.h>

#include <sys/regdef.h>
#include <sys/sbd.h>

#include <sys/EVEREST/IP25.h>
#include <sys/EVEREST/IP25addrs.h>
#include <sys/EVEREST/gda.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evconfig.h>

#include "ip25prom.h"
#include "prom_intr.h"
#include "prom_leds.h"
#include "pod.h"

#ifdef	PANIC
#   undef	PANIC
#endif

#define	PANIC(code, string) MESSAGE(a1, string);  li a0,code; j bevPanic; nop

#define	SETPC()	dla k0,27f;j k0;nop;27:
	
		.text
		.set 	noreorder
		.set	noat
#define	SPIN()	jal	__spin;  nop
	
/*
 * entry -- When the CPU begins executing it jumps
 *	through the power-on vector to this point.
 *	This routine initializes the processor and starts 
 *	basic system configuration.
 */
LEAF(entry)
	.set	at
	
	/* Check for NMI. */

	MFC0(k0, C0_SR)
	and	k0,SR_SR		/* Was it NMI? */
	bnez	k0,bevNmi		/*  let NMI handler take it */
	nop
	
	li	v0,PROM_SR		/* Our expected SR */
	MTC0(v0, C0_SR)			/* In known state */
	SETPC()

	LEDS(PLED_STARTUP)
	SPIN()

	jal	initializeCPU		/* Initialize the CPU */
	nop

	/*
	 * Until we arbitrate, assume we are a slave. That way, we will 
	 * never try to jump to pod, with a stack in the cache.
	 */
	dli	t0, BSR_SLAVE
	DMTBR(t0, BR_BSR)

	/*
	 * Configure the CC UART.
	 */
	LEDS(PLED_UARTINIT)
	jal	ccuart_init		/* configure the UART */
	nop				/* DELAY */
	LEDS(PLED_CCUARTDONE)

	/*
 	 * Now do some testing ....
	 */
	jal	testIcache
	nop
	beqz	v0,1f
        nop
	FLASH(FLED_ICACHE)

1:	jal	testDcache
	nop
	beqz	v0,1f
	FLASH(FLED_DCACHE)
1:	
#if 0
	LEDS(PLED_CKCCLOCAL)		/* Check local CC registers */
	jal	pod_check_cclocal	
	nop				/* DELAY */
#endif
	jal	pod_check_ccconfig	/* Check CC config registers */
	nop				/* DELAY */

	jal	initializeIP25
	nop				/* DELAY */

	EV_GET_SPNUM(t0, t1)
	EV_SET_PROCREG(t0, t1, EV_CFG_PGBRDEN, zero) /* no piggies */
	
	/*
	 * Compute SCC cache tag mask ---- use the SR because it has the
	 * log2 value as it stands.
	 */

	MFC0(v0,C0_CONFIG)
	and	v0,CONFIG_SS
	srl	v0,CONFIG_SS_SHFT

	/*
	 * SCC needs to know the size of the secondary cache at
	 * this point to correctly mask bits and generate
	 * Parity.
	 */	
	li	v1,5
	sub	v0,v1,v0			/* SCC value:CACHE_SZ */
	EV_GET_SPNUM(k0,k1)			/* our address */
	EV_SET_PROCREG(k0, k1, EV_CFG_CACHE_SZ, v0)

	DMFBR(t0, BR_BSR)		/* set boot status register */
	or	t0, BSR_CCUI
	DMTBR(t0, BR_BSR)
	
	/*
	 * Print PROM header now that we can talk to the world.
	 */

	DPRINT("\r\n\nEVEREST IP25 PROM ");
	MFC0(t0, C0_CONFIG)
	and	t0,CONFIG_BE
	bnez	t0,1f
	nop				/* DELAY */
	DPRINT("(LE)\r\n")
	b	2f
	nop				/* DELAY */
1:	
	DPRINT("(BE)\r\n")
2:

	/*
	 * Check and do initial configuration of the A chip. In order
	 * to avoid conflicts between processors as they test their
	 * connection with the A chip, we delay based on our location.
	 */
	DPRINT("Testing A chip ... \t\t\t");
	dli	a0, EV_SPNUM
	ld	a0,0(a0)
	and	a0,EV_PROCNUM_MASK
	jal	delay
	sll	a0,11			/* 2048 * slot # */
	LEDS(PLED_CKACHIP)
	jal	pod_check_achip		/* Check out Mr. A */
	nop
	DPRINT("ok\r\n");
	
	LEDS(PLED_AINIT)
	DPRINT("Initializing A chip ...\t\t\t");
	EV_GET_SPNUM(t0,zero)		# Get slot number
	EV_GET_CONFIG(t0, EV_A_ERROR_CLEAR, t1)
	EV_SET_CONFIG(t0, EV_A_RSC_TIMEOUT, RSC_TIMEOUT)
	EV_SET_CONFIG(t0, EV_A_URGENT_TIMEOUT, URG_TIMEOUT)
	DPRINT("ok\r\n")

	/*
	 * Check the EBUS.
	 */
	LEDS(PLED_CKEBUS1)
	DPRINT("Testing EBUS ...\t\t\t")
	jal	pod_check_ebus1
	nop
	beqz	v0, 1f			/* Keep going if OK */
	nop
	DPRINT("failed\r\n")
	FLASH(PLED_CKEBUS1)
	
1:	DPRINT("ok\r\n")
	
	/*
	 * Perform Boot master arbitration. If this processor is
 	 * the Boot Master, jump to the master processor thread of
	 * execution.  Otherwise, jump to the slave thread of execution.
	 */
	j	arb_bootmaster		/* Do actual arb */
	nop
	/* DOESN'T RETURN */
	END(entry)

/*
 * Function:	bevRestartMaster
 * Purpose:	entry point for restart from BEV vector. This is
 *		called when the OS wants to reboot or return
 *		from the prom monitor.
 * Parameters:	none
 * Returns:	Doesn't
 */
LEAF(bevRestartMaster)
	SETPC()
	jal	initializeCPU
	nop
	SCPRINT("Re-initing caches")
	jal	initializeIP25
	nop
	SCPRINT("Rebuilding stack")
	jal	initDcacheStack
	nop
	SCPRINT("Re-initing IO4")
	jal	io4_initmaster
	nop
	SCPRINT("Re-initing EPC UART")
	jal	init_epc_uart
	nop
	DMFBR(t0, BR_BSR)
	or	t0,BSR_EPCUART
	DMTBR(t0, BR_BSR)
	SCPRINT("Re-loading IO4 PROM")
	jal	load_io4prom
	nop
	FLASH(FLED_RESTART)
	END(bevRestartMaster)
/*
 * Function:	bevRestartSlave
 * Parameters:	entery point for restart from BEV vector. This is
 *		called by the slave CPUs.
 * Parameters:	none
 * Returns:	nothing
 */
LEAF(bevRestartSlave)
	.set	noreorder
	jal	initializeCPU
	nop
	jal	initializeIP25
	nop
	li	v0,BSR_SLAVE
	DMTBR(v0, BR_BSR)
	j	prom_slave
	nop
	.set	reorder
 	END(bevRestartSlave)

/*
 * Function: 	bevRestartMasterEPC
 * Purpose:	To restart POD using the EPC, ignoring all the other
 *		stuff.
 * Parameters:	none
 * Returns:	Never.
 */
LEAF(bevRestartMasterEPC)
	.set	noreorder
	jal	initializeCPU
	nop
	jal	initializeIP25
	nop
	li	t0,BSR_CCUI+BSR_EPCUART+BSR_POD+BSR_NODIAG
	DMTBR(t0, BR_BSR)
	j	prom_master		/* No arbitration here */
	nop
	.set reorder
	END(bevRestartMasterEPC)

/*
 * Function:	bedPodMode
 * Purpose:	To put the current CPU into POD. Allows symmon or
 *		whatever to jump back into pod.
 * Parameters:	None
 * Returns:	Nothing
 */
LEAF(bevPodMode)
	jal	initializeCPU
	jal	initializeIP25
	DMFBR(t0,BR_BSR)
	or	t0,BSR_CCUI
	DMTBR(t0,BR_BSR)
	MESSAGE(a1, "POD mode")
	li	a0,EVDIAG_DEBUG
	jal	podMode
	END(bevPodMode)

/*
 * Function: 	initializeCPU
 * Purpose:	Initializes the T5 registers to a known state.
 * Parameters:	None
 * Returns:	Nothing
 * Notes:	uses t0,v0,a0
 */
LEAF(initializeCPU)
	.set	noreorder
	move	t1,ra
	li	v0,PROM_SR		/* Our expected SR */
	MTC0(v0, C0_SR)			/* In known state */
	LEDS(PLED_STARTUP)		/* set startup leds */

	/*
	 * Initialize the busy-bit table in 2 steps:
	 *	1:
	 *	2:
	 */
	.set	noat	
        add	$at, $0, $0	;         mtc1    $0, $f0
        add 	$2, $0, $0	;         mtc1    $0, $f1
        add 	$3, $0, $0	;         mtc1    $0, $f2
        add 	$4, $0, $0	;         mtc1    $0, $f3
        add 	$5, $0, $0	;         mtc1    $0, $f4
        add 	$6, $0, $0	;         mtc1    $0, $f5
        add 	$7, $0, $0	;         mtc1    $0, $f6
        add 	$8, $0, $0	;         mtc1    $0, $f7
        add 	$9, $0, $0	;         mtc1    $0, $f8
        add 	$10, $0, $0	;         mtc1    $0, $f9
        add 	$11, $0, $0	;         mtc1    $0, $f10
        add 	$12, $0, $0	;         mtc1    $0, $f11
        /*add 	$13, $0, $0*/	;         mtc1    $0, $f12
        add 	$14, $0, $0	;         mtc1    $0, $f13
        add 	$15, $0, $0	;         mtc1    $0, $f14
        add 	$16, $0, $0	;         mtc1    $0, $f15
        add 	$17, $0, $0	;         mtc1    $0, $f16
        add 	$18, $0, $0	;         mtc1    $0, $f17
        add 	$19, $0, $0	;         mtc1    $0, $f18
        add 	$20, $0, $0	;         mtc1    $0, $f19
        add 	$21, $0, $0	;         mtc1    $0, $f20
        add 	$22, $0, $0	;         mtc1    $0, $f21
        add 	$23, $0, $0	;         mtc1    $0, $f22
        add 	$24, $0, $0	;         mtc1    $0, $f23
        add 	$25, $0, $0	;         mtc1    $0, $f24
        add 	$26, $0, $0	;         mtc1    $0, $f25
        add 	$27, $0, $0	;         mtc1    $0, $f26
        add 	$28, $0, $0	;         mtc1    $0, $f27
        add 	$29, $0, $0	;         mtc1    $0, $f28
        add 	$30, $0, $0	;         mtc1    $0, $f29
        add	$31, $0, $0	;         mtc1    $0, $f30
        mtc1    $0, $f31	        
	mult	$1, v0          # for hi lo registers;

	/*
	 * Step 2: Initialize other half of busy bit table.
	 */

        li      v0, 32
1:
        mtc1    zero, $f0
	sub     v0, 1
        bgez    v0, 1b
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
	 * Test coprocessor 1 registers that we plan to use ...
	 */
	LEDS(PLED_TESTCP1)
	DMTBR(zero, BR_BSR)		/* Boot status register */
	DMTBR(zero, BR_DIAG)		/* Diagnostic value */
	DMFBR(v0, BR_BSR)		/* Read it back */
	beq	v0,zero,1f		/* skip panic if OK */
	nop
	FLASH(FLED_CP1)			/* Dead coproc # 1 */
1:	DMFBR(v0, BR_DIAG)
	beq	v0,zero,1f
	nop
	FLASH(FLED_CP1)
1:	dli	v0,0xa5a5a5a5a5a5a5a5	/* Another try */
	DMTBR(v0, BR_BSR)
	DMTBR(v0, BR_DIAG)
	DMFBR(t0, BR_BSR)
	beq	v0,t0,1f		/* checks out ok */
	nop
	FLASH(FLED_CP1)
1:	DMFBR(t0, BR_DIAG)
	beq	v0,t0,1f
	nop
	FLASH(FLED_CP1)
1:		
	/*
	 * Assume Coproc #1 (boot registers) is working, and initialize 
	 * all the registers.
	 */
	DMTBR(zero, BR_BSR)
	DMTBR(zero, BR_NOFAULT)
	DMTBR(zero, BR_ASMHNDLR)
	DMTBR(zero, BR_DUARTBASE)
	DMTBR(zero, BR_DIAG)
	jal	flushTlb
	nop
	move	ra,t1
	j	ra
	nop
	.set	reorder
	END(initializeCPU)

/* 
 * Function: 	InitializeIP25
 * Purpose:	To initialize the IP25 to a known state after diags
 *		have been run or the kernel returned control to
 *		us.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses a, t, v, s0, at
 */
LEAF(initializeIP25)
	.set	noreorder
	move	s0,ra
	dli	a0,EV_ERTOIP
	ld	a0,0(a0)
	DMTBR(a0,BR_ERTOIP)
	jal	invalidateIDcache	/* Clear Instruction/Data cache */
	nop	
	jal	invalidateScache	/* Clear secondary cache */
	nop
	dli	a0, EV_IGRMASK		/* Turn off all Interrupt Groups */
	sd	zero, 0(a0)		
	dli	a0, EV_ILE		/* Disable interrupts */
	sd	zero, 0(a0)
	dli	a0, EV_CIPL124		/* Clear all level 1,2 ints */
	sd	zero, 0(a0)
	dli	a0, EV_CERTOIP		/* Clear any error states */
        dli	v0,-1
	sd	v0, 0(a0)
	dli	a0, EV_GRCNTL
	dli	v0, EV_GRCNTL_ENABLE+EV_GRCNTL_WS
	sd	v0,0(a0)		/* Enable WG - for accelerated only */
	dli	a0, EV_ERADDR		/* Clear any errors */
	sd	zero,0(a0)

	/*
	 * Figure out what to do about CC configuration. If we are running
	 * with 1 outstanding read, then disable the Abort Logic, since
	 * it is not required, and we are probably running this way because
	 * the abort logic is broken (as in rev D SCC). However, this
	 * affects ONLY rev D and greater SCCs.
	 */
	move	v0,zero			/* Start off with this for now */
	.set	at
	GET_CC_REV(t0,t1)		/* t0 has revision - 0 -> X */
	.set	noat
	sub	t0,3			/* 'A' - 'D' == 3 */
	bltz	t0,1f			/* < Rev D - ignore */
	nop
	
	/* Check if 1 outstanding read */

	MFC0(t0, C0_CONFIG)
	and	t0,CONFIG_PM
	srl	t0,CONFIG_PM_SHFT	/* # requests - 1 */
	bnez	t0,1f			/* More that 1 request */
	nop
	or	v0,EV_ECCSB_DIAL|EV_ECCSB_SWM	/* disable aborts */
1:	
	dli	a0, EV_ECCSB		/* Address */
	sd	v0,0(a0)		/* Bang ! */

	.set	at	
	EV_GET_SPNUM(t0, t1)
	EV_SET_PROCREG(t0, t1, EV_CFG_ECCHKDIS, zero)
	.set	noat
	
	DMTBR(zero, BR_BSR)		/* Clear BSR */
	j	s0
 	nop
	.set	reorder
	END(initializeIP25)
	
/*
 * Function:	bevNmi
 * Purpose:	Process an NMI interrupt
 * Parameters:	none, assumes entered from boot vector.
 * Returns:	doesn't
 */
LEAF(bevNmi)
	.set	noreorder
	/*
	 * Be sure FPU is enabled
	 */
	MFC0(k0, C0_SR)

	dli	k1, PROM_SR
	or	k0, k1

	MTC0(k0, C0_SR)
#if 1	
	li	k0,0xb9004000

	DMFC0(k1,C0_ERROR_EPC)
	sd	k1,8(k0)
	DMFC0(k1,C0_EPC)
	sd	k1,0x10(k0)
	DMFC0(k1,C0_BADVADDR)
	sd	k1,0x18(k0)
	MFC0(k1, C0_CAUSE)
	sd	k1,0x20(k0)
	MFC0(k1, C0_SR)
	sd	k1,0x28(k0)

	sd	AT,0x100(k0)
	sd	v0,0x108(k0)
	sd	v1,0x110(k0)
	sd	a0,0x118(k0)
	sd	a1,0x120(k0)
	sd	a2,0x128(k0)
	sd	a3,0x130(k0)
	sd	a4,0x138(k0)
	sd	a5,0x140(k0)
	sd	a6,0x148(k0)
	sd	a7,0x150(k0)

	sd	t0,0x158(k0)
	sd	t1,0x160(k0)
	sd	t2,0x168(k0)
	sd	t3,0x170(k0)

	sd	s0,0x178(k0)
	sd	s1,0x180(k0)
	sd	s2,0x188(k0)
	sd	s3,0x190(k0)
	sd	s4,0x198(k0)
	sd	s5,0x1a0(k0)
	sd	s6,0x1a8(k0)
	sd	s7,0x1b0(k0)

	sd	t8,0x1c0(k0)
	sd	t9,0x1c8(k0)

	sd	gp,0x1d0(k0)
	sd	sp,0x1d8(k0)
	sd	s8,0x1e0(k0)
	sd	ra,0x1e8(k0)
#endif

	SETPC()

	DMTBR(zero, BR_NOFAULT)		/* Turn off no fault */
	dla	k0, bevNmiFailedAccess	/* Jump-to on exception */
	DMTBR(k0, BR_ASMHNDLR)		/* Set up assembler handler */
	/*
	 * Check if host has NMI vector set that we sould branch to.
	 * If yes, go there. This access may fail, in which case we
	 * branch to the ASMHDLR.
	 */

	dli	k0, GDA_ADDR		/* Global data area */
	lw	k0, G_MAGICOFF(k0)	/* Check magic number */
	li	k1, GDA_MAGIC
	bne	k0,k1,bevNmiBadGDA	/* Hmmm - bummer */
	nop

	dli	k0, GDA_ADDR		/* Reload GDA address */
	lw	k1, G_NMIVECOFF(k0)	/* Load the vector */
	beqz	k1, 2f			/* If vec is zero, just goto pod */
	nop
1:
	dli	k0, EV_SCRATCH
	sd	k1, 0(k0)

	/* Set LEDS using k0/k1 only */
	
	li	k0, PLED_NMI
	dli	k1, EV_LED_BASE
	sd	k0, 0(k1)		/* Set the LEDs to NMI jump value */

	/*
	 * wait for all to come through, then zero and launch.
	 */
	li	k0,1000000
3:	
	bnez	k0,3b
	sub	k0,1
	dli	k0,GDA_ADDR
	sw	zero,G_NMIVECOFF(k0)

	/*
	 * Now on to user ....
	 */
	
	dli	k0, EV_SCRATCH
	ld	k1, 0(k0)
	j	k1			/* Off to OS land .... */
	nop
2:
	/*
	 * Save away our registers ...
	 */
	DMTC1(ra,RA_FP)
	jal	saveGprs
	nop
	
	/*
	 * No NMI vector set, so we go to pod mode - check who is
	 * master.
	 */
	dli	k0,GDA_ADDR
	lw	k0,G_MASTEROFF(k0)	/* pick up master CPU # */
	dli	k1, EV_ERTOIP
	ld	k1, 0(k1)
	DMTBR(k1, BR_ERTOIP)
	dli	k1,EV_SPNUM
	ld	k1,0(k1)		/* Pick up our CPU # */
	beq	k0,k1,bevNmiMaster
	nop
	/*
	 * We are a slave.
	 */
	PMESSAGE("CPU running in slave mode\r\n");
	li	a0,BSR_SLAVE
	DMTBR(a0, BR_BSR)
	j	prom_slave
	nop
	
bevNmiMaster:	
	/*
	 * We are master and an NMI occured. If we are in 
	 * manufacturing mode, use the CC uart, otherwise use the
	 * EPC uart. GDA Address in k0.
	 */
	dli	k0,GDA_ADDR
	lw	k1,G_VDSOFF(k0)
	andi	k1,VDS_MANUMODE		/* if Manumode */
	bnez	k1,3f
	li	a0,BSR_CCUI+BSR_MANUMODE /* DELAY: use CC uart */

	/*
	 * Attempt to use EPC uart. Reload the EPC pointer from the 
	 * GDA and hope it works. 
	 */
	ld	k1,G_EPC(k0)		/* EPC pointer */
	beqz	k1,bevNmiBadGDA
	li	a0,BSR_EPCUART+BSR_CCUI	/*           else EPC uart */
	DMTBR(k1, BR_DUARTBASE)
3:	
	DMTBR(a0,BR_BSR)		/* BSR set up for uart routines */
	MESSAGE(a1,"\r\n\n*** Non-maskable interrupt occurred. \r\n\n")
	j	podMode
	ori	a0,EVDIAG_NMI

bevNmiFailedAccess:
	/*
	 * We land here if the memory access above failed, in that
	 * case clear memory access error.
	 */
	dli	k0,EV_CERTOIP
	li	k1,0xffffffff /* CC_ERROR_MY_ADDR %%% */
	sd	k1, 0(k0)
	
	DMFBR(a0, BR_BSR)		/* Don't use EPC serial port */
	li	a1, BSR_EPCUART
	not	a1
	and	a0,a1
	DMTBR(a0, BR_BSR)

	SCPRINT("IP25Prom: NMI handler exception")
	MESSAGE(a1, "\r\n\n*** NMI Interrupt (Exception handler)")
	j	podMode
	move	a0,zero

bevNmiBadGDA:
	/*
	 * We land here if the memory access worked, but the GDA appears
	 * botched. All we can do is Flash the leds at this point.
	 */
	li	a0,BSR_CCUI		/* Let it at least try CC uart */
	DMTBR(a0,BR_BSR)		/* BSR set up for uart routines */
	FLASH(FLED_BADGDA)
	.set	reorder
	END(bevNmi)

/*
 * Function:	bevPanic
 * Purpose:	Panic, or longjump back to the pod goober that set up
 *	   	for this.
 * Parameters:	a0 - panic code
 *		a1 - pointer to panic string
 *		a2 - diag code (used by sc_disp in pod_loop).
 * Returns:	
 */
LEAF(bevPanic)
	.set	noreorder
	move	s0,a0			/* Save panic code */
	move	s1,a1			/* Save panic string */

	/*
	 * Check if the nofault vector is set, and if so, long jump
	 * back to where ever ...
	 */

	DMFBR(a0, BR_NOFAULT)
	beqz	a0,1f			/* Not set, oh-oh */
	nop				/* DELAY */
	DMTBR(zero, BR_NOFAULT)
	j	longjmp
	ori	a1,zero,1		/* DELAY: Tell program */
1:
	/* No handler set, so do normal processing now */

	DMFBR(a0, BR_BSR)
	andi	a0,BSR_SLAVE		/* Check if we are slave */
	beqz	a0,2f
	nop
	FLASH(s0)			/* Flash error */
2:	
	move	a1,s1	
	j	podMode
	ori	a0,zero, EVDIAG_PANIC	/* pass panic code */

	.set	reorder	
	END(bevPanic)

/*
 * Function: 	bevFlashLeds
 * Purpose:	Entery point to allow kernel to request leds flash
 * Parameters:	None
 * Returns:	Never
 */
LEAF(bevFlashLeds)
	FLASH(FLED_OS)
	END(bevFlashLeds)

/*
 * Define:	BEV_LEAF
 * Purpose: 	Defines the first "N" instructions of the vectored
 * 		handlers, and the entry point.
 * Parameters:	r - function name
 *		_X - Bits to turn o in the SR.
 * Notes:	In order to avoid trashing the registers saved in the fprs,
 * 		we need to check for an asm fault handler immediately when
 * 		we enter each of the exception handlers.  If one isn't set,
 * 		we save the ra and then proceed.  This stuff is written as
 * 		macro so that we won't lose the ra.
 * 
 */
#define	BEV_LEAF(r, _X)			\
        .set noreorder;			\
	LEAF(r);			\
	MFC0(k0,C0_SR);			\
	li	k1,SR_CU1+SR_FR+_X;	\
	or	k0,k1;			\
	MTC0(k0,C0_SR);			\
	SETPC();			\
	DMFBR(k0,BR_ASMHNDLR);		\
	DMTBR(zero, BR_ASMHNDLR);	\
	beqz	k0, 99f;		\
	nop;				\
	j	k0;			\
99:	dmtc1	ra, RA_FP;		\
	jal	saveGprs;		\
	nop;				\
	dli	k0,EV_ERTOIP;		\
	ld	k0,0(k0);		\
	DMTBR(k0,BR_ERTOIP);
        .set reorder

/*
 * Function:	bevGeneral
 * Purpose:	Vector entry point for general exception vector.
 * Parameters:	As per exceptions
 * Returns:	Nothing
 */
BEV_LEAF(bevGeneral, 0)
	/*
	 * We allow for "break" and "watch-point" exceptions....
	 */
	.set	at
	.set	noreorder
	MFC0(v0, C0_CAUSE)
	.set	reorder
	and	v0,CAUSE_EXCMASK
        li	v1,EXC_WATCH
	beq	v0,v1,bevGeneralWatch
        li	v1,EXC_BREAK
	beq	v0,v1,bevGeneralBreak
bevGeneralPanic:	
	PANIC(FLED_GENERAL, "General Exception\r\n")
bevGeneralWatch:
        PMESSAGE("*** Watchpoint");
        b	1f
bevGeneralBreak:
        PMESSAGE("*** Breakpoint");
1:
        .set	noreorder
        DMFC0(v0, C0_EPC)
        .set	reorder
	PMESSAGE(" EPC:	"); PHEX(v0);
2:	
	PMESSAGE("\r\n Continue (y/n)? ")
	PGETC()
	beq	v0, 0x6e, bevGeneralPanic	/* 'n' */
	bne	v0, 0x79, 2b
	/*
	 * Resotre registers and continue...
	 */
	j	podResume
	END(bevGeneral)


/*
 * Function:	bevECC
 * Purpose:	ECC handler
 * Parameters:	none
 * Returns:	Does not return, unless ASM handler is set.
 */
BEV_LEAF(bevECC, 0)
	.set	noreorder
	PANIC(FLED_ECC, "ECC Exception")
	.set	reorder
	END(bevECC)

/*
 * Function:	bevXtlbRefill
 * Purpose:	BEV exception handler for XTLB refill expcetion
 * Parameters:	none
 * Returns:	Panics, unless ASM handler set.
 */
BEV_LEAF(bevXtlbRefill, 0)
	PANIC(FLED_XTLBMISS, "XTLB Refill Exception")
	END(bevXtlbRefill)
	
/*
 * Function:	notimplemented
 * Purpose:	process a an unimplemented exception
 * Parameters:	none
 * Returns:	does not return
 */
BEV_LEAF(notimplemented, 0)
	PANIC(FLED_NOTIMPL, "Unimplemented Exception")
	END(notimplemented)

/*
 * Function:	bevRePod
 * Purpose:	TO return to pod Uncached
 * Parameters:	none
 * Returns:	nothing
 */
BEV_LEAF(bevRePod, 0)

	dli	sp, (POD_STACKPADDR + POD_STACKSIZE) | K1BASE
	dli	a0,1
	move	a1,zero
	j	pod_loop
	nop

	/* Doesn't return */

END(bevRePod)

/*
 * Function:	bevIP25monExit
 * Purpose:	To return to pod - flushing caches and running out of 
 *		cache again.
 * Parameters:	none
 * Returns:	nothing
 */
BEV_LEAF(bevIP25monExit, 0)
	jal	cacheFlush
	nop
	jal	initializeCPU
	nop
	jal	initializeIP25
	nop
	j	pod_loop
	nop

	/* Doesn't return */

END(bevIP25monExit)

/*
 * Function:	bevCache
 * Purpose:	BEV cache error handler
 * Parameters:	none
 * Returns:	does not return
 * Notes:	Turn on the DE bit in hopes of actually being able
 *		to spit out a message.
 */
BEV_LEAF(bevCache, SR_DE)
	.set	reorder
	PANIC(FLED_CACHE, "Cache Error Exception")
	END(bevCache)

#ifdef	SABLE
LEAF(bevSuccess)
	.set	noreorder
        ori         zero,  zero,  0x2222
        ori         zero,  zero,  0x1111
        ori         zero,  zero,  0x0000
        .word       ((0xffff0 << 6) | 0x000D)
        nop
	j	ra
	nop
	.set	reorder
	END(bevSuccess)
#endif

LEAF(__spin)
	.set	reorder
#if	!defined(SABLE)
	.set	noreorder
	li	a0,0x60000
1:	
	sub	a0,1
        bgt	a0,zero,1b
        nop
#endif
	j	ra
	END(__spin)
	
