#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/types.h>
#include <trace.h>
#include <assert.h>
#include <flash.h>
#include <libsc.h>
#include <sys/ds17287.h>
#include <fault.h>
#include <sys/crimechip.h>
#include <sys/mace.h>
#include <stringlist.h>
#include <errno.h>
#include <arcs/restart.h>
#include <arcs/pvector.h>
#include <arcs/spb.h>
#include <fwcallback.h>
#include <guicore.h>
#include <gfxgui.h>

#define UNIMPLEMENTED 0
#define IMPOSSIBLE    0

/* #define DEBUG 1  */
#undef  DEBUG
#if defined(DEBUG)
extern void print_cache_config(void);
#endif
extern void config_cache(void);
extern void flush_cache(void);
extern void _hook_exceptions(void);
extern void init_env(void);
extern void cpu_rtcinit(void);
extern void _init_saio(void);
extern void post2(int,int);
extern void post3(int,int);
extern void SetSR(unsigned long);
extern unsigned long GetSR(void);
extern void fwStart(int,int);
extern void _main(void);
extern void initTiles(void);
extern void DPRINTF(char *, ...);
extern void led_on(int), led_off(int);
extern void startup(void);
extern void cpu_hardreset(void);
extern void cpu_softreset(void);
extern void cpu_powerdown(void);
extern void cpu_powerdown(void);
extern void IP32processorTCI(void);
extern int  Read_C0_LLADDR(void);
extern unsigned
int         cpu_get_memsize(void);
extern void mte_zero(paddr_t, size_t);

extern void initGraphics(int functionCode);
extern void initVideo(void);
extern int  doGui(void);
extern void setGuiMode(int,int);
extern int  isGuiMode(void);
extern int  popupDialog(char*,int*,int,int);
extern void alloc_memdesc(void) ;

extern struct string_list environ_str;
void finit2(int,int);
void finit3(int,int);
void fw_dispatcher(int) ;
void FixupFirmwareVector(void) ;

static void waitOnUST(void);

/* PROM variable.  Non-zero if executing from "Prom" environment */

int _prom = 0x0 ;
int _rtcinitted = 0x0;


/* Two seconds of MACE_UST_PERIOD ticks */
#define TWO_SECONDS (2000000000/MACE_UST_PERIOD)

extern void playTune(int);

/**********
 * firmware	Called to manage firmware startup
 *---------
 * This routine is called right after finit1 which zeros the bss and
 * sets up the stack.
 *
 * On entry, we have a zeroed BSS and a firmware stack.  We proceed by
 * first initializing device independent services (config the cache, connect
 * to RAM based exceptions and so on).  When that is done, we have a  minimal
 * programming environment.
 *
 * The sequence post1, finit1, post2, finit2, post3, finit3 invoke various
 * functions in a relatively formal order, allowing a reasonable level of
 * specificity as to when various tests and initializations are performed.
 */
void
firmware(int functionCode, int resetCount){
  int            postFlag;

  /* Turn led to amber again, (post1 may have changed the led colors) */
  led_on(ISA_LED_RED|ISA_LED_GREEN);

  /*
   * Configure and flush the cache
   */
  config_cache();
  flush_cache();
  
#if defined(DEBUG)
  print_cache_config();
#endif

  /*
   * Hookup normal trap vectors and clear the BEV bit.
   */
  _hook_exceptions();

  /*
   * Switch to RAM based exceptions.
   */
  SetSR(GetSR() & ~SR_BEV);
  
  /*
   * Initialize RTC chip so that the kickstart switch will work.
   */
  cpu_rtcinit() ;
  
  /* Initialize the environment variables */
  init_env();

  /* Wait a bit */
  waitOnUST();

  /* Determine if we need to run the POST routines */
  postFlag = (functionCode==FW_HARD_RESET || 
	      (functionCode==FW_SOFT_RESET && resetCount!=1));

  if (postFlag) {
    post2(functionCode,resetCount);
    led_on(ISA_LED_RED|ISA_LED_GREEN);
  }
  finit2(functionCode,resetCount);
  
  if (postFlag) {
    post3(functionCode,resetCount);
    led_on(ISA_LED_RED|ISA_LED_GREEN);
  }
  finit3(functionCode,resetCount);
  
  /* Now is time to check all those cb stuff    */
  fw_dispatcher(functionCode);
  assert(IMPOSSIBLE);
  
#if 0  
  startup();
#endif  
}


