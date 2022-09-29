/* ns16550Sio.c - NS 16550 UART tty driver */

/* Copyright 1984-1995 Wind River Systems, Inc. */

#include "copyright_wrs.h"

/*
modification history
--------------------
01g,10dec96,db   added hardware options and modem control(SPRs #7570, #7082).
01f,18dec95,myz  added case IIR_TIMEOUT in ns16550Int routine.
01e,28nov95,myz  fixed bugs to work at 19200 baud or above with heavy traffic.
01d,09nov95,jdi  doc: style cleanup.
01c,02nov95,myz  undo 01b fix
01b,02nov95,p_m  added test for 960CA and 960JX around access to lcr field
		 in order to compile on all architectures.
01a,24oct95,myz  written from ns16550Serial.c.
*/

/*
DESCRIPTION
This is the device driver for the ns16550 UART.

USAGE
A NS16550_CHAN data structure is used to describe the chip.

The BSP's sysHwInit() routine typically calls sysSerialHwInit(),
which initializes all the values in the NS16550_CHAN structure (except
the SIO_DRV_FUNCS) before calling ns16550DevInit().
The BSP's sysHwInit2() routine typically calls sysSerialHwInit2(), which
connects the chips interrupts via intConnect() (either the single
interrupt `ns16550Int' or the three interrupts `ns16550IntWr', `ns16550IntRd',
and `ns16550IntEx').

INCLUDE FILES: drv/sio/ns16552Sio.h

*/

#include "vxWorks.h"
#include "intLib.h"
#include "errnoLib.h"
#include "errno.h"
#include "sioLib.h"
#include "drv/serial/ns16552.h"
#include "tty.h"

#if 0
/* local defines       */
#define	DEBUG
#define TEST 

#ifdef TEST
#define NVST(offset,byte) (sysNvRamByteWrite (offset, byte))
#endif

#ifdef  TEST
char  count=0;
#endif 

/* memory mapped register access macros */
#define REG(reg,pchan) \
 (*(volatile UINT8 *)((UINT32)pchan->regs + (reg*pchan->regDelta)))
#define REGPTR(reg,pchan) \
 ((volatile UINT8 *)((UINT32)pchan->regs + (reg*pchan->regDelta)))
#endif

/* function prototypes */

static int ns16550CallbackInstall (SIO_CHAN *,int,STATUS (*)(),void *);
static STATUS ns16550DummyCallback ();
static void ns16550InitChannel (NS16550_CHAN *);
static STATUS ns16550Ioctl (NS16550_CHAN *,int,int);
static void ns16550TxStartup (NS16550_CHAN *);
static int ns16550PollOutput (NS16550_CHAN *,char);
static int ns16550PollInput (NS16550_CHAN *,char *);
LOCAL STATUS ns16550OptsSet (NS16550_CHAN *, UINT);
LOCAL STATUS ns16550Open ( NS16550_CHAN * pChan );
LOCAL STATUS ns16550Hup ( NS16550_CHAN * pChan );

/* driver functions */

static SIO_DRV_FUNCS ns16550SioDrvFuncs =
    {
    (int (*)())ns16550Ioctl,
    (int (*)())ns16550TxStartup,
    ns16550CallbackInstall,
    (int (*)())ns16550PollInput,
    (int (*)(SIO_CHAN *,char))ns16550PollOutput
    };

/******************************************************************************
*
* ns16550DummyCallback - dummy callback routine.
*/

static STATUS ns16550DummyCallback (void)
    {
    return (ERROR);
    }

/******************************************************************************
*
* ns16550DevInit - intialize an NS16550 channel
*
* This routine initializes some SIO_CHAN function pointers and then resets
* the chip in a quiescent state.  Before this routine is called, the BSP
* must already have initialized all the device addresses, etc. in the
* NS16550_CHAN structure.
*
* RETURNS: N/A
*/

