/*
 *  test name:      mem_addr5.s
 *  descriptions:   32 bytes Write/Read
 *                  to each bank of the following configuration -
 *                  8 32Mbyte bank, 16Mbit SDRAM, block read/write.
 */

#include <asm_support.h>
#include <sys/regdef.h>
#include <mooseaddr.h>

/* Define addresses for registers */

#define crime_base  0x14000000
#define cntl        0x08        /* Crime cntl register                */
#define cntl_data   0xBEEF      /* cntl data                          */
#define PAGESIZE    8192
#define BANKSIZE    (32*1024*1024)
    
.globl mem_addr5
.ent   mem_addr5, 0
mem_addr5:
    .set      noreorder
    subu    sp, framesize       /* create current stack frame.          */
    sd      AT,regsave-0x00(sp) /* save AT.                             */
    sd      ra,regsave-0x08(sp) /* save AT.                             */
    sd      t0,regsave-0x10(sp) /* save t0.                             */
    sd      t1,regsave-0x18(sp) /* save t1.                             */
    sd      t2,regsave-0x20(sp) /* save t2.                             */
    sd      t3,regsave-0x28(sp) /* save t3.                             */
    sd      t4,regsave-0x30(sp) /* save t4.                             */
    sd      a0,regsave-0x38(sp) /* save a0.                             */
    sd      a1,regsave-0x40(sp) /* save a1.                             */
    sd      a2,regsave-0x48(sp) /* save a2.                             */
    sd      a3,regsave-0x50(sp) /* save a3.                             */
    
/*
 *  [0] block write to initialize 8 lines in 8 pages.                
 */
    lwu     a0, PCACHESET(gp)   /* load the cache set bit.          */
    li      t0, BANKSIZE        /* Load the page offset.            */
    li      t1, PAGESIZE        /* Load the page offset.            */
    li      t2, 0x8             /* Walk through 8 banks.            */
    lui     t3, (mem_base>>16)  /* Load the base address in t3.     */
    or      t3, t3, a0          /* or in the cache offset.          */
    
    li      a0, 8               /* 32 bytes, 1 line.                */
    li      a2, 0               /* Do not do insert parity error.   */
1:  li      t4, 0x8             /* Do 8 pages.                      */
    move    a1, t3              /* Block starting address.          */
2:  break   3                   /* Call for block write.            */
    
    sub     t4, t4, 1           /* Are we donw with 8 pages ??      */
    bne     t4, 0,  2b          /* loop till done!                  */
    addu    a1, a1, t1          /* Add in the page offset - 8K byte */

    sub     t2, t2, 1           /* Are we done with 8 banks ??      */
    bne     t2, 0,  1b          /* loop until done.                 */
    addu    t3, t3, t0          /* Move to next bank.               */

/*
 *  [1] Read all those data back and compare with the
 *      expected data pattern.
 */
    lwu     a0, PCACHESET(gp)   /* load the cache offset bit.       */
    li      t2, 0x8             /* Walk through 8 banks.            */
    lui     t3, (mem_base>>16)  /* Load the base address in t3.     */
    or      t3, t3, a0          /* or in the cache set offset.      */
    
    li      a0, 8               /* 32 bytes 1 line.                 */
    li      a2, 0               /* Check the data just read.        */
1:  li      t4, 0x8             /* Do 8 pages.                      */
    move    a1, t3              /* Block starting address.          */
2:  break   2                   /* Call for block read.             */
    
    sub     t4, t4, 1           /* Are we donw with 8 pages ??      */
    bne     t4, 0,  2b          /* loop till done!                  */
    addu    a1, a1, t1          /* Add in the page offset - 8K byte */

    sub     t2, t2, 1           /* Are we done with 8 banks ??      */
    bne     t2, 0,  1b          /* loop until done.                 */
    addu    t3, t3, t0          /* Move to next bank.               */
    
/*
 * [2]  We done!
 */
    b       return              /* return to dispatcher.                */
    li      v0, 1               /* diag passed indicator.               */

/*
 * [3] Handling error case.
 */
err:    
    move    v0, zero            /* diag failed indicator.               */
    
/*
 * [4] Return.
 */
return:
    ld      AT,regsave-0x00(sp) /* load AT.                             */
    ld      ra,regsave-0x08(sp) /* load AT.                             */
    ld      t0,regsave-0x10(sp) /* load t0.                             */
    ld      t1,regsave-0x18(sp) /* load t1.                             */
    ld      t2,regsave-0x20(sp) /* load t2.                             */
    ld      t3,regsave-0x28(sp) /* load t3.                             */
    ld      t4,regsave-0x30(sp) /* load t4.                             */
    ld      a0,regsave-0x38(sp) /* load a0.                             */
    ld      a1,regsave-0x40(sp) /* load a1.                             */
    ld      a2,regsave-0x48(sp) /* load a2.                             */
    ld      a3,regsave-0x50(sp) /* load a3.                             */
    jr      ra                  /* return                               */
    addiu   sp, framesize       /* create current stack frame.          */
    
.end mem_addr5


/*
 * ===================================================
 * $Date: 1997/08/18 20:40:54 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
#
#  $Log: mem_addr5.s,v $
#  Revision 1.1  1997/08/18 20:40:54  philw
#  updated file from bonsai/patch2039 tree
#
# Revision 1.3  1996/10/31  21:50:54  kuang
# Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
#
# Revision 1.2  1996/04/13  00:54:54  kuang
# minor changes to the diag structure
#
# Revision 1.1  1996/04/04  23:37:39  kuang
# Ported from petty_crime design verification test suite
#
# Revision 1.3  1995/05/10  01:23:18  kuang
# *** empty log message ***
#
# Revision 1.2  1995/02/10  03:38:12  kuang
# Fixed the problem that Crime can not accept a block
# write/read size greater then 8.
#
# Revision 1.1  1995/01/25  00:46:47  kuang
# Initial revision
#
 */

/* END OF FILE */
