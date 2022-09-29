/***********************************************************************\
*	File:		ledprom.s					*
*									*
*	Contains an incredibly simple prom which simply flashes the	*
*	LEDS.  Used to test very basic hardware functionality.		*
*									*
\***********************************************************************/

#ident	"$Revision: 1.9 $"

#include "ml.h"
#include <asm.h>
#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "ip21prom.h"
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
 * to ensure that this file is loaded at 0x900000001fc00000 so that
 * the vectors appear in the memory locations expected by
 * the TFP.
 */

LEAF(start)

	/*
  	 * Set the status register values.
	 *	Enable the FP coprocessor.
	 */
	dli	v0, PROM_SR
	dmtc0	v0,C0_SR		# Put SR into known state
	NOP_MTC0_HAZ
	dmtc0	zero, C0_CAUSE		# Clear software interrupts
	NOP_MTC0_HAZ

	/*
	 * Now flash the LEDS in an aesthetically pleasing 
	 * manner (this particular pattern is dedicated to
	 * Rick Bahr, who made it all happen).
	 */
	dli	a0, EV_LED_BASE
	li	t0, 1
Loop:
	li	t1, 8 

1:					# DO 
	sd	t0, 0(a0)		#   Switch on the leds
	DELAY(1)			#   Wait for 1/10 of a second
	bne	t0, t1, 1b		# WHILE (led_value != 8) 
	dsll	t0, 1			# (BD) Shift the led_value left 

	dsrl	t0, t0, 2		# Set the value to 64
	li	t1, 1			# Set the guard value to 1
2:					# DO
	sd	t0, 0(a0)		#   Switch the LEDS
	DELAY(1)			#   Wait for awhile
	bne	t0, t1, 2b		# WHILE (led_value != 1) 
	dsrl	t0, 1			# (BD) Shift the led_value right

	j	Loop			# Jump to the top and do it again
	li	t0, 2			# (BD) Reload bit value
END(start)
