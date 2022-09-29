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
 *	epc_plptest.c - epc parallel port write test			*
 *									*
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <saioctl.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evintr.h>
#include <setjmp.h>
#include <fault.h>
#include <uif.h>
#include <io4_tdefs.h>

#ifdef TFP
#include <prototypes.h>
#endif

#include "plp.h"

/*
 * address step between one register and the next on epc chip
 */
#define	REGSTEP		8

/*
 * default pport setting has strobe low, busy high, ack low, busy mode,
 * strobe pulse, hold, and setup all at 500 uS, output mode, "busy"
 * (Centronics-compatible) mode set
 */
#define EPC_PPDEFAULT   (EPC_PPSTB_L | EPC_PPBUSY_H | EPC_PPACK_L \
		| EPC_PPBUSYMODE \
		| (500 << EPC_PPPLSSHIFT) \
		| (500 << EPC_PPHLDSHIFT) \
		| (500 << EPC_PPSTPSHIFT)) \
		| EPC_PPOUT \
		| EPC_PPBUSYMODE

/*
 * no printer default pport setting has strobe low, busy high, ack low,
 * busy mode, strobe pulse, hold, and setup all at 500 uS, output mode,
 * Soft Acknowlege (no-handshake) mode set
 */
#define EPC_NOPPDEFAULT   (EPC_PPSTB_L | EPC_PPBUSY_H | EPC_PPACK_L \
		| EPC_PPBUSYMODE \
		| (500 << EPC_PPPLSSHIFT) \
		| (500 << EPC_PPHLDSHIFT) \
		| (500 << EPC_PPSTPSHIFT)) \
		| EPC_PPOUT \
		| EPC_PPSACKMODE 

#define EPC_PPPULDEF    (EPC_PPSTRBPUL_H | EPC_PPBUSYPUL_H)

#define PLP_RTO         (20 * HZ)       /* default: 1 sec timeout on a read */

#define PLP_WTO         0               /* default: never timeout on a write */


/*
 * Pbus register access stuff.  These routines are endian-independent.
 */
#define PBUS_ADDR(_region, _adap, _x) \
        (SWIN_BASE(_region, _adap) + (_x) + BYTE_SELECT)

#define PBUS_GET(_region, _adap, _reg) \
        ( *((unsigned char *) PBUS_ADDR(_region,_adap, _reg)))
#define PBUS_SET(_region, _adap, _reg, _value) \
        PBUS_GET(_region, _adap, _reg) = (_value);

#define DELAY_1_SEC	1000000

#define PRINT_TEST_DEL   (5 * DELAY_1_SEC)

#ifdef TFP
#define PAGESIZE	0x4000
#else
#define PAGESIZE	0x1000
#endif

char epc_bigbuf[2 * PAGESIZE];


static jmp_buf faultbuf;
static int internal_mode;
static int pp_defaults;
static int ide_epc_plpwrt(int, int);
static int printer_status(int, int);
static void plp_reset(int, int);

int
epc_plptest (int argc, char** argv)
{
    int retval;

    retval = 0;
    internal_mode = TRUE;

    /*
     * check for "-e" command line switch to try external printer
     * if "-e" switch used, cuts it out of argv before calling io4_select
     */
    if (argc > 1)
    {
	if ((argv[1])[0] == '-')
	{
	    if ((argv[1])[1] == 'e')
	    {
		int tc, ti;

		internal_mode = FALSE;
		if (argc > 2)
		{
		    for (ti = 0, tc = argc-2; tc > 0; tc--, ti++)
		    {
			argv[1+ti] = argv[2+ti];
		    }
		}
		argc--;
	    }
	}
    }

    /*
     * if bad command line, exit
     */
    if (io4_select (TRUE, argc, argv))
    {
	msg_printf(ERR, "\tusage: epc_plptest [-e] [slot# adapter#]\n");
	return(1);
    }

    msg_printf(INFO, "\nepc_plptest - epc parallel port write test\n");

    if (io4_tslot)
	retval = ide_epc_plpwrt(io4_tslot, io4_tadap);
    else
	retval = test_adapter(argc, argv, IO4_ADAP_EPC, ide_epc_plpwrt);

    switch (retval)
    {
	case TEST_PASSED:
	    msg_printf(INFO, "epc_plptest passed\n");
	    break;

	case TEST_SKIPPED:
	    msg_printf(INFO, "SKIPPED - epc_plptest\n");
	    break;

	default:
	    msg_printf(INFO, "FAILED - epc_plptest\n");
    }

    return (retval);
}


