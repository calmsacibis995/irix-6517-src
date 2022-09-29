/* ledprom - this PROM flashs the LED in a loop */

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <asm.h>
#include <regdef.h>

#if EMULATION
#define WAITCOUNT       2000
#else
#define WAITCOUNT       200000
#endif

LEAF(start)
				/* 0x000 offset */
	j	realstart	/* prom entry point */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x040 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x080 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x0c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x100 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x140 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x180 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x1c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x200 offset */
	j	bev_utlbmiss	/* utlbmiss boot exception vector, 32 bits */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x240 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x280 offset */
	j	bev_xutlbmiss	/* utlbmiss boot exception vector, 64 bits */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x2c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x300 offset */
	j	bev_cacheerror	/* cache exception vector */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x340 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
				/* 0x380 offset */
	j	bev_general	/* general exception vector */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented

realstart:
	.set noreorder
	li	a0,SR_KX|SR_BEV	/* 64-bits addressing, use PROM based */
	mtc0	a0,C0_SR	/* exception handlers, mask all interrupts */
	mtc0	zero,C0_CAUSE	/* clear all pending exceptions */
	.set reorder

	LI	a0,PHYS_TO_COMPATK1(HEART_MODE)
	ld	v0,0(a0)
	dli	v1,~HM_TRIG_SRC_SEL_MSK
	and	v0,v1
	dli	v1,HM_TRIG_REG_BIT
	or	v0,v1
	sd	v0,0(a0)

	LI	a0,PHYS_TO_COMPATK1(HEART_TRIGGER)
	li	v0,0x1		/* set pin initially */
	sd	v0,0(a0)

1:
	li	a1,WAITCOUNT

	.set	noreorder
2:	bne	a1,zero,2b
	sub	a1,1
	.set	reorder

	xor	v0,1		/* switch pin state */
	sd	v0,0(a0)

	b	1b
	END(start)

/* boot exception vector handler */
LEAF(bev_general)
	.set	noreorder
	mfc0	v0,C0_CAUSE
	nop
	sd	v0,0(a0)
	.set	reorder
1:
	b	1b
	END(bev_general)

LEAF(bev_cacheerror)
	.set	noreorder
	mfc0	v0,C0_CACHE_ERR
	nop
	sd	v0,0(a0)
	.set	reorder
1:
	b	1b
	END(bev_cacheerror)

LEAF(bev_utlbmiss)
	.set	noreorder
	mfc0	v0,C0_CAUSE
	nop
	sd	v0,0(a0)
	.set	reorder
1:
	b	1b
	END(bev_utlbmiss)

LEAF(bev_xutlbmiss)
	.set	noreorder
	mfc0	v0,C0_CAUSE
	nop
	sd	v0,0(a0)
	.set	reorder
1:
	b	1b
	END(bev_xutlbmiss)

LEAF(notimplemented)
1:
	b	1b
	END(notimplemented)
