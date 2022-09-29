#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/crime.h>
#include <post1supt.h>

.extern  delay 
	
    .set    noreorder
    
/***************
 * TLBentries
 *--------------
 * Return tlb entries for current CPU.
 *--------------
 */
    .globl  TLBentries
    .ent    TLBentries
TLBentries:
    mfc0    a0, PRid                /* read Processor ID register.      */
    nop                             /* CP0 delay slot.                  */
    li      v0, TRITON              /* Triton impl #                    */
    srl     a0, 8                   /* mask impl field.                 */
    andi    a0, 0xff                /* get ride of uninteresting stuff  */
    bne     v0, a0, 1f              /* we have triton processor.        */
    nop                             /* delay slot.                      */
    b       2f                      /* prepare to return.               */
    li      v0, 48                  /* Triton has 48 dual tlb entries.  */
    
  # this block are reserved for future processor.
1:  nop
  # for furture processors.
  
2:  jr      ra                      /* return.                          */
    move    a0, zero                /* clear a0 register.               */
    
    .end    TLBentries
    
    
/***************
 * CPUinterruptOFF
 *--------------
 * Turn off interrupt in status register
 *--------------
 */
    .globl  CPUinterruptOFF
    .ent    CPUinterruptOFF
CPUinterruptOFF:
    mfc0    a0, Status              /* Read the status register,        */
    nop                             /* delay slot.                      */
    li      a1, 1                   /* the interrupt bit.               */
    not     a1                      /* the interrupt enable mask.       */
    and     a0, a0, a1              /* turn interrupt off.              */
    mtc0    a0, Status              /* update the Status register.      */
    move    a1, zero                /* restore a1.                      */
    jr      ra                      /* return.                          */
    move    a0, zero                /* restore a1.                      */
    
    .end    CPUinterruptOFF
    
    
/***************
 * CPUinterruptON
 *--------------
 * Turn on interrupt in status register
 *--------------
 */
    .globl  CPUinterruptON
    .ent    CPUinterruptON
CPUinterruptON:
    mfc0    a2, Status              /* Read the status register,        */
    li      a1, 0xff00              /* the IM mask                      */
    not     a1                      /* invert the mask.                 */
    and     a1, a1, a2              /* mask off the original IM bits.   */
    sll     a0, 8                   /* move to right position.          */
    or      a1, a0, a1              /* get the IM in place.             */
    ori     a1, 1                   /* turn on interrupt.               */
    mtc0    a1, Status              /* Update the Status register.      */
    move    a0, zero                /* restore the a0                   */
    move    a1, zero                /* restore the a1                   */
    jr      ra                      /* return to caller.                */
    move    a2, zero                /* restore the a2                   */
    
    .end    CPUinterruptON
    
    
/***************
 * scachesize
 *--------------
 * Return the size of the secondary cache.
 *--------------
 */
    .globl  scachesize
    .ent    scachesize
scachesize:
 # Check Mike Neilson's Triton manual for detail.
    j       ra
    nop
    .end    scachesize
    

/***************
 * icachesize
 *--------------
 * return the primary i cache size.
 *--------------
 */
    .globl  icachesize
    .ent    icachesize
icachesize:
    mfc0    v0, Config
    li      a0, 0x1000
    nop
    srl     v0, 9
    andi    v0, 0x7
    sllv    v0, a0, v0
    
    j       ra
    move    a0, zero
    
    .end    icachesize 
    
    
/***************
 * pcachesize
 *--------------
 * Return the primary data cache size.
 *--------------
 */
    .globl  pcachesize
    .ent    pcachesize
pcachesize:
    mfc0    v0, Config
    li      a0, 0x1000
    nop
    srl     v0, 6
    andi    v0, 0x7
    sllv    v0, a0, v0
    
    j       ra
    move    a0, zero
    
    .end    pcachesize 
    
    
