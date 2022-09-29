/***********************************************************************\
*	File:		podasm.s					*
*									*
*	This file (some of which was lifted from the IP17prom source)	*
*	contains code for the various power-on functions used by the	*
*	Everest PROM.  Among other things, it provides routines for 	*
*	reading and writing from the uart, zonking the caches, etc.	*
*									*
\***********************************************************************/

#ident "$Revision: 1.45 $"

#include <regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include "ip19prom.h"
#include "pod.h"
#include "prom_leds.h"
#include "pod_failure.h"
#include <sys/iotlb.h>

/*
 * run_cached
 *      This routine reinitializes the primary and secondary caches,
 *      invalidates the bus tags, then sets up its stack in cached
 *      space and jumps back to the address stored in a0.
 */

LEAF(run_cached)
	.set	noreorder
	move	s6, a0				# Save jump address
	DPRINT("Setup secondary cache\r\n")
	jal	pod_setup_scache
	nop
	DPRINT("Clear/invalidate primary icache\r\n")
	la	a0, POD_SCACHEADDR
	jal	pon_zero_icache
	nop
	jal	pon_invalidate_icache
	nop
	DPRINT("Invalidating primary dcache\r\n")
	jal	pon_invalidate_dcache
	nop
#ifndef NOMEM
	DPRINT("Testing and clearing bus tags\r\n")
	jal	pod_check_bustags
	nop
#endif /* NOMEM */
	DPRINT("Invalidating secondary cache\r\n")
	jal	pon_invalidate_scache
	nop
	li	sp, IP19PROM_STACK
	li	t0, EV_SPNUM
	ld	t0, 0(t0)
	sll	t0, 16
	add	sp, t0

	DPRINT("Jumping to pod loop\r\n")
	li	a1, EVDIAG_DEBUG			# Set second POD parameter.
	j	s6
	li	a0, 0				# (BD) Send a 0 as a parm.
	.set	reorder
	END(run_cached)

LEAF(run_uncached)
	move	s5, a0
	move	s6, a1

	li	sp, IP19PROM_STACK
	li	t0, EV_SPNUM
	ld	t0, 0(t0)
	lui	t1, 0xa000
	or	sp, t1
	sll	t0, 16
	add	sp, t0

	DPRINT("Invalidating primary dcache\r\n")
	jal	pon_invalidate_dcache

	move	a0, s6				# retrieve argument
	j	s5

	/* Doesn't return */

	END(run_uncached)

/*
 * Toggle the memory available switch on and off.
 */
LEAF(set_memory_switch)
	.set	noreorder
	j	ra			#   Return	
	nop
	.set	reorder
	END(set_memory_switch)	


LEAF(pod_gprs_to_fprs)
	.set	noreorder
	.set	noat

	# Stash away GPRs in FPRs.  To do this, we must make sure
	# that the FPR registers are enabled.
	#
        mfc0    k0, C0_SR               # Make sure FR and CU1 bits are on.
        li      k1, SR_CU1|SR_FR
        or      k0, k1
        mtc0    k0, C0_SR

	dmtc1	$at, AT_FP
	dmtc1	a0, A0_FP
	dmtc1	a1, A1_FP
	dmtc1	a2, A2_FP
	dmtc1	v0, V0_FP
	dmtc1	v1, V1_FP
	dmtc1	t0, T0_FP
	dmtc1	t1, T1_FP
	dmtc1	t2, T2_FP
	dmtc1	t3, T3_FP
	dmtc1	t4, T4_FP
	dmtc1	t5, T5_FP
	dmtc1	t6, T6_FP
	dmtc1	t7, T7_FP
	dmtc1	s0, S0_FP
	dmtc1	s1, S1_FP
	dmtc1	s2, S2_FP
	dmtc1	s3, S3_FP
	dmtc1	s4, S4_FP
	dmtc1	s5, S5_FP
	dmtc1	t8, T8_FP
	dmtc1	t9, T9_FP
	dmtc1	sp, SP_FP
	# dmtc1	ra, RA_FP	# Exception handlers need to store RA

	# No more GPRs to stash.  If you change this code, be sure to change
	# 						load_regs as well.

	j	ra
	nop

	END(pod_gprs_to_fprs)


