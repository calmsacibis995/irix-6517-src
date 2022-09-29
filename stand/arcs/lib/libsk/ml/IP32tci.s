
/**************************************************************************
 *                                                                        *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/IP32.h>
#include <IP32R10k.h>

#define	R5KTLBS          48
#define	R10KTLBS         64
#define	PAGE4K           0x1000
#define	TLBUNCACH        0x11
#define	R10KPIBLK        64
#define	R10KPDBLK        32
#define	R10KPCACH        0x8000
#define	R10KPCSET        0x4000
#define DIRTY            0x00C0
#define LRU              0x0004
#define SDIRTY           0x0C00
#define VINDEX           0x3000
#define V_SHIFT          5
#define S512K            0x80000
#define M256M            0x10000000
#define TLB512M          0x800000   /* (M512M >> 12) << 6               */
#define CACH512M         0x80800000 

#define C0_IMP_R5000     0x23
#define C0_IMP_RM5271	 0x28
#define	C0_IMPMASK	 0xff00
#define	C0_IMPSHIFT	 8

/*
 * Stuff from sys/sbd.h
 */
#define CONFIG_TR_SS	0x00300000  /* Triton SS (2nd cache size)    */
#define CONFIG_TR_SC	CONFIG_SC   /* Triton SC (2nd cache present) */
#define CONFIG_TR_SE    0x00001000  /* Triton SE (2nd cache enabled) */

/*
 *  Constant used for stack frames.
 */
#define framesize   176
#define inargs      176         
#define frameoffset  32         /* allow 4 doubleword.              */
#define localargs   168         /* sp+framesize-0x08, max 4  dwords */
#define regsave     136         /* sp+framesize-(4*8)-0x8, 16 regs  */
#define outargs       8         /* sp+framesize-(4*8)-(16*8)-0x8    */

/*
 *  We assume that there is no stack available to us, caller have to save 
 *  following registers - t0, t1, t2, t3
 *  Figure what kind of processor we are running at.
 */
LEAF(processor_tci)
    lui     t0, 0xa000              /* following code have to run in    */
    la      t1, 1f                  /* uncached mode.                   */
    or      t0, t1                  /* create uncached address.         */
    jr      t0                      /* branch to uncached address.      */
    .set    noreorder               /* disable assembler reordering     */
1:  mtc0    zero, C0_PGMASK         /* zap the mask register.           */
    mtc0    zero, C0_TAGHI          /* zap the TagHi register.          */
    li      t1, C0_IMP_R5000        /* the R5000 IMP number.            */
    nop                             /* one more nop just in case.       */
    mfc0    t0, C0_PRID             /* read the processor ID register   */
    .set    reorder                 /* enable assembler reordering      */
    andi    t0, C0_IMPMASK          /* get the inplementation num       */
    srl     t0, C0_IMPSHIFT         /* shift to bit 0.                  */
    bne     t0, t1, r10k_tci        /* its not an r5k processor.        */
    li      t1, C0_IMP_RM5271
    bne     t0, t1, r10k_tci        /* its R10K processor.              */


/*
 *  Invalidate r5k tlbs.
 */
r5k_tci:
    li      t0, R5KTLBS-1           /* the TLB index.                   */
    lui     t1, M256M>>16           /* the Virtual and Physical addr    */
    .set    noat                    /* disable AT macro replacement.    */
1:  li      AT, 0x1fff              /* clear the lower addr<12:0>       */
    not     t2, AT                  /* create the mask.                 */
    and     t2, t1                  /* this is VPN2 for EntryHi reg.    */
    lui     AT, 0x8000              /* load the kseg0 address.          */
    .set    noreorder               /* disable assembler reordering     */
    mtc0    t0, C0_INX              /* update the index register.       */
    or      t2, AT                  /* we have the real VPN2            */
    mtc0    t2, C0_TLBHI            /* update the EntryHi register.     */
    srl     AT, t1, 12              /* extract addr<35:12>              */
    sll     AT, 6                   /* and move it to bit<29:6>         */
    ori     AT, TLBUNCACH           /* and add the tlb attribute to it. */
    mtc0    AT, C0_TLBLO_0          /* Initialize EntryLo0              */
    addiu   t2, AT, PAGE4K>>6       /* point to next 4K block.          */
    mtc0    AT, C0_TLBLO_1          /* Initialize EntryLo1              */
    .set    at                      /* enable AT macro replacement.     */
    sub     t0, 1                   /* decrement the tlb counter        */
    subu    t1, PAGE4K<<1           /* point to next 8k block.          */
    bgtz    t0, 1b                  /* loop for all 48 entries.         */
    tlbwi                           /* initialize current indexed tlb   */
    .set    reorder                 /* enable assembler reordering      */
    