void ns16550DevInit
    (
    NS16550_CHAN * pChan
    )
    {
    int oldlevel = intLock();

    /* initialize the driver function pointers in the SIO_CHAN's */

    pChan->pDrvFuncs    = &ns16550SioDrvFuncs;

    /* set the non BSP-specific constants */

    pChan->getTxChar    = ns16550DummyCallback;
    pChan->putRcvChar   = ns16550DummyCallback;
    pChan->channelMode  = 0;    /* undefined */
	 pChan->options      = (CLOCAL|CREAD|CS8); 
/*  pChan->options      = (CREAD|CS8);*/

    /* reset the chip */

    ns16550InitChannel(pChan);

    intUnlock(oldlevel);
    }

/*******************************************************************************
*
* ns16550InitChannel - initialize UART
*
* Initialize the number of data bits, parity and set the selected
* baud rate.
* Set the hardware flow signals if the option is selected.
*
* RETURNS: N/A
*/

static void ns16550InitChannel
    (
    NS16550_CHAN *pChan
    )
    {
    /* Configure Port -  Set 8 bits, 1 stop bit, no parity. */
    pChan->lcr = (UINT8)(CHAR_LEN_8 | ONE_STOP | PARITY_NONE);
	 (*pChan->outByte) (pChan->lcr_add, pChan->lcr);

    /* Clear the FIFOs / Enable the transmit and receive FIFOs */
    pChan->fcr = RxCLEAR | TxCLEAR | FIFO_ENABLE;
    (*pChan->outByte) (pChan->fcr_add, pChan->fcr);


    /* Now Enable the FIFO DMA and Triger level */
    pChan->fcr = FIFO_ENABLE | FCR_DMA | FCR_RXTRIG_H;
    (*pChan->outByte) (pChan->fcr_add, pChan->fcr);
 
    /* Enable access to the divisor latches by setting DLAB in LCR. */
    (*pChan->outByte) (pChan->lcr_add, LCR_DLAB | pChan->lcr);


   /* the baud rate as not been loaded into pChan yet, so it will be
    * defaulted here to 9600 baud
    */
    /* Set divisor latches. Default to 9600 Baud for now*/
    pChan->dll = pChan->xtal/(16*9600);
    pChan->dlm = (pChan->xtal/(16*9600)) >> 8;
    (*pChan->outByte) (pChan->dll_add, pChan->dll);
    (*pChan->outByte) (pChan->dlm_add, pChan->dlm);

    /* Restore line control register */
    (*pChan->outByte) (pChan->lcr_add, pChan->lcr);

    /* Make a copy since register is not readable */
    pChan->ier = (UINT8)(RxFIFO_BIT | TxFIFO_BIT); 

    /* Disable interrupts */
   (*pChan->outByte) (pChan->ier_add, 0);

    if (pChan->options & CLOCAL) 
		pChan->mcr = (MCR_DTR|MCR_RTS|MCR_OUT2);
    else {

		/* !clocal enables hardware flow control(DTR/DSR) interrupts */
		pChan->mcr = (MCR_DTR|MCR_RTS|MCR_OUT2);
    	pChan->ier &= (~TxFIFO_BIT); 
		pChan->ier |= IER_EMSI;             /* enable modem status interrupt */
	 }
	 (*pChan->outByte) (pChan->mcr_add, pChan->mcr);

#if 0
    (*pChan->outByte) (pChan->ier_add, pChan->ier);
#endif

}

/*******************************************************************************
*
* ns16550OptsSet - set the serial options
*
* Set the channel operating mode to that specified.  All sioLib options
* are supported: CLOCAL, HUPCL, CREAD, CSIZE, PARENB, and PARODD.
* When the HUPCL option is enabled, a connection is closed on the last
* close() call and opened on each open() call.
*
* Note, this routine disables the transmitter.  The calling routine
* may have to re-enable it.
*
* RETURNS:
* Returns OK to indicate success, otherwise ERROR is returned
*/

