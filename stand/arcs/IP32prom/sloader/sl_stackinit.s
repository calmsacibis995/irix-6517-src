#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/IP32.h>
#include <IP32R10k.h>

#define	R5KTLBS          48
#define	R10KTLBS         64
#define	PAGE4K           0x1000
#define	R10KPIBLK        64
#define	R10KPDBLK        32
#define	R10KPCACH        0x8000
#define	R10KPCSET        0x4000
#define DIRTY            0x00C0
#define LRU              0x0008
#define SDIRTY           0x0C00
#define VINDEX           0x3000
#define V_SHIFT          5
#define S512K            0x80000
#define C0_IMP_R10000    0x09
#define	C0_IMPMASK	 0xff00
#define	C0_IMPSHIFT	 8


/**************
 * init_slstack
 *-------------
 * Initializes the loader stack area.
 * This code sizes the primary D cache using the config register, and then
 * invalidates it and marks it dirty.  The result is a block of
 * data space allocated in the primary D cache.  This space can be used
 * as the stack by the serial loader and other code.  It's important that
 * users of this space don't "fall out" of the initialized cache area as
 * then we would get a cache miss and an attempt to do a cache writeback
 * followed by a cache fill and if that occurs before the memory is
 * initialized and cleared, an error will likely result.
 */

/*
 *  s0 - Base address of the intended stack.
 *  sp - Top address of the stack.
 */
LEAF(init_slstack)
    .set    noreorder               /* disable assembler reordering     */
    mfc0    t0, C0_PRID             /* read the processor ID register   */
    .set    reorder                 /* enable assembler reordering      */
    li      t1, C0_IMP_R10000       /* the R10000 IMP number.           */
    andi    t0, C0_IMPMASK          /* get the inplementation num       */
    srl     t0, C0_IMPSHIFT         /* shift to bit 0.                  */
    bne     t0, t1, r5k_slstack_init /* its not R10000                  */
    b	    r10k_slstack_init       /* jump to r10k cpu init code       */

/*
 *  Initialize the r5k slstack.
 */
r5k_slstack_init:
    .set    noreorder               /* disable assembler reordering     */
    mtc0    zero, C0_TAGHI          /* initialize TagHi reg.            */
    .set    reorder                 /* enable assembler reordering      */
    move    t0, s0                  /* get the stack base address.      */
    .set    noat                    /* turn off AT macro replacement    */
1:  sll	    AT, t0, 3               /* remove upper 3 bits.             */
    srl	    AT, 15                  /* we have physical addr<35:12>     */
    srl	    AT, 8                   /* and move to bit<31:8>            */
    ori	    AT, DIRTY               /* make it dirty exclusive.         */
    .set    noreorder               /* disable assembler reordering     */
    mtc0    AT, C0_TAGLO            /* initialize TagLo reg.            */
    move    t1, t0                  /* save current virtual addr.       */
    addiu   t0, 32                  /* point to next block.             */
    cache   CACH_PD|C_IST, 0(t1)    /* initialize the Tag,              */
    .set    reorder                 /* enable assembler reordering      */
    sltu    AT, sp, t0              /* check the boundary.              */
    beqz    AT, 1b                  /* go do cnext block.               */
    .set    at                      /* turn on AT macro replacement.    */
    b	    flood_slstack           /* flood the stack with inverted    */
	                            /* stack address.                   */
/*
 *  Invalidate r10k slstack.
 */
r10k_slstack_init:
    .set    noreorder               /* disable assembler reordering     */
    mfc0    t1, C0_CONFIG           /* read the config register,        */
    mtc0    zero, C0_TAGHI          /* initialize TagHi register.       */
    andi    t1, CONFIG_SB           /* extract the block size.          */
    bgtz    t1, 1f                  /* figureout the right block size.  */
    li	    t0, 128                 /* 128 bytes - 32 words.            */
    li	    t0, 64                  /* 64 bytes  - 16 words.            */
    .set    noat                    /* turn off AT macro replacement.   */
1:  move    t1, s0                  /* get the stack base address.      */
                                    /* initialize the 2nd cache.        */
    .set    reorder                 /* enable assembler reordering      */
2:  sll	    AT, t1, 3               /* remove upper 3 bits.             */
    srl     AT, 21                  /* we have physicall addr<35:18>    */
    sll	    AT, 14                  /* and move to bit<31:14>           */
    ori	    AT, SDIRTY		    /* also make the line dirty.        */
    andi    t2, t1, VINDEX	    /* extract VA<13:12> for Vindex.    */
    srl     t2, V_SHIFT             /* move to TagLo<8:7>               */
    or 	    t2, AT                  /* also make the line dirty.        */
    .set    noreorder               /* disable assembler reordering     */
    mtc0    t2, C0_TAGLO            /* initialize TagLo reg.            */
    nop                             /* delay slot.                      */
    addu    t3, t1, t0              /* point to next block.             */
    cache   CACH_SD|C_IST, 0(t1)    /* initialize secondary cache tag   */
    sltu    AT, sp, t3              /* Loop for the whole stack.        */
    beqz    AT, 2b                  /* are we done yet??                */
    move    t1, t3                  /* pass the address back.           */
                                    /* do primary cache.                */
    lui     t0, 0x4000              /* initialize the TagHi reg with    */
    mtc0    t0, C0_TAGHI            /* 010 state modifier.              */
    move    t1, s0                  /* get the base address again.      */
3:  sll	    AT, t1, 3               /* remove upper 3 bits.             */
    srl     AT, 15                  /* we have physicall addr<35:12>    */
    sll	    AT, 8                   /* and move to bit<31:8>            */
    ori	    AT, (DIRTY|LRU)         /* also make the line dirty.        */
    mtc0    AT, C0_TAGLO            /* initialize TagLo reg.            */
    nop                             /* give one delay slot.             */
    addiu   t2, t1, R10KPDBLK       /* delay slot required.             */
    sltu    AT, sp, t2              /* check the subblock boundary.     */
    cache   CACH_PD|C_IST, 0(t1)    /* initialize primary cache Tag,    */
    nop                             /* one nop just in case.            */
    bnez    AT, 3b                  /* we are done with pcache yet.     */
    move    t1, t2                  /* pass alone the next block addr.  */
    .set    reorder                 /* enable assembler reordering      */
    .set    at                      /* turn on  AT macro replacement.   */
    b	    flood_slstack           /* flood the stack with inverted    */
	                            /* stack address.                   */
/*
 *  Flood the stack in cache with the inverted address.
 *  s0 - Base address of the intended stack.
 *  sp - Top address of the stack.
 */
flood_slstack:
    subu    t0, sp, 8               /* load the starting address.       */
    .set    noreorder               /* disable reordering               */
    nop                             /* give one nop just in case.       */
1:  not	    t1, t0                  /* inverted the address.            */
    subu    t2, t0, 8               /* point to next word in stack      */
    sd	    t1, 0(t0)               /* then initialize it.              */
    bne	    t2, s0, 1b              /* loop for whold stack             */
    move    t0, t2                  /* pass it back.                    */
    .set    reorder                 /* enable reordering.               */
    jr	    ra                      /* done!                            */
    END(init_slstack)
    
/* END OF FILE */
