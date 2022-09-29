#include <crime.h>

#if 0
/*
 * Part of this was defined in <sys/types.h> but not all of it,
 * and that really sucks.
 * In bonsai tree all are defined, but just for the backward compatability
 * lets do the define checking here.
 */
#if !defined(uint8_t)
typedef	unsigned char		uint8_t;
#endif
#if !defined(uint16_t)
typedef	unsigned short		uint16_t;
#endif
#if !defined(uint32_t)
typedef	unsigned int		uint32_t;
#endif
#if !defined(int64_t)
typedef	signed long long int	int64_t;
#endif
#if !defined(uint64_t)
typedef	unsigned long long int	uint64_t;
#endif
#if !defined(intmax_t)
typedef signed long long int	intmax_t;
#endif
#if !defined(uintmax_t)
typedef unsigned long long int	uintmax_t;
#endif
#if !defined(intptr_t)
typedef signed long int		intptr_t;
#endif
#if !defined(uintptr_t)
typedef unsigned long int	uintptr_t;
#endif
#endif

#define K1      0xa0000000
#define L00     0
#define L01     4
#define L10     (32*1024*1024)-8
#define L11     (32*1024*1024)-4
#define L20     32*1024*1024
#define L21     (32*1024*1024)+4
#define L30     (128*1024*1024)-8
#define L31     (128*1024*1024)-4
#define D00     0xffffffff
#define D01     0x00000000
#define D10     0xfe000007
#define D11     0x01fffff8
#define D20     0xfdffffff
#define D21     0x02000000
#define D30     0xf8000007
#define D31     0x07fffff8
#define NoMEM   0x0000011f
#define LoMEM   256*1024*1024
#define CRMBANKCTRLBASE CRM_MEM_BANK_CTRL(0)

#define V32M    32*1024*1024
#define V128M   128*1024*1024
#define P16M    16*1024*1024
#define P16Minc (P16M>>6)

extern  void        printf          (char *, ...)                   ;
extern  void        WritePGMSK      (uint32_t)                      ;
extern  void        TLBidxWrite     (int)                           ;
extern  void        TLBidxRead      (int)                           ;
extern  void        WriteTLBPkg     (uint32_t, uint32_t, uint32_t)  ;
extern  uint32_t    ReadEntryLO0    (void)                          ;
extern  uint32_t    ReadEntryLO1    (void)                          ;
extern  uint32_t    ReadEntryHi     (void)                          ;


/*********
 * PrintBANKinfo(int *)
 *--------
 *  print the bank control array.
 *--------
 *  Return nothing
 */
void
PrintBANKinfo(int *bank) 
{
  int   *ptr ;

  ptr = bank ; 
  printf ("<post1> <SizeMEM>   \n"
          "        Bank0 = %dM \n"
          "        Bank1 = %dM \n"
          "        Bank2 = %dM \n"
          "        Bank3 = %dM \n"
          "        Bank4 = %dM \n"
          "        Bank5 = %dM \n"
          "        Bank6 = %dM \n"
          "        Bank7 = %dM \n", ptr[0], ptr[1], ptr[2], ptr[3],
          ptr[4], ptr[5], ptr[6], ptr[7]) ;
  
  return ;
}

/*********
 * UNmap128Mpage
 *--------
 *  Invalidate previous mapped `128M pages.
 *--------
 *  Return nothing
 */
void
UNmap128Mpage(void)
{
  int       idx ;
  uint32_t  Lo0, Lo1, EntyHi ;

  WritePGMSK(0x0)           ;   /* Create a 16M pages.  */
  
  for (idx = 0 ; idx < 4 ; idx += 1) {
    TLBidxRead(idx) ;
    Lo0 = ReadEntryLO0 () ;
    Lo1 = ReadEntryLO1 () ;
    EntyHi = ReadEntryHi () ;
    Lo0 = Lo0 & ~0x2 ; 
    Lo1 = Lo1 & ~0x2 ;
    WriteTLBPkg(EntyHi, Lo0, Lo1) ;
    TLBidxWrite(idx) ;
  }
  return ;
}


/*********
 * map128Mpage
 *--------
 *  16M/tlb, we need 4 pairs of tlb entries to 
 *  complete a 128M memory bank,
 *--------
 *  Return nothing
 */
