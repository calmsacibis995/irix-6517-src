/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:39:53 $
 * -------------------------------------------------------------
 *  descriptions:   The diag command loop.
 */

#ifdef  DEBUG
#include <stdio.h>
#include <termios.h>
typedef unsigned long long uint64_t ;
#else
#include <inttypes.h>
#endif
#include <debugcard.h>

#if !defined(GETC) & defined(DEBUG)
#define GETC getchar
#else
#define GETC getc
#endif

#include <utilities.h>
#include <asm_support.h>
#include <st16c1451.h>
#include <uartio.h>
#include <Dflash.h>
#include <fwcallback.h>

#define FW_HARD_RESET   0   /* Power on or hard reset*/
#define FW_SOFT_RESET   1   /* Soft reset or NMI */
#define FW_EIM          2   /* Enter Interactive Mode */
#define FW_HALT         3   /* Halt */
#define FW_POWERDOWN    4   /* Power down */
#define FW_RESTART      5   /* Restart */
#define FW_REBOOT       6   /* Reboot */

/*
 * Function prototypes for the studip lint stuff.
 */
extern  SimpleMEMtst                (uint32_t, uint32_t)    ;
extern  uint32_t SizeMEM            (void)                  ;
extern  uint32_t rd_status          (void)                  ;
extern  void     wr_status          (uint32_t)              ;
extern  void printbankinfo          (void)                  ;
extern  void memory_walk_1          (uint32_t)              ;
extern  void memory_walk_0          (uint32_t)              ;
extern  void memory_walk_addr0      (uint32_t)              ;
extern  void memory_walk_addr1      (uint32_t)              ;
extern  void memory_2pattern_test   (uint64_t*, uint64_t*)  ;
extern  void memory_addrtst         (uint64_t*, uint64_t*)  ;
extern  void unexpected_intr        (void)                  ;

extern  void test_pci_cfg_space     (void)                  ; 
void    do_pci_test                 (void)                  ;
void    printdiagmenu               (uint32_t)              ;
void    do_qmem                     (uint32_t)              ;
void    do_memory_test              (uint32_t)              ;
void    Dispatcher                  (void)                  ;
void    do_firmware_test            (void)                  ;
void    do_TV_test                  (void)                  ;

/*
 * Function print the debugcard main menu.
 */
void    
printdiagmenu (uint32_t size)
{
  uint32_t memSize ;

  memSize = size / (1024*1024) ;
  
  /*
   * Print diagnostic menu.
   */
  printf ("\n\n") ;
  printf ("Moosehead diagnostic monitor v0.0\n\n"
          "     [0]  Quick Memory test of %d Mbytes\n"
          "     [1]  Memory test of %d MBytes\n"
          "     [2]  Simple PCI/SCSI diag\n"
          "     [3]  Start Moosehead DVT\n"
          "     [4]  Test firmware call back functions\n"
          "     [5]  Test firmware Transfer Vectors\n"
          "     [6]  Exit diag menu\n"
          "\n"
          "     =====> ",
          memSize, memSize) ;
  return ; 
}


/*
 * This is the TOP menu main command loop.
 */
uint32_t
diagselect (uint32_t mSize)
{
    char c ;
    uint64_t de ;
    uint32_t memsize ; 

    /*
     * Size memory and save the memory size.
     */
    if (mSize != 0x0) {
      memsize = mSize ;
      printf ("Test %d MBytes of memory\n", memsize/(1024*1024)) ;
    } else {
      memsize = SizeMEM () ;
      printf ("Size memory completed, found %d MBytes of memory\n",
              memsize/(1024*1024)) ;
    }
    
    /*
     *  Display the first level menu items.
     */
    printdiagmenu (memsize) ;

    for (;;) {
      switch (c = GETC ()) {
      case '0':
        putchar (c) ;
        do_qmem (memsize) ;
        break ;
      case '1':
        putchar (c) ;
        do_memory_test (memsize) ;
        break ;
      case '2':
        putchar (c) ;
        do_pci_test () ; 
        break ;
      case '3':
        putchar (c) ;
        Dispatcher () ;
        break ; 
      case '4':
        putchar (c) ;
        do_firmware_test();
      case '5':
        putchar (c) ;
        do_TV_test();
      case '6':
        putchar (c) ;
        return memsize ; 
      default:
        putchar (c) ;
        printf (" ;  *** INVALID OPTION *** ") ;
        break ;
      }
      printdiagmenu (memsize) ;
    }
    return memsize ; 
}

/*
 * The memory diagnostic entry point.
 */
void
do_qmem (uint32_t memsize) 
{
  uint32_t Size ;

  Size = memsize ; 
  memory_walk_1 (0x0) ;
  memory_walk_0 (0x0) ;
  memory_walk_addr1(Size) ;
  memory_walk_addr0(Size) ;
  
  return ; 
}

/*
 * Function print the debugcard main menu.
 */