/***************
 * mtc0_config
 *--------------
 * Update the config register.
 *--------------
 */
    .globl  mtc0_config
    .ent    mtc0_config
mtc0_config:
    mtc0    a0, Config
    j       ra
    nop
    .end    mtc0_config    

    
/***************
 * mfc0_config
 *--------------
 * Read the error program counter from CP0
 *--------------
 */
    .globl  mfc0_config
    .ent    mfc0_config
mfc0_config:
    mfc0    v0, Config
    j       ra
    nop
    .end    mfc0_config    

/***************
 * mtc0_epc
 *--------------
 * Update the error program counter.
 *--------------
 */
    .globl  mtc0_epc
    .ent    mtc0_epc
mtc0_epc:
    mtc0    a0, EPC
    j       ra
    nop
    .end    mtc0_epc    

    
/***************
 * mfc0_epc
 *--------------
 * Read the error program counter from CP0
 *--------------
 */
    .globl  mfc0_epc
    .ent    mfc0_epc
mfc0_epc:
    mfc0    v0, EPC
    j       ra
    nop
    .end    mfc0_epc    

    
/***************
 * mfc0_CacheErr
 *--------------
 * Read the Cache Error register from CP0
 *--------------
 */
    .globl  mfc0_CacheErr
    .ent    mfc0_CacheErr
mfc0_CacheErr:
    mfc0    v0, CacheErr
    j       ra
    nop
    .end    mfc0_CacheErr    
    

/***************
 * mfc0_status
 *--------------
 * read the Status register from CP0
 *--------------
 */
    .globl  mfc0_status
    .ent    mfc0_status
mfc0_status:
    mfc0    v0, Status
    j       ra
    nop
    .end    mfc0_status    
    

/***************
 * mfc0_cause
 *--------------
 * read the Cause register from CP0.
 *--------------
 */
    .globl  mfc0_cause
    .ent    mfc0_cause
mfc0_cause:
    mfc0    v0, Cause
    j       ra
    nop
    .end    mfc0_cause    
    

/***************
 * mtc0_index
 *--------------
 * Update the Index register in CP0.
 *--------------
 */
    .globl  mtc0_index
    .ent    mtc0_index
mtc0_index:
    mtc0    a0, Index
    nop
    j       ra
    nop
    .end    mtc0_index
    

/***************
 * mfc0_index
 *--------------
 * Update the Index register in CP0.
 *--------------
 */
    .globl  mfc0_index
    .ent    mfc0_index
mfc0_index:
    mtc0    v0, Index
    nop
    j       ra
    nop
    .end    mfc0_index    
    

/***************
 * mtc0_entrylo0
 *--------------
 * Update the EntryLo0 register.
 *--------------
 */
    .globl  mtc0_entrylo0
    .ent    mtc0_entrylo0
mtc0_entrylo0:
    mtc0    a0, EntryLo0
    nop
    j       ra
    nop
    .end    mtc0_entrylo0    
    

/***************
 * mtc0_entrylo1
 *--------------
 * Update the EntryLo1 register.
 *--------------
 */
    .globl  mtc0_entrylo1
    .ent    mtc0_entrylo1
mtc0_entrylo1:
    mtc0    a0, EntryLo1
    nop
    j       ra
    nop
    .end    mtc0_entrylo1    
    
    
/***************
 * mtc0_entryhi
 *--------------
 * Update EntryHi register.
 *--------------
 */
    .globl  mtc0_entryhi
    .ent    mtc0_entryhi	
mtc0_entryhi:
    mtc0    a0, EntryHi
    nop
    j       ra
    nop
    .end    mtc0_entryhi    
    

/***************
 * mtc0_pagemask
 *--------------
 * Update the pagemask register.
 *--------------
 */
    .globl  mtc0_pagemask
    .ent    mtc0_pagemask
mtc0_pagemask:
    mtc0    a0, PageMask
    nop
    j       ra
    nop
    .end    mtc0_pagemask    
    

