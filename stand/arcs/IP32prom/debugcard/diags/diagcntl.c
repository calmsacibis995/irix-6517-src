#include <inttypes.h>
#include <debugcard.h>
#include <utilities.h>
#include <asm_support.h>
#include <st16c1451.h>
#include <uartio.h>
#include <Dflash.h>
#include <diags.h>	
#include <sys/regdef.h>
#include <inttypes.h>
#include <post1supt.h>

#define ESC     '\033'
#define CTLC    '\003'
#define NL      '\012'
#define CR      '\015'

/************
 * External functions prototype.
 *-----------
 */
extern void MemAddrTST          (void) ;
extern void unexpected_intr     (void) ;
extern void enable_interrupt    (int)  ;

/************
 * diagnostic tests prototype.
 *-----------
 */
void        setUPmh             (void) ;    
void        unregist_interrupt  (void) ;    
int         endOFdiags          (void) ;
int         mace_serial_test    (void) ;
int         mem_BEtst           (void) ;
int         mem_addr1           (void) ;
int         mem_addr2           (void) ;
int         mem_addr3           (void) ;
int         mem_addr4           (void) ;
int         mem_addr5           (void) ;
int         mem_ECCtst10        (void) ;
int         mem_ECCtst12        (void) ;
int         mem_ECCtst13        (void) ;
int         mem_ECCtst14        (void) ;
int         mem_ECCtst21        (void) ;
int         mem_ECCtst22        (void) ;
int         mem_ECCtst3         (void) ;
int         mem_ECCtst4         (void) ;
int         mem_ECCtst5         (void) ;
int         mem_ECCtst6         (void) ;
int         mem_ECCtst7         (void) ;
int         mem_ECCtst8         (void) ;
int         mem_ECCtst9         (void) ;
int         mem_REGtst          (void) ;
int         mem_ECCtst111       (void) ;

/************
 * Test table
 *-----------
 * for every available diag must have an entry in the
 * following table, and endOFdiags must be the LAST one in the
 * table.
 *-----------
 */
static struct DIAGTAB {
  int   (*diag)() ;
  char  *diagname ;
} diagtab[] = {
  {mem_BEtst,     "mem_BEtst: cpu different size access."},
  {mem_REGtst,    "mem_REGtst:    REG test.   "},
  {mem_ECCtst10,  "mem_ECCtst10:  ECC test  1."},
  {mem_ECCtst12,  "mem_ECCtst12:  ECC test  2."},
  {mem_ECCtst13,  "mem_ECCtst13:  ECC test  3."},
  {mem_ECCtst14,  "mem_ECCtst14:  ECC test  4."},
  {mem_ECCtst21,  "mem_ECCtst21:  ECC test  5."},
  {mem_ECCtst22,  "mem_ECCtst22:  ECC test  6."},
  {mem_ECCtst3,   "mem_ECCtst3:   ECC test  7."},
  {mem_ECCtst4,   "mem_ECCtst4:   ECC test  8."},
  {mem_ECCtst5,   "mem_ECCtst5:   ECC test  9."},
  {mem_ECCtst6,   "mem_ECCtst6:   ECC test 10."},
  {mem_ECCtst7,   "mem_ECCtst7:   ECC test 11."},
  {mem_ECCtst8,   "mem_ECCtst8:   ECC test 12."},
  {mem_ECCtst9,   "mem_ECCtst9:   ECC test 13."},
  {mem_ECCtst111, "mem_ECCtst111: ECC test 14."},
  {mem_addr1,     "mem_addr1: mem address tst1"},
  {mem_addr2,     "mem_addr2: mem address tst2"},
  {mem_addr4,     "mem_addr4: mem address tst4"},
  {mem_addr5,     "mem_addr5: mem address tst5"},
  {endOFdiags,    "End of diags."},
  {mem_addr3,     "mem_addr3: mem address tst3"},
  {0, "\0"}
} ;

/****************
 * diag_error_msg
 *---------------
 * print diagnostic error messages then return or loop on error.
 *---------------
 */