LEAF(pod_resume)
	.set	noreorder
	.set	noat
	# Stash away GPRs in FPRs

	mfc0	t9, C0_EPC

	dmfc1	$at, AT_FP
	dmfc1	a0, A0_FP
	dmfc1	a1, A1_FP
	dmfc1	a2, A2_FP
	dmfc1	v0, V0_FP
	dmfc1	v1, V1_FP
	dmfc1	t0 T0_FP
	
	addi	t9, 4
	mtc0	t9, C0_EPC

	dmfc1	t1, T1_FP
	dmfc1	t2, T2_FP
	dmfc1	t3, T3_FP
	dmfc1	t4, T4_FP
	dmfc1	t5, T5_FP
	dmfc1	t6, T6_FP
	dmfc1	t7, T7_FP
	dmfc1	s0, S0_FP
	dmfc1	s1, S1_FP
	dmfc1	s2, S2_FP
	dmfc1	s3, S3_FP
	dmfc1	s4, S4_FP
	dmfc1	s5, S5_FP
	dmfc1	t8, T8_FP
	dmfc1	t9, T9_FP
	dmfc1	sp, SP_FP
	dmfc1	ra, RA_FP

	eret	

	# No more GPRs to stash.  If you change this code, be sure to change
	# 						load_regs as well.

	END(pod_resume)

LEAF(break_handler)
	.set	noreorder

	move	s3, a0
	la	a0, break_msg
	jal	pod_puts
	move	s4, a1			# Save exception string (BD)

	mfc0	s0, C0_EPC
	li	a1, 0xfffff		# Mask off 20 bit expression (LD)
	lw	a0, 0(s0)		# Retrieve instruction
	srl	a0, 16			# Shift down BP number
	jal	pod_puthex		# Print breakpoint number
	and	a0, a1			# Put number in a0 (BD)
	la	a0, break_at		# Load "Break at" message
	jal	pod_puts
	nop				# (BD)
	jal	pod_puthex		# Print EPC
	move	a0, s0			# Put s0 (EPC) in a0 (BD)

	la	a0, query_msg
	jal	pod_puts
	nop				# (BD)

	jal	pod_getc
	nop				# (BD)
	li	t0, 0x71		# ASCII for 'q'
	bne	t0, v0, pod_resume	# Continue executing
	nop				# (BD)

	la	a1, reentering_pod	# Load message to print
	j	bev_panic		# Abort
	li	a0, FLED_GENERAL	# (BD)

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
	la	a0, watch_msg
	jal	pod_puts
	.set	noreorder
	mfc0	a0, C0_EPC
	nop
	.set	reorder
	jal	pod_puthex
	la	a0, crlf
	jal	pod_puts
	.set	noreorder
	mfc0	a0, C0_WATCHHI
	nop
	.set	reorder
	jal	pod_puthex
	la	a0, crlf
	jal	pod_puts
	.set	noreorder
	mfc0	a0, C0_WATCHLO
	nop
	.set	reorder
	jal	pod_puthex

	la	a0, query_msg
	jal	pod_puts

	jal	pod_getc
	li	t0, 0x71		# ASCII for 'q'
	bne	t0, v0, pod_resume	# Continue executing

	.set	noreorder
	nop
	mtc0	zero, C0_WATCHLO
	mtc0	zero, C0_WATCHHI
	nop
	.set	reorder

	move	a0, s3
	move	a1, s4

	j	bev_panic		# Abort

	.data
watch_msg:
	.asciiz	"Watchpoint hit (EPC, WatchHi, WatchLo): \r\n"
query_msg:
	.asciiz	"\r\nGo on? ('q' to quit to POD)\r\n"
	.text

	END(watch_handler)

/* void pod_handler(char *message, uchar code)
 * 	Configure the primary data cache as a dirty-exclusive stack and 
 *	run a small command interpretor.  Requires no memory or bus access.
 *	Prints "message" and uses "code" to determine what to display on
 *	the system controller. 
 *	Never returns.
 */
