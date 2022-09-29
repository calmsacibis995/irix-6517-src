/***********************************************************************\
*	File:		podasm.s					*
*									*
*	This file (some of which was lifted from the IP17prom source)	*
*	contains code for the various power-on functions used by the	*
*	IP27 PROM.  Among other things, it provides routines for 	*
*	reading and writing from the UART, zonking the caches, etc.	*
*									*
\***********************************************************************/

#ident "$Revision: 1.46 $"

#include <asm.h>

#include <sys/mips_addrspace.h>
#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <sys/iotlb.h>
#include <sys/immu.h>

#include <sys/SN/SN0/IP27.h>
#include <sys/SN/addrs.h>

#include "ip27prom.h"
#include "pod.h"
#include "prom_leds.h"
#include "pod_failure.h"
#include "hub.h"
#include "rtc.h"

#   define	PMESSAGE(s)	MESSAGE(a0,s); jal puts; nop

	.set	reorder

/*
 * run_dex (call from assembly only)
 *
 *   This routine moves the PC into the PROM, initializes the primary
 *   cache as dirty-exclusive, and places the stack pointer in the cache.
 *   Also moves the ELSC area.
 */

LEAF(run_dex)
	move	s6, ra				# Save return address

	jal	tlb_prom

	.set	noreorder
	HUB_LED_SET(PLED_INVICACHE)
	jal	cache_inval_i
	 nop
	HUB_LED_SET(PLED_INVDCACHE)
	jal	cache_inval_d
	 nop
	HUB_LED_SET(PLED_MAKESTACK)
	.set	reorder

	dli	a0, PROMDATA_VADDR
	dli	a1, PROMDATA_PADDR
	dli	a2, PROMDATA_SIZE
	jal	cache_dirty

	DMFBR(v0, BR_BSR)
	or	v0, BSR_DEX
	and	v0, ~BSR_KDEBUG
	DMTBR(v0, BR_BSR)

	dli	sp, POD_STACKVADDR + POD_STACKSIZE

	dli	a0, POD_ELSCVADDR
	jal	set_elsc
	GET_NASID_ASM(a1)
	jal	elsc_init			# Stack required for this

	j	s6

	END(run_dex)

/*
 * run_uncached (call from assembly only)
 *
 *   This routine places the stack pointer in the PROM stack area
 *   in ualias (uncached) space.  The caches are not invalidated.
 *   Also moves the ELSC area to uncached memory.
 */

LEAF(run_uncached)
	move	s6, ra				# Save return address

	jal	tlb_ram_ualias

	dli	v0, IP27PROM_BASE_MAPPED + \
			(IP27PROM_STACK_A - IP27PROM_BASE)
	ld	v1, LOCAL_HUB(PI_CPU_NUM)
	dsll	v1, IP27PROM_STACK_SHFT
	daddu	sp, v0, v1
	daddu	sp, IP27PROM_STACK_SIZE

	jal	get_nasid			# Warning: C routine
	dsll	v0, NASID_SHFT
	daddu	a0, v0, TO_HSPEC(IP27PROM_ELSC_BASE_A)	/* Ualias */
	ld	v1, LOCAL_HUB(PI_CPU_NUM)
	dsll	v1, IP27PROM_ELSC_SHFT
	daddu	a0, v1
	jal	set_elsc
	GET_NASID_ASM(a1)
	jal	elsc_init

	DMFBR(v0, BR_BSR)
	and	v0, ~BSR_DEX
	and	v0, ~BSR_KDEBUG
	DMTBR(v0, BR_BSR)

	j	s6

	END(run_uncached)

/*
 * run_cached (call from assembly only)
 *
 *   This routine places the stack pointer in the PROM stack area
 *   in cached space.  The caches are invalidated.
 *   Also moves the ELSC area to cached memory.
 */

