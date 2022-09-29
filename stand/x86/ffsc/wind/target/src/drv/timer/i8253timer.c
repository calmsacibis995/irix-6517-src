/* i8253Timer.c - Intel 8253 timer library */

/* Copyright 1984-1993 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01i,25apr94,hdn  changed sysAuxClkTicksPerSecond from 60 to 64.
01h,09nov93,vin  added SYS_CLK_RATE_MIN, SYS_CLK_RATE_MAX, AUX_CLK_RATE_MIN,
                 AUX_CLK_RATE_MAX macros, moved intConnect of auxiliary clock
		 sysLib.c; made sysAuxClkRateSet return error if any value
		 other than the one in the auxTable is set. Added sysClkDisable
		 and sysClkEnable in sysClkRateSet.
01g,27oct93,hdn  deleted memAddToPool stuff from sysClkConnect().
01f,12oct93,hdn  added sysIntEnablePIC(), sysIntDisablePIC().
01e,16aug93,hdn  added aux clock functions.
01d,16jun93,hdn  updated to 5.1.
01c,08apr93,jdi  doc cleanup.
01d,07apr93,hdn  renamed compaq to pc.
01c,26mar93,hdn  added the global descriptor table, memAddToPool.
		 moved enabling A20 to romInit.s. added cacheClear for 486.
01b,18nov92,hdn  supported nested interrupt.
01a,15may92,hdn  written based on frc386 version.
*/

/*
DESCRIPTION
This library contains routines to manipulate the timer functions on the
Intel 8253 chip with a board-independent interface.  This library handles
both the system clock and the auxiliary clock functions.

The macros SYS_CLK_RATE_MIN, SYS_CLK_RATE_MAX, AUX_CLK_RATE_MIN, and
AUX_CLK_RATE_MAX must be defined to provide parameter checking for the
sys[Aux]ClkRateSet() routines.

The macro PIT_CLOCK must also be defined to indicate the clock frequency
of the i8253.

The system clock is the programable interrupt timer.
The auxiliary clock is the real time clock.
*/

/* locals */

LOCAL FUNCPTR sysClkRoutine	= NULL; /* routine to call on clock interrupt */
LOCAL int sysClkArg		= NULL; /* its argument */
LOCAL int sysClkRunning		= FALSE;
LOCAL int sysClkConnected	= FALSE;
LOCAL int sysClkTicksPerSecond	= 60;

LOCAL FUNCPTR sysAuxClkRoutine	= NULL;
LOCAL int sysAuxClkArg		= NULL;
LOCAL int sysAuxClkRunning	= FALSE;
LOCAL int sysAuxClkConnected	= FALSE;
LOCAL int sysAuxClkTicksPerSecond = 64;

LOCAL CLK_RATE auxTable[] =
    {
    {   2, 0x0f}, 
    {   4, 0x0e}, 
    {   8, 0x0d}, 
    {  16, 0x0c}, 
    {  32, 0x0b}, 
    {  64, 0x0a}, 
    { 128, 0x09}, 
    { 256, 0x08}, 
    { 512, 0x07}, 
    {1024, 0x06}, 
    {2048, 0x05}, 
    {4096, 0x04}, 
    {8192, 0x03} 
    };


/*******************************************************************************
*
* sysClkInt - interrupt level processing for system clock
*
* This routine handles the system clock interrupt.  It is attached to the
* clock interrupt vector by the routine sysClkConnect().
*/

LOCAL void sysClkInt (void)

    {

    /* acknowledge interrupt */

    if (sysClkRoutine != NULL)
	(* sysClkRoutine) (sysClkArg);

    }
/*******************************************************************************
*
* sysClkConnect - connect a routine to the system clock interrupt
*
* This routine specifies the interrupt service routine to be called at each
* clock interrupt.  It does not enable system clock interrupts.
* Normally it is called from usrRoot() in usrConfig.c to connect
* usrClock() to the system clock interrupt.
*
* RETURN: OK, or ERROR if the routine cannot be connected to the interrupt.
*
* SEE ALSO: intConnect(), usrClock(), sysClkEnable()
*/

