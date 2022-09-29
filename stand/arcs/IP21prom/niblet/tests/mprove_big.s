/* mprove.s 
 *		Roving Producer Test
 */

#include "nsys.h"
#include <regdef.h>
#include "testdefines.h"
	
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

#define QSIZE	64	/* How big's the queue */
#define QMASK	63	/* Mask to and pointers with */

#define	PASSES	100	/* How many times around the queue */

#define SHMEM	r3

/*
	This test is a producer/consumer test.  It implements a queue.  It must be run 
	with two tasks running this test at the same time.  Each task may either add an 
    entry to head of the queue or remove an entry from tail of the queue.  Each task 
	tries to first see if it can add the the queue, but trying to get the p lock 
	(producer lock).  If it can get the p lock, then if the queue is not full, it 
	adds an entry to the queue.  If the queue is full, it spins waiting for the queue 
	to not be full.  If a task cannot get the p lock, then it tries to get the c lock 
	(consumer lock).  If it gets the consumer lock, and if the queue is not empty, it 
	removes an entry from the queue.  If the queue is empty, it spins, waiting for 
	the other task to add an entry to the queue.

	try_plock:
		get plock
		if didn't get plock, gotto try_clock

		R8 = [PPASS + SHMEM]
		if (R8 == PASSES)
			goto pdone
		else 
			R8 = R8 + 1
			Store R8 into [PPASS + SHMEM]
		
		R6 = [HEAD + SHMEM]
		R7 = R6 + 4
	checkfull:
		R1 = [TAIL + SHMEM]
		R7 = R7 and 0x3f
		if (R1 == R7)   If next head = tail 
			goto checkfull
		else 
			RPARM = SHMEM + R6
			R8 = R6 xor R8
			store R8 into QSTART(RPARM)
			store R7 into [HEAD + SHMEM]
		P_UNLOCK		

	try_clock
		get clock
		if didn't get clock goto try_plock
		R7 = [CPASS + SHMEM]
		if (R7 == PASSES)
			goto cdone
		else 
			R7 = R7 + 1
			Store R7 into [CPASS + SHMEM]
			R6 = [TAIL + SHMEM]
	checkempty:
		R1 = [HEAD + SHMEM]
		if (R1 == R6)
			goto checkempty
		else 
			R8 = R6 xor R7

*/

	.set	noreorder
	.set	noat

	.globl	ENTRY

ENTRY:
	li	r10, 0
	li	r11, 0
	DEBUG_PRINT_ID(0x1010100a)
	SYS_GSHARED()		  	# Get address of shared address space
	or	SHMEM, RPARM, RPARM

try_plock:
	P_LOCK(r9)


#ifdef NIBTESTDEBUG_BIG
	li	RPARM, 0xaaaaa00
	or	RPARM, RPARM, r10
	DEBUG_BIG_PRINT_REG(RPARM)
#endif


	beq	r0, r9, try_clock
	nop

	lw	r8, PPASS(SHMEM)
	li	r9, PASSES
	
	beq	r8, r9, pdone
	lw	r6, HEAD(SHMEM)		# Load in delay slot
	addi	r8, r8, 1		# Increment pass number (load delay)
	daddu	r10, r8, r0
	sw	r8, PPASS(SHMEM)

	addi	r7, r6, 4		# r7 = future head
checkfull:
#ifdef NIBTESTDEBUG_BIG
	li	RPARM, 0xccccc00
	or	RPARM, RPARM, r10
	DEBUG_BIG_PRINT_REG(RPARM)
#endif

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

#ifdef NIBTESTDEBUG_BIG
	li	RPARM, 0xbbbbb00
	or	RPARM, RPARM, r11
	DEBUG_BIG_PRINT_REG(RPARM)
#endif

	beq	r0, r9, try_plock
	nop

	lw	r7, CPASS(SHMEM)
	daddu	r11, r7, r0
	li	r9, PASSES - 1		# Get num passes (load delay)
	beq	r7, r9, cdone		# Load in delay slot  /* r7 used to be r8.  I think this was a bug */

	lw	r6, TAIL(SHMEM)		# r6 = tail

	addi	r7, r7, 1		# Increment pass number
	sw	r7, CPASS(SHMEM)

checkempty:
#ifdef NIBTESTDEBUG_BIG
	li	RPARM, 0xddddd00
	or	RPARM, RPARM, r11
	DEBUG_BIG_PRINT(RPARM)
#endif
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
	DEBUG_PRINT(0xeeeeeee)
	j	try_clock
	nop

cdone:
	C_UNLOCK
	DEBUG_PRINT(0xfffffff)
	PASS()

wrong:
	FAIL()
