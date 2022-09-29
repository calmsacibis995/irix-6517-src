#include "sys/asm.h"
#include "sys/regdef.h"
#include "sys/fpregdef.h"

/* We seem to get loaded with alignment to 2^6 boundaries, but
 * a cache line is 2^7, and a page is 2^12.
 * The problem is that the .align directive works fine for the
 * asm phase, but breaks during the ld phase.
 * So, put in a LOT of entry points, and choose the good one at
 * run-time.
 */

.align 5	/* Align to 64-byte boundary. */
LEAF(test_00)
	j	ra
END (test_00)

/* For now, add 1 to first arg, and return it */
LEAF(data_text)
	.set	noreorder
	addi	v0,a0,1
	j	ra
	nop
	.set	reorder
END(data_text)

/*
 * a0 contains value saying whether to branch or not.
 * a1 contains address to store to in BDSLOT.
 * a2 contains value to store.
 */
LEAF(bd_store)
	.set	noreorder
	bne	a0,zero,1f
	sw	a2,0(a1)	#BDSLOT
	nop
1:
	j	ra
	nop
	.set	reorder
END(bd_store)

/*
 * a0 contains source value.
 * a1 contains address to store fp register to.
 */
LEAF(fp_store)
	.set	noreorder
	mtc1	a0,ft0
	nop
	swc1	ft0,0(a1)
	j	ra
	nop
	.set	reorder
END(fp_store)

/*
 * a0 contains first 1/2 of source value.
 * a1 contains second 1/2 of source value.
 * a2 contains address to store fp register to.
 */
LEAF(fp_dstore)
	.set	noreorder
	mtc1	a0,ft0f		# Note the order.  "f" reg goes in low
	mtc1	a1,ft0		# address.
	nop
	sdc1	ft0,0(a2)
	j	ra
	nop
	.set	reorder
END(fp_dstore)

/*
 * store_around
 * Do unaligned store operations all around a given byte.
 * a0 is the address of the byte.
 * a1 holds stuff to store.
 */
LEAF(store_around)
	li	t0,3
	and	t1,t0,a0
	beq	t0,t1,1f	# If address&3=3, no swl needed.
	swl	a1,1(a0)
1:	beq	t1,zero,2f	# If address&3=0, no swr needed.
	swr	a1,-1(a0)
2:	jr	ra
END(store_around)

/*
 * load_around
 * Do unaligned load operations all around a given byte.
 * a0 is the address of the byte.
 * v0 holds data read back.
 */
LEAF(load_around)
	li	t0,3
	and	t1,t0,a0
	beq	t0,t1,1f	# If address&3=3, no lwl needed.
	lwl	v0,1(a0)
1:	beq	t1,zero,2f	# If address&3=0, no lwr needed.
	lwr	v0,-1(a0)
2:	jr	ra
END(load_around)

LEAF(touch_cacheline)
	lb	t0,0(a0)
	.set noreorder
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	.set reorder
	li	t0,1
	sb	t0,16(a0)
	END(touch_cacheline)
	
/*
Some random junk to be cut and pasted in where necessary.
	.set	noreorder
	.set	reorder
	j	1f
1:
	nop
	nop
	nop
	nop
	nop
	nop
*/
