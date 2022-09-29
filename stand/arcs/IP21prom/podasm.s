/***********************************************************************\
*	File:		podasm.s					*
*									*
*	This file (some of which was lifted from the IP17prom source)	*
*	contains code for the various power-on functions used by the	*
*	IP21 PROM.  Among other things, it provides routines for 	*
*	reading and writing from the UART, zonking the caches, etc.	*
*									*
\***********************************************************************/

#ident "$Revision: 1.75 $"

#include "ml.h"
#include <sys/regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include "ip21prom.h"
#include "pod.h"
#include "prom_leds.h"
#include "pod_failure.h"
#include <sys/iotlb.h>
#include <sys/immu.h>

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
   	jal 	pon_setup_stack_in_dcache
	nop
	DPRINT("Invalidating primary i and d caches\r\n")
	jal	pon_invalidate_IDcaches
	nop
#ifndef NOMEM
	DPRINT("Testing and clearing bus tags\r\n")
	jal	pod_check_bustags
	nop
#endif /* NOMEM */
	dli	sp, IP21PROM_STACK
	dli	t0, 0x9fffffff
	and	sp, t0
	or	sp, K0BASE
	dli	t0, EV_SPNUM
	ld	t0, 0(t0)
	andi	t0, EV_SPNUM_MASK
	dsll	t0, 16
	dadd	sp, t0

	DPRINT("Jumping to pod loop\r\n")
	dli	a1, EVDIAG_DEBUG		# Set second POD parameter.
	j	s6
	li	a0, 0				# (BD) Send a 0 as a parm.
	.set	reorder
	END(run_cached)

LEAF(run_uncached)
	move	s5, a0
	move	s6, a1

	dli	sp, IP21PROM_STACK
	dli	t0, 0x9fffffff
	and	sp, t0
	or	sp, K0BASE			# run *cached*
	dli	t0, EV_SPNUM
	ld	t0, 0(t0)
	andi	t0, EV_SPNUM_MASK
	dsll	t0, 16
	dadd	sp, t0

	.set	noreorder
	DPRINT("Invalidating primary dcache\r\n")
	.set	reorder
	jal	pon_invalidate_dcache

	.set	noreorder
	DPRINT("Flushing SAQs\r\n")
	.set	reorder
	jal	flush_SAQueue

	move	a0, s6				# retrieve argument
	j	s5

	/* Doesn't return */

	END(run_uncached)


/*
 * Toggle the memory available switch on and off.
 *
 * Not called anywhere.
 */
LEAF(set_memory_switch)
	j	ra			#   Return	
	END(set_memory_switch)	


LEAF(pod_gprs_to_fprs)
	.set	noreorder
	.set	noat

	# Stash away GPRs in FPRs.  To do this, we must make sure
	# that the FPR registers are enabled.
	#
        DMFC0(k0, C0_SR)               # Make sure FR and CU1 bits are on.
        li	k1, SR_CU1|SR_FR
        or	k0, k1
        DMTC0(k0, C0_SR)

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
	# 						load_regs as well.

	j	ra
	nop

	END(pod_gprs_to_fprs)


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
	DMFC0(ra, RA_REG)

	eret	

	# No more GPRs to stash.  If you change this code, be sure to change
	# 						load_regs as well.

	END(pod_resume)

LEAF(break_handler)
	.set	noreorder

	move	s3, a0
	dla	a0, break_msg
	jal	pod_puts
	move	s4, a1			# Save exception string (BD)

	DMFC0(s0, C0_EPC)
	li	a1, 0xfffff		# Mask off 20 bit expression (LD)
	lw	a0, 0(s0)		# Retrieve instruction
	srl	a0, 16			# Shift down BP number
	jal	pod_puthex		# Print breakpoint number
	and	a0, a1			# Put number in a0 (BD)
	dla	a0, break_at		# Load "Break at" message
	jal	pod_puts
	nop				# (BD)
	jal	pod_puthex		# Print EPC
	move	a0, s0			# Put s0 (EPC) in a0 (BD)

	dla	a0, query_msg
	jal	pod_puts
	nop				# (BD)

	jal	pod_getc
	nop				# (BD)
	li	t0, 0x71		# ASCII for 'q'
	bne	t0, v0, pod_resume	# Continue executing
	nop				# (BD)

	dla	a1, reentering_pod	# Load message to print
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
	dla	a0, watch_msg
	jal	pod_puts
	.set	noreorder
	DMFC0(a0, C0_EPC)
	.set	reorder
	jal	pod_puthex
	dla	a0, crlf
	jal	pod_puts

	dla	a0, query_msg
	jal	pod_puts

	jal	pod_getc
	li	t0, 0x71		# ASCII for 'q'
	bne	t0, v0, pod_resume	# Continue executing

	move	a0, s3
	move	a1, s4

	j	bev_panic		# Abort

	.data
