/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
/*
 * File: led.c
 * Purpose: Set led patters on CPU board. This only works on IP25. The
 *	    routines are NOPs for other everest platforms.
 *
 */

#ident "$Revision: 1.12 $"

/*
 * LED handling
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/sysinfo.h>
#include <sys/ksa.h>
#include <sys/ktime.h>
#include <sys/SN/SN0/hub.h>
#include <sys/SN/SN0/IP27.h>

#ifdef NI_DEBUG
#include <sys/SN/router.h>
#endif

#define	LED_VALUE	0x8421248
#define	LED_RATE	(HZ/2)
#define NLEDSPERPROC	7	/* # of load LEDs per processor. */
#define LEDPERFMASK	((1 << NLEDSPERPROC) - 1)
#define LEDPERFSHFT	1
#define LED_HB_BIT	1	/* Heartbeat bit. */

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

#ifdef NI_DEBUG
int kill_node = -1;
int kill_mode = 0;
#endif

/*
 * Change leds value to next value.  Used by clock handler to show a
 * visible heartbeat.
 */
void
bump_leds(void)
{
	int value, curi;
	int heartbeat;
	
	if (--private.p_led_counter == 0) {

#ifdef NI_DEBUG
		if (kill_node == cnodeid()) {
			if (kill_mode == 0) {
				printf("node %d, killing local link\n",
					cnodeid());
				LOCAL_HUB_S(NI_PORT_RESET, 2);
			} else {
				router_reg_t rr;

				printf("node %d, killing link %d\n",
					cnodeid(), kill_mode);
				hub_vector_read(cnodeid(), 0, 0,
					RR_DIAG_PARMS, &rr);
				hub_vector_write(cnodeid(), 0, 0,
					RR_DIAG_PARMS, rr |
					RDPARM_DISABLE(kill_mode));
			}
		}
#endif
		private.p_led_counter += led_rate;
		heartbeat = private.p_led_value;
		heartbeat = (heartbeat ^ LED_HB_BIT) & LED_HB_BIT;
		curi = private.ksaptr->si.cpu[CPU_IDLE] +
		       private.ksaptr->si.cpu[CPU_WAIT];
		value = curi - private.p_lastidle;
		private.p_lastidle = curi;
		value *= 100;
		value /= led_rate;
		value = 100 - value;	/* convert to % active */
		/* now convert to number between 0 and NLEDSPERPROC
		 * since there are really 7 available values
		 */
		value *= NLEDSPERPROC;
		value /= 100;

		/* hold real value to set lastvalue later */
		curi = value;
		value = (value + private.p_lastvalue)/2;
		private.p_lastvalue = curi;

		/* For the moment, just set all the LEDs to the load. */
		private.p_led_value =
		    (((((1 << (value + 1)) - 1) ^ LEDPERFMASK)
					& LEDPERFMASK)
					<< LEDPERFSHFT) | heartbeat;

		SET_MY_LEDS(private.p_led_value);
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
        private.p_led_value = pattern;
	SET_MY_LEDS(private.p_led_value);
}

void
sys_setled(int pattern)
{
	set_leds(pattern);
}
