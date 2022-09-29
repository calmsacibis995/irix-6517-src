/*
 *  test name:      mem_BEtst.s
 *  descriptions:   Big Endian 1 byte, 2 bytes, 4 bytes, and 8 bytes
 *                  write/read test.
 */

#define  BIGSTACKFRAME
#include <asm_support.h>
#include <sys/regdef.h>
#include <mooseaddr.h>

/* Define addresses for registers */

#define crime_base  0x14000000
#define cntl        0x08        /* Crime cntl register                */
#define cntl_data   0xBEEF      /* cntl data                          */

#if 0
#define ForceError5 
#define ForceError4
#define ForceError3
#define ForceError2
#define ForceError1
#endif
    
.globl mem_BEtst
.ent mem_BEtst, 0
mem_BEtst:
    .set    noreorder
    subu    sp, framesize           /* create current stack frame.      */
    sd      AT,regsave-0x00(sp)     /* save AT register.                */
    sd      ra,regsave-0x08(sp)     /* save ra register.                */
    sd      t1,regsave-0x10(sp)     /* save t1 register.                */
    sd      t2,regsave-0x18(sp)     /* save t2 register.                */
    sd      t3,regsave-0x20(sp)     /* save t3 register.                */
    sd      t4,regsave-0x28(sp)     /* save t4 register.                */
    sd      t5,regsave-0x30(sp)     /* save t5 register.                */
    sd      s1,regsave-0x38(sp)     /* save s1 register.                */
    sd      s2,regsave-0x40(sp)     /* save s2 register.                */
    sd      s3,regsave-0x48(sp)     /* save s3 register.                */
    sd      s4,regsave-0x50(sp)     /* save s4 register.                */
    sd      s5,regsave-0x58(sp)     /* save s5 register.                */
    sd      s6,regsave-0x60(sp)     /* save s6 register.                */
    sd      s7,regsave-0x68(sp)     /* save s7 register.                */
    sd      a0,regsave-0x70(sp)     /* save a0 register.                */
    sd      a1,regsave-0x78(sp)     /* save a1 register.                */
    sd      a2,regsave-0x80(sp)     /* save a2 register.                */
    sd      a3,regsave-0x88(sp)     /* save a3 register.                */
/*
 *    [0] Doubleword write to initialize a line in memory <8 bytes write>.
 */
    li      t2, 0x08080808      /* Use increment pattern.               */
    dsll32  t4, t2, 0           /* Get it to upper 32 bits.             */
    or      t4, t4, t2          /* We have a 64 bit data.               */
    li      t2, 0x00010203      /* Load the upper 32 bits.              */
    dsll32  t5, t2, 0           /* get it in t5                         */
    li      t2, 0x04050607      /* And the lower 32 bits.               */
    or      t5, t5, t2          /* This is the data.                    */
    lui     t3, (mem_base>>16)  /* At location 0xC000,0000.             */
    
    sd      t5, 0(t3)            /* First double word.                  */
    daddu   t5, t5, t4           /* Generate next double word.          */
    sd      t5, 8(t3)            /* Second double word.                 */
    daddu   t5, t5, t4           /* Generate next double word.          */
    sd      t5, 16(t3)           /* Third double word.                  */
    daddu   t5, t5, t4           /* Generate next double word.          */
    sd      t5, 24(t3)           /* Fourth double word.                 */

/*
 *  [1] Byte read, Halfword read, Word read test.
 *      33222222 22221111  11111100 00000000
 *      10987654 32109876  54321098 76543210
 */
    lui     t3, (mem_base>>16)    /* Start at location 0xC000,0000      */
    li      t1, 0x00010203        /* First word expected value.         */
    li      t2, 0x04040404        /* The incremental value between words*/
    li      t4, 8                 /* There are 32 bytes in a line.      */
1:  lw      s1, 0x0(t3)           /* Read the word.                     */
    lbu     s2, 3(t3)             /* Last byte in the word.             */
    lbu     s3, 2(t3)             /* The 3rd byte in the word.          */
    lbu     s4, 1(t3)             /* The 2nd byte in the word.          */
    lbu     s5, 0(t3)             /* The 1st byte in the word.          */
    lhu     s6, 2(t3)             /* The Last halfword in the word.     */
    lhu     s7, 0(t3)             /* The 1st halfword in the word.      */
#if !defined(ForceError1)   
    beq     s1, t1, 2f            /* Check the word read in.            */
    sll     s5, s5, 24            /* move the first byte                */
#endif    
    sd      t1,localargs-0x00(sp) /* save the expected data.            */
    sd      s1,localargs-0x08(sp) /* save the observed data.            */
    bal     err                   /* branch to error routine.           */
    move    t1, t3                /* save the test address.             */
2:  sll     s4, s4, 16            /* move the second byte.              */
    sll     s3, s3, 8             /* move the third byte.               */
    or      a0, s5, zero          /* Or them together and the result    */
    or      a0, s4, a0            /* should equal to the word read in.  */
    or      a0, s3, a0
    or      a0, s2, a0
#if !defined(ForceError2)
    beq     a0, s1, 2f            /* Check the result of the byte read. */
    sll     s7, s7, 16            /* Move the last halfword to upper 16 */
#endif
    sd      a0,localargs-0x00(sp) /* save the expected data.            */
    sd      s1,localargs-0x08(sp) /* save the observed data.            */
    bal     err                   /* branch to error routine.           */
    move    t1, t3                /* save the test address.             */
2:  or      a1, s6, s7            /* form the complete 32 bits word.    */
#if !defined(ForceError3)
    beq     a1, s1, 2f            /* Check the halfword read.           */
    sub     t4, t4, 1             /* Are we done yet ??                 */