LOCAL STATUS ns16550OptsSet
    (
    NS16550_CHAN * pChan, /* ptr to channel */
    UINT options	/* new hardware options */
    )
    {
    FAST int     oldlevel;		/* current interrupt level mask */

    if (pChan == NULL || options & 0xfffff000)
	return ERROR;

    switch (options & CSIZE)
	{
	case CS5:
	    pChan->lcr = CHAR_LEN_5; break;
	case CS6:
	    pChan->lcr = CHAR_LEN_6; break;
	case CS7:
	    pChan->lcr = CHAR_LEN_7; break;
	default:
	case CS8:
	    pChan->lcr = CHAR_LEN_8; break;
	}

    if (options & STOPB)
		pChan->lcr |= LCR_STB;
    else
		pChan->lcr |= ONE_STOP;
    
    switch (options & (PARENB|PARODD))
		{
		case PARENB|PARODD:
	    	pChan->lcr |= LCR_PEN; break;
		case PARENB:
	    	pChan->lcr |= (LCR_PEN|LCR_EPS); break;
		default:
		case 0:
	    	pChan->lcr |= PARITY_NONE; break;
		}

	 (*pChan->outByte) (pChan->ier_add, 0);

	/*
	 * the modem bits are always enabled regardles of the state of
	 * hardware flow control.  However, if hardware flown control is
	 * enabled, then the modem status register interrupt will be enabled. 
	 */
	 pChan->mcr |= (MCR_DTR|MCR_RTS|MCR_OUT2);
    pChan->ier &= (~TxFIFO_BIT); 

    if (!(options & CLOCAL))
		{
		/* !clocal enables hardware flow control(DTR/DSR) */
		pChan->ier |= IER_EMSI;    /* enable modem status interrupt */
	 }
    else
		pChan->ier &= ~IER_EMSI; /* disable modem status interrupt */ 

    oldlevel = intLock ();

    (*pChan->outByte) (pChan->lcr_add, pChan->lcr);
    (*pChan->outByte) (pChan->mcr_add, pChan->mcr);


    /* now reset the channel mode registers */
    pChan->fcr = (FCR_RXTRIG_H|RxCLEAR|TxCLEAR|FIFO_ENABLE);
    (*pChan->outByte) (pChan->fcr_add, pChan->fcr);

    if (options & CREAD)  
		pChan->ier |= RxFIFO_BIT;

    if (pChan->channelMode == SIO_MODE_INT)
		{
      (*pChan->outByte) (pChan->ier_add, pChan->ier);
	 }

    intUnlock (oldlevel);

    pChan->options = options;

    return OK;
    }

/*******************************************************************************
*
* ns16550Hup - hang up the modem control lines 
*
* Disable the modem control line to close the connection.
*
* RETURNS:
* Returns OK to indicate success, otherwise ERROR is returned
*/

LOCAL STATUS ns16550Hup
    (
    NS16550_CHAN * pChan 	/* ptr to channel */
    )
    {
    FAST int     oldlevel;		/* current interrupt level mask */

    oldlevel = intLock ();

	 (*pChan->outByte) (pChan->ier_add, 0);	/* disable interrupts */

    pChan->ier &=(~IER_EMSI); /* disable modem status interrupt */ 
    pChan->mcr &= ~(MCR_DTR|MCR_RTS); 
    (*pChan->outByte) (pChan->mcr_add, pChan->mcr);
    pChan->fcr |= (RxCLEAR|TxCLEAR|FIFO_ENABLE);
    (*pChan->outByte) (pChan->fcr_add, pChan->fcr);    

    if (pChan->channelMode == SIO_MODE_INT)
    	(*pChan->outByte) (pChan->ier_add, pChan->ier);

    intUnlock (oldlevel);

    return (OK);

    }    

/*******************************************************************************
*
* ns16550Open - Enable the hardware control flow 
*
* Enable the hardware control flow.
*
* RETURNS:
* Returns OK to indicate success, otherwise ERROR is returned
*/