static int ide_epc_plpwrt(int slot, int adap)
{
    int window, retval, int_mask, bcount, ivect;
    unsigned char * data_buffer;
    unsigned char ch;
    __psunsigned_t readback;

    retval=TEST_PASSED;
    ivect = EVINTR_LEVEL_EPC_PPORT;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    msg_printf(INFO, "epc_plpwrt - looking at slot %x, adap %x\n", slot, adap);

    /*
     * Get the current interrupt mask contents and disable all interrupt
     * sources - if cannot disable interrupts, exit
     */
    setup_err_intr(slot, adap);
    io_err_clear(slot, adap);
    clear_err_intr(slot, adap);
    int_mask = (int)(0xFF & EPC_GET(window, adap, EPC_ISTAT));
    EPC_SET(window, adap, EPC_IMRST, int_mask);
    readback = EPC_GET(window, adap, EPC_ISTAT);
    if (readback != 0) {
	msg_printf(ERR, "ERROR: epc_plptest - slot %x, adap %x, register %s\n",
		slot, adap, "EPC_ISTAT");
	msg_printf(VRB, "  read back %x, expected 0\n", readback);
	msg_printf(VRB, "  unable to disable EPC interrupts\n");
	retval++;
	goto TEST_DONE;
    }

    /*
     * if any flags input are true, we can assume that there is a printer
     * out there and use the default handshakes - otherwise, we will use
     * the SACK mode - we attempt to reset printer before testing to see
     * if it is present just to bring it back to a known state
     *
     * if internal_mode flag is true, we just reset the port and continue
     */
    plp_reset(slot, adap);

    if (internal_mode == TRUE)
    {
    	pp_defaults = EPC_NOPPDEFAULT;
	msg_printf(VRB, "Internal Mode Selected - using SACK mode\n");
    }
    else if (PBUS_GET(window, adap, EPC_PPSTAT) & 0xF)
    {
	pp_defaults = EPC_PPDEFAULT;
	msg_printf(VRB, "Printer detected - using BUSY mode\n");
    }
    else
    {
	pp_defaults = EPC_NOPPDEFAULT;
	msg_printf(VRB, "No Printer detected - using SACK mode\n");
    }

    /*
     * get buffer and memory descriptor that lie within a page
     */
    data_buffer = (unsigned char *)((__psunsigned_t)epc_bigbuf + PAGESIZE &
			~(PAGESIZE-1));

    if (data_buffer == NULL) {
        msg_printf (ERR, "Failed to allocate data buffer\n");
	retval++;
	goto TEST_DONE;
    }

    /*
     * fill the buffer with printable characters
     */
    for (ch = FIRST_CHAR, bcount = 0; ch <= LAST_CHAR; ch++, bcount++)
	data_buffer[bcount] = ch;
    data_buffer[bcount++] = '\r';
    data_buffer[bcount++] = '\n';

    if (setjmp(faultbuf))
    {
	if ((_exc_save == EXCEPT_NORM)
	    && ((_cause_save & CAUSE_EXCMASK) == EXC_INT)
	    && cpu_intr_pending()
	   )
	{
	    __psunsigned_t rbvect;
	    set_SR(get_SR() & ~SR_IMASK);
	    EPC_SET(window, adap, EPC_PPCTRL, pp_defaults);

	    rbvect = EV_GET_LOCAL(EV_HPIL);
	    if (rbvect != ivect)
	    {
		msg_printf(ERR,
		     "Wrong interrupt level: was %x, expected %x\n",
			   (int) rbvect, (int) ivect);
		retval++;
	    }

	    rbvect = EPC_GET(window, adap, EPC_PPCOUNT);

	    if (rbvect != bcount)
	    {
		msg_printf(ERR,
		    "Wrong PPORT DMA byte count: was %x, expected %x\n",
			(int) rbvect, (int) bcount);
		retval++;
	    }

	    if (retval)
	    {
		printer_status(slot, adap);
		goto TEST_DONE;
	    }

	}
	else
	{		/* non-PPORT interrupt */

	    retval++;
	    msg_printf(ERR, "Phantom interrupt\n");
	    io_err_show(window, EPC_ADAPTER);
	    goto TEST_DONE;
	}
    }
    else
    {
	/*
	 * enable interrupt vectors in io4 & epc, set up error handler 
	 * must be done every time through, since interrupt being taken
	 * clears the interrupt enables
	 */
	setup_err_intr(slot, adap);

	/*
	 * reset the port
	 */
	plp_reset(slot, adap);

	/*
	 * initialize pport
	 */
	EPC_SET(window, adap, EPC_PPCTRL, pp_defaults);

	/* Set up our bits in EPC_PRSTSET, EPC_PRSTCLR to EPC_PPPULDEF
	 *          polarity is reversed.
	 * Use EPC_PULLMASK to avoid screwing up e-net
	 */
	EPC_SET(window, adap, EPC_PRSTSET, ~EPC_PPPULDEF & EPC_PPPULLMASK);
	EPC_SET(window, adap, EPC_PRSTCLR, EPC_PPPULDEF & EPC_PPPULLMASK);

	/* Set up EPC_PPORT (PBUS reg.) */
	/* Who knows what this should be set to!! */
	/*
	EPC_SET(window, adap, EPC_PPORT, 3);
	*/

	/*
	 * Clear the high part of the address - we're not using it.
	 */
	EPC_SET(window, adap, EPC_PPBASEHI, 0);

	/*
	 * Set the low part of the address to the buffer
	 */
	EPC_SET(window, adap, EPC_PPBASELO, K1_TO_PHYS(data_buffer));
	
	/*
	 * Set the byte count
	 */
	EPC_SET(window, adap, EPC_PPLEN, bcount);

	nofault = faultbuf;

	/* Enable interrupts on the R4k processor */
	set_SR(get_SR() | SRB_DEV | SR_IE);

	/*
	 * set up the interrupt and enter delay loop here
	 */

	/*
	 * initialize pport, start DMA
	 */
	EPC_SET(window, adap, EPC_PPCTRL, pp_defaults | EPC_PPSTARTDMA);

	/*
	 * wait a second for the values to clock into the registers
	 */
	us_delay(PRINT_TEST_DEL); 

	set_SR(get_SR() & ~SR_IMASK);

	retval++;
	msg_printf(ERR, "Time out waiting for PPORT DMA interrupt\n");

	/*
	 * DEBUG stuff 
	 */
	if (cpu_intr_pending() == 0)
	{
	    msg_printf(DBG, "No ebus interrupt\n");
	    readback = EV_GET_LOCAL(EV_HPIL);
	    msg_printf(DBG,
		"Pending Interrupt Level was %x\n", readback);
	    printer_status(slot, adap);
	}
	else
	{
	    msg_printf(DBG, "ebus interrupt, no CPU interrupt\n");
	    readback = EV_GET_LOCAL(EV_HPIL);
	    msg_printf(DBG,
		"Pending Interrupt Level was %x\n", readback);

	    readback = get_SR();
	    msg_printf(DBG,
		"Interrupt Status Register was %x\n", readback);
	    printer_status(slot, adap);
	}

	/*
	 * stop DMA since not completed
	 */
	EPC_SET(window, adap, EPC_PPCTRL, pp_defaults);

    }		/* end of if ( setjmp() ) */

	
TEST_DONE:
    us_delay(2 * DELAY_1_SEC);		/* let things settle down */

    /*
     * restore original interrupt mask and return
     */
    EPC_SET(window, adap, EPC_IERR, 0x1000000);
    readback = EPC_GET(window, adap, EPC_IERRC);
    EPC_SET(window, adap, EPC_IMSET, int_mask);
    plp_reset(slot, adap);
    return (retval);
}

