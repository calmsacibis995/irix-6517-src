/*
 * -------------------------------------------------------------
 *  $Revision: 1.1 $
 *  $Date: 1997/08/18 20:41:10 $
 * -------------------------------------------------------------
 *  define the asm_support function prototype.
 */

#if defined(BIGSTACKFRAME)
/*
 * DebugCARD stack frame, a bigger register save area and more outargs and
 * local args.
 */
#define framesize   256
#define inargs      256         
#define frameoffset  64         /* allow 8 doubleword.              */
#define localargs   248         /* sp+framesize-0x08, max 8  dwords */
#define regsave     184         /* sp+framesize-(8*8)-0x8, 20 regs  */
#define outargs      24         /* sp+framesize-(4*8)-(20*8)-0x8    */
#else                           /* allow 4 extra integer registers  */
/*
 * DebugCARD stack frame, a smaller register save area and less outargs and
 * local args.
 */
#define framesize   176
#define inargs      176         
#define frameoffset  32         /* allow 4 doubleword.              */
#define localargs   168         /* sp+framesize-0x08, max 4  dwords */
#define regsave     136         /* sp+framesize-(4*8)-0x8, 16 regs  */
#define outargs       8         /* sp+framesize-(4*8)-(16*8)-0x8    */
#endif                          /* allow 2 extra integer registers  */
                                
/*
 * DebugCARD globalparms
 *  gp:   globalparm.
 *        gp + 0x00: Reserved and uninitialized.
 *        gp + 0x04: prinary cache size / 2.
 *        gp + 0x08: global interrupt flag
 *        gp + 0x0c: user interrupt handler address.
 *        gp + 0x10: Interrupt Count.
 *        gp + 0x14: Saved stack pointer.
 *        gp + 0x18: user specified interrupt flag.
 *        gp + 0x1c: The LOWPROM address toggled flag.
 *        gp + 0x20: Flash callback functioncode.
 *        gp + 0x24: Reserved and uninitialized.
 *        gp + 0x28: Reserved and uninitialized.
 *        gp + 0x2c: Reserved and uninitialized.
 *        gp + 0x30: Reserved and uninitialized.
 *        gp + 0x34: Reserved and uninitialized.
 *        gp + 0x38: Reserved and uninitialized.
 *        gp + 0x3c: Reserved and uninitialized.
 */
#define  PCACHESET     0x04
#define  GLOBALINTR    0x08
#define  USERINTR      0x0c
#define  INTRCOUNT     0x10
#define  SAVED_SP      0x14
#define  USRINTRFLG    0x18
#define  LowPROMFlag   0x1c

/*
 * Interrupt handlers related defines.
 */
#define INTR_INDICATOR  0xdeadbeef

/*
 * Some functions will be used by C programs.
 */
#if defined(_LANGUAGE_C)

extern  void    bevON           (uint32_t, uint32_t) ;
extern  void    bevOFF          (uint32_t, uint32_t) ;
extern  int     enablelow       (uint32_t, uint32_t) ;
extern  int     disablelow      (uint32_t, uint32_t) ;
extern  void    _jump2Flash     (uint32_t, uint32_t) ;
extern  void    regist_interrupt(uint32_t*) ;
extern  void    IP32LED_ON      (int)       ;
extern  void    IP32LED_OFF     (int)       ;

#endif

#if defined(_LANGUAGE_ASSEMBLY)

.extern  regist_interrupt   0

#endif

/*
 * -------------------------------------------------------------
 *
 * $Log: asm_support.h,v $
 * Revision 1.1  1997/08/18 20:41:10  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.4  1996/10/31  21:51:05  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.4  1996/10/04  20:09:04  kuang
 * Fixed some general problem in the diagmenu area
 *
 * Revision 1.3  1996/04/04  23:10:58  kuang
 * Added more diagnostic supports
 *
 * Revision 1.2  1996/03/25  22:17:15  kuang
 * These should fix the PHYS_TO_K1 problem
 *
 * Revision 1.1  1995/11/15  00:41:20  kuang
 * initial checkin
 *
 * Revision 1.1  1995/11/14  22:48:02  kuang
 * Initial revision
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
