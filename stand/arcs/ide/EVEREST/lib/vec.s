/*
 * prims/vec.s
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.7 $"

#include <asm.h>
#include <regdef.h>
#include <cp0.h>
#include <fault.h>
/* #include <sys/ip5.h> */
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/fpu.h>
#include <sys/sbd.h>
#include <saioctl.h>
#include "pon.h"
/* #include "mp.h" */

#define         GETREGS \
        li      k0,PHYS_TO_K1(EV_SPNUM); \
        lw      k0,0(k0);       \
        and     k0,MPSR_IDMASK; \
        sll     k0,9;   \
        la      k1,_regs; \
        addu    k1,k0

#ifndef pon
#define	 LONGWAIT   	400000
#define	 SHORTWAIT  	100000
#define  ALLLEDSLIT     0x3F
#define  ALLLEDSOUT     0x00

 #  FastFlash blinks all the LEDs on and off rapid rate (about 1/4 second);
 #  ShortWait provides a delay-loop of approximately 1/4 second when running
 #  from prom.

/*
LEAF(FastFlash)
	move	t8,ra		# save our return address
 	li	t9,10		# flash 'em for about 5 secs
	move	v1,a0		# save LED error value to write
2:	li	a0, ALLLEDSOUT	# so turn the LEDS off
	jal	set_leds	# write to the CPU LEDS
	li 	v0,SHORTWAIT	# get the delay count
1:	addiu	v0,v0,-1	# decrement the count
	bgtz	v0,1b		# spin till it be 0
 	move	a0,v1		# now show LED error value
	jal	set_leds	# write to the CPU LEDS
	li 	v0,SHORTWAIT	# get the delay count
1:	addiu	v0,v0,-1	# decrement the count
	bgtz	v0,1b		# spin till it be 0
	addiu	t9,t9,-1	# decrement the loop-counter
	bgtz	t9,2b		# keep on flashin'
	j	t8		# and back to the caller
	END(FastFlash)
*/
#endif


LEAF( UpdateCause )
	.set	noreorder
	mfc0	v0,C0_CAUSE
	NOP_1_3
	.set	reorder
	and	a1,v0
	or	a0,a1
	.set	noreorder
	mtc0	a0,C0_CAUSE
	nop
	.set	reorder
	j	ra
.end	UpdateCause 

/*
 * save_state -- save all of registers so we can resume to the
 *		 same execution point.	      
 */