LEAF(pod_handler)
	.set	noreorder
	nop
	mfc0	s0,C0_SR
	nop	
	nop
	nop
	nop
	li	v0,POD_SR
	mtc0	v0,C0_SR
	nop
	nop	
	nop
	nop
	.set	reorder			# WARNING: IN REORDER MODE!!!!
	
	move	s1, a0			# Grab argument and save it
	move	s6, a1			# Grab failure code and save it
	move	s5, ra			# Save RA
	li	a0, PLED_POD		# 
	jal	set_cc_leds		# Set POD mode LEDs

	# 1st argument passed in is a string set by the prom handler.
	# print that first.
	move	a0, s1			# restore print string
	jal	pod_puts		# print string 

	li	a1, EVDIAG_DEBUG
	beq	a1, s6, 1f		# On debug, don't print stuff

	la	a0,exception_msg	# Indicate vector entered on
	jal	pod_puts		# Display vector designator

	.set	noreorder
	mfc0	a0,C0_EPC		# Display the exception PC
	nop
	.set	reorder
	jal	pod_puthex
	la	a0,cause_msg		# Display cause register heading
	jal	pod_puts
	.set	noreorder
	mfc0	a0,C0_CAUSE		# Get cause register
	nop
	.set	reorder
	jal	pod_puthex		# Display cause register
	la	a0,status_msg		# Display status heading
	jal	pod_puts
	move	a0,s0			# Get the status register
	jal	pod_puthex		# Display it
	la	a0,crlf
	jal	pod_puts
	la	a0, err_epc_msg		# Display error epc message
	jal	pod_puts
	.set	noreorder
	mfc0	a0, C0_ERROR_EPC	# Get error EPC
	nop
	.set	reorder
	jal	pod_puthex		# Display it
	la	a0,badvaddr_msg		# Display badvaddr register
	jal	pod_puts
	.set	noreorder
	mfc0	a0,C0_BADVADDR
	nop
	.set	reorder
	jal	pod_puthex
	la	a0,ra_msg		# Display return address register
	jal	pod_puts
	move	a0,s5
	jal	pod_puthex
	la	a0,crlf
	jal	pod_puts
1:
	move	a0, s5
	move	a1, s6

	jal	run_incache		# and loop forever in pod_loop....
	END(pod_handler)

/*
 * Run POD with stack in the primary dcache. To do this, each 
 * 2ndary line is invalidated, and each PD line is initialized
 * to 'dirty_exclusive' state, to prevent writebacks to memory/scache.
 * Call function with stack at POD_STACKADDR + dcachesize - 4
 * pod_handler code is in s6.
 */
LEAF(run_incache)

	# pon_fix_dcache_parity first invalidates (zeroes) all
	# 2ndary tags, then initializes the primary dcache
	# without causing writebacks, parity errors, etc.
	# sets each line to 'dirty exclusive', setting the address
	# starting at POD_STACKADDR.

#if 0	
/* Only set-up the scache if we want to be able to download code into
 *	the scache.  Otherwise, this adds dependencies to POD mode.
 *	We don't want to do this anymore.
 */
	jal	pod_setup_scache
#else
	jal	pon_fix_dcache_parity
#endif
	# SR_DE is the indicator that the machine is in pod mode.
	li	t0,POD_SR
	.set	noreorder
	mtc0	t0,C0_SR
	nop
	.set	reorder

	# set sp to be POD_STACKADDR + dcachesize - 4
	jal	get_dcachesize
	li	sp,POD_STACKADDR
	subu	v0,4
	addu	sp,v0

	# and off to pod_loop (won't return)
	li	a0, 1			# 1 means dirty-exclusive
	move	a1, s6			# Code passed to pod_handler
	j	pod_loop
	END(run_incache)

/*
 * podmode
 * make a guess if in pod mode
 * return 0 if not
 */
LEAF(podmode)
	.set	noreorder
	.set	at
	mfc0	v0,C0_SR
	nop
	.set	reorder
	and	v0,SR_DE
	j	ra
	END(podmode)


/*
 * routines added to allow poking CPU registers at pod level
 * rw = 0 --> read
 */

/* _sp(rw, val) */
LEAF(_sp)
	beq	a0,zero,1f
	# write
	move	sp,a1
	j	ra
1:	# read only
	move v0,sp
	j	ra
	END(_sp)

/* _wl(rw, val) */
LEAF(_wl)
	beq	a0,zero,1f
	# write
	.set	noreorder
	mtc0	a1,C0_WATCHLO
	nop
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	mfc0 	v0,C0_WATCHLO
	nop
	.set	reorder
	j	ra
	END(_wl)

/* _wh(rw, val) */
LEAF(_wh)
	beq	a0,zero,1f
	# write
	.set	noreorder
	mtc0	a1,C0_WATCHHI
	nop
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	mfc0 	v0,C0_WATCHHI
	nop
	.set	reorder
	j	ra
	END(_wh)

/* _sr(rw, val) */
LEAF(_sr)
	beq	a0,zero,1f
	# write
	.set	noreorder
	mtc0	a1,C0_SR
	nop
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	mfc0 	v0,C0_SR
	nop
	.set	reorder
	j	ra
	END(_sr)


/* _cause(rw, val) */
LEAF(_cause)
	beq	a0,zero,1f
	# write
	.set	noreorder
	mtc0	a1,C0_CAUSE
	nop
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	mfc0 	v0,C0_CAUSE
	nop
	.set	reorder
	j	ra
	END(_cause)


