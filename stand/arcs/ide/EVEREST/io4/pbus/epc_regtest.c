/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
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
 *	epc_regtest.c - epc chip register read/write tests		*
 *									*
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evconfig.h>
#include <setjmp.h>
#include <uif.h>
#include <io4_tdefs.h>

/*
 * address step between one register and the next on epc chip
 */
#define		REGSTEP		8

/*
 * convenient way to group epc info for test
 */
typedef struct regstruct {
    unsigned int value;			/* place to save real values */
    int offset;				/* address within epc chip */
    unsigned int mask;			/* 1's for bits in use */
    char * name;			/* name to give in error messages */
}T_Regs;

static T_Regs epc_testregs[] = {
/* interrupt vector registers */
{ 0, EPC_IIDDUART0, 0xFFFF, "EPC_IIDDUART0" },
{ 0, EPC_IIDDUART1, 0xFFFF, "EPC_IIDDUART1" },
{ 0, EPC_IIDENET, 0xFFFF, "EPC_IIDENET" },
{ 0, EPC_IIDPROFTIM, 0xFFFF, "EPC_IIDPROFTIM" },
{ 0, EPC_IIDSPARE, 0xFFFF, "EPC_IIDSPARE" },
{ 0, EPC_IIDPPORT, 0xFFFF, "EPC_IIDPPORT" },
{ 0, EPC_IIDERROR, 0xFFFF, "EPC_IIDERROR" },
/* ethernet address bytes */
{ 0, EPC_EADDR0, 0xFF, "EPC_EADDR0" },
{ 0, EPC_EADDR1, 0xFF, "EPC_EADDR1" },
{ 0, EPC_EADDR2, 0xFF, "EPC_EADDR2" },
{ 0, EPC_EADDR3, 0xFF, "EPC_EADDR3" },
{ 0, EPC_EADDR4, 0xFF, "EPC_EADDR4" },
{ 0, EPC_EADDR5, 0xFF, "EPC_EADDR5" },
/* ethernet transmit and receive command registers */
{ 0, EPC_TCMD, 0xF, "EPC_TCMD" },
{ 0, EPC_RCMD, 0xFF, "EPC_RCMD" },
/* transmit buffer base address, buffer index, and timeout registers */
{ 0, EPC_TBASELO, 0xFFFFFFFF, "EPC_TBASELO" },
{ 0, EPC_TBASEHI, 0xFF, "EPC_TBASEHI" },
{ 0, EPC_TLIMIT, 0x7, "EPC_TLIMIT" },
{ 0, EPC_TITIMER, 0x1F, "EPC_TITIMER" },
/* receive buffer base address, buffer index, and timeout registers */
{ 0, EPC_RBASELO, 0xFFFFFFFF, "EPC_RBASELO" },
{ 0, EPC_RBASEHI, 0xFF, "EPC_RBASEHI" },
{ 0, EPC_RLIMIT, 0x7, "EPC_RLIMIT" },
{ 0, EPC_RITIMER, 0x1F, "EPC_RITIMER" },
/* parallel port buffer base address, dma size, and control registers */
{ 0, EPC_PPBASELO, 0xFFFFFFFF, "EPC_PPBASELO" },
{ 0, EPC_PPBASEHI, 0xFF, "EPC_PPBASEHI" },
{ 0, EPC_PPLEN, 0xFFF, "EPC_PPLEN" },
{ 0, EPC_PPCTRL, 0xFFFFFFDF, "EPC_PPCTRL" },
/* END of registers to be tested */
{ 0, (-1), 0, "Undefined EPC Register" }
};

static int test_patterns[] = {
    0x0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA, 0xA5A5A5A5, 0x5A5A5A5A
};

#define NUM_TEST_PATS	6

static int ide_epc_regs(int, int);

