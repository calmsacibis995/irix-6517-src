#include <regdef.h>
#include "nsys.h"

#define MAGIC 0xbeeff00d

	.globl ENTRY
	.set noreorder
ENTRY:
	li	RSYS, GSHARED	# Get the address of the shared address space
	syscall			
	li	RSYS, REST
	syscall
	syscall
	li	r6, MAGIC
	sw 	r6, 0(RPARM)
	syscall
	li 	RSYS, NPASS	# Resched	
	syscall	

