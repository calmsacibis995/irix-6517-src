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

#include <setjmp.h>

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/cpu.h>

#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evdiag.h>

#include "ip25prom.h"
#include "prom_externs.h"

/*
 * address step between one register and the next on epc chip
 */
#define		REGSTEP		8

/*
 * convenient way to group epc info for test
 */
typedef struct regstruct {
    uint index;				/* index of real values n "storage" */
    int offset;				/* address within epc chip */
    uint mask;				/* 1's for bits in use */
    char * name;			/* name to give in error messages */
} T_Regs;

static T_Regs epc_testregs[28] = {
/* interrupt vector registers */
{ 0, EPC_IIDDUART0, 0xFFFF, "EPC_IIDDUART0" },
{ 1, EPC_IIDDUART1, 0xFFFF, "EPC_IIDDUART1" },
{ 2, EPC_IIDENET, 0xFFFF, "EPC_IIDENET" },
{ 3, EPC_IIDPROFTIM, 0xFFFF, "EPC_IIDPROFTIM" },
{ 4, EPC_IIDSPARE, 0xFFFF, "EPC_IIDSPARE" },
{ 5, EPC_IIDPPORT, 0xFFFF, "EPC_IIDPPORT" },
{ 6, EPC_IIDERROR, 0xFFFF, "EPC_IIDERROR" },

/* According to Bob, it is probably unnecessarily dangerous to write to these
 * registers.  We have already seen situations where doing so has provoked 
 * problems, and it is kind of foolish to touch registers when we can't easily
 * fix it via a software upgrade.
 */
#if 0
/* ethernet address bytes */
{ 7, EPC_EADDR0, 0xFF, "EPC_EADDR0" },
{ 8, EPC_EADDR1, 0xFF, "EPC_EADDR1" },
{ 9, EPC_EADDR2, 0xFF, "EPC_EADDR2" },
{ 10, EPC_EADDR3, 0xFF, "EPC_EADDR3" },
{ 11, EPC_EADDR4, 0xFF, "EPC_EADDR4" },
{ 12, EPC_EADDR5, 0xFF, "EPC_EADDR5" },
/* ethernet transmit and receive command registers */
{ 13, EPC_TCMD, 0xF, "EPC_TCMD" },
{ 14, EPC_RCMD, 0xFF, "EPC_RCMD" },
#endif

/* transmit buffer base address, buffer index, and timeout registers */
{ 15, EPC_TBASELO, 0xFFFFFFFF, "EPC_TBASELO" },
{ 16, EPC_TBASEHI, 0xFF, "EPC_TBASEHI" },
{ 17, EPC_TLIMIT, 0x7, "EPC_TLIMIT" },
{ 18, EPC_TITIMER, 0x1F, "EPC_TITIMER" },
/* receive buffer base address, buffer index, and timeout registers */
{ 19, EPC_RBASELO, 0xFFFFFFFF, "EPC_RBASELO" },
{ 20, EPC_RBASEHI, 0xFF, "EPC_RBASEHI" },
{ 21, EPC_RLIMIT, 0x7, "EPC_RLIMIT" },
{ 22, EPC_RITIMER, 0x1F, "EPC_RITIMER" },
/* parallel port buffer base address, dma size, and control registers */
{ 23, EPC_PPBASELO, 0xFFFFFFFF, "EPC_PPBASELO" },
{ 24, EPC_PPBASEHI, 0xFF, "EPC_PPBASEHI" },
{ 25, EPC_PPLEN, 0xFFF, "EPC_PPLEN" },
{ 26, EPC_PPCTRL, 0xFFFFFFDF, "EPC_PPCTRL" },
/* END of registers to be tested */
{ 27, (-1), 0, "Undefined EPC Register" }
};

#define NUM_TEST_PATS	6

uint test_patterns[NUM_TEST_PATS] = {
    0x0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA, 0xA5A5A5A5, 0x5A5A5A5A
};

int pod_check_epc(int slot, int adap, int window)
{
    uint pat_num, retval, readback, wroteout, int_mask;
    T_Regs *treg;
    uint storage[28];

    retval=0;

    ccloprintf("Testing master EPC (slot %b, adap %a)...\n", slot, adap);

    /*
     * Get the current interrupt mask contents and disable all interrupt
     * sources - if cannot disable interrupts, exit
     */
/* XXX - deal with me.
    setup_err_intr(slot, adap);
*/
    int_mask = (0xFF & (uint)EPC_PROMGET(window, adap, EPC_ISTAT));
    EPC_PROMSET(window, adap, EPC_IMRST, int_mask);
    readback = (uint)EPC_PROMGET(window, adap, EPC_ISTAT);
    if (readback != 0) {
	ccloprintf("*** ERROR: epc_regtest - slot %b, adap %b, register %s\n",
		slot, adap, "EPC_ISTAT");
	ccloprintf("    read back %x, expected 0\n", readback);
	ccloprintf("    unable to disable EPC interrupts\n");
	retval++;
	goto TEST_DONE;
    }


    /*
     * save existing values from the registers to be tested
     */
    for (treg = epc_testregs; treg->offset != -1; treg++)
	storage[treg->index] = EPC_PROMGET(window, adap, treg->offset);
    
    /*
     * run all test patterns, now
     */
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	for (treg = epc_testregs; treg->offset != -1; treg++)
	    EPC_PROMSET(window, adap, treg->offset,
		    treg->mask & test_patterns[pat_num]);

	/*
	 * verify test patterns in the registers
	 */
	for (treg = epc_testregs; treg->offset != -1; treg++) {
	    wroteout = (test_patterns[pat_num] & treg->mask);
	    readback =  (uint)EPC_PROMGET(window, adap, treg->offset);

	    if (readback != wroteout) {
		ccloprintf("*** ERROR: epc_regtest - slot %b, adap %b, register %s\n",
			slot, adap, treg->name);
		ccloprintf("    read back %x, expected %x\n",
			readback, wroteout);
		retval++;
	    }
	}
    }
	
TEST_DONE:

    /*
     * restore existing values to the registers tested
     */
    for (treg = epc_testregs; treg->offset != -1; treg++)
	EPC_PROMSET(window, adap, treg->offset, storage[treg->index]);

    /*
     * restore original interrupt mask and return
     */
    EPC_PROMSET(window, adap, EPC_IERR, 0x1000000);
    readback = (uint)EPC_PROMGET(window, adap, EPC_IERRC);
    EPC_PROMSET(window, adap, EPC_IMSET, int_mask);

    if (retval)
	return EVDIAG_EPCREG_FAILED;
    else
	return EVDIAG_PASSED;
}