int
epc_regtest (int argc, char** argv)
{
    int slot, adap, atype, retval;

    retval = 0;
    slot = 0;
    adap = 0;

    /*
     * if bad command line, exit
     */
    if (io4_select (TRUE, argc, argv))
	return(1);

    msg_printf(INFO, "\nepc_regtest - epc chip register read/write test\n");

    /*
     * iterate through all io boards in the system
     */
    for (slot = EV_MAX_SLOTS; slot > 0; slot--) {

	/*
	 * cheat - if slot was given on command line, use it
	 */
	if (io4_tslot)
	    slot = io4_tslot;

	if (board_type(slot) == EVTYPE_IO4) {
	    /*
	     * iterate through all io adapters in the system
	     */
	    for (adap = 1; adap < IO4_MAX_PADAPS; adap++) {

		/*
		 * cheat - if adapter was given on command line, use it
		 */
		if (io4_tadap)
		    adap = io4_tadap;

		/*
		 * get the current adapter type
		 */
		atype = adap_type(slot, adap);

		if (atype == IO4_ADAP_EPC)
		    retval |= ide_epc_regs(slot, adap);
		else if (io4_tadap) {
		    msg_printf(ERR, "ERROR: %s - slot %x, adap %x was %x, expected %x\n",
			 argv[0], slot, adap, atype, IO4_ADAP_EPC);
		    retval++;
		}
	    
		/*
		 * if adap was given on command line, we are done
		 */
		if (io4_tadap)
		    break;
	    }
	
	    /*
	     * if slot was given on command line, we are done
	     */
	    if (io4_tslot)
		break;
	}
    }

    if (retval)
	msg_printf(INFO, "FAILED - epc_regtest\n");
    else
	msg_printf(INFO, "epc_regtest passed\n");

    return (retval);
}


static int ide_epc_regs(int slot, int adap)
{
    int window, pat_num, retval, readback, int_mask;
    T_Regs *treg;

    retval=0;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    msg_printf(INFO, "epc_regtest - looking at slot %x, adap %x\n", slot, adap);

    /*
     * Get the current interrupt mask contents and disable all interrupt
     * sources - if cannot disable interrupts, exit
     */
    setup_err_intr(slot, adap);
    int_mask = (0xFF & EPC_GET(window, adap, EPC_ISTAT));
    EPC_SET(window, adap, EPC_IMRST, int_mask);
    readback = EPC_GET(window, adap, EPC_ISTAT);
    if (readback != 0) {
	msg_printf(ERR, "ERROR: epc_regtest - slot %x, adap %x, register %s\n",
		slot, adap, "EPC_ISTAT");
	msg_printf(VRB, "  read back %x, expected 0\n", readback);
	msg_printf(VRB, "  unable to disable EPC interrupts\n");
	retval++;
	goto TEST_DONE;
    }


    /*
     * save existing values from the registers to be tested
     */
    for (treg = epc_testregs; treg->offset != -1; treg++)
	treg->value = EPC_GET(window, adap, treg->offset);
    
    /*
     * run all test patterns, now
     */
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	for (treg = epc_testregs; treg->offset != -1; treg++)
	    EPC_SET(window, adap, treg->offset,
		    treg->mask & test_patterns[pat_num]);

	/*
	 * verify test patterns in the registers
	 */
	for (treg = epc_testregs; treg->offset != -1; treg++) {
	    readback =  EPC_GET(window, adap, treg->offset);

	    if (readback != (test_patterns[pat_num] & treg->mask)) {
		msg_printf(ERR, "ERROR: epc_regtest - slot %x, adap %x, register %s\n",
			slot, adap, treg->name);
		msg_printf(VRB, "  read back %x, expected %x\n",
			readback, (treg->mask & test_patterns[pat_num]));
		retval++;
	    }
	}
    }
	
    /*
     * restore existing values to the registers tested
     */
    for (treg = epc_testregs; treg->offset != -1; treg++)
	EPC_SET(window, adap, treg->offset, treg->value);


TEST_DONE:

    /*
     * restore original interrupt mask and return
     */
    EPC_SET(window, adap, EPC_IERR, 0x1000000);
    readback = EPC_GET(window, adap, EPC_IERRC);
    EPC_SET(window, adap, EPC_IMSET, int_mask);
    return (retval);
}
