
/**************************************************************************
 *                                                                        *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if !defined(IP32SIM)

/*
 * -------------------------------------------------------------
 *  $Revision: 1.1 $
 *  $Date: 1997/08/18 20:47:47 $
 * -------------------------------------------------------------
 *  routines to initialize the st16c1450 chip and others.
 */

#include <inttypes.h>
#include <debugcard.h>
#include <st16c1451.h>

/* #define DebugUART */

/*
 * Function nop (), which will decrement an register
 * int variable from x to zero then return.
 */
void
UARTnop(int num)
{
  register int  i ;
  for (i = num ; i != 0x0 ; i--) ;
  return ;
}

    
/*
 * Function: uart_status () 
 * Return 0 if UART is not initialized and 1 otherwise.
 */
uint32_t
UARTstatus () 
{

    return (UartRd(SPR) == 0xde) ;
}

/*
 * Function: uartinit () 
 * Initialize the st16c1450 to 9600 baud, 8 bit data,
 * 1 stop bit, and no parity.
 */
void
UARTinit ()
{
    register char c ; 
    
    /*
     *  Enable the baud rate generator divider latch.
     *  set bit 7 of LCR 
     *  base->LCR = 0x80 ;
     */
    UartWr(LCR, 0x80) ; 
    
    /*
     *  Set to 9600 baud.
     * ----------------------------------
     *  set baud rate generate to 0x0018
     *  base->DLM = 0x00 ;
     *  base->DLL = 0x18 ;
     */
    UartWr(DLM, 0x00) ; 
    UartWr(DLL, 0x18) ; 

    /*
     *  Set 8 data bit, 1 stop bit, no parity.
     *  base->LCR = 0x00 ; Turn off baud rate register access.
     *  base->LCR = 0x03 ; set the data bit, stop bit, ...
     */
    UartWr(LCR, 0x03) ; 

    /*  
     *  No interrupt please.
     *  base->IER = 0x00 ;
     */
    UartWr(IER, 0x00) ; 

    /*
     *  Set RTS and DTR to low, in case we have modem attached to it.
     *  Do we need this ??
     *  base->MCR = 0x0b ;
     * -------------------
     *  base->MCR = 0x00 ;
     */
    UartWr(MCR, 0x00) ; 

    /*
     * indicate that we initialized the UART.
     * base->SPR = 0xde ;
     * this is it.
     */
    UartWr(SPR, 0xde) ; 

    /*
     * Now we done.
     */
    return ;
    
}


/*
 * Function: Dgetchar ()
 *  Return data or -1 if non.
 */
int32_t 
Dgetchar ()
{
    int        datain ;

    datain = -1 ;
    while ((UartRd(LSR) & RXREADY) != 0x0) {
        datain = UartRd(RHR) ;
        datain = datain & 0xff ;
    }
    return datain ;
}


/*
 * Function: Dgetc ()
 *  Return a 7 bit char until one is available.
 */
uint32_t 
Dgetc ()
{
    char        datain ;

    datain = 0x0 ;
    while ((UartRd(LSR) & RXREADY) == 0x0) {} ;
    datain = UartRd(RHR) & 0x7F ; 
    return ((uint32_t) datain) ;
    
}


/*
 * Function: DPUTCHAR ()
 *  Write data to the THR register.
 */
void
DPUTCHAR (char dataout)
{
/* Do we need to check the CTS ?? */
/*  while (((ptr->LCR & TXREADY) == 0x0) && ((ptr->MSR & CTS) == 0x10)) {} ; */
/*  ptr->THR = dataout ; */
    while ((UartRd(LSR) & TXREADY) == 0x0) {} ;
    UartWr(THR, dataout) ; 
    return ;
}

/*
 * -------------------------------------------------------------
 *
 * $Log: st16c1451.c,v $
 * Revision 1.1  1997/08/18 20:47:47  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.1  1996/10/21  03:08:09  kuang
 * Merged in changes from Pulpwood IP32prom - up to v2.4 release
 *
 * Revision 1.3  1996/09/12  23:54:31  kuang
 * Added support for IP32SIM environment variable.
 *
 * Revision 1.2  1995/12/21  21:06:26  kuang
 * Fixed the getchar wait loop.
 *
 * Revision 1.1  1995/11/15  00:42:48  kuang
 * initial checkin
 *
 * Revision 1.2  1995/11/14  23:31:33  kuang
 * Rearranged the RCS keyword a bit and ready for ptool checkin
 *
 * Revision 1.1  1995/09/07  18:44:32  kuang
 * Initial revision *
 * -------------------------------------------------------------
 */

#endif

/* END OF FILE */
