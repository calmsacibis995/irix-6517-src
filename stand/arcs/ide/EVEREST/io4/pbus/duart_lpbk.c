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
 *	duart_lpbk.c - duart loopback functionality tests		*
 *									*
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evintr.h>
#include <sys/param.h>
#include <sys/z8530.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <ide_msg.h>
#include <io4_tdefs.h>


#define	EXTERNAL_LPBK_MODE	2
#define	INTERNAL_LPBK_MODE	1
#define	KEYBOARD_CHANNEL	0
#define	MOUSE_CHANNEL		1
#define	TTY_CONSOLE_CHANNEL	4
#define RS422_CHANNEL		5
#define	DUART_WAIT		1000
#define MAX_CHANNELS		6
#define	BAD_RR1_STATUS	(RR1_FRAMING_ERR | RR1_RX_ORUN_ERR | RR1_PARITY_ERR)

extern char gfxalive;

static void configure_duart(volatile u_char *, u_short, int);
static void setup_enet_carrier_detection();
static int do_duart_loopback(int, int);

static u_char *duart_ctrl[6];
static u_char *duart_data[6];
static jmp_buf faultbuf;

/* do 0x00 and 0x01 last to turn off LEDs on keyboard */
static u_char pattern[] = {
			   ~0x00, ~0xaa, ~0xcc, ~0xf0,
			   0xf0, 0xcc, 0xaa, 0x00,
			   0x01,
};

/* do 600 last to please the keyboard */
static u_short baud[] =
{
 38400, 19200, 9600, 7200,
 4800, 2400, 1200, 300, 600,
};

typedef struct d_info
{
    int index;
    u_char regval;
} Duart_Info;

static Duart_Info duart_save_array[] =
{
 {RR0, 0},			/* Tx/Rx buffer/external status */
 {RR1, 0},			/* Rx condition status/residue codes */
 {RR2, 0},			/* interrup vector (modified in channel B
				 * only) */
 {RR3, 0},			/* interrupt pending (channel A only) */
 {RR8, 0},			/* Rx buffer */
 {RR10, 0},			/* loop/clock status */
 {RR12, 0},			/* lower byte of baud rate generator time
				 * constant */
 {RR13, 0},			/* upper byte of baud rate generator time
				 * constant */
 {RR14, 0},			/* extended function register, WR7' */
 {RR15, 0},			/* external status interrupt enable */
 {-1, 0}			/* last register saved */
};


static int slot, adap;


static void save_duart(volatile u_char *);
static void restore_duart(volatile u_char *);

static void configure_addrs(int, int);


/* DUART loopback test */

/*
 * This test was adapted from the IP12 duart loopback test, and is as much
 * like the original as the EVEREST architecture allows. Since Everest
 * currently only expects duarts on the first IO4, slot/adapter scanning in
 * not performed - we look at the EPC adapter address of the first (highest
 * slot #) IO4 installed in a system to find the duarts
 */
int
 duart_loopback(int argc, char **argv)

