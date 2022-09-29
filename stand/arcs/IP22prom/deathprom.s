#include <asm.h>
#include <regdef.h>

/*
 * deathprom - this prom reads from the PROM in a loop
 */

LEAF(start)
	la	a0, p1
	la	a1, p2
1:
	lw	t0, 0(a0)
	lw	t1, 0(a1)
	b	1b
	END(start)

	.data
p1:	.word	0x55555555
p2:	.word	0xaaaaaaaa
