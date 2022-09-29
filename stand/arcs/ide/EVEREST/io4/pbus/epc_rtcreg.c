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
 *	epc_rtcreg.c -  basic register and local NVRAM test for the RTC	*
 *			chip on the EPC's pbus				*
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

#ident "arcs/ide/EVEREST/pbus/epc_rtcreg.c $Revision: 1.3 $"

/*
 * convenient way to group RTC info for test
 */
typedef struct regstruct {
    int offset;				/* address within rtc chip */
    unsigned int mask;			/* 1's for bits in use */
    char * name;			/* name to give in error messages */
}T_Regs;

static T_Regs rtc_testregs[] = {
/* RTC Registers */
{ NVR_SEC, 0x7F, "NVR_SEC" },
{ NVR_SECALRM, 0xFF, "NVR_SECALRM" },
{ NVR_MIN, 0xFF, "NVR_MIN" },
{ NVR_MINALRM, 0xFF, "NVR_MINALRM" },
{ NVR_HOUR, 0xFF, "NVR_HOUR" },
{ NVR_HOURALRM, 0xFF, "NVR_HOURALRM" },
{ NVR_WEEKDAY, 0xFF, "NVR_WEEKDAY" },
{ NVR_DAY, 0xFF, "NVR_DAY" },
{ NVR_MONTH, 0xFF, "NVR_MONTH" },
{ NVR_YEAR, 0xFF, "NVR_YEAR" },
/* END of registers to be tested */
{ (-1), 0, "Undefined RTC Register" }
};

static char test_patterns[] = {
    0x0, 0xFF, 0x55, 0xAA, 0xA5, 0x5A
};

#define NUM_TEST_PATS	6

#define	START_NVRAM	0xE
#define	END_NVRAM	0x3F

#define	NVRAM_SIZE	((END_NVRAM+1)-START_NVRAM)

static char save_array[NVRAM_SIZE];

static int ide_epc_rtcreg(int, int);

int
epc_rtcreg(int argc, char** argv)
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

    msg_printf(INFO, "\nepc_rtcreg - RTC register read/write test\n");

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

		if (atype == IO4_ADAP_EPC) {
		    retval |= ide_epc_rtcreg(slot, adap);
		}
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
	msg_printf(INFO, "FAILED - epc_rtcreg\n");
    else
	msg_printf(INFO, "epc_rtcreg passed\n");

    return (retval);
}


static int ide_epc_rtcreg(int slot, int adap)
{
    int window, pat_num, offset, retval;
    char *saveloc, readback;
    T_Regs *reginfo;

    retval=0;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    msg_printf(INFO, "epc_rtcreg - looking at slot %x, adap %x\n", slot, adap);

/***********************************************************************
 * RTC Local NVRAM 
 */
    /*
     * reset the RTC to get it into a known/accessable state
     */
    rtc_reset(window, adap);

    /*
     * save existing nvram contents
     */
    saveloc=save_array;
    for (offset = START_NVRAM; offset <= END_NVRAM; offset++)
	*saveloc++ = get_rtcreg(window, adap, offset);
    
    /*
     * run all test patterns, now
     */
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	for  (offset = START_NVRAM; offset <= END_NVRAM; offset++) {
	    set_rtcreg(window, adap, offset, test_patterns[pat_num]);
	}

	/*
	 * verify test patterns in the registers
	 */
	for  (offset = START_NVRAM; offset <= END_NVRAM; offset++) {
	    readback =  get_rtcreg(window, adap, offset);

	    if (readback != test_patterns[pat_num]) {
		msg_printf(ERR,
		    "ERROR: epc_rtcreg - slot %x, adap %x, address %x\n",
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
    for (offset = START_NVRAM; offset <= END_NVRAM; offset++) {
	set_rtcreg(window, adap, offset, offset);
    }

    /*
     * verify test pattern in the registers
     */
    for  (offset = START_NVRAM; offset <= END_NVRAM; offset++) {
	readback =  get_rtcreg(window, adap, offset);

	if (readback != (offset & 0xff)) {
	    msg_printf(ERR,
		"ERROR: epc_rtcreg - slot %x, adap %x, address %x\n",
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
    for (offset = START_NVRAM; offset <= END_NVRAM; offset++)
	set_rtcreg(window, adap, offset, *saveloc++);

/***********************************************************************
 * RTC Registers
 */

    /*
     * save the existing RTC values from the chip
     */
    get_rtcvals(window, adap, save_array);

    /*
     * turn off updates, select 24 hour binary mode
     */
    set_rtcreg(window, adap, NVR_RTCB, NVR_SET|NVR_2412|NVR_DM);

    /*
     * run all test patterns in the RTC chip registers, now
     */
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	for  (reginfo = rtc_testregs; reginfo->offset != -1; reginfo++) {
	    set_rtcreg(window, adap, reginfo->offset, test_patterns[pat_num]);
	}

	/*
	 * verify test patterns in the registers
	 */
	for  (reginfo = rtc_testregs; reginfo->offset != -1; reginfo++) {
	    readback =  get_rtcreg(window, adap, reginfo->offset);

	    if (readback != (reginfo->mask & test_patterns[pat_num])) {
		msg_printf(ERR,
		    "ERROR: epc_rtcreg - slot %x, adap %x, register %s\n",
		    slot, adap, reginfo->name);
		msg_printf(VRB, "  read back %x, expected %x\n",
			(int) readback,
			(int) (reginfo->mask & test_patterns[pat_num]));
		retval++;
	    }
	}
    }

    /*
     * write address-in-address test pattern
     */
    for  (reginfo = rtc_testregs; reginfo->offset != -1; reginfo++) {
	set_rtcreg(window, adap, reginfo->offset, reginfo->offset);
    }

    /*
     * verify test pattern in the registers
     */
    for  (reginfo = rtc_testregs; reginfo->offset != -1; reginfo++) {
	readback =  get_rtcreg(window, adap, reginfo->offset);

	if (readback != (reginfo->offset & reginfo->mask)) {
	    msg_printf(ERR,
		"ERROR: epc_rtcreg - slot %x, adap %x, register %s\n",
		slot, adap, reginfo->name);
	    msg_printf(VRB, "  read back %x, expected %x\n",
		    (int) readback, (int) (reginfo->offset & reginfo->mask));
	    retval++;
	}
    }

    /*
     * restore the RTC to its original values
     */
    set_rtcvals(window, adap, save_array);

    /*
     * return success/fail flag
     */
    return (retval);
}
