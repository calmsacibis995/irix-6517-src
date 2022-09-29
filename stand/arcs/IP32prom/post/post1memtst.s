#include <asm.h>
#include <regdef.h>

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/crime.h>
#include <sys/mace.h>
#include <post1supt.h>

#define C0_IMP_R10000    0x09
#define	C0_IMPMASK	 0xff00
#define	C0_IMPSHIFT	 8


.extern  delay 
	
.set    noreorder
    
/***************
 * bootstrip
 *--------------
 * a simple first level post1 loader.
 *--------------
 */
 
    .globl  bootstrap
    .globl  bootstrap_end
    .ent    bootstrap
bootstrap:
 # copy post1diags image to memory.
    addu    a1, 8                   /* bump to next dword.              */
1:  ld      a3, 0x0(a0)             /* read from source.                */
    addu    a0, 8                   /* point to next double word.       */
    sd      a3, 0x0(a2)             /* write to target.                 */
    bne     a0, a1, 1b              /* OK, see if we're done yet!       */
    addu    a2, 8                   /* point to next double word.       */
    jr      ra                      /* return to caller.                */
bootstrap_end:                      /* the end of the loader.           */
    nop                             /* branch delay slot.               */
    .end    bootstrip
    
    
/***************
 * ReadEntryHI 
 *--------------
 * Read the EntryHi register.
 *--------------
 */
    .globl  ReadEntryHi 
    .ent    ReadEntryHi 
ReadEntryHi:
    mfc0    v0, EntryHi             /* Read the EntryHi                 */
    nop                             /* give a nop                       */
    jr      ra                      /* return to caller.                */
    nop                             /* jump delay slot.                 */
    
    .end    ReadEntryHi
    
/***************
 * ReadEntryLO1
 *--------------
 * Read the EntryLo1 register.
 *--------------
 */
    .globl  ReadEntryLO1
    .ent    ReadEntryLO1
ReadEntryLO1:
    mfc0    v0, EntryLo1            /* Read the EntryLo1                */
    nop                             /* give a nop                       */
    jr      ra                      /* return to caller.                */
    nop                             /* jump delay slot.                 */
    
    .end    ReadEntryLO1
    
/***************
 * ReadEntryLO0
 *--------------
 * Read the EntryLo0 register.
 *--------------
 */
    .globl  ReadEntryLO0
    .ent    ReadEntryLO0
ReadEntryLO0:
    mfc0    v0, EntryLo0            /* Read the EntryLo0                */
    nop                             /* give a nop                       */
    jr      ra                      /* return to caller.                */
    nop                             /* jump delay slot.                 */
    
    .end    ReadEntryLO0
    
/***************
 * TLBidxRead
 *--------------
 * Read the indexed TLB entry.
 *--------------
 */
    .globl  TLBidxRead
    .ent    TLBidxRead
TLBidxRead:
    mtc0    a0, Index               /* update index register.           */
    nop                             /* give 4 nops                      */
    nop                             /* give 4 nops                      */
    nop                             /* give 4 nops                      */
    jr      ra                      /* return to caller.                */
    tlbr                            /* read the tlb entry               */
    
    .end    TLBidxRead
    

/***************
 * WritePGMSK
 *--------------
 * Update page mask cp0 register.
 *--------------
 */
    .globl  WritePGMSK
    .ent    WritePGMSK
WritePGMSK:
    mtc0    a0, PageMask            /* Update PageMask register,        */
    nop                             /* one nop just in case.            */
    jr      ra                      /* and return to caller.            */
    nop                             /* one delay slot.                  */
    
    .end    WritePGMSK
    

/***************
 * WriteTLBPkg
 *--------------
 * Update EntryHi, EntryLo0, and EntryLo1 
 *--------------
 */
    .globl  WriteTLBPkg
    .ent    WriteTLBPkg
WriteTLBPkg:
    mtc0    a0, EntryHi             /* Update EntryHi  register,        */
    mtc0    a1, EntryLo0            /* Update EntryLo0 register,        */
    mtc0    a2, EntryLo1            /* Update EntryLo1 register,        */
    nop                             /* one nop just in case.            */
    nop                             /* one nop just in case.            */
    jr      ra                      /* and return to caller.            */
    nop                             /* one delay slot.                  */
    
    .end    WriteTLBPkg
    
/***************
 * TLBidxWrite
 *--------------
 * Update TLB array
 *--------------
 */
    .globl  TLBidxWrite
    .ent    TLBidxWrite
