/***********************************************************************\
*	File:		podasm.s					*
*									*
*	This file (some of which was lifted from the IP17prom source)	*
*	contains code for the various power-on functions used by the	*
*	IP25 PROM.  Among other things, it provides routines for 	*
*	reading and writing from the UART, zonking the caches, etc.	*
*									*
\***********************************************************************/

#ident "$Revision: 1.3 $"

#include <asm.h>
	
#include <sys/mips_addrspace.h>
#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <sys/iotlb.h>
#include <sys/immu.h>

#include <sys/EVEREST/IP25.h>
#include <sys/EVEREST/IP25addrs.h>

#include "ml.h"
#include "ip25prom.h"
#include "pod.h"
#include "prom_leds.h"
#include "pod_failure.h"

/*
 * run_cached
 *      This routine reinitializes the primary and secondary caches,
 *      invalidates the bus tags, then sets up its stack in cached
 *      space and jumps back to the address stored in a0.
 */

LEAF(run_cached)
	.set	noreorder
	move	s6, a0				# Save jump address
	DPRINT("Clearing secondary cache\r\n")
   	jal 	invalidateScache
	nop
	DPRINT("Invalidating primary i and d caches\r\n")
	jal	invalidateIDcache
	nop
	DPRINT("Building Stack\r\n")
	jal	initDcacheStack
	nop

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

	dli	sp, IP25PROM_STACK
	dli	t0, 0x9fffffff
	and	sp, t0
	or	sp, K1BASE
	dli	t0, EV_SPNUM
	ld	t0, 0(t0)
	andi	t0, EV_SPNUM_MASK
	dsll	t0, 16
	dadd	sp, t0

	.set	noreorder
	DPRINT("Invalidating primary dcache\r\n")
	.set	reorder
	jal	invalidateDcache

	move	a0, s6				# retrieve argument
	j	s5

	/* Doesn't return */

	END(run_uncached)

LEAF(saveGprs)
/*
 * Function: saveGprs
 * Purpose:  To save away the current general registers into the floating
 *		point registers.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	IF YOU CHANGE THIS, CHANGE podResume
 */

	.set	noat

        /* Enable floating point registers. */

        .set	noreorder
        MFC0(k0, C0_SR)
        li	k1, SR_CU1|SR_FR
        or	k0, k1
        MTC0(k0, C0_SR)
        .set	reorder

        /* Save everything away */

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
        DMTC1(ta0,T4_FP)
	DMTC1(ta1,T5_FP)
	DMTC1(ta2,T6_FP)
	DMTC1(ta3,T7_FP)
	DMTC1(s0, S0_FP)
	DMTC1(s1, S1_FP)
	DMTC1(s2, S2_FP)
 	DMTC1(s3, S3_FP)
	DMTC1(s4, S4_FP) 
	DMTC1(s5, S5_FP)
	DMTC1(t8, T8_FP)
	DMTC1(t9, T9_FP) 
        DMTC1(sp, SP_FP)

	j	ra
	nop
        .set	at

	END(saveGprs)


LEAF(podResume)
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

	# No more GPRs to stash.  If you change this code, be sure to change
	# 						load_regs as well.

	END(podResume)

/* void podMode(uchar code, char *message)
 * 	Configure the primary data cache as a dirty-exclusive stack and 
 *	run a small command interpretor.  Requires no memory or bus access.
 *	Prints "message" and uses "code" to determine what to display on
 *	the system controller. 
 *	Never returns.
 */
LEAF(podMode)
	.set	noreorder
	MFC0(s0,C0_SR)
	dli	v0,POD_SR
	MTC0(v0,C0_SR)
	
	move	s1, a1			# Grab argument and save it
	move	s6, a0			# Grab failure code and save it
	move	s5, ra			# Save RA
	LEDS(PLED_POD)

	# 1st argument passed in is a string set by the prom handler.
	# print that first.
	move	a0, s1			# restore print string
	jal	pod_puts		# print string 

	li	a1, EVDIAG_DEBUG
	beq	a1, s6, 1f		# On debug, don't print stuff
	nop	

	PMESSAGE("\n\r EPC:   0x"); DMFC0(a0,C0_EPC); PHEX(a0)
	PMESSAGE("   ERROR-EPC: 0x"); DMFC0(a0,C0_ERROR_EPC);PHEX(a0)
        PMESSAGE("\n\r");	

	PMESSAGE(" BadVA: 0x"); DMFC0(a0,C0_BADVADDR); PHEX(a0);
	PMESSAGE("   Return:    0x"); DMFC1(a0, RA_FP); PHEX(a0);
	
	PMESSAGE("\r\n SP:    0x"); PHEX(sp); 
        PMESSAGE("   A0:        0x"); DMFC1(a0, A0_FP); PHEX(a0)
	PMESSAGE("\r\n")

	PMESSAGE(" Cause: 0x"); MFC0(a0,C0_CAUSE); PHEX32(a0);
	PMESSAGE(" Status: 0x"); PHEX32(s0); 
	PMESSAGE(" Cache Error: 0x");	MFC0(a0, C0_CACHE_ERR); PHEX32(a0)
        PMESSAGE("\n\r");	

	
	dli	s1,POD_STACKVADDR-8-1024+4096
1:
	move	a0, s5
	move	a1, s6

	jal	run_incache		# and loop forever in pod_loop....
	.set	reorder
	END(podMode)

/*
 * Function:	run_incache
 * Purpose:	To set up the stack in the primary data cache, and
 * 		branch into pod.
 * Parameters:	None
 * Returns:	Nothing
 * Notes:	Uses initDcacheStack to set up primary data cache 
 *		at POD_STACKADDR.
 */
