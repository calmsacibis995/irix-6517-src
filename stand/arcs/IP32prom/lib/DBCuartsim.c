
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

#if defined(IP32SIM)

/*
 * -------------------------------------------------------------
 *  $Revision: 1.1 $
 *  $Date: 1997/08/18 20:46:55 $
 * -------------------------------------------------------------
 *  routines to initialize the triton simulator UART port.
 */

#include <inttypes.h>
#include <DBCsim.h>
#include <DBCuartsim.h>

char HelloWorld[] = "SIMULATED UART INITIALIZED\n Hello World!\n" ;

/*
 * Function: uart_status () 
 * Return 0 if UART is not initialized and 1 otherwise.
 */
uint32_t
UARTstatus () 
{
    return (0x1) ;
}

/*
 * Function: uartinit () 
 * Initialize the st16c1450 to 9600 baud, 8 bit data,
 * 1 stop bit, and no parity.
 */
void
UARTinit ()
{
    int	     i, c ;
    int32_t  *UARTregs ; 

    i = 0 ;
    UARTregs = (int32_t *) (UARTbase|Uconfig) ;
    * UARTregs = 0x1 ; 
    UARTregs = (int32_t *) (UARTbase|RDWRport) ;
    while ((c = HelloWorld [i++]) != '\0') *UARTregs = c ;

}

/*
 * Function: getchar ()
 *  Return data or NULL if non.
 */
uint32_t 
DGETCHAR ()
{
    int32_t *U_port, *U_flag, datain ;

    datain = 0x0 ;
    U_flag = (int32_t *) (UARTbase|RDWRport) ;
    U_flag = (int32_t *) (UARTbase|RxREADY) ;
    while (*U_flag == 0x1) datain = *U_port ; 
    return (datain) ;
    
}


/*
 * Function: getc ()
 *  Return a 7 bit char until one is available.
 */
uint32_t 
DGETC ()
{
    uint32_t * U_port = (uint32_t *) (UARTbase|RDWRport) ;
    uint32_t * U_flag = (uint32_t *) (UARTbase|RxREADY) ;

    while (*U_flag != 0x1) {} ;
    return (*U_port & 0x7F) ;

}


/*
 * Function: putchar ()
 *  Write data to the THR register.
 */
void
DPUTCHAR (char dataout)
{
    char c = '\015' ;
    
    uint32_t * U_port = (uint32_t *) (UARTbase|RDWRport) ;
    * U_port = (uint32_t) dataout ;
    if (dataout == '\n') *U_port = (uint32_t) c ;
    return ;
}


/*
 * -------------------------------------------------------------
 *
 * $Log: DBCuartsim.c,v $
 * Revision 1.1  1997/08/18 20:46:55  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.1  1996/10/21  03:08:05  kuang
 * Merged in changes from Pulpwood IP32prom - up to v2.4 release
 *
 * Revision 1.2  1996/09/12  23:54:25  kuang
 * Added support for IP32SIM environment variable.
 *
 * Revision 1.1  1995/11/20  22:49:10  kuang
 * initial checkin
 *
 * -------------------------------------------------------------
 */

#endif

/* END OF FILE */