/***********
 * waitOnUST  Wait until 2 seconds after initial UST timestamp.
 */
static void
waitOnUST(void){
/* NOTE: Disabled until simulator supports UST reads */
#ifdef NOTE
  unsigned long  oldUST,currentUST;
  ds17287_clk_t* rtc;

  oldUST =
    (rtc->ram[RTC_SAVE_UST+3]<<24) |
      (rtc->ram[RTC_SAVE_UST+2]<<16) |
	(rtc->ram[RTC_SAVE_UST+1]<<8) |
	  rtc->ram[RTC_SAVE_UST];

  /*
   * Note, if currentUST and oldUST are the same, we assume the UST is
   * not moving (broken) and we exit.
   */
  do
    currentUST = *(long*)PHYS_TO_K1(MACE_UST+4);
  while ((currentUST!=oldUST) &&(currentUST - oldUST)<TWO_SECONDS);
#endif
}


#if 0 /* finit1 is now in the csu.s */
/*****************************************************************************
 *                              finit 1                                      *
 *****************************************************************************/

/********
 * finit1	Pre-console initializations
 *-------
 * Note that this code runs on the "short" stack set up on entry into the
 * prom.
 *
 * Returns the top of the stack.
 */
unsigned long*
finit1(int resetCount){
  extern int sc_sable;
  extern unsigned firstBss[];
  unsigned bssSize = (unsigned)PROM_STACK-64 - K0_TO_K1((unsigned)firstBss);

  sc_sable = 1; /* Used to turn off bss and malbuf init */

  /*
   * Zero the BSS
   * (Everything between firstBss and PROM_STACK-64).  Note that _fbss 
   * and _end and those things are broken.  If they are ever fixed, this
   * should just clear from _fbss to _end.  Note that we skip this if
   * the bss has a positive address  or sc_sable is 1, either indicates
   * that the code is being simulated.
  if ((long)firstBss<0 && sc_sable==0) {
   */
  if ((long)firstBss<0 && sc_sable==0 && _prom==0x0)
    bzero((void*)K0_TO_K1(firstBss),bssSize);
  
  /* Declare that we are in the prom */
  _prom = 1;

  _fault_sp = (unsigned long*)PROM_STACK;

  return (unsigned long*)PROM_STACK-EXSTKSZ;

  /*
   * On exit from finit1, the BSS has been zeroed and we have determined
   * the firmware stack address.  The caller's stack cannot be in the bss
   * to PROM_STACK-64 region as we clobber that area.
   */
}
#endif


/*****************************************************************************
 *                              finit 2                                      *
 *****************************************************************************/

/* TEMPORARY */
void initMaceSerial(void){}

/********
 * finit2
 * ------
 * Perform initializations to bring up console.
 */
void
finit2(int functionCode, int resetCount)
{
  /*
   * Setup graphics HW.
   * Setup serial HW.
   */
  initMaceSerial();

  /*
   * Initialize tiles.
   */
  initTiles();

  /*
   * initialize all graphic related CRIME stuff.
   */
#if !defined(IP32SIM)
  initGraphics(functionCode);
#endif

  /*
   * Initialize saio.
   *
   * NOTE: At present, init_saio initializes practically everything.
   * It basically assumes that everything works.  At this point, we haven't
   * run the post3 tests so some things haven't been tested.  This seems
   * safe enough but there may be a problem here.  The goal is that post2
   * only tests console devices so that other hardware can be tested when
   * console is available to report errors.
   */
  _init_saio();

  /*
   * Initialize the video interface, primarily so
   * there is a valid (legal) video ouput signal.
   */
  /*** initVideo(); ***/

   /*
    * Now go ahead clean up ECC for memory above firmware.
    */
#if !defined(IP32SIM)
   mte_zero((PROM_STACK & 0x1fffffff)|LINEAR_BASE, cpu_get_memsize());
#endif

  /*
   * Allocate memory descriptors.
   * Startup console
   */
  alloc_memdesc();
  initConsole();

  /*
   * Upon exit from finit2, all device independent services are available.
   * All console I/O devices are tested and available.
   * Boot devices and other miscellaneous devices have not been teste.
   */
}