watch_msg:
	.asciiz	"Watchpoint hit (EPC): \r\n"
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
	DMFC0(s0,C0_SR)
	dli	v0,POD_SR
	DMTC0(v0,C0_SR)
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

	dla	a0,exception_msg	# Indicate vector entered on
	jal	pod_puts		# Display vector designator

	.set	noreorder
	DMFC0(a0,C0_EPC)		# Display the exception PC
	.set	reorder
	jal	pod_puthex
	dla	a0,cause_msg		# Display cause register heading
	jal	pod_puts
	.set	noreorder
	DMFC0(a0,C0_CAUSE)		# Get cause register
	.set	reorder
	jal	pod_puthex		# Display cause register
	dla	a0,status_msg		# Display status heading
	jal	pod_puts
	move	a0,s0			# Get the status register
	jal	pod_puthex		# Display it
	dla	a0,crlf
	jal	pod_puts
	dla	a0,badvaddr_msg		# Display badvaddr register
	jal	pod_puts
	.set	noreorder
	DMFC0(a0,C0_BADVADDR)
	.set	reorder
	jal	pod_puthex
	dla	a0,ra_msg		# Display return address register
	jal	pod_puts

	.set	noreorder
	DMFC0(a0,RA_REG)
	.set	reorder
	jal	pod_puthex
	dla	a0,crlf
	jal	pod_puts

	dla	a0,crlf
	jal	pod_puts
	dla	a0,sp_msg		# Display stack pointer
	jal	pod_puts
	move	a0,sp
	jal	pod_puthex
	dla	a0,crlf
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
 * Call function with stack at POD_STACKADDR + dcachesize - 8
 * pod_handler code is in s6.
 */
LEAF(run_incache)

	# pon_fix_dcache_parity first invalidates (zeroes) all
	# 2ndary tags, then initializes the primary dcache
	# without causing writebacks, parity errors, etc.
	# sets each line to 'dirty exclusive', setting the address
	# starting at POD_STACKADDR.

	jal	pon_fix_dcache_parity

	# SR_DE is the indicator (on IP19) that the machine is in pod mode.
	# No SR_DE in TFP.  So use C0_WORK1 as the indicator (on IP21) that
	# the machine is in pod mode.
	dli	t0,POD_SR
	.set	noreorder
	DMTC0(t0,C0_SR)
	dli	t1, PODMODE_BIT
	DMTC0(t1,C0_WORK1)
	.set	reorder

	# set sp to be POD_STACKADDR + dcachesize - 8
	jal	get_dcachesize
	dli	sp,POD_STACKADDR
	subu	v0,8
	daddu	sp,v0

	# and off to pod_loop (won't return)
	li	a0, 1			# 1 means dirty-exclusive
	move	a1, s6			# Code passed to pod_handler
	j	pod_loop
	END(run_incache)

/*
 * podmode
 * Check to see if we are in pod mode.
 * return 0 if not
 */
LEAF(podmode)
	.set	noreorder
	.set	at
	DMFC0(v0,C0_WORK1)
	.set	reorder
	and	v0,PODMODE_BIT
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
	move	v0,sp
	j	ra
	END(_sp)

/* _sr(rw, val) */
LEAF(_sr)
	beq	a0,zero,1f
	# write
	.set	noreorder
	DMTC0(a1,C0_SR)
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	DMFC0(v0,C0_SR)
	.set	reorder
	j	ra
	END(_sr)


/* _cause(rw, val) */
LEAF(_cause)
	beq	a0,zero,1f
	# write
	.set	noreorder
	DMTC0(a1,C0_CAUSE)
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	DMFC0(v0,C0_CAUSE)
	.set	reorder
	j	ra
	END(_cause)


/* _epc(rw, val) */
LEAF(_epc)
	beq	a0,zero,1f
	# write
	.set	noreorder
	DMTC0(a1,C0_EPC)
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	DMFC0(v0,C0_EPC)
	.set	reorder
	j	ra
	END(_epc)

/* _badvaddr(rw, val) */
LEAF(_badvaddr)
	beq	a0,zero,1f
	# write
	.set	noreorder
	DMTC0(a1,C0_BADVADDR)
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	DMFC0(v0,C0_BADVADDR)
	.set	reorder
	j	ra
	END(_badvaddr)