LOCAL STATUS ns16550Open
    (
    NS16550_CHAN * pChan 	/* ptr to channel */
    )
    {
    FAST int     oldlevel;		/* current interrupt level mask */
    char mask;

	 mask = ((*pChan->inByte) (pChan->msr_add) & (MSR_CTS | MSR_DSR));

    if (mask & MSR_CTS ) 
    	{ 
    	oldlevel = intLock ();

    	pChan->ier |= IER_EMSI; 	/* enable modem status interrupt */ 
    	pChan->ier &= (~TxFIFO_BIT); 
    	pChan->mcr |= (MCR_DTR|MCR_RTS|MCR_OUT2); 

    	(*pChan->outByte) (pChan->mcr_add, pChan->mcr);

    	pChan->fcr |= (RxCLEAR|TxCLEAR|FIFO_ENABLE);
      (*pChan->outByte) (pChan->fcr_add, pChan->fcr);    


    	if (pChan->channelMode == SIO_MODE_INT)
    		(*pChan->outByte) (pChan->ier_add, pChan->ier);

    	intUnlock (oldlevel);
	}

    return (OK);
    }

/*******************************************************************************
*
* ns16550Ioctl - special device control
*
* Includes commands to get/set baud rate, mode(INT,POLL), hardware options(
* parity, number of data bits), and modem control(RTS/CTS and DTR/DSR).
* The ioctl command SIO_HUP is sent by ttyDrv when the last close() function 
* call is made. Likewise SIO_OPEN is sent when the first open() function call
* is made.
*
* RETURNS: OK on success, EIO on device error, ENOSYS on unsupported
*          request.
*/

static STATUS ns16550Ioctl
    (
    NS16550_CHAN *pChan,		/* device to control */
    int        request,		/* request code */
    int        arg		/* some argument */
    )
    {
    FAST int     oldlevel;		/* current interrupt level mask */
    FAST STATUS  status;

    status = OK;

	switch (request)
		{
		case SIO_BAUD_SET:

			if (arg < 50 || arg > 115200)
				{
				status = EIO;		/* baud rate out of range */
				break;
			}

			/* disable interrupts during chip access */
	
			oldlevel = intLock ();

			/* Enable access to the divisor latches by setting DLAB in LCR. */
			(*pChan->outByte) (pChan->lcr_add, LCR_DLAB | pChan->lcr);

			/* Set divisor latches. */
			pChan->dll = pChan->xtal/(16*arg);
			pChan->dlm = (pChan->xtal/(16*arg)) >> 8;
			(*pChan->outByte) (pChan->dll_add, pChan->dll);
			(*pChan->outByte) (pChan->dlm_add, pChan->dlm);
	
			/* Restore line control register */
			(*pChan->outByte) (pChan->lcr_add, pChan->lcr);

			pChan->baudRate = arg;
 
			intUnlock (oldlevel);

			break;

		case SIO_BAUD_GET:
			*(int *)arg = pChan->baudRate;
			break; 

		case SIO_MODE_SET:
			if ((arg != SIO_MODE_POLL) && (arg != SIO_MODE_INT))
				{
				status =  EIO;
				break;
			}
           
			oldlevel = intLock ();

			if (arg == SIO_MODE_INT)
				{
				/* Enable appropriate interrupts */
		
				if (pChan->options & CLOCAL) 
					pChan->ier |= (RxFIFO_BIT | TxFIFO_BIT);
				else  
					{
					(*pChan->outByte) (pChan->mcr_add, pChan->mcr);
					pChan->ier &= (~TxFIFO_BIT); 
					pChan->ier |= (RxFIFO_BIT | IER_EMSI);  
				}	
				(*pChan->outByte) (pChan->ier_add, pChan->ier);

			}
			else
				{
				/* Disable the interrupts */ 

				pChan->ier = 0;   
				(*pChan->outByte) (pChan->ier_add, pChan->ier);   
			}

			pChan->channelMode = arg;

			intUnlock(oldlevel);
			break;          

		case SIO_MODE_GET:
			*(int *)arg = pChan->channelMode;
			break;

		case SIO_AVAIL_MODES_GET:
			*(int *)arg = SIO_MODE_INT | SIO_MODE_POLL;
			break;

		case SIO_HW_OPTS_SET:
			status = ns16550OptsSet(pChan, arg);
			break;

		case SIO_HW_OPTS_GET:
			*(int *)arg = pChan->options;
			break;

		case SIO_HUP:
			status = ns16550Hup(pChan);
			break;
	
		case SIO_OPEN:
			if (pChan->options & HUPCL) 
			status = ns16550Open(pChan);
			break;

		case SIO_RTS_SUSPEND:
			pChan->mcr &= ~MCR_RTS;
			(*pChan->outByte) (pChan->mcr_add, pChan->mcr);
			break;

		case SIO_RTS_RESUME:
			pChan->mcr |= MCR_RTS;
			(*pChan->outByte) (pChan->mcr_add, pChan->mcr);
			break;

		default:
			status = ENOSYS;
		}
	return (status);
}