LEAF(run_cached)
	move	s6, ra				# Save return address

	jal	tlb_prom			# Temporarily back to PROM

	jal	cache_inval_i
	jal	cache_inval_d
	jal	cache_inval_s

	GET_NASID_ASM(a0)
	jal	tlb_ram_cac_node		# Takes NASID in a0

	dli	v0, IP27PROM_BASE_MAPPED + (IP27PROM_STACK_A - IP27PROM_BASE)
	ld	v1, LOCAL_HUB(PI_CPU_NUM)
	dsll	v1, IP27PROM_STACK_SHFT
	daddu	sp, v0, v1
	daddu	sp, IP27PROM_STACK_SIZE

	jal	get_nasid			# Warning: C routine
	dsll	v0, NASID_SHFT
	daddu	a0, v0, TO_CAC(IP27PROM_ELSC_BASE_A)
	ld	v1, LOCAL_HUB(PI_CPU_NUM)
	dsll	v1, IP27PROM_ELSC_SHFT
	daddu	a0, v1
	jal	set_elsc
	GET_NASID_ASM(a1)
	jal	elsc_init

	DMFBR(v0, BR_BSR)
	and	v0, ~BSR_DEX
	and	v0, ~BSR_KDEBUG
	DMTBR(v0, BR_BSR)

	j	s6

	END(run_cached)


LEAF(pod_gprs_to_fprs)

	.set	noreorder
	.set	noat

	# Stash away GPRs in FPRs.  To do this, we must make sure
	# that the FPR registers are enabled.
	#
	MFC0(k0, C0_SR)		      # Make sure FR and CU1 bits are on.
	li	k1, SR_CU1|SR_FR
	or	k0, k1
	MTC0(k0, C0_SR)
XLEAF(saveGprs)
	DMTC1($at, AT_FP)
	DMTC1(a0, A0_FP)
	DMTC1(a1, A1_FP)
	DMTC1(a2, A2_FP)
	DMTC1(v0, V0_FP)
	DMTC1(v1, V1_FP)
	DMTC1(t0, T0_FP)
	DMTC1(t1, T1_FP)
	DMTC1(t2, T2_FP)
	DMTC1(t3, T3_FP)
	DMTC1(ta0, T4_FP)
	DMTC1(ta1, T5_FP)
	DMTC1(ta2, T6_FP)
	DMTC1(ta3, T7_FP)
	DMTC1(s0, S0_FP)
	DMTC1(s1, S1_FP)
	DMTC1(s2, S2_FP)
	DMTC1(s3, S3_FP)
	DMTC1(s4, S4_FP)
	DMTC1(s5, S5_FP)
	DMTC1(t8, T8_FP)
	DMTC1(t9, T9_FP)
	DMTC1(sp, SP_FP)

	# No more GPRs to stash.  If you change this code, be sure to change
	#					pod_fprs_to_gprs as well.

	j	ra
	nop

	END(pod_gprs_to_fprs)

LEAF(pod_fprs_to_gprs)

	.set	noreorder
	.set	noat

	# Restore GPRs from FPRs.  To do this, we must make sure
	# that the FPR registers are enabled.
	#
	MFC0(k0, C0_SR)		      # Make sure FR and CU1 bits are on.
	li	k1, SR_CU1|SR_FR
	or	k0, k1
	MTC0(k0, C0_SR)
XLEAF(restoreGprs)
	DMFC1($at, AT_FP)
	DMFC1(a0, A0_FP)
	DMFC1(a1, A1_FP)
	DMFC1(a2, A2_FP)
	DMFC1(v0, V0_FP)
	DMFC1(v1, V1_FP)
	DMFC1(t0, T0_FP)
	DMFC1(t1, T1_FP)
	DMFC1(t2, T2_FP)
	DMFC1(t3, T3_FP)
	DMFC1(ta0, T4_FP)
	DMFC1(ta1, T5_FP)
	DMFC1(ta2, T6_FP)
	DMFC1(ta3, T7_FP)
	DMFC1(s0, S0_FP)
	DMFC1(s1, S1_FP)
	DMFC1(s2, S2_FP)
	DMFC1(s3, S3_FP)
	DMFC1(s4, S4_FP)
	DMFC1(s5, S5_FP)
	DMFC1(t8, T8_FP)
	DMFC1(t9, T9_FP)
	DMFC1(sp, SP_FP)

	# No more GPRs to restore.  If you change this code, be sure to change
	#					pod_fprs_to_gprs as well.

	j	ra
	nop

	END(pod_fprs_to_gprs)


