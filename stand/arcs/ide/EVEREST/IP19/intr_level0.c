
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
 *	intr_level0.c - IP19 level 0 interrupt test                     *
 *									*
 ************************************************************************/

#ident "$Revision: 1.7 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/IP19.h>
#include <setjmp.h>
#include <fault.h>
#include "libsk.h"
#include "err_hints.h"
#include "ide_msg.h"
#include "intr.h"
#include "ip19.h"
#include "prototypes.h"
#include "everr_hints.h"

uint verifymulti(__psunsigned_t, __psunsigned_t);
uint verifyipri(__psunsigned_t, __psunsigned_t, uint);
uint verifyelevel(__psunsigned_t, __psunsigned_t, int, int);
static __psunsigned_t cpu_loc[2];
static jmp_buf fault_buf;

/*
 * level 0 interrupts are the 256 priorities of everest bus interrupts
 */
intr_level0()
{
	int i, retval;
	__psunsigned_t slot, proc;
	long long tmp;

	retval=0;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (intr_level0) \n");

	slot = cpu_loc[0];
	proc = cpu_loc[1];
	msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", proc, slot);

/*	for (i = 0; i < EV_MAX_SLOTS; i++)
	    if (board_type(i) == EVTYPE_IP19)
*/
		    setup_err_intr(slot, 0); /* 0 is a no op for mc3 */

	/* Enable level 0 intrs. */
	tmp = EV_GET_LOCAL(EV_ILE);
	EV_SET_LOCAL(EV_ILE, tmp | EV_EBUSINT_MASK);

	/* hit the lo and hi of each range of 16 */
	for (i = 0; i < 128; i += 16) {
		retval = verifyipri(slot, proc, i);
		retval = verifyipri(slot, proc, i+1);
		retval = verifyipri(slot, proc, i+6);
		retval = verifyipri(slot, proc, i+14);
		retval = verifyipri(slot, proc, i+15);
	}

	msg_printf(VRB, "Checking IP19 current execution level\n");

	retval = verifyelevel(slot, proc, 0, 0);
	retval = verifyelevel(slot, proc, 0, 1);
	retval = verifyelevel(slot, proc, 1, 0);
	retval = verifyelevel(slot, proc, 127, 126);
	retval = verifyelevel(slot, proc, 126, 127);
	retval = verifyelevel(slot, proc, 127, 127);

	msg_printf(VRB, "Checking IP19 multiple interrupts\n");

	retval = verifymulti(slot, proc);

	/* Disable level 0 interrupts */
	tmp = EV_GET_LOCAL(EV_ILE);
	EV_SET_LOCAL(EV_ILE, tmp | ~EV_EBUSINT_MASK);

	msg_printf(INFO, "Completed IP19 Level 0 interrupt test\n");
	return(retval);
}


/*
 * Verify proper operation of the Int. Pend. and Highest Pri. Int. Lev.
 * registers -- the first 2 registers in the CC interrupt path.
 *
 * This routine expects that 1) R4000 interrupts are disabled,
 * 2) CC level 0 interrupts are enabled, 3) current execution level is 0,
 * and 4) there are no pending interrupts.  This routine returns with
 * the given interrupt priority cleared.
 */
