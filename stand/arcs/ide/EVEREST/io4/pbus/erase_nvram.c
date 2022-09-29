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
 *	erase_nvram.c - erases NVRAM for selected IO4 board		*
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

#ident "arcs/ide/EVEREST/pbus/erase_nvram.c $Revision: 1.1 $"

static int ide_erase_nvram(int, int);
static char *tname;

int
erase_nvram(int argc, char** argv)
{
    int slot, adap, atype, retval;

    retval = 0;
    slot = 0;
    adap = 0;
    tname = argv[0];		/* test name from command line */

    /*
     * if bad command line, exit
     */
    if (io4_select (TRUE, argc, argv))
	return(1);

    msg_printf(INFO, "%s - erases NVRAM on selected IO4\n", tname);

    /*
     * cheat - if slot was given on command line, use it
     */
    if (io4_tslot) {
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
		    retval |= ide_erase_nvram(slot, adap);
		else if (io4_tadap) {
		    msg_printf(ERR,
		        "ERROR: %s - slot %x, adap %x was %x, expected %x\n",
			 tname, slot, adap, atype, IO4_ADAP_EPC);
		    retval++;
		}
	    
		/*
		 * if adap was given on command line, we are done
		 */
		if (io4_tadap)
		    break;
	    }
	}
	else {
	    msg_printf(ERR,
		"ERROR: %s - slot %x, was type %x, expected type %x (IO4)\n",
		tname, slot, board_type(slot), EVTYPE_IO4);
		retval++;
	}

    }
    else {
	msg_printf(INFO, "%s must have IO4 slot specified!\n", tname);
	retval++;
    }

    if (retval)
	msg_printf(SUM, "FAILED - %s\n", tname);
    else
	msg_printf(SUM, "%s passed\n", tname);

    return (retval);
}


static int ide_erase_nvram(int slot, int adap)
{
    int window, pat_num, offset, retval;
    char *saveloc, readback;

    retval=0;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    msg_printf(INFO, "%s - looking at slot %x, adap %x\n", tname, slot, adap);

    /*
     * write NVRAM to 0x0 - is default value
     */
    for  (offset = 0; offset < NVLEN_MAX; offset++)
	set_nvreg(window, adap, offset, 0x0);

    return (retval);
}
