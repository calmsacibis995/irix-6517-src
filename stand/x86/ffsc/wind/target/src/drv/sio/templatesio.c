/* templateSio.c - template serial driver */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,01aug95,ms   written.
*/

/*
DESCRIPTION

OVERVIEW
--------
This is a template serial driver. It can be used as a starting point
when writing new drivers for VxWorks version 5.3 or later.

These drivers support new functionality not found in the older style
serial drivers. First, they provide an interface for setting hardware
options; e.g., the number of stop bits, data bits, parity, etc.
Second, they provide an interface for polled communication which
can be used to provided external mode debugging (i.e., ROM monitor
style debugging) over a serial line. Currently only asynchronous mode
drivers are supported.

Throughout this file the word "template" is used in place of a real
device name, which by convention uses the first letter of the
manufacturers name followed by the part number. For example, the
Zilog 8530 serial device would have a data structure called a
Z8530_DUSART, rather than TEMPLATE_DUSART.

DATA STRUCTURES
---------------
Device data structures are defined in the header file h/drv/sio/templateSio.h.
A device data structure, TEMPLATE_CHAN, is defined for each channel.
Devices with multiple serial channels also have a data structure for
the whole chip. For example, this driver has a chip-level structure
called a TEMPLATE_DUSART which contains two channel structures as members.

CALLBACKS
---------
Servicing a "transmitter ready" interrupt involves making a callback to a
higher level library in order to get a character to transmit.
By default, this driver installs dummy callback routines which do
nothing. A higher layer library that wants to use this driver (e.g., ttyDrv)
will install its own callback routine using the SIO_INSTALL_CALLBACK
ioctl command.
Likewise, a receiver interrupt handler makes a callback to pass the
character to the higher layer library.

MODES
-----
Ideally the driver should support both polled and interrupt modes, and be
capable of switching modes dynamically. However this is not required.
VxWorks will be able to support a tty device on this driver even if
the driver only supports interrupt mode.
A communication path to an external mode debug agent (i.e., for monitor
style debugging) can be supported on this driver even if the driver
only supports polled mode.
Adding dynamic mode switching allows the external agent to be activated
whenever a message is sent to it (e.g., to be interrupted on first packet).

For dynamically mode switchable drivers, be aware that the driver may be
asked to switch modes in the middle of its input ISR. A driver's input ISR
will look something like this:
   inChar = *pDev->dr;          /@ read a char from data register @/
   *pDev->cr = GOT_IT;          /@ acknowledge the interrupt @/
   pDev->putRcvChar (...);      /@ give the character to the higher layer @/
If this channel is used as a communication path to an external mode
debug agent, and if the character received is a special "end of packet"
character, then the agent's callback will lock interrupts, switch
the device to polled mode, and use the device in polled mode for awhile.
Later on the agent will unlock interrupts, switch the device back to
interrupt mode, and return to the ISR.
In particular, the callback can cause two mode switches, first to polled mode
and then back to interrupt mode, before it returns.
This may require careful ordering of the callback within the interrupt
handler. For example, you may need to acknowledge the interrupt before
invoking the callback.

USAGE
-----
The driver is typically only called only by the BSP. The directly callable
routines in this module are templateDevInit(), templateIntRcv(),
templateIntTx(), and templateIntErr().
The BSP calls templateDevInit() to reset the device.
It connects the driver's interrupt handlers (templateIntRcv, templateIntTx,
and templateIntErr), using intConnect().

BSP
---
By convention all the BSP-specific serial initialization is performed in
a file called sysSerial.c, which is #include'ed by sysLib.c.
sysSerial.c implements at least three functions, sysSerialHwInit()
sysSerialHwInit2(), and sysSerialChanGet(), which work as follows:

sysSerialHwInit is called by sysHwInit to initialize the serial devices.
This routine will initialize all the board specific fields in the
TEMPLATE_DUSART structure (e.g., register I/O addresses, etc) before
calling templateDevInit(), which resets the device and installs the driver
function pointers. sysSerialHwInit() should also perform any other processing
which is needed for the serial drivers, such as configuring on-board
interrupt controllers as appropriate.

sysSerialHwInit2 is called by sysHwInit2 to connect the serial driver's
interrupt handlers using intConnect().

sysSerialChanGet is called by usrRoot to get the serial channel descriptor
associated with a serial channel number. The routine takes a single parameter
which is a channel number ranging between zero and NUM_TTY. It returns
a pointer to the corresponding channel descriptor, SIO_CHAN *, which is
just the address of the TEMPLATE_CHAN structure.

TESTING
-------
The interrupt driven interface can be tested in the usualy way; VxWorks
prints to the serial console when it comes up, so seeing the usual VxWorks
output on power-up shows that the driver is basically working.
The VxWorks portkit test can be used to perform a more strenuous test.

The polled interface should be easy enough to verify - you should be able
to call the channels SIO_MODE_SET ioctl to put it in polled mode.

The dynamic mode switching can be verified using the tornado tools.
Reconfigure the agent to use the WDB_COMM_UDLP_SLIP communication path (see
the Configuration section in the VxWorks run-time Guide for details).
Start VxWorks, and connect the tgtsvr to the agent using the wdbserial
backend (see the Tornado Users Guide for details).
Start the wtxtcl shell as follows:
	% wtxtcl
From the tcl prompt, attach to the target server:
	wtxtcl.ex> wtxToolAttach <tgtsvr name>
Tell the agent to switch to external mode, and verify the reply is OK (0).
	wtxtcl.ex>wtxAgentModeSet 2
	0
Ask the agent to suspend the system (the request will reach the agent in
interrupt mode, but the reply will be sent in polled mode):
	wtxtcl.ex>wtxContextSuspend 0 0
	0
At this point the target will be suspended. The console should apprear
frozen (if the board has a console device), and you will not be able to
"ping" the target's network interface.
Resume the target:
	wtxtcl.ex>wtxContextResume 0 0
	0
The target should now be running again, so you should be able to type into
the console (if the board has a console device) and ping the targets network
interface from the host.

INCLUDE FILES: drv/sio/templateSio.h sioLib.h
*/