/***************
 * tlb_probe
 *--------------
 * TLB probe function.
 *--------------
 */
    .globl  tlb_probe
    .ent    tlb_probe
tlb_probe:
    j       ra
    tlbp
    .end    tlb_probe    
    

/***************
 * tlb_read
 *--------------
 * Read one tlb entries.
 *--------------
 */
    .globl  tlb_read
    .ent    tlb_read
tlb_read:
    j       ra
    tlbr
    .end    tlb_read    
    

/***************
 * tlb_write
 *--------------
 * Write a TLB entries
 *--------------
 */
    .globl  tlb_write
    .ent    tlb_write
tlb_write:
    j       ra
    tlbwi
    .end    tlb_write    
    

/***************
 * mtc0_taglo
 *--------------
 * Update the TagLo register.
 *--------------
 */
    .globl  mtc0_taglo
    .ent    mtc0_taglo
mtc0_taglo:
    mtc0    a0, TagLo
    nop
    j       ra
    nop
    .end    mtc0_taglo
    

/***************
 * mfc0_taglo
 *--------------
 * Update the TagLo register.
 *--------------
 */
    .globl  mfc0_taglo
    .ent    mfc0_taglo
mfc0_taglo:
    mfc0    v0, TagLo
    nop
    j       ra
    nop
    .end    mfc0_taglo
    

/***************
 * pcache_hit_invalid
 *--------------
 * invalidate a cache entry.
 *--------------
 */
    .globl  pcache_hit_invalid
    .ent    pcache_hit_invalid
pcache_hit_invalid:
    j       ra
    cache   (HitI|PD), 0x0(a0)
    .end    pcache_hit_invalid
    

/***************
 * pcache_hit_wb_invalid
 *--------------
 * Hit invalidate a cache entry.
 *--------------
 */
    .globl  pcache_hit_wb_invalid
    .ent    pcache_hit_wb_invalid
pcache_hit_wb_invalid:
    j       ra
    cache   (HWI|PD), 0x0(a0)
    .end    pcache_hit_wb_invalid
    

/***************
 * pcache_hit_wb
 *--------------
 * Write back a cache entry.
 *--------------
 */
    .globl  pcache_hit_wb
    .ent    pcache_hit_wb
pcache_hit_wb:
    j       ra
    cache   (HWB|PD), 0x0(a0)
    .end    pcache_hit_wb
    

/***************
 * pcache_hit_set_virtual
 *--------------
 * Hit set virtual
 *--------------
 */
    .globl  pcache_hit_set_virtual
    .ent    pcache_hit_set_virtual
pcache_hit_set_virtual:
    j       ra
    cache   (HSV|PD), 0x0(a0)
    .end    pcache_hit_set_virtual
    


/***************
 * pcache_index_wb_invalid
 *--------------
 * Index write back invalidate
 *--------------
 */
    .globl  pcache_index_wb_invalid
    .ent    pcache_index_wb_invalid
pcache_index_wb_invalid:
    j       ra
    cache   (IWI|PD), 0x0(a0)
    .end    pcache_index_wb_invalid
    

/***************
 * pcache_index_load_tag
 *--------------
 * Index load tag for pcache.
 *--------------
 */
    .globl  pcache_index_load_tag
    .ent    pcache_index_load_tag
pcache_index_load_tag:
    j       ra
    cache   (ILT|PD), 0x0(a0)
    .end    pcache_index_load_tag
    

/***************
 * pcache_index_store_tag
 *--------------
 * Index store tag.
 *--------------
 */
    .globl  pcache_index_store_tag
    .ent    pcache_index_store_tag
pcache_index_store_tag:
    j       ra
    cache   (IST|PD), 0x0(a0)
    .end    pcache_index_store_tag
    

