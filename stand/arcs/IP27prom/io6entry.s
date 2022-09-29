#include <asm.h>

#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#define IP27_CONFIG_1MB_100MHZ		/* Really only used for IP29 bit */
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/agent.h>
#include <hub.h>

#define	PROM_SR			SR_CU1|SR_FR|SR_KX|SR_BEV

	.text
	.set	noreorder
	.set	at

LEAF(entry)
	HUB_LED_SET(0x99)
	
	jal	initializeCPU
	 nop

	dli	sp, 0xa800000000210000
	dla	gp, _gp			/* Required before calling C */

	jal	main
	 nop

1:	b	1b
	 nop

	END(entry)

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

	/*
	 * Initialize the busy-bit table in 2 steps:
	 *	1:
	 *	2:
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

	jal	tlb_flush
	 nop
	j	t1			/* t1 contains ra */
	 nop
	END(initializeCPU)

/*
 * Delay uses the smallest possible loop to loop a specified number of times.
 * For accurate micro-second delays based on the hub clock, see rtc.s.
 */

LEAF(delay)
1:
        bnez    a0, 1b
         daddu  a0, -1
        j       ra
         nop
        END(delay)

#define IP27_CONFIG_1MB_100MHZ

#include <sys/SN/SN0/ip27config.h>

#if 0
	.word		CONFIG_TIME_CONST
	.word		CONFIG_CPU_MODE

	.dword		CONFIG_FREQ_CPU
	.dword		CONFIG_FREQ_HUB
	.dword		CONFIG_FREQ_RTC

	.dword		CONFIG_ECC_ENABLE
#endif
