/*
 *    test name:      mem_addr3.s
 *    descriptions:   32 bytes Write/Read
 *                    to each bank of the following configuration -
 *                    8 128Mbyte bank, 64Mbit SDRAM.
 */


#include <asm_support.h>
#include <sys/regdef.h>
#include <mooseaddr.h>

/* Define addresses for registers */

#define crime_base  0x14000000
#define cntl        0x08        /* Crime cntl register                */
            
#define cntl_data   0xBEEF      /* cntl data                          */

#define PAGESIZE    (8*1024)
#define LINESIZE    0x20    
#define BANKSIZE    (128*1024*1024)
    
.globl mem_addr3
.ent mem_addr3, 0
mem_addr3:
    .set      noreorder
    subu    sp, framesize       /* create current stack frame.          */
    sd      AT,regsave-0x00(sp) /* save return address.                 */
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
    sd      s1,regsave-0x58(sp) /* save s1                              */
    sd      s2,regsave-0x60(sp) /* save s2                              */
    sd      k0,regsave-0x68(sp) /* save k0                              */
    sd      k1,regsave-0x70(sp) /* save k1                              */
/*
 *  [0] Doubleword write to initialize 5 line in 8 pages, for 4 banks
 *      of total 128M bytes.
 */
    move    k0, zero            /* Current bank address.            */
    li      t2, 0x08080808      /* Use increment pattern.           */
    dsll32  t4, t2, 0           /* Get it to upper 32 bits.         */
    or      t4, t4, t2          /* We have a 64 bit data.           */
    li      t2, 0x00010203      /* Load the upper 32 bits.          */
    dsll32  t5, t2, 0           /* get it in t5                     */
    li      t2, 0x04050607      /* And the lower 32 bits.           */
    or      t5, t5, t2          /* This is the data.                */
    lui     t3, (mem_base>>16)  /* Load the base address in t3.     */

    li      k1, 8               /* Do 8 banks.                      */
3:  li      a3, 0x8             /* Do 8 pages.                      */
1:  li      a1, (4*LINESIZE)    /* Test 5 lines in a page.          */

2:  addu    a2, t3, a1          /* Add in the line addr offset.     */
    sd      t5, 0(a2)           /* First double word.               */
    daddu   t6, t5, t4          /* Generate next double word.       */
    sd      t6, 8(a2)           /* Second double word.              */
    daddu   t5, t6, t4          /* Generate next double word.       */
    sd      t5, 16(a2)          /* Third double word.               */
    daddu   t6, t5, t4          /* Generate next double word.       */
    sd      t6, 24(a2)          /* Fourth double word.              */
    sub     a1, a1, LINESIZE    /* Initialized one line.            */
    bge     a1, 0,  2b          /* Loop until all 5 lines are done. */
    daddu   t5, t6, t4          /* Next word doubleword pattern.    */

    sub     a3, a3, 1           /* Are we donw with 8 pages ??      */
    li      a1, PAGESIZE        /* Load the page Offset.            */
    bne     a3, 0 1b            /* loop till done!                  */
    addu    t3, t3, a1          /* Add in the page offset - 8K byte */

    sub     k1, k1, 1           /* Are we done with all 4 banks ??  */
    lui     t3, (mem_base>>16)  /* Load the base address in t3.     */
    li      a1, BANKSIZE        /* Load the bank offset.            */
    addu    k0, k0, a1          /* point to next bank.              */
    bne     k1, 0, 3b           /* loop till all 4 banks are done.  */
    addu    t3, t3, k0          /* Move to next bank.               */

/*
 *  [1] Read all those data back and compare with the
 *      expected data pattern.
 */
    move    k0, zero            /* Reset current bank address.      */
    li      t2, 0x00010203      /* Increment data pattern.          */
    dsll32  t5, t2, 0           /* get it in t5                     */
    li      t2, 0x04050607      /* And the lower 32 bits.           */
    or      t5, t5, t2          /* This is the data.                */
    lui     t3, (mem_base>>16)  /* Load the base address in t3.     */

    li      k1, 8               /* Do 4 banks.                      */
3:  li      a3, 0x8             /* Do 8 pages.                      */
1:  li      a1, (4*LINESIZE)    /* Test 5 lines in a page.          */

2:  addu    a2, t3, a1          /* Add in the line addr offset.     */
    ld      s1, 0x0(a2)         /* 1st doubleword.                  */
    ld      s2, 0x8(a2)         /* 2nd doubleword.                  */
    beq     s1, t5, 8f          /* Is it correct ??                 */
    daddu   t6, t5, t4          /* Next data pattern.               */
    sd      s1, localargs-0x00(sp) /* save the observed data        */
    sd      t5, localargs-0x08(sp) /* save the expected data        */
    bal     err                 /* call the error handler.          */
    move    a0, a2              /* save the test  address.          */
