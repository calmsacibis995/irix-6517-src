
/*
 * SAMPLE - this file makes it easy to write prom-based code segments
 *
 * Instructions:
 * =============
 * Copy this file to xxx.s
 * Modify it as you wish.
 * Type "make xxx" to build it.
 */

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <asm.h>
#include <regdef.h>
#include <sys/SN/agent.h>
#include "ip27prom.h"


	.set	reorder
LEAF(entry)
	LIGHT_HUB_LED0(0)
	LIGHT_HUB_LED1(0)


	/* ADD YOUR CODE HERE.  */

	/* STOP ADDING YOUR CODE HERE. */


	LIGHT_HUB_LED0(0xff)
	LIGHT_HUB_LED1(0xff)
1:	b	1b		# Loop here forever
	END(entry)
