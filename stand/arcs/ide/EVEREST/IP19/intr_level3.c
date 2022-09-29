
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
 *	intr_level3.c - Verify CC level 3 interrupts                    *
 *									*
 ************************************************************************/

#ident "$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/IP19.h>
#include <sys/EVEREST/everror.h>
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
 * Verify CC level 3 interrupts.
 */
intr_level3()
{
	uint retval;
	__psunsigned_t osr, tmp, oile;

	retval = 0;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (intr_level3) \n");

	msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

	osr = get_sr();
	set_sr(osr | ~SR_IE);

	if (tmp = EV_GET_LOCAL(EV_ERTOIP))
	    /* clear whatever is currently pending */
	    EV_SET_LOCAL(EV_CERTOIP, tmp);

	tmp = EV_GET_LOCAL(EV_RO_COMPARE);

	/* see if there are any interrupt pending */
	if (get_cause() & CAUSE_IP6) {
	    err_msg(INT_L3_IP6NC, cpu_loc, get_cause());
	    set_sr(osr);
	    return(1);
	}
	if (setjmp(fault_buf))
	{
	    /* check IP in cause register */
	    if (get_cause() & CAUSE_IP6) 
		msg_printf(DBG, "Received level 3 interrupt!\n");
	    else
	    {
		err_msg(INT_L3_IP6, cpu_loc);
		retval = 1;
	    }

	    /* check ERTOIP register */
	    if ((tmp = EV_GET_LOCAL(EV_ERTOIP)) != 0x4) 
	    {
		err_msg(INT_L3_ERTOIP, cpu_loc);
		retval = 1;
	    }
	    if (retval)
		show_fault();

	    /* clear pending interrupt */
	    EV_SET_LOCAL(EV_CERTOIP, tmp);
	    tmp = EV_GET_LOCAL(EV_RO_COMPARE); /* delay */
	    if (tmp = EV_GET_LOCAL(EV_ERTOIP)) 
	    {
		err_msg(INT_L3_CERTOIP, cpu_loc, tmp);
		retval = 1;
	    }

	    /* check cause cleared */
	    if (get_cause() & CAUSE_IP6) 
	    {
		err_msg(INT_L3_IP6NC, cpu_loc, get_cause());
		retval = 1;
	    }
	}
	else
	{
	    nofault = fault_buf;

	    /* Enable level 3 intrs. */
	    oile = EV_GET_LOCAL(EV_ILE);
	    EV_SET_LOCAL(EV_ILE, oile | EV_ERTOINT_MASK);
	    msg_printf(DBG, "EV_ILE contents set to 0x%llx\n", EV_GET_LOCAL(EV_ILE));

	    /* enable interrupt for level 3 */
	    set_sr(osr | SR_IBIT6 | SR_IE);
	    msg_printf(DBG, "Status register set to 0x%x\n", get_sr());

	    /* force an error by writing to ERTOIP register */
	    EV_SET_LOCAL(EV_ERTOIP, 0x4);

	    tmp = EV_GET_LOCAL(EV_RO_COMPARE); /* delay */
	
	    /* no interrupt occur if here */
	    retval = 1;
	    clear_nofault();
	    err_msg(INT_L3_NOINT, cpu_loc, EV_GET_LOCAL(EV_ERTOIP));
	    show_fault();
	}

	/* Leave things tidy */
	SetSR(osr);
	/* Disable level 3 intrs. */
	EV_SET_LOCAL(EV_ILE, oile);

	msg_printf(INFO, "Completed IP19 Level 3 interrupt test\n");
        return(retval);
}

