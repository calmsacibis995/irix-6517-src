
#include <asm.h>
#include <regdef.h>

LEAF(OVERFLOW_OP)
	.text
	.set	noreorder

	li	t0,0x7f7fffff
	nop
	mtc1	t0,$f2
	nop
	mtc1	t0,$f4
	nop
	nop
	add.s	$f6,$f2,$f4
	nop
	.set	reorder
	j	ra

	END(OVERFLOW_OP)
