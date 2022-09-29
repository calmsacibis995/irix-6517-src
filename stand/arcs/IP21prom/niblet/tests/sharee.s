#include <regdef.h>
#include "nsys.h"

#define MAGIC 0xbeeff00d

	.globl ENTRY
	.set noreorder
ENTRY:
	li	RSYS, GSHARED	# Get the address of the shared address space
	syscall			
loop:
	lw 	r3, 0(RPARM)
	nop
	bne	r3, r0, go_on
	nop
	li 	RSYS, REST	# Resched	
	syscall
	j loop
	nop
go_on:
	li	r6, MAGIC
	bne	r6, r3, die
	nop

	li	RSYS, NPASS
	syscall

die:
	li	RSYS, NFAIL
	syscall

