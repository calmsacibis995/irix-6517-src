#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"
/* #include "passfail.h" */
#include "testdefines.h"

#ifdef SHORT
#define PASSMASK        0x1
#else
#define PASSMASK        0xfff
#endif


	.globl ENTRY
	.set noreorder
	.set noat

/*
  R6 = GCOUNT (get instruction count)
  R3 = counter register, set to 600000000
  R31 counts interations.  
  PRINT R3
  Get my pid, shift it left, add it to counter register
loop:
  REST
  GCOUNT (get instruction count)
  if bottom 12 bits (PASSMASK) of iteration count (r31) 
      PRINT iteration count, instruction count
      GCOUNT (get instruction count)
  R31 = R31 + 1
  if instruction count is less than 60000000
      goto loop
  REST
  PRINT M_PROCNUM message
  get my pid and PRINT it
  PRINT M_ICOUNT message
  get my GCOUNT and PRINT it
  PRINT M_NUMPROCS message
  get number of proces running and PRINT it
  PASS
*/

ENTRY:
	DEBUG_PRINT_ID(0x10101002)
	li	r31, 0		# count number of passes through loop
	SYS_GCOUNT()		# Get count
	or	r6, RPARM, r0	# Copy count to RPARM

#ifdef SHORT
	li	r3, 1000
#else
	li	r3, 1000000 # 60000000
#endif
	
	SYS_GPID()		# Get my PID
	dsll	r3, r3, RPARM
	daddu	r3, r6, r3

loop:
	SYS_REST()

	/* 
	If PASSMASK bits of iteration counter are all zero, print
	iteration number and then print the instruction count.
	*/
	li      r1, PASSMASK
	and     r1, r31
	bnez    r1, 1f                  # Print pass number?
	nop

	DEBUG_PRINT_PASSNUM(r31)	# print pass num
	SYS_GCOUNT()			# Get instruction count
	DEBUG_PRINT_REG(RPARM)		# and print it

1:
	sltu	r6, RPARM, r3	
	bne	r0, r6, loop
	daddi	r31, 1

	SYS_REST()

	DEBUG_SYS_MSG(M_PROCNUM)

	SYS_GPID()
	DEBUG_PRINT_REG(RPARM)

	DEBUG_SYS_MSG(M_ICOUNT)	

	SYS_GCOUNT()
	DEBUG_PRINT_REG(RPARM)	

	DEBUG_SYS_MSG(M_NUMPROCS)	

	SYS_GNPROC()
	DEBUG_PRINT_REG(RPARM)

	PASS()