LEAF(pod_resume)
	.set	noreorder
	.set	noat
	# Stash away GPRs in FPRs

	DMFC0(t9, C0_EPC)

	DMFC1($at, AT_FP)
	DMFC1(a0, A0_FP)
	DMFC1(a1, A1_FP)
	DMFC1(a2, A2_FP)
	DMFC1(v0, V0_FP)
	DMFC1(v1, V1_FP)
	DMFC1(t0, T0_FP)

	daddi	t9, 4
	DMTC0(t9, C0_EPC)

	DMFC1(t1, T1_FP)
	DMFC1(t2, T2_FP)
	DMFC1(t3, T3_FP)
	DMFC1(ta0, T4_FP)
	DMFC1(ta1, T5_FP)
	DMFC1(ta2, T6_FP)
	DMFC1(ta3, T7_FP)
	DMFC1(s0, S0_FP)
	DMFC1(s1, S1_FP)
	DMFC1(s2, S2_FP)
	DMFC1(s3, S3_FP)
	DMFC1(s4, S4_FP)
	DMFC1(s5, S5_FP)
	DMFC1(t8, T8_FP)
	DMFC1(t9, T9_FP)
	DMFC1(sp, SP_FP)
	DMFC1(ra, RA_FP)

	eret

	.set	reorder
	.set	at

	# No more GPRs to stash.  If you change this code, be sure to change
	#						load_regs as well.

	END(pod_resume)

LEAF(break_handler)
	.set	noreorder

	move	s3, a0
	dla	a0, break_msg
	jal	puts
	move	s4, a1			# Save exception string (BD)

	DMFC0(s0, C0_EPC)
	li	a1, 0xfffff		# Mask off 20 bit expression (LD)
	lw	a0, 0(s0)		# Retrieve instruction
	srl	a0, 16			# Shift down BP number
	jal	puthex			# Print breakpoint number
	and	a0, a1			# Put number in a0 (BD)
	dla	a0, break_at		# Load "Break at" message
	jal	puts
	nop				# (BD)
	jal	puthex			# Print EPC
	move	a0, s0			# Put s0 (EPC) in a0 (BD)

	dla	a0, query_msg
	jal	puts
	nop				# (BD)

	jal	getc
	nop				# (BD)
	li	t0, 0x71		# ASCII for 'q'
	bne	t0, v0, pod_resume	# Continue executing
	nop				# (BD)

	dla	a1, reentering_pod	# Load message to print
	j	bevPanic		# Abort
	li	a0, KLDIAG_EXC_GENERAL	# (BD)

	.data
break_msg:
	.asciiz	"\r\nHit breakpoint #"
break_at:
	.asciiz	" at address "
reentering_pod:
	.asciiz " Reentering POD mode\r\n"
	.text
	END(break_handler)

LEAF(watch_handler)
	.set	reorder

	move	s3, a0
	move	s4, a1
	dla	a0, watch_msg
	jal	puts
	.set	noreorder
	DMFC0(a0, C0_EPC)
	.set	reorder
	jal	puthex
	li	a0, 10
	jal	putc

	dla	a0, query_msg
	jal	puts

	jal	getc
	li	t0, 0x71		# ASCII for 'q'
	bne	t0, v0, pod_resume	# Continue executing

	move	a0, s3
	move	a1, s4

	j	bevPanic		# Abort

	.data
watch_msg:
	.asciiz	"Watchpoint hit (EPC): \r\n"
query_msg:
	.asciiz	"\r\nGo on? ('q' to quit to POD)\r\n"
	.text

	END(watch_handler)

/*
 * void pod_mode(int mode, int diagval, char *message)
 *
 *   Configure the stack according to mode (POD_MODE_xxx) and
 *   run the command interpreter.
 *
 *   If 'mode' is negative, no initialization is done and POD
 *   assumes the mode is '-mode'.
 *
 *   'diagval' determines what to display on the system controller.
 *
 *   If switching to uncached or cached memory, it's assumed the
 *   RAM copy of the PROM already exists in memory.
 *
 *   Never returns.
 */

LEAF(pod_mode)
	.set	noreorder

	dla	gp, _gp

	MFC0(s0, C0_SR)
	dli	v0, POD_SR
	MTC0(v0, C0_SR)

	move	s0, a0
	move	s1, a1
	move	s2, a2
	move	s3, ra			/* Save where POD was entered from */

	HUB_LED_SET(PLED_PODMODE)

	.set	reorder

	/*
	 * Re-initialize the RTC to ensure it's coming from the
	 * local Hub and not the network.
	 */

	jal	rtc_init

	/*
	 * Set up the desired execution mode.
	 * If the mode is negative, no initialization is done
	 */

	bne	s0, POD_MODE_DEX, 1f
	jal	run_dex
	b	2f
1:	bne	s0, POD_MODE_UNC, 1f
	jal	run_uncached
	b	2f
1:	bne	s0, POD_MODE_CAC, 1f
	jal	run_cached
	b	2f
