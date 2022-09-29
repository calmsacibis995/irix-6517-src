/* wdbTemplatektDrv.c - template packet driver for lightweight UDP/IP */

/*
modification history
--------------------
01a,23aug95,ms   written.
*/

/*
DESCRIPTION

OVERVIEW
--------
This module is a template for drivers interfacing with the WDB agent's
lightweight UDP/IP interpreter. It can be used as a starting point
when writing new drivers. Such drivers are the lightweight equivalent
of a network interface driver.

These drivers, along with the lightweight UDP-IP interpreter, have two
benefits over the stand combination of a netif driver + the full VxWorks
networking stack; First, they can run in a much smaller amout of target
memory because the lightweight UDP-IP interpreter is much smaller than
the VxWorks network stack (about 800 bytes total). Second, they provide
a communication path which is independant of the OS, and thus can be
used to support an external mode (e.g., monitor style) debug agent.

Throughout this file the word "template" is used in place of a real
driver name. For example, if you were writing a lightweight driver
for the lance ethernet chip, you would want to substitute "template"
with "ln" throughout this file.

PACKET READY CALLBACK
---------------------
When the driver detects that a packet has arrived (either in its receiver
ISR or in its poll input routine), it invokes a callback to pass the
data to the debug agent. Right now the callback routine is called "udpRcv",
however other callbacks may be added in the future. 
The driver's wdbTemplateDevInit() routine should be passed the callback as
a parameter and place it in the device data structure. That way the driver
will continue to work if new callbacks are added later.

MODES
-----
Ideally the driver should support both polled and interrupt mode, and be
capable of switching modes dynamically. However this is not required.
When the agent is not running, the driver will be placed in "interrupt mode"
so that the agent can be activated as soon as a packet arrives.
If your driver does not support an interrupt mode, you can simulate this
mode by spawning a VxWorks task to poll the device at periodic intervals
and simulate a receiver ISR when a packet arrives.

For dynamically mode switchable drivers, be aware that the driver may be
asked to switch modes in the middle of its input ISR. A driver's input ISR
will look something like this:

   doSomeStuff();
   pPktDev->wdbDrvIf.stackRcv (pMbuf);   /@ invoke the callback @/
   doMoreStuff();

If this channel is used as a communication path to an external mode
debug agent, then the agent's callback will lock interrupts, switch
the device to polled mode, and use the device in polled mode for awhile.
Later on the agent will unlock interrupts, switch the device back to
interrupt mode, and return to the ISR.
In particular, the callback can cause two mode switches, first to polled mode
and then back to interrupt mode, before it returns.
This may require careful ordering of the callback within the interrupt
handler. For example, you may need to acknowledge the interrupt within
the doSomeStuff() processing rather than the doMoreStuff() processing.

USAGE
-----
The driver is typically only called only from usrWdb.c. The only directly
callable routine in this module is wdbTemplatePktDevInit().
You will need to modify usrWdb.c to allow your driver to be initialized
by the debug agent.
You will want to modify usrWdb.c to include your driver's header
file, which should contain a definition of WDB_TEMPLATE_PKT_MTU.
There is a default user-selectable macro called WDB_MTU, which must
be no larger than WDB_TEMPLATE_PKT_MTU. Modify the begining of
usrWdb.c to insure that this is the case by copying the way
it is done for the other drivers.
The routine wdbCommIfInit() also needs to be modified so that if your
driver is selected as the WDB_COMM_TYPE, then your drivers init
routine will be called. Search usrWdb.c for the macro "WDB_COMM_UDPL_CUSTOM"
and mimic that style of initialization for your driver.

DATA BUFFERING
--------------
The drivers only need to handle one input packet at a time because
the WDB protocol only supports one outstanding host-request at a time.
If multiple input packets arrive, the driver can simply drop them.
The driver then loans the input buffer to the WDB agent, and the agent
invokes a driver callback when it is done with the buffer.

For output, the agent will pass the driver a chain of mbufs, which
the driver must send as a packet. When it is done with the mbufs,
it calls wdbMbufChainFree() to free them.
The header file wdbMbuflib.h provides the calls for allocating, freeing,
and initializing mbufs for use with the lightweight UDP/IP interpreter.
It ultimatly makes calls to the routines wdbMbufAlloc and wdbMbufFree, which
are provided in source code in usrWdb.c.
*/