void
map128Mpage(uint32_t VA)
{
  int       idx ;
  uint32_t  viraddr, phyaddr ; 
  uint32_t  Lo0, Lo1, EntyHi ; 

  WritePGMSK(0x01ffe000)    ;   /* Create a 16M pages.  */
  viraddr = VA              ;   /* starting Virtual.    */
  phyaddr = 0x40000000 + VA ;   /* starting Physical.   */
  
  for (idx = 0 ; idx < 4 ; idx += 1) {
    EntyHi = viraddr & ~0x1fff ; 
    Lo0 = (phyaddr >> 6) | 0x17 ;
    Lo1 = Lo0 + P16Minc ;
    WriteTLBPkg(EntyHi, Lo0, Lo1) ;
    TLBidxWrite(idx) ;
    viraddr += V32M ;
    phyaddr += V32M ;
    
  }
  return ;
}

  
/*********
 * sizemem
 *--------
 *  There are two possible memory configuration - 
 *      32 Mbyte/bank or 128Mbyte/bank
 *  The memory probe were executed out of PROM, and assume the 
 *  sloader stack are still in good condition.
 *--------
 *  Return the size of the memory, in Bytes.
 */
#define DeBug
uint32_t
SizeMEM()
{
  uint32_t  CRIME_REG_ADDR, mem_addr ;
  uint64_t  tmp ;
  int       banknum, bankidx ;
  int       bank[8] ;

#if defined(DeBug)
  printf ("<post1> <SizeMEM> Entering routine\n") ;
#endif

  /*
   * Clear the bank stuff.
   */
  for (bankidx = 0 ; bankidx < 8 ; bankidx += 1) bank[bankidx] = 0 ;
#if defined(DeBug)
  PrintBANKinfo(bank) ;
#endif
  
  /*
   * Assume nothing, turn off ECC checking first.
   */
  CRIME_REG_ADDR = (uint32_t) (K1|CRM_CONTROL) ;
  tmp = * (uint64_t *) CRIME_REG_ADDR ;
  * (uint64_t *) CRIME_REG_ADDR = tmp & (~CRM_MEM_CONTROL_ECC_ENA) ;

#if defined(DeBug)
  printf ("<post1> <SizeMEM> ECC off \n") ;
#endif
  
  /*
   * Initialize all bank to bank 0, and assume we have the 128M simm.
   * then bank1, ...
   */
  mem_addr  = 0 ;        /* memory address start at physical 0   */
  bankidx   = 0 ;
  CRIME_REG_ADDR = (uint32_t) (K1|CRMBANKCTRLBASE) ;
  while (bankidx < 8) {

#if defined(DeBug) 
    printf ("<post1> <SizeMEM> Initilaize BANK%d\n", bankidx) ;
#endif

    /*
     * Map in current bank virtual address range.
     */
    map128Mpage(mem_addr) ;
    
    /*
     * make all the following point to current bank.
     */
    for (banknum = bankidx ; banknum < 8 ; banknum += 1) {
      /*    if (mem_addr >= LoMEM) mem_addr = mem_addr | 0x40000000 ; */
      * (uint64_t *) (CRIME_REG_ADDR + (uint32_t)(banknum << 3)) =
        (uint64_t) (((mem_addr >> 25) & 0x1f) | CRM_MEM_BANK_CTRL_SDRAM_SIZE) ;
    }

#if defined(DeBug)
    printf ("<post1> <SizeMEM> mem_addr = 0x%0x\n", mem_addr) ;
#endif
      
    /*
     * Write to following address -
     *   (mem_addr + 0, (mem_addr + 32M - 8, (mem_addr + 32M
     *   (mem_addr + 128M -8.
     */
    * (int32_t *)(mem_addr + L00) = D00 ;
    * (int32_t *)(mem_addr + L10) = D10 ; 
    * (int32_t *)(mem_addr + L20) = D20 ; 
    * (int32_t *)(mem_addr + L30) = D30 ;

    * (int32_t *)(mem_addr + L01) = D01 ;
    * (int32_t *)(mem_addr + L11) = D11 ; 
    * (int32_t *)(mem_addr + L21) = D21 ; 
    * (int32_t *)(mem_addr + L31) = D31 ;

    /*
     * following writes should clear the memory bus.
     */
    * (int32_t *)(mem_addr + 0x10) = 0xdeadbeef ;
    * (int32_t *)(mem_addr + 0x14) = 0xbeefdead ; 
    * (int32_t *)(mem_addr + 0x18) = 0x5a5a5a5a ; 
    * (int32_t *)(mem_addr + 0x20) = 0xa5a5a5a5 ; 
    
    /*
     * Read it back compare the result and determind the size
     * of current SIMM.
     */
    if ((* (int32_t *)(mem_addr + L00) == (int32_t)D20) &&
        (* (int32_t *)(mem_addr + L01) == (int32_t)D21) &&
        (* (int32_t *)(mem_addr + L10) == (int32_t)D30) &&
        (* (int32_t *)(mem_addr + L11) == (int32_t)D31) &&
        (* (int32_t *)(mem_addr + L20) == (int32_t)D20) &&
        (* (int32_t *)(mem_addr + L21) == (int32_t)D21) &&
        (* (int32_t *)(mem_addr + L30) == (int32_t)D30) &&
        (* (int32_t *)(mem_addr + L31) == (int32_t)D31)) {
      
      /* We have a 32M  bank on hand, the mem control need get  */
      /* modified to reflect the fact, we di it in second pass  */
      /* leave it as it for now but update the bank indicator.  */
      bank[bankidx] = 32 ;
      
#if defined(DeBug) 
      printf("<post1> <SizeMEM> bank%d =  0x%lx (32M)\n",
         bankidx, *(uint64_t*)(CRIME_REG_ADDR + (uint32_t)(bankidx << 3))) ;
#endif

    } else {
      if ((* (int32_t *)(mem_addr + L00) == (int32_t)D00) &&
          (* (int32_t *)(mem_addr + L01) == (int32_t)D01) &&
          (* (int32_t *)(mem_addr + L10) == (int32_t)D10) &&
          (* (int32_t *)(mem_addr + L11) == (int32_t)D11) &&
          (* (int32_t *)(mem_addr + L20) == (int32_t)D20) &&
          (* (int32_t *)(mem_addr + L21) == (int32_t)D21) &&
          (* (int32_t *)(mem_addr + L30) == (int32_t)D30) &&
          (* (int32_t *)(mem_addr + L31) == (int32_t)D31)) {
        
        /* We have a 128M bank on hand, set the bank indicator  */
        /* accordingly.                                         */
        bank[bankidx] = 128 ;
#if defined(DeBug)   
        printf("<post1> <SizeMEM> bank%d = 0x%0lx (128M)\n",
           bankidx, *(uint64_t*)(CRIME_REG_ADDR + (uint32_t)(bankidx << 3))) ;
#endif  
      } else {
        if (bankidx == 0) {
          printf ("Error, no SIMM in bank0\n") ;
          return (-100) ;
        } else {
          /* No memory found in current bank.                   */
          bank[bankidx] = 0 ;
#if defined(DeBug)     
          printf ("<post1> <SizeMEM> bank%d = 0x%0lx, no simm installed\n",
             bankidx, *(uint64_t*)(CRIME_REG_ADDR + (uint32_t)(bankidx << 3)));
#endif    
        }
      }
    }
    bankidx  += 1   ;   /* Do next bank.    */
    mem_addr += V128M ; /* 128M increment.  */
  }

  /*
   * Print the SIMM info we have found so far.
   */
#if defined(DeBug)
  PrintBANKinfo(bank) ;
#endif

  /*
   * Now proceed with the second pass, find all the 128M bank and assign
   * then to lower physical address.
   */
  mem_addr = 0x0 ; 
  for (bankidx = 0 ; bankidx < 8 ; bankidx += 1) {
    if (bank[bankidx] == 128) {
      bank[bankidx] = -1 ;
      * (uint64_t *) (CRIME_REG_ADDR + (uint32_t)(bankidx << 3)) =
          (uint64_t) (((mem_addr >> 25) & 0x1f) |
                      CRM_MEM_BANK_CTRL_SDRAM_SIZE) ;
      mem_addr += V128M ;
    }
  }

  /*
   * Print the SIMM info we have found so far.
   */
#if defined(DeBug)
  PrintBANKinfo(bank) ;
#endif
  
  /*
   * Third pass - taking care of those 32M SIMMs.
   */
  for (bankidx = 0 ; bankidx < 8 ; bankidx += 1) {
    if (bank[bankidx] == 32) {
      bank[bankidx] = -1 ;
      * (uint64_t *) (CRIME_REG_ADDR + (uint32_t)(bankidx << 3)) =
          (uint64_t) ((mem_addr >> 25) & 0x1f) ;
      mem_addr += V32M ;
    }
  }

  /*
   * Print the SIMM info we have found so far.
   */
#if defined(DeBug)
  PrintBANKinfo(bank) ;
#endif

  /*
   * Final pass - taking care of those bank which do not have SIMM
   */
  tmp = * (uint64_t *) CRIME_REG_ADDR ;
  for (bankidx = 0 ; bankidx < 8 ; bankidx += 1) {
    if (bank[bankidx] == 0) {
      bank[bankidx] = -1 ;
      * (uint64_t *) (CRIME_REG_ADDR + (uint32_t)(bankidx << 3)) = tmp ; 
    }
  }
    
  /*
   * print the memory size.
   */
  printf ("\n\n Found %d bytes memory\n", mem_addr) ;
           
  /*
   * We done!
   */
  UNmap128Mpage() ;
  return (mem_addr) ;
}


