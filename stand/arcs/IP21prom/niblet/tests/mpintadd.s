#include "nsys.h"
#include "regdef.h"
#include "testdefines.h"

        .globl ENTRY
	.set noreorder
	.set noat

	/*
        summary: mpintadd
        This test adds n + (n - 1) + (n - 2) + ... and then confirms that the
        result it got is n * (n + 1) / 2.   It stores its intermediate results
        in a location in shared memory which is based on the pid of the task
        running, so when multiple tasks are running, they all access locations
        in shared memory that are close to each other.
		
		This test is designed to be run on exactly two processors at once.
		The barriers synchronize the two processors at the barrier points,
		and are hardcoded to synchronize two processors.  If this test is
		run on more than two processors at once, I think it will still work,
		but the synchroniziation will still only be synchronizing two processors
		at a time and the full set up mpintadd tasks that are running will not
		all stay synchronized together at the barrier points.

		This test does not do any RESTs.  If the number of mpintadd tasks is
		the same as the number of processors, then it will work without preemptive
		scheduling, but if the number of tasks (mpintadd or other tests) is greater
		than the number of processors, you must have preemptive scheduling.

	   		SHMEM = address of shared memory
			OFFSHEM = a unique number which is a multiple of 4.  First proc to
					get here will get 4, next will get 8, etc.
			print OFFSHMEM
			OFFSHMEM = OFFSHMEM + SHMEM
			R6 = # of Passes (1024)
  --->	mainloop:
 |			Store 0 into [COUNTER + OFFSHMEM] == [SHARE_OFFSET + 128 + OFFSHMEM] == [768 + 128 + OFFSHMEM]
 |			Wait at Barrier A until another process gets there
 |			Reset Barrier C
 |			R9 = # of iterations = 32
 |	 -->  countloop:
 |	|       This loop sums up iterations + (interations - 1) + (interations - 2) + ...
 |	|       which gives you the value (iterations * (iterations + 1) / 2)
 |	|		SCR = load value from [COUNTER + OFFSHMEM] = (0 that I previously stored)
 |	|		SCR = SCR + # of iterations = SCR + 32
 |	|		Store SCR back into [COUNTER + OFFSHMEM]
 |	|		R9 = R9 - 1 => # iterations = # iterations - 1
 |	 ----	if # iterations is not 0, go back to countloop
 |			else
 |			Wait at Barrier B until another process gets there
 |			Reset Barrier A
 |			SCR = load value from [COUNTER + OFFSHMEM]
 |			RPARP = SUM = (iterations * (iterations + 1) / 2)
 |			if SCR != SUM, goto wrong
 |			Wait at Barier C until another process gets there
 |			Reset Barrier B
 |		
 |			If # passes is modulo 65, PRINT pass number
 |			r6 = r6 - 1     (#passes = #passes - 1)
 ---------	if #passes > 0, goto mainloop
 
		PASS
	*/
	
#define SHARE_OFFSET    768             /* Allows multiple MP tests */

#define BAR_A		SHARE_OFFSET + 0
#define BAR_B		SHARE_OFFSET + 4
#define BAR_C		SHARE_OFFSET + 8
#define OFFSET_ADDR	SHARE_OFFSET + 12

#define COUNTER		SHARE_OFFSET + 128

#define SCR		r1	/* Scratch register */
#define SHMEM		r8	/* Start of shared memory */
#define OFFSHMEM	r7	/* Start of shared mem offset by unique # */

#define	NUM_PROCS	2
#define	ITER		32
#define PASSES		256 /* 1024 */ /* Margie changed:	 remove */
#define	SUM		(ITER * (ITER + 1) / 2)


ENTRY:
	DEBUG_PRINT_ID(0x10101006)
	SYS_GSHARED()
	or	SHMEM, RPARM, RPARM
	UNIQUE_NUM(OFFSET_ADDR, OFFSHMEM, 4)
	or	RPARM, OFFSHMEM, OFFSHMEM
	DEBUG_PRINT_REG(RPARM)
	add	OFFSHMEM, OFFSHMEM, SHMEM	# r7 = unique offset + SHMEM

	li	r6, PASSES		# r6 = passes
mainloop:
	sw	r0, COUNTER(OFFSHMEM)
	BARRIER(BAR_A)			# Barrier A
	sw	r0, BAR_C(SHMEM)	# Reset barrier C
	li	r9, ITER		# r9 = counter

countloop:
	lw	SCR, COUNTER(OFFSHMEM)
	nop
	add	SCR, r9, SCR
	sw	SCR, COUNTER(OFFSHMEM)
	addi	r9, r9, -1
	bne	r9, r0, countloop
	nop

	BARRIER(BAR_B)			# Barrier B
	sw	r0, BAR_A(SHMEM)	# Reset barrier A

	lw	SCR, COUNTER(OFFSHMEM)
	li	RPARM, SUM
	bne	RPARM, SCR, wrong
	nop

	BARRIER(BAR_C)			# Barrier C
	sw	r0, BAR_B(SHMEM)	# Reset barrier B

	andi	RPARM, r6, 0x3f
	bnez	RPARM, 1f
	nop
	DEBUG_PRINT_PASSNUM(r6)	# print pass number
1:
	addi	r6, r6, -1
	bne	r6, r0, mainloop
	nop

	PASS()

wrong:
	FAIL()
