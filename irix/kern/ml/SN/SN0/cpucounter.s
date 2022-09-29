/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Miscellaneous SN0 assembly language stubs.  Stuff that is 
 * basically processor-independent goes here.
 */

#include <ml/ml.h>

/*
 * Return the current value of the R10K count register.
 */
LEAF(_get_count)
XLEAF(get_r4k_counter)
	.set	noreorder
	DMFC0(v0, C0_COUNT)
	NOP_0_4
	.set	reorder
	j	ra
	END(_get_count)

/*
 * Return the current value of the R10K compare register.
 */
LEAF(_get_compare)
	.set	noreorder
	DMFC0(v0, C0_COMPARE)
	NOP_0_4
	.set	reorder
	j	ra
	END(_get_compare)

/*
 * Set the count register to the specified value.
 */
LEAF(_set_count)
	.set	noreorder
	DMTC0(a0, C0_COUNT)
	.set	reorder
	j	ra
	END(_set_count)


/*
 * Routine to set count and compare system coprocessor registers,
 * used for the scheduling clock.
 *
 * We mess around with the count register, so it can't be used for
 * other timing purposes.
 */
LEAF(resetcounter)
        .set    noreorder
        mfc0    t1,C0_SR
        NOP_0_4
        li      t0,SR_KADDR
        mtc0    t0,C0_SR                # disable ints while we muck around
        NOP_0_4
        mfc0    t0,C0_COUNT
        mfc0    t2,C0_COMPARE
        li      t3,1000                 # "too close for comfort" value
        NOP_0_3                         # (be safe)
        sltu    ta1,t0,t2               # if COUNT < COMPARE
        bne     ta1,zero,1f             #  then COUNT has wrapped around
        subu    ta0,t0,t2               #   and we're way way past!
        sltu    ta1,a0,ta0              # if (COUNT-COMPARE)
        bne     ta1,zero,1f             #  exceeds interval, then way past!
        subu    ta0,t0,t2               #   and we're way way past!
        sltu    ta1,a0,ta0              # if (COUNT-COMPARE)
        bne     ta1,zero,1f             #  exceeds interval, then way past!
        subu    a0,ta0                  # decrement overage from next interval
        sltu    ta1,a0,t3               # if next COMPARE
        beq     ta1,zero,2f             #  is really soon, then...
        nop
1:
        move    a0,t3                   # go with an "immediate" interrupt
2:
        mtc0    zero,C0_COUNT           # finally, start COUNT back at zero,
        NOP_0_4                         #  (be safe)
        mtc0    a0,C0_COMPARE           # and set up COMPARE for next interval
        NOP_0_4                         #  (be safe)
        mtc0    t1,C0_SR                # reenable ints (don't need NOPs after)
        j       ra
        nop
        .set    reorder
        END(resetcounter)

