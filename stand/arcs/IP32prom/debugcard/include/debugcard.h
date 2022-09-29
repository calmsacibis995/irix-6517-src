/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:41:26 $
 * -------------------------------------------------------------
 * Debug card specific addresses and error codes.
 */

/*
 *  Define the phony LA address, where we can write data to
 *  it.
 */
#ifndef	SIM 
#define LA_ERRaddr1             0xbfc40000      /* This I don't know yet*/
#define LA_ERRaddr2             0xbfc40000      /* as above.            */
#define LA_ERRaddr3             0xbfc40000      /* same here.           */
#define LA_ERRaddr4             0xbfc40000      /* same.                */
#define UARTaddr                0xbfd00000      /* DebugCard UART base  */
#define FLASH_ALIAS             0xbfc00000      /* PROM addr base.      */
#define PROM_BASE               0xbfe00000      /* DebugCard PROM addrb */
#define ENLOW                   0xbff00000      /* Switch prom on write */
#else
#define LA_ERRaddr1             _la_erraddr1
#define LA_ERRaddr2             _la_erraddr2
#define LA_ERRaddr3             _la_erraddr3
#define LA_ERRaddr4             _la_erraddr4
#define UARTaddr                _uartaddr
#define FLASH_ALIAS             0xbfc00000      /* FIXED                */
#define PROM_BASE               0xbfe00000      /* FIXED                */
#define ENLOW                   0xbff00000      /* FIXED                */
#endif

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
 * -------------------------------------------------------------
 *
 * $Log: debugcard.h,v $
 * Revision 1.1  1997/08/18 20:41:26  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:51:13  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.1  1995/11/15  00:41:22  kuang
 * initial checkin
 *
 * Revision 1.1  1995/11/14  22:52:39  kuang
 * Initial revision
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