TLBidxWrite:
    mtc0    a0, Index               /* update Index register.           */
    nop                             /* give 4 nops.                     */
    nop                             /* give 4 nops.                     */
    nop                             /* give 4 nops.                     */
    nop                             /* give 4 nops.                     */
    jr      ra                      /* and return to caller.            */
    tlbwi
    
    .end    TLBidxWrite
    
/**********
 * Copy2MEM
 *---------
 * Copy2MEM(addr,tmp1,tmp2)
 */
    .globl  Copy2MEM
    .ent    Copy2MEM
Copy2MEM:
    lui     a1, 0x8000              /* load data from cache.            */
    or      a1, a0                  /* cached data address.             */
    ld      a2, 0(a1)               /* read from cache.                 */
    nop                             /* one nop just in case.            */
    lui     a1, 0xa000              /* memory address.                  */
    or      a1, a0                  /* uncached address.                */
    sd      a2, 0(a1)               /* write to memory.                 */
    jr      ra                      /* and return to caller.            */
    nop                             /* one delay slot.                  */
    .end    Copy2MEM
    
/***************
 * IcacheInvalidate
 *--------------
 * Invalidate the primary I cache by using
 * the indx Invalidate cache op.
 *--------------
 */
    .globl  IcacheInvalidate
    .ent    IcacheInvalidate
IcacheInvalidate:
    nop	                            /* to fool the global optimization  */
    cache   CACH_PI|C_IINV, 0x0(a0) /* Index invalidate.                */
    nop	                            /* give one nop to sattle down      */
    jr	    ra                      /* the cop0 pipeline.               */
    nop	                            /* and the delay slot.              */
    
    .end    IcacheInvalidate
    

/***************
 * CacheWriteBack(uint32)
 *--------------
 *  Do a cacheop hit writeback on the given
 *  Virtual address.
 *--------------
 */
    .globl  CacheWriteBack
    .ent    CacheWriteBack
CacheWriteBack:
    nop	                            /* to fool the global optimization  */
    cache   CACH_PD|C_HWB, 0x0(a0)  /* hit writeback.                   */
    nop	                            /* give one nop to sattle down      */
    jr	    ra                      /* the cop0 pipeline.               */
    nop	                            /* and the delay slot.              */
    
    .end    CacheWriteBack
    

/***************
 * CacheWriteBackInvalidate(uint32)
 *--------------
 *  Do a cacheop hit writeback on the given
 *  Virtual address.
 *--------------
 */
    .globl  CacheWriteBackInvalidate
    .ent    CacheWriteBackInvalidate
CacheWriteBackInvalidate:
    .set    noat                    /* turn off AT macro replacement.   */
    subu    sp, 0x20                /* create current stack frame.      */
    sd      AT, 0x08(sp)            /* save AT register.                */
    sd      t0, 0x10(sp)            /* save t0.                         */
    mfc0    AT, C0_PRID             /* read the processor ID register   */
    li      t0, C0_IMP_R10000       /* the R10000 IMP number.           */
    andi    AT, C0_IMPMASK          /* get the inplementation num       */
    srl     AT, C0_IMPSHIFT         /* shift to bit 0.                  */
    bne     t0, AT, 1f              /* its not R10K.                    */
    cache   CACH_PD|C_HWBINV, 0x0(a0) /* hit writeback invalidate       */
    cache   CACH_SD|C_HWBINV, 0x0(a0) /* hit writeback invalidate       */
1:  ld      AT, 0x08(sp)            /* save AT register.                */
    ld      t0, 0x10(sp)            /* save t0.                         */
    jr	    ra                      /* the cop0 pipeline.               */
    addiu   sp, 0x20                /* create current stack frame.      */
    .set    at                      /* turn on AT macro replacement.    */
    .end    CacheWriteBackInvalidate
    

/***************
 * SimpleMEMtst 
 *--------------
 *  alternate c program entry point for simple_memtst
 *--------------
 */
#undef 	    SIM
    .globl  SimpleMEMtst
    .ent    SimpleMEMtst