/***************
 * pcache_create_dirty_exclusive
 *--------------
 * Pcache create dirty exclusive.
 *--------------
 */
    .globl  pcache_create_dirty_exclusive
    .ent    pcache_create_dirty_exclusive
pcache_create_dirty_exclusive:
    j       ra
    cache   (CDE|PD), 0x0(a0)
    .end    pcache_create_dirty_exclusive
    

/***************
 * pcache_i_fill
 *--------------
 * Primary icache FILL
 *--------------
 */
    .globl  pcache_i_fill
    .ent    pcache_i_fill
pcache_i_fill:
    j       ra
    cache   (FILL|PI), 0x0(a0)
    .end    pcache_i_fill
    

/***************
 * icache_idx_invalidate 
 *--------------
 * Primary index invalidate function.
 *--------------
 */
    .globl  icache_idx_invalidate
    .ent    icache_idx_invalidate
icache_idx_invalidate:
    j       ra
    cache   (IDXI|PI), 0x0(a0)
    .end    icache_idx_invalidate
    

/***************
 * scache_clear_sequence
 *--------------
 * secondary cache clear sequence 
 *--------------
 */
    .globl  scache_clear_sequence
    .ent    scache_clear_sequence
scache_clear_sequence:
    lui     a0, 0x8000
    j       ra
    cache   (SDC|SD), 0x0(a0)
    .end    scache_clear_sequence
    

/***************
 * scache_page_invalidate
 *--------------
 * Seconary cache page invalidate.
 *--------------
 */
    .globl  scache_page_invalid
    .ent    scache_page_invalid
scache_page_invalid:
    j       ra
    cache   (SCPI|SD), 0x0(zero)
    .end    scache_page_invalid

    
/***************
 * scache_index_load_tag
 *--------------
 * Secondary cache index load tag.
 *--------------
 */
    .globl  scache_index_load_tag
    .ent    scache_index_load_tag
scache_index_load_tag:
    j       ra
    cache   (ILT|SD), 0x0(a0)
    .end    scache_index_load_tag
    

/***************
 * scache_index_store_tag
 *--------------
 * Secondary index store tag.
 *--------------
 */
    .globl  scache_index_store_tag
    .ent    scache_index_store_tag
scache_index_store_tag:
    j       ra
    cache   (IST|SD), 0x0(a0)
    .end    scache_index_store_tag
    

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***********
 * heartBeat
 *----------
 * Print a rotating bar to indicate I'm alive.
 *--------------
 */
    .globl  heartBeat
    .ent    heartBeat
heartBeat:
    subu    sp, 0x40                /* create new window frame.         */
    sd      ra, 0x18(sp)            /* save return address.             */
    sd      AT, 0x20(sp)            /* save return address.             */
    sd      t0, 0x28(sp)            /* save t0.                         */
    sd      t1, 0x30(sp)            /* save t1.                         */
    sd      a0, 0x38(sp)            /* save return address.             */
    li      t0, 0x2d                /* check for '-'                    */
    beq     t0, a0, 1f              /* is this the one ??               */
    li      t0, 0x2f                /* print '/' if found '-'           */
    beq     t0, a0, 1f              /* check '/'                        */
    li      t0, 0x5c                /* print '\' if found '/'           */
    beq     t0, a0, 1f              /* check '\'                        */
    li      t0, 0x2d                /* print '-' if found '\'           */
    b       2f                      /* found nothing, no print          */
    nop                             /* delay slot.                      */
1:  jal     putchar                 /* go print it.                     */
    move    a0, t0                  /* the indicator.                   */
    jal     putchar                 /* back to the previous position.   */
    li      a0, 0x08                /* back space character.            */
    move    v0, t0                  /* return the next indicator.       */
2:  ld      ra, 0x18(sp)            /* save return address.             */
    ld      AT, 0x20(sp)            /* save return address.             */
    ld      t0, 0x28(sp)            /* save t0.                         */
    ld      t1, 0x30(sp)            /* save t1.                         */
    ld      a0, 0x38(sp)            /* save return address.             */
    jr      ra                      /* return to caller.                */
    addu    sp, 0x40                /* create new window frame.         */
    
    .end    heartBeat  
    
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
    
