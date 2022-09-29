#include "regdef.h"
#include "asm.h"
/*
 * The label is after the third nop since the minimum section size is 16
 * bytes and there is no reason to execute 4 nops when one will do so that
 * the label gets in the right section.
 */
	.text .init
	.set	noreorder
	nop
	nop
	nop
LEAF(__istart)
	nop
	END(__istart)
	.set	reorder
