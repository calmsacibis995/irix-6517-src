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
 *	local_regtest.c - IP19 local register read/write tests		*
 *									*
 ************************************************************************/

#ident "$Revision: 1.6 $"

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

/*
 * convenient way to group register info for test
 */
typedef struct regstruct {
    evreg_t value;			/* place to save orig reg values */
    uint offset;			/* R4K virtual address */
    long long int mask;			/* 1's for bits in use */
    char * name;			/* name to give in error messages */
}T_Regs;

static T_Regs readonly_reg[] = {

{ 0, EV_SPNUM, 0x7F, "EV_SPNUM" },
{ 0, EV_SYSCONFIG, 0xFFFFFFFFFFFF, "EV_SYSCONFIG" },
{ 0, EV_HPIL, 0x7F, "EV_HPIL" },
{ 0, EV_RO_COMPARE, 0xFFFFFFFFFFFFFFFF, "EV_RO_COMPARE" },
{ 0, EV_RTC, 0xFFFFFFFFFFFFFFFF, "EV_RTC" },
{ 0, EV_WGCOUNT, 0x33F3F, "EV_WGCOUNT" },

/* END of registers to be tested */
{ 0, 0, 0, "Undefined Local Register" }
};

static T_Regs ip19_testregs[] = {

{ 0, EV_WGDST, 0x7FFFFFFF00, "EV_WGDST" },	/* write gatherer destination */
{ 0, EV_WGCNTRL, 0x03, "EV_WGCNTRL" },		/* write gatherer control */
{ 0, EV_IP0, 0xFFFFFFFFFFFFFFFF, "EV_IP0" },	/* interrupts 63 - 0 */
{ 0, EV_IP1, 0xFFFFFFFFFFFFFFFF, "EV_IP1" },	/* interrupts 127 - 64 */
{ 0, EV_CEL, 0x7F, "EV_CEL" },			/* current execution level */
{ 0, EV_IGRMASK, 0xFFFF, "EV_IGRMASK" },	/* interrupt group mask */
{ 0, EV_ILE, 0x1F, "EV_ILE" },			/* interrupt level enable */
{ 0, EV_ERTOIP, 0x7FF, "EV_ERTOIP" },		/* error/timeout interrupt pending */
{ 0, EV_ECCSB_DIS, 0x01, "EV_ECCSB_DIS" },	/* ECC single-bit error disable */

/* END of registers to be tested */
{ 0, 0, 0, "Undefined Local Register" }
};

#define T_ARR_SIZE	(sizeof(ip19_testregs)/sizeof(T_Regs))

static long long test_patterns[] = {
    0x0, 
    0xFFFFFFFFFFFFFFFF, 
    0x5555555555555555, 
    0xAAAAAAAAAAAAAAAA, 
    0xA5A5A5A5A5A5A5A5, 
    0x5A5A5A5A5A5A5A5A
};

#define NUM_TEST_PATS	(sizeof(test_patterns)/sizeof(T_Regs))

int
local_regtest ()
{

    unsigned int pat_num, retval;
    long long readback;
    T_Regs *treg;

    retval=0;
    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (local_regtest) \n");
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    /*
     * read existing values from the read-only registers
     */
    for (treg = readonly_reg; treg->offset != 0; treg++) { 
	treg->value = EV_GET_LOCAL(treg->offset);
	msg_printf(VRB, "reading 0x%llx register %s\n", treg->value, treg->name);
    }
    
    /*
     * save existing values from the registers to be tested
     */
    for (treg = ip19_testregs; treg->offset != 0; treg++) { 
	treg->value = EV_GET_LOCAL(treg->offset);
	msg_printf(VRB, "reading 0x%llx register %s\n", treg->value, treg->name);
    }
    
    /*
     * run all test patterns, now
     */
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	for (treg = ip19_testregs; treg->offset != 0; treg++) {
	    EV_SET_LOCAL(treg->offset, (treg->mask & test_patterns[pat_num])); 
	    msg_printf(VRB, "writing 0x%llx register %s\n", test_patterns[pat_num] & treg->mask, treg->name);
	}

	/*
	 * verify test patterns in the registers
	 */
	for (treg = ip19_testregs; treg->offset != 0; treg++) {
	    readback =  EV_GET_LOCAL(treg->offset);

	    if (readback != (test_patterns[pat_num] & treg->mask)) {
		err_msg(LOCAL_REGERR, cpu_loc, treg->name, (treg->mask & test_patterns[pat_num]), readback);
		retval = 1;
	    }
	    EV_SET_LOCAL(treg->offset, treg->value);
	}
    }
	
    msg_printf(INFO, "Completed IP19 local register test\n");

    return (retval);
}
