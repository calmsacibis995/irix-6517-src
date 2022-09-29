/* i8250Sio.c - I8250 serial driver */

/* Copyright 1984-1995 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01f,12oct95,dgf  added bauds > 38k
01e,03aug95,myz  fixed the warning messages
01d,12jul95,myz  fixed the baud rate problem.
01c,20jun95,ms	 fixed comments for mangen.
01b,15jun95,ms   updated for new driver structure
01a,15mar95,myz  written (using i8250Serial.c + the VxMon polled driver).
*/

/*
DESCRIPTION
This is the driver for the Intel 8250 UART Chip used on the PC 386.
It uses the SCCs in asynchronous mode only.

USAGE
An I8250_CHAN structure is used to describe the chip.
The BSP's sysHwInit() routine typically calls sysSerialHwInit()
which initializes all the register values in the I8250_CHAN structure
(except the SIO_DRV_FUNCS) before calling i8250HrdInit().
The BSP's sysHwInit2() routine typically calls sysSerialHwInit2() which
connects the chips interrupt handler (i8250Int) via intConnect().

IOCTL FUNCTIONS
This driver responds to all the same ioctl() codes as a normal serial driver;
for more information, see the comments in sioLib.h.  As initialized, the
available baud rates are 110, 300, 600, 1200, 2400, 4800, 9600, 19200, and
38400.

INCLUDE FILES: drv/sio/i8250Sio.h
*/

#include "vxworks.h"
#include "iv.h"
#include "intlib.h"
#include "errnolib.h"
#include "drv/sio/i8250sio.h"


/* locals */

/* baudTable is a table of the available baud rates, and the values to write
 * to the UART's baud rate divisor {high, low} register. the formula is
 * 1843200(source) / (16 * baudrate)
 */

static BAUD baudTable [] =
    {
    {50, 2304}, {75, 1536}, {110, 1047}, {134, 857}, {150, 768},
    {300, 384}, {600, 192}, {1200, 96}, {2000, 58}, {2400, 48},
    {3600,32}, {4800, 24}, {7200,16}, {9600, 12}, {19200, 6}, {38400, 3},
    {57600, 2}, {115200, 1}
    };

static SIO_DRV_FUNCS i8250SioDrvFuncs;

/* forward declarations */

static void i8250InitChannel (I8250_CHAN *);
static int i8250Ioctl(I8250_CHAN *,int,int);
static int i8250Startup(SIO_CHAN *pSioChan);
static int i8250PRxChar (SIO_CHAN *pSioChan,char *ch);
static int i8250PTxChar(SIO_CHAN *pSioChan,char ch);

/******************************************************************************
*
* i8250CallbackInstall - install ISR callbacks to get put chars.
*/ 

static int i8250CallbackInstall
    (
    SIO_CHAN *  pSioChan,
    int         callbackType,
    STATUS      (*callback)(),
    void *      callbackArg
    )
    {
    I8250_CHAN * pChan = (I8250_CHAN *)pSioChan;

    switch (callbackType)
        {
        case SIO_CALLBACK_GET_TX_CHAR:
            pChan->getTxChar    = callback;
            pChan->getTxArg     = callbackArg;
            return (OK);
        case SIO_CALLBACK_PUT_RCV_CHAR:
            pChan->putRcvChar   = callback;
            pChan->putRcvArg    = callbackArg;
            return (OK);
        default:
            return (ENOSYS);
        }
    }


/******************************************************************************
*
* i8250HrdInit - initialize the chip
*
* This routine is called to reset the chip in a quiescent state.
*/

void i8250HrdInit
    (
    I8250_CHAN *pChan
    )
    {
    (*pChan->outByte) (pChan->ier,0x0);  /* disable interrupts */

    if (i8250SioDrvFuncs.ioctl == NULL)
	{
	i8250SioDrvFuncs.ioctl		= (int (*)())i8250Ioctl;
	i8250SioDrvFuncs.txStartup	= i8250Startup;
	i8250SioDrvFuncs.callbackInstall = i8250CallbackInstall;
	i8250SioDrvFuncs.pollInput	= i8250PRxChar;
	i8250SioDrvFuncs.pollOutput	= i8250PTxChar;
	}

    pChan->pDrvFuncs = &i8250SioDrvFuncs;
    }

/*******************************************************************************
*
* i8250InitChannel  - initialize a single channel
*/

static void i8250InitChannel
    (
    I8250_CHAN *pChan
    )
    {
    int oldLevel;

    oldLevel = intLock ();

    /* 8 data bits, 1 stop bit, no parity */

    (*pChan->outByte) (pChan->lcr, 0x03);

    /* enable the receiver and transmitter */

    (*pChan->outByte) (pChan->mdc, 0x0b);

    (*pChan->inByte) (pChan->data);	/* clear the port */

    /* enables interrupts  */

    (*pChan->outByte) (pChan->ier,0x1);
    
    intUnlock (oldLevel);
    }

/*******************************************************************************
*
* i8250Ioctl - special device control
*
* RETURNS: OK on success, EIO on device error, ENOSYS on unsupported
*          request.
*/

