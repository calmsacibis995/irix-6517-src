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
 *	epc_rtcinc.c - RTC clock increment (time-of-day) test		*
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

#ident "arcs/ide/EVEREST/pbus/epc_rtcinc.c $Revision: 1.5 $"

#define	DEL_50_MS	50000		/* 50,000 us delay */
#define	DEL_100_MS	100000		/* 100,000 us delay */
#define	DEL_250_MS	250000		/* 250,000 us delay */
#define	DEL_400_MS	400000		/* 400,000 us delay */
#define	DEL_500_MS	500000		/* 500,000 us delay */
#define	DEL_1_SECOND	1000000		/* 1,000,000 us delay */
#define	DEL_FOR_INC	(DEL_400_MS)	/* sampling at .4, .8, 1.2, 1.6 */

/*
 * convenient way to group RTC info for test
 */
typedef struct regstruct {
    int offset;				/* address within rtc chip */
    unsigned int reg;			/* 1's for bits in use */
    char * name;			/* name to give in error messages */
}T_Regs;

static T_Regs rtc_starttime[] = {
/* RTC Registers */
{ NVR_SEC, 59, "NVR_SEC" },
{ NVR_MIN, 59, "NVR_MIN" },
{ NVR_HOUR, 23, "NVR_HOUR" },
{ NVR_WEEKDAY, 7, "NVR_WEEKDAY" },
{ NVR_DAY, 31, "NVR_DAY" },
{ NVR_MONTH, 12, "NVR_MONTH" },
{ NVR_YEAR, 99, "NVR_YEAR" },
{ NVR_RTCA, NVR_OSC_ON, "NVR_RTCA" },		/* oscillator enabled, no int */
{ NVR_RTCB, (NVR_DM | NVR_2412), "NVR_RTCB" },	/* binary time, 24 hour mode */
/* END of registers to be tested */
{ (-1), 0, "Undefined RTC Register" }
};

static T_Regs rtc_stoptime[] = {
/* RTC Registers */
{ NVR_SEC, 0, "NVR_SEC" },
{ NVR_MIN, 0, "NVR_MIN" },
{ NVR_HOUR, 0, "NVR_HOUR" },
{ NVR_WEEKDAY, 1, "NVR_WEEKDAY" },
{ NVR_DAY, 1, "NVR_DAY" },
{ NVR_MONTH, 1, "NVR_MONTH" },
{ NVR_YEAR, 0, "NVR_YEAR" },
/* END of registers to be tested */
{ (-1), 0, "Undefined RTC Register" }
};

#define NUM_REGS	14
#define NUM_SAMPLES	4

static char save_array[NUM_REGS], work_array[NUM_SAMPLES][NUM_REGS];

static int ide_epc_rtcinc(int, int);

int
epc_rtcinc(int argc, char** argv)
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

    msg_printf(INFO, "\nepc_rtcinc - RTC register read/write test\n");

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
		    retval |= ide_epc_rtcinc(slot, adap);
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
	msg_printf(INFO, "FAILED - epc_rtcinc\n");
    else
	msg_printf(INFO, "epc_rtcinc passed\n");

    return (retval);
}


static int ide_epc_rtcinc(int slot, int adap)
{
    int window, sample, offset, retval;
    char *cp, readback;
    T_Regs *reginfo;

    retval=0;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    msg_printf(INFO, "epc_rtcinc - looking at slot %x, adap %x\n", slot, adap);

/***********************************************************************
 * RTC Registers
 */

    /*
     * save the existing RTC values from the chip
     */
    get_rtcvals(window, adap, save_array);

    /*
     * reset the RTC to get it into a know/accessable state
     */
    rtc_reset(window, adap);

    /*
     * initialize the working array to a known value
     */
    for (offset=0, cp=&work_array[0][0]; offset < NUM_REGS; offset++)
	*cp++ = 0;

    /*
     * turn off updates, select 24 hour binary format
     */
    set_rtcreg(window, adap, NVR_RTCB, NVR_SET|NVR_DM|NVR_2412);

    /*
     * set the start time in the RTC chip registers
     */
    for (reginfo=rtc_starttime; reginfo->offset != -1; reginfo++)
	work_array[0][reginfo->offset] = reginfo->reg;

    set_rtcvals(window, adap, &work_array[0][0]);

    /*
     * delay to let the rtc clock start if it was not running . . .
     * (rtc takes ~500 MS to start running from a halted condition)
     */
    us_delay(DEL_500_MS);

    /*
     * now set up the start time again
     */
    set_rtcvals(window, adap, &work_array[0][0]);

    /*
     * sample increment early, on, and later than predicted time
     * we are checking increment here, not start-up exactness
     * there is about a .5 second instability in the oscillator startup
     */
    for (sample=0; sample < NUM_SAMPLES; sample++) {

	/*
	 * wait for the values to clock into the registers
	 */
	us_delay(DEL_FOR_INC);

	/*
	 * read back the current sample values
	 */
	get_rtcvals(window, adap, &work_array[sample][0]);
    }


    /*
     * turn off updates
     */
    set_rtcreg(window, adap, NVR_RTCB, NVR_SET|NVR_DM|NVR_2412);

    /*
     * find if any of the samples match the expected stop time
     */
    for (sample=0; sample < NUM_SAMPLES; sample++) {
	
	retval = 0;

	/*
	 * compare the values read back with the expected values
	 */
	for (reginfo=rtc_stoptime; reginfo->offset != -1; reginfo++){

	    if (work_array[sample][reginfo->offset] != reginfo->reg) {
		retval++;
	    }
	}

	/*
	 * if the sample matched the expected stop time, test has passed
	 * print out debug pass number and exit
	 */
	if (!retval) {
	    msg_printf (DBG, "Passed on sample %d\n", sample);
	    break;
	}
    }

    /*
     * if failed all three samples, print out the gory details
     */
    if (retval) {

	retval = 0; 

	/*
	 * look at each sample
	 */
	for (sample=0; sample < NUM_SAMPLES; sample++) {

	    /*
	     * compare the values read back with the expected values
	     */
	    for (reginfo=rtc_stoptime; reginfo->offset != -1; reginfo++){

		if (work_array[sample][reginfo->offset] != reginfo->reg) {
	    msg_printf(ERR,
	    "ERROR: epc_rtcinc - slot %x, adap %x, sample %x register %s\n",
	    slot, adap, sample, reginfo->name);
	    msg_printf(VRB, "  read back %x, expected %x\n",
	    (int) work_array[sample][reginfo->offset], (int) reginfo->reg);
	    retval++;
		}
	    }
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
