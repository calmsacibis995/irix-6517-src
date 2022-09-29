/* MP Monotonicity test.  Requires two processors to run. */

#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"

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

/* a is the offset from SHMEM, b is the register in which to return a
	unique integar */
#define UNIQUE_NUM(a, b)		\
254:	ll	SCR, a( SHMEM );	\
	nop;				\
	addi	SCR, SCR, 1;		\
	or	b, SCR, SCR;		\
	sc	SCR, a ( SHMEM );	\
	nop;				\
	beq	SCR, r0, 254b;		\
	nop

/* a is the offset from the shared memory start, and b is a register
	containing the amount to add */

#define ATOMIC_ADD(a, b)		\
254:	ll      SCR, a( SHMEM );	\
	nop;				\
        add	SCR, SCR, b;		\
	sc	SCR, a( SHMEM );	\
	nop;				\
	beq	SCR, r0, 254b;		\
	nop

/* a is the offset from the shared memory start, and b is the constant
	to add */

#define ATOMIC_ADDI(a, b)		\
254:	ll      SCR, a( SHMEM );	\
	nop;				\
        addi	SCR, SCR, b;		\
	sc	SCR, a( SHMEM );	\
	nop;				\
	beq	SCR, r0, 254b;		\
	nop

#define BARRIER(x)			\
	ATOMIC_ADDI(x, 1);		\
253:	lw	SCR, x(SHMEM);		\
	nop;				\
	addi	SCR, SCR, -NUM_PROCS;	\
	bne	r0, SCR, 253b;		\
	nop

ENTRY:
/*
	li	RSYS, SUSPND	# Suspend scheduling.
	syscall
	# Can't suspend scheduling if you'll ever run on fewer than two CPUs.

*/
	li      RSYS, GSHARED   # Get the address of the shared address space
	syscall
	or	SHMEM, RPARM, RPARM	# Put shared memory start in SHMEM
	UNIQUE_NUM(CPU_NUM, r7)

	# Print our "CPU" number.
	or	RPARM, r7, r7
	li	RSYS, PRNTHEX
	syscall

	li	r1, 2
	beq	r1, r7, cpu2code
	nop

cpu1code:
	li	r7, ITERATIONS
iter1:
	li	r8, 4			# r8 = i
	li	r9, ARRAY_SIZE 		# r9 = upper bound of loop

	# Finish set-up and wait at barrier.
	li	RPARM, 0xa
	li	RSYS, PRNTHEX
	syscall
	BARRIER(BAR_A)
	sw	r0, BAR_C(SHMEM)	# Clear barrier C
loop1:
	sw	r8, A_OFF(SHMEM)
	bne	r8, r9, loop1
	addi	r8, r8, 4

	li	RPARM, 0xb
	li	RSYS, PRNTHEX
	syscall
	BARRIER(BAR_B)
	sw	r0, BAR_A(SHMEM)	# Clear barrier A

	jal	checkit
	nop

	li	RPARM, 0xc
	li	RSYS, PRNTHEX
	syscall
	BARRIER(BAR_C)
	sw	r0, BAR_B(SHMEM)	# Clear barrier B

	addi	r7, r7, -1
	bne	r7, r0, iter1
	nop

	li	RSYS, NPASS
	syscall
	nop

cpu2code:
	li	r7, ITERATIONS
iter2:
	addi	r8, SHMEM, X_OFF	# r8 now contains SHMEM + X_OFF
	li	r9, ARRAY_SIZE		# r9 = k
	add	r9, r8, r9		# r9 = upper bound of loop
	
	# Finish set-up and wait at barrier.
	li	RPARM, 0xa
	li	RSYS, PRNTHEX
	syscall
	BARRIER(BAR_A)
	sw	r0, BAR_C(SHMEM)	# Clear barrier C
loop2:
	lw	r1, A_OFF(SHMEM)	# Get A
	nop
	sw	r1, 0(r8)		# X[i] := A
	addi	r8, r8, 4		# i++
	bne	r8, r9, loop2
	nop

	li	RPARM, 0xb
	li	RSYS, PRNTHEX
	syscall
	BARRIER(BAR_B)
	sw	r0, BAR_A(SHMEM)	# Clear barrier A

	jal	checkit
	nop

	li	RPARM, 0xc
	li	RSYS, PRNTHEX
	syscall
	BARRIER(BAR_C)
	sw	r0, BAR_B(SHMEM)	# Clear barrier B

	addi	r7, r7, -1
	bne	r7, r0, iter2
	nop

	li	RSYS, NPASS
	syscall
	nop

ltzfail:
	li	RSYS, NFAIL
	syscall
	nop
gtkfail:
	li	RSYS, NFAIL
	syscall
	nop
gti1fail:
	li	RSYS, NFAIL
	syscall
	nop

checkit:
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