void
diag_error_msg (uint64_t *addr, uint64_t *observ, uint64_t* expect,
                uint32_t* pc)
{
  uint64_t  expected = *expect ;
  uint64_t  observed = *observ ;

  printf ("\n"
          "Error:       error pc = 0x%0x\n"
          "        Test addredss = 0x%0x\n"
          "        Expected data = 0x%0lx\n"
          "        Observed data = 0x%0lx\n",
          pc, addr, expected, observed) ;
  unexpected_intr();
  return ;
          
}

/************
 * Dispatcher
 *-----------
 * Initialize and dispatch diagnostic one by one.
 *-----------
 */
void
Dispatcher () 
{
  volatile int32_t      result, idx ;
  volatile uint64_t     *FCNTL = (uint64_t*)FLASH_CNTL ;
  
  /*
   * Initialize some diag global control variables.
   */
  printf ("\n\n --- Initialize the 4600/DebugCard petty_crime DVT EXEC environment ---\n\n") ;
  *FCNTL &= ~0x30 ;     /* Turn both led on.    */

  /*
   * Loop through the diag testtable and exec all the
   * available diags.
   */
  for (idx=0, result=0;;) {
    setUPmh () ;
    printf ("DVT[%d]\t%s", idx+1, diagtab[idx].diagname) ;
    switch (result=(int32_t)(*diagtab[idx].diag)()) {
    case 0:
      *FCNTL |= 0x20 ;     /* Turn off Green LED */
      printf ("\nDVT[%d]\t%s [FAILED]\n", idx+1, diagtab[idx].diagname) ;
      idx += 1 ;
      break ;
    case 1:
      printf (" [PASSED]\n");
      idx += 1 ;
      break;
    default:
      printf ("\n\nEND OF THE PETTY CRIME DVT\n") ;
      if (!(*FCNTL&0x30)) *FCNTL |= 0x10;  /* Turn off red LED only if
                                              there are no test failed  */
      return ;
    }
  }
}
    

#if defined(DIAGScompleted)
/************
 * LoopOnTest   
 *-----------
 * Check the loop on test variable to determind
 * whether or not we should loop on the current test.
 *-----------
 */
int
LoopOnTest ()
{
  int result ;

  result = 0 ; 
  if (LOT) result = LOT ; 
  else
    if (tab[CurrentTest].testLoop) result = tab[CurrentTest].testLoop ;

  return result ; 
}
    

/*************
 * LoopOnError  
 *------------
 * Check the loop on Error variable to determind
 * whether or not we should loop on error
 *-----------
 */
int
LoopOnError ()
{
  int result ;

  result = 0 ; 
  if (LOE) result = LOT ; 
  else
    if (tab[CurrentTest].errorLoop) result = tab[CurrentTest].errorLoop ;

  return result ; 

}


/***********
 * DiagBreak
 *----------
 * Check control C from UART.
 *-----------
 */
int
DiagBreak ()
{
  int   c ;

  switch (c = getchar()) {
  case ESC:
  case CTLC:
    printf ("\n\nCurrent test aborted!!!\n") ;
    LOT = LOE = 0 ; 
    return 1 ;
  case 'L':
  case 'l':
    printf ("\n\nLoop on curent test!!!\n") ;
    LOT = 1 ;
    break ;
  case 'E':
  case 'e':
    printf ("\n\nLoop on error!!!\n") ;
    LOE = 1 ;
    break ;
  default:
    break ;
  }
  return 0 ;
}

#endif
/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/




/*
 *  Function name: setUPmh
 *  Description:   return 1 to indicate that the diags exec 
 *                 environment is in place.
 */