/*
 *  Disable secondary cache.
 */
    .set    noreorder               /* disable assembler reordering     */
    mfc0    t0, C0_CONFIG           /* read the config register.        */
    .set    reorder                 /* enable assembler reordering      */
    and	    t0, ~CONFIG_TR_SE       /* set bit<12> to zero, disable 2nd */
    .set    noreorder               /* cache,  assembler reordering     */
    mtc0    t0, C0_CONFIG           /* write to config register.        */
    .set    reorder                 /* enable assembler reordering      */
    
/*
 *  Invalidate the r5k primary I cache.
 */
    .set    noreorder               /* disable assembler reordering     */
    mfc0    t0, C0_CONFIG           /* read the config register.        */
    .set    reorder                 /* enable assembler reordering      */
    li      t1, 0x1000              /* the 4K base cache size.          */
    srl     t0, 9                   /* move it to bit <0:3>             */
    andi    t0, 0x7                 /* and clear everything else.       */
    sllv    t0, t1, t0              /* now we have the size.            */
    sub     t0, 32                  /* point to the last line.          */
    lui     t1, 0x8000              /* use kseg0 address.               */
    addu    t2, t0, t1              /* the target cache line.           */
    .set    noreorder               /* disable assembler reordering     */
1:  cache   CACH_PI|C_IINV, 0x0(t2) /* invalidate the line.             */
    .set    reorder                 /* enable assembler reordering      */
    sub     t0, 32                  /* point to next line.              */
    addu    t2, t0, t1              /* the target cache line.           */
    bgez    t0, 1b                  /* loop till done.                  */

/*
 *  Invalidate the r5k primary D cache.
 */
    .set    noreorder               /* disable assembler reordering     */
    mfc0    t0, C0_CONFIG           /* read the config register.        */
    .set    reorder                 /* enable assembler reordering      */
    li      t1, 0x1000              /* the 4K base cache size.          */
    srl     t0, 6                   /* move it to bit <0:3>             */
    andi    t0, 0x7                 /* and clear everything else.       */
    sllv    t0, t1, t0              /* now we have the size.            */
    sub     t0, 32                  /* point to the last line.          */
    lui     t1, 0x8000              /* use kseg0 address.               */
    .set    noat                    /* turn off AT macro replacement.   */
    .set    noreorder               /* disable assembler reordering     */
    lui	    AT, (M256M >>16)        /* mapped to 512M addr              */
1:  addu    AT, AT, t0              /* add to the base addr.            */
    srl     AT, 12                  /* extract addr<35:12>              */
    sll     AT, 8                   /* and move it to TagLo<31:8>       */
    mtc0    AT, C0_TAGLO            /* and initialize the TagLo reg.    */
    addu    t2, t0, t1              /* the target cache line.           */
    sub     t0, 32                  /* point to next line.              */
    cache   CACH_PD|C_IST, 0x0(t2)  /* invalidate the line.             */
    bgez    t0, 1b                  /* loop till done.                  */
    lui	    AT, (M256M >>16)        /* mapped to 256M addr              */
    .set    reorder                 /* enable assembler reordering      */
    .set    at                      /* turn on AT macro replacement.    */

/*
 *  Done with the r5k cpu initialization.
 */
    jr	    ra                      /* return to caller                 */


/*
 *  Invalidate r10k tlbs.
 */
r10k_tci:
    li      t0, R10KTLBS-1          /* the TLB index.                   */
    lui     t1, M256M>>16           /* the Virtual and Physical addr    */
    .set    noat                    /* disable AT macro replacement.    */