#include "string.h"
#include "errno.h"
#include "siolib.h"
#include "intlib.h"
#include "wdb/wdbmbuflib.h"
#include "drv/wdb/wdbtemplatepktdrv.h"


/* pseudo-macros */

#define DRIVER_INIT_HARDWARE(pDev) {}
#define DRIVER_MODE_SET(pDev, mode)
#define DRIVER_PACKET_IS_READY(pDev) FALSE
#define DRIVER_RESET_INPUT(pDev) {pDev->inputBusy=FALSE;}
#define DRIVER_RESET_OUTPUT(pDev) {pDev->outputBusy=FALSE;}
#define DRIVER_GET_INPUT_PACKET(pDev, pBuf) (50)
#define DRIVER_DATA_FROM_MBUFS(pDev, pBuf) {}
#define DRIVER_POLL_TX(pDev, pBuf) {}
#define DRIVER_TX_START(pDev) {}

/* forward declarations */

static STATUS wdbTemplatePoll (void *pDev);
static STATUS wdbTemplateTx   (void *pDev, struct mbuf * pMbuf);
static STATUS wdbTemplateModeSet (void *pDev, uint_t newMode);
static void   wdbTemplateFree (void *pDev);

/******************************************************************************
*
* wdbTemplatePktDevInit - initialize a template packet device.
*/

void wdbTemplatePktDevInit
    (
    WDB_TEMPLATE_PKT_DEV *pPktDev,	/* template device structure to init */
    void	(*stackRcv)()		/* receive packet callback (udpRcv) */
    )
    {
    /* initialize the wdbDrvIf field with driver info */

    pPktDev->wdbDrvIf.mode	= WDB_COMM_MODE_POLL | WDB_COMM_MODE_INT;
    pPktDev->wdbDrvIf.mtu	= WDB_TEMPLATE_PKT_MTU;
    pPktDev->wdbDrvIf.stackRcv	= stackRcv;		/* udpRcv */
    pPktDev->wdbDrvIf.devId	= (WDB_TEMPLATE_PKT_DEV *)pPktDev;
    pPktDev->wdbDrvIf.pollRtn	= wdbTemplatePoll;
    pPktDev->wdbDrvIf.pktTxRtn	= wdbTemplateTx;
    pPktDev->wdbDrvIf.modeSetRtn = wdbTemplateModeSet;

    /* initialize the device specific fields in the driver structure */

    pPktDev->inputBusy		= FALSE;
    pPktDev->outputBusy		= FALSE;

    /*
     * Put your hardware initialization code here. It should initialze
     * the device structure's register addresses and initialize
     * the hardware.
     * You can either connect the driver's ISR handler(s) here (in which
     * case you should have the interrupt vector passed to this driver init
     * routine), or just require the BSP to connect the driver interrupt
     * handler in sysHwInit2() routine.
     */

    DRIVER_INIT_HARDWARE(pPktDev);

    /* put the device in a quiescent state (setting polled mode is one way) */

    wdbTemplateModeSet (pPktDev, WDB_COMM_MODE_POLL);
    }

/******************************************************************************
*
* wdbTemplateInt - template driver interrupt handler
*
* RETURNS: N/A
*/

void wdbTemplateInt
    (
    WDB_TEMPLATE_PKT_DEV	* pPktDev
    )
    {
    struct mbuf * pMbuf;
    int           packetSize;

    /*
     * The code below is for handling a packet-recieved interrupt.
     * A real driver may also have to check for other interrupting
     * conditions such as DMA errors, etc.
     */

    /* input buffer already in use - drop this packet */

    if (pPktDev->inputBusy)
	{
        DRIVER_RESET_INPUT(pPktDev);
	return;
	}

    /*
     * Fill the input buffer with the packet. Use an mbuf cluster to pass
     * the packet on to the agent.
     */

    packetSize = DRIVER_GET_INPUT_PACKET (pPktDev, pPktDev->inBuf);
    pMbuf = wdbMbufAlloc();
    if (pMbuf == NULL)
	{
	DRIVER_RESET_INPUT(pPktDev);
	return;
	}

    wdbMbufClusterInit (pMbuf, pPktDev->inBuf, packetSize, \
			(int (*)())wdbTemplateFree, (int)pPktDev);
    pPktDev->inputBusy = TRUE;
    (*pPktDev->wdbDrvIf.stackRcv) (pMbuf);  /* invoke callback */
    }