#endif
    sd      a1,localargs-0x00(sp) /* save the expected data.            */
    sd      s1,localargs-0x08(sp) /* save the observed data.            */
    bal     err                   /* branch to error routine.           */
    move    t1, t3                /* save the test address.             */
2:  addiu   t3, t3, 4             /* Point top next word.               */
    bgtz    t4, 1b                /* Do next word.                      */
    addu    t1, t1, t2            /* The expected value.                */

/*
 *  [2]    Byte write, Halfword write, word write
 */
    lui     t3, (mem_base>>16)    /* Start at location 0.               */
    li      t4, 0xfffefdfc        /* The decrement data pattern.        */
    li      t5, 8                 /* 32 bytes line size.                */
1:  sw      t4, 0x0(t3)           /* Word write.                        */
    subu    t4, t4, t2            /* Generate next data pattern.        */
    srl     s1, t4, 24            /* the first byte of the word.        */
    sb      s1, 0x4(t3)           /* byte write.                        */
    srl     s1, t4, 16            /* move second byte into position.    */
    and     s1, s1, 0xff          /* mask off the high byte.            */
    sb      s1, 0x5(t3)           /* another byte write.                */
    and     s1, t4, 0xffff        /* generate the first halfword.       */
    sh      s1, 0x6(t3)           /* halfword write.                    */
    addu    t3, t3, 8             /* point to next doubleword.          */
    sub     t5, t5, 2             /* already wrote 2 words out.         */
    bne     t5, 0, 1b             /* do it again until zero.            */
    subu    t4, t4, t2            /* next data pattern.                 */
    
/*
 * [3] Check the result.
 */
    li      t4, 0x00010203        /* Load the expected data pattern,    */
    dsll32  t4, t4, 0             /* shift into high position.          */
    li      t5, 0x04050607        /* the lower word data pattern.       */
    or      t4, t4, t5            /* Generate a 64 bit data.            */
    not     t4                    /* get 0xfffefdfcfbfaf9f8             */
    li      t2, 0x08080808        /* The difference between word.       */
    dsll32  t5, t2, 0             /* Shift into high position.          */
    or      t2, t2, t5            /* form 64 bit data.                  */
    
    li      t5, 4                 /* Four double words in a line.       */
    lui     t3, (mem_base>>16)    /* Start at location 0.               */
1:  ld      s1, 0x0(t3)           /* Double word read.                  */
    ld      s2, 0x8(t3)           /* another double word read.          */
#if !defined(ForceError4)    
    beq     t4, s1, 2f            /* Check first read.                  */
    nop                           /* branch delay slot.                 */
#endif
    sd      t4,localargs-0x00(sp) /* save the expected data.            */
    sd      s1,localargs-0x08(sp) /* save the observed data.            */
    bal     err                   /* branch to error routine.           */
    move    t1, t3                /* save the test address.             */
2:  dsubu   t4, t4, t2            /* Form next double word pattern.     */
#if !defined(ForceError5)
    beq     t4, s2, 2f            /* check 2nd double word read.        */
    sub     t5, t5, 2             /* already done 2 double read.        */
#endif
    sd      t4,localargs-0x00(sp) /* save the expected data.            */
    sd      s2,localargs-0x08(sp) /* save the observed data.            */
    bal     err                   /* branch to error routine.           */
    addiu   t1, t3, 8             /* save the test address.             */
2:  addu    t3, t3, 0x10          /* point to next quardfword.          */
    bnez    t5, 1b                /* Do it for whole line.              */
    dsubu   t4, t4, t2            /* Next expected data pattern.        */

/*
 * [4]    We done!
 */
    b       return                  /* jump to return .                 */
    li      v0, 1                   /* indicate diag passed.            */
    
/*
 * [5]  Handling error cases.
 *      t1: test address.
 */
err:
    subu    a3, ra, 0x18            /* error pc, the branch instruction */
    addiu   a2, sp, localargs       /* the expected data address.       */
    addiu   a1, sp, localargs-0x08  /* the observed data address.       */
    jal     diag_error_msg          /* call diag_error_msg routine.     */
    move    a0, t1                  /* the test address.                */
    move    v0, zero                /* indicate diag failed.            */
    
/*
 *  Return to dispatcher.
 */
return:
    ld      AT,regsave-0x00(sp)     /* save AT register.    1           */
    ld      ra,regsave-0x08(sp)     /* save ra register.    2           */
    ld      t1,regsave-0x10(sp)     /* save t1 register.    3           */
    ld      t2,regsave-0x18(sp)     /* save t2 register.    4           */
    ld      t3,regsave-0x20(sp)     /* save t3 register.    5           */
    ld      t4,regsave-0x28(sp)     /* save t4 register.    6           */
    ld      t5,regsave-0x30(sp)     /* save t5 register.    7           */
    ld      s1,regsave-0x38(sp)     /* save s1 register.    8           */
    ld      s2,regsave-0x40(sp)     /* save s2 register.    9           */
    ld      s3,regsave-0x48(sp)     /* save s3 register.    10          */
    ld      s4,regsave-0x50(sp)     /* save s4 register.    11          */
    ld      s5,regsave-0x58(sp)     /* save s5 register.    12          */
    ld      s6,regsave-0x60(sp)     /* save s6 register.    13          */
    ld      s7,regsave-0x68(sp)     /* save s7 register.    14          */
    ld      a0,regsave-0x70(sp)     /* save a0 register.    15          */
    ld      a1,regsave-0x78(sp)     /* save a1 register.    16          */
    ld      a2,regsave-0x80(sp)     /* save a2 register.    17          */
    ld      a3,regsave-0x88(sp)     /* save a3 register.    18          */
    jr      ra                      /* return.                          */
    addu    sp, framesize           /* create current stack frame.      */
    
.end mem_BEtst