/* _epc(rw, val) */
LEAF(_epc)
	beq	a0,zero,1f
	# write
	.set	noreorder
	mtc0	a1,C0_EPC
	nop
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	mfc0 	v0,C0_EPC
	nop
	.set	reorder
	j	ra
	END(_epc)


/* _error_epc(rw, val) */
LEAF(_error_epc)
	beq	a0,zero,1f
	# write
	.set	noreorder
	mtc0	a1,C0_ERROR_EPC
	nop
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	mfc0 	v0,C0_ERROR_EPC
	nop
	.set	reorder
	j	ra
	END(_error_epc)


/* _config(rw, val) */
LEAF(_config)
	beq	a0,zero,1f
	# write
	.set	noreorder
	mtc0	a1,C0_CONFIG
	nop
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	mfc0 	v0,C0_CONFIG
	nop
	.set	reorder
	j	ra
	END(_cause)

	# Get an icache tag
LEAF(_itag)
	.set noreorder
	cache   C_ILT|CACH_PI, 0(a0)	# Get tag
	nop
	mfc0	v0, C0_TAGLO		# Copy tag
	nop
	j	ra
	nop
	.set reorder
	END(_itag)

	# Get a dcache tag
LEAF(_dtag)
	.set noreorder
	cache   C_ILT|CACH_PD, 0(a0)	# Get tag
	nop
	mfc0	v0, C0_TAGLO		# Copy tag
	nop
	j	ra
	nop
	.set reorder
	END(_dtag)

	# Get an scache tag
LEAF(_stag)
	.set noreorder
	cache   C_ILT|CACH_SD, 0(a0)	# Get tag
	nop
	mfc0	v0, C0_TAGLO		# Copy tag
	nop
	j	ra
	nop
	.set reorder
	END(_stag)

LEAF(set_bsr_bit)
	.set	noreorder
	GET_BSR(t0)
	or	t0, a0
	SET_BSR(t0)
	j	ra
	nop
	.set	reorder
	END(set_bsr_bit)

LEAF(clr_bsr_bit)
	.set	noreorder
	GET_BSR(t0)
	not	a0
	and	t0, a0
	SET_BSR(t0)
	j	ra
	nop
	.set	reorder
	END(clr_bsr_bit)

/* Clear EXL/ERL bits, CC ERTOIP register and pending level 0 interrupts */
LEAF(clear_ip19_state)
	.set	noreorder
	mfc0	a0, C0_SR
	li	a1, ~(SR_EXL | SR_ERL)
	and	a1, a0
	mtc0	a0, C0_SR
	nop
	li	a0, EV_ERTOIP
	sd	zero, 0(a0)
	li	a0, EV_IP0
	sd	zero, 0(a0)
	li	a0, EV_IP1
	sd	zero, 0(a0)
	j	ra
	nop
	END(clear_ip19_state)

/* This macro is a result of endian confusion...  I left it here in case
	this doesn't work the way I think it does.  */
#define STORE_HALVES(_reg, _offset, _indirect)	\
	sd	_reg, _offset(_indirect);

#define STORE_FHALVES(_reg, _offset, _indirect)	\
	sdc1	_reg, _offset(_indirect);