uint verifyipri(__psunsigned_t slot, __psunsigned_t proc, uint ipri)
{
    ulong tmpword;
    int retval=0;
    __psunsigned_t osr;
    long long shift, tmp;

    /* XXX make sure there are currently no pending interrupts */

    osr = GetSR();
    msg_printf(DBG, "Interrupt priority 0x%x\n", ipri);

    if (get_cause() & 0x7f00)
    {
	msg_printf(DBG, 
	    "Cause register indicates interrupt pending 0x%x before test\n",
	    get_cause());
	clear_IP();
    }
    if (setjmp(fault_buf))
    {
	/* Verify that the interrupt has made it to EV_IP? and EV_HPIL. */

	if (ipri < 64)
	    tmp = EV_GET_LOCAL(EV_IP0);
	else
	    tmp = EV_GET_LOCAL(EV_IP1);
	shift = 1;
	if (tmp != (shift << (ipri % 64))) {
	    err_msg(INT_L0_IP, &cpu_loc[1], ipri, EV_GET_LOCAL(EV_IP0), 
		EV_GET_LOCAL(EV_IP1));
	    retval = 1;
	}

	tmp = EV_GET_LOCAL(EV_HPIL);
	if (tmp != ipri) {
	    err_msg(INT_L0_HPIL, &cpu_loc[1], tmp);
	    retval = 1;
	}

	/* Verify R4000 Cause -- IP3 bit is set for level 0 intr. */
	tmpword = get_cause();
	if (!(tmpword & CAUSE_IP3)) {
	    err_msg(INT_L0_CAUSE, &cpu_loc[1], tmpword);
	    retval = 1;
	}
	if (retval)
	    show_fault();

	/* Clear pending interrupt. */
	EV_SET_LOCAL(EV_CIPL0, ipri);
	
	/* Wait a cycle (is this necessary?). */
	tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	/* Verify that EV_IP? is now zero and EV_HPIL is now zero. */
	if (ipri < 64)
	    tmp = EV_GET_LOCAL(EV_IP0);
	else
	    tmp = EV_GET_LOCAL(EV_IP1);
	if (tmp != 0) {
	    err_msg(INT_L0_IPCLR, &cpu_loc[1], EV_GET_LOCAL(EV_IP0), 
		EV_GET_LOCAL(EV_IP1));
	    retval = 1;
	}

	tmp = EV_GET_LOCAL(EV_HPIL);
	if (tmp != 0) {
	    err_msg(INT_L0_HPILCLR, &cpu_loc[1], tmp);
	    retval = 1;
	}

	/* Verify R4000 cause -- IP3 bit should now be zero. */
	tmpword = get_cause();
	if (tmpword & CAUSE_IP3) {
	    err_msg(INT_L0_CAUSECLR, &cpu_loc[1], tmpword);
	    retval = 1;
	}
    }
    else
    {
	nofault = fault_buf;

	SetSR(osr | SR_IBIT3 | SR_IE);

	/* Send this proc a directed interrupt. */
	senddint(slot, proc, ipri);

	/* Wait a bus cycle for intr to propagate. */
	tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	retval = 1;
	clear_nofault();
	err_msg(INT_L0_NOINT, &cpu_loc[1], ipri);
	show_fault();
    }
    /* Leave things tidy */
    SetSR(osr);
    EV_SET_LOCAL(EV_CIPL0, 0);
    return(retval);
}


/*
 * Verify proper operation of the Current Execution Level register.
 *
 * This routine expects that 1) R4000 interrupts are disabled, 2) CC level 0
 * interrupts are enabled, and 3) there are no pending interrupts.  This
 * routine returns with the given interrupt priority cleared and the
 * Current Execution Level set to 0.
 */
uint verifyelevel(__psunsigned_t slot, __psunsigned_t proc, int ipri, int elevel)
{
	ulong tmpword;
	int retval=0;
	__psunsigned_t tmp;

	/* XXX fail if there are currently any pending interrupts */

	msg_printf(DBG, 
	    "Interrupt priority 0x%x Current execution level 0x%x\n", 
	    ipri, elevel);

	/* Set Current Execution Level as specified */
	EV_SET_LOCAL(EV_CEL, elevel);

	/* verify that it changed */
	tmp = EV_GET_LOCAL(EV_CEL);
	if (tmp != elevel) {
	    err_msg(INT_L0_CEL, &cpu_loc[1], elevel, tmp);
	    retval = 1;
	}

	/* Send intr of the specified priority */
	senddint(slot, proc, ipri);

	/* Wait a bus cycle for intr to propagate. */
	tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	/* if interrupt priority high enough intr should hit r4k */
	tmpword = get_cause();
	if (ipri >= elevel) {	/* r4k should see intr */
		if (!(tmpword & CAUSE_IP3)) {
		    err_msg(INT_L0_CELHI, &cpu_loc[1], tmpword);
		    retval = 1;
		}
	} else { /* if too low it should _not_ reach the r4k */
		if (tmpword & CAUSE_IP3) {
		    err_msg(INT_L0_CELLO, &cpu_loc[1], tmpword);
		    retval = 1;
		}
	}

	/* Clear pending interrupt. */
	EV_SET_LOCAL(EV_CIPL0, ipri);
	
	/* Wait a cycle (is this necessary?). */
	tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	/* interrupt condition in R4000 should now be off for either case */
	tmpword = get_cause();
	if (tmpword & CAUSE_IP3) {
	    err_msg(INT_L0_CELCLR, &cpu_loc[1], tmpword);
	    retval = 1;
	}

	/* Leave things tidy */
	EV_SET_LOCAL(EV_CIPL0, 0);
	EV_SET_LOCAL(EV_CEL, 0);

	return(retval);
}

