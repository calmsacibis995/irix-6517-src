/* mprove.s 
 *		Roving Producer Test
 */

#include "nsys.h"
#include <regdef.h>
	
	.globl ENTRY
	.set noreorder
	.set noat

	.globl	try_plock
	.globl	try_clock
	.globl	checkfull
	.globl	checkempty
	.globl	pdone
	.globl	cdone
	.globl	wrong

#define SHARE_OFFSET    256     	/* Allows multiple MP tests */

#define HEAD	SHARE_OFFSET + 0	/* 256 */
#define PPASS	SHARE_OFFSET + 4	/* 260 */
#define TAIL	SHARE_OFFSET + 8	/* 264 */
#define CPASS	SHARE_OFFSET + 12	/* 272 */
#define PLOCK	SHARE_OFFSET + 128	/* 384 */
#define CLOCK	SHARE_OFFSET + 256	/* 512 */
#define QSTART	SHARE_OFFSET + 384	/* 640 */

#define QSIZE	32	/* How big's the queue */
#define QMASK	31	/* Mask to and pointers with */

#define	PASSES	100	/* How many times around the queue */

#define SHMEM	r3

/* If we get lock, y = 1, else y = 0 */
#define C_LOCK( y ) \
253:	ll      y, CLOCK( SHMEM );			\
	nop;                                            \
	bne     y, r0, 254f;				\
	or	y, r0, r0;				\
	addi    y, y, 1;				\
	sc      y, CLOCK( SHMEM );			\
254:	nop

/* If we get lock, y = 1, else y = 0 */
#define P_LOCK( y ) \
253:	ll      y, PLOCK( SHMEM );			\
	nop;                                            \
	bne     y, r0, 254f;				\
	or	y, r0, r0;				\
	addi    y, y, 1;				\
	sc      y, PLOCK( SHMEM );			\
254:	nop

#define C_UNLOCK \
	sw      r0, CLOCK(SHMEM)

#define P_UNLOCK \
	sw      r0, PLOCK(SHMEM)

	.set	noreorder
	.set	noat

	.globl	ENTRY

ENTRY:
	li      RSYS, GSHARED   	# Get address of shared address space

	syscall

	or	SHMEM, RPARM, RPARM

try_plock:
	P_LOCK(r9)

	li	RSYS, PRNTHEX
	li	RPARM, 0xaaaa
	syscall

	beq	r0, r9, try_clock
	nop

	lw	r8, PPASS(SHMEM)
	li	r9, PASSES
	
	beq	r8, r9, pdone
	lw	r6, HEAD(SHMEM)		# Load in delay slot
	addi	r8, r8, 1		# Increment pass number (load delay)
	sw	r8, PPASS(SHMEM)

	addi	r7, r6, 4		# r7 = future head
checkfull:
	li	RSYS, PRNTHEX
	li	RPARM, 0xcccc
	syscall

	lw	r1, TAIL(SHMEM)
	andi	r7, r7, QMASK
	beq	r1, r7, checkfull
	add	r9, SHMEM, r6		# Compute HEAD + SHMEM

	xor	r8, r6, r8		# Xor HEAD and pass number
	sw	r8, QSTART(r9)		# Enqueue item
	sw	r7, HEAD(SHMEM)		# Store new head
	
	P_UNLOCK

try_clock:
	C_LOCK(r9)

	li	RSYS, PRNTHEX
	li	RPARM, 0xbbbb
	syscall

	beq	r0, r9, try_plock
	nop

	lw	r7, CPASS(SHMEM)
	li	r9, PASSES - 1		# Get num passes (load delay)
	beq	r8, r9, cdone		# Load in delay slot
	lw	r6, TAIL(SHMEM)		# r6 = tail

	addi	r7, r7, 1		# Increment pass number
	sw	r7, CPASS(SHMEM)

checkempty:
	li	RSYS, PRNTHEX
	li	RPARM, 0xdddd
	syscall

	lw	r1, HEAD(SHMEM)
	add	r9, SHMEM, r6		# r9 = tail + SHMEM
	beq	r1, r6, checkempty
	xor	r8, r6, r7		# Xor TAIL and pass number into r8

	addi	r7, r6, 4		# r7 = New TAIL
	andi	r7, r7, QMASK

	lw	r6, QSTART(r9)		# Load item into r6
	sw	r7, TAIL(SHMEM)

	bne	r8, r6, wrong		# if r8 != r6, fail

	C_UNLOCK

	j try_plock
	nop

pdone:
	P_UNLOCK
	j	try_clock
	nop

cdone:
	C_UNLOCK
	li	RSYS, NPASS
	syscall
	nop

wrong:
	li	RSYS, NFAIL
	syscall
	nop