LEAF(store_gprs)
	.set noat
	.set noreorder
	sd	r0, R0_OFF(a0)			/* zero */
	STORE_FHALVES(AT_FP, R1_OFF, a0)	/* at */
	STORE_FHALVES(V0_FP, R2_OFF, a0)	/* v0 */
	STORE_FHALVES(V1_FP, R3_OFF, a0)	/* v1 */
	STORE_FHALVES(A0_FP, R4_OFF, a0)	/* a0 */
	STORE_FHALVES(A1_FP, R5_OFF, a0)	/* a1 */
	STORE_FHALVES(A2_FP, R6_OFF, a0)	/* a2 */
	STORE_HALVES(r7, R7_OFF, a0)		/* a3 */
	STORE_FHALVES(T0_FP, R8_OFF, a0)	/* t0 */
	STORE_FHALVES(T1_FP, R9_OFF, a0)	/* t1 */
	STORE_FHALVES(T2_FP, R10_OFF, a0)	/* t2 */
	STORE_FHALVES(T3_FP, R11_OFF, a0)	/* t3 */
	STORE_FHALVES(T4_FP, R12_OFF, a0)	/* t4 */
	STORE_FHALVES(T5_FP, R13_OFF, a0)	/* t5 */
	STORE_FHALVES(T6_FP, R14_OFF, a0)	/* t6 */
	STORE_FHALVES(T7_FP, R15_OFF, a0)	/* t7 */
	STORE_FHALVES(S0_FP, R16_OFF, a0)	/* s0 */
	STORE_FHALVES(S1_FP, R17_OFF, a0)	/* s1 */
	STORE_FHALVES(S2_FP, R18_OFF, a0)	/* s2 */
	STORE_FHALVES(S3_FP, R19_OFF, a0)	/* s3 */
	STORE_FHALVES(S4_FP, R20_OFF, a0)	/* s4 */
	STORE_FHALVES(S5_FP, R21_OFF, a0)	/* s5 */
	STORE_FHALVES(S6_FP, R22_OFF, a0)	/* s6 */
	STORE_HALVES(r23, R23_OFF, a0)		/* s7 */
	STORE_FHALVES(T8_FP, R24_OFF, a0)	/* t8 */
	STORE_FHALVES(T9_FP, R25_OFF, a0)	/* t9 */
	STORE_HALVES(r26, R26_OFF, a0)		/* k0 */
	STORE_HALVES(r27, R27_OFF, a0)		/* k1 */
	STORE_HALVES(r28, R28_OFF, a0)		/* gp */
	STORE_FHALVES(SP_FP, R29_OFF, a0)	/* sp */
	STORE_HALVES(fp, R30_OFF, a0)		/* fp */
	STORE_FHALVES(RA_FP, R31_OFF, a0)	/* ra */

	mfc0	$at, C0_SR
	nop
	sw	$at, SR_OFF(a0)
	mfc0	$at, C0_CAUSE
	nop
	sw	$at, CAUSE_OFF(a0)
	mfc0	$at, C0_BADVADDR
	nop
	STORE_HALVES($at, BADVA_OFF, a0)
	mfc0	$at, C0_EPC
	nop
	STORE_HALVES($at, EPC_OFF, a0)
	j	ra
	nop

	.set reorder
	.set at
	END(store_gprs)

LEAF(GetCause)
	.set	noreorder
	mfc0	v0,C0_CAUSE
	nop
	.set	reorder
	j	ra
	END(GetCause)

LEAF(occupied_slots)
	.set noreorder
	li	t0, EV_SYSCONFIG
	ld	t1, 0(t0)
	nop
	j	ra
	and	v0, t1, 0Xffff
	.set reorder
	END (occupied_slots)

LEAF(cpu_slots)
	.set noreorder
	li	t0, EV_SYSCONFIG
	ld	t1, 0(t0)
	nop
	dsrl	t1, t1, 16
	j	ra
	and	v0, t1, 0Xffff
	.set reorder
	END (cpu_slots)

LEAF(memory_slots)
	.set noreorder
	li	t0, EV_SYSCONFIG
	ld	t1, 0(t0)
	nop
	dsrl	t1, t1, 32
	j	ra
	and	v0, t1, 0Xffff
	.set reorder
	END (memory_slots)

/* a0 = index of tlb entry to get */
LEAF(get_enhi)
	.set	noreorder
	mtc0	a0, C0_INX
	nop
	nop
	nop
	nop
	tlbr
	nop
	nop
	nop
	nop
	mfc0	v0, C0_TLBHI
	j	ra
	nop
	END(get_enhi)

/* a0 = index of tlb entry to get */
LEAF(get_enlo0)
	.set	noreorder
	mtc0	a0, C0_INX
	nop
	nop
	nop
	nop
	tlbr
	nop
	nop
	nop
	nop
	mfc0	v0, C0_TLBLO_0
	j	ra
	nop
	END(get_enlo0)


/* a0 = index of tlb entry to get */
LEAF(get_enlo1)
	.set	noreorder
	mtc0	a0, C0_INX
	nop
	nop
	nop
	nop
	tlbr
	nop
	nop
	nop
	nop
	mfc0	v0, C0_TLBLO_1
	j	ra
	nop
	END(get_enlo1)

LEAF(flush_tlb)
	.set	noreorder
        li      r2, NTLBENTRIES - 1
        li      r5, 0x80000000

        mtc0    r5, C0_TLBHI
        mtc0    r0, C0_TLBLO_0
        mtc0    r0, C0_TLBLO_1
	nop
1:
        mtc0    r2, C0_INX              # Change index register
        nop
        nop
        nop
        nop
        tlbwi
        nop; nop; nop; nop

        bnez    r2, 1b
        addi    r2, -1                  # (BD)
	j	ra
	nop				# (BD)
	END(flush_tlb)

/* wr_restore() - read and save the value at an address, write the one
 *	provided, read it back, restore the original value.
 * Parameters:
 * 	a0 = address to read and write
 *	a1 = value to store
 * Returns
 *	v0 = value read back from memory
 */
