#include <sys/asm.h>
#include <sys/regdef.h>
#include <sys/fpu.h>
#include <sys/softfp.h>
#include <sys/signal.h>
#include <sys/sbd.h>

#include "lat_op.h"

/*
 *   Inline timing code, counts successful and failed iterations,
 *   including parallel workload in between each successful iteration.
 *
 *   Branch to label BRANCH_FAIL if an operation fails.
 *
 *   Depends on arguments failp, sucp, work, and counterp being
 *   defined to registers.
 *
 *   WORK_TMP is used as a temp register and *must be saved* between
 *   invokations of WORKLOAD_WAIT.
 */

#define BRANCH_FAIL	3f
#define WORK_TMP	v1

#if defined(USE_MAPPED_COUNTER)
#error This code doesn't use work_ticks_jitter properly yet!
#define WORKLOAD_WAIT						\
	ld v0,0(counterp);					\
2:	ld v1,0(counterp);					\
	dsubu v1,v1,v0;						\
	dsub v1,v1,work;					\
	bgez v1,1b;						\
	nop;							\
	b 2b;							\
	nop
#elif defined(USE_COP0_COUNTER)
#error This code doesn't use work_ticks_jitter properly yet!
#define WORKLOAD_WAIT						\
	mfc0 v0,C0_COUNT;					\
2:	mfc0 v1,C0_COUNT;					\
	subu v1,v1,v0;						\
	sub v1,v1,work;						\
	bgez v1,1b;						\
	nop;							\
	b 2b;							\
	nop
#elif defined(USE_NOP_COUNTER)
#define WORKLOAD_INIT						\
	li WORK_TMP,0
#define WORKLOAD_WAIT						\
	daddiu WORK_TMP,WORK_TMP,1;				\
	and v0,WORK_TMP,CLOCK_JITTER_MASK;			\
	dsll v0,v0,3;						\
	daddu v0,v0,work;					\
	ld v0,0(v0);						\
2:	beq v0,zero,1b;						\
	nop;							\
	b 2b;							\
	daddiu v0,v0,-1
#else
#error No workload counter defined!
#endif

#define TIMING_BEGIN 						\
	WORKLOAD_INIT;						\
1:

#define TIMING_END 						\
	ld v0,0(sucp);		/* last op was successful */	\
	daddi v0,v0,1;						\
	sd v0,0(sucp);						\
								\
	WORKLOAD_WAIT;						\
								\
3:	ld v0,0(failp);		/* last op failed */	 	\
	daddi v0,v0,1;						\
	sd v0,0(failp);						\
	b 1b;							\
	nop

#define sharedp a0
#define failp a1
#define sucp a2
#define work a3
#define counterp a4


/* use_fetch_and_add_asm(sharedp, failp, sucp, work, counterp)

   loop forever, atomically incrementing sharedp,
   increment failp on LOCK_SC failure,
   increment sucp on LOCK_SC success.

   bad assembly code follows
*/

LEAF(use_fetch_and_add_asm)
	.set noreorder

	TIMING_BEGIN

	LOCK_LL v0,0(sharedp)		# try atomic op
	addiu v0,v0,1
	LOCK_SC v0,0(sharedp)

	beql v0,zero,BRANCH_FAIL	# did LOCK_SC fail?
	nop				# NOTE: beql for R10k LL/SC bug

	TIMING_END

	.set reorder
	END(use_fetch_and_add_asm)


/* use_fail_asm(sharedp, failp, sucp, work, counterp)

   loop forever, branching to BRANCH_FAIL
*/

LEAF(use_fail_asm)
	.set noreorder

	TIMING_BEGIN

	b BRANCH_FAIL
	nop

	TIMING_END

	.set reorder
	END(use_fail_asm)


/* use_null_asm(sharedp, failp, sucp, work, counterp)

   loop forever, doing nothing, timing the work loop
*/

LEAF(use_null_asm)
	.set noreorder

	TIMING_BEGIN

/* null operation */

	TIMING_END

	.set reorder
	END(use_null_asm)


/* use_load_asm(sharedp, failp, sucp)

   loop forever, loading a value from sharedp,
   increment sucp on each iteration
*/

LEAF(use_load_asm)
	.set noreorder

	TIMING_BEGIN

	ld v0,0(sharedp)

	TIMING_END

	.set reorder
	END(use_load_asm)

/* use_store_asm(sharedp, failp, sucp)

   loop forever, storing a value to sharedp,
   increment sucp on each iteration
*/

LEAF(use_store_asm)
	.set noreorder

	TIMING_BEGIN

	sd v0,0(sharedp)

	TIMING_END

	.set reorder
	END(use_store_asm)

#undef sharedp
#undef failp
#undef sucp
#undef work
#undef counterp

/* calibrate_nop_counter(nloops, work)

   loop for nloops, doing work ticks worth of work
*/

#define nloops a0
#define work a1

LEAF(calibrate_nop_counter)
	.set noreorder

	WORKLOAD_INIT
	move t0,nloops

1:	beq t0,zero,3f
	nop

	daddiu t0,t0,-1

	WORKLOAD_WAIT

3:	j ra
	nop

	.set reorder
	END(calibrate_nop_counter)

#undef nloops
#undef work