SimpleMEMtst:
    subu    sp, 0x80                /* Decrement the stack pointer.     */
    sd	    ra, 0x08(sp)            /* Save return address.             */
    sd	    a0, 0x10(sp)	    /* Save starting address.           */
    sd	    a1, 0x18(sp)            /* Save ending address.             */
    sd	    t0, 0x20(sp)            /* Save t0 <do we need this ??>     */
    sd	    t1, 0x28(sp)            /* Save t0 <do we need this ??>     */
    sd	    t2, 0x30(sp)            /* Save t0 <do we need this ??>     */
    sd	    t3, 0x38(sp)            /* Save t0 <do we need this ??>     */
    sd	    t4, 0x40(sp)            /* Save t0 <do we need this ??>     */
    sd	    t5, 0x48(sp)            /* Save t0 <do we need this ??>     */
    sd	    t6, 0x50(sp)            /* Save t0 <do we need this ??>     */
    sd	    s0, 0x58(sp)            /* Save t0 <do we need this ??>     */
    sd	    s1, 0x60(sp)            /* Save t0 <do we need this ??>     */
    sd	    s2, 0x68(sp)            /* Save t0 <do we need this ??>     */
    
#if !defined(SIM)
    jal	    simple_memtst           /* call leaf routine simple_memtst  */
    nop	                            /* branch delay slot.               */
#endif
    
    ld	    ra, 0x08(sp)            /* Save return address.             */
    ld	    a0, 0x10(sp)	    /* Save starting address.           */
    ld	    a1, 0x18(sp)            /* Save ending address.             */
    ld	    t0, 0x20(sp)            /* Save t0 <do we need this ??>     */
    ld	    t1, 0x28(sp)            /* Save t0 <do we need this ??>     */
    ld	    t2, 0x30(sp)            /* Save t0 <do we need this ??>     */
    ld	    t3, 0x38(sp)            /* Save t0 <do we need this ??>     */
    ld	    t4, 0x40(sp)            /* Save t0 <do we need this ??>     */
    ld	    t5, 0x48(sp)            /* Save t0 <do we need this ??>     */
    ld	    t6, 0x50(sp)            /* Save t0 <do we need this ??>     */
    ld	    s0, 0x58(sp)            /* Save t0 <do we need this ??>     */
    ld	    s1, 0x60(sp)            /* Save t0 <do we need this ??>     */
    ld	    s2, 0x68(sp)            /* Save t0 <do we need this ??>     */
    jr	    ra                      /* return                           */
    addu    sp, 0x80                /* and restore the stack pointer.   */
    
    .end    SimpleMEMtst
    
    
/***************
 * simple_memtst
 *--------------
 *  memory address test routine, location indenpendent.
 *  walking 0/1 patterns test, 2 patterns test, and 
 *  address marching test
 *--------------
 * register usage:
 *  a0:     starting physical address, also used by the program as 
 *          first test address.
 *  a1:     size of the memory to test, also as second test address.
 *  t0:     starting address in KSEG1.
 *  t1:     ending   address in KSEG1.
 *  t2:     crime id register, read from this register to clear the bus.
 *  t3:     test data pattern.
 *  t4:     expected data pattern.
 *  t5:     error code when error, otherwise tmp register.
 */
/*--------------
 * Special flags used for LAB bringup.
 *--------------
 * #define LAB
 */
#undef LAB
#undef DEBUG
    .globl  simple_memtst
    .ent    simple_memtst
simple_memtst:
    lui     t0, 0xa000              /* KSG1 base address.               */
    addu    t0, t0, a0              /* get the starting addr in KSEG1   */
    addu    t1, t0, a1              /* the ending addr in KSEG1         */
    la      t2, PHYS_TO_K1(CRM_ID)  /* crime id register.               */

  # Walking 1/0 test (at starting address, a doubleword read/write)
LOOP1:
#if defined(LAB)
    li      s0, 2                   /* indicate this is the first pass  */
    move    s1, zero                /* fill memory with 0 forst.        */
    move    s2, t0                  /* save the test address.           */
4:  nop	                            /* place holder for looping.        */
#endif
    li      a0, 0x20                /* The size of memory bus.          */
    sub     a0, 8                   /* point to the last word.          */
1:  bltz    a0, 3f                  /* done with all 256 bits.          */
    addu    a1, t0, a0              /* create the test address.         */
    li      t3, 0x1                 /* walking 1 at starting address.   */
#if defined(LAB)
5:  sd      s1, 0x00(s2)            /* fill memory with 0/1             */
    sd      s1, 0x08(s2)            /* fill memory with 0/1             */
    sd      s1, 0x10(s2)            /* fill memory with 0/1             */
    sd      s1, 0x18(s2)            /* fill memory with 0/1             */
