/* if_eex32.c - Intel EtherExpress Flash 32 routines for if_ei.c */

/* Copyright 1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01d,14jun95,hdn  cleaned up.
01c,24oct94,hdn  cleaned up and included in 5.2.
01b,28jun94,vin  cleaned up and included in BSP.
01a,12may94,bcs  written
*/


/*
DESCRIPTION

GENERAL INFORMATION
This module contains routines to make the Intel 82596 Ethernet network
interface driver work with the Intel EtherExpress Flash 32 EISA board.
This release supports 4 units, assumed to be the same number if_ei.c
supports.  A mismatch of number of units could be a large problem!
The connector type should should be configured to right one using the EISA
configuration utility which comes along with the card. The ether express
card supports AUI (thick ethernet), BNC (thin ethernet) and RJ-45 (twisted
pair ethernet). Automatic detection of connector type is not implemented 
in the driver even though one choses the autodetect option. So the user 
should choose the right connector being used.

BOARD LAYOUT
This device is soft-configured.  No jumpering diagram required.

EXTERNAL INTERFACE
This module provides system-specific support functions.

SEE ALSO: if_ei.c
*/

/* includes */

#include "if_eex32.h"

/* defines */

#undef		EEX32_DEBUG		/* Compiles debug output */
#define 	MAX_UNITS	4	/* maximum units to support */

/* LAN Board Types */

#define		TYPE_UNKNOWN	0	/* initial value */
#define		TYPE_EEX32	1	/* Intel EtherExpress Flash 32 */

/* typedefs */

typedef struct i596Info			/* extra information struct */
    {
    UINT	port;			/* I/O port base address */
    UINT	type;			/* Type of LAN board this unit is */
    int		ilevel;			/* Interrupt level of this unit */
    } I596_INFO;


/* globals */

STATUS 	sysEnetAddrGet	(int unit, char addr[]);
STATUS 	sys596Init	(int unit);
STATUS 	sys596IntAck	(int unit);
STATUS 	sys596IntEnable (int unit);
void   	sys596IntDisable (int unit);
void   	sys596Port	(int unit, int cmd, UINT32 addr);
void   	sys596ChanAtn	(int unit);

/* locals */

LOCAL I596_INFO i596Info [MAX_UNITS];

LOCAL unsigned char eex32IdString [] =	/* Fixed-length, no NUL terminator! */
    {
    EEX32_EISA_ID0,
    EEX32_EISA_ID1,
    EEX32_EISA_ID2,
    EEX32_EISA_ID3
    };

LOCAL unsigned char eex32IntLevel [8] = /* first 4 are PLX, rest are FLEA */
    {
    5, 9, 10, 11, 3, 7, 12, 15
    };

/* forward function declarations */

LOCAL STATUS sysInitNecessary (int unit);
LOCAL STATUS sysFindEisaBoard (I596_INFO *pI596Info, UCHAR *idString);

/*******************************************************************************
*
* sysEnetAddrGet - retrieve net unit's Ethernet address
*
* The driver expects this routine to provide the six byte Ethernet address
* that will be used by this unit.  This routine must copy the six byte
* address to the space provided by pCopy.  This routine is expected to
* return OK on success, or ERROR.
*
* The driver calls this routine, once per unit, from the eiattach() routine.
* In if_ei.c version 03c,15jan93, this routine is the first 82596 support
* routine called.  In the PC environment, it is necessary for this routine
* then to find the hardware corresponding to the given unit number, before
* it can return the Ethernet address!  This is done by the function
* sysInitNecessary() (q.v.).
*
* Here we blithely assume that sysInitNecessary() finds a unit for us,
* as if no one would have attched the unit if it didn't exist!
*
* RETURNS: OK or ERROR.
*
* SEE ALSO:
*/

STATUS sysEnetAddrGet
    (
    int unit,
    char addr[]
    )
    {
    I596_INFO *pI596Info = &i596Info[unit];
    UINT charIndex;

    /* Find a physical board to go with this unit and set up the
     * I596_INFO structure to refer to that board.
     */

    if (sysInitNecessary (unit) != OK)
	return (ERROR);

    for (charIndex = 0; charIndex < 6; charIndex++)
	addr[charIndex] = sysInByte (pI596Info->port + NET_IA0 + charIndex);

    return (OK);
    }

/*******************************************************************************
*
* sys596Init - prepare LAN board for 82596 initialization
*
* This routine is expected to perform any target specific initialization
* that must be done prior to initializing the 82596.  This routine would
* typically be empty.  This routine is expected to return OK on success,
* or ERROR.
*
* The driver calls this routine, once per unit, from the eiattach() routine,
* immediately before starting up the 82596.  We here call sysInitNecessary()
* again in case eiattach() hasn't called sysInetAddrGet() yet.
*
* RETURNS: OK or ERROR.
*/

STATUS sys596Init
    (
    int unit
    )
    {
    I596_INFO *pI596Info = &i596Info[unit];
    UCHAR intIndex;

    if (sysInitNecessary (unit) != OK)
	return (ERROR);

    if ( (sysInByte (pI596Info->port + IRQCTL) & IRQ_EXTEND) == 0)
        {
        intIndex = (sysInByte (pI596Info->port + PLX_CONF0) &
                    (IRQ_SEL0 | IRQ_SEL1)) >> 1;
        }
    else
        {
        intIndex = ( (sysInByte (pI596Info->port + IRQCTL) &
                      (IRQ_SEL0 | IRQ_SEL1)) >> 1) + 4;
        }

    pI596Info->ilevel = eex32IntLevel[intIndex];

    sysIntEnablePIC (pI596Info->ilevel);

    return (OK);
    }

