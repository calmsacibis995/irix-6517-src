#include <sys/sbd.h>
#include <asm.h>
#include <regdef.h>

/*
 * deathprom - this prom reads from the PROM in a loop
 */

LEAF(start)
#ifdef R4000			/* force 64bit mode and intrs off */
	.set noreorder
	li	a0, SR_KX
	MTC0	(a0, C0_SR)
	.set reorder
#endif

#ifdef TFP			/* make sure interrupts are cleared and off */
	.set noreorder
	li	v0, SR_CU1|SR_FR
	DMTC0	(v0, C0_SR)
	DMTC0	(zero, C0_CAUSE)
	.set reorder
#endif

	LA	a0, p1
	LA	a1, p2
1:
	ld	t0, 0(a0)
	ld	t1, 0(a1)
	b	1b
	END(start)

	.data
p1:	.dword	0x5555555555555555
p2:	.dword	0xaaaaaaaaaaaaaaaa
