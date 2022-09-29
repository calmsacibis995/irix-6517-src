/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/cpu.h>
#include <sys/mace.h>
#include <sio.h>
#include <TL16550.h>
#include <sys/crime.h>
#define POST1KSEG1


/*
 * Part of this was defined in <sys/types.h> but not all of it,
 * and that really sucks.
 */
#if 0
typedef	unsigned char		uint8_t;
typedef	unsigned short		uint16_t;
typedef	unsigned int		uint32_t;
typedef	signed long long int	int64_t;
typedef	unsigned long long int	uint64_t;
typedef signed long long int	intmax_t;
typedef unsigned long long int	uintmax_t;
typedef signed long int		intptr_t;
typedef unsigned long int	uintptr_t;
#endif

#define K1      0xa0000000
#define L0      0
#define L1      (32*1024*1024)-8
#define L2      32*1024*1024
#define L3      (128*1024*1024)-8
#define L4      128*1024*1024
#define D0      0xffffffff00000000LL
#define D1      0xfe00000701fffff8LL
#define D2      0xfdffffff02000000LL
#define D3      0xf800000707fffff8LL
#define NoMEM   0x11fLL
#define LoMEM   256*1024*1024
#define CRMBANKCTRLBASE CRM_MEM_BANK_CTRL(0)

#define V32M    32*1024*1024
#define P16M    16*1024*1024
#define P16Minc (P16M>>6)

extern  void        DPRINTF         (char *, ...)                   ;
extern  void        SimpleMEMtst    (uint32_t, uint32_t)            ;
extern  void        CacheWriteBack  (uint32_t)                      ;
extern  void        CacheWriteBackInvalidate    (uint32_t)          ;
extern  void        WritePGMSK      (uint32_t)                      ;
extern  void        TLBidxWrite     (int)                           ;
extern  void        TLBidxRead      (int)                           ;
extern  void        WriteTLBPkg     (uint32_t, uint32_t, uint32_t)  ;
extern  uint32_t    ReadEntryLO0    (void)                          ;
extern  uint32_t    ReadEntryLO1    (void)                          ;
extern  uint32_t    ReadEntryHi     (void)                          ;


/*****************
 * 1 ms delay loop
 *----------------
 */