/*******************************************************************************
*
* sys596IntAck - acknowledge current interrupt from 82596
*
* This routine performs any non-82596 interrupt acknowledge that may be
* required.  This typically involves an operation to some interrupt
* control hardware.  Take special note that the INT signal from the 82596
* behaves in an "edge-triggered" mode.  Therefore, this routine would
* typically clear a latch within the control circuitry.
*
* The driver calls this routine from the interrupt handler.
*/

STATUS sys596IntAck
    (
    int unit
    )
    {
    I596_INFO *pI596Info = &i596Info[unit];

    if (pI596Info->type == TYPE_UNKNOWN)
	return (ERROR);

    sysOutByte (pI596Info->port + IRQCTL,
                sysInByte (pI596Info->port + IRQCTL) | IRQ_LATCH);
    return (OK);
    }

/*******************************************************************************
*
* sys596IntEnable - enable interrupt from 82596
*
* Manipulate board hardware to allow interrupt.
*
* The driver calls this routine throughout normal operation to terminate
* critical sections of code.
*
* SEE ALSO: sys596IntDisable
*/

STATUS sys596IntEnable
    (
    int unit
    )
    {
    I596_INFO *pI596Info = &i596Info[unit];

    if (pI596Info->type == TYPE_UNKNOWN)
	return (ERROR);

    sysOutByte (pI596Info->port + IRQCTL,
		sysInByte (pI596Info->port + IRQCTL) & ~IRQ_FORCE_LOW);

    return (OK);
    }

/*******************************************************************************
*
* sys596IntDisable - disable interrupt from 82596
*
* Manipulate board hardware to prevent interrupt.
*
* The driver calls this routine throughout normal operation to protect
* critical sections of code from interrupt handler intervention.
*/

void   sys596IntDisable
    (
    int unit
    )
    {
    I596_INFO *pI596Info = &i596Info[unit];

    if (pI596Info->type == TYPE_UNKNOWN)
	return;

    sysOutByte (pI596Info->port + IRQCTL,
		sysInByte (pI596Info->port + IRQCTL) | IRQ_FORCE_LOW);
    }

/*******************************************************************************
*
* sys596Port - issue "PORT" command to 82596
*
* This routine provides access to the special port function of the 82596.
* The driver expects this routine to deliver the command and address
* arguments to the port of the specified unit.
*
* The driver calls this routine primarily during initialization, but may
* also call it during error recovery procedures.  Because it is called
* during initialization, this is yet another routine that may be called
* before the board for this unit has been found, depending on how
* eiattach() might be modified in the future.
*/

void   sys596Port
    (
    int unit,
    int cmd,
    UINT32 addr
    )
    {
    I596_INFO *pI596Info = &i596Info[unit];

    if (sysInitNecessary (unit) != OK)	/* Just in case not called yet! */
	return;

    /* PORT command wants to see 16 bits at a time, low-order first */

    sysOutWord (pI596Info->port + PORT, (cmd + addr) & 0xffff);
    sysOutWord (pI596Info->port + PORT + 2, ((cmd + addr) >> 16) & 0xffff);
    }

/*******************************************************************************
*
* sys596ChanAtn - assert Channel Attention signal to 82596
*
* This routine provides the channel attention signal to the 82596, for the
* specified unit.
*
* The driver calls this routine frequently throughout all phases of
* operation.
*/

void   sys596ChanAtn
    (
    int unit
    )
    {
    I596_INFO *pI596Info = &i596Info[unit];

    if (pI596Info->type == TYPE_UNKNOWN)
	return;

    sysOutByte (pI596Info->port + CA, 0);
    }

/*******************************************************************************
*
* sysInitNecessary - if unit is undefined, locate and set up the next board
*
*/

LOCAL STATUS sysInitNecessary
    (
    int unit
    )
    {
    I596_INFO *pI596Info = &i596Info[unit];

    if (pI596Info->type == TYPE_UNKNOWN)
	{
	if (sysFindEisaBoard (pI596Info, eex32IdString) == ERROR)
	    return (ERROR);

	pI596Info->type = TYPE_EEX32;
	}

    /* Set up next unit structure in case there's another board */

    if (unit < (MAX_UNITS - 1))
        i596Info[unit + 1].port = i596Info[unit].port;

    return (OK);
    }

/*******************************************************************************
*
* sysFindEisaBoard - scan selected EISA slots for a board with given ID bytes
*
*/

LOCAL STATUS sysFindEisaBoard
    (
    I596_INFO *pI596Info,		/* Start at I/O address in here */
    UCHAR *idString
    )
    {
    UINT byteNumber;
    BOOL failed;

    do
        {
	failed = FALSE;
	for (byteNumber = 0; (byteNumber < 4) && !failed; byteNumber++)
	    {
	    failed = (sysInByte (pI596Info->port + EISA_ID0 + byteNumber)
		     != idString[byteNumber]);
	    }
	if (!failed)
	    return (OK);

	pI596Info->port += 0x1000;
        }
    while (pI596Info->port != 0xf000);

    return (ERROR);
    }

/* END OF FILE */


