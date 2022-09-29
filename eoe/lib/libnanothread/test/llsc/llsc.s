#include <asm.h>
#include <regdef.h>

LEAF(main)
	.set noreorder
	PTR_ADDI	sp, -256
	sd		zero, 8(sp)
	sd		zero, 136(sp)
1:
	lld		v0, 8(sp)
 # On R10K llbit for a0 is cleared by the load to a1
 #	ld		a0, 136(sp)
 #	ld		a1, 144(sp)
 # On R4K it is possible to store to a different address, than that for
 # for which the llbit is set.
	dadd		v1, v0, -1
	scd		v1, 128(sp)
	beql		v1, zero, 1b
	nada
	jr		ra
	.set reorder
END(main)
