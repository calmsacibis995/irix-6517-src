/*
 * ledprom - this PROM flashs the LED in a loop
 *
 * Code was taken from IP30prom and modified for IP27.
 */

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <asm.h>
#include <regdef.h>

#define IP27_CONFIG_1MB_62_5MHZ
#include <sys/SN/SN0/ip27config.h>

#define SETPC() dla k0,27f;j k0;nop;27:

#define INIT_COP1	0

#define DELAY_SECOND	100000

#ifdef SABLE
#define DELAY_FLASH	2000
#else
#define DELAY_FLASH	(DELAY_SECOND / 4)
#endif

#define HALT	99: b 99b

	.set	reorder

LEAF(start)
				/* 0x000 offset */
	j	entry		/* prom entry point */
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
				/* 0x060 offset */
				/* R10000 boot mode bits */
	.word	CONFIG_TIME_CONST
	.word	CONFIG_CPU_MODE

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
				/* 0x3c0 offset */
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented
	j	notimplemented

	END(start)

	.set	noreorder

LEAF(entry)			/* 0x400 offset */

#if 0
	/* adjust MD prom params */
	dli	a0, LOCAL_HUB(MD_MEMORY_CONFIG)
	ld	k0, 0(a0)
	li	a2, 1
	dsll	a2, a2, 16
	dsll	a2, a2, 16
	dsll	a2, a2, 21
	or	k0, k0, a2 
	sd	k0, 0(a0)
	ld	k0, 0(a0)
#endif

#if 0
	/* turn on debug signals */
	dli	a0, LOCAL_HUB(0x2000c0)
	li	a1, 0xb
	sd	a1, 0(a0)
	ld	a2, 0(a0)
	andi	a2, 0x0f
	bne	a1, a2, badvalue
	nop
#endif

        lui     k0,1                    /* Turn off branch prediction */
        mtc0  k0, C0_BRDIAG             /*  (BPmode = 1, all not taken) */

#if INIT_COP1
	li	a0,SR_CU1|SR_FR|SR_KX|SR_BEV
#else /* INIT_COP1 */
	li	a0,SR_KX|SR_BEV
#endif /* INIT_COP1 */

	mtc0	a0,C0_SR	/* exception handlers, mask all interrupts */

	SETPC()

	/*
 	 * FLASH LEDS
	 *
	 * All CPU 0 LEDs will alternate with all CPU 1 LEDs.
	 */
	
	/* get new sn00 led lcoations */
	dli	a0, LOCAL_HUB(MD_UREG1_0)
	li	v0, 0
1:
	sd	v0, 0(a0)
	not	v0
	
	/* extra code for sn00 since the leds aren't offset by 8 as in lego */
	dli	a0, LOCAL_HUB(MD_UREG1_4)
	sd	v0, 0(a0)

	/* return state of a0 */
	dli	a0, LOCAL_HUB(MD_UREG1_0)
	li	a2, DELAY_FLASH

2:	bnez	a2, 2b
	sub	a2, 1

	b	1b
	 nop


badvalue:
	dli	a0, LOCAL_HUB(MD_UREG1_0)
	sd	a2, 0(a0)
	dli	a0, LOCAL_HUB(MD_UREG1_4)
	sd	a1, 0(a0)
	b	badvalue
	nop
	
	END(entry)

/* boot exception vector handler */
LEAF(bev_general)
XLEAF(bev_cacheerror)
XLEAF(bev_utlbmiss)
XLEAF(bev_xutlbmiss)
	mfc0	v0,C0_CAUSE
        lui     a0, 0x9200
        dsll    a0, a0, 16
        ori     a0, a0, 0x100
        dsll    a0, a0, 16
        lui     a2, 0x22
        daddu   a2, a2, a0

	# bits 7..0 of cause register
	li	v0, 0x55
	sd	v0,80(a2)
	not	v0
	sd	v0,88(a2)

	HALT
	END(bev_general)

LEAF(bev_cacheerror)
	mfc0	v0,C0_CACHE_ERR
	sd	v0,0(a0)
	HALT
	END(bev_cacheerror)

LEAF(bev_utlbmiss)
	mfc0	v0,C0_CAUSE
	sd	v0,0(a0)
	HALT
	END(bev_utlbmiss)

LEAF(bev_xutlbmiss)
	mfc0	v0,C0_CAUSE
	sd	v0,0(a0)
	HALT
	END(bev_xutlbmiss)

LEAF(notimplemented)
	HALT
	END(notimplemented)