1:	sub	s0, zero, s0
2:

	move	a0, s0
	move	a1, s1
	move	a2, s2
	move	a3, s3

	jal	pod_loop		/* Never returns */

	/* NOTREACHED */

	END(pod_mode)

/*
 * void pod_move_sp(__uint64_t sp)
 */

LEAF(pod_move_sp)
	dsubu	s6, a0, IP27PROM_ELSC_SIZE
	jal	cache_inval_i
	jal	cache_inval_d
	jal	cache_inval_s
	move	sp, s6
	move	a0, s6
	jal	set_elsc
	GET_NASID_ASM(a1)
	jal	elsc_init
	li	a0, POD_MODE_DEX
	li	a1, KLDIAG_DEBUG
	MESSAGE(a2, "Entering POD mode with new stack pointer")
	li	a3, 0
	jal	pod_loop
	END(pod_move_sp)

LEAF(store_gprs)
	.set	noat
	.set	noreorder
	sd	zero,  R0_OFF(a0)		/* zero */
	sdc1	AT_FP, R1_OFF(a0)		/* at */
	sdc1	V0_FP, R2_OFF(a0)		/* v0 */
	sdc1	V1_FP, R3_OFF(a0)		/* v1 */
	sdc1	A0_FP, R4_OFF(a0)		/* a0 */
	sdc1	A1_FP, R5_OFF(a0)		/* a1 */
	sdc1	A2_FP, R6_OFF(a0)		/* a2 */
	sd	a3,    R7_OFF(a0)		/* a3 */
	sdc1	T4_FP, R8_OFF(a0)		/* ta0 */
	sdc1	T5_FP, R9_OFF(a0)		/* ta1 */
	sdc1	T6_FP, R10_OFF(a0)		/* ta2 */
	sdc1	T7_FP, R11_OFF(a0)		/* ta3 */
	sdc1	T0_FP, R12_OFF(a0)		/* t0 */
	sdc1	T1_FP, R13_OFF(a0)		/* t1 */
	sdc1	T2_FP, R14_OFF(a0)		/* t2 */
	sdc1	T3_FP, R15_OFF(a0)		/* t3 */

	sdc1	S0_FP, R16_OFF(a0)		/* s0 */
	sdc1	S1_FP, R17_OFF(a0)		/* s1 */
	sdc1	S2_FP, R18_OFF(a0)		/* s2 */
	sdc1	S3_FP, R19_OFF(a0)		/* s3 */
	sdc1	S4_FP, R20_OFF(a0)		/* s4 */
	sdc1	S5_FP, R21_OFF(a0)		/* s5 */
	sdc1	S6_FP, R22_OFF(a0)		/* s6 */
	sd	s7,    R23_OFF(a0)		/* s7 */
	sdc1	T8_FP, R24_OFF(a0)		/* t8 */
	sdc1	T9_FP, R25_OFF(a0)		/* t9 */
	sd	k0,    R26_OFF(a0)		/* k0 */
	sd	k1,    R27_OFF(a0)		/* k1 */
	sd	gp,    R28_OFF(a0)		/* gp */
	sdc1	SP_FP, R29_OFF(a0)		/* sp */
	sd	fp,    R30_OFF(a0)		/* fp */
	sdc1	RA_FP, R31_OFF(a0)		/* ra */

	MFC0($at, C0_SR)
	sd	$at, SR_OFF(a0)
	MFC0($at, C0_LLADDR)
	sd	$at, NMISR_OFF(a0)
	MFC0($at, C0_CAUSE)
	sd	$at, CAUSE_OFF(a0)
	DMFC0($at, C0_BADVADDR)
	sd	$at, BADVA_OFF(a0)
	DMFC0($at, C0_ERROR_EPC)
	sd	$at, ERROR_EPC_OFF(a0)
	DMFC0($at, C0_EPC)
	sd	$at, EPC_OFF(a0)
	DMFC0($at, C0_CACHE_ERR)
	sd	$at, CACHE_ERR_OFF(a0)
	j	ra
	 nop

	.set	reorder
	.set	at
	END(store_gprs)

LEAF(docall)
	dsubu	sp, sp, 0x20
	sd	gp, 0x8(sp)
	sd	ra, 0x10(sp)
	or	gp, a5, a5	 
	jalr	a4
	ld	gp, 0x8(sp)
	ld	ra, 0x10(sp)
	daddu	sp, sp, 0x20
	j	ra
	END(docall)