/*
LEAF(save_state)
	.set	noat
	move	k0,AT
	.set	at
	GETREGS
	sw	k0,EF_AT*4(k1)
	sw	v0,EF_V0*4(k1)

	.set	noreorder
	mfc0	v0,C0_EPC
	NOP_1_3
	sw	v0,EF_EPC*4(k1)

	mfc0	v0,C0_SR
	NOP_1_3
	sw	v0,EF_SR*4(k1)

	mfc0	v0,C0_BADVADDR
	NOP_1_3
	sw	v0,EF_BADVADDR*4(k1)

	mfc0	v0,C0_CAUSE
	NOP_1_3
	sw	v0,EF_CAUSE*4(k1)
	.set	reorder

	sw	sp,EF_SP*4(k1)
	li	v0,EV_SPNUM
	lw	v0,0(v0)
	sw	v0,EF_MPSR*4(k1)
	lw	v0,EF_EPC*4(k1)
	sw	v0,R_EPC*4(k1)
	lw	v0,EF_SR*4(k1)
	sw	v0,R_SR*4(k1)
	lw	v0,EF_AT*4(k1)
	sw	v0,R_AT*4(k1)
	lw	v0,EF_V0*4(k1)
	sw	v0,R_V0*4(k1)
	lw	v0,EF_EXC*4(k1)
	sw	v0,R_EXCTYPE*4(k1)
	lw	v0,EF_BADVADDR*4(k1)
	sw	v0,R_BADVADDR*4(k1)
	lw	v0,EF_CAUSE*4(k1)
	sw	v0,R_CAUSE*4(k1)
	lw	v0,EF_SP*4(k1)
	sw	v0,R_SP*4(k1)
	sw	zero,R_ZERO*4(k1)	# we don't trust anything
	sw	v1,R_V1*4(k1)
	sw	a0,R_A0*4(k1)
	sw	a1,R_A1*4(k1)
	sw	a2,R_A2*4(k1)
	sw	a3,R_A3*4(k1)
	sw	t0,R_T0*4(k1)
	sw	t1,R_T1*4(k1)
	sw	t2,R_T2*4(k1)
	sw	t3,R_T3*4(k1)
	sw	t4,R_T4*4(k1)
	sw	t5,R_T5*4(k1)
	sw	t6,R_T6*4(k1)
	sw	t7,R_T7*4(k1)
	sw	s0,R_S0*4(k1)
	sw	s1,R_S1*4(k1)
	sw	s2,R_S2*4(k1)
	sw	s3,R_S3*4(k1)
	sw	s4,R_S4*4(k1)
	sw	s5,R_S5*4(k1)
	sw	s6,R_S6*4(k1)
	sw	s7,R_S7*4(k1)
	sw	t8,R_T8*4(k1)
	sw	t9,R_T9*4(k1)
	li	k0,0xbad00bad		# make it obvious we can't save this
	sw	k0,R_K0*4(k1)
	sw	k1,R_K1*4(k1)
	sw	fp,R_FP*4(k1)
	sw	gp,R_GP*4(k1)
	sw	ra,R_RA*4(k1)
	mflo	v0
	sw	v0,R_MDLO*4(k1)
	mfhi	v0
	sw	v0,R_MDHI*4(k1)

	.set	noreorder
	mfc0	v0,C0_INX
	NOP_1_3
	sw	v0,R_INX*4(k1)
	NOP_1_3

	mfc0	v0,C0_RAND		# save just to see it change
	NOP_1_3
	sw	v0,R_RAND*4(k1)
	NOP_1_3
	
	mfc0	v0,C0_TLBLO
	NOP_1_3
	sw	v0,R_TLBLO*4(k1)
	NOP_1_3

	mfc0	v0,C0_TLBHI
	NOP_1_3
	sw	v0,R_TLBHI*4(k1)
	NOP_1_3

	mfc0	v0,C0_CTXT
	NOP_1_3
	sw	v0,R_CTXT*4(k1)
	NOP_1_3

	# now save the R4K-specific cp0 regs
	mfc0	v0,C0_TLBLO_1
	NOP_1_3
	sw	v0,R_TLBLO1*4(k1)
	NOP_1_3

	mfc0	v0,C0_PGMASK
	NOP_1_3
	sw	v0,R_PGMSK*4(k1)
	NOP_1_3

	mfc0	v0,C0_TLBWIRED
	NOP_1_3
	sw	v0,R_WIRED*4(k1)
	NOP_1_3

	mfc0	v0,C0_COUNT
	NOP_1_3
	sw	v0,R_COUNT*4(k1)
	NOP_1_3

	mfc0	v0,C0_COMPARE
	NOP_1_3
	sw	v0,R_COMPARE*4(k1)
	NOP_1_3

	mfc0	v0,C0_CONFIG
	NOP_1_3
	sw	v0,R_CONFIG*4(k1)
	NOP_1_3

	mfc0	v0,C0_LLADDR
	NOP_1_3
	sw	v0,R_LLADDR*4(k1)
	NOP_1_3

	mfc0	v0,C0_WATCHLO
	NOP_1_3
	sw	v0,R_WATCHLO*4(k1)
	NOP_1_3

	mfc0	v0,C0_WATCHHI
	NOP_1_3
	sw	v0,R_WATCHHI*4(k1)
	NOP_1_3

	mfc0	v0,C0_ECC
	NOP_1_3
	sw	v0,R_ECC*4(k1)
	NOP_1_3

	mfc0	v0,C0_CACHE_ERR
	NOP_1_3
	sw	v0,R_CACHERR*4(k1)
	NOP_1_3

	mfc0	v0,C0_TAGLO
	NOP_1_3
	sw	v0,R_TAGLO*4(k1)
	NOP_1_3

	mfc0	v0,C0_TAGHI
	NOP_1_3
	sw	v0,R_TAGHI*4(k1)
	NOP_1_3

	mfc0	v0,C0_ERROR_EPC
	NOP_1_3
	sw	v0,R_ERREPC*4(k1)
	NOP_1_3
	.set	reorder


	lw	v0,EF_SR*4(k1)
	and	v0,SR_CU1
	beq	v0,zero,nosave
	swc1	$f0,R_F0*4(k1)
	swc1	$f1,R_F1*4(k1)
	swc1	$f2,R_F2*4(k1)
	swc1	$f3,R_F3*4(k1)
	swc1	$f4,R_F4*4(k1)
	swc1	$f5,R_F5*4(k1)
	swc1	$f6,R_F6*4(k1)
	swc1	$f7,R_F7*4(k1)
	swc1	$f8,R_F8*4(k1)
	swc1	$f9,R_F9*4(k1)
	swc1	$f10,R_F10*4(k1)
	swc1	$f11,R_F11*4(k1)
	swc1	$f12,R_F12*4(k1)
	swc1	$f13,R_F13*4(k1)
	swc1	$f14,R_F14*4(k1)
	swc1	$f15,R_F15*4(k1)
	swc1	$f16,R_F16*4(k1)
	swc1	$f17,R_F17*4(k1)
	swc1	$f18,R_F18*4(k1)
	swc1	$f19,R_F19*4(k1)
	swc1	$f20,R_F20*4(k1)
	swc1	$f21,R_F21*4(k1)
	swc1	$f22,R_F22*4(k1)
	swc1	$f23,R_F23*4(k1)
	swc1	$f24,R_F24*4(k1)
	swc1	$f25,R_F25*4(k1)
	swc1	$f26,R_F26*4(k1)
	swc1	$f27,R_F27*4(k1)
	swc1	$f28,R_F28*4(k1)
	swc1	$f29,R_F29*4(k1)
	swc1	$f30,R_F30*4(k1)
	swc1	$f31,R_F31*4(k1)
	cfc1	v0,$30
	sw	v0,R_C1_EIR*4(k1)
	cfc1	v0,$31
	sw	v0,R_C1_SR*4(k1)
nosave:
	j	ra
	END(save_state)
*/
/*
 * restore_state -- resume execution of mainline code
 */