/***************
 * SimpleMEMtst 
 *--------------
 *  alternate c program entry point for simple_memtst
 *--------------
 */
#define     frameSize          0x120
    .globl  SimpleMEMtst
    .ent    SimpleMEMtst
SimpleMEMtst:
    subu    sp, frameSize           /* Decrement the stack pointer.     */
    sd	    ra, frameSize-0x08(sp)  /* Save return address.             */
    sd	    a0, frameSize-0x10(sp)  /* Save starting address.           */
    sd	    a1, frameSize-0x18(sp)  /* Save ending address.             */
    sd	    t0, frameSize-0x20(sp)  /* Save t0 <do we need this ??>     */
    sd	    t1, frameSize-0x28(sp)  /* Save t0 <do we need this ??>     */
    sd	    t2, frameSize-0x30(sp)  /* Save t0 <do we need this ??>     */
    sd	    t3, frameSize-0x38(sp)  /* Save t0 <do we need this ??>     */
    sd	    t4, frameSize-0x40(sp)  /* Save t0 <do we need this ??>     */
    sd	    t5, frameSize-0x48(sp)  /* Save t0 <do we need this ??>     */
    sd	    t6, frameSize-0x50(sp)  /* Save t0 <do we need this ??>     */
    sd	    s0, frameSize-0x58(sp)  /* Save t0 <do we need this ??>     */
    sd	    s1, frameSize-0x60(sp)  /* Save t0 <do we need this ??>     */
    sd	    s2, frameSize-0x68(sp)  /* Save t0 <do we need this ??>     */
    sd	    s3, frameSize-0x70(sp)  /* Save t0 <do we need this ??>     */
    sd	    s4, frameSize-0x78(sp)  /* Save t0 <do we need this ??>     */
    sd	    s5, frameSize-0x80(sp)  /* Save t0 <do we need this ??>     */
    sd	    AT, frameSize-0x88(sp)  /* Save t0 <do we need this ??>     */
    b       simple_memtst           /* call leaf routine simple_memtst  */
    nop	                            /* branch delay slot.               */
simple_mem_rtn:
    nop                             /* place holder for return.         */
    ld	    ra, frameSize-0x08(sp)  /* Save return address.             */
    ld	    a0, frameSize-0x10(sp)  /* Save starting address.           */
    ld	    a1, frameSize-0x18(sp)  /* Save ending address.             */
    ld	    t0, frameSize-0x20(sp)  /* Save t0 <do we need this ??>     */
    ld	    t1, frameSize-0x28(sp)  /* Save t0 <do we need this ??>     */
    ld	    t2, frameSize-0x30(sp)  /* Save t0 <do we need this ??>     */
    ld	    t3, frameSize-0x38(sp)  /* Save t0 <do we need this ??>     */
    ld	    t4, frameSize-0x40(sp)  /* Save t0 <do we need this ??>     */
    ld	    t5, frameSize-0x48(sp)  /* Save t0 <do we need this ??>     */
    ld	    t6, frameSize-0x50(sp)  /* Save t0 <do we need this ??>     */
    ld	    s0, frameSize-0x58(sp)  /* Save t0 <do we need this ??>     */
    ld	    s1, frameSize-0x60(sp)  /* Save t0 <do we need this ??>     */
    ld	    s2, frameSize-0x68(sp)  /* Save t0 <do we need this ??>     */
    ld	    s3, frameSize-0x70(sp)  /* Save t0 <do we need this ??>     */
    ld	    s4, frameSize-0x78(sp)  /* Save t0 <do we need this ??>     */
    ld	    s5, frameSize-0x80(sp)  /* Save t0 <do we need this ??>     */
    ld	    AT, frameSize-0x88(sp)  /* Save t0 <do we need this ??>     */
    jr	    ra                      /* return                           */
    addu    sp, frameSize           /* and restore the stack pointer.   */
    
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
 
    .globl  simple_memtst
    .ent    simple_memtst