/*****************************************************************************
 *                              finit 3                                      *
 *****************************************************************************/

/*
 * See lib/libsk/spb.c for detail
 * Put firmware and private vectors on the SPB page, so they can be changed.
 * 0x1000 - 0x17ff: spb (2K)
 * 0x1800 - 0x1bff: firmware vector (1K)
 * 0x1c00 - 0x1fff: private vector (1K)
 */

/********
 * finit3
 * ------
 * Initialize any remaining context.
 */
void
finit3(int functionCode, int resetCount)
{
   /*
    * By now SPB->TransferVector should already initialized and pointed
    * to the memoy copy of the FirmwareVector, this will not work for
    * IP32 however,
    * Fixing up the SPB Firmware TV table for IP32 by pointing to the 
    * prom version of the FirmwareVector instead, it only support
    * EnterInteractiveMode, Halt, PowerDown, Restart and Reboot
    * callback functions.
    */
   FixupFirmwareVector();
   
   /*
    * initConsole() leaves a graphics dialog box up.  Since POST2
    * diagnostics have passed, we can now clean up the dialog box.
    */
   pongui_cleanup();

}

/***************************************************************************
 * EnterInteractiveMode, Halt, PowerDown, Restart and Reboot are called	   *
 * from the SPB. They return control to the PROM to perform the actual     *
 * operation.                                                              *
 *                                                                         *
 * Note: In every instance, the finit routines are called.  This may slow  *
 * things down somewhat but it doesn't seem worth the trouble to avoid the *
 * minor performance hit.                                                  *
 *                                                                         *
 * Note: The real function definitions of those call back functions are    *
 * now in the sloader/fwcallback.c                                         *
 ***************************************************************************/

/*********************
 * FixUpFirmwareVector
 */
void
FixupFirmwareVector(void) {
  FirmwareVector *SPB_TV = SPB->TransferVector;
  SPB_TV->EnterInteractiveMode = FWCB_EnterInteractiveMode;
  SPB_TV->Halt                 = FWCB_Halt;
  SPB_TV->PowerDown            = FWCB_PowerDown;
  SPB_TV->Restart              = FWCB_Restart;
  SPB_TV->Reboot               = FWCB_Reboot;
}

/**********************
 * EnterInteractiveMode
 */
void
EnterInteractiveMode(void) {
  FirmwareVector *SPB_TV = SPB->TransferVector;
  (*SPB_TV->EnterInteractiveMode)();
}

/******
 * Halt
 */
void
Halt(void) {
  FirmwareVector *SPB_TV = SPB->TransferVector;
  (*SPB_TV->Halt)();
}

/**********
 * PownDown
 */
void
PowerDown(void) {
  FirmwareVector *SPB_TV = SPB->TransferVector;
  (*SPB_TV->PowerDown)();
}

/*********
 * Restart
 */
void
Restart(void) {
  FirmwareVector *SPB_TV = SPB->TransferVector;
  (*SPB_TV->Restart)();
}

/********
 * Reboot
 */
void
Reboot(void) {
  FirmwareVector *SPB_TV = SPB->TransferVector;
  (*SPB_TV->Reboot)();
}

/******
 * halt
 * ----
 * Ready to power down the system, reset
 * the system to fix any uncertainty of the
 * IO subsystem.
 */
void
halt(void) {
  if (!isGuiMode()) {
    printf ("\n\nOkay to power off the system now.\n\n"
            "Press RESET button to restart.");
    getchar();
    putchar('\n');
  } else {
    if (doGui()) setGuiMode(1,GUI_NOLOGO);
    popupDialog("\n\nOkay to power off the system now.\n\n"
                "Press RESET button to restart.",
                0,DIALOGINFO,DIALOGCENTER|DIALOGBIGFONT);
    /* do not need to wait for power down tune since we spin here */
  }
  cpu_hardreset();
}

