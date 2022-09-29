
#include <asm.h>
#include <regdef.h>

LEAF(UNDERFLOW_OP)
	.text
	.set	noreorder

	li	t0,0x00800000
	li	t1,0x2
	nop
	mtc1	t0,$f2
	nop
	mtc1	t1,$f4
	nop
	nop
	div.s	$f6,$f2,$f4
	nop
	.set	reorder
	j	ra

	END(UNDERFLOW_OP)