8:  beq     s2, t6, 8f          /* Is it correct ??                 */
    daddu   t5, t6, t4          /* Next data pattern.               */
    sd      s2, localargs-0x00(sp) /* save the observed data        */
    sd      t6, localargs-0x08(sp) /* save the expected data        */
    bal     err                 /* call the error handler.          */
    addiu   a0, a2, 8           /* save the test address.           */
8:  ld      s1, 0x10(a2)        /* Third doubleword.                */
    ld      s2, 0x18(a2)        /* Fourth doubleword.               */
    beq     s1, t5, 8f          /* Is it correct ??                 */
    daddu   t6, t5, t4          /* Next data pattern.               */
    sd      s1, localargs-0x00(sp) /* save the observed data        */
    sd      t5, localargs-0x08(sp) /* save the expected data        */
    bal     err                 /* call the error handler.          */
    addiu   a0, a2, 0x10        /* save the test address.           */
8:  beq     s2, t6, 8f          /* Is it correct ??                 */
    sub     a1, a1, LINESIZE    /* Complete one line.               */
    sd      s2, localargs-0x00(sp) /* save the observed data        */
    sd      t6, localargs-0x08(sp) /* save the expected data        */
    bal     err                 /* call the error handler.          */
    addiu   a0, a2, 0x18        /* save the test address.           */
8:  bge     a1, 0, 2b           /* Loopback until for 5 lines.      */
    daddu   t5, t6, t4          /* Next data pattern.               */
    
    sub     a3, a3, 1           /* Are we donw with 8 pages ??      */
    li      a1, PAGESIZE        /* Load the page Offset.            */
    bne     a3, 0 1b            /* loop till done!                  */
    addu    t3, t3, a1          /* Add in the page offset - 8K byte */

    sub     k1, k1, 1           /* Are we done with all 4 banks ??  */
    lui     t3, (mem_base>>16)  /* Load the base address in t3.     */
    li      a1, BANKSIZE        /* Load the bank offset.            */
    addu    k0, k0, a1          /* point to next bank.              */
    bne     k1, 0, 3b           /* loop till all 4 banks are done.  */
    addu    t3, t3, k0          /* Move to next bank.               */

/*
 * [2]  We done!
 */
    b       return              /* return to dispatcher.                */
    li      v0, 1               /* indicate diags passed.               */

/*
 * [3]  Error handler.
 */
err:    
    subu    a3, ra, 24            /* the error pc                       */
    addiu   a2, sp,localargs-0x08 /* pass the expected data addr.       */
    jal     diag_error_msg        /* print error message and keep error */
    addu    a1, sp,localargs-0x00 /* pass the observed dara addr.       */
    move    v0, zero              /* indicate diags failed.             */
    
/*
 * [4] Restore all registers.
 */
return:
    ld      AT,regsave-0x00(sp) /* load return address.                 */
    ld      ra,regsave-0x08(sp) /* load return address.                 */
    ld      t2,regsave-0x10(sp) /* load t2                              */
    ld      t3,regsave-0x18(sp) /* load t3                              */
    ld      t4,regsave-0x20(sp) /* load t4                              */
    ld      t5,regsave-0x28(sp) /* load t5                              */
    ld      t6,regsave-0x30(sp) /* load t6                              */
    ld      a0,regsave-0x38(sp) /* load a0                              */
    ld      a1,regsave-0x40(sp) /* load a1                              */
    ld      a2,regsave-0x48(sp) /* load a2                              */
    ld      a3,regsave-0x50(sp) /* load a3                              */
    ld      s1,regsave-0x58(sp) /* load s1                              */
    ld      s2,regsave-0x60(sp) /* load s2                              */
    ld      k0,regsave-0x68(sp) /* load k0                              */
    ld      k1,regsave-0x70(sp) /* load k1                              */
    jr      ra                  /* return to dispatcher.                */
    addiu   sp, framesize       /* create current stack frame.          */
    
.end mem_addr3

/*
 * ===================================================
 * $Date: 1997/08/18 20:40:50 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
#
#  $Log: mem_addr3.s,v $
#  Revision 1.1  1997/08/18 20:40:50  philw
#  updated file from bonsai/patch2039 tree
#
# Revision 1.2  1996/10/31  21:50:51  kuang
# Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
#
# Revision 1.1  1996/04/04  23:37:37  kuang
# Ported from petty_crime design verification test suite
#
# Revision 1.7  1995/05/10  01:23:16  kuang
# *** empty log message ***
#
# Revision 1.6  1995/03/15  22:19:54  kuang
# fixed the bank looping problem.
#
# Revision 1.5  1995/03/15  19:19:17  kuang
# fixed line address offset in the read/verify loop.
#
# Revision 1.4  1995/02/03  02:23:11  kuang
# chaged data pattern increment size from 0x04040404 to 0x0808080808080808
#
# Revision 1.3  1995/01/28  01:19:23  kuang
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
