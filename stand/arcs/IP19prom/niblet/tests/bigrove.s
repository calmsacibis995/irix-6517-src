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

#define SHARE_OFFSET    128     	/* Allows multiple MP tests */

#define PLOCK	SHARE_OFFSET + 0	/* 128 */
#define CLOCK	SHARE_OFFSET + 4	/* 132 */
#define HEAD	SHARE_OFFSET + 128	/* 256 */
#define PPASS	SHARE_OFFSET + 132	/* 260 */
#define TAIL	SHARE_OFFSET + 136	/* 264 */
#define CPASS	SHARE_OFFSET + 140	/* 268 */
#define QSTART	SHARE_OFFSET + 260	/* 388 */

#define QSIZE	64	/* How big's the queue */
#define QMASK	63	/* Mask to and pointers with */

#define	PASSES	(2 << 22)	/* How many times around the queue */

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
	P_LOCK(RPARM)

	beq	r0, RPARM, try_clock
	nop

	lw	r8, PPASS(SHMEM)
	li	RPARM, PASSES
	
	beq	r8, RPARM, pdone
	lw	r6, HEAD(SHMEM)		# Load in delay slot
	addi	r8, r8, 1		# Increment pass number (load delay)
	sw	r8, PPASS(SHMEM)

	addi	r7, r6, 4		# r7 = future head
checkfull:
	lw	r1, TAIL(SHMEM)
	andi	r7, r7, QMASK
	beq	r1, r7, checkfull
	add	RPARM, SHMEM, r6		# Compute HEAD + SHMEM

	xor	r8, r6, r8		# Xor HEAD and pass number
	sw	r8, QSTART(RPARM)		# Enqueue item
	sw	r7, HEAD(SHMEM)		# Store new head
	
	P_UNLOCK

try_clock:
	C_LOCK(RPARM)

	beq	r0, RPARM, try_plock
	nop

	lw	r7, CPASS(SHMEM)
	li	RPARM, PASSES		# Get num passes (load delay)
	beq	r8, RPARM, cdone		# Load in delay slot
	lw	r6, TAIL(SHMEM)		# r6 = tail

	addi	r7, r7, 1		# Increment pass number
	sw	r7, CPASS(SHMEM)

checkempty:
	lw	r1, HEAD(SHMEM)
	add	RPARM, SHMEM, r6		# RPARM = tail + SHMEM
	beq	r1, r6, checkempty
	xor	r8, r6, r7		# Xor TAIL and pass number into r8

	addi	r7, r6, 4		# r7 = New TAIL
	andi	r7, r7, QMASK

	lw	r6, QSTART(RPARM)		# Load item into r6
	sw	r7, TAIL(SHMEM)

	bne	r8, r6, wrong		# if r8 != r6, fail

	C_UNLOCK

	j try_plock
	nop

pdone:
	P_UNLOCK
	li	RSYS, VPASS
	syscall
	nop

cdone:
	C_UNLOCK
	li	RSYS, VPASS
	syscall
	nop

wrong:
	li	RSYS, NFAIL
	syscall
	nop
