/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:42:14 $
 * -------------------------------------------------------------
 *  descriptions:   The main command loop.
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

#if !defined(IP32)
#define IP32 1
#endif
#include <utilities.h>
#include <asm_support.h>
#include <st16c1451.h>
#include <uartio.h>
#include <Dflash.h>
#include <sbd.h>
#include <mace.h>

char NotImp  [] = " ; to be implemented!\n" ;
char Aborted [] = "Function aborted!\n\n" ;

/*
 * Function prototypes for the studip lint stuff.
void        diagselect      (void) ;
 */
void        printmenu       (void) ;
void        do_loadflash    (void) ;
void        do_jumpflash    (void) ;
void        power_off       (void) ;
uint32_t    diagselect      (uint32_t) ;
extern uint32_t rd_cause    (void);
extern uint32_t rd_status   (void);

/*
 * Firmware dispatch codes
 */
#define FW_HARD_RESET   0   /* Power on or hard reset*/
#define FW_SOFT_RESET   1   /* Soft reset or NMI */
#define FW_EIM          2   /* Enter Interactive Mode */
#define FW_HALT         3   /* Halt */
#define FW_POWERDOWN    4   /* Power down */
#define FW_RESTART      5   /* Restart */
#define FW_REBOOT       6   /* Reboot */

/*
 * Call the printdiagmen in diags subdirectory
 */
void
do_poweroff(void) {
  printf ("\nSystem going down now!!!\n");
  power_off();
  return;
}
  
/*
 * Call the printdiagmen in diags subdirectory
 */
uint32_t
do_diags (uint32_t S) {
  return diagselect(S) ;
}

/*
 * Functions load the MH Flash.
 */
void
do_loadflash ()
{
  char  *ptr ;
  /*if (loadFlash()) printf ("MH Flash loaded!\n\n") ; */
  if (UPloadFlash()) printf ("MH Flash loaded!\n\n") ;
  else printf ("Checksum error while loading MH Flash,\n%s",Aborted) ;
  return ;
}


/*
 * Jump to MH system prom - jump start MH from debugcard prom.
 */
void
do_jumpflash ()
{
  printf ("Cause = 0x%0x\n", rd_cause());
  if (verifyFlash()) _jump2Flash (0,0) ;
  printf ("Can not find valid sloader header in MH Flash\n%s", Aborted) ;
  return ;
}


/*
 * Function print the debugcard main menu.
 */
void
printmenu ()
{
  /*
   * Turn on Green led and turn off RED led IP32
   */
  IP32LED_OFF(ISA_LED_RED);
  IP32LED_ON(ISA_LED_GREEN);
  
  printf ("\n\n") ;
  printf ("Moosehead debugcard firmware monitor v0.0\n\n"
          "     [1]  Utilities menu\n"
          "     [2]  Diagnostic menu\n"
          "     [3]  Verify MH system PROM\n"
          "     [4]  Load MH system PROM\n"
          "     [5]  Start system (Jump to MH PROM)\n"
          "     [6]  Exit (power off)\n"
          "\n"
          "     =====> ") ;
  return ; 
}


/*
 * This is the TOP menu main command loop.
 */
cmdloop () 
{
    char c ;
    uint64_t de ;
    uint32_t memSize ; 

    /*
     * Initialize the memSize to zero.
     */
    memSize = 0 ;

    /*
     *  Display the first level menu items.
     */
    printmenu () ;

    for (;;) {
      switch (c = GETC ()) {
      case '1':
        putchar (c) ;
        do_utilities () ;
        break ;
      case '2':
        putchar (c) ;
        memSize = do_diags (memSize) ;
        break ;
      case '3':
        putchar (c) ;
        verifyFlash () ;
        break ;
      case '4':
        putchar (c) ;
        do_loadflash () ;
        break ;
      case '5':
        putchar (c) ;
        do_jumpflash () ;
        break ;
      case '6':
        putchar (c) ;
        do_poweroff () ;
        break ;
      default:
        putchar (c) ;
        printf (" ;  *** INVALID OPTION *** ") ;
        break ;
      }
      printmenu () ;
    }
}

#ifdef  DEBUG
main ()
{
  int c ;
  struct termios t_orig, t;

  tcgetattr(0, &t_orig);
  t = t_orig;         /* structure copy */
  t.c_iflag &= ~(INLCR | ICRNL | ISIG) ;
  t.c_lflag &= ~(ICANON  | ECHO);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  tcsetattr(0, TCSANOW, &t);
    
  cmdloop () ;

  tcsetattr(0, TCSANOW, &t_orig);
  exit(0);
  
}
#endif

/*
 * -------------------------------------------------------------
 *
 * $Log: menu.c,v $
 * Revision 1.1  1997/08/18 20:42:14  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.5  1996/10/31  21:51:53  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.5  1996/10/04  20:09:12  kuang
 * Fixed some general problem in the diagmenu area
 *
 * Revision 1.4  1996/04/04  23:17:26  kuang
 * Added more diagnostic support and some general cleanup
 *
 * Revision 1.3  1996/03/25  22:21:25  kuang
 * Added power off option to the main menu
 *
 * Revision 1.2  1995/12/30  03:28:24  kuang
 * First moosehead lab bringup checkin, corresponding to 12-29-95 d15
 *
 * Revision 1.1  1995/11/15  00:42:46  kuang
 * initial checkin
 *
 * Revision 1.2  1995/11/14  23:09:14  kuang
 * program is not quite complete, BUT good enough for engineering
 * bringup use.
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
