/*
 * -------------------------------------------------------------
 *  $Revision: 1.1 $
 *  $Date: 1997/08/18 20:41:53 $
 * -------------------------------------------------------------
 *  routines to initialize the st16c1450 chip and others.
 */

#include <inttypes.h>
#include <debugcard.h>
#include <UARTsim.h>

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
getchar ()
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
getc ()
{
    volatile uint32_t* U_port = (volatile uint32_t*) (UARTbase|RDWRport) ;
    volatile uint32_t* U_flag = (volatile uint32_t*) (UARTbase|RxREADY) ;

    while (*U_flag != 0x1) {} ;
    return (*U_port & 0x7F) ;

}


/*
 * Function: putchar ()
 *  Write data to the THR register.
 */
void
putchar (char dataout)
{
    char c = '\015' ;
    
    volatile uint32_t* U_port = (volatile uint32_t*) (UARTbase|RDWRport) ;
    * U_port = (uint32_t) dataout ;
    if (dataout == '\n') *U_port = (uint32_t) c ;
    return ;
}


/*
 * -------------------------------------------------------------
 *
 * $Log: UARTsim.c,v $
 * Revision 1.1  1997/08/18 20:41:53  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.3  1996/10/31  21:51:42  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.2  1996/04/04  23:15:50  kuang
 * Added volatile to the simulated uart base
 *
 * Revision 1.1  1995/11/15  00:42:39  kuang
 * initial checkin
 *
 * Revision 1.2  1995/11/14  23:02:35  kuang
 * Final cleanup for ptool checkin
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