/*******************************************************************************
*
* ns16550IntWr - handle a transmitter interrupt 
*
* This routine handles write interrupts from the UART.
*
* RETURNS: N/A
*
*/

void ns16550IntWr 
    (
    NS16550_CHAN *pChan
    )
    {
    char            outChar;

    if ((*pChan->getTxChar) (pChan->getTxArg, &outChar) != ERROR)
      /* write char. to Transmit Holding Reg. */
      (*pChan->outByte) (pChan->data_add, outChar);
    else
        {
        pChan->ier &= (~TxFIFO_BIT); /* indicates to disable Tx Int */
        (*pChan->outByte) (pChan->ier_add, pChan->ier);
        }
    }

/*****************************************************************************
*
* ns16550IntRd - handle a receiver interrupt 
*
* This routine handles read interrupts from the UART.
*
* RETURNS: N/A
*
*/

void ns16550IntRd 
    (
    NS16550_CHAN *pChan
    )
    {
    char            inchar;

    /* read character from Receive Holding Reg. */
    inchar = (*pChan->inByte) (pChan->data_add);

    (*pChan->putRcvChar) (pChan->putRcvArg, inchar);
    }

/**********************************************************************
*
* ns16550IntEx - miscellaneous interrupt processing
*
* This routine handles miscellaneous interrupts on the UART.
*
* RETURNS: N/A
*
*/

void ns16550IntEx 
    (
    NS16550_CHAN *pChan
    )
    {

    /* Nothing for now... */
    }

/********************************************************************************
* ns16550Int - interrupt level processing
*
* This routine handles interrupts from the UART.
*
* RETURNS: N/A
*
*/

