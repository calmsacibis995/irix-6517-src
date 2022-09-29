/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.25 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reg.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/ds1687clk.h>
#include <sys/hwgraph.h>
#include <sys/proc.h>
#include <ksys/xthread.h>
#include <ksys/vproc.h>
#include "RACERkern.h"
#include <sys/ksignal.h>
#include <sys/RACER/sflash.h>

/*
 * power.c - softpower support
 */

#define MAX_BASEIO 2
static vertex_hdl_t baseio_qlvertex[MAX_BASEIO];
int qlcnt = 0;

extern int clean_shutdown_time; 	/* mtune/kernel */

void
baseio_qlsave(vertex_hdl_t v)
{
	if (qlcnt < MAX_BASEIO)
		baseio_qlvertex[qlcnt] = v;
	qlcnt++;
}

static void flash_led(int on);
static void powerintr_back(void);
int qlreset(vertex_hdl_t);

extern thd_int_t *acfail_tinfo;

/*ARGSUSED*/
static void
powerfail_check(int arg)
{
	/* Note that interrupt is no longer enabled after a spurious event */
	cmn_err(CE_WARN,"\\Power failure was spurious.\n");

	/* Re-enable bridge error interrupt (may migrate from bootmaster) */
	heart_imr_bits_rmw(cpuid(),0,1ULL<<IP30_HVEC_ACFAIL);

	/* Release dallas clock */
	nested_spinunlock(&ip30clocklock);
}

void
powerfail_ql_thread(void)
{
	int adap;

	ASSERT(curthreadp);

	/*  Maybe should spin a bit before complaining?  Sometimes
	 * see this when people power off.  May want to use this
	 * for 'clean' shutdowns if we ever get enough power to
	 * write out part of the superblocks, or whatever.  For
	 * now, reset base SCSI buses to attempt to ensure we don't
	 * wind up with a media error due to a block header write being
	 * in progress when the power actually fails.
	 */
	for (adap = 0; adap < MAX_BASEIO; adap++) {
		if (baseio_qlvertex[adap])
			(void)qlreset(baseio_qlvertex[adap]);
	}

	return;
}

/* Called from Bridge widget error handler, which also disables the error
 * in bridge.
 */
/* ARGSUSED */
void
powerfail(intr_arg_t arg)
{
	/* Make sure that dallas is parked before losing AC, and make sure
	 * we return to fast mode incase we AC failed while accessing the
	 * dallas part.
	 */
	restore_ioc3_sio(slow_ioc3_sio() | (0xf << SIO_CR_CMD_PULSE_SHIFT));

	cmn_err(CE_WARN,"\\Power failure detected.\n");

	/* launch pseudo ithread for qlreset() */
	if (acfail_tinfo->thd_flags & THD_OK)
		vsema(&acfail_tinfo->thd_isync);

	/* If after 2 seconds we still have power print a warning. */
	timeout(powerfail_check,0,2*HZ);

	/* Disable the heart interrupt bit.  This may trip a spinlock,
	 * but at this point it's probably ok if we hang here.  Give
	 * it a try if we are not threaded yet as it probably will work.
	 *
	 * Note, since we share with the bridge widget error, we will
	 * disable bridge errors for a while.  If we are really going
	 * down that probably doesn't matter.  If we are spurious, then
	 * we have a HW bug, and will ignore bridge errors for a few
	 * seconds.
	 */
	heart_imr_bits_rmw(cpuid(),1ULL<<IP30_HVEC_ACFAIL,0);

	/* Grab dallas clock lock w/o spl to prevent any clock access during
	 * the loss of power.  We could spin here forever (which also
	 * accomplishes the goal, except on spurious power fails).
	 */
	nested_spinlock(&ip30clocklock);
}

static void
cpu_godown(void)
{
	cmn_err(CE_WARN,
		"init 0 did not shut down after %d seconds, powering off.\n",
		clean_shutdown_time);
	DELAY(500000);	/* give them 1/2 second to see it */
	cpu_soft_powerdown();
}

/*  Poll for completion of a write operation to the Flash PROM before
 * initiating a system power down.
 *
 * if we come through here for more than 10 minutes, oh well.
 * (120 == 10min * 60sec/min / 5sec)
 */
static void
powerintr_pollflash()
{
	static int ten_minutes_max = 120;

	if ( (ten_minutes_max-- > 0) && (srflash_softpower_check()) ) {
		/* go to sleep for another 5 seconds */
		timeout(powerintr_pollflash, 0, 5*HZ);
	}
	else {
		/* 
		 * start the power down process, we'll be down soon ...
		 */
		cmn_err(CE_NOTE, "\\Flash PROM still open after 10 minutes. "
				 	 "Shutting down now.\n");
		powerintr_back();
	}

	return;
}

/*  Handle power button interrupt by shutting down the machine (signal init)
 * on the first press, and killing the power on after a second press.
 */
/*ARGSUSED*/
void
powerintr(intr_arg_t arg)
{
	static int powerpoll_started = 0;
	static int leds_are_flashing = 0;

	/* clear the interupt */
	cpu_clearpower();
	flushbus();

	if (!leds_are_flashing) {
		leds_are_flashing = 1;
		flash_led(3);		/* ack button press */
	}

	/*
	 * if we're already started the powerpoll timeout thread,
	 * then we're waiting on the flash before powering off ...
	 * so ignore the user pressing the button :)
	 */
	if (!powerpoll_started) {
		if ( srflash_softpower_startdown() ) {
			powerpoll_started = 1;
			cmn_err(CE_NOTE, "\\Power switch pressed while "
				 	 "Flash PROM open, waiting.\n");
			timeout(powerintr_pollflash, 0, 5*HZ);
		}
		else {
			powerintr_back();
		}
	}

	return;
}

/*
 * Back half of the power switch interrupt. Broken out here so that
 * if the Flash PROM is open for writing when we get the power intr,
 * that we can come back later and start the power down cycle.
 */
static void
powerintr_back( void )
{
	static time_t doublepower;
	extern int power_off_flag;	/* set in powerintr(), or uadmin() */

	power_off_flag = 1;		/* for prom_reboot() in ars.c */

	if (!doublepower) {
		/*  Send SIGINT to init, which is the same as
		 * '/etc/init 0'.  Set a timeout
		 * in case init wedges.  Set doublepower to handle
		 * quick (with a little debounce room) 'doubleclick'
		 * on the power switch for an immediate shutdown.
		 */
		if (!sigtopid(INIT_PID, SIGINT, SIG_ISKERN|SIG_NOSLEEP,
								0, 0, 0)) {
			timeout(cpu_godown, 0, clean_shutdown_time*HZ);	
			doublepower = lbolt + HZ;
			set_autopoweron();
		}
		else {
			us_delay(500000);/* wait a bit for switch to settle */
			cpu_soft_powerdown();
		}
	}
	else if (lbolt > doublepower) {
		cmn_err(CE_NOTE, "\\Power switch pressed twice -- "
				 "shutting off power now.\n");

		us_delay(500000);	/* wait a bit for switch to settle */
		cpu_soft_powerdown();
	}

	return;
}

/*  Flash LED from green to off to indicate system powerdown is in
 * progress.
 */
static void
flash_led(int on)
{
	set_leds(on);
	timeout(flash_led,(void *)(on ? 0 : (__psint_t)3),HZ/4);
}

/* While looping, check if the power button is being pressed.  Used to
 * keep the power switch working after a panic.
 */
void
cpu_waitforreset(void)
{
	while (1) {
		if ((BRIDGE_K1PTR->b_int_status & PWR_SWITCH_INTR) != 0)
			cpu_soft_powerdown();
	}
}
