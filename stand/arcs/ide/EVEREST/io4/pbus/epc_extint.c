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
 *	epc_extint.c -  basic external interrupt write/read utility	*
 *			writes out to and reads back from the		*
 *			external interrupt port on the EPC's pbus	*
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

#ident "arcs/ide/EVEREST/pbus/epc_extint.c $Revision: 1.4 $"

/*
 * External interrup port access stuff.  These routines are endian-independent.
 * output address is at EPC_PPSTAT since we grabbed EPC_EXTINT for Ethernet
 * status
 */
#define EXTINT_ADDR(_region, _adap, _x) \
        (SWIN_BASE(_region, _adap) + EPC_PPSTAT + (_x) + BYTE_SELECT)

#define EXTINT_GET(_region, _adap, _reg) \
        ( *((unsigned char *) EXTINT_ADDR(_region,_adap, _reg)))
#define EXTINT_SET(_region, _adap, _reg, _value) \
        EXTINT_GET(_region, _adap, _reg) = (_value);

static jmp_buf faultbuf;
static int ide_epc_extint(int, int, int);

int
epc_extint(int argc, char** argv)
{
    int slot, adap, atype, retval;

    retval = 0;
    slot = 0;
    adap = 0;

    /*
     * if bad command line, exit - must have IO4 adapter, pattern specified
     */
    if ((argc != 3) || (io4_select (FALSE, argc, argv)))
    {
	msg_printf(INFO, "\nERROR: epc_extint - bad format or slot number\n");
	msg_printf(INFO, "  use: \"epc_extint slot# test_pattern\"\n");
	for (retval = 0; retval < argc; retval++)
	    msg_printf(DBG, "Argument %d was %snumber\n", retval, argv[retval]);
    }
    else
    {
	/*
	 * also exit if pattern specifed is not a valid binary number
	 */
	if (atob(argv[2], &retval) == 0)
	    return(1);
	slot = io4_tslot;
	retval &= 0xF;		/* mask off all but lowest 4 bits */
    }

    msg_printf(INFO,
	"\nepc_extint - external interrupt write/readback utility\n");

    /*
     * iterate through all io adapters in the system
     */
    for (adap = 1; adap < IO4_MAX_PADAPS; adap++) {

	/*
	 * get the current adapter type
	 */
	atype = adap_type(slot, adap);

	/*
	 * found first EPC adapter - write outputs, read inputs, and exit
	 * since there should be only one EPC per IO4, leave after first
	 */
	if (atype == IO4_ADAP_EPC) {
	    retval = ide_epc_extint(slot, adap, retval);
	    msg_printf(DBG,
		"Read %x from external interrupts on slot %x, adap %x\n",
			retval, slot, adap);
	    break;
	}
    }
    return (retval);
}


static int ide_epc_extint(int slot, int adap, int pattern)
{
    int window, bit, offset, retval;
    char readback;
    caddr_t swin;

    retval=0;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);
    swin = (caddr_t)SWIN_BASE(window, adap);

    msg_printf(INFO,
	"epc_extint - writing %x to external interrupts on slot %x, adap %x\n",
	pattern, slot, adap);

    /*
     * clear any old fault handlers, timers, etc
     */
    clear_nofault();

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
	    if (rbvect != EVINTR_LEVEL_EPC_ERROR)
	    {
		msg_printf(ERR,
		     "Wrong interrupt level: was 0x%x, expected 0x%x\n",
		     rbvect, EVINTR_LEVEL_EPC_ERROR);
	    }
	    else {
		retval = 1;
	        msg_printf(INFO,
		    "Received SPARE Interrupt\n");
	    }
	}
	else
	{   /* unknown interrupt cause */

	    msg_printf(ERR, "Phantom interrupt\n");
	    io_err_show(window, EPC_ADAPTER);
	}
    }
    else
    {
	/*
	 * clear each of the 1-bit output addresses - ls bit goes to offset 0
	 * if loopback fixture is in place, this should generate an interrupt
	 */
	for (bit = 0; bit < 4; bit++) {
	    offset = bit * 8;
	    EXTINT_SET(window, adap, offset, 0);
	}

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
	 * write each of the 1-bit output addresses - ls bit goes to offset 0
	 * if loopback fixture is in place, this should generate an interrupt
	 */
	for (bit = 0; bit < 4; bit++) {
	    offset = bit * 8;
	    if ((pattern >> bit) & 1) {
		EXTINT_SET(window, adap, offset, 1);
	    }
	    else {
		EXTINT_SET(window, adap, offset, 0);
	    }
	}

	/*
	 * wait  100 ms for the interrupt to occur
	 */
	us_delay(100000);

	/*
	 * disable interrupts again - not going to happen now
	 */
	set_SR(get_SR() & ~SR_IMASK);

	/*
	 * timed  out and no interrupt yet
	 */
	msg_printf(INFO, "No Spare Interrupt In\n");

	/*
	 * DEBUG stuff 
	 */
	if (cpu_intr_pending() == 0)
	{
	    msg_printf(DBG, "No ebus interrupt\n");
	    readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
	    msg_printf(DBG,
		"Pending Interrupt Level was %x\n", readback);
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
	}
    }		/* end of if ( setjmp() ) */

    set_SR(get_SR() & ~SR_IMASK);	/* disable interrupts */

    /*
     * clear fault handlers, timers, etc
     */
    clear_nofault();

    cpu_err_clear(slot, adap);
    clear_err_intr(slot, adap);

    /*
     * return interrupt input values
     */
    return (retval);
}
