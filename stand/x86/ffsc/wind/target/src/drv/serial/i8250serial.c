/* i8250Serial.c - I8250 tty driver */

/* Copyright 1984-1995 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01n,15aug95,hdn  added support for i386ex, and cleanup.
01m,14jun95,hdn  removed function declarations defined in sysLib.h.
01l,06jun94,hdn  doc cleanup for 5.1.1 FCS.
01k,22apr94,hdn  add a support for 38400.
		 changed a description mentioned PC_KBD_TYPE.
01j,30mar94,hdn  changed PC_CONSOLE tty number from 0,1 to 2,3.
01i,17feb94,hdn  moved TY_CO_DEV structure from sysLib.c.
01h,20oct93,vin  included support for keyboard and vga drivers
01g,12oct93,hdn  added sysIntEnablePIC(), sysIntDisablePIC().
01f,16jun93,hdn  renamed to i8250Serial.c.
01e,21apr93,hdn  fixed a bug in tyCoInt.
01d,09apr93,jdi  doc cleanup.
01c,07apr93,hdn  renamed compaq to pc.
01b,19oct92,hdn  supported nested interrupt.
01a,15may92,hdn  written based on frc386 version.
*/

/*
DESCRIPTION
This is the driver for the Intel 8250 UART Chip used on the PC 386.
If the macro INCLUDE_PC_CONSOLE is defined then this driver also
initializes the keyboard and VGA console devices on the PC.
If INCLUDE_PC_CONSOLE is defined then the keyboard and VGA consoles
are created first. Depending on the definition of PC_KBD_TYPE in config.h
the corresponding keyboard driver is included.

USER-CALLABLE ROUTINES
Most of the routines in this driver are accessible only through the I/O
system.  Two routines, however, must be called directly:  tyCoDrv() to
initialize the driver, and tyCoDevCreate() to create devices.

Before the driver can be used, it must be initialized by calling tyCoDrv().
This routine should be called exactly once, before any reads, writes, or
calls to tyCoDevCreate().  Normally, it is called from usrRoot() in
usrConfig.c.

Before a terminal can be used, it must be created, using tyCoDevCreate().
Each port to be used should have exactly one device associated with it
by calling this routine.

IOCTL FUNCTIONS
This driver responds to all the same ioctl() codes as a normal tty driver;
for more information, see the manual entry for tyLib.  As initialized, the
available baud rates are 110, 300, 600, 1200, 2400, 4800, 9600, 19200, and
38400.

SEE ALSO: tyLib
*/

#include "vxworks.h"
#include "iv.h"
#include "iolib.h"
#include "ioslib.h"
#include "tylib.h"
#include "intlib.h"
#include "errnolib.h"
#include "syslib.h"
#include "config.h"


/* globals */

TY_CO_DEV tyCoDv [N_UART_CHANNELS] =
    {
    {{{{NULL}}}, FALSE,
     UART_LCR(COM1_BASE_ADR),
     UART_LST(COM1_BASE_ADR),
     UART_MDC(COM1_BASE_ADR),
     UART_MSR(COM1_BASE_ADR),
     UART_IER(COM1_BASE_ADR),
     UART_IID(COM1_BASE_ADR),
     UART_BRDL(COM1_BASE_ADR),
     UART_BRDH(COM1_BASE_ADR),
     UART_RDR(COM1_BASE_ADR)},
    {{{{NULL}}}, FALSE,
     UART_LCR(COM2_BASE_ADR),
     UART_LST(COM2_BASE_ADR),
     UART_MDC(COM2_BASE_ADR),
     UART_MSR(COM2_BASE_ADR),
     UART_IER(COM2_BASE_ADR),
     UART_IID(COM2_BASE_ADR),
     UART_BRDL(COM2_BASE_ADR),
     UART_BRDH(COM2_BASE_ADR),
     UART_RDR(COM2_BASE_ADR)},
    };


/* locals */

/* baudTable is a table of the available baud rates, and the values to write
 * to the UART's baud rate divisor {high, low} register. the formula is
 * 1843200(source) / (16 * baudrate)
 */

LOCAL BAUD baudTable [] =
    {
    {50, 2304}, {75, 1536}, {110, 1047}, {134, 857}, {150, 768},
    {300, 384}, {600, 192}, {1200, 96}, {2000, 58}, {2400, 48},
    {3600,32}, {4800, 24}, {7200,16}, {9600, 12}, {19200, 6}, {38400, 3} 
    };

LOCAL int tyCoDrvNum;		/* driver number assigned to this driver */


