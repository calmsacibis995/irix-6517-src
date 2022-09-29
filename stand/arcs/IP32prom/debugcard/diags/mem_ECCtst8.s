/*
 *  test name:      mem_ECCtst8.s
 *  descriptions:   ecc random data pattern test.
 */
 	
#include <sys/regdef.h>
#include <crimereg.h>
#include <mooseaddr.h>
#include <cacheops.h>
#include <asm_support.h>

#define PAGESIZE        (8*1024)
#define BANKSIZE        (32*1024*1024)
#define TESTADDR        0x1DFFFE0
#define LINESIZE        0x20

.extern line_in_cache

.text
.set   noreorder
.globl mem_ECCtst8
.ent   mem_ECCtst8, 0
mem_ECCtst8:
    subu    sp, framesize       /* create current stack frame.          */
    sd      ra,regsave-0x08(sp) /* save return address.                 */
    sd      AT,regsave-0x10(sp) /* save AT                              */
    sd      t0,regsave-0x18(sp) /* save t0                              */
    sd      t1,regsave-0x20(sp) /* save t1                              */
    sd      t2,regsave-0x28(sp) /* save t2                              */
    sd      t3,regsave-0x30(sp) /* save t3                              */
    sd      t4,regsave-0x38(sp) /* save t4                              */
    sd      t5,regsave-0x40(sp) /* save t5                              */
    sd      a0,regsave-0x48(sp) /* save a0                              */
    sd      a1,regsave-0x50(sp) /* save a1                              */
    sd      a2,regsave-0x58(sp) /* save a2                              */
    sd      a3,regsave-0x60(sp) /* save a3                              */
    sd      s0,regsave-0x68(sp) /* save s0                              */
    sd      s1,regsave-0x70(sp) /* save s1                              */

/*
 *  [0] Make sure that all interrupt are cleared to begin with.
 */
    lui     t0, CrimeBASE  >> 16    /* load the crime register base addr */
    sd      zero, MEMoffset+MEMErrSTATUS(t0) /* clear mem error status. */
    sd      zero, CrimeIntrStatus(t0) /* clear the crime interrupt      */
    lui     t2, CRIMEerr_INT >> 16  /* Enable Crime error interrupt     */
    sd      t2, CrimeIntrEnable(t0) /* enable the crime interrupt.      */    
    
/*
 *  [1] Walking 1 through 64 bits double and verify the correctness of 
 *      the ecc check bit generate.
 *      register used:
 *          t1:  data address.
 *          t2:  64 bit data.
 *          t3:  Expected check bits.
 *          t4:  Observed check bits. (generated).
 *          t5:  testaddr.
 */
    la      a0, TESTADDR            /* load the test address.           */
    lui     a1, 0x80000000 >> 16    /* xfer kseg0 base address.         */
    or      a0, a0, a1              /* This is kseg0 test address.      */
    la      a1, rand_table          /* load the data table address.     */
    li      a2, 0x45495c3f          /* load the bis<63:32>              */
    dsll32  a2, a2, 0               /* move into place.                 */
    li      a3, 0x2928b8c9          /* and load bits<31:0>              */
    or      a2, a2, a3              /* we have a 64 bit data.           */
    
#if 0
1:  cache   (PD|HitI), 0x0(a0)      /* Invalid the cache line.          */
    li      a3, 24                  /* doubleword counter in a line.    */
    move    s0, zero                /* the generated ECC check bits.'   */
    cache   (PD|CDE), 0x0(a0)       /* create the line in cache.        */
    
2:  ld      t3, 0x8(a1)             /* read the 8 bit ecc check bits.   */
    ld      t2, 0x0(a1)             /* read the 64 bit data.            */
#else
1:  move    a3, a0                  /* save current kseg0 test addr.    */
    dsll32  a0, 3                   /* remove the upper 3 bits.         */
    dsrl32  a0, 3                   /* the physical address.            */
    jal	    line_in_cache           /* initialize a line in cache.      */
    move    s0, zero                /* the generated ECC check bits.'   */
    move    a0, a3                  /* restore current kseg0 test addr. */
    li	    a3, 24                  /* doubleword counter in a line.    */
    
2:  lwu     t2, 0x0(a1)             /* read the first half of the dword */
    lwu     t3, 0x4(a1)             /* and second half of the dword.    */
    dsll32  t2, 0                   /* move to bit <63:32>              */
    or      t2, t2, t3              /* for a complete 64 bit data       */
    lwu     t3, 0xc(a1)             /* read the 64 bit data             */
#endif

    sllv    t3, t3, a3              /* move the the right place.        */
    or      s0, s0, t3              /* place in the right byte position. */
    sd      t2, 0x0(a0)             /* write the first word.            */
    beq     t2, a2, 4f              /* are we done yet ??               */
    sub     a3, a3, 8               /* point to next double word.       */
    bltz    a3, 3f                  /* time to check the ecc check bits. */
    addu    a1, a1, 0x10            /* point to next pair of data.      */

    b       2b                      /* We have no complete a line yet.  */
    addu    a0, a0, 0x8             /* point to next double word.       */
    
