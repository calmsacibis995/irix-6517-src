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
 *	bustags_reg.c - IP19 bus tags read/write tests		*
 *									*
 ************************************************************************/

#ident "arcs/ide/EVEREST/IP19/bustags_reg.c $Revision: 1.4 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evconfig.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include <ip19.h>
#include <prototypes.h>

static __psunsigned_t cpu_loc[2];

static uint test_patterns[] = {
    0x0, 
    0xFFFFFFFF, 
    0x55555555, 
    0xAAAAAAAA, 
    0xA5A5A5A5, 
    0x5A5A5A5A
};

#define NUM_TEST_PATS	(sizeof(test_patterns)/sizeof(uint))


int
bustags_reg ()
{

    unsigned int pat_num, retval, tag_size;
    __psunsigned_t osr, max_tagaddr, tag_addr, readback, tag_mask;

    retval=0;
    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (bustags_reg) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    osr = GetSR();
    msg_printf(DBG, "original SR setting 0x%x\n", osr);

    if ((osr & SR_DE) == 0)
	SetSR(osr & ~SR_DE);
    
    tag_size = calc_bustag_size();
    msg_printf(DBG, "bus tag size: 0x%x\n", tag_size);
    max_tagaddr = EV_BUSTAG_BASE + tag_size;
    tag_mask = get_bustag_mask();
    msg_printf(DBG, "bus tag mask: 0x%x\n", tag_mask);

    /*
     * run all test patterns, now
     */
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	msg_printf(VRB, "writing 0x%x to bus tags\n", test_patterns[pat_num] & tag_mask);

	for (tag_addr = EV_BUSTAG_BASE; tag_addr <= max_tagaddr; tag_addr+=8) {
	    EV_SET_REG(tag_addr, test_patterns[pat_num] & tag_mask); 
	}

	/*
	 * verify test patterns in the bus tags
	 */
	for (tag_addr = EV_BUSTAG_BASE; tag_addr <= max_tagaddr; tag_addr+=8) {
	    readback =  EV_GET_REG(tag_addr);

	    if (readback != test_patterns[pat_num] & tag_mask) {
		err_msg(BUSTAG_ERR, cpu_loc, tag_addr, test_patterns[pat_num] & tag_mask, readback);
		retval = 1;
	    }
	}
    }
	
    SetSR(osr);
    msg_printf(INFO, "Completed IP19 bus tags read/write test\n");

    return (retval);
}