{
    char *atob();

    u_int channel;
    char *command = *argv;
    int errcount;
    int mode, lmode;
    int atype, master;

    errcount = 0;
    slot = 0;
    adap = 0;

#ifdef NOTYET
    /*
     * if bad command line, exit
     */
    if (io4_select (TRUE, argc, argv))
	return(1);
#else
    /*
     * force non-select for slot and adapter
     */
    io4_tslot = 0;
    io4_tadap = 0;
#endif 
    msg_printf(INFO,
  "\nduart_loopback - io4 duart loopback handshake, r/w, and interrupt test\n");

    if (argc > 2)
	errcount++;
    else
    if (argc < 2)
	mode = INTERNAL_LPBK_MODE;
    else
    {				/* parse command options */
	argv++;

	if ((*argv)[0] != '-')
	    errcount++;
	else
	{
	    switch ((*argv)[1])
	    {
		case 'e':	/* -e -> external loopback */
		    mode = EXTERNAL_LPBK_MODE;
		    break;

		case 'i':	/* -i -> internal loopback */
		    mode = INTERNAL_LPBK_MODE;
		    break;

		default:
		    errcount++;
		    break;
	    }
	}
    }

    if (errcount)
    {
	msg_printf(ERR,
		   "Usage: %s -(e|i)\n", command);
	return errcount;
    }

    /*
     * iterate through all io boards in the system
     */
    for (slot = EV_MAX_SLOTS, master = 1; slot > 0; slot--) {

	if (board_type(slot) == EVTYPE_IO4) {
	    /*
	     * cheat - if slot was given on command line, use it
	     */
	    if (io4_tslot && (slot != io4_tslot)) {
		master = 0;
		continue;
	    }

	    /*
	     * iterate through all io adapters in the system
	     */
	    for (adap = 1; adap < IO4_MAX_PADAPS; adap++) {

		/*
		 * get the current adapter type
		 */
		atype = adap_type(slot, adap);

		if (atype == IO4_ADAP_EPC)
		{
		    /*
		     * cheat - if adapter was given on command line, use it
		     */
		    if (io4_tadap && (adap != io4_tadap)) {
			master = 0;
			continue;
		    }
		
		    /*
		     * set up the duart address tables for selected EPC
		     * in selected slot
		     */
		    configure_addrs(slot, adap);

		    msg_printf(INFO,
			"Testing Serial Ports on IO4 in slot %x, adap %x\n",
			slot, adap);

		    for (channel = 0; channel < MAX_CHANNELS; channel++)
		    {
			lmode = mode;

			/*
			 * only check for keyboard, mouse, and console on the
			 * master EPC adapter (lowest # EPC on highest # IO4)
			 */
			if (master) {
			    if ((console_is_gfx() &&
				 channel == KEYBOARD_CHANNEL) ||
				(!console_is_gfx() &&
				 channel == TTY_CONSOLE_CHANNEL))
			    {
				msg_printf(VRB,
			       "Cannot run loopback test on console channel\n");
				msg_printf(VRB,
				   "Skipping channel %d for now\n", channel);
				continue;
			    }

			    if (mode == EXTERNAL_LPBK_MODE &&
				channel == KEYBOARD_CHANNEL)
			    {
				msg_printf(VRB,
			"Cannot run external loopback test on keyboard channel\n");
				msg_printf(VRB,
				   "Running internal loopback test instead\n");
				lmode = INTERNAL_LPBK_MODE;
			    }

			    if (mode == EXTERNAL_LPBK_MODE &&
				channel == MOUSE_CHANNEL)
			    {
				msg_printf(VRB,
			"Cannot run external loopback test on mouse channel\n");
				msg_printf(VRB,
				   "Running internal loopback test instead\n");
				lmode = INTERNAL_LPBK_MODE;
			    }

			    if (mode == EXTERNAL_LPBK_MODE &&
				channel == RS422_CHANNEL)
			    {
				msg_printf(VRB,
			"Cannot run external loopback test on RS422 channel\n");
				msg_printf(VRB,
				   "Running internal loopback test instead\n");
				lmode = INTERNAL_LPBK_MODE;
			    }
			}

			if (lmode == INTERNAL_LPBK_MODE)
			    msg_printf(VRB,
			       "DUART channel %d internal loopback test\n",
			       channel);
			else
			if (lmode == EXTERNAL_LPBK_MODE)
			    msg_printf(VRB,
			       "DUART channel %d external loopback test\n",
			       channel);

			errcount += do_duart_loopback(channel, lmode);
		    }
		    master = 0;
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

    if (master && io4_tadap) {
	atype = adap_type(io4_tslot, io4_tadap);
	msg_printf(ERR,
	    "ERROR: %s - slot %x, adap %x was %x, expected %x\n",
	     argv[0], io4_tslot, io4_tadap, atype, IO4_ADAP_EPC);
	errcount++;
    }

END_TEST:
    msg_printf(INFO, "%s duart_lpbk in %s loopback mode\n",
	       (errcount ? "Failed" : "Passed"),
	       ((mode == EXTERNAL_LPBK_MODE) ? "external" : "internal"));

    return errcount;
}

/*
 * does the actual loopback test on one channel of the duart's serial port
 */
static int
 do_duart_loopback(int channel, int mode)
{
    u_char data;
    volatile u_char *datap;
    volatile u_char *ctrl;

    int errcount = 0;
    uint ivect, readback;
    int i;
    int j;
    int k;

    u_char rr0;
    u_char rr1;

    /*
     * get the pointers to the current channel's data and control registers 
     */
    ctrl = duart_ctrl[channel];
    datap = duart_data[channel];

    /*
     * duarts 1 & 2 now use a common vector channels 0 & 1 on duart 0, all
     * others on duarts 1 & 2 
     */
    if (channel < 2)
	ivect = EVINTR_LEVEL_EPC_DUART0;
    else
	ivect = EVINTR_LEVEL_EPC_DUART1;


    save_duart(ctrl);

    /* check I/O signals */
    if (mode != INTERNAL_LPBK_MODE && channel != KEYBOARD_CHANNEL)
    {
	/* disable i/o external status interrupt, reset signals */
	WR_CNTRL(ctrl, WR15, 0x0);
	WR_WR0(ctrl, WR0_RST_EXT_INT);

	if (mode == EXTERNAL_LPBK_MODE)
	{
	    /*
	     * the loopback must have RTS connected to CTS, DTR to DCD, and
	     * Rx to Tx 
	     */
	    WR_CNTRL(ctrl, WR5, WR5_RTS);
	    rr0 = RD_RR0(ctrl);
	    if ((rr0 & (RR0_CTS | RR0_DCD)) != RR0_CTS)
	    {
		++errcount;
		msg_printf(ERR,
	       "No loopback / RTS-DTR not connected to CTS-DCD / Bad Z85130\n");
		goto duart_lpbk_done;
	    }

	    WR_CNTRL(ctrl, WR5, WR5_DTR);
	    rr0 = RD_RR0(ctrl);
	    if ((rr0 & (RR0_CTS | RR0_DCD)) != RR0_DCD)
	    {
		++errcount;
		msg_printf(ERR,
	       "No loopback / RTS-DTR not connected to CTS-DCD / Bad Z85130\n");
		goto duart_lpbk_done;
	    }
	}
	else
	{

	    /*
	     * the loopback must have DTR connected to CTS, Rx- to Tx-, and
	     * Rx+ to Tx+ 
	     */
	    WR_CNTRL(ctrl, WR5, WR5_DTR);
	    rr0 = RD_RR0(ctrl);
	    if (!(rr0 & RR0_CTS))
	    {
		++errcount;
		msg_printf(ERR,
		   "No loopback / DTR not connected to CTS / Bad Z85130\n");
		goto duart_lpbk_done;
	    }

	    WR_CNTRL(ctrl, WR5, 0x0);
	    rr0 = RD_RR0(ctrl);
	    if (rr0 & RR0_CTS)
	    {
		++errcount;
		msg_printf(ERR,
		   "No loopback / DTR not connected to CTS / Bad Z85130\n");
		goto duart_lpbk_done;
	    }
	}

    }

    msg_printf(DBG, "Handshake signals OK\n");

    /* use different baud rates and different data patterns */
    for (i = 0; i < sizeof(baud) / sizeof(u_short); i++)
    {
	configure_duart(ctrl, baud[i], mode);

	WR_CNTRL(ctrl, WR1, WR1_RX_INT_ERR_ALL_CHR);
	WR_CNTRL(ctrl, WR9, WR9_MIE);	/* enable interrupt */

	/*
	 * clear any old fault handlers, timers, etc
	 */
	clear_nofault();

	for (j = 0; j < sizeof(pattern) / sizeof(u_char); j++)
	{
	    if (setjmp(faultbuf))
	    {
		if ((_exc_save == EXCEPT_NORM)
		    && ((_cause_save & CAUSE_EXCMASK) == EXC_INT)
		    && cpu_intr_pending()
		    && (RD_RR0(ctrl) & RR0_RX_CHR))
		{
		    rr1 = RD_CNTRL(ctrl, RR1);
		    data = RD_DATA(datap);
		    if (data != pattern[j])
		    {
			errcount++;
			msg_printf(ERR,
			"Baud: %d, data expected: 0x%02x, actual: 0x%02x\n",
				   baud[i], pattern[j], data);
			goto duart_lpbk_done;
		    }

		    if (rr1 & BAD_RR1_STATUS)
		    {
			errcount++;
			msg_printf(ERR,
				   "Bad Rx status, RR1: 0x%02x\n",
				   rr1);
			goto duart_lpbk_done;
		    }

		    readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
		    if (readback != ivect)
		    {
			msg_printf(ERR,
			     "Wrong interrupt level: was %x, expected %x\n",
				   readback, ivect);
			errcount++;
			goto duart_lpbk_done;
		    }

		    continue;


		}
		else
		{		/* non-DUART interrupt */

		    errcount++;
		    msg_printf(ERR, "Phantom interrupt\n");
		    io_err_show(EVCFGINFO->ecfg_epcioa, EPC_ADAPTER);
		    goto duart_lpbk_done;
		}
	    }
	    else
	    {
		msg_printf(DBG, "Baud: %d, data: 0x%02x\n",
			   baud[i], pattern[j]);

		/*
		 * enable interrupt vectors in io4 & epc, set up error handler 
		 * must be done every time through, since interrupt being taken
		 * clears the interrupt enables
		 */
		setup_err_intr(slot, adap);

		nofault = faultbuf;

		/* Enable interrupts on the R4k processor */
		set_SR(get_SR() | SRB_DEV | SR_IE);

		/* wait for Tx empty */
		for (k = DUART_WAIT; k > 0; k--)
		{
		    if (RD_RR0(ctrl) & RR0_TX_EMPTY)
			break;
		    else
			us_delay(100);
		}

		/* too long */
		if (!(RD_RR0(ctrl) & RR0_TX_EMPTY))
		{
		    errcount++;
		    msg_printf(ERR,
			       "Time out waiting for Tx ready\n");
		    goto duart_lpbk_done;
		}

		WR_DATA(datap, pattern[j]);
		us_delay(DUART_WAIT * 100);

		errcount++;
		msg_printf(ERR, "Time out waiting for Rx ready\n");

		/*
		 * DEBUG stuff 
		 */
		if (cpu_intr_pending() == 0)
		{
		    msg_printf(DBG, "No ebus interrupt\n");
		    readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
		    msg_printf(DBG,
			"Pending Interrupt Level was %x\n", readback);

		    goto duart_lpbk_done;
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

		    goto duart_lpbk_done;
		}
	    }		/* end of if ( setjmp() ) */
	}		/* end of test patterns loop */
    }			/* end of baud rate loop */

duart_lpbk_done:
    set_SR(get_SR() & ~SR_IMASK);	/* disable interrupts */
    setup_err_intr(slot, adap);
    clear_err_intr(slot, adap);
    cpu_err_clear();
    configure_duart(ctrl, 9600, EXTERNAL_LPBK_MODE);

    /*
     * clear fault handlers, timers, etc
     */
    clear_nofault();

    msg_printf(INFO, "%s DUART %s loopback test on channel %d\n",
	       (errcount ? "Failed" : "Passed"),
	       ((mode == EXTERNAL_LPBK_MODE) ? "external" : "internal"),
	       channel);

    return (errcount);
}


/* configure the DUART */
static void
 configure_duart(volatile u_char * p, u_short baud, int mode)
{
/*
    volatile u_char *port_config = (u_char *) PHYS_TO_K1(PORT_CONFIG);
*/
    u_short time_constant =
    (CLK_SPEED + CLK_FACTOR * baud) / (CLK_FACTOR * 2 * baud) - 2;

    /* software reset */
    if ((u_int) p & CHNA_CNTRL_OFFSET)
    {
	WR_CNTRL(p, WR9, WR9_RST_CHNA);
    }
    else
    {
	WR_CNTRL(p, WR9, WR9_RST_CHNB);
    }

    /* odd parity for keyboard */
    if (p == duart_ctrl[KEYBOARD_CHANNEL])
    {
	WR_CNTRL(p, WR4, WR4_X16_CLK | WR4_1_STOP | WR4_PARITY_ENBL);
    }
    else
    {
	WR_CNTRL(p, WR4, WR4_X16_CLK | WR4_1_STOP);
    }
    WR_CNTRL(p, WR3, WR3_RX_8BIT);
    WR_CNTRL(p, WR5, WR5_TX_8BIT);
    WR_CNTRL(p, WR10, 0x0);	/* NRZ encoding */
    WR_CNTRL(p, WR11, WR11_RCLK_BRG | WR11_TCLK_BRG);

    /* set up baud rate time constant */
    WR_CNTRL(p, WR14, 0x0);	/* disable BRG */
    WR_CNTRL(p, WR12, time_constant & 0xff);
    WR_CNTRL(p, WR13, (time_constant >> 8) & 0xff);
    if (mode != INTERNAL_LPBK_MODE)
    {
	WR_CNTRL(p, WR14, WR14_BRG_ENBL);
    }
    else
    {
	WR_CNTRL(p, WR14, WR14_BRG_ENBL | WR14_LCL_LPBK);
    }

    /*
     * Rx/Tx must not be enabled before setting up other parameters 
     */
    WR_CNTRL(p, WR3, WR3_RX_8BIT | WR3_RX_ENBL);
    WR_CNTRL(p, WR5, WR5_TX_8BIT | WR5_TX_ENBL);


    /* sometimes a BREAK gets generated during reset */
    WR_WR0(p, WR0_RST_EXT_INT);	/* reset latches */
    if (RD_RR0(p) & RR0_BRK)
    {
	while (RD_RR0(p) & RR0_BRK)
	    ;
	WR_WR0(p, WR0_RST_EXT_INT);	/* reset latches */
    }

    /* clear FIFO and error */
    while (RD_RR0(p) & RR0_RX_CHR)
    {
	RD_CNTRL(p, RR8);
	WR_WR0(p, WR0_RST_ERR);
    }
}



static void
 setup_enet_carrier_detection()
{
    volatile u_char *cntrl = (u_char *) PHYS_TO_K1(DUART0B_CNTRL);

    /* for etherent carrier detection */
    WR_CNTRL(cntrl, WR9, 0x00);	/* no interrupt to the MIPS */
    WR_CNTRL(cntrl, WR15, WR15_DCD_IE);	/* latch carrier signal */
    WR_CNTRL(cntrl, WR1, WR1_EXT_INT_ENBL);
}


/*
 *  these two save and restore the duart channel under test's configuration
 *
 *  done as functions mainly to keep the main test function code a bit cleaner
 */
static void
 save_duart(volatile u_char * cntrl)
{
    Duart_Info *ptr;

    ptr = &duart_save_array[0];

    while (ptr->index != -1)
    {
	ptr->regval = RD_CNTRL(cntrl, ptr->index);
	ptr++;
    }
}

static void
 restore_duart(volatile u_char * cntrl)
{
    Duart_Info *ptr;

    ptr = &duart_save_array[0];

    WR_CNTRL(cntrl, WR14, 0x0);

    while (ptr->index != -1)
    {
	WR_CNTRL(cntrl, ptr->index, ptr->regval);
	ptr++;
    }

    WR_CNTRL(cntrl, WR14, WR14_BRG_ENBL);

    /* sometimes a BREAK gets generated during reset */
    WR_WR0(cntrl, WR0_RST_EXT_INT);	/* reset latches */
    if (RD_RR0(cntrl) & RR0_BRK)
    {
	while (RD_RR0(cntrl) & RR0_BRK)
	    ;
	WR_WR0(cntrl, WR0_RST_EXT_INT);	/* reset latches */
    }

    /* clear FIFO and error */
    while (RD_RR0(cntrl) & RR0_RX_CHR)
    {
	RD_CNTRL(cntrl, RR8);
	WR_WR0(cntrl, WR0_RST_ERR);
    }
}

/*
 * kludge initialization routine to allow for variable expansion in macros
 */
static void configure_addrs(int slot, int adap)
{
    int i, win;
    unsigned long swin;

    win = io4_window(slot);
    swin = (unsigned long) SWIN_BASE(win, adap);

    i = 0;
    duart_ctrl[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART0 + CHNA_CNTRL_OFFSET  + BYTE_SELECT );
    duart_ctrl[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART0 + CHNB_CNTRL_OFFSET  + BYTE_SELECT );
    duart_ctrl[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART1 + CHNA_CNTRL_OFFSET  + BYTE_SELECT );
    duart_ctrl[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART1 + CHNB_CNTRL_OFFSET  + BYTE_SELECT );
    duart_ctrl[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART2 + CHNA_CNTRL_OFFSET  + BYTE_SELECT );
    duart_ctrl[i] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART2 + CHNB_CNTRL_OFFSET  + BYTE_SELECT );

    i = 0;
    duart_data[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART0 + CHNA_DATA_OFFSET  + BYTE_SELECT );
    duart_data[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART0 + CHNB_DATA_OFFSET  + BYTE_SELECT );
    duart_data[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART1 + CHNA_DATA_OFFSET  + BYTE_SELECT );
    duart_data[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART1 + CHNB_DATA_OFFSET  + BYTE_SELECT );
    duart_data[i++] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART2 + CHNA_DATA_OFFSET  + BYTE_SELECT );
    duart_data[i] = (u_char *)
	PHYS_TO_K1( swin +  EPC_DUART2 + CHNB_DATA_OFFSET  + BYTE_SELECT );
}
