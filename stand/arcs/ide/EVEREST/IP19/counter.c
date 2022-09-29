
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
 *	counter.c - R4K count/compare test                              *
 *									*
 ************************************************************************/

#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <setjmp.h>
#include <fault.h>
#include "libsk.h"
#include "everr_hints.h"
#include "ide_msg.h"
#include "ip19.h"
#include "pattern.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];
static jmp_buf fault_buf;

static uint test_patterns[] = {
    0x0, 
    0xFFFFFFFF, 
    0x55555555, 
    0xAAAAAAAA, 
    0xA5A5A5A5, 
    0x5A5A5A5A
};

#define NUM_TEST_PATS	(sizeof(test_patterns)/sizeof(uint))
#define	TDELAY	0x8000000

/*
 * Check the R4000 Counter/Compare Interrupt
 */

counter()
{
    unsigned int retval, ocmp;
    __psunsigned_t osr;
    uint pat_num, readback;
    unsigned int delay = 0x400;

    retval = 0;
    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (counter) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    osr = GetSR();

    /*
     * just in case of bad start status, disable all interrupt sources
     */
    SetSR (osr & ~SR_IMASK);

    /*
     * setup for interrupt - we should only end up in this case if
     * the counter interrupt went off on schedule
     */
    if (setjmp (fault_buf))
    {
	/*
	 * was this a count/compare interrupt?
	 */
	if (GetCause() & CAUSE_IP8)
	{
	    readback = get_count();

	    /*
	     * if bad count, disable counter interrupts & log error
	     */
	    if (readback < TDELAY)
	    {
		clear_nofault();
		SetSR(GetSR() & ~SR_IMASK);
		err_msg(COUNTER_BAD, cpu_loc, TDELAY, readback);
		retval = 1;
	    }   /* if */

	    msg_printf(VRB, "Received R4000 Counter interrupt\n");

	}
	else
	{
	    retval = 1;
	    err_msg(COUNTER_PINT, cpu_loc);
	    show_fault ();
	}   /* if */
    }
    else
    {
	/*
	 * reset count and compare registers and enable the
	 * count/compare interrupt. IEC in the status register of 
	 * coprocessor 0 must be re-set after each interrupt and
	 * EXL and ERL must be cleared
	 */
	nofault = fault_buf;
	ocmp = get_compare();

	/*
	 * perform basic write/read test on compare register
	 */
	msg_printf(DBG, "Write/read test on compare register\n");
	for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {
	    set_compare(test_patterns[pat_num]);
	    readback = get_compare();
	    if (readback != test_patterns[pat_num]) {
		err_msg(COMPARE_DERR, cpu_loc, test_patterns[pat_num], readback);
		retval = 1;
	    }
	}

	/*
	 * test functionality of both count and compare registers
	 */
	set_count(0);
	set_compare(TDELAY);
	msg_printf(DBG, "Compare register set to 0x%x\n", get_compare());
	SetSR (SR_IBIT8 | SR_IEC | (GetSR() & ~SR_IMASK & ~(SR_EXL | SR_ERL)));
	msg_printf(DBG, "Status register set to 0x%x\n", GetSR());

	/*
	 * wait in loop for interrupt - if the count/compare interrupt ever
	 * occurs, it should happen in the while() loop, so the code after
	 * the while() only gets executed in case of failure
	 */
	delay = TDELAY * 3;
	msg_printf(DBG, "Counting down delay 0x%x\n", delay);
	while(delay--);

	/*
	 * disable count/compare interrupt, log error & get out
	 */
	retval = 1;
	clear_nofault();
	SetSR (GetSR () & ~SR_IMASK);
	err_msg(COUNTER_NOINT, cpu_loc, get_compare(), get_count());
    }   /* if */

    /*
     * in case got here by bogus interrupt, disable all interrupt sources
     */
    nofault = 0;
    set_compare(ocmp);
    SetSR (osr);

    msg_printf(INFO, "Completed counter/compare functions test\n");
    return (retval);
}   