LEAF(wr_restore)
	.set noreorder
	lw	t0, 0(a0)	# Read old value
	sw	a1, 0(a0)	# Write new one
	lw	v0, 0(a0)	# Read new one back
	j	ra
	sw	t0, 0(a0)	# Write old one back
END(wr_restore)


/* pod_ecc() - decode and print information from the CacheError register
 *	of the R4000.  Depending on the source of the error, dump the
 *	affected cacheline to the UART.
 */
LEAF(pod_ecc)
	.set	noreorder
	mfc0	t0, C0_CACHE_ERR
	nop
	
	# Save the current status register value and turn on the DE bit.
	# 	without the DE bit, we'll get another exception accessing
	#	the bad cache line.
	mfc0	s3, C0_SR
	nop
	li	t0, SR_DE
	or	t0, s3			# Turn on DE bit
	mtc0	t0, C0_SR
	nop
	.set	reorder

	move	s2, ra

	# Print the raw C0_CACHE_ERR value.

	la	a0, ecc_decode_msg
	jal	pod_puts

	move	a0, t0
	jal	pod_puthex

	la	a0, crlf
	jal	pod_puts

	# Decode the CacheError bits and print translations.
	# 	Store away error source information in registers t2,t3.

	# Check primary/secondary bit
	li	t1, CACHERR_EC
	and	t1, t0
	bnez	t1, 1f
	la	a0, primary_ref
	li	t2, 0				# 0 = primary
	b	2f
1:
	la	a0, secondary_ref
	li	t2, 1				# 1 = secondary
2:	jal	pod_puts

	# Check instruction/data bit
	li	t1, CACHERR_ER
	and	t1, t0
	bnez	t1, 1f
	la	a0, inst_ref
	li	t3, 0				# 0 = icache
	b	2f
1:
	la	a0, data_ref
	li	t3, 1				# 1 = dcache
2:	jal	pod_puts
	# Data field error bit
	li	t1, CACHERR_ED
	and	t1, t0
	beqz	t1, 1f
	la	a0, data_field
	jal	pod_puts
1:

	# Tag field error bit
	li	t1, CACHERR_ET
	and	t1, t0
	beqz	t1, 1f
	la	a0, tag_field
	jal	pod_puts
1:

	# Check internal/external bit
	li	t1, CACHERR_ES
	and	t1, t0
	bnez	t1, 1f
	la	a0, internal_req
	b	2f
1:
	la	a0, external_req
2:	jal	pod_puts

	# Instruction and data error bit
	li	t1, CACHERR_EB
	and	t1, t0
	beqz	t1, 1f
	la	a0, inst_data_err
	jal	pod_puts
1:

	# Store miss refill bit
	li	t1, CACHERR_EI
	and	t1, t0
	beqz	t1, 1f
	la	a0, smiss_refill_err
	jal	pod_puts
1:

	# Primary and secondary line indices
	li	t1, CACHERR_SIDX_MASK
	and	t1, t0
	la	a0, sidx_msg
	jal	pod_puts
	move	a0, t1
	move	t4, t1				# Keep the sidx around in t4
	jal	pod_puthex
	la	a0, pidx_msg
	jal	pod_puts
	li	t1, CACHERR_PIDX_MASK
	and	t1, t0
	sll	t1, CACHERR_PIDX_SHIFT
	move	a0, t1
	jal	pod_puthex
	la	a0, crlf
	jal	pod_puts

	# t0 = cache error reg
	# t1 = pidx
	# t2 = 0 if primary, 1 if secondary
	# t3 = 0 if picache, 0 if pdcache, x if scache
	# t4 = sidx

	# If the error's in the secondary cache, jump away		
	bnez	t2, 1f

	# Handle primary errors
	andi	t4, 0xfff		# Get bottom of sidx (bits 11-0)
	or	t4, t1			# Or in pidx (vaddr bits 14-12)

	# t4 now primary address

	# If the error's in the instruction cache, jump away.
	beqz	t3, 2f

	# Handle dcache

	# Fetch the dcache line's tag
	.set noreorder
	li	t2, 0x80000000
	or	t2, t4
	cache   C_ILT|CACH_PD, 0(t2)	# Get tag

	mfc0	t2, C0_TAGLO		# Copy tag
	nop
	.set reorder

	la	a0, rawtag
	jal	pod_puts

	move	a0, t2
	jal	pod_puthex

	li	t3, ~(0xff)
	and	t3, t2			# Get PTagLo (bits 31..8)
	sll	t3, 8			# Shift into position

	# Print message, vaddr (t4), tag addr (t3)
	la	a0, pdaddrs
	jal	pod_puts
	
	move	a0, t4
	jal	pod_puthex

	la	a0, comma_space
	jal	pod_puts

	move	a0, t3
	jal	pod_puthex

	srl	t2, t2, 6
	andi	t2, 3			# Get PState

	# Print message and primary data cache line state

	la	a0, pstate
	jal	pod_puts

	move	a0, t2
	jal	pod_puthex

	# Print "Data:", dump the cache line as doublewords.

	la	a0, datadump
	jal	pod_puts

	jal	get_dcacheline		# Get the primary data cache line size.

	srl	t3, 15
	sll	t3, 15			# Wipe out bits 14..0 of physical tag

	addi	t1, v0, -1
	not	t1

	li	t0, 0x80000000
	or	t0, t4			# or in virtual index
	or	t0, t3			# or in physical tag
	and	t0, t1			# mask off index into line
	add	t1, t0, v0		# End address
