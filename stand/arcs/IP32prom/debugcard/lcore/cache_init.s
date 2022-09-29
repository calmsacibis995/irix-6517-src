/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:42:01 $
 * -------------------------------------------------------------
 *  descriptions:   Initialize the primary cache to a known state
 */
 
#include <sys/asm.h>
#include <sys/regdef.h>
#include <cp0regdef.h>
#include <cacheops.h>
#include <debugcard.h>
#include <asm_support.h>

/*
 *  Define all external functions.
 */
.extern  UARTinit      0
.extern  UARTstatus    0
.extern  printf        0
.extern  cmdloop       0
.extern  _jump2Flash   0

/*
 * Special macros are defined here
 */
/* #define DebugCache */
/* #define SeeConfig  */


/*
 *  We'll have to find out what kind processor we are running, and 
 *  initialize the Cache entries accordingly.
 *  Note:
 *    Cache state - 0 invalid, 1 Shared, 2 Clean exclusive
 *                  3 dirty exclusive.
 */
    .globl  cache_init
    .ent    cache_init
cache_init:
    .set    noreorder
    mfc0    k0, Config              /* read the configuration reg.      */
    nop                             /* cp0 read delay.                  */
    nop                             /* cp0 read delay.                  */
    nop                             /* cp0 read delay.                  */
    nop                             /* cp0 read delay.                  */
    
    mfc0    a0, Status              /* Reading the status register.     */
    lui     a1, 0x1                 /* Set the DE bit to disable cache  */
    nop                             /* parity during the initialization */
    nop                             /* need at least 4 nops             */
    nop                             /* need at least 4 nops             */
    or      a0, a0, a1              /* set the DE bit.                  */
    mtc0    a0, Status              /* Update the status register.      */
    nop                             /* give another 4 nops              */
    nop                             /* give another 4 nops              */
    nop                             /* leave the parity off when        */
    nop                             /* booting the PROM.                */
    
#if defined(SeeConfig)
    lui     a0, 0xbff0
    sw      k0, 0x500(a0)
#endif

    li      a0, 0x7                 /* Make sure its writeback cache.   */
    not     a0                      /* Inverted K0 mask.                */
    and     a0, a0, k0              /* clear previous K0 field.         */
    ori     a0, 0x3                 /* Make it cacheable writeback.     */
    mtc0    a0, Config              /* write it back.                   */
    li      a1, 0x1c0               /* Now check the Data cache size.   */
    and     a1, a1, k0              /* Mask off rest of the bits.       */
    srl     a1, 6                   /* Shift in place.                  */
    li      a0, 0x1000              /* Load the base cache size.        */
    sllv    v0, a0, a1              /* $v0 has the primary cache size   */
    sub     a0, v0, 0x20            /* The last line in the cache.      */
    mtc0    zero, TagHi             /* Clear the TagHi register.        */
    
1:  srl     k0, a0, 12              /* Create PTagLo                    */
    sll     k0, 8                   /* $k0 has the PTagLo of current    */
    ori     k0, 0xc0                /* line, with dirty exclusive state */
#if defined(DEBUGCARDSTACK)         /* Use non existence memory region  */
    lui     a1, 0x8000              /* set the PTagLo<31>               */
    or      k0, k0, a1              /* VA[0] -> PA[0x800000000]         */
#endif                              /* used by debugcard stack.         */
    mtc0    k0, TagLo               /* load the TagLo register.         */
    nop                             /* at least 4 nops                  */
#if defined(DEBUGCARDSTACK)         /* create the VA                    */
    move    a1, zero                /* do not required the kseg0 prefix */
#else                               /* tlb_init routine.                */
    lui     a1, 0x8000              /* Kseg0 address space.             */
#endif
    or      a2, a0, a1              /* Create kseg0 line address.       */
    cache   (PD|IST), 0x0(a2)       /* Create the dirty cache line.     */
    
