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
 *	cfig_regtest.c - IP19 configuration register read/write tests	*
 *									*
 ************************************************************************/

#ident "arcs/ide/EVEREST/IP19/cfig_regtest.c $Revision: 1.7 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/IP19.h>
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
    __psunsigned_t value;	/* place to save orig reg values */
    __psunsigned_t offset;	/* R4K virtual address */
    uint mask;			/* mask of bits used only */
    char * name;		/* name to give in error messages */
}T_Regs;

/* CC config registers */

static T_Regs configregs[] = {

{ 0, EV_PGBRDEN, 0x01, "EV_PGBRDEN" },
{ 0, EV_PROC_DATARATE, 0x01, "EV_PROC_DATARATE" },
/* { 0, EV_WGRETRY_TOUT, 0x1F, "EV_WGRETRY_TOUT" }, */
{ 0, EV_CACHE_SZ, 0x07, "EV_CACHE_SZ" },

/* END of registers to be tested */
{ 0, 0, 0, "Undefined Configuration Register" }
};

static long long int test_patterns[] = {
    0x0, 
    0xFFFFFFFFFFFFFFFF, 
    0x5555555555555555, 
    0xAAAAAAAAAAAAAAAA, 
    0xA5A5A5A5A5A5A5A5, 
    0x5A5A5A5A5A5A5A5A
};

#define NUM_TEST_PATS	(sizeof(test_patterns)/sizeof(long long))

int
cfig_regtest ()
{

    unsigned int pat_num, retval;
    __psunsigned_t save_cmpreg, readback, slot, proc;
    T_Regs *treg;

    retval=0;
    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (cfig_regtest) \n");

    slot = cpu_loc[0];
    proc = cpu_loc[1];
    msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", proc, slot);

    /*
     * Report a couple of read-only configuration registers
     */
    readback = EV_GETCONFIG_REG(slot, 0, EV_A_BOARD_TYPE);
    msg_printf(VRB, "reading 0x%llx register EV_A_BOARD_TYPE\n", readback);
    readback = EV_GETCONFIG_REG(slot, 0, EV_A_LEVEL);
    msg_printf(VRB, "reading 0x%llx register EV_A_LEVEL\n", readback);

    /*
     * save existing values from the registers to be tested
     */
    for (treg = configregs; treg->offset != 0; treg++) { 
	treg->value = EV_GETCONFIG_REG(slot, proc, treg->offset);
	msg_printf(VRB, "reading 0x%llx register %s\n", treg->value, treg->name);
    }
    save_cmpreg = EV_GET_REG(EV_RO_COMPARE);
    msg_printf(VRB, "reading 0x%llx register EV_CMPREG0-3\n", save_cmpreg);
    
    /*
     * run all test patterns, now
     */
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	for (treg = configregs; treg->offset != 0; treg++) {
	    EV_SETCONFIG_REG(slot, proc, treg->offset, (treg->mask & test_patterns[pat_num])); 
	    msg_printf(VRB, "writing 0x%llx register %s\n", test_patterns[pat_num] & treg->mask, treg->name);
	}
	/*
	 * write pattern to timer config registers
	 */
	EV_SETCONFIG_REG(slot, proc, EV_CMPREG0, test_patterns[pat_num]);
	EV_SETCONFIG_REG(slot, proc, EV_CMPREG1, test_patterns[pat_num]>>8);
	EV_SETCONFIG_REG(slot, proc, EV_CMPREG2, test_patterns[pat_num]>>16);
	EV_SETCONFIG_REG(slot, proc, EV_CMPREG3, test_patterns[pat_num]>>24);
	msg_printf(VRB, "writing 0x%llx register EV_CMPREG0 - EV_CMPREG3\n", test_patterns[pat_num]);

	/*
	 * verify test patterns in the registers
	 */
	for (treg = configregs; treg->offset != 0; treg++) {
	    readback =  EV_GETCONFIG_REG(slot, proc, treg->offset);

	    if (readback != (test_patterns[pat_num] & treg->mask)) {
		err_msg(CONFIG_ERR, cpu_loc, treg->name, (treg->mask & test_patterns[pat_num]), readback);
		retval = 1;
	    }
	    /*
	     * restore original values in config registers
	     */
	    EV_SETCONFIG_REG(slot, proc, treg->offset, treg->value);
	}
	/*
	 * verify pattern in timer config registers
	 */
	readback = EV_GET_REG(EV_RO_COMPARE);
	if (readback != (test_patterns[pat_num] & 0xFFFFFFFF)) {
	    err_msg(CONFIG_ERR, cpu_loc, "EV_CMPREG0-3", test_patterns[pat_num], readback);
	    retval = 1;
	}
    }
    /*
     * restore original values in timer config registers
     */
    EV_SETCONFIG_REG(slot, proc, EV_CMPREG0, save_cmpreg);
    EV_SETCONFIG_REG(slot, proc, EV_CMPREG1, save_cmpreg>>8);
    EV_SETCONFIG_REG(slot, proc, EV_CMPREG2, save_cmpreg>>16);
    EV_SETCONFIG_REG(slot, proc, EV_CMPREG3, save_cmpreg>>24);

    msg_printf(INFO, "Completed IP19 configuration register test\n");

    return (retval);
}