1:  li      AT, 0x1fff              /* clear the lower addr<12:0>       */
    not     t2, AT                  /* create the mask.                 */
    and     t2, t1                  /* this is VPN2 for EntryHi reg.    */
    lui     AT, 0xa000              /* load the kseg1 address.          */
    .set    noreorder               /* disable assembler reordering     */
    mtc0    t0, C0_INX              /* update the index register.       */
    or      t2, AT                  /* we have the real VPN2            */
    mtc0    t2, C0_TLBHI            /* update the EntryHi register.     */
    srl     AT, t1, 12              /* extract addr<35:12>              */
    sll     AT, 6                   /* and move it to bit<29:6>         */
    ori     AT, TLBUNCACH           /* and add the tlb attribute to it. */
    mtc0    AT, C0_TLBLO_0          /* Initialize EntryLo0              */
    addiu   t2, AT, PAGE4K>>6       /* point to next 4K block.          */
    mtc0    AT, C0_TLBLO_1          /* Initialize EntryLo1              */
    .set    at                      /* enable AT macro replacement.     */
    sub     t0, 1                   /* decrement the tlb counter        */
    subu    t1, PAGE4K<<1           /* point to next 8k block.          */
    bgtz    t0, 1b                  /* loop for all 48 entries.         */
    tlbwi                           /* initialize current indexed tlb   */
    .set    reorder                 /* enable assembler reordering      */

#define ZEROINIT 1
#if defined(ZEROINIT)
/*
 *  Invalidate r10k primary instruction cache entries.
 */
    .set    noreorder               /* disable assembler reordering.    */
    mtc0    zero, C0_TAGHI          /* zap the TagHi register.          */
    mtc0    zero, C0_TAGLO          /* and TagLo register.              */
    mtc0    zero, C0_ECC            /* and ECC register.                */
    lui     t0, 0x8000              /* use kseg0 address.               */
    li      t1, R10KPCSET-R10KPIBLK /* the last line in the set.        */
1:  or      t2, t0, t1              /* the cache line index.            */
    cache   CACH_PI|C_IST, 0(t2)    /* zap set 0 tag.                   */
    sub     t1, R10KPIBLK           /* decrement the line index.        */
    cache   CACH_PI|C_IST, 1(t2)    /* zap set 1 tag.                   */
    bgez    t1, 1b                  /* loop the whole set.              */
    nop                             /* synch the cache operations.      */
    .set    reorder                 /* enable assembler reordering      */
    
/*
 *  Invalidate r10k primary data cache entries.
 */
    .set    noreorder               /* disable assembler reordering.    */
    mtc0    zero, C0_TAGHI          /* zap the TagHi register.          */
    mtc0    zero, C0_TAGLO          /* and TagLo register.              */
    mtc0    zero, C0_ECC            /* and ECC register.                */
    lui     t0, 0x8000              /* use kseg0 address.               */
    li      t1, R10KPCSET-R10KPDBLK /* the last line in the set.        */
1:  or      t2, t0, t1              /* the cache line index.            */
    cache   CACH_PD|C_IST, 0(t2)    /* zap set 0 tag.                   */
    sub     t1, R10KPDBLK           /* decrement the line index.        */
    cache   CACH_PD|C_IST, 1(t2)    /* zap set 1 tag.                   */
    bgez    t1, 1b                  /* loop the whole set.              */
    nop                             /* synch the cache operations.      */
    .set    reorder                 /* enable assembler reordering      */
    
/*
 *  Invalidate r10k secondary cache entries.
 */
    .set    noreorder               /* disable assembler reordering     */
3:  mfc0    t0, C0_CONFIG           /* read the config register.        */
    lui     t1, S512K>>16           /* the base size of the scache.     */
    srl     t2, t0, CONFIG_SS_SHFT  /* shift to bit 0                   */
    andi    t2, 0x7                 /* mask off unused bits.            */
4:  bgt     t2, 5, 4b               /* Die here for reserved size.      */
    nop                             /* branch delay slot.               */
    sllv    t0, t1, t2              /* scache size in byte in t0        */
    .set    noat                    /* turn off at replacement          */
    andi    AT, t0, CONFIG_SB       /* check scache block size.         */
    li      t2, 128                 /* 128 bytes -  32 words            */
    bgtz    AT, 5f                  /* block size of scache             */
    nop                             /* branch delay slot.               */
    li      t2, 64                  /* 64 bytes  -  16 words            */
    .set    at                      /* turn on at replacement.          */
