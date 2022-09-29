#include "nsys.h"
#include "regdef.h"

        .globl ENTRY
	.set noreorder
	.set noat

#define SHARE_OFFSET    768             /* Allows multiple MP tests */

#define BAR_A		SHARE_OFFSET + 0
#define BAR_B		SHARE_OFFSET + 4
#define BAR_C		SHARE_OFFSET + 8
#define OFFSET_ADDR	SHARE_OFFSET + 12

#define COUNTER		SHARE_OFFSET + 128

#define SCR		r1	/* Scratch register */
#define SHMEM		r8	/* Start of shared memory */
#define OFFSHMEM	r7	/* Start of shared mem offset by unique # */

#define	NUMPROCS	2
#define	ITER		32
#define PASSES		1024
#define	SUM		(ITER * (ITER + 1) / 2)

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

/* a is the offset from SHMEM, b is the register in which to return a
	unique offset */
#define UNIQUE_WOFFSET(a, b)            \
254:    ll      SCR, a( SHMEM );        \
	nop;                            \
	addi    SCR, SCR, 4;            \
	or      b, SCR, SCR;            \
	sc      SCR, a ( SHMEM );       \
	nop;                            \
	beq     SCR, r0, 254b;          \
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
	UNIQUE_WOFFSET(OFFSET_ADDR, OFFSHMEM)
	li	RSYS, PRNTHEX
	or	RPARM, OFFSHMEM, OFFSHMEM
	syscall
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
	or	RPARM, r6, r6		# (BD)
	li	RSYS, PRNTHEX
	syscall				# Print the pass number
1:
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
