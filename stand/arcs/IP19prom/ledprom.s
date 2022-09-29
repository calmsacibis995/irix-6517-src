/***********************************************************************\
*	File:		ledprom.s					*
*									*
*	Contains an incredibly simple prom which simply flashes the	*
*	LEDS.  Used to test very basic hardware functionality.		*
*									*
\***********************************************************************/

#ident	"$Revision: 1.3 $"

#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "ip19prom.h"
#include "prom_leds.h"
#include "prom_config.h"

#define DELAY(_x)			\
	li	t8, 110000 * (_x) ;	\
99:	bne	t8, zero, 99b ;		\
	sub	t8, 1


		.text
		.set 	noreorder
		.set	at
/*
 * The power-on vector table starts here.  It is important
 * to ensure that this file is loaded at 0xbfc00000 so that
 * the vectors appear in the memory locations expected by
 * the R4000.
 * 
 * If we are running under NEWT, this file must be loaded at
 * address 0x80000000.
 */

#define JUMP(_label)	j _label ; nop

LEAF(start)
	JUMP(entry)			# 0xbfc00000	
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x040 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x080 offset */
        JUMP(bev_xtlbrefill)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x0c0 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x100 offset */
        JUMP(bev_ecc)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x140 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x180 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x1c0 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x200 offset */
        JUMP(bev_tlbrefill)	/* 32-bit EXL==0 TLB exc vector */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x240 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x280 offset */
        JUMP(bev_xtlbrefill)	/* Extended addressing TLB exc vector */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x2c0 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x300 offset */
        JUMP(bev_ecc)		/* Cache error boot exception vector */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x340 offset */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
                                /* 0x380 offset */
        JUMP(bev_general)	/* General exception vector */
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
        JUMP(notimplemented)
END(start)
 
/*
 * entry -- When the CPU begins executing it jumps
 *	through the power-on vector to this point.
 *	This routine initializes the R4000 and starts 
 *	basic system configuration.
 */

LEAF(entry)

	/*
  	 * Set the status register values.
	 *	Use exception vectors, disable ECC reporting,
	 *	and enable the FP coprocessor.
	 */
	li	v0, PROM_SR
	mtc0	v0,C0_SR		# Put SR into known state
	nop
	nop
	mtc0	zero, C0_CAUSE		# Clear software interrupts
	nop
	nop
	mtc0	zero, C0_WATCHLO	# Clear all watchpoints
	nop
	nop
	mtc0	zero, C0_WATCHHI	#
	nop

	/*
	 * Now flash the LEDS in an aesthetically pleasing 
	 * manner (this particular pattern is dedicated to
	 * Rick Baur, who made it all happen).
	 */
	li	a0, EV_LED_BASE
	li	t0, 1
Loop:
	li	t1, 8 

1:					# DO 
	sd	t0, 0(a0)		#   Switch on the leds
	DELAY(1)			#   Wait for 1/10 of a second
	bne	t0, t1, 1b		# WHILE (led_value != 8) 
	sll	t0, 1			# (BD) Shift the led_value left 

	srl	t0, t0, 2		# Set the value to 64
	li	t1, 1			# Set the guard value to 1
2:					# DO
	sd	t0, 0(a0)		#   Switch the LEDS
	DELAY(1)			#   Wait for awhile
	bne	t0, t1, 2b		# WHILE (led_value != 1) 
	srl	t0, 1			# (BD) Shift the led_value right

	j	Loop			# Jump to the top and do it again
	li	t0, 2			# (BD) Reload bit value
END(entry)



LEAF(notimplemented)
	li	a0, EV_LED_BASE
	li	t0, 0xff
	sd	t0, 0(a0)

1:	b	1b
	nop
END(notimplemented)


LEAF(bev_xtlbrefill)
	li	a0, EV_LED_BASE
	li	t0, 0xe1
	sd	t0, 0(a0)

1:	b	1b
	nop
END(bev_xtlbrefill)


LEAF(bev_tlbrefill)
	li	a0, EV_LED_BASE
	li	t0, 0xe3
	sd	t0, 0(a0)
	
1:	b	1b
	nop
END(bev_tlbrefill)


LEAF(bev_ecc)
	li	a0, EV_LED_BASE
	li	t0, 0xe7
	sd	t0, 0(a0)
	
1:	b	1b
	nop
END(bev_ecc)


LEAF(bev_general)

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop				# Give things a chance to stabilize

	mfc0	t1, C0_CAUSE
	li	a0, EV_LED_BASE
	li	t0, 0xef
1:	
	sd	t0, 0(a0)
	DELAY(10)
	sd	t1, 0(a0)
	DELAY(10)
	b	1b
	nop
END(bev_general)