#if defined(DebugCache)
    lui     t1, 0xa000              /* Write Tags to LA.                */
    sw      a2, 0x100(t1)           /* Push out to SysAD                */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    mtc0    zero, TagLo             /* clear Taglo register.            */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    cache   (PD|ILT), 0x0(a2)       /* Read the Tags back.              */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    mfc0    t0, TagLo               /* Read the TagLo register.         */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    li      t2, 0x3                 /* ignore F and P bits.             */
    not     t2                      /* create the mask.                 */
    and     t2, t2, t0              /* TagLo without F and P            */
9:  sw      t0, 0x200(t1)           /* Push to SysAD                    */
    sw      t2, 0x300(t1)           /* Write SysAD                      */
    bne     t2, k0, 9b              /* Can not create tags              */
    sw      k0, 0x400(t1)           /* and expected taglo.              */
#endif
    
    li      t0, 4                   /* there are 4 double in a line.    */
    move    a3, a2                  /* Save current line address.       */
2:  not     k1, a3                  /* Create 64 bits data.             */
    dsll32  k1, 0                   /* move to upper 32 bits.           */
    or      k1, k1, a3              /* create the data                  */
    sd      k1, 0x0(a3)             /* this write should be cache hit.  */
    sub     t0, 1                   /* decrement the counter.           */
    bgtz    t0, 2b                  /* loop for 4 doubles.              */
    addu    a3, 0x8                 /* point to next double word.       */
    
    sub     a0, 0x20                /* point to previous line.          */
    bgez    a0, 1b                  /* loop for the whole cache.        */
    mtc0    zero, TagHi             /* clear TagHi.                     */
    
    srl     a0, v0, 1               /* Use half of the cache as stack.  */
    sub     sp, a0, 0x40            /* point to the last line           */
    or      gp, sp, a1              /* also Create globalparm           */
    move    sp, gp                  /* and stack pointer.               */
    sw      a0, PCACHESET(gp)       /* save pcache/2    in globalparms  */
    sw      zero, GLOBALINTR(gp)    /* save pcache/2    in globalparms  */
    sw      zero, USERINTR(gp)      /* save pcache/2    in globalparms  */
    sw      zero, INTRCOUNT(gp)     /* save pcache/2    in globalparms  */
    sw      zero, SAVED_SP(gp)      /* save pcache/2    in globalparms  */
    sw      zero, USRINTRFLG(gp)    /* save pcache/2    in globalparms  */
    sw      zero, LowPROMFlag(gp)   /* save pcache/2    in globalparms  */
    sw      zero, 0x20(gp)          /* save pcache/2    in globalparms  */
    sw      zero, 0x24(gp)          /* save pcache/2    in globalparms  */
    sw      zero, 0x28(gp)          /* save pcache/2    in globalparms  */
    sw      zero, 0x2c(gp)          /* save pcache/2    in globalparms  */
    sw      zero, 0x30(gp)          /* save pcache/2    in globalparms  */
    sw      zero, 0x34(gp)          /* save pcache/2    in globalparms  */
    sw      zero, 0x38(gp)          /* save pcache/2    in globalparms  */
    sw      zero, 0x3c(gp)          /* save pcache/2    in globalparms  */
    
#if defined(DdbugCache)    
    lui     t0, 0xa000              /* Write SysAD                      */
    sd      sp, 0x800(t0)           /* Push it out.                     */
#endif    

    jr      ra                      /* Return to caller.                */
    nop
    
.end cache_init

/*
 *  We'll have to find out what kind processor we are running, and 
 *  initialize the Cache entries accordingly to invalidate state and
 *  disable the cache parity.
 *  Note:
 *    Cache state - 0 invalid, 1 Shared, 2 Clean exclusive
 *                  3 dirty exclusive.
 */
