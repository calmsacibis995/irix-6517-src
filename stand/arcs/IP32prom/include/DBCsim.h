/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:45:43 $
 * -------------------------------------------------------------
 * Debug card specific addresses and error codes.
 */

#ifndef DBCSIM_H
#define DBCSIM_H

/*
 *  Define the phony LA address, where we can write data to
 *  it.
 */
#define UARTaddr                _uartaddr
#define FLASH_ALIAS             0xbfc00000      /* FIXED                */
#define PROM_BASE               0xbfe00000      /* FIXED                */
#define ENLOW                   0xbff00000      /* FIXED                */

/*
 *  Define error codes, which will be used in conjuction with a
 *  LA in the lab.
 */
#define ERR_AFTER_RESET         0x0000001f      /* Error after reset.   */
#define ERR_NMI_SF_RESET        0x0000002f      /* NMI/soft reset error.*/
#define TLB_ERROR1              0x0000003f      /* TLB miss             */
#define TLB_ERROR2              0x0000004f      /* xTLB miss.           */
#define CACHE_ERROR             0x0000005f      /* Cache error.         */
#define UNEXPECTED_ERROR        0x0000006f      /* 0x380 error.         */

/*
 *  Define the flash accress control register.
 */
#define FLASH_CNTL              0xbf310008      /* ISA flash controlreg.*/

/*
 * Prototypes
 */

extern void DPRINTF(char *, ...);

#endif /* DBCSIM_H */
