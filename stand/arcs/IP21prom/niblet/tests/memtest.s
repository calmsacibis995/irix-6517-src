#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"
#include "testdefines.h"

#define PG_SZ		4096
/* NUM_PAGES used to be 20 */
/* NUM_PAGES used to be 128 */
#define NUM_PAGES	64
#ifdef SHORT
#define NUM_PASSES	20
#else
#define NUM_PASSES 256
/* #define NUM_PASSES	2 << 20 */
#endif
#define STRIDE		382	# Must be an even number
#ifdef SHORT
#define PASSMASK	0x1
#else
#define PASSMASK	0x1fff
#endif

	.globl ENTRY
	.set noat
	.set noreorder
	/* ********************************************************************
	   This test is a very basic test of storing to and loading from memory.
		It does not use any shared memory and can be run on any number of
		processors at once.  It declares its own data section and stores to
		and reads from that data section.  

		The basic idea is that this test writes to a data area using a stride
		of 382.  Every 382 bytes it stores a half word until it gets to the
		end of the data area.  Then it reads back the half words that it
		stored to make sure it gets the correct values.  Then it does the 
		whole thing again, this time storing a different constant.

		It executes the store/load/store/load loop NUM_MASSES times


            R8 = GET_MY_PID
            R31 = NUM_PASSES
 ------> repeat:
|           R3 = 0xa5a5 xor MY_PID
|   ---> start:
|  |        R7 = size of data area = (PG_SZ * NUM_PAGES) = 4096 * 20
|  |        R6 = startmem
|  |        R7 = starmem + size of data area
|  |        R9 = STRIDE = 382
|  | 		R7 = R7 - STRIDE - 2
|  |  -> wloop1:
|  | |		sh R3, 0(r6)  store 0xa5a5 xor MY_PID
|  | |		R6 = R6 + STRIDE
|  |  ----	if R6 < R7, goto wloop1
|  | 	
|  | 		r6 = startmem
|  |  -> rloop1:
|  | |      lh	r1, 0(r6)
|  | |      R6 = R6 + STRIDE
|  | |      r1 = r1 & 0xffff
|  | |      if (R1 != R3), branch to badread
|  |  ----	if R6 < R7, goto wloop1
|  | 	
|  |        R3 = ~R3
|   ------  If I just stored pattern 0xa5a5, go back to start and do it again with 0x5a5a
|   
|           If R31 = # of passes is modulo 8182 (0x1fff) PRINT pass number
|           R31 = R31 - 1
 ---------  if R31 = # passes != 0, branch to repeat
		
	************************************************************************/
ENTRY:
	DEBUG_PRINT_ID(0x10101003);
	SYS_GPID()			# Get my PID in RPARM
	or	r8, RPARM, r0		# Copy PID to r8


	li	r31, NUM_PASSES
repeat:
	li	r3, 0xa5a5		# pattern to write.
	xor	r3, r8, r3		# XOR it with the PID.

start:
	li      r7, (PG_SZ * NUM_PAGES )# Size of data area
	dla	r6, startmem		# Start of data area
	dadd	r7, r7, r6		# Compute end of data area
	li	r9, STRIDE		# Stride of _almost_ a cache line
	dsub	r7, r7, r9		# Subtract stride from end.
	li	r10, 2			# Loop the first time.
	dsub	r7, r7, r10		# Subtract 2 from end...


wloop1:
	sh	r3, 0(r6)		# Write the halfword to 0(r6)
	dadd	r6, r9, r6		# Increment r6
	bne	r10, r0, wloop1		# Branch based on last test
	sltu	r10, r6, r7		# Branch if still in data area

	dla	r6, startmem		# load start address
	li	r10, 1			# Loop the first time.
rloop1:
	lh	r1, 0(r6)		# read the halfword from 0(r6)
	dadd	r6, r9, r6		# Increment r6
	andi	r1, 0xffff		# And off top halfword	
	bne	r1, r3, badread		# If the value we put there isn't
					#	there, fail
	nop
	bne	r10, r0, rloop1		# Branch based on last test
	sltu	r10, r6, r7		# Branch if still in data area

	li	r1, 0xffff
	xor	r3, r3, r1		# Negate r3 (write pattern)
	andi	r10, r3, 0xff00		# Get second byte of pattern
	li	r1, 0x5a00		# If it's this, loop
	beq	r1, r10, start		# Do loop twice.
	nop

	li	r1, PASSMASK
	and	r1, r31
	bnez	r1, 1f			# Print pass number?
	nop
	
	DEBUG_PRINT_PASSNUM(r31)

1:
	bne	r31, 0, repeat
	daddi	r31, -1			# (BD)

	PASS()

loop:
	j loop
	nop

badread:
	PRINT_REG(r6)			# Print virtual address

	SYS_VTOP(r6)			# Print corresponding physical addr
	PRINT_REG(RPARM)

	PRINT_REG(r3)			# Print expected value
	PRINT_REG(r1)			# Print actual value read
	FAIL()

	.lcomm	startmem, PG_SZ * NUM_PAGES

