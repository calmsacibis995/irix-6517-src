
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <asm.h>
#include <regdef.h>

#include "1MB-100MHZ.h"

/*
 * Startup code for use with SAMPLE.s.  
 * Currently NOT USED by any real prom code!
 */

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
/* boot exception vector handler */
LEAF(bev_general)
	mfc0	a0,C0_CAUSE
	jal	hub_led_set
	HALT
	END(bev_general)

LEAF(bev_cacheerror)
	mfc0	a0,C0_CACHE_ERR
	jal	hub_led_set
	HALT
	END(bev_cacheerror)

LEAF(bev_utlbmiss)
	mfc0	a0,C0_CAUSE
	jal	hub_led_set
	HALT
	END(bev_utlbmiss)

LEAF(bev_xutlbmiss)
	mfc0	a0,C0_CAUSE
	jal	hub_led_set
	HALT
	END(bev_xutlbmiss)

LEAF(notimplemented)
	HALT
	END(notimplemented)
	.set	reorder
