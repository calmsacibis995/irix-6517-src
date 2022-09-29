/*
 *  
 *    test name:      mem_addr1.s
 *    descriptions:   32 bytes Write/Read
 *                    to each bank of the following configuration -
 *                    1 32Mbyte bank, 16Mbit SDRAM.
 */

#include <sys/regdef.h>
#include <mooseaddr.h>
#include <asm_support.h>

/* Define addresses for registers */

#define crime_base  0x14000000
#define cntl        0x08        /* Crime cntl register                */
            
#define cntl_data   0xBEEF      /* cntl data                          */

#define PAGESIZE    8192
#define LINESIZE    0x20

#if 0
#define ForceError
#endif
    
.set        noreorder
.globl      mem_addr1
.ent        mem_addr1, 0
mem_addr1:
    subu    sp, framesize       /* create current stack frame.          */
    sd      ra,regsave-0x08(sp) /* save return address.                 */
    sd      t2,regsave-0x10(sp) /* save t2                              */
    sd      t3,regsave-0x18(sp) /* save t3                              */
    sd      t4,regsave-0x20(sp) /* save t4                              */
    sd      t5,regsave-0x28(sp) /* save t5                              */
    sd      t6,regsave-0x30(sp) /* save t6                              */
    sd      a0,regsave-0x38(sp) /* save a0                              */
    sd      a1,regsave-0x40(sp) /* save a1                              */
    sd      a2,regsave-0x48(sp) /* save a2                              */
    sd      a3,regsave-0x50(sp) /* save a3                              */
    sd      s0,regsave-0x58(sp) /* save s1                              */
    sd      s1,regsave-0x60(sp) /* save s1                              */
    sd      s2,regsave-0x68(sp) /* save s2                              */
    sd      s3,regsave-0x70(sp) /* save s2                              */
    sd      AT,regsave-0x78(sp) /* save AT                              */
/*
 *  [0] Doubleword write to initialize 5 line in 8 pages.                
 */
    li      t2, 0x08080808      /* Use increment pattern.           */
    dsll32  t4, t2, 0           /* Get it to upper 32 bits.         */
    or      t4, t4, t2          /* We have a 64 bit data.           */
    li      t2, 0x00010203      /* Load the upper 32 bits.          */
    dsll32  t5, t2, 0           /* get it in t5                     */
    li      t2, 0x04050607      /* And the lower 32 bits.           */
    or      t5, t5, t2          /* This is the data.                */
    lui     t3, (mem_base>>16)  /* Load the base address in t3.     */

    li      a3, 0x8             /* Do 8 pages.                      */
1:  li      a1, (4*LINESIZE)    /* Test 5 lines in a page.          */

2:  addu    a2, t3, a1          /* Add in the line addr offset.     */
    sd      t5, 0(a2)           /* First double word.               */
    daddu   t5, t5, t4          /* Generate next double word.       */
    sd      t5, 8(a2)           /* Second double word.              */
    daddu   t5, t5, t4          /* Generate next double word.       */
    sd      t5, 16(a2)          /* Third double word.               */
    daddu   t5, t5, t4          /* Generate next double word.       */
    sd      t5, 24(a2)          /* Fourth double word.              */
    sub     a1, a1, LINESIZE    /* Initialized one line.            */
    bge     a1, 0,  2b          /* Loop until all 5 lines are done. */
    daddu   t5, t5, t4          /* Next word doubleword pattern.    */

    sub     a3, a3, 1           /* Are we donw with 8 pages ??      */
    li      a1, PAGESIZE        /* Load the page Offset.            */
    bne     a3, 0 1b            /* loop till done!                  */
    addu    t3, t3, a1          /* Add in the page offset - 8K byte */

/*
 *  [1] Read all those data back and compare with the
 *      expected data pattern.
 */
    li      t2, 0x00010203      /* Increment data pattern.          */
    dsll32  t5, t2, 0           /* get it in t5                     */
    li      t2, 0x04050607      /* And the lower 32 bits.           */
    or      t5, t5, t2          /* This is the data.                */
    lui     t3, (mem_base>>16)  /* Load the base address in t3.     */

    li      a3, 0x8             /* Do 8 pages.                      */
1:  li      a1, (4*LINESIZE)    /* Test 5 lines in a page.          */
2:  addu    a2, t3, a1          /* Add in the line addr offset.     */
    ld      s1, 0x0(a2)         /* 1st doubleword.                  */
    ld      s2, 0x8(a2)         /* 2nd doubleword.                  */
    move    s0, a2              /* test address.                    */