/******************************************************************************
*
* wdbTemplateTx - transmit a packet.
*
* The packet is realy a chain of mbufs. We may have to just queue up
* this packet is we are already transmitting.
*
* RETURNS: OK or ERROR
*/

static STATUS wdbTemplateTx
    (
    void *	pDev,
    struct mbuf * pMbuf
    )
    {
    WDB_TEMPLATE_PKT_DEV *pPktDev = pDev;
    int 	lockKey;

    /* if we are in polled mode, transmit the packet in a loop */

    if (pPktDev->mode == WDB_COMM_MODE_POLL)
	{
	DRIVER_POLL_TX(pPktDev, pMbuf);
	return (OK);
	}

    lockKey = intLock();

    /* if the txmitter isn't cranking, queue the packet and start txmitting */

    if (pPktDev->outputBusy == FALSE)
	{
	pPktDev->outputBusy = TRUE;

	DRIVER_DATA_FROM_MBUFS(pPktDev, pMbufs);
	DRIVER_TX_START(pPktDev);
	}

    /* else just queue the packet */

    else
	{
	DRIVER_DATA_FROM_MBUFS(pPktDev, pMbufs);
	}

    intUnlock (lockKey);
    wdbMbufChainFree (pMbuf);
    return (OK);
    }

/******************************************************************************
*
* wdbTemplateFree - free the input buffer
*
* This is the callback used to let us know the agent is done with the
* input buffer we loaded it.
*
* RETURNS: N/A
*/

static void wdbTemplateFree
    (
    void *	pDev
    )
    {
    WDB_TEMPLATE_PKT_DEV *	pPktDev = pDev;

    pPktDev->inputBusy = FALSE;
    }

/******************************************************************************
*
* wdbTemplateModeSet - switch driver modes
*
* RETURNS: OK for a supported mode, else ERROR
*/

static STATUS wdbTemplateModeSet
    (
    void *	pDev,
    uint_t	newMode
    )
    {
    WDB_TEMPLATE_PKT_DEV * pPktDev = pDev;

    DRIVER_RESET_INPUT  (pPktDev);
    DRIVER_RESET_OUTPUT (pPktDev);

    if (newMode == WDB_COMM_MODE_INT)
	DRIVER_MODE_SET (pPktDev, WDB_COMM_MODE_INT);
    else if (newMode == WDB_COMM_MODE_POLL)
	DRIVER_MODE_SET (pPktDev, WDB_COMM_MODE_POLL);
    else
	return (ERROR);

    return (ERROR);
    }

/******************************************************************************
*
* wdbTemplatePoll - poll for a packet
*
* This routine polls for a packet. If a packet has arrived it invokes
* the agents callback.
*
* RETURNS: OK if a packet has arrived, else ERROR.
*/ 

static STATUS wdbTemplatePoll
    (
    void *	pDev
    )
    {
    WDB_TEMPLATE_PKT_DEV * pPktDev = pDev;
    struct mbuf * pMbuf;
    int           packetSize;

    if (DRIVER_PACKET_IS_READY(pPktDev))
	{
	/*
	 * Fill the input buffer with the packet. Use an mbuf cluster to pass
	 * the packet on to the agent.
	 */

	packetSize = DRIVER_GET_INPUT_PACKET (pPktDev, pPktDev->inBuf);
	pMbuf = wdbMbufAlloc();
	if (pMbuf == NULL)
	    {
	    DRIVER_RESET_INPUT(pPktDev);
	    return (ERROR);
	    }

	wdbMbufClusterInit (pMbuf, pPktDev->inBuf, packetSize, \
			(int (*)())wdbTemplateFree, (int)pPktDev);
	pPktDev->inputBusy = TRUE;
	(*pPktDev->wdbDrvIf.stackRcv) (pMbuf);  /* invoke callback */
	return (OK);
	}

    return (ERROR);
    }