/*
#include "ml.h"
LEAF(restore_state)
	GETREGS
	lw	v0,R_SR*4(k1)
	and	v0,SR_CU1
	beq	v0,zero,1f	
	.set	noreorder
	mtc0	v0,C0_SR
	.set	reorder
	lwc1	$f0,R_F0*4(k1)
	lwc1	$f1,R_F1*4(k1)
	lwc1	$f2,R_F2*4(k1)
	lwc1	$f3,R_F3*4(k1)
	lwc1	$f4,R_F4*4(k1)
	lwc1	$f5,R_F5*4(k1)
	lwc1	$f6,R_F6*4(k1)
	lwc1	$f7,R_F7*4(k1)
	lwc1	$f8,R_F8*4(k1)
	lwc1	$f9,R_F9*4(k1)
	lwc1	$f10,R_F10*4(k1)
	lwc1	$f11,R_F11*4(k1)
	lwc1	$f12,R_F12*4(k1)
	lwc1	$f13,R_F13*4(k1)
	lwc1	$f14,R_F14*4(k1)
	lwc1	$f15,R_F15*4(k1)
	lwc1	$f16,R_F16*4(k1)
	lwc1	$f17,R_F17*4(k1)
	lwc1	$f18,R_F18*4(k1)
	lwc1	$f19,R_F19*4(k1)
	lwc1	$f20,R_F20*4(k1)
	lwc1	$f21,R_F21*4(k1)
	lwc1	$f22,R_F22*4(k1)
	lwc1	$f23,R_F23*4(k1)
	lwc1	$f24,R_F24*4(k1)
	lwc1	$f25,R_F25*4(k1)
	lwc1	$f26,R_F26*4(k1)
	lwc1	$f27,R_F27*4(k1)
	lwc1	$f28,R_F28*4(k1)
	lwc1	$f29,R_F29*4(k1)
	lwc1	$f30,R_F30*4(k1)
	lwc1	$f31,R_F31*4(k1)
	lw	v0,R_C1_EIR*4(k1)
	ctc1	v0,$30
	lw	v0,R_C1_SR*4(k1)
	ctc1	v0,$31
1:
	lw	a0,R_A0*4(k1)
	lw	a1,R_A1*4(k1)
	lw	a2,R_A2*4(k1)
	lw	a3,R_A3*4(k1)
	lw	t0,R_T0*4(k1)
	lw	t1,R_T1*4(k1)
	lw	t2,R_T2*4(k1)
	lw	t3,R_T3*4(k1)
	lw	t4,R_T4*4(k1)
	lw	t5,R_T5*4(k1)
	lw	t6,R_T6*4(k1)
	lw	t7,R_T7*4(k1)
	lw	s0,R_S0*4(k1)
	lw	s1,R_S1*4(k1)
	lw	s2,R_S2*4(k1)
	lw	s3,R_S3*4(k1)
	lw	s4,R_S4*4(k1)
	lw	s5,R_S5*4(k1)
	lw	s6,R_S6*4(k1)
	lw	s7,R_S7*4(k1)
	lw	t8,R_T8*4(k1)
	lw	t9,R_T9*4(k1)
	lw	k1,R_K1*4(k1)
	lw	gp,R_GP*4(k1)
	lw	fp,R_FP*4(k1)
	lw	ra,R_RA*4(k1)
	lw	v0,R_MDLO*4(k1)
	mtlo	v0
	lw	v1,R_MDHI*4(k1)
	mthi	v1

	.set	noreorder
	lw	v0,R_INX*4(k1)
	NOP_1_3
	mtc0	v0,C0_INX
	NOP_1_3

	lw	v1,R_TLBLO*4(k1)
	NOP_1_3
	TLBLO_FIX_250MHz(C0_TLBLO)	# 250MHz R4K workaround
	mtc0	v1,C0_TLBLO
	NOP_1_3

	lw	v0,R_TLBHI*4(k1)
	NOP_1_3
	mtc0	v0,C0_TLBHI
	NOP_1_3

	lw	v1,R_CTXT*4(k1)
	NOP_1_3
	mtc0	v1,C0_CTXT
	NOP_1_3

	lw	v0,R_CAUSE*4(k1)
	NOP_1_3
	mtc0	v0,C0_CAUSE		# for software interrupts

	# add R4K-specific regs now
	lw	v0,R_TLBLO1*4(k1)
	NOP_1_3
	TLBLO_FIX_250MHz(C0_TLBLO_1)	# 250MHz R4K workaround
	mtc0	v0,C0_TLBLO_1

	lw	v1,R_PGMSK*4(k1)
	NOP_1_3
	mtc0	v1,C0_PGMASK
	NOP_1_3

	lw	v0,R_WIRED*4(k1)
	NOP_1_3
	mtc0	v0,C0_TLBWIRED
	NOP_1_3

	lw	v0,R_COMPARE*4(k1)
	NOP_1_3
	mtc0	v0,C0_COMPARE
	NOP_1_3

	lw	v1,R_WATCHLO*4(k1)
	NOP_1_3
	mtc0	v1,C0_WATCHLO
	NOP_1_3

	lw	v0,R_WATCHHI*4(k1)
	NOP_1_3
	mtc0	v0,C0_WATCHHI
	NOP_1_3

	lw	v1,R_TAGLO*4(k1)
	NOP_1_3
	mtc0	v1,C0_TAGLO
	NOP_1_3

	lw	v0,R_SR*4(k1)
	NOP_1_3
	mtc0	v0,C0_SR
	NOP_1_4
	NOP_1_4
	.set	reorder

	lw	sp,R_SP*4(k1)
	lw	v0,R_V0*4(k1)
	lw	v1,R_V1*4(k1)
	addiu	ra,8
	.set	noat
	.set	noreorder
	lw	AT,R_AT*4(k1)
	j	ra
	.set	reorder
	.set	at
	END(restore_state)
*/

