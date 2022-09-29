/* slocktest.s 
 *		Soft lock test
 */

#include "nsys.h"
#include <regdef.h>
#include "testdefines.h"
	
	.globl ENTRY
	.set noreorder

#define SHARE_OFFSET    0       	/* Allows multiple MP tests */

#define FLAG	SHARE_OFFSET + 0
#define LOCK	SHARE_OFFSET + 8
#define TURN	SHARE_OFFSET + 12
#define WHO	SHARE_OFFSET + 16	/* 16 byte array of flags */
#define VALUE	SHARE_OFFSET + 32

#define SCR2	r1
#define CPUNUM	r9
#define SCRATCH	r6
#define SHMEM	r7
#define COUNTER r10

#define NUMTIMES	16
#define WAITCOUNT	10

	.globl	releaselock
	.globl	getlock
	.globl	toploop
	.globl	wholoop
	.globl	skipself
	.globl	foundit

	.set	noat

	# Figure out which two processes we are.
/*
	while (other == 0)
		for (i = 1; i < 16; i++) {
			if (i == PID)
				continue;
			if (who[i] != 0) {
				other = i;
				break;
			}
		}
	
	if (other > PID)
		CPUNUM = 0;
	else
		CPUNUM = 4;
*/
	/* ********************************************************************
       Summary:
       The basic idea of this test is that each of the two mpslock tasks
       store to and read back from the same one location.  The store/read
       sequence is a critical section and is protected by locks, so a task
       should always get back the data that it stored.  If it doesn't, it fails


       This test is intended to run as a two task pair.  It *must*
       have a 'sister' task, which is running this test because at the
       start of the test it waits for its 'sister' to start up and write
       to a location in the who array.  If more than two of this test are
       running, I think that the Peterson's algorithm used to implement
       critical regions will not work!  But it is ok if other tasks that
       are not running this test (mpslock.s) are running with the pair
       of mpslock tests.

            SHMEM = R7 = GET SHMEM ADDRESS
            R8 = MY PID
            store '1' into [SHMEM + MY_PID + 16]
        toploop:
            COUNTER = 15
    --> wholoop
   |        if MY PID == COUNTER, goto skipself
   |        SCRATCH = [SHMEM + COUNTER + 16]
   |        if (SCRATCH != 0) goto foundit
   |    skipself:
   |        COUNTER = COUNTER - 1
    ------  if COUNTER > 0 goto wholoop

        j toploop and try again if didn't find it yet.

		
		foundit:
			if COUNTER < MY PID (if his id is less than mine)
				CPUNUM = 0
				goto dotest
			else 
				CPUNUM = 4


        The dotest loop just stores a value to a location, waits a few
        cycles, then reads it back to make sure it is the same value.  Before
        it does the store it gets the lock and after it does the store, it
        releases the lock, insuring that store/load sequence is protected
        as a critical region.

		dotest:
			COUNTER = NUMTIMES
     -> testloop:
    |      getlock()
    |      R3 = COUNTER * 16 + MY PID
    |      sw R3 into VALUE[SHMEM] = [SHMEM + 32]
    |      SCRATCH = WAITCOUNT = 16
    |      while (SCRATCH)
    |          SCRATCH = SCRATCH - 1
    |      SCR2 = VALUE[SHMEM]
    |      if SCR2 != R3
    |          goto failed
    |      else
    |          releaselock()
     ----- if COUNTER-- > 0 got0 testloop

	********************************************************************/
            

ENTRY:
	DEBUG_PRINT_ID(0x10101007)
	SYS_GSHARED()				# Get address of shared memory
	or	SHMEM, RPARM, RPARM		# Copy address to SHMEM

	SYS_GPID()
	or	r8, RPARM, RPARM		# Copy PID to r8

	li	SCRATCH, 1
	add	SCR2, r8, SHMEM			# shared memory + PID offset
	sb	SCRATCH, WHO(SCR2)		# who[pid] := 1

toploop:
	li	COUNTER, 15
wholoop:
	beq	COUNTER, r8, skipself	# Don't check our own slot.
	add	SCR2, COUNTER, SHMEM		# Add offset to start of shared mem.	
	lb	SCRATCH, WHO(SCR2)	# Get byte flag
	nop
	bne	r0, SCRATCH, foundit	
	nop
skipself:
	addi	COUNTER, COUNTER, -1		# Decrement r8
	bne	COUNTER, 0, wholoop
	nop

	j	toploop
	nop

foundit:
	sgt	SCRATCH, COUNTER, r8		# other < PID
	beq	r0, SCRATCH, cpuzero
	nop
	li	CPUNUM, 4		# We're CPU one
	j	dotest
	nop
cpuzero:
	li	CPUNUM, 0		# We're CPU zero
dotest:
	
	li	COUNTER, NUMTIMES

	/* While (COUNTER > 0) { */
testloop:
	jal	getlock

	sll	r3, COUNTER, 4		# Shift counter over 4
	add	r3, r3, r8		# Add in PID

	sw	r3, VALUE(SHMEM)	# Store our value into shared mem.

	li	SCRATCH, WAITCOUNT
spin:
	bgtz	SCRATCH, spin
	addi	SCRATCH, -1

	lw	SCR2, VALUE(SHMEM)
	nop
	bne	SCR2, r3, failed
	nop

	jal	releaselock
	nop

	bgtz	COUNTER, testloop		# Loop if counter > 0
	addi	COUNTER, COUNTER, -1		# Decrement counter

	PASS()

failed:
	FAIL()


/********************************************************************
   getlock() & releaselock() implement Peterson's algorithm.  See pg
   56 of Tanenbaum's Operating Systems Design.  The algorithm is:

      getslock(process)
      {
          other = 1 - process;
          interested[process] = TRUE
          turn = process;
          while (turn == process && interested[other] == TRUE) ;
      }

      releaselock(process)
      {
          interested[process] = FALSE;
      }
************************************************************************/
releaselock:
	add	SCRATCH, SHMEM, CPUNUM		# flag[i]
	sw	r0, FLAG(SCRATCH)		# flag[i] := 0
	j	ra
	nop


getlock:
	add	SCRATCH, SHMEM, CPUNUM		# shmem + i * 4
	li	SCR2, 4
	sw	SCR2, FLAG(SCRATCH)		# flag[i] := 4
	sub	SCR2, SCR2, CPUNUM		# SCR2 := j
	add	SCRATCH, SHMEM, SCR2		# shmem + j * 4
	sw	SCR2, TURN(SHMEM)		# turn := j
	nop
254:	lw	SCR2, TURN(SHMEM)		# SCR2 := turn
	nop
	beq	SCR2, CPUNUM, 255f		# if turn <> i, skip
	lw	SCR2, FLAG(SCRATCH)		# SCR2 := flag[j]
	nop
	beq	SCR2, r0, 255f			# if flag[j]<>0, skip
	nop
	j	254b				# end of while
255:	nop					# Acquired lock!
	j	ra
	nop


