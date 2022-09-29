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
 *	epc_rtcint.c - RTC interrupt generation test			*
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
#include <sys/EVEREST/evintr.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <io4_tdefs.h>
#include <rtc_access.h>

#ident "arcs/ide/EVEREST/pbus/epc_rtcint.c $Revision: 1.6 $"


#define	DEL_250_MS	250000		/* 250,000 us delay */
#define	DEL_500_MS	500000		/* 500,000 us delay */
#define	DEL_1_SECOND	1000000		/* 1,000,000 us delay */
#define DEL_FOR_INC	(DEL_1_SECOND+DEL_500_MS)

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
{ NVR_SECALRM, 0, "NVR_SEC" },
{ NVR_MIN, 59, "NVR_MIN" },
{ NVR_MINALRM, 0, "NVR_MIN" },
{ NVR_HOUR, 23, "NVR_HOUR" },
{ NVR_HOURALRM, 0, "NVR_HOUR" },
{ NVR_WEEKDAY, 7, "NVR_WEEKDAY" },
{ NVR_DAY, 31, "NVR_DAY" },
{ NVR_MONTH, 12, "NVR_MONTH" },
{ NVR_YEAR, 99, "NVR_YEAR" },
/* oscillator enabled, no int */
{ NVR_RTCA, NVR_OSC_ON, "NVR_RTCA" },
/* binary time, 24 hour mode, alarm interrupt enabled */
{ NVR_RTCB, (NVR_DM | NVR_2412 | NVRB_AIE), "NVR_RTCB" },
/* END of registers to be tested */
{ (-1), 0, "Undefined RTC Register" }
};

static T_Regs rtc_uptime[] = {
/* RTC Registers */
{ NVR_SEC, 59, "NVR_SEC" },
{ NVR_MIN, 59, "NVR_MIN" },
{ NVR_HOUR, 23, "NVR_HOUR" },
{ NVR_WEEKDAY, 7, "NVR_WEEKDAY" },
{ NVR_DAY, 31, "NVR_DAY" },
{ NVR_MONTH, 12, "NVR_MONTH" },
{ NVR_YEAR, 99, "NVR_YEAR" },
/* oscillator enabled, no int */
{ NVR_RTCA, NVR_OSC_ON, "NVR_RTCA" },
/* binary time, 24 hour mode, update interrupt enabled */
{ NVR_RTCB, (NVR_DM | NVR_2412 | NVRB_UIE), "NVR_RTCB" },
/* END of registers to be tested */
{ (-1), 0, "Undefined RTC Register" }
};

static T_Regs rtc_pertime[] = {
/* RTC Registers */
{ NVR_SEC, 59, "NVR_SEC" },
{ NVR_MIN, 59, "NVR_MIN" },
{ NVR_HOUR, 23, "NVR_HOUR" },
{ NVR_WEEKDAY, 7, "NVR_WEEKDAY" },
{ NVR_DAY, 31, "NVR_DAY" },
{ NVR_MONTH, 12, "NVR_MONTH" },
{ NVR_YEAR, 99, "NVR_YEAR" },
/* oscillator enabled, 256 hz periodic interrupt */
{ NVR_RTCA, NVR_OSC_ON|1, "NVR_RTCA" },
/* binary time, 24 hour mode, update interrupt enabled */
{ NVR_RTCB, (NVR_DM | NVR_2412 | NVRB_UIE), "NVR_RTCB" },
/* END of registers to be tested */
{ (-1), 0, "Undefined RTC Register" }
};

#define NUM_REGS	14

static char save_array[NUM_REGS], work_array[NUM_REGS];

static jmp_buf faultbuf;

static int ide_epc_rtcint(int, int);


int
epc_rtcint(int argc, char** argv)
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

    msg_printf(INFO, "\nepc_rtcint - RTC register read/write test\n");

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
		    retval |= ide_epc_rtcint(slot, adap);
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
	msg_printf(INFO, "FAILED - epc_rtcint\n");
    else
	msg_printf(INFO, "epc_rtcint passed\n");

    return (retval);
}