5:  mtc0    zero, C0_TAGHI          /* zap the TagHi register.          */
    mtc0    zero, C0_TAGLO          /* and TagLo register.              */
    srl     t1, t0, 1               /* create the set size in t1        */
    mtc0    zero, C0_ECC            /* and ECC register.                */
    sub     t1, t2                  /* the last line within a set.      */
6:  lui     t3, 0x8000              /* use kseg0 address space          */
    or      t3, t1                  /* the scache line index.           */
    cache   CACH_SD|C_IST, 0(t3)    /* initialize set 0.                */
    sub     t1, t2                  /* point to next block.             */
    cache   CACH_SD|C_IST, 1(t3)    /* initialize set 1.                */
    bgez    t1, 6b                  /* loop for whole set.              */
    nop                             /* branch delay slot.               */
    .set    reorder                 /* enable assembler reordering      */

/*
 *  Note: Do not alter the sequence of the code, T5 is very picky
 *        about it.
 *  Now initialize the data ram, Primary data.
 */
    .set    noreorder               /* turn off the assembler reordering */
10: mtc0    zero, C0_ECC            /* clear the ECC register.          */
    mtc0    zero, C0_TAGHI          /* clear the TagHi register.        */
    mtc0    zero, C0_TAGLO          /* and also clear the taglo.        */
    nop                             /* branch delay slot.               */
    move    t0, zero                /* start with location 0.           */
11: lui     t1, 0x8000              /* the virtual address.             */
    or      t1, t0                  /* we have a winner.                */
    cache   CACH_PD|C_ISD, 0(t1)    /* write to set 0.                  */
    addi    t0, 4                   /* point to next word.              */
    li      t2, R10KPCSET           /* the last word in the set.        */
    cache   CACH_PD|C_ISD, 1(t1)    /* write to set 1.                  */
    blt     t0, t2, 11b             /* loop for whole set.              */
    nop                             /* branch delay slot.               */
    .set    reorder                 /* turn on reordering.              */

/*
 *  And Primary Instruction.
 */
    .set    noreorder               /* turn off the assembler reordering */
10: mtc0    zero, C0_ECC            /* clear the ECC register.          */
    mtc0    zero, C0_TAGHI          /* clear the TagHi register.        */
    mtc0    zero, C0_TAGLO          /* and also clear the taglo.        */
    nop                             /* branch delay slot.               */
    move    t0, zero                /* start with location 0.           */
11: lui     t1, 0x8000              /* the virtual address.             */
    or      t1, t0                  /* we have a winner.                */
    cache   CACH_PI|C_ISD, 0(t1)    /* write to set 0.                  */
    addi    t0, 4                   /* point to next word.              */
    li      t2, R10KPCSET           /* the last word in the set.        */
    cache   CACH_PI|C_ISD, 1(t1)    /* write to set 1.                  */
    blt     t0, t2, 11b             /* loop for whole set.              */
    nop                             /* branch delay slot.               */
    .set    reorder                 /* turn on reordering.              */

/*
 *  Now secondary cache data ram.
 */
    .set    noreorder               /* turn off the assembler reordering */
    mfc0    t0, C0_CONFIG           /* read the config register.        */
    lui     t1, S512K>>16           /* the base size of the scache.     */
    srl     t2, t0, CONFIG_SS_SHFT  /* shift to bit 0                   */
    andi    t2, 0x7                 /* mask off unused bits.            */
4:  bgt     t2, 5, 4b               /* Die here for reserved size.      */
    nop                             /* branch delay slot.               */
    sllv    t0, t1, t2              /* scache size in byte in t0        */
    mtc0    zero, C0_ECC            /* clear the ECC register.          */
    srl     t2, t0, 1               /* create the set size in t2.       */
    mtc0    zero, C0_TAGHI          /* clear the TagHi register.        */
    mtc0    zero, C0_TAGLO          /* and also clear the taglo.        */
    move    t0, zero                /* start with location 0.           */
    .set    noat                    /* disable AT macro replacement.    */
11: lui     AT, 0x8000              /* use kseg0 address space.         */
    or      t1, t0, AT              /* we have a winner.                */
    .set    at                      /* enable AT macro replacement.     */
    cache   CACH_SD|C_ISD, 0(t1)    /* write to set 0                   */
    addi    t0, 16                  /* point ot next quardword.         */
    cache   CACH_SD|C_ISD, 1(t1)    /* write to set 1                   */
    blt     t0, t2, 11b             /* loop for whold set.              */
    nop                             /* branch delay slot.               */
    .set    reorder                 /* turn on reordering.              */