#include "vxworks.h"
#include "siolib.h"
#include "intlib.h"
#include "errno.h"
#include "drv/sio/templatesio.h"

/* forward static declarations */

static int    templateTxStartup (SIO_CHAN * pSioChan);
static int    templateCallbackInstall (SIO_CHAN *pSioChan, int callbackType,
				    STATUS (*callback)(), void *callbackArg);
static int    templatePollOutput (SIO_CHAN *pSioChan, char	outChar);
static int    templatePollInput (SIO_CHAN *pSioChan, char *thisChar);
static int    templateIoctl (SIO_CHAN *pSioChan, int request, void *arg);
static STATUS dummyCallback (void);

/* local variables */

static SIO_DRV_FUNCS templateSioDrvFuncs =
    {
    templateIoctl,
    templateTxStartup,
    templateCallbackInstall,
    templatePollInput,
    templatePollOutput
    };

/******************************************************************************
*
* templateDevInit - initialize a TEMPLATE_DUSART
*
* This routine initializes the driver
* function pointers and then resets the chip in a quiescent state.
* The BSP must have already initialized all the device addresses and the
* baudFreq fields in the TEMPLATE_DUSART structure before passing it to
* this routine.
*
* RETURNS: N/A
*/

void templateDevInit
    (
    TEMPLATE_DUSART * pDusart
    )
    {
    /* initialize each channel's driver function pointers */

    pDusart->portA.pDrvFuncs	= &templateSioDrvFuncs;
    pDusart->portB.pDrvFuncs	= &templateSioDrvFuncs;

    /* install dummy driver callbacks */

    pDusart->portA.getTxChar    = dummyCallback;
    pDusart->portA.putRcvChar	= dummyCallback;
    pDusart->portB.getTxChar    = dummyCallback;
    pDusart->portB.putRcvChar	= dummyCallback;
    
    /* reset the chip */

    *pDusart->masterCr = TEMPLATE_RESET_CHIP;

    /* setting polled mode is one way to make the device quiet */

    templateIoctl ((SIO_CHAN *)&pDusart->portA, SIO_MODE_SET,
		(void *)SIO_MODE_POLL);
    templateIoctl ((SIO_CHAN *)&pDusart->portB, SIO_MODE_SET,
		(void *)SIO_MODE_POLL);
    }

/******************************************************************************
*
* templateIntRcv - handle a channel's receive-character interrupt.
*
* RETURNS: N/A
*/ 