static int ide_epc_rtcint(int slot, int adap)
{
    int window, pat_num, offset, retval;
    char *cp, readback, ivect;
    T_Regs *reginfo;

    retval=0;
    ivect = EVINTR_LEVEL_EPC_PROFTIM;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    msg_printf(INFO, "epc_rtcint - looking at slot %x, adap %x\n", slot, adap);

/***********************************************************************
 * RTC Registers
 */

    io_err_clear(slot, 0);
    io_err_clear(slot, (1 << adap));

    /*
     * save the existing RTC values from the chip
     */
    get_rtcvals(window, adap, save_array);

    /*
     * clear any old fault handlers, timers, etc
     */
    clear_nofault();

    /*
     * Test the time-of-day alarm interrupt
     */
    msg_printf(VRB, "Setting up RTC alarm interrupt\n");

    /*
     * reset the RTC to get it into a known/accessable state
     */
    rtc_reset(window, adap);

    if (setjmp(faultbuf))
    {
	if ((_exc_save == EXCEPT_NORM)
	    && ((_cause_save & CAUSE_EXCMASK) == EXC_INT)
	    && cpu_intr_pending()
	   )
	{
	    unsigned int rbvect;
	    set_SR(get_SR() & ~SR_IMASK);
	    rbvect = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    readback = get_rtcreg(window, adap, NVR_RTCC);
	    if (rbvect != ivect)
	    {
		msg_printf(ERR,
		     "Wrong interrupt level: was %x, expected %x\n",
			   (int) rbvect, (int) ivect);
		retval++;
	    }

	    if (!(readback & NVRC_IRQF))
	    {
		retval++;
		msg_printf(ERR, "No RTC interrupt flag set\n");
		adap_error_show(slot, adap);
	    }

	    if (!(readback & NVRC_AF))
	    {
		retval++;
		msg_printf(ERR, "RTC interrupt, Alarm Flag missing\n");
		msg_printf(INFO, "RTC Register C was 0x%x\n", (int) readback);
	    }

	    if (retval)
		goto rtc_ints_done;

	}
	else
	{		/* non-DUART interrupt */

	    retval++;
	    msg_printf(ERR, "Phantom interrupt\n");
	    io_err_show(window, EPC_ADAPTER);
	    goto rtc_ints_done;
	}
    }
    else
    {
	/*
	 * initialize the working array to a known value
	 */
	for (offset=0, cp=work_array; offset < NUM_REGS; offset++)
	    *cp++ = 0;

	/*
	 * turn off updates, select 24 hour binary format
	 */
	set_rtcreg(window, adap, NVR_RTCB, NVR_SET|NVR_DM|NVR_2412);

	/*
	 * enable interrupt vectors in io4 & epc, set up error handler 
	 * must be done every time through, since interrupt being taken
	 * clears the interrupt enables
	 */
	setup_err_intr(slot, adap);

	nofault = faultbuf;

	/* Enable interrupts on the R4k processor */
	set_SR(get_SR() | SRB_DEV | SR_IE);

	/*
	 * set up the interrupt and enter delay loop here
	 */

	/*
	 * set the start time in the RTC chip registers
	 */
	for (reginfo=rtc_starttime; reginfo->offset != -1; reginfo++)
	    work_array[reginfo->offset] = reginfo->reg;

	set_rtcvals(window, adap, work_array);

	/*
	 * wait a second and a half  for the values to clock into the registers
	 */
	us_delay(DEL_FOR_INC);

	set_SR(get_SR() & ~SR_IMASK);

	/*
	 * turn off updates
	 */
	set_rtcreg(window, adap, NVR_RTCB, NVR_SET|NVR_DM|NVR_2412);

	/*
	 * timed  out and no interrupt yet
	 */
	retval++;
	msg_printf(ERR, "Time out waiting for RTC alarm interrupt\n");

	/*
	 * DEBUG stuff 
	 */
	if (cpu_intr_pending() == 0)
	{
	    msg_printf(DBG, "No ebus interrupt\n");
	    readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    msg_printf(DBG,
		"Pending Interrupt Level was %x\n", readback);

	    goto rtc_ints_done;
	}
	else
	{
	    msg_printf(DBG, "ebus interrupt, no CPU interrupt\n");
	    readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    msg_printf(DBG,
		"Pending Interrupt Level was %x\n", readback);

	    readback = (unsigned) get_SR();
	    msg_printf(DBG,
		"Interrupt Status Register was %x\n", readback);

	    goto rtc_ints_done;
	}
    }		/* end of if ( setjmp() ) */

    /*
     * Test the update ended interrupt
     */
    msg_printf(VRB, "Setting up RTC update ended interrupt\n");

    /*
     * reset the RTC to get it into a known/accessable state
     */
    rtc_reset(window, adap);

    if (setjmp(faultbuf))
    {
	if ((_exc_save == EXCEPT_NORM)
	    && ((_cause_save & CAUSE_EXCMASK) == EXC_INT)
	    && cpu_intr_pending()
	   )
	{
	    unsigned int rbvect;
	    set_SR(get_SR() & ~SR_IMASK);
	    rbvect = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    readback = get_rtcreg(window, adap, NVR_RTCC);
	    if (rbvect != ivect)
	    {
		msg_printf(ERR,
		     "Wrong interrupt level: was %x, expected %x\n",
			   (int) rbvect, (int) ivect);
		retval++;
	    }

	    if (!(readback & NVRC_IRQF))
	    {
		retval++;
		msg_printf(ERR, "No RTC interrupt flag set\n");
	    }

	    if (!(readback & NVRC_UF))
	    {
		retval++;
		msg_printf(ERR, "RTC interrupt, Update Flag missing\n");
		msg_printf(INFO, "RTC Register C was 0x%x\n", (int) readback);
	    }

	    if (retval)
		goto rtc_ints_done;

	}
	else
	{		/* non-DUART interrupt */

	    retval++;
	    msg_printf(ERR, "Phantom interrupt\n");
	    io_err_show(window, EPC_ADAPTER);
	    goto rtc_ints_done;
	}
    }
    else
    {
	/*
	 * initialize the working array to a known value
	 */
	for (offset=0, cp=work_array; offset < NUM_REGS; offset++)
	    *cp++ = 0;

	/*
	 * turn off updates, select 24 hour binary format
	 */
	set_rtcreg(window, adap, NVR_RTCB, NVR_SET|NVR_DM|NVR_2412);

	/*
	 * enable interrupt vectors in io4 & epc, set up error handler 
	 * must be done every time through, since interrupt being taken
	 * clears the interrupt enables
	 */
	setup_err_intr(slot, adap);

	nofault = faultbuf;

	/* Enable interrupts on the R4k processor */
	set_SR(get_SR() | SRB_DEV | SR_IE);

	/*
	 * set up the interrupt and enter delay loop here
	 */

	/*
	 * set the update ended interrupt enables in the RTC
	 */
	for (reginfo=rtc_uptime; reginfo->offset != -1; reginfo++)
	    work_array[reginfo->offset] = reginfo->reg;

	set_rtcvals(window, adap, work_array);

	/*
	 * wait a second and a half for the values to clock into the registers
	 */
	us_delay(DEL_FOR_INC); 

	set_SR(get_SR() & ~SR_IMASK);

	/*
	 * turn off updates
	 */
	set_rtcreg(window, adap, NVR_RTCB, NVR_SET|NVR_DM|NVR_2412);

	/*
	 * timed  out and no interrupt yet
	 */
	retval++;
	msg_printf(ERR, "Time out waiting for RTC update interrupt\n");

	/*
	 * DEBUG stuff 
	 */
	if (cpu_intr_pending() == 0)
	{
	    msg_printf(DBG, "No ebus interrupt\n");
	    readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    msg_printf(DBG,
		"Pending Interrupt Level was %x\n", readback);

	    goto rtc_ints_done;
	}
	else
	{
	    msg_printf(DBG, "ebus interrupt, no CPU interrupt\n");
	    readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    msg_printf(DBG,
		"Pending Interrupt Level was %x\n", readback);

	    readback = (unsigned) get_SR();
	    msg_printf(DBG,
		"Interrupt Status Register was %x\n", readback);

	    goto rtc_ints_done;
	}
    }		/* end of if ( setjmp() ) */

    /*
     * Test the time-of-day alarm interrupt
     */
    msg_printf(VRB, "Setting up RTC periodic interrupt\n");

    /*
     * reset the RTC to get it into a known/accessable state
     */
    rtc_reset(window, adap);

    if (setjmp(faultbuf))
    {
	if ((_exc_save == EXCEPT_NORM)
	    && ((_cause_save & CAUSE_EXCMASK) == EXC_INT)
	    && cpu_intr_pending()
	   )
	{
	    unsigned int rbvect;
	    set_SR(get_SR() & ~SR_IMASK);
	    rbvect = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    readback = get_rtcreg(window, adap, NVR_RTCC);
	    if (rbvect != ivect)
	    {
		msg_printf(ERR,
		     "Wrong interrupt level: was %x, expected %x\n",
			   (int) rbvect, (int) ivect);
		retval++;
	    }

	    if (!(readback & NVRC_IRQF))
	    {
		retval++;
		msg_printf(ERR, "No RTC interrupt flag set\n");
	    }

	    if (!(readback & NVRC_PF))
	    {
		retval++;
		msg_printf(ERR, "RTC interrupt, Periodic Flag missing\n");
		msg_printf(INFO, "RTC Register C was 0x%x\n", (int) readback);
	    }

	    if (retval)
		goto rtc_ints_done;

	}
	else
	{		/* non-DUART interrupt */

	    retval++;
	    msg_printf(ERR, "Phantom interrupt\n");
	    io_err_show(window, EPC_ADAPTER);
	    goto rtc_ints_done;
	}
    }
    else
    {
	/*
	 * initialize the working array to a known value
	 */
	for (offset=0, cp=work_array; offset < NUM_REGS; offset++)
	    *cp++ = 0;

	/*
	 * turn off updates, select 24 hour binary format
	 */
	set_rtcreg(window, adap, NVR_RTCB, NVR_SET|NVR_DM|NVR_2412);

	/*
	 * enable interrupt vectors in io4 & epc, set up error handler 
	 * must be done every time through, since interrupt being taken
	 * clears the interrupt enables
	 */
	setup_err_intr(slot, adap);

	nofault = faultbuf;

	/* Enable interrupts on the R4k processor */
	set_SR(get_SR() | SRB_DEV | SR_IE);

	/*
	 * set up the interrupt and enter delay loop here
	 */

	/*
	 * set the start time in the RTC chip registers
	 */
	for (reginfo=rtc_pertime; reginfo->offset != -1; reginfo++)
	    work_array[reginfo->offset] = reginfo->reg;

	set_rtcvals(window, adap, work_array);

	/*
	 * wait a second and a half for the values to clock into the registers
	 */
	us_delay(DEL_FOR_INC); 

	set_SR(get_SR() & ~SR_IMASK);

	/*
	 * turn off updates
	 */
	set_rtcreg(window, adap, NVR_RTCB, NVR_SET|NVR_DM|NVR_2412);

	/*
	 * timed  out and no interrupt yet
	 */
	retval++;
	msg_printf(ERR, "Time out waiting for RTC periodic interrupt\n");

	/*
	 * DEBUG stuff 
	 */
	if (cpu_intr_pending() == 0)
	{
	    msg_printf(DBG, "No ebus interrupt\n");
	    readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    msg_printf(DBG,
		"Pending Interrupt Level was %x\n", readback);

	    goto rtc_ints_done;
	}
	else
	{
	    msg_printf(DBG, "ebus interrupt, no CPU interrupt\n");
	    readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    msg_printf(DBG,
		"Pending Interrupt Level was %x\n", readback);

	    readback = (unsigned) get_SR();
	    msg_printf(DBG,
		"Interrupt Status Register was %x\n", readback);

	    goto rtc_ints_done;
	}
    }		/* end of if ( setjmp() ) */

rtc_ints_done:
    set_SR(get_SR() & ~SR_IMASK);	/* disable interrupts */

    /*
     * restore the RTC to its original values
     */
    set_rtcvals(window, adap, save_array);

    setup_err_intr(slot, adap);
    clear_err_intr(slot, adap);
    cpu_err_clear();

    /*
     * clear fault handlers, timers, etc
     */
    clear_nofault();

    /*
     * return success/fail flag
     */
    return (retval);
}
