
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

/************************************************************************
 *									*
 *	intr_timer.c - Verify IP19 RTSC/interval timer interrupt        *
 *									*
 ************************************************************************/

#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/IP19.h>
#include <setjmp.h>
#include <fault.h>
#include "pattern.h"
#include "everr_hints.h"
#include "ide_msg.h"
#include "intr.h"
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

static jmp_buf fault_buf;

#define	ONESECNS	0x3b9aca00	/* one second in nanoseconds */
#define	CPERIOD		0x14		/* default CPERIOD */

/*
 * Simple synchronous test.  R4000 interrupts off
 */
intr_timer()
{
    uint retval;
    __psunsigned_t osr, slot, proc, oile, ocompare, nsperiod, tmp;

    retval=0;
    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (intr_timer) \n");

    slot = cpu_loc[0];
    proc = cpu_loc[1];
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    osr = get_sr();
    ocompare = EV_GET_LOCAL(EV_RO_COMPARE);
    nsperiod = EV_GETCONFIG_REG(slot, proc, EV_CPERIOD);

    set_sr(osr & ~SR_IE);	/* R4000 interrupts off */
    tmp = EV_GET_LOCAL(EV_RTC);

    msg_printf(DBG, "nsperiod : 0x%llx\n", nsperiod);
    msg_printf(DBG, "EV_RTC : 0x%llx\n", tmp);

    if (tmp == EV_GET_LOCAL(EV_RTC)) {
	msg_printf(ERR, "RTC not running - test aborted\n");
	set_sr(osr);
	return (1);
    }

    if (setjmp(fault_buf))
    {
	if (get_cause() & CAUSE_IP4)
	{
	    msg_printf(DBG, "Received timer interrupt: cause 0x%x\n", get_cause());
	    /* Clear pending interrupt. */
	    EV_SET_LOCAL(EV_CIPL124, 0x2);
	    tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	    /* Verify R4000 cause -- IP4 bit should now be zero. */
	    if (get_cause() & CAUSE_IP4) {
		err_msg(TIMER_CAUSECLR, cpu_loc);
		retval = 1;
	    }
	}
	else 
	{
	    err_msg(TIMER_INV_INT, cpu_loc);
	    retval = 1;
	}
	if (retval)
	    show_fault();
    }
    else
    {
	nofault = fault_buf;

	/* Enable level 1 intrs. */
	oile = EV_GET_LOCAL(EV_ILE);
	EV_SET_LOCAL(EV_ILE, oile | EV_CMPINT_MASK);
	msg_printf(DBG, "EV_ILE contents set to 0x%llx\n", EV_GET_LOCAL(EV_ILE));

	/* enable interrupt for level 1 */
	set_sr(osr | SR_IBIT4 | SR_IE);
	msg_printf(DBG, "Status register set to 0x%x\n", get_sr());
	/*
	 * set up timer interrupt
	 */
	tmp = EV_GET_LOCAL(EV_RTC) + 0x100000 * CPERIOD;
	msg_printf(DBG, "Compare value is 0x%llx\n", tmp);
	EV_SETCONFIG_REG(slot, proc, EV_CMPREG0, tmp);
	EV_SETCONFIG_REG(slot, proc, EV_CMPREG1, tmp >> 8);
	EV_SETCONFIG_REG(slot, proc, EV_CMPREG2, tmp >> 16);
	EV_SETCONFIG_REG(slot, proc, EV_CMPREG3, tmp >> 24);

	/*
	 * make sure the intr happened
	 * While we're waiting check that (ns) clock is ticking
	 */
	while ((uint) tmp > (uint)(EV_GET_LOCAL(EV_RTC)))
	msg_printf(DBG, "EV_RTC : 0x%llx  EV_RO_COMPARE : 0x%llx\n", EV_GET_LOCAL(EV_RTC), EV_GET_LOCAL(EV_RO_COMPARE));

	clear_nofault();
	err_msg(TIMER_NOINT, cpu_loc);
	retval = 1;
	show_fault();
    }

    /* Leave things tidy */
    set_sr(osr);
    /* Disable level 1 intrs. */
    EV_SET_LOCAL(EV_ILE, oile);
    /* Restore original RTSC compare register */
    EV_SETCONFIG_REG(slot, proc, EV_CMPREG0, tmp);
    EV_SETCONFIG_REG(slot, proc, EV_CMPREG1, tmp >> 8);
    EV_SETCONFIG_REG(slot, proc, EV_CMPREG2, tmp >> 16);
    EV_SETCONFIG_REG(slot, proc, EV_CMPREG3, tmp >> 24);

    msg_printf(INFO, "Completed timer interrupt test\n");
    return(retval);
}