/*
 *  Cache initialization are done!
 */
    b       Done                    /* OKOK!!                           */
    
#else	    /* if not defined ZEROINIT */
/*
 *  Invalidate r10k primary D cache entries.
 */
    lui     t1, 0x2000              /* set state modifier to 001        */
    .set    noreorder               /* disable assembler reordering     */
    mtc0    t1, C0_TAGHI            /* initialize TAGHI register.       */
    .set    reorder                 /* enable assembler reordering      */
    li      t2, 1                   /* the set index.                   */
    li      t0, R10KPCACH-R10KPDBLK /* the last line address.           */
1:  li      t1, R10KPCSET-R10KPDBLK /* the last line in the set.        */
2:  srl     t3, t0, 12              /* generate addr<35:12>             */
    sll     t3, 8                   /* move to TagLo<31:8>              */
    ori     t3, LRU                 /* indicate it least recently used. */
    .set    noreorder               /* disable assembler reordering     */
    mtc0    t3, C0_TAGLO            /* initialize TagLo register.       */
    subu    t1, R10KPDBLK           /* decrement the set index.         */
    .set    noat                    /* turn off at replacement          */
    lui     AT, 0x8000              /* use kseg0 address.               */
    addu    AT, AT, t0              /* the virtual address.             */
    or      AT, t2                  /* or in the set index.             */
    cache   CACH_PD|C_IST, 0(AT)    /* update the cache tags,           */
    .set    at                      /* turn on at replacement.          */
    .set    reorder                 /* enable assembler reordering      */
    subu    t0, R10KPDBLK           /* decrement the virtual address.   */
    bgez    t1, 2b                  /* do next block.                   */
    beqz    t2, 3f                  /* are we done with both set ??     */
    move    t2, zero                /* zap the set index.               */
    b       1b                      /* do set 0                         */
    
/*
 *  Invalidate r10k primary I cache entries.
 */
    .set    noreorder               /* disable assembler reordering     */
3:  mtc0    zero, C0_TAGHI          /* initialize TAGHI register.       */
    .set    reorder                 /* enable assembler reordering      */
    li      t2, 1                   /* the set index.                   */
    li      t0, R10KPCACH-R10KPIBLK /* the last line address.           */
1:  li      t1, R10KPCSET-R10KPIBLK /* the last line in the set.        */
2:  srl     t3, t0, 12              /* generate addr<35:12>             */
    sll     t3, 8                   /* move to TagLo<31:8>              */
    ori     t3, LRU                 /* indicate it least recently used. */
    .set    noreorder               /* disable assembler reordering     */
    mtc0    t3, C0_TAGLO            /* initialize TagLo register.       */
    subu    t1, R10KPIBLK           /* decrement the set index.         */
    .set    noat                    /* turn off at replacement          */
    lui     AT, 0x8000              /* use kseg0 address.               */
    addu    AT, AT, t0              /* the virtual address.             */
    or      AT, t2                  /* or in the set index.             */
    cache   CACH_PI|C_IST, 0(AT)    /* update the cache tags,           */
    .set    at                      /* turn on at replacement.          */
    .set    reorder                 /* enable assembler reordering      */
    subu    t0, R10KPIBLK           /* decrement the virtual address.   */
    bgez    t1, 2b                  /* do next block.                   */
    beqz    t2, 3f                  /* are we done with both set ??     */
    move    t2, zero                /* zap the set index.               */
    b       1b                      /* do set 0                         */

/*
 *  Invalidate r10k secondary cache entries.
 */
    .set    noreorder               /* disable assembler reordering     */
3:  mfc0    t0, C0_CONFIG           /* read the config register.        */
    mtc0    zero, C0_TAGHI          /* MRU=0, STag1=0 for TAGHI         */
    .set    reorder                 /* enable assembler reordering      */
    lui     t1, S512K>>16           /* the base size of the scache.     */
    srl     t2, t0, CONFIG_SS_SHFT  /* shift to bit 0                   */
    andi    t2, 0x7                 /* mask off unused bits.            */