void
setUPmh (void)
{
  register uint64_t  CrimeREG ;
  register uint32_t  addrptr, tmp ;
  register int       indx, enty ; 
    
  /*
   * [1] Disable processor interrupt.
   */
  CPUinterruptOFF (0x0, 0x0, 0x0) ;
  
  /*
   * [2] Turn off ECC checking.
   */
  * ((uint64_t *)(MEMcntlBASE|MEMCntl)) = 0x0;
  
  /*
   * [3] Turn off interrupts in Crime.
   */
  * ((uint64_t *)(CrimeBASE|CrimeIntrEnable)) = 0 ; 
  * ((uint64_t *)(CrimeBASE|CrimeSoftIntr))   = 0 ; 
  * ((uint64_t *)(CrimeBASE|CrimeHardIntr))   = 0 ; 
  * ((uint64_t *)(CrimeBASE|CPUErrENABLE))    = 0 ; 

  /*
   * [4] and zap the interrupt handler which was registed by previous
   *     diagnostic.
   */
  unregist_interrupt () ;

  /*
   * [5] Invalidate all tlb entries.
   */
  addrptr = 0x0 ;
  tmp = 0x15 ; 
  enty = TLBentries (0x0) ;
  mtc0_pagemask(0x0) ;      /* setup 4K pages.  */
  for (indx = 0 ; indx < enty ; indx += 1) {
    mtc0_index (indx) ;
    mtc0_entryhi  (addrptr) ;
    mtc0_entrylo0 (tmp)  ;
    mtc0_entrylo1 (tmp += 0x40) ; 
    tlb_write () ;
    addrptr += 0x2000 ;
    tmp += 0x40 ; 
  }

  /*
   * [6] We done.
   */
  
#if defined(Debug)  
  printf ("setUPmh: setting up diags exec environment\n") ;
#endif
  
  return ;
}

/*
 *  Function name: endOFdiags
 *  Description:   return -1 to indicate this is the end of the 
 *                 post1 testtable.
 */
int
endOFdiags (void)
{
  /*
   * Return -1 to indicate this is the end of post1diags.
   */
  return 0x1000;
}

/*
 *  Function name: pcache_init (paddr, lines)
 *  Description:   Initialize the primary cache to it address.
 */
void
pcache_init (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count, dword ;
    uint32_t    Paddr, testaddr, taglo, tmp ;
    uint64_t    data ;

    printf ("<post1> <pcache_init> Enter routine.\n") ;
    
    for (Paddr = paddr, count = 0 ; count < lines ; Paddr += 0x20, count++) {
      mtc0_taglo ((Paddr >> 12) << 8) ; 
      pcache_index_store_tag (KSEG0|Paddr) ;
      pcache_create_dirty_exclusive (KSEG0|Paddr) ;
      dword = 0 ;
      while (dword < 0x20) {
        testaddr = Paddr | dword ; 
        data = (uint64_t) (~(testaddr ^ key)) ;
        data = (data << 32) | (testaddr ^ key) ;
        * (uint64_t *) (KSEG0|testaddr) = data ;
        dword += 0x8 ;
      }
    }
    printf ("<post1> <pcache_init> Leaving routine.\n") ;
}


/*
 *  Function name: caches_init (paddr, lines)
 *  Description:   Initialize the primary/seconadry cache to it address.
 */
void
scache_init (uint32_t paddr, int32_t lines, uint32_t key)
{
    uint32_t    Paddr ;
    uint64_t    data ;
    int32_t     count ;

    printf ("<post1> <scache_init> Enter routine.\n") ;
    pcache_init (paddr, lines, key) ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        pcache_hit_wb_invalid (KSEG0|Paddr) ;
        Paddr += 0x20 ; 
    }
    printf ("<post1> <scache_init> Leaving routine.\n") ;
}


/*
 *  Function name: mem_init (paddr, lines)
 *  Description:   Initialize the memory without destroy caches.
 */
void
mem_init (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count ;
    uint32_t    Paddr, dword, testaddr ;
    uint64_t    data ;

    printf ("<post1> <mem_init> Enter routine.\n") ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        dword = 0 ;
        while (dword < 0x20) {
            testaddr = Paddr | dword ; 
            data = (uint64_t) (~(testaddr ^ key)) ;
            data = (data << 32) | (testaddr ^ key) ;
            * (uint64_t *) (KSEG1|testaddr) = data ;
            dword += 0x8 ;
        }
    }
    printf ("<post1> <mem_init> Leaving routine.\n") ;
}


/*
 *  Function name: pcache_verify (paddr, lines)
 *  Description:   Verify the cache line with its unique data pattern.
 */
