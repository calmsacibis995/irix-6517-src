#include <regdef.h>
#include "nsys.h"

	.globl ENTRY
	.set noreorder

ENTRY:
	li	r2, 0xaaaaaaaa
	li	RSYS, PRNTHEX
	syscall

	li	RSYS, ORDAIN
	syscall

	li	r2, 0xbbbbbbbb
	li	RSYS, PRNTHEX
	syscall

	li	RSYS, DEFROCK
	syscall

	li	r2, 0xcccccccc
	li	RSYS, PRNTHEX
	syscall

	li	RSYS, NPASS
	syscall