/*
 * plp_reset - reset the EPC parallel port interface
 */
static void
plp_reset(int slot, int adap)
{
    int window;

    window = io4_window(slot);

    /*
     * Shut down DMA, just in case.  Reset parameters.
     */
    EPC_SET(window, adap, EPC_PPCTRL, EPC_NOPPDEFAULT & ~EPC_PPSTARTDMA);

    /* Pull RESET and PRT low.  (RESET is inverted.)
     */
    PBUS_SET(window, adap, EPC_PPORT, EPC_PPRESET);

    /* Wait.
     */
    us_delay(400);

    /* Pull RESET and PRT back up.  (RESET is inverted.)
     */
    PBUS_SET(window, adap, EPC_PPORT, EPC_PPPRT);
}

/*
 * prints the parallel port status flags out - this is used a lot, so
 * encapsulated it - returns the status flags if needed for other stuff
 */
static int printer_status(int slot, int adap)
{
    int flags, control, rstat, window;

    /*
     * get window address and use that to get the status flags
     */
    window = io4_window(slot);
    rstat = PBUS_GET(window, adap, EPC_PRST);
    flags = PBUS_GET(window, adap, EPC_PPSTAT);
    control = EPC_GET(window, adap, EPC_PPCTRL);

    /*
     * print out the  DMA Byte count, control regs, and flags
     */
    msg_printf(VRB, "Parallel Port DMA Byte Count: 0x%x\n",
		(uint) EPC_GET(window, adap, EPC_PPCOUNT));

    msg_printf(VRB, "Parallel Port Reset Status: 0x%x\n", rstat);
    msg_printf(VRB, "Parallel Port Control Register: 0x%x\n", control);
    msg_printf(VRB,  "    PPSTB:    %s\n",
	    (control & EPC_PPSTB_H) ? "High" : "Low");
    msg_printf(VRB,  "    PPBUSY:   %s\n",
	    (control & EPC_PPBUSY_H) ? "High" : "Low");
    msg_printf(VRB,  "    PPACK:    %s\n",
	    (control & EPC_PPACK_H) ? "High" : "Low");
    msg_printf(VRB,  "    SACK:     %s\n",
	    (control & EPC_PPSACKMODE) ? "Soft Acknowlege" : "Busy/Ack");
    msg_printf(VRB,  "    Busy:     %s\n",
	    (control & EPC_PPBUSYMODE) ? "Busy Mode" : "Ack Mode");
    msg_printf(VRB,  "    DMA On:   %s\n",
	    (control & EPC_PPSTARTDMA) ? "DMA Enabled" : "DMA Disabled");
    msg_printf(VRB,  "    PPIN:     %s\n",
	    (control & EPC_PPIN) ? "Read PP" : "Write PP");

    msg_printf(VRB, "Parallel Port Status Flags: 0x%x\n", flags & 0xF);
    msg_printf(VRB, "    fault:    %s\n",
	    (flags & EPC_PPFAULT) ? "TRUE" : "FALSE");
    msg_printf(VRB, "    no ink:   %s\n",
	    (flags & EPC_PPNOINK) ? "TRUE" : "FALSE");
    msg_printf(VRB, "    no paper: %s\n",
	    (flags & EPC_PPNOPAPER) ? "TRUE" : "FALSE");
    msg_printf(VRB, "    online:   %s\n",
	    (flags & EPC_PPONLINE) ? "TRUE" : "FALSE");

    /*
     * return them now
     */
    return flags;
}