STATUS sysClkConnect
    (
    FUNCPTR routine,	/* routine to be called at each clock interrupt */
    int arg		/* argument with which to call routine */
    )
    {

    sysHwInit2 ();	/* XXX for now -- needs to be in usrConfig.c */

    sysClkRoutine = routine;
    sysClkArg	  = arg;
    sysClkConnected = TRUE;

    return (OK);
    }
/*******************************************************************************
*
* sysClkDisable - turn off system clock interrupts
*
* This routine disables system clock interrupts.
*
* RETURNS: N/A
*
* SEE ALSO: sysClkEnable()
*/

void sysClkDisable (void)

    {
    if (sysClkRunning)
	{
	sysOutByte (PIT_CMD (PIT_BASE_ADR), 0x38);
	sysOutByte (PIT_CNT0 (PIT_BASE_ADR), LSB(0));
	sysOutByte (PIT_CNT0 (PIT_BASE_ADR), MSB(0));

	sysIntDisablePIC (PIT_INT_LVL);
	
	sysClkRunning = FALSE;
	}
    }
/*******************************************************************************
*
* sysClkEnable - turn on system clock interrupts
*
* This routine enables system clock interrupts.
*
* RETURNS: N/A
*
* SEE ALSO: sysClkConnect(), sysClkDisable(), sysClkRateSet()
*/

void sysClkEnable (void)

    {
    UINT tc;

    if (!sysClkRunning)
	{
	/* write the timer value.
	 * counter is 1193180 / sysClkTicksPerSecond.
	 */
	
	tc = PIT_CLOCK / sysClkTicksPerSecond;

	sysOutByte (PIT_CMD (PIT_BASE_ADR), 0x36);
	sysOutByte (PIT_CNT0 (PIT_BASE_ADR), LSB(tc));
	sysOutByte (PIT_CNT0 (PIT_BASE_ADR), MSB(tc));

	/* enable clock interrupt */

	sysIntEnablePIC (PIT_INT_LVL);
	
	sysClkRunning = TRUE;
	}
    }
/*******************************************************************************
*
* sysClkRateGet - get the system clock rate
*
* This routine returns the interrupt rate of the system clock.
*
* RETURNS: The number of ticks per second of the system clock.
*
* SEE ALSO: sysClkRateSet(), sysClkEnable()
*/

int sysClkRateGet (void)
    
    {
    return (sysClkTicksPerSecond);
    }

/*******************************************************************************
*
* sysClkRateSet - set the system clock rate
*
* This routine sets the interrupt rate of the system clock.
* It does not enable system clock interrupts.
* Normally it is called by usrRoot() in usrConfig.c.
*
* RETURNS:
* OK, or ERROR if the tick rate is invalid or the timer cannot be set.
*
* SEE ALSO: sysClkRateGet(), sysClkEnable()
*/

STATUS sysClkRateSet
    (
    int ticksPerSecond	    /* number of clock interrupts per second */
    )
    {
    if (ticksPerSecond < SYS_CLK_RATE_MIN || ticksPerSecond > SYS_CLK_RATE_MAX)
	return (ERROR);

    sysClkTicksPerSecond = ticksPerSecond;

    if (sysClkRunning)
	{
	sysClkDisable ();
	sysClkEnable ();
	}

    return (OK);
    }

/*******************************************************************************
*
* sysAuxClkInt - handle an auxiliary clock interrupt
*
* This routine handles an auxiliary clock interrupt.  It acknowledges the
* interrupt and calls the routine installed by sysAuxClkConnect().
*
* RETURNS: N/A
*/

LOCAL void sysAuxClkInt (void)
    {

    /* acknowledge the interrupt */

    sysOutByte (RTC_INDEX, 0x0c);
    sysInByte (RTC_DATA);

    /* call auxiliary clock service routine */

    if (sysAuxClkRoutine != NULL)
	(*sysAuxClkRoutine) (sysAuxClkArg);
    }