void ns16550Int 
    (
    NS16550_CHAN *pChan
    )
    {
    FAST volatile char        intStatus;

    /* read the Interrrupt Status Register (Int. Ident.) */

    intStatus = ((*pChan->inByte) (pChan->isr_add)) & 0x0f;

    /*
     * This UART chip always produces level active interrupts, and the IIR 
     * only indicates the highest priority interrupt.  
     * In the case that receive and transmit interrupts happened at
     * the same time, we must clear both interrupt pending to prevent
     * edge-triggered interrupt(output from interrupt controller) from locking
     * up. One way doing it is to disable all the interrupts at the beginning
     * of the ISR and enable at the end.
     */

	(*pChan->outByte) (pChan->ier_add, 0);    /* disable interrupt */

	switch (intStatus)
		{
		case IIR_RLS:
			/* overrun,parity error and break interrupt */

			/* read LSR to reset interrupt */
			intStatus = ((*pChan->inByte) (pChan->lsr_add)) & 0x0f; 
			break;

		case IIR_RDA:     /* received data available */
		case IIR_TIMEOUT: /* receiver FIFO interrupt. In some case, IIR_RDA will
									not be indicated in IIR register when there is more
									than one char. in FIFO. */

			ns16550IntRd (pChan);  /* RxChar Avail */
			break;

		case IIR_THRE:  /* transmitter holding register ready */
			{
			char outChar;

			if ((*pChan->getTxChar) (pChan->getTxArg, &outChar) != ERROR)
            /*  char. to Transmit Holding Reg. */
            (*pChan->outByte) (pChan->data_add, outChar);
			else
				pChan->ier &= (~TxFIFO_BIT); /* indicates to disable Tx Int */

			}
			break;

		case IIR_MSTAT: /* modem status register */
			{
			char	msr;

			msr = (*pChan->inByte) (pChan->msr_add);

			/* if CTS enable tx interrupt */
			/* This interrupt should not be taken if hardware flow control
			 * is not enabled, therefore we only need to act upon the 
			 * status register signals
			 */

			/* Enable the Xmitter */
			if (msr & MSR_DCTS) {  
				if (msr & MSR_CTS)
					pChan->ier |= TxFIFO_BIT;
				else
					pChan->ier &= (~TxFIFO_BIT); 
			}
			}
			break;

		default:
			break;
	}

   /* enable interrupts accordingly */
   (*pChan->outByte) (pChan->ier_add, pChan->ier);

}

/*******************************************************************************
*
* ns16550TxStartup - transmitter startup routine
*
* Call interrupt level character output routine and enable interrupt if it is
* in interrupt mode with no hardware flow control.
* If the option for hardware flow control is enabled and CTS and DSR are set
* TRUE then the Tx interrupt is enabled.
* 
* RETURNS: N/A
*/

static void ns16550TxStartup
	(
	NS16550_CHAN *pChan 
	)
{
	char msr;

	if (pChan->channelMode == SIO_MODE_INT)
		{
		if (pChan->options & CLOCAL)
			{
			/* No modem control */

			pChan->ier |= TxFIFO_BIT;
		}
		else
			{
			msr = (*pChan->inByte) (pChan->msr_add);

			if (msr & MSR_CTS)
				pChan->ier |= TxFIFO_BIT;
			else
				pChan->ier &= ~TxFIFO_BIT; 
		}
		(*pChan->outByte) (pChan->ier_add, pChan->ier);
	}
}

/******************************************************************************
*
* ns16550PollOutput - output a character in polled mode.
*
* RETURNS: OK if a character arrived, EIO on device error, EAGAIN
*          if the output buffer if full.
*/

static int ns16550PollOutput
    (
    NS16550_CHAN *  pChan,
    char            outChar
    )
    {
    char pollStatus = (*pChan->inByte) (pChan->lsr_add);

    /* is the transmitter ready to accept a character? */

    if ((pollStatus & LSR_THRE) == 0x00)
        return (EAGAIN);

    /* write out the character */

    (*pChan->outByte) (pChan->data_add, outChar);       /* transmit character */

    return (OK);
    }
/******************************************************************************
*
* ns16550PollInput - poll the device for input.
*
* RETURNS: OK if a character arrived, EIO on device error, EAGAIN
*          if the input buffer if empty.
*/

static int ns16550PollInput
    (
    NS16550_CHAN *  pChan,
    char *          thisChar
    )
    {

    char pollStatus = (*pChan->inByte) (pChan->lsr_add);

    if ((pollStatus & LSR_DR) == 0x00)
        return (EAGAIN);

    /* got a character */

    *thisChar = (*pChan->inByte) (pChan->data_add);

    return (OK);
    }

/******************************************************************************
*
* ns16550CallbackInstall - install ISR callbacks to get/put chars.
*/

static int ns16550CallbackInstall
    (
    SIO_CHAN *  pSioChan,
    int         callbackType,
    STATUS      (*callback)(),
    void *      callbackArg
    )
    {
    NS16550_CHAN * pChan = (NS16550_CHAN *)pSioChan;

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