simple_memtst:
    subu    sp, 0x60
  # Turn off Red LED and turn on green LED.
#if 0
    move    s3, ra                  /* save return address.             */
#endif
    lui     t0, 0xbf31              /* load the PROM access addrsss.    */
    ld      t1, 0x8(t0)             /* save original value              */
    li      t2, ~0x11               /* LED mask.                        */
    and     t2, a1, a2              /* Turn both on                     */
    ori     t2, 0x10                /* and leave one off.               */
    sd      t2, 0x8(t0)             /* turn on the green led.           */
    
  # Now setup test range.
    lui     t0, 0xa000              /* KSG1 base address.               */
    addu    s0, t0, a0              /* get the starting addr in KSEG1   */
    addu    s1, t0, a1              /* the ending addr in KSEG1         */
    la      s2, PHYS_TO_K1(CRM_ID)  /* crime id register.               */

  # Walking 1/0 test (at starting address, a doubleword read/write)
    la      a0, mem_walking_1       /* load the control format.         */
    jal     printf                  /* print the test header.           */
    nop
LOOP1:
    li      a0, 0x20                /* The size of memory bus.          */
    sub     a0, 8                   /* point to the last word.          */
1:  bltz    a0, 3f                  /* done with all 256 bits.          */
    addu    a1, s0, a0              /* create the test address.         */
    li      t3, 0x1                 /* walking 1 at starting address.   */
2:  sd      t3, 0x0(a1)             /* do 64 bit write.                 */
    ld      t5, 0x0(s2)             /* clear the bus,                   */
    ld      t4, 0x0(a1)             /* read the pattern back.           */
    li      t5, 0x1                 /* load the error code.             */
    bne     t3, t4, mem_test_errors /* error cases.                     */
    dsll    t3, 1                   /* 64 bits shift left by 1 bit.     */
    bnez    t3, 2b                  /* loop the loop and loop ...       */
    nop                             /* branch delay slot.               */
    b       1b                      /* loop back to do next  word.      */
    sub     a0, 8                   /* point to next word.              */
3:  nop                             /* slot to hold the branch target.  */

 # Walking 0 test
    la      a0, mem_walking_0       /* load the test header message.    */
    jal     printf                  /* print it.                        */
    nop
3:  li      a0, 0x20                /* The size of memory bus.          */
    sub     a0, 8                   /* point to last word.              */
1:  bltz    a0, 3f                  /* We done.                         */
    addu    a1, s0, a0              /* the test address.                */
    li      t3, 0x1                 /* walking 1 at starting address.   */
    not     t3                      /* now we have walking 0 pattern.   */
2:  sd      t3, 0x0(a1)             /* do 64 bit write.                 */
    ld      t5, 0x0(s2)             /* clear the bus,                   */
    ld      t4, 0x0(a1)             /* read the pattern back.           */
    li      t5, 0x2                 /* load the error code.             */
    bne     t3, t4, mem_test_errors /* error cases.                     */
    not	    t6, t3                  /* invert the data pattern back.    */
    dsll    t6, 1                   /* 64 bits shift left by 1 bit.     */
    bnez    t6, 2b                  /* loop the loop and loop ...       */
    not     t3, t6                  /* restore t3.                      */
    b       1b                      /* loop back for next word.         */
    sub     a0, 8                   /* point ot the right word.         */
3:  nop                             /* branch target place holder.      */

  # Two patterns write, write, read test.
    la      a0, two_pattern_test    /* load the test header.            */
    jal     printf                  /* print ti to debug console.       */
    nop                             /* delay slot.                      */
    li      t6, 0x2d                /* setup initial value of heart beat */