LEAF(run_incache)
	/*
	 * Set up Primary data cache for stack.
	 */
	jal	initDcacheStack
	dli	t0,POD_SR
	.set	noreorder
	MTC0(t0,C0_SR)
	.set	reorder

	# and off to pod_loop (won't return)
	li	a0, 1			# 1 means dirty-exclusive
	move	a1, s6			# Code passed to pod_handler
	j	pod_loop
	END(run_incache)

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
	MTC0(a1,C0_SR)
	.set	reorder
	j	ra
1:	# read only
	.set	noreorder
	MFC0(v0,C0_SR)
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
	MFC0(v0,C0_CAUSE)
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
	MFC0(v0,C0_CONFIG)
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
	MFC0(v0,C0_COUNT)
	.set	reorder
	j	ra
	END(_count)

/*
 * Function:	clearIP25State
 * Purpose:	Clears the board local error states.
 * Parameters:	None
 * Returns:	Nothing
 */
LEAF(clearIP25State)
	.set	noreorder
	dli	a0, EV_CERTOIP
	dli	a1,-1
	sd	a1, 0(a0)
	dli	a0, EV_IP0
	sd	zero, 0(a0)
	dli	a0, EV_IP1
	sd	zero, 0(a0)
	j	ra
	nop
	END(clearIP25State)

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
	STORE_FHALVES(RA_FP, R31_OFF, a0)	/* ra */

	MFC0($at, C0_SR)
	sd	$at, SR_OFF(a0)
	MFC0($at, C0_CAUSE)
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
 */
LEAF(get_enhi)
	.set	noreorder
	MTC0(a0, C0_INX)
	tlbr
	DMFC0(v0,C0_TLBHI)
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(get_enhi)

/*
 * a0 = index of tlb entry to get
 * a1 = 0 TLBLO0
 *    = 1 TLBLO1
 */
LEAF(get_enlo)
	.set	noreorder
	MTC0(a0, C0_INX)
	tlbr
	bnez	a1,1f
	nop				# BDSLOT
	MFC0(v0, C0_TLBLO_0)
	j	ra
	nop				# BDSLOT
1:	
	MFC0(v0, C0_TLBLO_1)
	j	ra
	nop				# BDSLOT	
	.set	reorder
	END(get_enlo)


/* 
 * Function:	flushTlb
 * Purpose:	Invalidate all TLB entries
 * Paremters:	None
 * Returns:	Nothing
 * Notes:	Uses a0, v0
 */
LEAF(flushTlb)
	.set    noreorder
        DMTC0(zero, C0_TLBLO_0)		# clear all bits in EntryLo 0
        DMTC0(zero, C0_TLBLO_1)		# clear all bits in EntryLo 1
        li      v0, NTLBENTRIES - 1     # v0 is set counter
        dli     a0, 0xC000000000000000  # Kernel Global region
        DMTC0(a0, C0_TLBHI)

1:		
        DMTC0(v0, C0_INX)		# specify which TLB index
        tlbwi
        bgtz	v0, 1b			# are we done with all indeces?
        addi    v0, -1                  # BDSLOT decrement index counter

        j       ra
        nop
        .set    reorder
END(flushTlb)

LEAF(readCP0)
/*
 * Function:	readCP0
 * Purpose:	Read a CP0 register.
 * Parameters:	a0 - CP0 register to read.
 * Returns:	v0 - value read
 */
        .set	noreorder
	dla	v1,readCP0_table
	dsll	a0,4
	daddu	a0,v1
	j	a0
	nop

#define	READCP0(cp0Reg)		DMFC0(v0, $cp0Reg); j ra; nop; nop

readCP0_table:
	READCP0(0)
	READCP0(1)
	READCP0(2)
	READCP0(3)
	READCP0(4)
	READCP0(5)
	READCP0(6)
	READCP0(7)
	READCP0(8)
	READCP0(9)
	READCP0(10)
	READCP0(11)
	READCP0(12)
	READCP0(13)
	READCP0(14)
	READCP0(15)
	READCP0(16)
	READCP0(17)
	READCP0(18)
	READCP0(19)
	READCP0(20)
	READCP0(21)
	READCP0(22)
	READCP0(23)
	READCP0(24)
	READCP0(25)
	READCP0(26)
	READCP0(27)
	READCP0(28)
	READCP0(29)
	READCP0(30)
	
        .set	reorder
        END(readCP0)

LEAF(writeCP0)
/*
 * Function:	writeCP0
 * Purpose:	Write a  CP0 register.
 * Parameters:	a0 - CP0 register to write.
 *		a1 - value to write.
 * Returns:	v0 - value read
 */
        .set	noreorder
	dla	v1,writeCP0_table
	dsll	a0,4
	daddu	a0,v1
	j	a0
	nop

#define	WRITECP0(cp0Reg)	DMTC0(a1, $cp0Reg); j ra; nop; nop

writeCP0_table:
	WRITECP0(0)
	WRITECP0(1)
	WRITECP0(2)
	WRITECP0(3)
	WRITECP0(4)
	WRITECP0(5)
	WRITECP0(6)
	WRITECP0(7)
	WRITECP0(8)
	WRITECP0(9)
	WRITECP0(10)
	WRITECP0(11)
	WRITECP0(12)
	WRITECP0(13)
	WRITECP0(14)
	WRITECP0(15)
	WRITECP0(16)
	WRITECP0(17)
	WRITECP0(18)
	WRITECP0(19)
	WRITECP0(20)
	WRITECP0(21)
	WRITECP0(22)
	WRITECP0(23)
	WRITECP0(24)
	WRITECP0(25)
	WRITECP0(26)
	WRITECP0(27)
	WRITECP0(28)
	WRITECP0(29)
	WRITECP0(30)

        .set	reorder
        END(writeCP0)