uint32_t
printFTmenu (void)
{
  /*
   * Print diagnostic menu.
   */
  printf ("\n\n"    
          "Firmware callback submenu \n\n"
          "     [1]  Reboot test\n"
          "     [2]  PowerDown test\n"
          "     [3]  Restart test\n"
          "     [4]  Halt test\n"
          "     [5]  EnterInterActiveMode test\n"
          "     [6]  Hardreset\n"
          "     [7]  Softreset\n"
          "     [8]  That's enough\n"
          "\n"
          "     =====> ");
  return GETC();
}

extern void _PROMcallback(int) ;

/*
 * The firmware callback function test.
 */
void
do_firmware_test(void) 
{
  register uint32_t functionCode, c;
  void (*Firmware)(int);

  Firmware = (void (*)(int))_PROMcallback ;
  
  do {
    putchar(c = printFTmenu()) ;
    switch (c) {
    case '1':
      (*Firmware)(FW_REBOOT);
      unexpected_intr();
    case '2':
      (*Firmware)(FW_POWERDOWN);
      unexpected_intr();
    case '3':
      (*Firmware)(FW_RESTART);
      unexpected_intr();
    case '4':
      (*Firmware)(FW_HALT);
      unexpected_intr();
    case '5':
      (*Firmware)(FW_EIM);
      unexpected_intr();
    case '6':
      (*Firmware)(FW_HARD_RESET);
      unexpected_intr();
    case '7':
      (*Firmware)(FW_SOFT_RESET);
      unexpected_intr();
    case '8':
      functionCode = 0x0;
      break;
    default:
      printf("\n\nInvalid option, try again\n\n");
      functionCode = 0x1;
      break;
    }
  } while(functionCode);
  return ; 
}

/*
 * Function print the debugcard main menu.
 */
uint32_t
printTVmenu (void)
{
  /*
   * Print diagnostic menu.
   */
  printf ("\n\n"    
          "Firmware TV callback submenu \n\n"
          "     [1]  Reboot test\n"
          "     [2]  PowerDown test\n"
          "     [3]  Restart test\n"
          "     [4]  Halt test\n"
          "     [5]  EnterInterActiveMode test\n"
          "     [6]  That's enough\n"
          "\n"
          "     =====> ");
  return GETC();
}

/*
 * The firmware callback function test.
 */
void
do_TV_test(void)
{
  register uint32_t functionCode, c;
  struct TV {
    void (*Reboot)(void);
    void (*Restart)(void);
    void (*Halt)(void);
    void (*PowerDown)(void);
    void (*EnterInteractiveMode)(void);
  } _TV ;

  _TV.Reboot               = FWCB_Reboot;
  _TV.Restart              = FWCB_Restart;
  _TV.Halt                 = FWCB_Halt;
  _TV.PowerDown            = FWCB_PowerDown;
  _TV.EnterInteractiveMode = FWCB_EnterInteractiveMode;

  do {
    putchar(c = printTVmenu()) ;
    switch (c) {
    case '1':
      _fwTVtst(_TV.Reboot);
      unexpected_intr();
    case '2':
      _fwTVtst(_TV.PowerDown);
      unexpected_intr();
    case '3':
      _fwTVtst(_TV.Restart);
      unexpected_intr();
    case '4':
      _fwTVtst(_TV.Halt);
      unexpected_intr();
    case '5':
      _fwTVtst(_TV.EnterInteractiveMode);
      unexpected_intr();
    case '6':
      functionCode = 0x0;
      break;
    default:
      printf("\n\nInvalid option, try again\n\n");
      functionCode = 0x1;
      break;
    }
  } while(functionCode);
  return ; 
}

void
do_memory_test (uint32_t Size)
{

  memory_walk_1 (0x0) ;
  memory_walk_0 (0x0) ;
  memory_2pattern_test ((uint64_t*)0xa0000000, (uint64_t*)0xa2000000) ;
  memory_addrtst ((uint64_t*)0xa0000000, (uint64_t*)0xa2000000) ;
  
  return ; 
}

void
do_pci_test ()
{
    test_pci_cfg_space () ;
    return ;
}

/*
 * -------------------------------------------------------------
 *
 * $Log: diagmenu.c,v $
 * Revision 1.1  1997/08/18 20:39:53  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.5  1996/10/31  21:50:11  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.6  1996/10/04  21:08:43  kuang
 * Cleanup the sources list
 *
 * Revision 1.5  1996/10/04  20:08:59  kuang
 * Fixed some general problem in the diagmenu area
 *
 * Revision 1.4  1996/05/01  18:26:44  bongers
 * add 'do_pci_test' back to switch statement
 *
 * Revision 1.3  1996/04/04  23:33:44  kuang
 * Added more diagnostic supports
 *
 * Revision 1.2  1996/01/02  22:19:23  kuang
 * new and improved memory diags
 *
 * Revision 1.1  1995/12/30  03:50:44  kuang
 * moosehead bringup 12-29-95 d15 checkin
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
