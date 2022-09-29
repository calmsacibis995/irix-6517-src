/*
 *---------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:48:26 $
 *---------------------------------------------------------------
 */
 	
#include <sys/regdef.h>
#include <post1supt.h>


    .set	noreorder           /* the whole thing ought to be no   */
	
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
 * diag_error_exit
 *--------------
 * common exit point for all post1 diags.
 *--------------
 */
    .globl  diag_error_exit
    .ent    diag_error_exit
diag_error_exit:
 # diag detected some sort of error, since we are not going to 
 # do any recover from this let just write the PC to location 0
    la      v0, 0xa0000000
    b       diag_error_exit
    sd      ra, 0x0(v0)
    .end    diag_error_exit    
    
    
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
    
/*
 * ---------------------------------------------------
#
# $Log: post1asmsupt.s,v $
# Revision 1.1  1997/08/18 20:48:26  philw
# updated file from bonsai/patch2039 tree
#
# Revision 1.3  1996/10/21  19:17:45  kuang
# Merged in changes from Pulpwood IP32prom to bring bonsai IP32prom to Pulpwood IP32 PROM v2.4
#
# Revision 1.2  1995/11/28  02:16:45  kuang
# General cleanup
#
# Revision 1.1  1995/11/20  23:07:20  kuang
# initial checkin
#
 * ---------------------------------------------------
 */

/* END OF FILE */
