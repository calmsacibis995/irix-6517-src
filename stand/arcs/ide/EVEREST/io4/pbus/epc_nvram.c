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
 *	epc_nvram.c - read/write test for NVRAM under the epc chip	*
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
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/evconfig.h>
#include <setjmp.h>
#include <uif.h>
#include <io4_tdefs.h>
#include <rtc_access.h>

#ident "arcs/ide/EVEREST/pbus/epc_nvram.c $Revision: 1.5 $"

static char save_array[NVLEN_MAX];

static char test_patterns[] = {
    0x0, 0xFF, 0x55, 0xAA, 0xA5, 0x5A
};

#define NUM_TEST_PATS	6

static int ide_epc_nvram(int, int);

int
epc_nvram(int argc, char** argv)
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

    msg_printf(INFO, "\nepc_nvram - nvram read/write test\n");

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
		    retval |= ide_epc_nvram(slot, adap);
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
	msg_printf(INFO, "FAILED - epc_nvram\n");
    else
	msg_printf(INFO, "epc_nvram passed\n");

    return (retval);
}


static int ide_epc_nvram(int slot, int adap)
{
    int window, pat_num, offset, retval;
    char *saveloc, readback;

    retval=0;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    msg_printf(INFO, "epc_nvram - looking at slot %x, adap %x\n", slot, adap);


    /*
     * save existing nvram contents
     */
    saveloc=save_array;
    for (offset = 0; offset < NVLEN_MAX; offset++)
	*saveloc++ = ide_get_nvreg(window, adap, offset);
	 
    
    /*
     * run all test patterns, now
     */
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	for  (offset = 0; offset < NVLEN_MAX; offset++)
	    ide_set_nvreg(window, adap, offset, test_patterns[pat_num]);

	/*
	 * verify test patterns in the registers
	 */
	for  (offset = 0; offset < NVLEN_MAX; offset++) {
	    readback =  ide_get_nvreg(window, adap, offset);

	    if (readback != test_patterns[pat_num]) {
		msg_printf(ERR,
		    "ERROR: epc_nvram - slot %x, adap %x, address %x\n",
		    slot, adap, offset);
		msg_printf(VRB, "  read back %x, expected %x\n",
			(int) readback, (int) test_patterns[pat_num]);
		retval++;
	    }
	}
    }

    /*
     * write address-in-address test pattern
     */
    for  (offset = 0; offset < NVLEN_MAX; offset++)
	ide_set_nvreg(window, adap, offset, offset);

    /*
     * verify test pattern in the registers
     */
    for  (offset = 0; offset < NVLEN_MAX; offset++) {
	readback =  ide_get_nvreg(window, adap, offset);

	if (readback != (offset & 0xff)) {
	    msg_printf(ERR,
		"ERROR: epc_nvram - slot %x, adap %x, address %x\n",
		slot, adap, offset);
	    msg_printf(VRB, "  read back %x, expected %x\n",
		    (int) readback, (int) (offset & 0xff));
	    retval++;
	}
    }

    /*
     * restore existing values to the NVRAM
     */
    saveloc=save_array;
    for (offset = 0; offset < NVLEN_MAX; offset++)
	ide_set_nvreg(window, adap, offset, *saveloc++);

    /*
     * return success/fail flag
     */
    return (retval);
}