/* forward declarations */

LOCAL void	tyCoStartup	(TY_CO_DEV *pTyCoDv);
LOCAL int	tyCoOpen	(TY_CO_DEV *pTyCoDv, char *name, int mode);
LOCAL STATUS	tyCoIoctl	(TY_CO_DEV *pTyCoDv, int request, int arg);
LOCAL void	tyCoHrdInit	(void);

#ifdef INCLUDE_PC_CONSOLE	/* if key board and VGA console needed */

#include "serial/pcconsole.c"
#include "serial/m6845vga.c"

#if (PC_KBD_TYPE == PC_PS2_101_KBD)	/* 101 KEY PS/2                 */
#include "serial/i8042kbd.c"
#else
#include "serial/i8048kbd.c"		/* 83 KEY PC/PCXT/PORTABLE      */
#endif /* (PC_KBD_TYPE == PC_XT_83_KBD) */

#endif /* INCLUDE_PC_CONSOLE */

/*******************************************************************************
*
* tyCoDrv - initialize the tty driver
*
* This routine initializes the serial driver, sets up interrupt vectors,
* and performs hardware initialization of the serial ports.
*
* This routine should be called exactly once, before any reads, writes,
* or calls to tyCoDevCreate().  Normally, it is called by usrRoot()
* in usrConfig.c.
*
* RETURNS: OK or ERROR if the driver cannot be installed.
*
* SEE ALSO: tyCoDevCreate()
*/

STATUS tyCoDrv (void)

    {
    /* check if driver already installed */

#ifdef INCLUDE_PC_CONSOLE
    if (pcConDrv () == ERROR)
       return (ERROR);
#endif 

    if (tyCoDrvNum > 0)
	return (OK);

    tyCoHrdInit ();

    tyCoDrvNum = iosDrvInstall (tyCoOpen, (FUNCPTR) NULL, tyCoOpen,
				(FUNCPTR) NULL, tyRead, tyWrite, tyCoIoctl);

    return (tyCoDrvNum == ERROR ? ERROR : OK);
    }
/*******************************************************************************
*
* tyCoDevCreate - create a device for an on-board serial port
*
* This routine creates a device for a specified serial port.  Each port
* to be used should have exactly one device associated with it by calling
* this routine.
*
* For instance, to create the device "/tyCo/0", with buffer sizes of 512
* bytes, the proper call would be:
* .CS
*     tyCoDevCreate ("/tyCo/0", 0, 512, 512);
* .CE
*
* RETURNS: OK, or ERROR if the driver is not installed, the channel is
* invalid, or the device already exists.
*
* SEE ALSO: tyCoDrv()
*/

STATUS tyCoDevCreate
    (
    char *name,		/* name to use for this device */
    FAST int channel,	/* physical channel for this device (0 only) */
    int rdBufSize,	/* read buffer size, in bytes */
    int wrtBufSize	/* write buffer size, in bytes */
    )
    {
    TY_CO_DEV *pTyCoDv = &tyCoDv[channel];

    if (channel >= N_UART_CHANNELS)
	{
#ifdef	INCLUDE_PC_CONSOLE
        channel -= N_UART_CHANNELS;
        if (channel < N_VIRTUAL_CONSOLES)
	    return (pcConDevCreate (name, channel, rdBufSize, wrtBufSize));
#endif	/* INCLUDE_PC_CONSOLE */
	return (ERROR);
	}

    if (tyCoDrvNum <= 0)
	{
	errnoSet (S_ioLib_NO_DRIVER);
	return (ERROR);
	}

    /* if this device already exists, don't create it */

    if (pTyCoDv->created)
	return (ERROR);

    if (tyDevInit (&pTyCoDv->tyDev, rdBufSize, wrtBufSize, 
	(FUNCPTR)tyCoStartup) != OK)
	{
	return (ERROR);
	}

    /* enable the receiver and receiver error */

    sysOutByte (pTyCoDv->ier, 0x01);

    if (channel == 0)
	sysIntEnablePIC (COM1_INT_LVL);
    else
	sysIntEnablePIC (COM2_INT_LVL);

    /* mark the device as created, and add the device to the I/O system */

    pTyCoDv->created = TRUE;
    return (iosDevAdd (&pTyCoDv->tyDev.devHdr, name, tyCoDrvNum));
    }
/*******************************************************************************
*
* tyCoHrdInit - initialize the UART
*/

