/* pcConsole.c - Compaq Deskpro 386 console handler */

/* Copyright 1993-1993 Wind River System, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01b,14jun95,hdn  removed function declarations defined in sysLib.h.
01a,09sep93,vin  created.
*/

/*
DESCRIPTION
This file is used to link the keyboard driver and the vga driver.


USER CALLABLE ROUTINES
Most of the routines in this driver are accessible only through the I/O
system.  Two routines, however, must be called directly: pcConDrv() to
initialize the driver, and pcConDevCreate() to create devices.

Before using the driver, it must be initialized by calling pcConDrv ()
This routine should be called exactly once, before any reads, writes, or
calls to pcConDevCreate().  Normally, it is called from usrRoot() in 
usrConfig.c.

Before a console can be used, it must be created using pcConDevCreate().

IOCTL FUNCTIONS
This driver responds to the same ioctl codes as a normal ty driver.

SEE ALSO: tyLib

NOTES
The macro N_VIRTUAL_CONSOLES should be defined in config.h file.

*/

/* includes */

#include "vxworks.h"
#include "iv.h"
#include "iolib.h"
#include "ioslib.h"
#include "memlib.h"
#include "tylib.h"
#include "intlib.h"
#include "errnolib.h"
#include "config.h"
#include "drv/serial/pcconsole.h"


/* globals */

PC_CON_DEV pcConDv [N_VIRTUAL_CONSOLES] ;	/* device descriptors */


/* locals */

LOCAL int	pcConDrvNum;	/* driver number assigned to this driver */


/* forward declarations */

LOCAL int	pcConDrvOpen ();
LOCAL STATUS	pcConDrvIoctl (PC_CON_DEV * pPcCoDv, int request, int arg);
LOCAL void	pcConDrvHrdInit ();


/******************************************************************************
*
* pcConDrv - console driver initialization routine
*
* This routine initializes the console driver, sets up interrupt vectors,
* and performs hardware initialization of the keybord and display.
*
* RETURNS: OK or ERROR if unable to install driver.
*/

STATUS pcConDrv (void)
    {

    /* check if driver already installed */

    if (pcConDrvNum > 0)
	return (OK);

    pcConDrvHrdInit ();

    pcConDrvNum = iosDrvInstall (pcConDrvOpen, (FUNCPTR) NULL, pcConDrvOpen,
				(FUNCPTR) NULL, tyRead, tyWrite, pcConDrvIoctl
				 );

    return (pcConDrvNum == ERROR ? ERROR : OK);
    }

/******************************************************************************
*
* pcConDevCreate - create a device for the onboard ports
*
* This routine creates a device on one of the pcConsole ports.  Each port
* to be used should have exactly one device associated with it, by calling
* this routine.
*
* RETURNS: OK or ERROR if no driver or already exists.
*/

STATUS pcConDevCreate 
    (
    char *	name,		/* Name to use for this device	*/
    FAST int	channel,        /* virtual console number	*/
    int		rdBufSize,	/* Read buffer size, in bytes	*/
    int		wrtBufSize	/* Write buffer size in bytes	*/
    )
    {
    FAST PC_CON_DEV *pPcCoDv;

    if (pcConDrvNum <= 0)
	{
	errnoSet (S_ioLib_NO_DRIVER);
	return (ERROR);
	}

    /* if this device already exists, don't create it */

    if (channel < 0 || channel >= N_VIRTUAL_CONSOLES)
        return (ERROR);

    pPcCoDv = &pcConDv [channel];

    if (pPcCoDv->created)
	return (ERROR);

    if (tyDevInit (&pPcCoDv->tyDev, rdBufSize, wrtBufSize, vgaWriteString)
	!= OK)
	{
	return (ERROR);
	}

    /* enable the keybord interrupt */
    
    sysIntEnablePIC (KBD_INT_LVL);

    /* mark the device as created, and add the device to the I/O system */

    pPcCoDv->created = TRUE;
    return (iosDevAdd (&pPcCoDv->tyDev.devHdr, name, pcConDrvNum));
    }

/******************************************************************************
*
* pcConDrvHrdInit - initialize the Keyboard and VGA
*/

LOCAL void pcConDrvHrdInit (void)
    {
    FAST int 	oldlevel;	/* to hold the oldlevel of interrupt */

    oldlevel= intLock ();

    /* Keyboard initialization */

    kbdHrdInit ();

    /* (VGA) Display initialization */

    vgaHrdInit ();

    /* interrupt is masked out: the keyboard interrupt will be enabled
     * in the pcConDevCreate 
     */

    intUnlock (oldlevel);

    } 

/*******************************************************************************
*
* pcConDrvOpen - open file to Console
*
*/

LOCAL int pcConDrvOpen 
    (
    PC_CON_DEV *	pPcCoDv,
    char *		name,
    int 		mode
    )
    {
    return ((int) pPcCoDv);
    }

/*******************************************************************************
*
* pcConDrvIoctl - special device control
*
* This routine handles FIOGETOPT requests and passes all others to tyIoctl.
*
* RETURNS: OK or ERROR if invalid baud rate, or whatever tyIoctl returns.
*/

LOCAL STATUS pcConDrvIoctl 
    (
    PC_CON_DEV *	pPcCoDv,	/* device to control */
    int 		request,	/* request code */
    int 		arg		/* some argument */
    )
    {
    int 	status = OK;

    switch (request)
	{
        case CONIOSETATRB:
            pPcCoDv->vs->curAttrib = arg ;
	    break;

        case CONIOGETATRB:
            status = pPcCoDv->vs->curAttrib;
	    break;

        case CONIOSETKBD:
            if (arg == 0 || arg == 1)
                pPcCoDv->ks->kbdMode = arg;
            else
                status = ERROR;
	    break;

        case CONIOSCREENREV:
	    pPcCoDv->vs->rev = (pPcCoDv->vs->rev) ? FALSE : TRUE;
	    pPcCoDv->vs->vgaHook (pPcCoDv->vs, arg, 0);	/* reverse screen */
            break;

        case CONIOBEEP:
	    pPcCoDv->vs->vgaHook (pPcCoDv->vs, arg, 1);	/* produce beep */
	    break;

        case CONIOCURSORON:
	    pPcCoDv->vs->vgaHook (pPcCoDv->vs, arg, 2); /* vgaCursor on */
	    break;

        case CONIOCURSOROFF:
	    pPcCoDv->vs->vgaHook (pPcCoDv->vs, arg, 3);	/* vgaCursor off */
	    break;

        case CONIOCURSORMOVE:
	    pPcCoDv->vs->vgaHook (pPcCoDv->vs, arg, 4); /* position cursor */
	    break;
	    
	case CONIOCURCONSOLE:		/* change current console */
	    if ((arg >= 0) && (arg < N_VIRTUAL_CONSOLES))
	       pPcCoDv->ks->currCon = arg;
	    break;
	default:
	    status = tyIoctl (&pPcCoDv->tyDev, request, arg);
	    break;
	}

    return (status);
    }