void
delay(int count)
{
  volatile counters_t * pCounters = (counters_t *)COUNTERS;
  unsigned long long	tmp;

  tmp = pCounters->ust;
  tmp += (long long)count;
  while(tmp > pCounters->ust);
}

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
  DPRINTF ("<post1> <SizeMEM>   \n"
           "        Bank0 = %dM \n"
           "        Bank1 = %dM \n"
           "        Bank2 = %dM \n"
           "        Bank3 = %dM \n"
           "        Bank4 = %dM \n"
           "        Bank5 = %dM \n"
           "        Bank6 = %dM \n"
           "        Bank7 = %dM \n",
           ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
           
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

uint32_t
SizeMEM()
{
  uint32_t  CRIME_REG_ADDR, mem_addr ;
  int       banknum, bankidx ;
  int       bank[8] ;
  uint64_t  tmp, *MACE_LEDreg=(uint64_t*)(0xa0000000|ISA_FLASH_NIC_REG);

  DPRINTF ("<post1> <SizeMEM> Entering routine\n") ;  

  /*
   * Clear the bank stuff.
   */
  for (bankidx = 0 ; bankidx < 8 ; bankidx += 1) bank[bankidx] = 0 ;
  PrintBANKinfo(bank) ;  
  
  /*
   * Assume nothing, turn off ECC checking first.
   */
  CRIME_REG_ADDR = (uint32_t) (K1|CRM_CONTROL) ;
  tmp = * (uint64_t *) CRIME_REG_ADDR ;
  * (uint64_t *) CRIME_REG_ADDR = tmp & (~CRM_MEM_CONTROL_ECC_ENA) ;

  DPRINTF ("<post1> <SizeMEM> ECC off \n") ;
  
  /*
   * Initialize all bank to bank 0, and assume we have the 128M simm.
   * then bank1, ...
   */
  mem_addr  = 0 ;        /* memory address start at physical 0   */
  bankidx   = 0 ;
  CRIME_REG_ADDR = (uint32_t) (K1|CRMBANKCTRLBASE) ;
  while (bankidx < 8) {
    
    DPRINTF ("<post1> <SizeMEM> Initilaize BANK%d\n", bankidx) ;

    /*
     * Map in current bank virtual address range.
     */
    map128Mpage(mem_addr) ;
    
    /*
     * make all the following point to current bank.
     */
    for (banknum = bankidx ; banknum < 8 ; banknum += 1) {
      * (uint64_t *) (CRIME_REG_ADDR + (uint32_t)(banknum << 3)) =
        (uint64_t) (((mem_addr >> 25) & 0x1f) | CRM_MEM_BANK_CTRL_SDRAM_SIZE) ;
    }

    DPRINTF ("<post1> <SizeMEM> mem_addr = 0x%0x\n", mem_addr) ;
      
    /*
     * Write to following address -
     *   (mem_addr + 0, (mem_addr + 32M - 8, (mem_addr + 32M
     *   (mem_addr + 128M -8.
     */
    * (uint64_t *)(mem_addr + L0) = D0 ;
    * (uint64_t *)(mem_addr + L1) = D1 ; 
    * (uint64_t *)(mem_addr + L2) = D2 ; 
    * (uint64_t *)(mem_addr + L3) = D3 ;

    /*
     * this 3 write are used to clear the memory bus.
     */
    * (uint64_t *)(mem_addr + 0x10) = 0x0LL ;
    * (uint64_t *)(mem_addr + 0x18) = 0x0LL ;
    * (uint64_t *)(mem_addr + 0x20) = 0x0LL ;
    
    /*
     * Read it back compare the result and determind the size
     * of current SIMM.
     */
    if ((* (uint64_t *)(mem_addr + L0) == D2) &&
        (* (uint64_t *)(mem_addr + L1) == D3) &&
        (* (uint64_t *)(mem_addr + L2) == D2) &&
        (* (uint64_t *)(mem_addr + L3) == D3)) {
      
      /* We have a 32M  bank on hand, the mem control need get  */
      /* modified to reflect the fact, we di it in second pass  */
      /* leave it as it for now but update the bank indicator.  */
      bank[bankidx] = 32 ; 
      DPRINTF("<post1> <SizeMEM> bank%d =  0x%lx (32M)\n",
         bankidx, *(uint64_t*)(CRIME_REG_ADDR + (uint32_t)(bankidx << 3))) ;

    } else {
      if ((* (uint64_t *)(mem_addr + L0) == D0) &&
          (* (uint64_t *)(mem_addr + L1) == D1) &&
          (* (uint64_t *)(mem_addr + L2) == D2) &&
          (* (uint64_t *)(mem_addr + L3) == D3)) {
        
        /* We have a 128M bank on hand, set the bank indicator  */
        /* accordingly.                                         */
        bank[bankidx] = 128 ; 
        DPRINTF("<post1> <SizeMEM> bank%d = 0x%0lx (128M)\n",
           bankidx, *(uint64_t*)(CRIME_REG_ADDR + (uint32_t)(bankidx << 3))) ;
      } else {
        /*
         * Note:
         * Why we have to have memory in bank0 -
         *     [1] we always do.
         *     [2] if there are more then one bank controll register
         *         matches the given physical address CRIME will choose
         *         the first control register as the target memory bacnk.
         *     [3] CRIME will not report memory error if software access 
         *         an physically empty bank but the physical address matches
         *         the bank control register, actually CRIME will do exactly
         *         nothing about it and simply return whatever is left on
         *         its internal memory bus which will led software to
         *         wonder why it getting an incorrect data and make software
         *         debug a BIG headache (almost impossible).
         *     [4] by mapping all unused bank control registers to bank0
         *         would
         *         enable CRIME to report memory access error because there
         *         is no control register matches the given physical addr.
         */
        if (bankidx == 0) {
          DPRINTF ("Error, no SIMM in bank0\n") ;
          tmp = ISA_LED_RED|ISA_LED_GREEN;
          /* Turn off the led   */
          *MACE_LEDreg = *MACE_LEDreg | tmp;
          while(1) {
            /* Now blink the led - amber    */
            delay(500000); /* Wait for half second */
            *MACE_LEDreg = *MACE_LEDreg ^ tmp;
          }
          return (-100) ;
        } else {
          /* No memory found in current bank.                   */
          bank[bankidx] = 0 ; 
          DPRINTF ("<post1> <SizeMEM> bank%d = 0x%0lx, no simm installed\n",
             bankidx, *(uint64_t*)(CRIME_REG_ADDR + (uint32_t)(bankidx << 3)));
        }
      }
    }
    UNmap128Mpage() ;   /* Clean up the TLB */
    bankidx  += 1   ;   /* Do next bank.    */
    mem_addr += L4  ;   /* 128M increment.  */
  }

  /*
   * Print the SIMM info we have found so far.
   */
  PrintBANKinfo(bank) ;

  /*
   * Now proceed with the second pass, find all the 128M bank and assign
   * then to lower physical address.
   */
  mem_addr = 0x0 ; 
  for (bankidx = 0 ; bankidx < 8 ; bankidx += 1) {
    if (bank[bankidx] == 128) {
      * (uint64_t *) (CRIME_REG_ADDR + (uint32_t)(bankidx << 3)) =
          (uint64_t) (((mem_addr >> 25) & 0x1f) |
                      CRM_MEM_BANK_CTRL_SDRAM_SIZE) ;
      bank[bankidx] = -1 ;
      mem_addr += L4 ;
      DPRINTF ("<post1> <SizeMEM> bank%d = 128M mem_addr[next] = 0x%0x\n",
               bankidx, mem_addr) ; 
    }
  }

  /*
   * Print the SIMM info we have found so far.
   */
  PrintBANKinfo(bank) ;
  
  /*
   * Third pass - taking care of those 32M SIMMs.
   */
  for (bankidx = 0 ; bankidx < 8 ; bankidx += 1) {
    if (bank[bankidx] == 32) {
      * (uint64_t *) (CRIME_REG_ADDR + (uint32_t)(bankidx << 3)) =
          (uint64_t) ((mem_addr >> 25) & 0x1f) ;
      bank[bankidx] = -1 ;
      mem_addr += L2 ;
      DPRINTF ("<post1> <SizeMEM> bank%d = 32M mem_addr[next] = 0x%0x\n",
               bankidx, mem_addr) ; 
      
    }
  }

  /*
   * Print the SIMM info we have found so far.
   */
  PrintBANKinfo(bank) ;  

  /*
   * Final pass - taking care of those bank which do not have SIMM
   */
  for (bankidx = 0 ; bankidx < 8 ; bankidx += 1) {
    if (bank[bankidx] == 0) {
      * (uint64_t *) (CRIME_REG_ADDR + (uint32_t)(bankidx << 3)) =
          * (uint64_t *) CRIME_REG_ADDR ;
      bank[bankidx] = -1 ;
      DPRINTF ("<post1> <SizeMEM> bank%d = 0M BankControl = 0x%0lx\n",
               bankidx, * (uint64_t *) CRIME_REG_ADDR) ;
    }
  }
    
  /*
   * print the memory size, and die here if there is no memory installed.
   */
  DPRINTF ("<post1> <SizeMEM> MEM Size = 0x%0x bytes\n", mem_addr) ;

  /*
   * We done!
   */
  UNmap128Mpage() ;
  return (mem_addr) ;
}




/************
 * DupSLStack
 *-----------
 *  Copy sloader stack to kseg1 memory space, 
 *  - a simple memory test for the region of 0x0 to 0x1000
 *  - if memory test ok then do cache op <hit write back> for the
 *    whole black.
 *  - when we done the sloader stack should be in both location and
 *    we should be able to go kseg0.
 *-----------
 *  Return 1 if copy successfully, -1 otherwise.
 */
#define PROM_STARTADDR  (PROM_RAMBASE&0x1fffffff)
#define PROM_SIZE       ((PROM_STACK&0x1fffffff)-PROM_STARTADDR)
extern  Copy2MEM(uint32_t,uint32_t,uint32_t);
int32_t
DupSLStack ()
{
  uint64_t  tmp  ;
  uint32_t  addr ; 
  volatile uint64_t  data ; 

  /*
   * Test the memory region that will be used by sloader stack
   * simple_memtst will not return if it encounter a memory error.
   */
  SimpleMEMtst((uint32_t)0x0, (uint32_t)0x1000) ; 

#if 0  
  /*
   * Test the memory region that will be used by firmware
   * simple_memtst will not return if it encounter a memory error.
   * Note: It took ~ 2 minutes to complete following test, not acceptable.
   */
  SimpleMEMtst((uint32_t)PROM_STARTADDR, (uint32_t)PROM_SIZE);
#endif  

  /*
   * Now copy the sloader stack to memory.
   * [1] Touch every line in the sl-stack to make sure that the
   *     "W" bit in the primary cache tags are all "on".
   * [2] Do cache hit writeback for every line.
   */
#if 1
  for (addr = 0x0 ; addr <= 0x1000 ; addr += 8)
    Copy2MEM(addr,0x0,0x0);
#else 
  for (addr = 0x80000000 ; addr != 0x80001000 ; addr += 0x20) {
    data = * (uint64_t *) addr ;    /* Read it from cache,              */
    * (uint64_t *) addr = data ;    /* and make the line's "W" bit on   */
    /* CacheWriteBack(addr) ;       /* and write the cache line to mem  */
    CacheWriteBackInvalidate(addr); /* and write the cache line to mem  */
  }
#endif

  /*
   * And we done
   */
  return (1) ;
}


/*********
 * CopyPOST1
 *--------
 *  - copy memory resident post1 code to memory.
 *  - initialize post1 stack.
 *  - invalid primary I cache.
 *  - go uncache, and live happliy ever after.
 *--------
 *  Return 1 if copy successfully, -1 otherwise.
 */

extern void bootstrap() ;
extern void bootstrap_end() ;

/* #define Sim         */
/* #define EnablePass2 */
int
CopyPOST1 (int32_t startaddr, int32_t endaddr, int32_t targetaddr)
{
  void      (*bootpost1)(int32_t, int32_t, int32_t, uint32_t) ;
  int       idx, bitshift ; 
  uint64_t  value ;
  uint32_t  CRIME_REG_ADDR, tmp ;
  uint32_t  *pptr, *mptr, *eptr ; 
  uint8_t   *bpptr, *bmptr, *beptr ; 
  uint32_t  checksum = 0x0 ;
  
  /*
   * Assume nothing, turn off ECC checking first.
   */
  CRIME_REG_ADDR = (uint32_t) (K1|CRM_CONTROL) ;
  value = * (uint64_t *) CRIME_REG_ADDR ;
  * (uint64_t *) CRIME_REG_ADDR = value & (~CRM_MEM_CONTROL_ECC_ENA) ;

  DPRINTF ("<post1> <CopyPOST1> ECC off \n") ;
  
  pptr      = (uint32_t *) bootstrap ; 
  eptr      = (uint32_t *) bootstrap_end ; 
  mptr      = (uint32_t *) 0xa0010000 ;
  bootpost1 = (void (*) (int32_t, int32_t, int32_t,uint32_t)) 0x80010000 ; 

  DPRINTF ("<post1> <CopyPOST1>\n"
           "  pass 1: Copy bootstrap 0x%0x to 0x%0x\n",
           (uint32_t)pptr, (uint32_t)eptr) ;

  while (pptr++ != eptr) {
    *mptr++ = *pptr ;
    /*    DPRINTF ("  0postptr = 0x%0x, memptr = 0x%0x\n", postptr, memptr) ; */
  }
  
  DPRINTF ("<post1> <CopyPOST1>\n"
           "        pass 1: Bootstrap POST1DIAGS\n"
           "                Starting addr: 0x%0x\n"
           "                Ending   addr: 0x%0x\n"
           "                Target   addr: 0x%0x\n",
           startaddr, endaddr, targetaddr) ;

  (*bootpost1)((int)startaddr,(int)endaddr,(int)targetaddr, (uint32_t)0x0) ;

  
#if !defined(EnablePass2)
  
  idx = 1 ;
  
#else
  
  /*
   * Read everything back from flash and compare the result.
   */
  checksum = 0 ;
  bpptr = (uint8_t *) startaddr ; 
  beptr = (uint8_t *) endaddr ; 
  bmptr = (uint8_t *) targetaddr ; 

  DPRINTF ("<post1> <CopyPOST1>\n"
           "        pass 2: CheckSUM POST1 IMAGES\n"
           "                Starting addr: 0x%0x\n"
           "                Ending   addr: 0x%0x\n", 
           targetaddr, targetaddr + (endaddr - startaddr)) ;

  while (bpptr++ != beptr) {
    checksum = checksum ^ (*bmptr++ & 0xff) ; 
  }

  tmp = * (uint32_t *) endaddr ;
  idx = (tmp == checksum) ; 
  DPRINTF ("<post1> <CopyPOST1>\n"
           "        pass 2: Original checksum = 0x%0x, Computed"
           " checksum = 0x%0x\n", tmp, checksum) ;
#endif
  
  return (idx) ;
  
}

