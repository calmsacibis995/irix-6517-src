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

#ident	"$Revision: 1.9 $"

#include <ml/ml.h>

#if R10000
/*
 * Assembly language routines to read & write the performance monitoring
 * registers.
 */

		

LEAF(read_branch_diag)
	.set noreorder
	MFC0(v0,C0_BRDIAG)
	j 	ra
	nop
	.set reorder
	END(read_branch_diag)

LEAF(write_branch_diag)
        .set noreorder
        MTC0(a0,C0_BRDIAG)
        j       ra
        nop
        .set reorder
        END(write_branch_diag)

LEAF(branch_prediction_off)
	.set noreorder
	MFC0(v0,C0_BRDIAG)
	and	v0,v0,~0x30000
	or	v0,v0,0x10000
	MTC0(v0,C0_BRDIAG)
	j 	ra
	nop
	.set reorder
	END(branch_prediction_off)

LEAF(branch_prediction_on)
	.set noreorder
	MFC0(v0,C0_BRDIAG)
	and	v0,v0,~0x30000
	MTC0(v0,C0_BRDIAG)
	j 	ra
	nop
	.set reorder
	END(branch_prediction_on)

LEAF(branch_prediction_taken)
	.set noreorder
	MFC0(v0,C0_BRDIAG)
	and	v0,v0,~0x30000
	or	v0,v0,0x20000
	MTC0(v0,C0_BRDIAG)
	j 	ra
	nop
	.set reorder
	END(branch_prediction_taken)

LEAF(branch_prediction_fwd_not_taken)
	.set noreorder
	MFC0(v0,C0_BRDIAG)
	or	v0,v0,0x30000
	MTC0(v0,C0_BRDIAG)
	j 	ra
	nop
	.set reorder
	END(branch_prediction_fwd_not_taken)

/*
 * Check if the hardware perf counters overflowed and caused an interrupt.
 * This will produce a bit vector of length 4 with the bits
 * corresponding to the perf counter set.  Cuts down the expense to 1
 * function call and returns results of all 4 counters.
 */
LEAF(r12k_perf_overflow_intr)
	.set	noreorder
	MFPC(t0,PRFCNT0)
	MFPC(t1,PRFCNT1)
	slt	t0,t0,zero
	beq	t0,zero, 1f
	slt	t1,t1,zero
	MFPS(t2,PRFCRT0)
	andi	t0, t2, 0x10
1:
	beq	t1, zero, 2f
	MFPS(t2,PRFCRT1)
	andi	t1, t2, 0x10
2:
	or	t3,t0,t1

	MFPC(t0,PRFCNT2)
	MFPC(t1,PRFCNT3)
	slt	t0,t0,zero
	beq	t0,zero,3f
	slt	t1,t1,zero
	MFPS(t2,PRFCRT2)
	andi	t0,t2, 0x10
3:
	beq	t1,zero, 4f
	MFPS(t2,PRFCRT3)
	andi	t1, t2, 0x10
4:
	or	t0,t0,t1

	j	ra
	or	v0,t0,t3
	.set reorder
	END(r1nk_perf_overflow_intr) 


/*
 * Check if the hardware perf counters overflowed and caused an interrupt.
 */
LEAF(r10k_perf_overflow_intr)
        .set    noreorder
        MFPC(t0,PRFCNT0)                # hardware performance counters
        MFPC(t1,PRFCNT1)
        slt     t0,t0,zero
        beq     t0, zero, 1f
        slt     t1,t1,zero              #BDSLOT
        MFPS(t2,PRFCRT0)
        andi    t0, t2, 0x10            # interrupt enabled?
1:
        beq     t1, zero, 2f
        MFPS(t2,PRFCRT1)                #BDSLOT
        andi    t1, t2, 0x10            # interrupt enabled?
2:
        j       ra
        or      v0,t0,t1                # BDSLOT
        .set    reorder
        END(r10k_perf_overflow_intr)




/* 
 * r1nkperf_data_register_get(counter)
 *	a0 = # of the counter we are interested in
 * Read the hardware performance counter data
 * Assumption : a0 can be only 0, 1, 2 or 3
 */