/* _config(rw, val) */
LEAF(_config)
	beq	a0,zero,1f
	# write
	.set	noreorder
	DMTC0(a1,C0_CONFIG)
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	DMFC0(v0,C0_CONFIG)
	.set	reorder
	j	ra
	END(_config)

/* _count(rw, val) */
LEAF(_count)
	beq	a0,zero,1f
	# write
	.set	noreorder
	DMTC0(a1,C0_COUNT)
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	DMFC0(v0,C0_COUNT)
	.set	reorder
	j	ra
	END(_count)

/*
 * _dtag
 * Get a dcache tag, load virtual addr into VADDR then read 
 * from cache tag by use dctr instruction
 */

LEAF(_dtag)

        .set    noreorder
        DMTC0(a0,C0_BADVADDR)
        dctr				# Dcache Tag Read
        ssnop;ssnop
        DMFC0(v0, C0_DCACHE)		# Read from Dcache register
        .set    reorder
        j       ra
        END(_dtag)


	# return stag content
LEAF(_stag)
	ld	v0, 0(a0)
        j       ra
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

/* Clear EXL bit, CC ERTOIP register and pending level 0 interrupts */
LEAF(clear_ip21_state)
	.set	noreorder
	DMFC0(a0, C0_SR)
	dli	a1, ~(SR_EXL)
	and	a1, a0
	DMTC0(a0, C0_SR)
	dli	a0, EV_ERTOIP
	sd	zero, 0(a0)
	dli	a0, EV_IP0
	sd	zero, 0(a0)
	dli	a0, EV_IP1
	sd	zero, 0(a0)
	j	ra
	nop
	END(clear_ip21_state)

/* This macro is a result of endian confusion...  I left it here in case
	this doesn't work the way I think it does.  */
#define STORE_HALVES(_reg, _offset, _indirect)	\
	sd	_reg, _offset(_indirect);

#define STORE_FHALVES(_reg, _offset, _indirect)	\
	sdc1	_reg, _offset(_indirect);

LEAF(store_gprs)
	.set noat
	.set noreorder
	sd	zero, R0_OFF(a0)			/* zero */
	STORE_FHALVES(AT_FP, R1_OFF, a0)	/* at */
	STORE_FHALVES(V0_FP, R2_OFF, a0)	/* v0 */
	STORE_FHALVES(V1_FP, R3_OFF, a0)	/* v1 */
	STORE_FHALVES(A0_FP, R4_OFF, a0)	/* a0 */
	STORE_FHALVES(A1_FP, R5_OFF, a0)	/* a1 */
	STORE_FHALVES(A2_FP, R6_OFF, a0)	/* a2 */
	STORE_HALVES(a3, R7_OFF, a0)		/* a3 */
	STORE_FHALVES(T0_FP, R8_OFF, a0)	/* t0 */
	STORE_FHALVES(T1_FP, R9_OFF, a0)	/* t1 */
	STORE_FHALVES(T2_FP, R10_OFF, a0)	/* t2 */
	STORE_FHALVES(T3_FP, R11_OFF, a0)	/* t3 */
	STORE_FHALVES(T4_FP, R12_OFF, a0)	/* ta0 */
	STORE_FHALVES(T5_FP, R13_OFF, a0)	/* ta1 */
	STORE_FHALVES(T6_FP, R14_OFF, a0)	/* ta2 */
	STORE_FHALVES(T7_FP, R15_OFF, a0)	/* ta3 */
	STORE_FHALVES(S0_FP, R16_OFF, a0)	/* s0 */
	STORE_FHALVES(S1_FP, R17_OFF, a0)	/* s1 */
	STORE_FHALVES(S2_FP, R18_OFF, a0)	/* s2 */
	STORE_FHALVES(S3_FP, R19_OFF, a0)	/* s3 */
	STORE_FHALVES(S4_FP, R20_OFF, a0)	/* s4 */
	STORE_FHALVES(S5_FP, R21_OFF, a0)	/* s5 */
	STORE_FHALVES(S6_FP, R22_OFF, a0)	/* s6 */
	STORE_HALVES(s7, R23_OFF, a0)		/* s7 */
	STORE_FHALVES(T8_FP, R24_OFF, a0)	/* t8 */
	STORE_FHALVES(T9_FP, R25_OFF, a0)	/* t9 */
	STORE_HALVES(k0, R26_OFF, a0)		/* k0 */
	STORE_HALVES(k1, R27_OFF, a0)		/* k1 */
	STORE_HALVES(gp, R28_OFF, a0)		/* gp */
	STORE_FHALVES(SP_FP, R29_OFF, a0)	/* sp */
	STORE_HALVES(fp, R30_OFF, a0)		/* fp */

	DMFC0($at,RA_REG)
	sd	$at, R31_OFF(a0)		/* ra */

	DMFC0($at, C0_SR)
	sd	$at, SR_OFF(a0)
	DMFC0($at, C0_CAUSE)
	sd	$at, CAUSE_OFF(a0)
	DMFC0($at, C0_BADVADDR)
	STORE_HALVES($at, BADVA_OFF, a0)
	DMFC0($at, C0_EPC)
	STORE_HALVES($at, EPC_OFF, a0)
	j	ra
	nop

	.set reorder
	.set at
	END(store_gprs)

