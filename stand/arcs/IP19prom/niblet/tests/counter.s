#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"

#define PASSMASK        0xfff


	.globl ENTRY
	.set noreorder
	.set noat


ENTRY:
	li	RSYS, GCOUNT	# Get count
	syscall
	or	r6, RPARM, r0	# Copy count to RPARM
	li	r3, 60000000
	li	RSYS, GPID	# Get my PID
	syscall
	sll	r3, r3, RPARM
	addu	r3, r6, r3
loop:
	li	RSYS, REST
	syscall

	li 	RSYS, GCOUNT
	syscall

        li      r1, PASSMASK
        and     r1, RPARM
        bnez    r1, 1f                  # Print pass number?
        nop

        li      RSYS, PRNTHEX
        syscall
	li 	RSYS, GCOUNT
	syscall

1:
	sltu	r6, RPARM, r3	
	bne	r0, r6, loop
	nop

	li	RSYS, REST
	syscall

	li	RSYS, MSG
	li	RPARM, M_PROCNUM
	syscall

	li 	RSYS, GPID
	syscall

	li	RSYS, PRNTHEX
	syscall
	
	li	RSYS, MSG
	li	RPARM, M_ICOUNT
	syscall

	li	RSYS, GCOUNT
	syscall
	
	li	RSYS, PRNTHEX
	syscall
	
	li	RSYS, MSG
	li	RPARM, M_NUMPROCS
	syscall

	li	RSYS, GNPROC
	syscall
	
	li	RSYS, PRNTHEX
	syscall

	li	RSYS, NPASS
	syscall
	nop
