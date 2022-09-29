/* i8259Pic.c - Intel 8259A PIC (Programmable Interrupt Controller) driver */

/* Copyright 1984-1993 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01d,14jun95,hdn  renamed sysEndOfInt to sysIntEOI.
		 moved global function prototypes to sysLib.h.
01c,08aug94,hdn  stopped toggeling IRQ9 in enabling and disabling.
01b,22apr94,hdn  made IRQ9 off in the initialization.
		 moved sysVectorIRQ0 to sysLib.c.
01a,05sep93,hdn  written.
*/

/*
DESCRIPTION
This module implements the Intel 8259A PIC driver.
*/

#include "drv/intrctl/i8259a.h"


/* globals */

IMPORT UINT    sysVectorIRQ0;           /* vector for IRQ0 */

/* forward static functions */

LOCAL void sysIntInitPIC (void);
LOCAL void sysIntEOI   (void);


/*******************************************************************************
*
* sysIntInitPIC - initialize the PIC
*
* This routine initializes the PIC.
*
*/

LOCAL void sysIntInitPIC (void)

    {

    /* initialize the PIC (Programmable Interrupt Controller) */

    sysOutByte (PIC_port1 (PIC1_BASE_ADR),0x11);        /* ICW1 */
    sysOutByte (PIC_port2 (PIC1_BASE_ADR),sysVectorIRQ0); /* ICW2 */
    sysOutByte (PIC_port2 (PIC1_BASE_ADR),0x04);        /* ICW3 */
    sysOutByte (PIC_port2 (PIC1_BASE_ADR),0x01);        /* ICW4 */

    sysOutByte (PIC_port1 (PIC2_BASE_ADR),0x11);        /* ICW1 */
    sysOutByte (PIC_port2 (PIC2_BASE_ADR),sysVectorIRQ0+8); /* ICW2 */
    sysOutByte (PIC_port2 (PIC2_BASE_ADR),0x02);        /* ICW3 */
    sysOutByte (PIC_port2 (PIC2_BASE_ADR),0x01);        /* ICW4 */

    /* disable interrupts */

    sysOutByte (PIC_IMASK (PIC1_BASE_ADR),0xfb);
    sysOutByte (PIC_IMASK (PIC2_BASE_ADR),0xff);
    }

/*******************************************************************************
*
* sysIntEOI - send EOI(end of interrupt) signal.
*
* This routine is called at the end of the interrupt handler.
*
*/

LOCAL void sysIntEOI (void)

    {
    sysOutByte (PIC_IACK (PIC1_BASE_ADR), 0x20);
    sysOutByte (PIC_IACK (PIC2_BASE_ADR), 0x20);
    }

/*******************************************************************************
*
* sysIntDisablePIC - disable a PIC interrupt level
*
* This routine disables a specified PIC interrupt level.
*
* RETURNS: OK.
*
* ARGSUSED0
*/

STATUS sysIntDisablePIC
    (
    int intLevel        /* interrupt level to disable */
    )
    {

    if (intLevel < 8)
	{
	sysOutByte (PIC_IMASK (PIC1_BASE_ADR),
	    sysInByte (PIC_IMASK (PIC1_BASE_ADR)) | (1 << intLevel));
	}
    else
	{
	sysOutByte (PIC_IMASK (PIC2_BASE_ADR),
	    sysInByte (PIC_IMASK (PIC2_BASE_ADR)) | (1 << (intLevel - 8)));
	}

    return (OK);
    }

/*******************************************************************************
*
* sysIntEnablePIC - enable a PIC interrupt level
*
* This routine enables a specified PIC interrupt level.
*
* RETURNS: OK.
*
* ARGSUSED0
*/

STATUS sysIntEnablePIC
    (
    int intLevel        /* interrupt level to enable */
    )
    {

    if (intLevel < 8)
	{
	sysOutByte (PIC_IMASK (PIC1_BASE_ADR),
	    sysInByte (PIC_IMASK (PIC1_BASE_ADR)) & ~(1 << intLevel));
	}
    else
	{
	sysOutByte (PIC_IMASK (PIC2_BASE_ADR),
	    sysInByte (PIC_IMASK (PIC2_BASE_ADR)) & ~(1 << (intLevel - 8)));
	}

    return (OK);
    }