LOOP2:
    li      t3, 0x5a5a5a5a          /* the first data pattern.          */
    move    a1, s0                  /* current test address.            */
    addu    a0, a1, 4               /* second write addr.               */
1:  move    s4, a0                  /* save a0                          */
    bal     heartBeat               /* go print indicator.              */
    move    a0, t6                  /* set the indicator.               */
    move    t6, v0                  /* save the new indicator.          */
    move    a0, s4                  /* restore a0                       */
    sw      t3, 0x0(a1)             /* write to addr                    */
    not     t3                      /* second data pattern.             */
    sw      t3, 0x0(a0)             /* write to addr+4                  */
    lwu     t4, 0x0(a1)             /* read addr.                       */
    not     t3                      /* switch back to pattern 1         */
    bne     t3, t4, mem_test_errors /* observed != expected.            */
    li      t5, 0x3                 /* load the error code.             */
    addu    a1, 4                   /* are we over the boarder yet ?    */
    beq     a1, s1, 2f              /* See if we have reach the end.    */
    addu    a0, 4                   /* point to next word.              */
    bne     a0, s1, 1b              /* loop the loop again and again... */
    nop                             /* branch delay slot.               */
    b       3f                      /* this is the end.                 */
    nop                             /* branch delay slot.               */
2:  b       1b                      /* reset the second test address.   */
    move    a1, s0                  /* wrap around the test addr.       */
3:  nop                             /* place holder                     */


  # Address marching test
    la      a0, addr_test           /* load the test header message     */
    jal     printf                  /* print the header message         */
    nop
    
LOOP3:
    la      a0, addr_test1          /* print the subtitle.              */
    jal     printf                  /* go print it.                     */
    move    t6, t5                  /* save the indicator.              */
    move    a1, s0                  /* load the test address.           */
    not     t3, a1                  /* invert the test addr.            */
1:  bal     heartBeat               /* go print indicator.              */
    move    a0, t6                  /* set the indicator.               */
    move    t6, v0                  /* save the new indicator.          */
    dsll32  t3, 0                   /* remove bits <63:32>              */
    dsrl32  t3, 0                   /* shift back to <31:0>             */
    dsll32  t5, a1, 0               /* get the test addr in <63:32>     */
    or      t3, t3, t5              /* for a 64 bit data pattern.       */
    sd      t3, 0x0(a1)             /* write to test address.           */
    addu    a1, 8                   /* increment test address.          */
    bne     a1, s1, 1b              /* loop back for next test addr.    */
    not     t3, a1                  /* invert the test address.         */
    
    la      a0, addr_test2          /* load the sub-title format        */
    jal     printf                  /* go print it.                     */
    nop                             /* delay slot.                      */
    move    a1, s0                  /* load the test address.           */
    not     t3, a1                  /* invert the test addr.            */
1:  bal     heartBeat               /* go print indicator.              */
    move    a0, t6                  /* set the indicator.               */
    move    t6, v0                  /* save the new indicator.          */
    dsll32  t3, 0                   /* remove bits <63:32>              */
    dsrl32  t3, 0                   /* shift back to <31:0>             */
    dsll32  t5, a1, 0               /* get the test addr in <63:32>     */
    ld      t4, 0x0(a1)             /* read from test address.          */
    or      t3, t3, t5              /* for a 64 bit data pattern.       */
    bne     t3, t4, mem_test_errors /* observed != expected.            */
    li      t5, 0x4                 /* load the error code.             */
    not     t4                      /* invert the test data.            */
    sd      t4, 0x0(a1)             /* write back to test addr.         */
    addu    a1, 8                   /* increment test address.          */
    bne     a1, s1, 1b              /* loop back for next test addr.    */
    not     t3, a1                  /* invert the test address.         */
    
    la      a0, addr_test3          /* load the sub-title.              */
    jal     printf                  /* and print it.                    */
    nop                             /* delay slot.                      */
    move    a1, s0                  /* load the test address.           */
    not     t3, a1                  /* invert the test addr.            */