void
pcache_verify (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count, dword ;
    uint32_t    Paddr, testaddr, taglo, expected_taglo ;
    uint64_t    expected_data, data ;

    printf ("<post1> <pcache_verify> Entering routine.\n") ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        pcache_index_load_tag (KSEG0|Paddr) ;
        expected_taglo = ((Paddr >> 12) << 8) | 0xc0 ;
        taglo = (mfc0_taglo() >> 5) << 5 ;
        if (taglo != expected_taglo) {
            printf ("FIXED ME");
        }
        
        dword = 0 ;
        while (dword < 0x20) {
            testaddr = Paddr | dword ; 
            expected_data = (uint64_t) (~(testaddr ^ key)) ;
            expected_data = (expected_data << 32) | (testaddr ^ key) ;
            data = * (uint64_t *) (KSEG0|testaddr) ;
            if (data != expected_data) {
            printf ("FIXED ME");
            }
            dword += 0x8 ;
        }
    }
    printf ("<post1> <pcache_verify> Leaving routine.\n") ;
}


/*
 *  Function name: scache_verify (paddr, line, key)
 *  Description:   verify the secondary cache line without destroy
 *                 the memory content.
 */
void
scache_verify (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count, dword ;
    uint32_t    Paddr, testaddr, expected_taglo ;
    uint64_t    expected_data, data ;

    printf ("<post1> <scache_verify> Entering routine.\n") ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        scache_index_load_tag (KSEG0|Paddr) ;
        expected_taglo = ((Paddr >> 19) << 15) | 0x1000 ;
        if (mfc0_taglo() != expected_taglo) {
            printf ("FIXED ME");
        }
        mtc0_taglo ((Paddr >>12) << 8) ;
        pcache_index_store_tag (KSEG0|Paddr) ;
        dword = 0 ;
        while (dword < 0x20) {
            testaddr = Paddr | dword ; 
            expected_data = (uint64_t) (~(testaddr ^ key)) ;
            expected_data = (expected_data << 32) | (testaddr ^ key) ;
            data = * (uint64_t *) (KSEG0|testaddr) ;
            if (data != expected_data) {
            printf ("FIXED ME");
            }
            dword += 0x8 ;
        }
    }
    printf ("<post1> <scache_verify> Leaving routine.\n") ;
}


/*
 *  Function name: mem_verify (paddr, line, key)
 *  Description:   verify the memory 
 */
void
mem_verify (uint32_t paddr, int32_t lines, uint32_t key)
{
    int32_t     count, dword ;
    uint32_t    Paddr, testaddr ;
    uint64_t    expected_data, data ;

    printf ("<post1> <mem_verify> Entering routine.\n") ;
    for (Paddr = paddr, count = 0 ; count < lines ; count++) {
        dword = 0 ;
        while (dword < 0x20) {
            testaddr = Paddr | dword ; 
            expected_data = (uint64_t) (~(testaddr ^ key)) ;
            expected_data = (expected_data << 32) | (testaddr ^ key) ;
            data = * (uint64_t *) (KSEG1|testaddr) ;
            if (data != expected_data) {
            printf ("FIXED ME");
            }
            dword += 0x8 ;
        }
    }
    printf ("<post1> <mem_verify> Leaving routine.\n") ;
}
    

/*
 * -------------------------------------------------------------
 *
 * $Log: diagcntl.c,v $
 * Revision 1.1  1997/08/18 20:39:51  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.6  1996/10/31  21:50:10  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.7  1996/10/04  21:08:41  kuang
 * Cleanup the sources list
 *
 * Revision 1.6  1996/10/04  20:08:57  kuang
 * Fixed some general problem in the diagmenu area
 *
 * Revision 1.5  1996/04/13  01:26:17  kuang
 * Cleanup a bit more ...
 *
 * Revision 1.4  1996/04/13  00:56:31  kuang
 * minor changes to the diags control structure
 *
 * Revision 1.3  1996/04/12  02:15:21  kuang
 * Now ECC tests run with R4600/DebugCard environment
 *
 * Revision 1.2  1996/04/04  23:33:43  kuang
 * Added more diagnostic supports
 *
 * Revision 1.1  1995/12/30  03:50:38  kuang
 * moosehead bringup 12-29-95 d15 checkin
 *
 * Revision 1.2  1995/12/26  22:24:14  philw
 * add serial port test to post1
 *
 * Revision 1.1  1995/11/28  02:17:00  kuang
 * General cleanup
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