#endif
2:  sd      t3, 0x0(a1)             /* do 64 bit write.                 */
    ld      t5, 0x0(t2)             /* clear the bus,                   */
    ld      t4, 0x0(a1)             /* read the pattern back.           */
    li      t5, 0x1                 /* load the error code.             */
#if !defined(LAB)
    bne     t3, t4, mem_test_errors /* error cases.                     */
    dsll    t3, 1                   /* 64 bits shift left by 1 bit.     */
    bnez    t3, 2b                  /* loop the loop and loop ...       */
    nop                             /* branch delay slot.               */
#else 
    dsll    t3, 1                   /* 64 bits shift left by 1 bit.     */
    bnez    t3, 5b                  /* loop the loop and loop ...       */
    nop	                            /* branch delay slot.               */
#endif
    b       1b                      /* loop back to do next  word.      */
    sub     a0, 8                   /* point to next word.              */
#if defined(LAB)    
3:  sub     s0, 1                   /* are we done with fill 0/1 passes */
    beqz    s0, 3f                  /* continue to walking 0 test       */
    nop	                            /* branch delay slot.               */
    b	    4b                      /* loopback for second pass.        */
    not	    s1, zero                /* fill 1's                         */
#endif

 # Walking 0 test
#if defined(LAB)
3:  li      s0, 2                   /* indicate this is the first pass  */
    move    s1, zero                /* fill memory with 0 forst.        */
    move    s2, t0                  /* save the test address.           */
4:  nop	                            /* place holder for looping.        */
#endif
3:  li      a0, 0x20                /* The size of memory bus.          */
    sub     a0, 8                   /* point to last word.              */
1:  bltz    a0, 3f                  /* We done.                         */
    addu    a1, t0, a0              /* the test address.                */
    li      t3, 0x1                 /* walking 1 at starting address.   */
    not     t3                      /* now we have walking 0 pattern.   */
#if defined(LAB)
5:  sd      s1, 0x00(s2)            /* fill memory with 0/1             */
    sd      s1, 0x08(s2)            /* fill memory with 0/1             */
    sd      s1, 0x10(s2)            /* fill memory with 0/1             */
    sd      s1, 0x18(s2)            /* fill memory with 0/1             */
#endif
2:  sd      t3, 0x0(a1)             /* do 64 bit write.                 */
    ld      t5, 0x0(t2)             /* clear the bus,                   */
    ld      t4, 0x0(a1)             /* read the pattern back.           */
    li      t5, 0x2                 /* load the error code.             */
#if !defined(LAB)
    bne     t3, t4, mem_test_errors /* error cases.                     */
    not	    t6, t3                  /* invert the data pattern back.    */
    dsll    t6, 1                   /* 64 bits shift left by 1 bit.     */
    bnez    t6, 2b                  /* loop the loop and loop ...       */
    not     t3, t6                  /* restore t3.                      */
#else
    not	    t6, t3                  /* invert the data pattern back.    */
    dsll    t6, 1                   /* 64 bits shift left by 1 bit.     */
    bnez    t6, 5b                  /* loop the loop and loop ...       */
    not     t3, t6                  /* restore t3.                      */
#endif
    b       1b                      /* loop back for next word.         */
    sub     a0, 8                   /* point ot the right word.         */
3:  nop
#if defined(LAB)    
    sub     s0, 1                   /* are we done with fill 0/1 passes */
    beqz    s0, 3f                  /* continue to walking 0 test       */
    nop	                            /* branch delay slot.               */
    b	    4b                      /* loopback for second pass.        */
    not	    s1, zero                /* fill 1's                         */
3:  nop	                            /* place holder for branch.         */
#endif


  # Two patterns write, write, read test.
LOOP2:
    li      t3, 0x5a5a5a5a          /* the first data pattern.          */
    move    a0, t0                  /* current test address.            */
    addu    a1, a0, 4               /* second write addr.               */
1:  sw      t3, 0x0(a0)             /* write to addr                    */
    not     t3                      /* second data pattern.             */
    sw      t3, 0x0(a1)             /* write to addr+4                  */
    lwu     t4, 0x0(a0)             /* read addr.                       */
    not     t3                      /* switch back to pattern 1         */
#if !defined(LAB)
    bne     t3, t4, mem_test_errors /* observed != expected.            */
#endif
    li      t5, 0x3                 /* load the error code.             */
    addu    a1, 4                   /* are we over the boarder yet ?    */
    beq     a1, t1, 2f              /* See if we have reach the end.    */
    addu    a0, 4                   /* point to next word.              */
    bne     a0, t1, 1b              /* loop the loop again and again... */
    nop                             /* branch delay slot.               */
    b       3f                      /* this is the end.                 */
    nop                             /* branch delay slot.               */