LEAF(newstack)
#if _MIPS_SIM == _ABI64
	addu	a3,a0,a1	# base of new stack
#else
	addu	t5,a0,a1	# base of new stack
#endif
	li	t0,6		# save 5 words of stack, + old sp
	sll	t0,t0,2		# number of bytes
#if _MIPS_SIM == _ABI64
	subu	t2,a3,t0	# start for copy
	move	a4,t2		# save new sp 
#else
	subu	t2,t5,t0	# start for copy
	move	t6,t2		# save new sp 
#endif
	addu	t1,t0,sp	# copy end address
	move	t3,sp		# copy start address
#if _MIPS_SIM == _ABI64
	sw	sp,(a3)		# save old sp
loop:
	lw	a2,(t3)		# get a word
#else
	sw	sp,(t5)		# save old sp
loop:
	lw	t4,(t3)		# get a word
#endif
	addiu	t3,t3,4		# bump count
#if _MIPS_SIM == _ABI64
	sw	a2,(t2)		# store the word
#else
	sw	t4,(t2)		# store the word
#endif
	addiu	t2,t2,4		# and bump the destination count
	blt	t3,t1,loop	# do it again

#if _MIPS_SIM == _ABI64
	move	sp,a4		# set top of new stack
	move	fp,a4		# set new frame