LEAF(GetCause)
	.set	noreorder
	DMFC0(v0,C0_CAUSE)
	.set	reorder
	j	ra
	END(GetCause)

LEAF(occupied_slots)
	.set noreorder
	dli	t0, EV_SYSCONFIG
	ld	t1, 0(t0)
	nop
	j	ra
	and	v0, t1, 0Xffff
	.set reorder
	END (occupied_slots)

LEAF(cpu_slots)
	.set noreorder
	dli	t0, EV_SYSCONFIG
	ld	t1, 0(t0)
	nop
	dsrl	t1, t1, 16
	j	ra
	and	v0, t1, 0Xffff
	.set reorder
	END (cpu_slots)

LEAF(memory_slots)
	.set noreorder
	dli	t0, EV_SYSCONFIG
	ld	t1, 0(t0)
	nop
	dsrl	t1, t1, 32
	j	ra
	and	v0, t1, 0Xffff
	.set reorder
	END (memory_slots)

/*
 * a0 = index of tlb entry to get
 * a1 = tlbset
 * a2 = pnumshft
 */
LEAF(get_enhi)
	.set	noreorder
	DMFC0(t0,C0_TLBHI)
	dsllv	a0,a2			# convert index into virtual address
	or	a0,KV1BASE
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set
	TLB_READ
	DMFC0(v0,C0_TLBHI)		# return value
	DMTC0(t0,C0_TLBHI)
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(get_enhi)

/*
 * a0 = index of tlb entry to get
 * a1 = tlbset
 * a2 = pnumshft
 */
LEAF(get_enlo)
	.set	noreorder
	DMFC0(t0,C0_TLBHI)
	dsllv	a0,a2			# convert index into virtual address
	or	a0,KV1BASE
	DMTC0(a0,C0_BADVADDR)		# select correct index
	DMTC0(a1,C0_TLBSET)		# select correct set
	TLB_READ
	DMFC0(v0,C0_TLBLO)		# return value
	DMTC0(t0,C0_TLBHI)
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(get_enlo)


/* All entries are marked invalid.  The VPN written to each of
 * the 3 sets is unique to ensure no duplicate hits.
 * v0: set counter
 * v1: index counter
 * a0: VAddr (and EntryHI if doing all indeces)
 * If doing only 8 indeces:
 *      a1: EntryHi
 *      a2: value to increment EntryHi to change it from one set to another
 */
LEAF(flush_entire_tlb)
	.set    noreorder
        DMTC0(zero, C0_TLBLO)		# clear all bits in EntryLo
        li      v0, NTLBSETS - 1        # v0 is set counter
        dli     a0, 0xC000000000000000  # Kernel Global region
1:
        DMTC0(a0, C0_TLBHI)
        li      v1, NTLBENTRIES         # do all indeces

        DMTC0(v0, C0_TLBSET)
        DMTC0(a0, C0_BADVADDR)		# specify which TLB index
2:
        addi    v1, -1                  # decrement index counter
        tlbw
	NOP_COM_HAZ
        daddiu  a0, NBPP		# increment address to next TLB index
        DMTC0(a0, C0_BADVADDR)		# specify which TLB index
        bgt	v1, zero, 2b		# are we done with all indeces?
        nop

        addi    v0, -1
        bgez    v0, 1b                  # are we done with all sets?
        nop
        j       ra
        nop
        .set    reorder
END(flush_entire_tlb)

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

LEAF(ip21prom_flash_leds)
	.set noreorder
	jal	flash_cc_leds
	li	a0, 255			# (BD) load the number to flash
	END(ip21prom_flash_leds)

	.data

exception_msg:
	.asciiz "EPC:    0x"
status_msg:
	.asciiz " Status: 0x"
cause_msg:
	.asciiz " Cause: 0x"
badvaddr_msg:
	.asciiz " BadVA: 0x"
ra_msg:
	.asciiz " Return: 0x"
sp_msg:
	.asciiz " SP: 0x"