3:  cache   (PD|IWI), 0x0(v0)       /* Flush the cache to memory.       */
    nop                             /* got to wait, ecc are slow.       */
    nop                             /* got to wait, ecc are slow.       */
    nop                             /* got to wait, ecc are slow.       */
    nop                             /* got to wait, ecc are slow.       */
    ld      t8, 0x0(a0)             /* read the word to get check bits  */		
    li      t5, 0xffffffff          /* setup the mask for the ecc       */
    dsll32  t5, t5, 0               /* registers.                       */
    not     t5                      /* now t5 has 0's on bits<63:32>    */
    ld      s1, EccCheckbits(t0)    /* read the ecc generated by crime. */
    and     s0, s0, t5              /* remove bits<63:32>               */
    beq     s0, s1, 2f              /* ecc check bits are not expected. */
    nop                             /* branch delay slot.               */
    bal     err                     /* branch to error handling routine */
    move    t5, s0                  /* save the expected data pattern.  */
2:  b       1b                      /* branch to loop top for next      */
    addiu   a0, 8                   /* point to next doubleword.        */
    
/*
 *  [2] Now verify the data we just wrote.
 */
4:  la      a0, TESTADDR            /* load the test address again      */
    lui     a1, 0x80000000 >> 16    /* with kseg0 base address.         */
    or      a0, a0, a1              /* using cache mode.                */
    la      a1, rand_table          /* and the data address.            */
    
#if 0
1:  ld      s1, 0x0(a0)             /* read the data from memory        */
    ld      t5, 0x0(a1)             /* and data from data table.        */
#else
1:  lwu     t5, 0x0(a1)             /* and data from data table.        */
    lwu	    s1, 0x4(a1)             /* the second half of the dword     */
    dsll32  t5, 0                   /* move to <63:32>                  */
    or	    t5, t5, s1              /* form a complete 64 bit data      */
    ld	    s1, 0x0(a0)             /* read the data from memory        */
#endif

    addu    a0, a0, 8               /* point to next double word.       */
    bne     s1, t5, err             /* not what we expected.            */
    addu    a1, a1, 0x10            /* point to next data.              */
    bne     t2, a2, 1b              /* is this the last data ??         */
    nop                             /* branch delay solt.               */

/*
 * [3]  We done!
 */
    b       return              /* return to dispatcher.            */
    li      v0, 1               /* indicate test passed.            */

/*
 *  [4] We run into errors 
 */
err:    
    sd	    s1,localargs-0x08(sp) /* save the observed data pattern.    */
    sd	    t5,localargs-0x10(sp) /* and expected data pattern.         */
    subu    a3, ra, 16            /* the error pc,                      */
    addu    a2, sp,localargs-0x10 /* pass the expected data addr.       */
    addu    a1, sp,localargs-0x08 /* pass the error data address.       */
    jal     diag_error_msg        /* print error message and keep error */
    move    a0, s0                /* the test address.                  */
    move    v0, zero              /* indicate diags failed.             */
    
/*
 *  [5] We done!
 */
return:
    ld      ra,regsave-0x08(sp) /* load return address.                 */
    ld      AT,regsave-0x10(sp) /* load AT                              */
    ld      t0,regsave-0x18(sp) /* load t0                              */
    ld      t1,regsave-0x20(sp) /* load t1                              */
    ld      t2,regsave-0x28(sp) /* load t2                              */
    ld      t3,regsave-0x30(sp) /* load t3                              */
    ld      t4,regsave-0x38(sp) /* load t4                              */
    ld      t5,regsave-0x40(sp) /* load t5                              */
    ld      a0,regsave-0x48(sp) /* load a0                              */
    ld      a1,regsave-0x50(sp) /* load a1                              */
    ld      a2,regsave-0x58(sp) /* load a2                              */
    ld      a3,regsave-0x60(sp) /* load a3                              */
    ld      s0,regsave-0x68(sp) /* load s0                              */
    ld      s1,regsave-0x70(sp) /* load s1                              */
    jr      ra                  /* Return.                              */
    addiu   sp, framesize       /* create current stack frame.          */
.end mem_ECCtst8

#include <eccrantab.s>



/*
 * ===================================================
 * $Date: 1997/08/18 20:40:40 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
#
# $Log: mem_ECCtst8.s,v $
# Revision 1.1  1997/08/18 20:40:40  philw
# updated file from bonsai/patch2039 tree
#
# Revision 1.3  1996/10/31  21:50:41  kuang
# Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
#
# Revision 1.3  1996/10/04  20:33:11  kuang
# Ported to run with crime ECC
#
# Revision 1.2  1996/04/12  02:15:33  kuang
# Now ECC tests run with R4600/DebugCard environment
#
# Revision 1.1  1996/04/04  23:50:07  kuang
# Ported from petty_crime design verification test suite
#
# Revision 1.1  1995/06/14  21:36:23  kuang
# Initial revision
#
# Revision 1.2  1995/06/09  22:17:41  kuang
# The first working version
#
 */

/* END OF FILE */