#else
	move	sp,t6		# set top of new stack
	move	fp,t6		# set new frame
#endif
	j	ra		# back to caller
END(newstack)

LEAF(oldstack)

#if _MIPS_SIM == _ABI64
	addu	a3,a0,a1	# base of new stack
	lw	sp,(a3)		# get back old sp
#else
	addu	t5,a0,a1	# base of new stack
	lw	sp,(t5)		# get back old sp
#endif
	move	fp,sp		# get back old frame
	j	ra		# back to caller
END(oldstack)

LEAF(runcached)
	subu	v0,ra,K0SIZE
1:	j	v0
	END(runcached)

LEAF(uncached)
	addu	v0,ra,K0SIZE
1:	j	v0
	END(uncached)

LEAF( DoExceptionEret )
	.set	noreorder
	mtc0	ra,C0_EPC
	nop
	nop
	nop
	eret
	.set	reorder
	END(DoExceptionEret)

LEAF( DoErrorEret )
	.set	noreorder
	dmtc0	ra,C0_EPC
	nop
	nop
	nop
	eret
	.set	reorder
	END(DoErrorEret)

/* sd_hitinv(vaddr): Secondary Data cache HIT INValidate.
 * invalidates secondary and primary cache blocks if virtual address is valid
 * a0: virtual address to invalidate if present */
LEAF(sd_hitinv)
	.set	noreorder
#ifdef TFP
	DMTC0(t1,C0_BADVADDR)
        DMTC0(zero,C0_DCACHE)           # clear all (4) Valid bits
        dctw                            # write to the Dcache Tag
        ssnop; ssnop; ssnop             # pipeline hazard prevention

#else
	cache	CACH_SD|C_HINV, 0(a0)
#endif
	nop
	nop
	j       ra
	nop
	.set	reorder
	END(sd_hitinv)

LEAF(get_count)
	.set	noreorder
	mfc0	v0,C0_COUNT
	nop
	j	ra
	nop
	.set	reorder
	END(get_count)

LEAF(set_count)
	.set	noreorder
	mtc0	a0,C0_COUNT
	nop
	.set	reorder
	j	ra
	END(set_count)

LEAF(get_compare)
	.set	noreorder
#ifndef TFP
	mfc0	v0,C0_COMPARE
	nop
#endif
	j	ra
	nop
	.set	reorder
	END(get_compare)

/* side effect of setting compare register: clears the timer interrupt */
LEAF(set_compare)
	.set	noreorder
#ifndef TFP
	mtc0	a0,C0_COMPARE
	nop
#endif
	.set	reorder
	j	ra
	END(set_compare)