5:
	move	a0, t0
	jal	pod_puthex
	la	a0, colon_space
	jal	pod_puts

	ld	a0, 0(t0)		# Load doubleword
	addi	t0, 8

	jal	pod_puthex
	la	a0, crlf
	jal	pod_puts

	bne	t0, t1, 5b

	b	4f			# Done!


	# Handle primary instruction cache
2:

	# Get and decode the tag

	.set noreorder
	li	t2, 0x80000000
	or	t2, t4
	cache   C_ILT|CACH_PI, 0(t2)	# Get tag

	mfc0	t2, C0_TAGLO		# Copy tag
	nop
	.set reorder

	la	a0, rawtag
	jal	pod_puts

	move	a0, t2
	jal	pod_puthex

	li	t3, ~(0xff)
	and	t3, t2			# Get PTagLo (bits 31..8)
	sll	t3, 8			# Shift into position

	# Print message, vaddr, tag addr
	la	a0, piaddrs
	jal	pod_puts
	
	move	a0, t4
	jal	pod_puthex

	la	a0, comma_space
	jal	pod_puts

	move	a0, t3
	jal	pod_puthex

	srl	t3, t2, 6
	andi	t3, 3			# Get PState

	# Print message and primary instruction cache line state

	la	a0, pstate
	jal	pod_puts

	move	a0, t2
	jal	pod_puthex

	la	a0, crlf
	jal	pod_puts

	b	4f			# Can't load data from icache
					# So we're done now.

	# Handle the secondary cache
1:
	# t0 = cache error reg
	# t1 = pidx
	# t2 = 0 if primary, 1 if secondary
	# t3 = 0 if picache, 0 if pdcache, x if scache
	# t4 = sidx

	# Get and decode the secondary cache line tag
		
	.set noreorder
	li	t2, 0x80000000
	or	t2, t4
	cache   C_ILT|CACH_SD, 0(t2)	# Get tag

	mfc0	t2, C0_TAGLO		# Copy tag
	nop
	.set reorder

	la	a0, rawtag
	jal	pod_puts

	move	a0, t2
	jal	pod_puthex

	li	t3, ~(0x1fff)
	and	t3, t2			# Get STagLo (bits 31..13)
	sll	t3, 4			# Shift into position

	# Print message, sindex (t4), tag addr (t3)
	la	a0, saddrs
	jal	pod_puts
	
	move	a0, t4
	jal	pod_puthex

	la	a0, comma_space
	jal	pod_puts

	move	a0, t3
	jal	pod_puthex

	andi	t3, t2, 0x7f		# Get the ECC bits

	# Print ECC bits

	la	a0, secc
	jal	pod_puts

	move	a0, t3
	jal	pod_puthex

	srl	t3, t2, 6
	andi	t3, 7			# Get SState (secondary cacheline state)

	# Print message and sstate

	la	a0, sstate
	jal	pod_puts

	move	a0, t3
	jal	pod_puthex

	# Print "Data:" followed by the cache line's contents in doublewords.

	la	a0, datadump
	jal	pod_puts

	jal	get_scacheline

	addi	t1, v0, -1
	not	t1

	li	t0, 0x80000000
	or	t0, t4			# Or in index
	or	t0, t3			# Or in tag
	and	t0, t1			# Mask off index into line
	add	t1, t0, v0		# End address
5:
	move	a0, t0
	jal	pod_puthex
	la	a0, colon_space
	jal	pod_puts

	ld	a0, 0(t0)		# Load doubleword
	addi	t0, 8

	jal	pod_puthex
	la	a0, crlf
	jal	pod_puts

	bne	t0, t1, 5b

