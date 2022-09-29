/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.6 $"

#include <ml/ml.h>

#if R4000

LEAF(clr_r4kcount_intr)
	.set	noreorder
	mtc0	zero,C0_COMPARE
	.set	reorder
	j	ra
	END(clr_r4kcount_intr)

LEAF(get_r4k_compare)
	.set	noreorder
	mfc0	v0,C0_COMPARE
	NOP_0_4
	.set	reorder
	j	ra
	END(get_r4k_compare)

LEAF(set_r4k_compare)
	.set	noreorder
	mtc0	a0,C0_COMPARE
	.set	reorder
	j	ra
	END(set_r4k_compare)

#if IP22 || IP32 || IPMHSIM
LEAF(_get_r4k_counter)
	.set	noreorder
	mfc0	v0,C0_COUNT		/* Read the Count register. */
	NOP_0_4
	.set	reorder
	j	ra
	END(_get_r4k_counter)
#endif

LEAF(get_r4k_counter)
	.set	noreorder
#ifdef IP22
	mfc0	a0, C0_SR
	lw	a1, r4000_clock_war	/* do we need the R4000 clock fix? */
	and	v0,a0,~SR_IE		/* clear IE value in v0 */
	mtc0	v0, C0_SR		/* clear IE in coprocessor 0 */
	NOP_0_1				/* required between mtc0 and mfc0 */
#endif
	mfc0	v0,C0_COUNT		/* Read the Count register. */
#ifdef IP22
	beq	a1, zero, 1f		/* not ioc1 --> done */
	mfc0	a2, C0_CAUSE		/* clock interrupt pending? */
	NOP_0_4
	and	a2, CAUSE_IP8
	bnez	a2, 1f			/* yes--nothing to do */
	nop				/* [BDSLOT] */

	/* was count close to or beyond shadow? */

	lw	a3, r4k_compare_shadow	/* count >= compare - 8 ? */
	subu	v1,v0,a3
5:
	.set	reorder
	addu	v1,8
	bgez	v1,3f			/* yes -> force an interrupt */
					/* count < compare - 40000000 ? */
	addu	a1,v1,40000000		
	bgez	a1,1f			/* no --> we are done */
	.set	noreorder

	/* force a count/compare interrupt */
3:
	lw	a1, _lr4kintrcnt	/* count forced interrupts */
	addu	a1,1
	sw	a1, _lr4kintrcnt

	/* loop until we get interrupt pending */
2:
	lw	a1, _pr4kintrcnt	/* count set loops */
	addu	a1,1
	sw	a1, _pr4kintrcnt
	
	mfc0	v1, C0_COUNT		/* set compare to */
					/*  (count + (count - base_count)) */ 
	NOP_0_4
	subu	a1,v1,v0
	blez	a1,6f			/* limit increment to 4096 */
	subu	a3,a1,4096
	blez	a3,6f
	nop
	li	a1,4096
6:
	addu	a3,a1,v1
	mtc0	a3, C0_COMPARE
	NOP_0_4

4:
	mfc0	a2, C0_CAUSE		/* clock interrupt pending? */
	NOP_0_4
	and	a2, CAUSE_IP8
	bnez	a2, 1f			/* yes--nothing to do */
	nop
	bgtz	a1,4b
	subu	a1,1
	
	/* recheck count/compare, since still no interrupt */
	mfc0	v1, C0_COUNT
	NOP_0_4
	b	5b			/* count >= compare - 8 ? */
	subu	v1,a3			/* [BDSLOT] */

	/* restore the interrupt mask */
1:
	mtc0	a0, C0_SR
	NOP_0_1
#endif
	.set	reorder
	j	ra
	END(get_r4k_counter)

/* return the content of the R4000/R4400 C0 config register */
LEAF(get_r4k_config)
	.set	noreorder
	mfc0	v0,C0_CONFIG
	.set	reorder
	j	ra
	END(get_r4k_config)
#endif /* R4000 */

#ifdef R4600

LEAF(wait_for_interrupt)
	.set	noreorder
	mfc0	t2,C0_PRID
	NOP_0_4
	.set	reorder
	andi	t2,C0_IMPMASK
	subu	t2,(C0_IMP_R4600 << C0_IMPSHIFT)
	beqz	t2,3f
	subu	t2,((C0_IMP_R4700 << C0_IMPSHIFT) - (C0_IMP_R4600 << C0_IMPSHIFT))
#ifdef TRITON	
	beqz	t2,3f
	subu	t2,((C0_IMP_TRITON << C0_IMPSHIFT) - (C0_IMP_R4700 << C0_IMPSHIFT))
	beqz	t2,3f
	subu	t2,((C0_IMP_RM5271 << C0_IMPSHIFT) - (C0_IMP_TRITON << C0_IMPSHIFT))
#endif /* TRITON */
 	bnez	t2,2f
3:	
	.set	noreorder
	mfc0	a1, C0_SR
	mtc0	zero, C0_SR
	.set	reorder
	lbu	t1,0(a0)		/* check idle flag */
	beqz	t1,1f
	.set	noreorder
	mtc0	a1, C0_SR
	NOP_0_1
	.set	reorder
2:
	j	ra

1:
	.set	noreorder
EXPORT(wait_for_interrupt_fix_loc)
	mtc0	a1,C0_SR		/* must be adjacent to avoid	*/
	c0	C0_WAIT			/* a race with an interrupt	*/
	NOP_0_4
	j	ra
	nop

	nop				/* room to slide the mtc0 and	*/
	nop				/* wait down to avoid a cache	*/
					/* miss on the wait or the	*/
					/* following nop 		*/
	.set	reorder	
	END(wait_for_interrupt)
#endif /* R4600 */
	
#ifdef _R4600_2_0_CACHEOP_WAR
/*
 *      Workaround version of eret for R4600 Rev. 2.0 parts which
 *	can't have the dcache refill buffer full when doing a cache
 *	operation (in which we might have taken an exception and
 *	are thus about to retry).
 *
 *	This workaround assumes that the refill will take at most
 *	12 cycles after the most recent load completes, that the
 *	load occurs just before the jump to _r4600_2_0_cacheop_war.
 *	The jmp and nop account for two cycles, and the eret is two more,
 *	so we need 8 nop's.
 *
 *	Startup code patches selected eret instructions to jump to
 *	this code.		 
 */
	.set	noreorder
LEAF(_r4600_2_0_cacheop_eret)
	nop ; nop ; nop ; nop
	nop ; nop ; nop ; nop
EXPORT(_r4600_2_0_cacheop_eret_inst)	
	eret	
	END(_r4600_2_0_cacheop_eret)
#endif /* _R4600_2_0_CACHEOP_WAR */