1:  bal     heartBeat               /* go print indicator.              */
    move    a0, t6                  /* set the indicator.               */
    move    t6, v0                  /* save the new indicator.          */
    dsll32  t3, 0                   /* remove bits <63:32>              */
    dsrl32  t3, 0                   /* shift back to <31:0>             */
    dsll32  t5, a1, 0               /* get the test addr in <63:32>     */
    ld      t4, 0x0(a1)             /* read from test address.          */
    or      t3, t3, t5              /* for a 64 bit data pattern.       */
    not     t3                      /* inverted test pattern.           */
    bne     t3, t4, mem_test_errors /* observed != expected.            */
    li      t5, 0x5                 /* load the error code.             */
    addu    a1, 8                   /* increment test address.          */
    bne     a1, s1, 1b              /* loop back for next test addr.    */
    not     t3, a1                  /* invert the test address.         */
    
  # Restore a0 and a1 and return.
    b       simple_mem_rtn          /* return to main body.             */
    addu    sp, 0x50
    
#if 0
    lui     t5, 0xa000              /* address base of KSEG1            */
    subu    a0, s0, t5              /* turn off KSEG1 base addr.        */
    jr      s3                      /* return                           */
    subu    a1, s1, t5              /* The size of memory to be tested. */
#endif
    
  # Error handling routine, currently loop on error only.
mem_test_errors:
    move    s0, a1                  /* save the test address.           */
    move    s1, t3                  /* the expected data pattern        */
    move    s2, t4                  /* the observed data pattern        */
    
    lui     t0, 0xbf31              /* load the PROM access addrsss.    */
    ld      t1, 0x8(t0)             /* save original value              */
    li      t2, ~0x11               /* LED mask.                        */
    and     t2, a1, a2              /* Turn both on                     */
    ori     t2, 0x01                /* and leave one off.               */
    sd      t2, 0x8(t0)             /* turn on the red   led.           */
    
    la	    a0, mem_error_msg       /* load the error msg format.       */
    move    a1, t3                  /* the expected data pattern.       */
    jal	    printf                  /* print the error message.         */
    move    a2, t4                  /* and observed data pattern.       */
    
mem_error_loop:
    la      a0, loop                /* print loop message.              */
    jal     printf                  /* print the loop message.          */
    nop                             /* branch delay slot.               */
    lui     t1, 0x1                 /* the loop count.                  */
    move    t0, zero                /* load the loop count.             */
1:  sd      s1, 0x0(s0)             /* write expected data pattern      */
    addi    t0, 1                   /* increment the loop count.        */
    blt     t0, t1, 1b              /* loop the whole thing.            */
    ld      t2, 0x0(s0)             /* read the data back.              */
    bal     heartBeat               /* print I'm alive indicator.       */
    move    a0, t5                  /* set the loop indicator.          */
    b       1b                      /* re-load the loop count.          */
    move    t0, zero                /* set it back to zero.             */
    
    b	    mem_error_loop
    nop
    
    .end    simple_memtst
    
mem_walking_0:
    .asciiz "\n\n  Walking 0 test\n"	
mem_walking_1:
    .asciiz "\n\n  Walking 1 test\n"	
two_pattern_test:
    .asciiz "\n\n  Two pattern test   "
addr_test:
    .asciiz "\n\n\n  Addressing test\n"
addr_test1:
    .asciiz "        Write  address   "
addr_test2:
    .asciiz "\n        Verify address and write inverted address   "
addr_test3:
    .asciiz "\n        Verify inverted address   "
mem_error_msg:
    .asciiz "\n        <ERROR> expected=0x%0lx, obsev=0x%0lx, addr=0x%0lx\n"
loop:
    .asciiz "                Loop on Error   "
    
