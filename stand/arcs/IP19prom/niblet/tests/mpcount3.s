#include "nsys.h"
#include "regdef.h"

        .globl ENTRY
	.set noreorder
	.set noat

#define SHARE_OFFSET    768             /* Allows multiple MP tests */

#define BAR_A		SHARE_OFFSET + 0
#define BAR_B		SHARE_OFFSET + 4
#define BAR_C		SHARE_OFFSET + 8

#define COUNTER		SHARE_OFFSET + 128


#define SCR		r1	/* Scratch register */
#define SHMEM		r8	/* Start of shared memory */

#define	NUMPROCS	3
#define	ITER		8	/* How many numbers to add */
#define PASSES		3	/* How many times to add them */
#define	SUM		(NUMPROCS * (ITER * (ITER + 1) / 2))

/* A is the offset from the shared memory start, and b is a register
	containing the amount to add */

#define ATOMIC_ADD(a, b)		\
254:	ll      SCR, a( SHMEM );	\
	nop;				\
        add	SCR, SCR, b;		\
	sc	SCR, a( SHMEM );	\
	nop;				\
	beq	SCR, r0, 254b;		\
	nop

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
	addi	SCR, SCR, -NUMPROCS;	\
	bne	r0, SCR, 253b;		\
	nop

ENTRY:
	li	RSYS, GSHARED
	syscall
	or	SHMEM, RPARM, RPARM
	li	r6, PASSES		# r6 = passes
mainloop:
	sw	r0, COUNTER(SHMEM)
	BARRIER(BAR_A)			# Barrier A
	sw	r0, BAR_C(SHMEM)	# Reset barrier C
	li	r9, ITER		# r9 = counter

countloop:
	ATOMIC_ADD(COUNTER, r9)
	addi	r9, r9, -1
	bne	r9, r0, countloop
	nop

	BARRIER(BAR_B)			# Barrier B
	sw	r0, BAR_A(SHMEM)	# Reset barrier A

	lw	r7, COUNTER(SHMEM)
	li	RPARM, SUM
	bne	RPARM, r7, wrong
	nop

	BARRIER(BAR_C)			# Barrier C
	sw	r0, BAR_B(SHMEM)	# Reset barrier B

	addi	r6, r6, -1
	bne	r6, r0, mainloop
	nop

	li	RSYS, NPASS
	syscall
	nop

wrong:
	li	RSYS, NFAIL
	syscall
	nop



	