/*******************************************************************************
*
* sysAuxClkConnect - connect a routine to the auxiliary clock interrupt
*
* This routine specifies the interrupt service routine to be called at each
* auxiliary clock interrupt.  It also connects the clock error interrupt
* service routine.
*
* RETURNS: OK, or ERROR if the routine cannot be connected to the interrupt.
*
* SEE ALSO: intConnect(), sysAuxClkEnable()
*/

STATUS sysAuxClkConnect
    (
    FUNCPTR routine,    /* routine called at each aux clock interrupt */
    int arg             /* argument with which to call routine        */
    )
    {
    sysAuxClkRoutine	= routine;
    sysAuxClkArg	= arg;
    sysAuxClkConnected	= TRUE;

    return (OK);
    }

/*******************************************************************************
*
* sysAuxClkDisable - turn off auxiliary clock interrupts
*
* This routine disables auxiliary clock interrupts.
*
* RETURNS: N/A
*
* SEE ALSO: sysAuxClkEnable()
*/

void sysAuxClkDisable (void)
    {
    if (sysAuxClkRunning)
        {
	/* disable interrupts */

	sysOutByte (RTC_INDEX, 0x0b);
	sysOutByte (RTC_DATA, 0x02);

	sysIntDisablePIC (RTC_INT_LVL);

	sysAuxClkRunning = FALSE;
        }
    }

/*******************************************************************************
*
* sysAuxClkEnable - turn on auxiliary clock interrupts
*
* This routine enables auxiliary clock interrupts.
*
* RETURNS: N/A
*
* SEE ALSO: sysAuxClkDisable()
*/

void sysAuxClkEnable (void)
    {
    int ix;
    char statusA;

    if (!sysAuxClkRunning)
        {

	/* set an interrupt rate */

	for (ix = 0; ix < NELEMENTS (auxTable); ix++)
	    {
	    if (auxTable [ix].rate == sysAuxClkTicksPerSecond)
		{
	        sysOutByte (RTC_INDEX, 0x0a);
	        statusA = sysInByte (RTC_DATA) & 0xf0;
	        sysOutByte (RTC_INDEX, 0x0a);
	        sysOutByte (RTC_DATA, statusA | auxTable [ix].bits);
		break;
		}
	    }

	/* start the timer */

	sysOutByte (RTC_INDEX, 0x0b);
	sysOutByte (RTC_DATA, 0x42);

	sysIntEnablePIC (RTC_INT_LVL);

	sysAuxClkRunning = TRUE;
	}
    }

/*******************************************************************************
*
* sysAuxClkRateGet - get the auxiliary clock rate
*
* This routine returns the interrupt rate of the auxiliary clock.
*
* RETURNS: The number of ticks per second of the auxiliary clock.
*
* SEE ALSO: sysAuxClkEnable(), sysAuxClkRateSet()
*/

int sysAuxClkRateGet (void)
    {
    return (sysAuxClkTicksPerSecond);
    }

/*******************************************************************************
*
* sysAuxClkRateSet - set the auxiliary clock rate
*
* This routine sets the interrupt rate of the auxiliary clock.  If the
* auxiliary clock is currently enabled, the clock is disabled and then
* re-enabled with the new rate.
*
* RETURNS: OK or ERROR.
*
* SEE ALSO: sysAuxClkEnable(), sysAuxClkRateGet()
*/

STATUS sysAuxClkRateSet
    (
    int ticksPerSecond  /* number of clock interrupts per second */
    )
    {
    int		ix;	/* hold temporary variable */
    BOOL	match;	/* hold the match status */

    match = FALSE; 	/* initialize to false */

    if (ticksPerSecond < AUX_CLK_RATE_MIN || ticksPerSecond > AUX_CLK_RATE_MAX)
        return (ERROR);

    for (ix = 0; ix < NELEMENTS (auxTable); ix++)
	{
	if (auxTable [ix].rate == ticksPerSecond)
	    {
	    sysAuxClkTicksPerSecond = ticksPerSecond;
	    match = TRUE;
	    break;
	    }
	}

    if (!match)		/* ticksPerSecond not matching the values in table */
       return (ERROR);

    if (sysAuxClkRunning)
	{
	sysAuxClkDisable ();
	sysAuxClkEnable ();
	}

    return (OK);
    }