/*
 * Verify proper operation of multiple pending interrupts
 */

uint verifymulti(__psunsigned_t slot, __psunsigned_t proc)
{
	long long tmp;
	uint retval=0; 
	ulong tmpword;

	/* Pound in a bunch of interrupts */
	senddint(slot, proc, 0);
	senddint(slot, proc, 62);
	senddint(slot, proc, 127);
	senddint(slot, proc, 65);
	senddint(slot, proc, 61);
	senddint(slot, proc, 124);
	senddint(slot, proc, 66);
	senddint(slot, proc, 3);

	/* Wait a bus cycle for last intr to propagate. */
	tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	/* Make sure this reflects the highest priority we sent above */
	tmp = EV_GET_LOCAL(EV_HPIL);
	if (tmp != 127) {
		err_msg(INT_L0_MULT, &cpu_loc[1], tmp);
		retval = 1;
	}

	/*
	 * Dig out the proper EV_IP? bits for each priority 
	 */
	tmp = EV_GET_LOCAL(EV_IP0);
	if (tmp != 0x6000000000000009) {
		err_msg(INT_L0_MULTIP0, &cpu_loc[1], tmp);
		retval = 1;
	}

	tmp = EV_GET_LOCAL(EV_IP1);
	if (tmp != 0x9000000000000006) {
		err_msg(INT_L0_MULTIP1, &cpu_loc[1], tmp);
		retval = 1;
	}
	
	/* Clear out all these pups.  No delay needed between */
	EV_SET_LOCAL(EV_CIPL0, 0);
	EV_SET_LOCAL(EV_CIPL0, 62);
	EV_SET_LOCAL(EV_CIPL0, 127);
	EV_SET_LOCAL(EV_CIPL0, 65);
	EV_SET_LOCAL(EV_CIPL0, 61);
	EV_SET_LOCAL(EV_CIPL0, 124);
	EV_SET_LOCAL(EV_CIPL0, 66);
	EV_SET_LOCAL(EV_CIPL0, 3);

	/* Wait a bus cycle for last intr to propagate. */
	tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	/* All EV_IP?s should now be 0 */
	tmp = EV_GET_LOCAL(EV_IP0);
	if (tmp != 0) {
		err_msg(INT_L0_MULTCL0, &cpu_loc[1], tmp);
		retval = 1;
	}
	tmp = EV_GET_LOCAL(EV_IP1);
	if (tmp != 0) {
		err_msg(INT_L0_MULTCL1, &cpu_loc[1], tmp);
		retval = 1;
	}

	/* Highest Pending Interrupt Level should now be zero */
	tmp = EV_GET_LOCAL(EV_HPIL);
	if (tmp != 0) {
		err_msg(INT_L0_MULTCLH, &cpu_loc[1], tmp);
		retval = 1;
	}

	/* R4000 IP3 should now be zero */
	tmpword = get_cause();
	if (tmpword & CAUSE_IP3) {
		err_msg(INT_L0_MULTCLC, &cpu_loc[1], tmpword);
		retval = 1;
	}

	/* Leave things tidy */
	EV_SET_LOCAL(EV_CIPL0, 0);
	return(retval);
}