.globl	    cache_invalidate
.ent	    cache_invalidate
cache_invalidate:
    mfc0    k0, Config              /* read the configuration reg.      */
    nop                             /* cp0 read delay.                  */
    nop                             /* cp0 read delay.                  */
    nop                             /* cp0 read delay.                  */
    nop                             /* cp0 read delay.                  */
    
    mfc0    a0, Status              /* Reading the status register.     */
    lui     a1, 0x1                 /* Set the DE bit to disable cache  */
    nop                             /* parity during the initialization */
    nop                             /* need at least 4 nops             */
    nop                             /* need at least 4 nops             */
    or      a0, a0, a1              /* set the DE bit.                  */
    mtc0    a1, Status              /* Update the status register.      */
    nop                             /* give another 4 nops              */
    nop                             /* give another 4 nops              */
    nop                             /* leave the parity off when        */
    nop                             /* booting the PROM.                */
    
    li      a0, 0x7                 /* Make sure its writeback cache.   */
    not     a0                      /* Inverted K0 mask.                */
    and     a0, a0, k0              /* clear previous K0 field.         */
    ori     a0, 0x3                 /* Make it cacheable writeback.     */
    mtc0    a0, Config              /* write it back.                   */
    li      a1, 0x1c0               /* Now check the Data cache size.   */
    and     a1, a1, k0              /* Mask off rest of the bits.       */
    srl     a1, 6                   /* Shift in place.                  */
    li      a0, 0x1000              /* Load the base cache size.        */
    sllv    v0, a0, a1              /* $v0 has the primary cache size   */
    sub     a0, v0, 0x20            /* The last line in the cache.      */
    mtc0    zero, TagHi             /* Clear the TagHi register.        */
    
1:  srl     k0, a0, 12              /* Create PTagLo                    */
    sll     k0, 8                   /* $k0 has the PTagLo of current    */
 #  ori     k0, 0x00                /* line, with invalidate cachestate */
    mtc0    k0, TagLo               /* load the TagLo register.         */
    nop                             /* at least 4 nops                  */
    lui     a1, 0x8000              /* Kseg0 address space.             */
    or      a2, a0, a1              /* Create kseg0 line address.       */
    cache   (PD|IST), 0x0(a2)       /* Create the dirty cache line.     */
    
#if defined(DebugCache)
    lui     t1, 0xa000              /* Write Tags to LA.                */
    sw      a2, 0x100(t1)           /* Push out to SysAD                */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    mtc0    zero, TagLo             /* clear Taglo register.            */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    cache   (PD|ILT), 0x0(a2)       /* Read the Tags back.              */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    mfc0    t0, TagLo               /* Read the TagLo register.         */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    nop                             /* at least 4 nops.                 */
    li      t2, 0x3                 /* ignore F and P bits.             */
    not     t2                      /* create the mask.                 */
    and     t2, t2, t0              /* TagLo without F and P            */
9:  sw      t0, 0x200(t1)           /* Push to SysAD                    */
    sw      t2, 0x300(t1)           /* Write SysAD                      */
    bne     t2, k0, 9b              /* Can not create tags              */
    sw      k0, 0x400(t1)           /* and expected taglo.              */
#endif
    
    sub     a0, 0x20                /* point to previous line.          */
    bgez    a0, 1b                  /* loop for the whole cache.        */
    mtc0    zero, TagHi             /* clear TagHi.                     */
    
#if defined(DdbugCache)    
    lui     t0, 0xa000              /* Write SysAD                      */
    sd      sp, 0x800(t0)           /* Push it out.                     */
#endif    

    
    jr      ra                      /* Return to caller.                */
    nop
    
.end cache_init

/*
 * -------------------------------------------------------------
#
#  $Log: cache_init.s,v $
#  Revision 1.1  1997/08/18 20:42:01  philw
#  updated file from bonsai/patch2039 tree
#
# Revision 1.3  1996/10/31  21:51:48  kuang
# Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
#
# Revision 1.3  1996/10/04  20:09:09  kuang
# Fixed some general problem in the diagmenu area
#
# Revision 1.2  1996/04/04  23:17:24  kuang
# Added more diagnostic support and some general cleanup
#
# Revision 1.1  1995/12/31  01:37:20  mwang
# cache funcs.
#
 * -------------------------------------------------------------
 */

/* END OF FILE */

