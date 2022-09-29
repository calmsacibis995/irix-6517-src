/*
 * -------------------------------------------------------------
 *  $Revision: 1.1 $
 *  $Date: 1997/08/18 20:46:22 $
 * -------------------------------------------------------------
 *  define the startech st16c1450 register set and related
 *  informations.
 */

#define RXREADY  0x01
#define TXREADY  0x20
#define CTS      0x10

#define RHR      0x0
#define THR      0x0
#define IER      0x1
#define ISR      0x2
#define LCR      0x3
#define MCR      0x4
#define LSR      0x5
#define MSR      0x6
#define SPR      0x7
#define DLL      0x0
#define DLM      0x1
#define UartWr(X,XD) {*(uint8_t*)((X<<8)|UARTaddr|XD)=XD;UARTnop(32);}
#define UartRd(X)    (*(volatile uint8_t*)((X<<8)|UARTaddr|3))

#if defined(_LANGUAGE_C)
extern void        UARTnop     (int) ;
extern uint32_t    UARTstatus  (void) ;
extern void        UARTinit    (void) ;
extern int32_t     DGETCHAR    (void) ;
extern uint32_t    DGETC       (void) ;
extern void        DPUTCHAR    (char) ;
#endif

#if defined(SIM) & defined(_LANGUAGE_C)
extern void _la_erraddr1 (void) ;
extern void _la_erraddr2 (void) ;
extern void _la_erraddr3 (void) ;
extern void _la_erraddr4 (void) ;
extern void _uartaddr (void) ;
#endif


/*
 * -------------------------------------------------------------
 *
 * $Log: st16c1451.h,v $
 * Revision 1.1  1997/08/18 20:46:22  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1995/12/21  21:01:22  kuang
 * Added volatile to macro UartRd
 *
 * Revision 1.1  1995/11/15  00:41:25  kuang
 * initial checkin
 *
 * Revision 1.1  1995/11/14  22:56:54  kuang
 * Initial revision
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
