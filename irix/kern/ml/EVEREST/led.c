/*
 * File: led.c
 * Purpose: Set led patters on CPU board. This only works on IP25. The
 *	    routines are NOPs for other everest platforms.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.10 $"

/*
 * LED handling
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/ktime.h>
#include <sys/EVEREST/everest.h>


#if IP25
#define	LED_VALUE	0x8421248
#define	LED_RATE	(HZ/8)
#else
#define	LED_VALUE	0x0
#define LED_RATE	(HZ/10)
#endif

#define	LED_FLIP	0x01
static int led_rate = LED_RATE;

/*
 * Initialize led counter and value
 */
void
reset_leds(void)
{
	private.p_led_counter = 1;
	private.p_led_value = LED_VALUE;
}

/*
 * Change leds value to next value.  Used by clock handler to show a
 * visible heartbeat.
 */
void
bump_leds(void)
{
	if (--private.p_led_counter == 0) {
		private.p_led_counter += led_rate;

#if IP25
		EV_SET_LOCAL(EV_LED_BASE,private.p_led_value&LED_CYCLE_MASK);
		if (!(private.p_led_value & ~LED_CYCLE_MASK)) { /* reload */
		    private.p_led_value = LED_VALUE;
		}
		private.p_led_value >>= LED_CYCLE_SHFT;
#else
		private.p_led_value ^= LED_FLIP;
#endif
	}
}

/*
 * Set the leds to a particular pattern.  Used during startup, and during
 * a crash to indicate the cpu state.
 */

/* ARGSUSED */
void
set_leds(int pattern)
{
#if IP25
        private.p_led_value = pattern;
	EV_SET_LOCAL(EV_LED_BASE, pattern);
#endif
}

