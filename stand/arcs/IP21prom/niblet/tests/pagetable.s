#include <regdef.h>
#include "nsys.h"

	.globl ENTRY
	.set noat
	.set noreorder

ENTRY:
	li	RSYS, WPTE	# Write page table entry
	li	r2, 7		# virtual page # 7
	li	r3, 0x16e	# page frame # 5 = address 0x5000

	syscall

	li	r4, 0xbeefcafe	
	li	r5, 0x7000
	sw	r4, 0(r5)
	nop
	lw	r6, 0(r5)
	nop
	bne	r4, r6, failed
	nop

	li	RSYS, NPASS	# Passed the test
	syscall

failed:
	li	RSYS, NFAIL	# Failed the test
	syscall
