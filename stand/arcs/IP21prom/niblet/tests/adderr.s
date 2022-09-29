#include <regdef.h>
#include "nsys.h"
#include "nmsg.h"

	.globl ENTRY
	.set noat
	.set noreorder
ENTRY:

loop:
	la	r3, mylabel
	lw	r4, 1(r3)

	j loop
	nop

	.data
mylabel:
	.word	0
