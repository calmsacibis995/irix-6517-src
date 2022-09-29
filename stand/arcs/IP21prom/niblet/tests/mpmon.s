/* MP Monotonicity test.  Requires two processors to run. */

#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"
#include "testdefines.h"

#define ARRAY_SIZE	16

#define NUM_PROCS	2

#define SHARE_OFFSET	5120	/* Allows multiple MP tests */
#define BAR_A		SHARE_OFFSET + 0
#define BAR_B		SHARE_OFFSET + 4
#define BAR_C		SHARE_OFFSET + 8
#define CPU_NUM		SHARE_OFFSET + 12
#define A_OFF		SHARE_OFFSET + 16	/* 5136 */
#define X_OFF		SHARE_OFFSET + 20	/* 5140 */

#define ITERATIONS	10

#define SHMEM		r3
#define SCR		r1

/*
	This test must be run with two tasks running this test at the same time.  
	Task 1 enters a loop where it writes a 4 word sequence of monotonically increasing
	integers.  Task 2 enters a loop where it copies that sequence to another array.
	Both task 1 and task 2 check the array that the sequence is copied to to
	make sure it is correct.

	This test uses BARRIERS to synchronize the writing and reading of the 4 word
	sequence.  Task 1 and task 2's loops look like this

        task 1                              task 2
	iter1: 16 times                         iter2: 16 times
		BARRIER_A                           	BARRIER_A
		reset BARRIER_A
    	loop1: 4 times                          loop2:  4 times
    		sw value                                BARRIER B
    		BARRIER_B                               load value that task 1 just stored
    		reset BARRIER B                         store that value to new array
    		BARRIER C                               BARRIER C
    		reset BARRIER C                         branch to loop2
    		add 4 to value I am storing
    		branch to loop1
    
    	checkit()                               checkit()
	

	
*/

	.globl ENTRY
	.set noat
	.set noreorder
	.globl	ltzfail
	.globl	gtkfail
	.globl	gti1fail
	.globl	checkit
	.globl	lastcheck
	.globl	iter1
	.globl	loop1
	.globl	iter2
	.globl	loop2
	.globl	checkloop
	.globl	cpu1code
	.globl	cpu2code


ENTRY:
/*
	li	RSYS, SUSPND	# Suspend scheduling.
	syscall
	# Can't suspend scheduling if you'll ever run on fewer than two CPUs.

*/
	DEBUG_PRINT_ID(0x1010100d)
	SYS_GSHARED()
	or	SHMEM, RPARM, RPARM	# Put shared memory start in SHMEM
	UNIQUE_NUM(CPU_NUM, r7, 1)

	# Print our "CPU" number.
	DEBUG_PRINT_REG(r7)

	li	r1, 2
	beq	r1, r7, cpu2code
	nop

cpu1code:
	li	r7, ITERATIONS
iter1:
	li	r8, 4			# r8 = i
	li	r9, ARRAY_SIZE 		# r9 = upper bound of loop

	# Finish set-up and wait at barrier.
loop1:
	DEBUG_PRINT(0xa)
	BARRIER(BAR_A)
	BARRIER_CLEAR(BAR_C)
	sw	r8, A_OFF(SHMEM)
	BARRIER(BAR_B)
	BARRIER_CLEAR(BAR_A)
	BARRIER(BAR_C)
	BARRIER_CLEAR(BAR_B)
	bne	r8, r9, loop1
	addi	r8, r8, 4

	DEBUG_PRINT(0xc)
	jal	checkit
	nop

	addi	r7, r7, -1
	bne	r7, r0, iter1
	nop

	PASS()
	nop

cpu2code:
	li	r7, ITERATIONS
iter2:
	addi	r8, SHMEM, X_OFF	# r8 now contains SHMEM + X_OFF
	li	r9, ARRAY_SIZE		# r9 = k
	add	r9, r8, r9		# r9 = upper bound of loop
	
loop2:
	DEBUG_PRINT(0xa)
	BARRIER(BAR_A)
	BARRIER(BAR_B)
	lw	r1, A_OFF(SHMEM)	# Get A
	nop
	sw	r1, 0(r8)		# X[i] := A
	BARRIER(BAR_C)
	addi	r8, r8, 4		# i++
	bne	r8, r9, loop2
	nop

	DEBUG_PRINT(0xc)
	jal	checkit
	nop

	addi	r7, r7, -1
	bne	r7, r0, iter2
	nop

	PASS()

ltzfail:
	FAIL()
gtkfail:
	FAIL()
gti1fail:
	FAIL()

checkit:
	DEBUG_PRINT(0xc)
	addi	r8, SHMEM, X_OFF	# r8 now contains SHMEM + X_OFF
	li	r9, ARRAY_SIZE		# r9 = k
	add	r9, r8, r9		# r9 = upper bound of loop
checkloop:
	lw	r6, 0(r8)		# tmp := X[i]
	lw	RPARM, 4(r8)		# tmp2 := X[i + 1]
	addi	r8, r8, 4		# i++
	beq	r8, r9, lastcheck	# i + 1 == k (can't check X[i+1])
	nop
	slt	r1, RPARM, r6		# If X[i] > X[i + 1], fail
	bne	r1, r0, gti1fail
	nop
	bltz	r6, ltzfail		# If X[i] < 0, fail
	nop
	li	RPARM, ARRAY_SIZE		# r2 = k
	slt	r1, RPARM, r6		# If X[i] > k, fail
	bne	r1, r0, gtkfail		# if !(X[i] < k) fail
	nop
	j	checkloop
	nop
lastcheck:
	bltz	r6, ltzfail		# If X[i] < 0, fail
	nop
	li	RPARM, ARRAY_SIZE		# r2 = k
	slt	r1, RPARM, r6		# If X[i] > k, fail
	bne	r1, r0, gtkfail		# if !(X[i] < k) fail
	nop

	j	r31
	nop
