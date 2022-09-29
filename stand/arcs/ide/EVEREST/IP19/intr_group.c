
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
 *	intr_group.c - Verify processor group interrupt                 *
 *									*
 ************************************************************************/

#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/IP19.h>
#include <setjmp.h>
#include <fault.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "intr.h"
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

static jmp_buf fault_buf;

/*
 * Verify group interrupts (live and MP)
 */
intr_group()
{
    uint i, group, priority, fail, retval;
    __psunsigned_t osr = get_sr();
    __psunsigned_t tmp, tmpword, mask, oile, oigrmask;

    retval = 0;
    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (intr_group) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    set_sr(osr | ~SR_IE);

    oile = EV_GET_LOCAL(EV_ILE);
    EV_SET_LOCAL(EV_ILE, oile | EV_EBUSINT_MASK);	/* enable CC */

    oigrmask = EV_GET_LOCAL(EV_IGRMASK);
    for (i = 3; i < 24; i += 4)
    {
	group = i - (uint)cpu_loc[1];
	if (group > 15)
	    priority = 60 - 2 * group;
	else
	    priority = 60 - 4 * group;
	msg_printf(DBG, "Group : 0x%x Priority : 0x%x\n", group, priority);

	/* see if there are any interrupt pending */
	if (get_cause() & 0x7f00)
	{
	    msg_printf(DBG, 
		"Cause register indicates interrupt pending 0x%x before test\n",
		get_cause());
	    clear_IP();
	}

	/* set group mask */
	mask = 0x1 << group;
	EV_SET_LOCAL(EV_IGRMASK, mask);
	msg_printf(DBG, "EV_IGRMASK set to 0x%llx\n", mask);

	if (setjmp(fault_buf))
	{
	    fail = 0;
	    msg_printf(VRB, "Received interrupt from group : 0x%x priority : 0x%x\n", group - cpu_loc[1], priority);

	    /* Verify that the interrupt has made it to EV_IP? and EV_HPIL. */

	    tmp = EV_GET_LOCAL(EV_IP0);
	    mask = (long long)0x1 << priority;
	    if (tmp != mask) {
		err_msg(GROUP_IP, cpu_loc, mask, tmp);
		fail = 1;
	    }

	    tmp = EV_GET_LOCAL(EV_HPIL);
	    if (tmp != priority) {
		err_msg(GROUP_HPIL, cpu_loc, tmp);
		fail = 1;
	    }

	    /* Verify R4000 Cause -- IP2 bit is set for level 0 intr. */
	    tmpword = get_cause();
	    if (!(tmpword & CAUSE_IP3)) {
		err_msg(GROUP_CAUSE, cpu_loc, tmpword);
		fail = 1;
	    }
	    if (fail)
	    {
		show_fault();
		retval = 1;
	    }

	    /* Clear pending interrupt. */
	    EV_SET_LOCAL(EV_CIPL0, priority);
	    
	    /* Wait a cycle (is this necessary?). */
	    tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	    /* Verify that EV_IP? is now zero and EV_HPIL is now zero. */
	    if (priority < 64)
		tmp = EV_GET_LOCAL(EV_IP0);
	    else
		tmp = EV_GET_LOCAL(EV_IP1);
	    if (tmp != 0) {
		err_msg(GROUP_IPCLR, cpu_loc, EV_GET_LOCAL(EV_IP0), EV_GET_LOCAL(EV_IP1));
		retval = 1;
	    }

	    tmp = EV_GET_LOCAL(EV_HPIL);
	    if (tmp != 0) {
		err_msg(GROUP_HPILCLR, cpu_loc, tmp);
		retval = 1;
	    }

	    /* Verify R4000 cause -- IP2 bit should now be zero. */
	    tmpword = get_cause();
	    if (tmpword & CAUSE_IP3) {
		err_msg(GROUP_CAUSECLR, cpu_loc, tmpword);
		retval = 1;
	    }
	}
	else
	{
	    nofault = fault_buf;

	    /* enable R4000 and CC level 0 interrupts */
	    set_sr(osr | SR_IBIT3 | SR_IE);		/* enable R4000 */

	    /*
	     * send group/broadcast interrupt to the group at the priority level
	     */
	    if (group > 15)
		sendbint(priority);
	    else
		sendgrint(priority, group);


	    /* Wait a bus cycle for intr to propagate. */
	    tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	    retval = 1;
	    clear_nofault();
	    err_msg(GROUP_NOINT, cpu_loc, group, priority);
	    show_fault();
	}
    }
    /* turn off anything we turned on */
    set_sr(osr);
    EV_SET_LOCAL(EV_IGRMASK, oigrmask);
    /* Disable level 3 intrs. */
    EV_SET_LOCAL(EV_ILE, oile);

    msg_printf(INFO, "Completed IP19 group interrupt test\n");
    return(retval);
}