2:  b       1b                      /* reset the second test address.   */
    move    a1, t0                  /* wrap around the test addr.       */
3:                                  /* place holder                     */

  # Address marching test
LOOP3:
    move    a0, t0                  /* load the test address.           */
    not     t3, a0                  /* invert the test addr.            */
1:  dsll32  t3, 0                   /* remove bits <63:32>              */
    dsrl32  t3, 0                   /* shift back to <31:0>             */
    dsll32  t5, a0, 0               /* get the test addr in <63:32>     */
    or      t3, t3, t5              /* for a 64 bit data pattern.       */
    sd      t3, 0x0(a0)             /* write to test address.           */
    addu    a0, 8                   /* increment test address.          */
    bne     a0, t1, 1b              /* loop back for next test addr.    */
    not     t3, a0                  /* invert the test address.         */
    move    a0, t0                  /* load the test address.           */
    not     t3, a0                  /* invert the test addr.            */
1:  dsll32  t3, 0                   /* remove bits <63:32>              */
    dsrl32  t3, 0                   /* shift back to <31:0>             */
    dsll32  t5, a0, 0               /* get the test addr in <63:32>     */
    ld      t4, 0x0(a0)             /* read from test address.          */
    or      t3, t3, t5              /* for a 64 bit data pattern.       */
#if !defined(LAB)    
    bne     t3, t4, mem_test_errors /* observed != expected.            */
#endif    
    li      t5, 0x4                 /* load the error code.             */
    not     t4                      /* invert the test data.            */
    sd      t4, 0x0(a0)             /* write back to test addr.         */
    addu    a0, 8                   /* increment test address.          */
    bne     a0, t1, 1b              /* loop back for next test addr.    */
    not     t3, a0                  /* invert the test address.         */
    move    a0, t0                  /* load the test address.           */
    not     t3, a0                  /* invert the test addr.            */
1:  dsll32  t3, 0                   /* remove bits <63:32>              */
    dsrl32  t3, 0                   /* shift back to <31:0>             */
    dsll32  t5, a0, 0               /* get the test addr in <63:32>     */
    ld      t4, 0x0(a0)             /* read from test address.          */
    or      t3, t3, t5              /* for a 64 bit data pattern.       */
    not     t3                      /* inverted test pattern.           */
#if !defined(LAB)    
    bne     t3, t4, mem_test_errors /* observed != expected.            */
#endif    
    li      t5, 0x5                 /* load the error code.             */
    addu    a0, 8                   /* increment test address.          */
    bne     a0, t1, 1b              /* loop back for next test addr.    */
    not     t3, a0                  /* invert the test address.         */
    
  # Restore a0 and a1 and return.
    lui     t5, 0xa000              /* address base of KSEG1            */
    subu    a0, t0, t5              /* turn off KSEG1 base addr.        */
    jr      ra                      /* return                           */
    subu    a1, t0, t1              /* The size of memory to be tested. */
    
  # Error handler routine, empty for now.
mem_test_errors:
#if defined(DEBUG)
    move    a3, a0                  /* save the test address.           */
    la	    a0, mem_error_msg       /* load the error msg format.       */
    move    a1, t3                  /* the expected data pattern.       */
    jal	    DPRINTF                 /* call debugcard printf.           */
    move    a2, t4                  /* and observed data pattern.       */
#endif
mem_error_loop:
    la      s0, 0xa0000000|ISA_FLASH_NIC_REG /* load the LED register.  */
    ld      s1, 0(s0)               /* save original value              */
    ori     s1, ISA_LED_RED|ISA_LED_GREEN    /* turn LED to amber.      */
1:  sd      s1, 0(s0)               /* light the led.                   */
    li      a0, 500000              /* wait for half second             */
    jal	    delay                   /* 1/2s delay loop.                 */
    nop                             /* branch delay slot.               */
    ld      s1, 0(s0)               /* save original value              */
    b       1b                      /* loop forever.                    */
    xori    s1, ISA_LED_RED|ISA_LED_GREEN    /* toggle the LEDs         */
    
    b	    mem_error_loop
    nop
    
    .end    simple_memtst

loop:
    .asciiz "looping %d"
    
mem_error_msg:
    .asciiz "post1 memtst error\n expected=0x%0lx, obsev=0x%0lx, addr=0x%0lx"
