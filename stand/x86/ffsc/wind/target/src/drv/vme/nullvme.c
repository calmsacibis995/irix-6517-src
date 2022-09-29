/* nullVme.c - NULL VMEbus library */

/* Copyright 1984-1993 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01b,15sep93,hdn  added a title of sysMailboxConnect () for documentation.
01b,18aug93,vin	 fixed title to sysMailboxConnect which causes error
		 when generating man pages.
01a,20aug92,ccc  created by copying routines from mb930 sysLib.c.
*/

/*
DESCRIPTION
This library contains NULL routines for boards which do not include any
common bus routines.

The functions addressed here include:

    Bus interrupt functions:
	- enable/disable VMEbus interrupt levels
	- generate bus interrupts

    Mailbox/locations monitor functions:
	- enable mailbox/location monitor interrupts

*/

/******************************************************************************
*
* sysLocalToBusAdrs - convert a local address to a bus address
*
* This routine gets the AT bus address that accesses a specified local
* memory address.
*
* RETURNS: ERROR, since there is no mapping to the AT bus.
*
* SEE ALSO: sysBusToLocalAdrs()
*/

STATUS sysLocalToBusAdrs
    (
    int  adrsSpace,     /* bus address space in which busAdrs resides,  */
                        /* use address modifier codes defined in vme.h, */
                        /* such as VME_AM_STD_SUP_DATA                  */
    char *localAdrs,    /* local address to convert                     */
    char **pBusAdrs     /* where to return bus address                  */
    )
    {
    return (ERROR);
    }

/******************************************************************************
*
* sysBusToLocalAdrs - convert a bus address to a local address
*
* This routine gets the local address that accesses a specified AT bus
* memory address.
*
* RETURNS: ERROR, since there is no mapping to the AT bus.
*
* SEE ALSO: sysLocalToBusAdrs()
*/
 
STATUS sysBusToLocalAdrs
    (
    int  adrsSpace,     /* bus address space in which busAdrs resides,  */
                        /* use address modifier codes defined in vme.h, */
                        /* such as VME_AM_STD_SUP_DATA                  */
    char *busAdrs,      /* bus address to convert                       */
    char **pLocalAdrs   /* where to return local address                */
    )
    {
    return (ERROR);
    }

/******************************************************************************
*
* sysBusIntAck - acknowledge a bus interrupt
*
* This routine acknowledges a specified AT bus interrupt.
*
* NOTE: This routine has no effect, since the SPARClite does not accept
* interrupts from the AT bus.
*
* RETURNS: NULL
*/

int sysBusIntAck
    (
    int intLevel        /* interrupt level to acknowledge */
    )
    {
    return (NULL);
    }

/******************************************************************************
*
* sysBusIntGen - generate a bus interrupt
*
* This routine generates an AT bus interrupt for a specified level with a
* specified vector.
*
* NOTE
* This routine has no effect, since the CPU board cannot generate an AT bus
* interrupt.
*
* RETURNS: ERROR, since the board cannot generate an AT bus interrupt.
*/

STATUS sysBusIntGen
    (
    int level,          /* bus interrupt level to generate          */
    int vector          /* interrupt vector to return (0-255)       */
    )
    {
    return (ERROR);
    }

/******************************************************************************
*
* sysMailboxConnect - connect a routine to the mailbox interrupt
*
* This routine specifies the interrupt service routine to be called at each
* mailbox interrupt.
*
* NOTE: This routine has no effect, since the hardware does not support mailbox
* interrupts.
*
* RETURNS: ERROR, since there is no mailbox facility.
*
* SEE ALSO: sysMailboxEnable()
*/

STATUS sysMailboxConnect
    (
    FUNCPTR routine,    /* routine called at each mailbox interrupt */
    int     arg         /* argument with which to call routine      */
    )
    {
    return (ERROR);
    }

/******************************************************************************
*
* sysMailboxEnable - enable the mailbox interrupt
*
* This routine enables the mailbox interrupt.
*
* NOTE: This routine has no effect, since the hardware does not support mailbox
* interrupts.
*
* RETURNS: ERROR, since there is no mailbox facility.
*
* SEE ALSO: sysMailboxConnect()
*/

STATUS sysMailboxEnable
    (
    INT8 *mailboxAdrs           /* mailbox address */
    )
    {
    return (ERROR);
    }

/********************************************************************************
* sysBusTas - test and set a location without AT bus interference
*
* This routine does a test-and-set (TAS) instruction.
*
* NOTE
* This routine is similar to vxTas().
*
* RETURNS:
* TRUE if the previous value had been zero, or FALSE if the value was
* set already.
*
* SEE ALSO: vxTas()
*/
 
BOOL sysBusTas
    (
    INT8 *addr          /* address to be tested and set */
    )
    {
    return(vxTas(addr));       /* RMW cycle */
    }