/***********
 * PowerDown
 */
void
powerdown(void) {
  cpu_powerdown();
}

/*********
 * Restart
 * -------
 * reset the system to fix any uncertainty of the
 * IO subsystem, specially related to PCI/SCSI IO.
 */
void
restart(void) {
#if 0  
  printf ("\nOkay to power off the machine");
  printf ("\nPress any key to restart ");
  getchar();
  putchar('\n');
#else
  static int restart_buttons[] = {DIALOGRESTART,-1};
  if (doGui()) setGuiMode(1,GUI_NOLOGO);
  popupDialog("\n\nOkay to power off the system now.\n\n"
              "Press any key to restart.",
              restart_buttons,DIALOGINFO,
              DIALOGCENTER|DIALOGBIGFONT);
  if (doGui()) setGuiMode(0,GUI_RESET);
#endif  
  
  /*
   * Try to autoboot.
   */
  startup();
  _main();      /* EnterInteractiveMode if no autoboot. */
}

/********
 * Reboot
 * ------
 * reset the system to fix any uncertainty of the
 * IO subsystem, specially related to PCI/SCSI IO.
 */
void
reboot(void) {
#if 0  
  printf("\nReboot the system\n");
  if (ESUCCESS != autoboot(0, 0, 0)) {
    p_printf("Unable to boot; press any key to continue: ");
    getchar();
    putchar('\n');
  }
#else
  if (!Verbose && doGui()) {
    setGuiMode(1,0);
  } else {
    p_panelmode();
    p_cursoff();
  }
  if (ESUCCESS != autoboot(0, 0, 0)) {
    putchar('\n');
    setTpButtonAction(EnterInteractiveMode,TPBCONTINUE,WBNOW);
    p_curson();
    p_printf("Unable to boot; press any key to continue: ");
    getchar();
    putchar('\n');
  }
#endif

  /*
   * If reboot fails, reinitialize software in case a partially
   * booted program has messed up the prom state.
   */
  EnterInteractiveMode();		/* should not get here */
}

/*******************
 * IP32firmware_init
 * -----------------
 * Implement the arcs init command.
 */
int
IP32firmware_init(void)
{
#if 1
  (*(void(*)(int,int))0xbfc00000)(FW_INIT,0);
#else 
  (*(void(*)(int,int))0xbfc00000)(FW_EIM,0);
#endif
}

/*****************************************************************************
 *                              fmisc                                        *
 *****************************************************************************/

/***************
 * fw_dispatcher
 * -------------
 * Handles miscellaneous unix callback functions
 * -------------
 * Called from top level firmware dispatch to handle non-reset entries (as
 * from the preceeding routines).
 * Note: Do not change the order of the case statements.
 */
void
fw_dispatcher(int dispatchCode) {
#if 0
  printf ("The dispatchCode = 0x%x\n", dispatchCode);
#endif
  switch(dispatchCode) {
  case FW_HARD_RESET:
  case FW_SOFT_RESET:
    playTune(0);    /* Play hello tune      */
  case FW_INIT:     /* Don't play tune for init. */
    startup();      /* We will enter the fw main by fall through the case
                       FW_EIM statement    */
  case FW_EIM:     
    _main();
    break;
  case FW_HALT:
    playTune(1);    /* Play goodbye tune    */
    halt();
    break;
  case FW_POWERDOWN:
    playTune(1);    /* Play goodbye tune    */
    powerdown();
    break;
  case FW_RESTART:
    playTune(1);    /* Play goodbye tune    */
    restart();
    break;
  case FW_REBOOT:
#if 0               /* Don't play tune for system reboot - init 6   */
    playTune(1);    /* Play goodbye tune    */
#endif
    reboot();
    break;
  default:
    return;
    break;
  }
  assert(IMPOSSIBLE);
  while(1);
}

