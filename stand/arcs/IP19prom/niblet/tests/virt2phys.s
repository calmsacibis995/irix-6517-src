#include "nsys.h"
#include "regdef.h"

	.set noreorder

	.globl	ENTRY
	.globl	done
	.globl	data_label
	.globl	offset_label

ENTRY:

	li	RSYS, VTOP
	la	r2, ENTRY
	syscall

	li	RSYS, PRNTHEX
	syscall

	la	RSYS, VTOP
	la	r2, done
	syscall

	li	RSYS, PRNTHEX
	syscall

	la	RSYS, VTOP
	la	r2, data_label
	syscall

	li	RSYS, PRNTHEX
	syscall

	la	RSYS, VTOP
	la	r2, offset_label
	syscall

	li	RSYS, PRNTHEX
	syscall

done:
	li	RSYS, NPASS
	syscall


	.data
data_label:
	.word	0
	.word	0
	.word	0
offset_label:
	.word	0