void templateIntRcv
    (
    TEMPLATE_CHAN *	pChan		/* channel generating the interrupt */
    )
    {
    char            inChar;

    /*
     * Grab the input character from the chip and hand it off via a
     * callback. For chips with input FIFO's it is more efficient
     * to empty the entire FIFO here.
     */

    inChar	= *pChan->dr;		/* grab the character */
    *pChan->cr	= TEMPLATE_RESET_INT;	/* acknowledge the interrupt */
    (*pChan->putRcvChar) (pChan->putRcvArg, inChar); /* hand off the char */
    }

/******************************************************************************
*
* templateIntTx - handle a channels transmitter-ready interrupt.
*
* RETURNS: N/A
*/ 

void templateIntTx
    (
    TEMPLATE_CHAN *	pChan		/* channel generating the interrupt */
    )
    {
    char            outChar;

    /*
     * If there's a character to transmit then write it out, else reset
     * the transmitter. For chips with output FIFO's it is more efficient
     * to fill the entire FIFO here.
     */

    if ((*pChan->getTxChar) (pChan->getTxArg, &outChar) != ERROR)
	*pChan->dr = outChar;
    else
        *pChan->cr = TEMPLATE_RESET_TX;

    *pChan->cr	= TEMPLATE_RESET_INT;	/* acknowledge the interrupt */
    }

/******************************************************************************
*
* templateIntErr - handle a channels error interrupt.
*
* RETURNS: N/A
*/ 

void templateIntErr
    (
    TEMPLATE_CHAN *	pChan		/* channel generating the interrupt */
    )
    {
    /* reset the error condition and ignore */

    *pChan->cr	= TEMPLATE_RESET_ERR;	/* reset the error */
    *pChan->cr	= TEMPLATE_RESET_INT;	/* acknowledge the interrupt */
    }

/******************************************************************************
*
* templateTxStartup - start the interrupt transmitter.
*
* RETURNS: OK on success, ENOSYS if the device is polled-only, or
*          EIO on hardware error.
*/

static int templateTxStartup
    (
    SIO_CHAN * pSioChan                 /* channel to start */
    )
    {
    TEMPLATE_CHAN * pChan = (TEMPLATE_CHAN *)pSioChan;
    char   outChar;

    /* writing a character to the data register startes the transmitter
     * for this chip */

    if ((*pChan->getTxChar) (pChan->getTxArg, &outChar) != ERROR)
	{
	*pChan->dr = outChar;
	}

    return (OK);
    }

/******************************************************************************
*
* templateCallbackInstall - install ISR callbacks to get/put chars.
*
* This driver allows interrupt callbacks for transmitting characters
* and receiving characters. In general, drivers may support other
* types of callbacks too.
*
* RETURNS: OK on success, or ENOSYS for an unsupported callback type.
*/ 

static int templateCallbackInstall
    (
    SIO_CHAN *	pSioChan,               /* channel */
    int		callbackType,           /* type of callback */
    STATUS	(*callback)(),          /* callback */
    void *      callbackArg             /* parameter to callback */
    )
    {
    TEMPLATE_CHAN * pChan = (TEMPLATE_CHAN *)pSioChan;

    switch (callbackType)
	{
	case SIO_CALLBACK_GET_TX_CHAR:
	    pChan->getTxChar	= callback;
	    pChan->getTxArg	= callbackArg;
	    return (OK);
	case SIO_CALLBACK_PUT_RCV_CHAR:
	    pChan->putRcvChar	= callback;
	    pChan->putRcvArg	= callbackArg;
	    return (OK);
	default:
	    return (ENOSYS);
	}

    }

/******************************************************************************
*

* templatePollOutput - output a character in polled mode.
*
* RETURNS: OK if a character arrived, EIO on device error, EAGAIN
*	   if the output buffer if full. ENOSYS if the device is
*          interrupt-only.
*/

static int templatePollOutput
    (
    SIO_CHAN *	pSioChan,
    char	outChar
    )
    {
    TEMPLATE_CHAN * pChan = (TEMPLATE_CHAN *)pSioChan;

    /* is the transmitter ready to accept a character? */

    if ((*pChan->sr & TEMPLATE_TX_READY) == 0x00)
	return (EAGAIN);

    /* write out the character */

    *pChan->dr = outChar;

    return (OK);
    }