#if !defined(ForceError)
    beq     s1, t5, 3f          /* Is it correct ??                 */
    daddu   t6, t5, t4          /* Next data pattern.               */
#endif
    bal	    err
    nop
    
3:  move    s1, s2              /* move s1.                         */
    move    t5, t6              /* move to t5                       */
    addu    s0, a2, 0x8         /* test address.                    */
    beq     s1, t5, 3f          /* Is it correct ??                 */
    daddu   t5, t6, t4          /* Next data pattern.               */
    bal	    err
    nop
    
3:  ld      s1, 0x10(a2)        /* Third doubleword.                */
    ld      s2, 0x18(a2)        /* Fourth doubleword.               */
    addu    s0, a2, 0x10        /* test address.                    */
    beq     s1, t5, 3f          /* Is it correct ??                 */
    daddu   t6, t5, t4          /* Next data pattern.               */
    bal	    err
    nop
    
3:  move    s1, s2              /* move s1.                         */
    move    t5, t6              /* move to t5                       */
    addu    s0, a2, 0x18        /* test address.                    */
    beq     s1, t5, 3f          /* Is it correct ??                 */
    sub     a1, a1, LINESIZE    /* Complete one line.               */
    bal	    err
    nop
    
3:  bge     a1, 0,  2b          /* Loopback until for 5 lines.      */
    daddu   t5, t6, t4          /* Next data pattern.               */
    
    sub     a3, a3, 1           /* Are we donw with 8 pages ??      */
    li      a1, PAGESIZE        /* Load the page Offset.            */
    bne     a3, 0,  1b          /* loop till done!                  */
    addu    t3, t3, a1          /* Add in the page offset - 8K byte */

/*
 * [2]  We done!
 */
    b       return              /* return to dispatcher.            */
    li      v0, 1               /* indicate test passed.            */

/*
 *  [3] We ran into errors 
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
 *  [4] Restore all registers.
 */
return:
    ld      ra,regsave-0x08(sp) /* save return address.                 */
    ld      t2,regsave-0x10(sp) /* save t2                              */
    ld      t3,regsave-0x18(sp) /* save t3                              */
    ld      t4,regsave-0x20(sp) /* save t4                              */
    ld      t5,regsave-0x28(sp) /* save t5                              */
    ld      t6,regsave-0x30(sp) /* save t6                              */
    ld      a0,regsave-0x38(sp) /* save a0                              */
    ld      a1,regsave-0x40(sp) /* save a1                              */
    ld      a2,regsave-0x48(sp) /* save a2                              */
    ld      a3,regsave-0x50(sp) /* save a3                              */
    ld      s0,regsave-0x58(sp) /* save s1                              */
    ld      s1,regsave-0x60(sp) /* save s1                              */
    ld      s2,regsave-0x68(sp) /* save s2                              */
    ld      s3,regsave-0x70(sp) /* save s2                              */
    ld      AT,regsave-0x78(sp) /* save AT                              */
    jr      ra                  /* return                               */
    addu    sp, framesize       /* delete current stack frame.          */

.end mem_addr1


/*
 * ===================================================
 * $Date: 1997/08/18 20:40:46 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
#
#  $Log: mem_addr1.s,v $
#  Revision 1.1  1997/08/18 20:40:46  philw
#  updated file from bonsai/patch2039 tree
#
# Revision 1.2  1996/10/31  21:50:48  kuang
# Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
#
# Revision 1.1  1996/04/04  23:37:35  kuang
# Ported from petty_crime design verification test suite
#
# Revision 1.8  1995/05/10  01:23:13  kuang
# *** empty log message ***
#
# Revision 1.7  1995/03/09  01:23:58  kuang
# fixed the loop problem in the read/verify portion where
# the memory line index did not get incremented properly.
#
# Revision 1.6  1995/03/09  00:48:20  kuang
# fixed the branch delay slot problem, actually
# this is more of r4k model's bug I just find a
# way to walk around the bug.
#
# Revision 1.5  1995/02/03  02:22:21  kuang
# chaged data pattern increment size from 0x04040404 to 0x0808080808080808
#
# Revision 1.4  1995/01/28  01:20:54  kuang
# fixed the local lable 2:
#
# Revision 1.3  1995/01/28  01:19:00  kuang
# fixed the first loop address problem.
#
# Revision 1.2  1995/01/25  00:44:03  kuang
# *** empty log message ***
#
# Revision 1.1  1995/01/25  00:40:47  kuang
# Initial revision
#
 */

/* END OF FILE */