4:					# Done!  Restore the status register
	.set	noreorder
	mtc0	s3, C0_SR
	nop
	.set	reorder

	j	s2 	# ra
	END(pod_ecc)


LEAF(ip19prom_flash_leds)
	.set noreorder
	jal	flash_cc_leds
	li	a0, 255			# (BD) load the number to flash
	END(ip19prom_flash_leds)


LEAF(reg_save_test)
	.set	noreorder
	.set	noat
	li	r1,	0x1
	li	r2,	0x2
	li	r3,	0x3
	li	r4,	0x4
	li	r5,	0x5
	li	r6,	0x6
	li	r7,	0x7
	li	r8,	0x8
	li	r9,	0x9
	li	r10,	0x10
	li	r11,	0x11
	li	r12,	0x12
	li	r13,	0x13
	li	r14,	0x14
	li	r15,	0x15
	li	r16,	0x16
	li	r17,	0x17
	li	r18,	0x18
	li	r19,	0x19
	li	r20,	0x20
	li	r21,	0x21
	li	r22,	0x22
	li	r23,	0x23
	li	r24,	0x24
	li	r25,	0x25
	li	r26,	0x26
	li	r27,	0x27
	li	r28,	0x28
	li	r29,	0x29
	li	r30,	0x30
	break	0x29
	break	0x30
	nop
	nop
	break	0x31	
	nop

	la	a0, returning_to_pod
	j	pod_handler
	li	a1, EVDIAG_RETURNING		# (BD)
	.set at
	END(reg_save_test)

/*
 * rmtlbent(indx)
 * Invalidate the indexed entry.
 */
LEAF(rmtlbent)

        .set noreorder
        sll     a0, TLBINX_INXSHIFT
        mtc0    a0, C0_INX
        nop
        mtc0    zero, C0_TLBLO_0
        nop
        mtc0    zero, C0_TLBLO_1
        nop
        li      a0, K0BASE&TLBHI_VPNMASK
        mtc0    a1, C0_TLBHI
        nop

        tlbwi   /* Invalidate the entry pointed by index */
        nop; nop

        j       ra
        or      v0, zero, zero  /* BD */

        END(rmtlbent)

/* read TLB page mask register */
LEAF(get_pgmask)
        .set    noreorder
        mfc0    v0,C0_PGMASK
        nop
        j       ra
        nop
        .set    reorder
        END(get_pgmask)

/* write TLB page mask register */
LEAF(set_pgmask)
        .set    noreorder
        mtc0    a0,C0_PGMASK
        j       ra
        nop
        .set    reorder
        END(set_pgmask)

	.data

ecc_decode_msg:
	.asciiz	"Cache error register contains "
inst_ref:
	.asciiz	"ICache;"
data_ref:
	.asciiz	"DCache;"
primary_ref:
	.asciiz	"Primary "
secondary_ref:
	.asciiz	"Secondary "
data_field:
	.asciiz	"Data field;"
tag_field:
	.asciiz	"Tag field;"
internal_req:
	.asciiz	"Int Req;"
external_req:
	.asciiz	"Ext Req;"
sysad_err:
	.asciiz	"SysAD Err;"
inst_data_err:
	.asciiz	"Istr+Data;"
smiss_refill_err:
	.asciiz	"SMiss Refill;"
sidx_msg:
	.asciiz	"\r\nSIdx: "
pidx_msg:
	.asciiz	"\r\nPIdx: "
exception_msg:
	.asciiz "EPC:    0x"
err_epc_msg:
	.asciiz "ErrEPC: 0x"
status_msg:
	.asciiz " Status: 0x"
cause_msg:
	.asciiz " Cause: 0x"
badvaddr_msg:
	.asciiz " BadVA: 0x"
ra_msg:
	.asciiz " Return: 0x"
test_msg:
	.asciiz "Testing POD mode.\r\n"
dcache_busted:
	.asciiz "Dcache test failed.\r\n"
pdaddrs:
	.asciiz	"\r\nPrimary dcache virtual index, tag: "
piaddrs:
	.asciiz	"\r\nPrimary icache virtual index, tag: "
saddrs:
	.asciiz	"\r\nSecondary cache index, tag: "
pstate:
	.asciiz	"\r\nPrimary line state: "
sstate:
	.asciiz "\r\nSecondary line state: "
secc:
	.asciiz "\r\nSecondary ECC check bits: "
datadump:
	.asciiz "\r\nData dump:\r\n"
rawtag:
	.asciiz	"Raw tag: "
comma_space:
	.asciiz	", "
colon_space:
	.asciiz	": "