/******************************************************************************
*
* templatePollInput - poll the device for input.
*
* RETURNS: OK if a character arrived, EIO on device error, EAGAIN
*	   if the input buffer if empty, ENOSYS if the device is
*          interrupt-only.
*/

static int templatePollInput
    (
    SIO_CHAN *	pSioChan,
    char *	thisChar
    )
    {
    TEMPLATE_CHAN * pChan = (TEMPLATE_CHAN *)pSioChan;

    if ((*pChan->sr & TEMPLATE_RX_AVAIL) == 0x00)
	return (EAGAIN);

    /* got a character */

    *thisChar = *pChan->dr;

    return (OK);
    }

/******************************************************************************
*
* templateModeSet - toggle between interrupt and polled mode.
*
* RETURNS: OK on success, EIO on unsupported mode.
*/

static int templateModeSet
    (
    TEMPLATE_CHAN * pChan,		/* channel */
    uint_t	    newMode          	/* new mode */
    )
    {
    if ((newMode != SIO_MODE_POLL) && (newMode != SIO_MODE_INT))
	return (EIO);

    /* set the new mode */

    pChan->mode = newMode;

    if (pChan->mode == SIO_MODE_INT)
	*pChan->cr |= TEMPLATE_INT_ENABLE;
    else
	*pChan->cr &= ~TEMPLATE_INT_ENABLE;

    return (OK);
    }

/*******************************************************************************
*
* templateIoctl - special device control
*
* RETURNS: OK on success, ENOSYS on unsupported request, EIO on failed
*          request.
*/

static int templateIoctl
    (
    SIO_CHAN *	pSioChan,		/* device to control */
    int		request,		/* request code */
    void *	someArg			/* some argument */
    )
    {
    TEMPLATE_CHAN *pChan = (TEMPLATE_CHAN *) pSioChan;
    int     oldlevel;		/* current interrupt level mask */
    short   baudConstant;
    int     arg = (int)someArg;

    switch (request)
	{
	case SIO_BAUD_SET:
	    /*
	     * Set the baud rate. Return EIO for an invalid baud rate, or
	     * OK on success.
	     */

	    if (arg < 50 || arg > 38400)
	        {
		return (EIO);
	        }

	    /* Calculate the baud rate constant for the new baud rate */

	    baudConstant = pChan->baudFreq / arg / 16;

	    /* disable interrupts during chip access */

	    oldlevel = intLock ();
	    *pChan->br = (short)baudConstant;
	    intUnlock (oldlevel);

	    return (OK);

	case SIO_BAUD_GET:
	    /* Get the baud rate and return OK */

	    *(int *)arg = *pChan->br * pChan->baudFreq * 16;
	    return (OK);

	case SIO_MODE_SET:
	    /*
	     * Set the mode (e.g., to interrupt or polled). Return OK
	     * or EIO for an unknown or unsupported mode.
	     */

	    return (templateModeSet (pChan, arg));

	case SIO_MODE_GET:
	    /*
	     * Get the current mode and return OK.
	     */

	    *(int *)arg = pChan->mode;
	    return (OK);

	case SIO_AVAIL_MODES_GET:
	    /*
	     * Get the available modes and return OK.
	     */

	    *(int *)arg = SIO_MODE_INT | SIO_MODE_POLL;
	    return (OK);

	case SIO_HW_OPTS_SET:
	    /*
	     * Optional command to set the hardware options (as defined
	     * in sioLib.h).
	     * Return OK, or ENOSYS if this command is not implemented.
	     * Note: several hardware options are specified at once.
	     * This routine should set as many as it can and then return
	     * OK. The SIO_HW_OPTS_GET is used to find out which options
	     * were actually set.
	     */

	case SIO_HW_OPTS_GET:
	    /*
	     * Optional command to get the hardware options (as defined
	     * in sioLib.h).
	     * Return OK or ENOSYS if this command is not implemented.
	     * Note: if this command is unimplemented, it will be
	     * assumed that the driver options are CREAD | CS8 (e.g.,
	     * eight data bits, one stop bit, no parity, ints enabled).
	     */

	default:
	    return (ENOSYS);
	}
    }

/*******************************************************************************
*
* dummyCallback - dummy callback routine.
*
* RETURNS: ERROR.
*/

STATUS dummyCallback (void)
    {
    return (ERROR);
    }