LEAF(r1nkperf_data_register_get)
	.set noreorder
	
	bne a0,0,1f			# check if we are to read counter 0
	nop
	MFPC(v0,PRFCNT0)		# read performance counter 0 data
	j ra
	nop
1:
	bne a0,1,2f			# check if we are to read counter 1
	nop
	MFPC(v0,PRFCNT1)		# read performance counter 1 data
	j ra
	nop
2:
	bne a0,2,3f			# check if we are reading counter 2
	nop
	MFPC(v0,PRFCNT2)		# read performance counter 2 data
	j ra
	nop
3:
	bne a0,3,4f			# check if we are reading counter 3
	nop
	MFPC(v0,PRFCNT3)		# read performance counter 3 data
	j ra
	nop
4:
	move v0,zero			# invalid counter number 
	j ra				# return 0
	nop
	.set reorder
	END(r1nkperf_data_register_get)

/*
 * r1nkperf_control_register_get(counter)
 *      a0 = # of the counter we are interested in
 * Read the hardware performance counter control
 * Assumption : a0 can be only 0, 1, 2 or 3 
 */
LEAF(r1nkperf_control_register_get)
	.set noreorder
	
	bne a0,0,1f			# check if we are to read counter 0
	nop
	MFPS(v0,PRFCRT0)		# read performance counter 0 control
	j ra
	nop
1:
	bne a0,1,2f			# check if we are to read counter 1
	nop
	MFPS(v0,PRFCRT1)		# read performance counter 1 control
	j ra
	nop
2:
	bne a0,2,3f			# check if we are to read counter 2
	nop
	MFPS(v0,PRFCRT2)		# read performance counter 2 control
	j ra
	nop
3:
	bne a0,3,4f			# check if we are to read counter 3
	nop
	MFPS(v0,PRFCRT3)		# read performance counter 3 control
	j ra
	nop
4:
	move v0,zero			# invalid counter number 
	j ra				# return 0
	nop
	.set reorder
	END(r1nkperf_control_register_get)

/*
 * r1nkperf_data_register_set(counter,count)
 * 	a0 = # of the counter we are interested in
 * 	a1 = actual count 
 * Write the performance counter data
 * Assumption : a0 can be only 0, 1, 2 or 3
 */
LEAF(r1nkperf_data_register_set)
	.set noreorder
	
	bne a0,0,1f			# check if we are to write counter 0
	nop
	MTPC(a1,PRFCNT0)		# write performance counter 0 data
	j ra
	nop
1:
	bne a0,1,2f			# check if we are to write counter 1
	nop
	MTPC(a1,PRFCNT1)		# write performance counter 1 data
	j ra
	nop
2:
	bne a0,2,3f			# check if we are to write counter 2
	nop
	MTPC(a1,PRFCNT2)		# write performance counter 2 data
	j ra
	nop
3:
	bne a0,3,4f			# check if we are to write counter 3
	nop
	MTPC(a1,PRFCNT3)		# write performance counter 3 data
	j ra
	nop
4:
	j ra				# invalid counter number 
	nop
	.set reorder
	END(r1nkperf_data_register_set)

/*
 * r1nkperf_control_register_set(counter,event_specifier)
 * 	a0 = # of the counter we are interested in
 * 	a1 = actual control register value to be written
 * Write the performance counter control
 * Assumption : a0 can be only 0, 1, 2 or 3 
 */
LEAF(r1nkperf_control_register_set)
	.set noreorder
	
	bne a0,0,1f			# check if we are to write counter 0
	nop
	MTPS(a1,PRFCRT0)		# write performance counter 0 control
	j ra
	nop
1:
	bne a0,1,2f			# check if we are to write counter 1
	nop
	MTPS(a1,PRFCRT1)		# write performance counter 1 control
	j ra
	nop
2:
	bne a0,2,3f			# check if we are to write counter 2
	nop
	MTPS(a1,PRFCRT2)		# write performance counter 2 control
	j ra
	nop
3:
	bne a0,3,4f			# check if we are to write counter 3
	nop
	MTPS(a1,PRFCRT3)		# write performance counter 3 control
	j ra
	nop
4:
	j ra				# invalid counter number 
	nop
	.set reorder
	END(r1nkperf_control_register_set)

#endif /* R10000 */
