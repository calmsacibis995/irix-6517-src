#ident	"$Id: led.s,v 1.1 1994/07/21 01:14:15 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/regdef.h"
#include "sys/asm.h"

#include "monitor.h"

/*------------------------------------------------------------------------+
| local define constant                                                   |
+------------------------------------------------------------------------*/
#define ALLLEDSLIT	0x3f
#define ALLLEDSOUT	0x00

#define SHORTWAIT	0x8000

	.text
	.align	4

/*------------------------------------------------------------------------+
| Routine name: flash_led(led_pattern)                                    |
|                                                                         |
| Description : flash_led blinks all the LEDs on and off rapid rate about |
|   1/4 second; ShortWait  provides  a delay-loop  of  approximately  1/4 |
|   second.                                                               |
|                                                                         |
| Register Usage:                                                         |
|                                                                         |
|       - a0    LEDs pattern to display.                                  |
|       - a1    saved return address.                                     |
|       - a2    saved LEDs pattern.                                       |
|       - a3    counter register.                                         |
|       - v0    scratch register.                                         |
+------------------------------------------------------------------------*/
LEAF(flash_led)
	move	a1, ra			# save our return address
	li	a3, 15			# flash 'em for about 5 secs
	move	a2, a0			# save LEDs error value to write
2:
	li	a0, ALLLEDSOUT		# turn the LEDs off
	jal	set_leds		# write to the CPU LEDs

	li	v0, SHORTWAIT		# get the delay count
1:	
	addiu	v0, v0, -1		# decrement the count
	bgtz	v0, 1b			# spin till it be 0

	move	a0, a2			# now show LEDs error value
	jal	set_leds		# write to the CPU LEDs

	li	v0, SHORTWAIT		# get the delay count
1:	
	addiu	v0, v0, -1		# decrement the count
	bgtz	v0, 1b			# spin till it be 0

	addiu	a3, a3, -1		# decrement the loop-counter
	bgtz	a3, 2b			# keep on flashin'

	j	a1			# and back to the caller
END(flash_led)


/*------------------------------------------------------------------------+
| Routine name: set_leds( led_pattern )                                   |
|                                                                         |
| Description : Write pattern to cpu board leds                           |
|                                                                         |
| Register Usage:                                                         |
|       - v0    LEDs reg pointer                                          |
|       - v1    scratch register.                                         |
+------------------------------------------------------------------------*/
LEAF(set_leds)
	li	v0, LED_BASE
	sb	a0, (v0)		# write to LEDs register.
	j	ra
END(set_leds)