static int i8250Ioctl
    (
    I8250_CHAN *pChan,	/* device to control */
    int request,	/* request code */
    int arg		/* some argument */
    )
    {
    int ix;
    int status;
    int baudH;
    int baudL;
    int oldlevel;

    switch (request)
	{
	case SIO_BAUD_SET:

            oldlevel = intLock();
	    status = EIO;
	    for (ix = 0; ix < NELEMENTS (baudTable); ix++)
		{
		if (baudTable [ix].rate == arg)	/* lookup baud rate value */
		    {
    		    (*pChan->outByte) (pChan->lcr, (char)0x83);
    		    (*pChan->outByte) (pChan->brdh, MSB (baudTable[ix].preset));
    		    (*pChan->outByte) (pChan->brdl, LSB (baudTable[ix].preset));
    		    (*pChan->outByte) (pChan->lcr, 0x03);
		    status = OK;
		    break;
		    }
		}

            intUnlock(oldlevel);
	    break;

        case SIO_BAUD_GET:
            
            oldlevel = intLock();

            status = EIO;
            (*pChan->outByte) (pChan->lcr, (char)0x83);
            baudH = (*pChan->inByte)(pChan->brdh);
            baudL = (*pChan->inByte)(pChan->brdl);
            (*pChan->outByte) (pChan->lcr, 0x03);

            for (ix = 0; ix < NELEMENTS (baudTable); ix++)
                {
                if ( baudH  == MSB (baudTable[ix].preset) &&
                     baudL  == LSB (baudTable[ix].preset) )
                    {
                    *(int *)arg = baudTable[ix].rate;
                    status = OK;
                    }
                }

            intUnlock(oldlevel);
            break;


	case SIO_MODE_SET:
            if (!(arg == SIO_MODE_POLL || arg == SIO_MODE_INT))
                {
                status = EIO;
                break;
                }

            oldlevel = intLock();
           
	    /* only reset the channel if it hasn't done yet */

	    if (!pChan->channelMode)
                i8250InitChannel(pChan);

            if (arg == SIO_MODE_POLL)
                (*pChan->outByte) (pChan->ier, 0x00);
            else
                (*pChan->outByte) (pChan->ier, 0x01);
            
            pChan->channelMode = arg;  
          
            intUnlock(oldlevel);
            status = OK;

	    break;

        case SIO_MODE_GET:
            *(int *)arg = pChan->channelMode;
            return (OK);

        case SIO_AVAIL_MODES_GET:
            *(int *)arg = SIO_MODE_INT | SIO_MODE_POLL;
            return (OK);

	default:
	    status = ENOSYS;
	    break;
	}

    return (status);
    }

/*******************************************************************************
*
* i8250Int - handle a receiver/transmitter interrupt
*
* This routine gets called to handle interrupts.
* If there is another character to be transmitted, it sends it.  If
* not, or if a device has never been created for this channel, just
* disable the interrupt.
*/

void i8250Int
    (
    I8250_CHAN  *pChan 
    )
    {
    char outChar;
    char interruptID;
    char lineStatus;
    int ix = 0;
    
    interruptID = (*pChan->inByte) ( pChan->iid );

    do {

	interruptID &= 0x06;

        if (interruptID == 0x06)
            lineStatus = (*pChan->inByte) (pChan->lst);
        else if (interruptID == 0x04)
	    {
            if (pChan->putRcvChar != NULL)
	        (*pChan->putRcvChar)( pChan->putRcvArg,
                               (*pChan->inByte) (pChan->data) );
	    else
	        (*pChan->inByte) (pChan->data);
	    }
        else if (interruptID == 0x02)
	    {
            if ((pChan->getTxChar != NULL) && 
               (*pChan->getTxChar) (pChan->getTxArg, &outChar) == OK )
	        (*pChan->outByte) (pChan->data, outChar);
            else
	        (*pChan->outByte) (pChan->ier, 0x01);
	    }

        interruptID = (*pChan->inByte) (pChan->iid);

	} while (((interruptID & 0x01) == 0) && (ix++ < 10));
    }

/*******************************************************************************
*
* i8250Startup - transmitter startup routine
*
* Call interrupt level character output routine.
*/

static int i8250Startup
    (
    SIO_CHAN *pSioChan		/* tty device to start up */
    )
    {

    /* enable the transmitter and it should interrupt to write the next char */
    
    if (((I8250_CHAN *)pSioChan)->channelMode != SIO_MODE_POLL)
        (*((I8250_CHAN *)pSioChan)->outByte) ( ((I8250_CHAN *)pSioChan)->ier, 0x03);

    return (OK);
    }

/******************************************************************************
*
* i8250PRxChar - poll the device for input.
*
* RETURNS: OK if a character arrived, ERROR on device error, EAGAIN
*          if the input buffer if empty.
*/

static int i8250PRxChar
    (
    SIO_CHAN *pSioChan,
    char *ch
    )
    {
    /* wait for Data */
    if ( ( (*((I8250_CHAN *)pSioChan)->inByte)
			  (((I8250_CHAN *)pSioChan)->lst) & 0x01 ) == 0 )
        return(EAGAIN);

    *ch = ( *((I8250_CHAN *)pSioChan)->inByte) ( ((I8250_CHAN *)pSioChan)->data );
    return(OK);
    }

/******************************************************************************
*
* i8250PTxChar - output a character in polled mode.
*
* RETURNS: OK if a character arrived, ERROR on device error, EAGAIN
*          if the output buffer if full.
*/

static int i8250PTxChar
    (
    SIO_CHAN *pSioChan,
    char ch                   /* character to output */
    )
    {

    /* wait for Empty */
    if ( ( (*((I8250_CHAN *)pSioChan)->inByte)
			  (((I8250_CHAN *)pSioChan)->lst) & 0x40 ) == 0 )      
        return(EAGAIN);

    (*((I8250_CHAN *)pSioChan)->outByte) (((I8250_CHAN *)pSioChan)->data, ch);
    return(OK);
    }