LOCAL void tyCoHrdInit (void)

    {
    int oldLevel = intLock ();
    TY_CO_DEV *pTyCoDv;
    int ix;

    for (ix = 0; ix < N_UART_CHANNELS; ix++)
	{
	pTyCoDv = &tyCoDv[ix];

	/* initialization 9600 baud */

	sysOutByte (pTyCoDv->ier,  0x00);
	sysOutByte (pTyCoDv->mdc,  0x00);
	sysInByte  (pTyCoDv->iid);
	sysInByte  (pTyCoDv->lst);
	sysInByte  (pTyCoDv->msr);
	sysOutByte (pTyCoDv->lcr,  0x80);
	sysOutByte (pTyCoDv->brdl, 0x0c);
	sysOutByte (pTyCoDv->brdh, 0x00);

	/* 8 data bits, 1 stop bit, no parity */

	sysOutByte (pTyCoDv->lcr, 0x03);

	/* enable the receiver and transmitter */

	sysOutByte (pTyCoDv->mdc, 0x0b);
	}

    /* all interrupts are masked out: the receiver interrupt will be enabled
     * in the tyCoDevCreate 
     */

    intUnlock (oldLevel);

    } 
/*******************************************************************************
*
* tyCoOpen - open file to UART
*
* ARGSUSED1
*/

LOCAL int tyCoOpen
    (
    TY_CO_DEV *pTyCoDv,
    char *name,
    int mode
    )
    {
    return ((int) pTyCoDv);
    }
/*******************************************************************************
*
* tyCoIoctl - special device control
*
* This routine handles FIOBAUDRATE requests and passes all others to tyIoctl.
*
* RETURNS: OK or ERROR if invalid baud rate, or whatever tyIoctl returns.
*/

LOCAL STATUS tyCoIoctl
    (
    TY_CO_DEV *pTyCoDv,	/* device to control */
    int request,	/* request code */
    int arg		/* some argument */
    )
    {
    int ix;
    int status;

    switch (request)
	{
	case FIOBAUDRATE:
	    status = ERROR;
	    for (ix = 0; ix < NELEMENTS (baudTable); ix++)
		{
		if (baudTable [ix].rate == arg)	/* lookup baud rate value */
		    {
    		    sysOutByte (pTyCoDv->lcr, 0x80);
    		    sysOutByte (pTyCoDv->brdh, MSB (baudTable[ix].preset));
    		    sysOutByte (pTyCoDv->brdl, LSB (baudTable[ix].preset));
    		    sysOutByte (pTyCoDv->lcr, 0x03);
		    status = OK;
		    break;
		    }
		}
	    break;

	default:
	    status = tyIoctl (&pTyCoDv->tyDev, request, arg);
	    break;
	}

    return (status);
    }
/*******************************************************************************
*
* tyCoInt - handle a receiver/transmitter interrupt
*
* This routine gets called to handle interrupts.
* If there is another character to be transmitted, it sends it.  If
* not, or if a device has never been created for this channel, just
* disable the interrupt.
*/

void tyCoInt
    (
    TY_CO_DEV *pTyCoDv
    )
    {
    char outChar;
    char interruptID;
    char lineStatus;
    int ix = 0;

    interruptID = sysInByte (pTyCoDv->iid);

    do {

	interruptID &= 0x06;

        if (interruptID == 0x06)
            lineStatus = sysInByte (pTyCoDv->lst);
        else if (interruptID == 0x04)
	    {
            if (pTyCoDv->created)
	        tyIRd (&pTyCoDv->tyDev, sysInByte (pTyCoDv->data));
	    else
	        sysInByte (pTyCoDv->data);
	    }
        else if (interruptID == 0x02)
	    {
            if ((pTyCoDv->created && tyITx (&pTyCoDv->tyDev, &outChar)) == OK)
	        sysOutByte (pTyCoDv->data, outChar);
            else
	        sysOutByte (pTyCoDv->ier, 0x01);
	    }

        interruptID = sysInByte (pTyCoDv->iid);

	} while (((interruptID & 0x01) == 0) && (ix++ < 10));
    }

/*******************************************************************************
*
* tyCoStartup - transmitter startup routine
*
* Call interrupt level character output routine.
*/

LOCAL void tyCoStartup
    (
    TY_CO_DEV *pTyCoDv		/* tty device to start up */
    )
    {
    char outChar;

    /* enable the transmitter and it should interrupt to write the next char */

    if (tyITx (&pTyCoDv->tyDev, &outChar) == OK)
	sysOutByte (pTyCoDv->data, outChar);
	
    sysOutByte (pTyCoDv->ier, 0x03);
    }