4:  bgt     t2, 5, 4b               /* Die here for reserved size.      */
    sllv    t1, t1, t2              /* scache size in byte.             */
    .set    noat                    /* turn off at replacement          */
    andi    AT, t0, CONFIG_SB       /* check scache block size.         */
    bgtz    AT, 1f                  /* block size of scache             */
    li      t2, 128                 /* 128 bytes -  32 words            */
    li      t2, 64                  /* 64 bytes  -  16 words            */
    .set    at                      /* turn on at replacement.          */
1:  sub     t0, t1, t2              /* the last line in the cache.      */
    srl     t1, 1                   /* create the set size.             */
    sub     t1, t2                  /* the last line within a set.      */
    .set    noreorder               /* disable assembler reordering     */
    mtc0    t1, C0_LLADDR           /* save the set size.               */
    .set    reorder                 /* enable assembler reordering      */
    li      t3, 1                   /* do set 1 first.                  */
                                    /* loops both set of the scache.    */
    .set    noat                    /* turn off at replacement.         */
2:  li      AT, 0x1fffffff          /* mask for addr<28:0>              */
    and     AT, AT, t0              /* this is the physical addr.       */
    srl     AT, 18                  /* extract bits<35:18>              */
    sll     AT, 13                  /* and move to bits<31:13>          */
    .set    noreorder               /* disable assembler reordering     */
    mtc0    AT, C0_TAGLO            /* update the TAGLO reg.            */
    sub     t1, t1, t2              /* decrement the block count.       */
    lui     AT, 0x8000              /* use kseg0 address space.         */
    addu    AT, AT, t0              /* the cache op virtual address.    */
    or      AT, AT, t3              /* or in set index.                 */
    cache   CACH_SD|C_IST, 0x0(AT)  /* update the scache tag.           */
    .set    reorder                 /* enable assembler reordering      */
    .set    at                      /* turn on at replacement.          */
    sub     t0, t0, t2              /* descrment the physical address.  */
    bgez    t1, 2b                  /* loop for one set.                */
    beqz    t3, Done                /* do set 0                         */
    .set    noreorder               /* disable assembler reordering     */
    mfc0    t1, C0_LLADDR           /* Reload the set size.             */
    .set    reorder                 /* enable assembler reordering      */
    move    t3, zero                /* re-initialize set index.         */
    b	    2b			    /* go back for set 0.               */
#endif
#undef ZEROINIT

/*
 *  This is the end of r10k cache and tlb initialization.
 */
Done:
    jr	    ra                      /* Return to caller                 */
    END(processor_tci)
    
/*
 *  This is the entry point where the stack is available to us.
 */
#define minimFSIZE 48
NESTED(IP32processorTCI,framesize,ra)
    .set    noat                    /* turn off AT macro replacement    */
    subu    sp, minimFSIZE          /* create current stack frame.      */
    sw	    ra, minimFSIZE-4(sp)    /* Save return address on stack     */
    sw	    AT, minimFSIZE-8(sp)    /* save AT                          */
    sw	    t0,minimFSIZE-12(sp)    /* Save t0                          */
    sw	    t1,minimFSIZE-16(sp)    /* Save t1                          */
    sw	    t2,minimFSIZE-20(sp)    /* Save t2                          */
    sw	    t3,minimFSIZE-24(sp)    /* Save t3                          */
    sw	    ta0,minimFSIZE-28(sp)    /* Save ta0                          */
    bal	    processor_tci           /* call the real function.          */
    lw	    ra, minimFSIZE-4(sp)    /* Load return address on stack     */
    lw	    AT, minimFSIZE-8(sp)    /* Load AT                          */
    lw	    t0,minimFSIZE-12(sp)    /* Load t0                          */
    lw	    t1,minimFSIZE-16(sp)    /* Load t1                          */
    lw	    t2,minimFSIZE-20(sp)    /* Load t2                          */
    lw	    t3,minimFSIZE-24(sp)    /* Load t3                          */
    lw	    ta0,minimFSIZE-28(sp)    /* Load ta0                          */
    addiu   sp, minimFSIZE          /* restore caller's stack frame.    */
    jr	    ra                      /* return to caller                 */
    END(IP32processorTCI)           /* this is the end of it.           */
    .set    at                      /* turn on AT  macro replacement    */

/* END OF FILE */
