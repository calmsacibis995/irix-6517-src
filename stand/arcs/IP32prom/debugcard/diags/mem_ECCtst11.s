/*
 *  test name:      mem_ECCtst1.s
 *  descriptions:   ecc Walking 0 pattern test.
 */
 	
#include <sys/regdef.h>
#include <crimereg.h>
#include <mooseaddr.h>
#include <cp0regdef.h>
#include <cacheops.h>

#define PAGESIZE        (8*1024)
#define BANKSIZE        (32*1024*1024)
#define TESTADDR        0x1000
#define LINESIZE        0x20

.text
.globl mem_ECCtst1
.ent   mem_ECCtst1, 0
mem_ECCtst1:

    .set      noreorder
/*
 *  [0] Make sure that all interrupt are cleared to begin with.
 */
    lui     t0, CrimeBASE  >> 16    /* load the crime register base addr */
    lui     t2, (CRIMEerr_INT|MEMerr_INT) >> 16  /* enable mem interrupt */
    sd      t2, CrimeIntrEnable(t0) /* enable the crime interrupt.      */    
    li      t1, CPUallIllegal       /* enable all error checking.       */
    sd      t1, CPUErrENABLE(t0)    /* write to the register in crime.  */
    ld      t1, CrimeIntrStatus(t0) /* read the interrupt status reg.   */
    nop                             /* read delay slot.                 */
    bnez    t1, err                 /* should not have anything set.    */
    nop                             /* branch delay slot.               */
    
/*
 *  [1] Setup some bookkeeping stuff, - interrupt counter, loop count, 
 *      and ...
 */
    li      gp, 0xdeadbeef          /* setup interrupt counter.         */
    not     gp                      /* invert the counter.              */
    la      fp, ecc_interrupt       /* setup ecc server routine pointer */
    li      s0, TESTADDR            /* setup test address, align to line. */
    li      s1, 0x18                /* the doubleword at fault.         */
    li      s2, 0x80                /* the bit at fault.                */
    
/*
 *  [2] Call error_ecc to create a ecc error at test address.
 */
9:  li      t9, 0x11                /* indicate it check bit error.     */
    addu    a0, s0, s1              /* the error address (without membase)*/
    move    a1, zero                /* do check bit first.              */
    move    a2, s2                  /* the bit at fault.                */
    bal     error_ecc               /* generate a bad ecc at testaddr.  */
    move    a3, zero                /* initialize the line.             */
    
/*
 *  [3.0] ECC error read test, do following test for a line.
 *        - double word read, 
 */
    li      t6, 0x1                 /* this is the trap counter.        */
    la      t1, (TESTADDR|mem_base) /* loadup test addr.                */
    move    t5, s0                  /* data pattern                     */
1:  not     a1, t5                  /* create upper bits.               */
    dsll32  a1, 0                   /* move into place.                 */
    or      a1, a1, t5              /* we have the expected pattern.    */
    ld      a0, 0x0(t1)             /* read double from the line.       */
    addu    t5, 8                   /* next data pattern.               */
    bal     wait_loop               /* single bit error are async.      */
    nop                             /* delay slot.                      */
    not     gp                      /* we should see ONE trap+interrupt. */
    bne     gp, t6, err             /* is it so ??                      */
    addi    t6, 1                   /* increment the interrupt count.   */
    addu    t1, 8                   /* point to next double word.       */
    andi    t3, t1, 0x18            /* check the double offset.         */
    bnez    t3, 1b                  /* loopback for whole line.         */
    not     gp                      /* and invert the global counter.   */
    
/*
 *  [3.1] ECC error read test, do following test for a line.
 *        - word read, 
 */
    la      t1, (TESTADDR|mem_base) /* loadup test addr.                */    
    move    t5, s0                  /* and the data pattern.            */
1:  not     a1, t5                  /* create upper bits.               */
    dsll32  a1, 0                   /* move into place.                 */
    dsrl32  a1, 0                   /* remove all the ff's              */
    lwu     a0, 0x0(t1)             /* read the word from the line.     */
    bal     wait_loop               /* go wait for the interrupt.       */
    nop                             /* delay slot.                      */
    not     gp                      /* we should see ONE trap+interrupt. */
    bne     gp, t6, err             /* is it so ??                      */
    addi    t6, 1                   /* increment the interrupt count.   */
    not     gp                      /* invert the global counter.       */
    move    a1, t5                  /* the expected lower 32 bits.      */
    lwu     a0, 0x4(t1)             /* read the next word.              */
    bal     wait_loop               /* go wait for the interrupt.       */
    nop                             /* delay slot.                      */
    not     gp                      /* we should see ONE trap+interrupt. */
    bne     gp, t6, err             /* is it so ??                      */
    addi    t6, 1                   /* increment the interrupt count.   */
    addu    t5, 8                   /* next data pattern.               */
    addu    t1, 8                   /* point to next double word.       */
    andi    t3, t1, 0x18            /* check the double offset.         */
    bnez    t3, 1b                  /* loopback for whole line.         */
    not     gp                      /* and invert the global counter.   */
    
/*
 *  [3.2] ECC error read test, do following test for a line.
 *        - half word read, 
 */
    la      t1, (TESTADDR|mem_base) /* loadup test addr.                */    
    move    t5, s0                  /* and the data pattern.            */
1:  not     t4, t5                  /* create upper bits.               */
    dsll32  t3, t4, 0               /* create lower order 16 bits       */
    dsrl32  a1, t3, 16              /* remove all the ff's              */
    lhu     a0, 0x0(t1)             /* read the word from the line.     */
    bal     wait_loop               /* go wait for the interrupt.       */
    nop                             /* delay slot.                      */
    not     gp                      /* we should see ONE trap+interrupt. */
    bne     gp, t6, err             /* is it so ??                      */
    addi    t6, 1                   /* increment the interrupt count.   */
    not     gp                      /* invert the global counter.       */
    dsll32  t3, t4, 16              /* move to high bit.                */
    dsrl32  a1, t3, 16              /* move to lower 16 bits.           */
    lhu     a0, 0x2(t1)             /* read the word from the line.     */
    bal     wait_loop               /* go wait for the interrupt.       */
    nop                             /* delay slot.                      */
    not     gp                      /* we should see ONE trap+interrupt. */
    bne     gp, t6, err             /* is it so ??                      */
    addi    t6, 1                   /* increment the interrupt count.   */
    not     gp                      /* invert the global counter.       */
    dsll32  t4, t5, 0               /* move to higher 32 bits.          */
    dsrl32  a1, t4, 16              /* move to lower 16 bits.           */
    lhu     a0, 0x4(t1)             /* read the next word.              */
    bal     wait_loop               /* go wait for the interrupt.       */
    nop                             /* delay slot.                      */
    not     gp                      /* we should see ONE trap+interrupt. */
    bne     gp, t6, err             /* is it so ??                      */
    addi    t6, 1                   /* increment the interrupt count.   */
    not     gp                      /* invert the global counter.       */
    dsll32  t4, t5, 16              /* move to bits<63:32>              */
    dsrl32  a1, t4, 16              /* shift to lower<15:0>             */
    lhu     a0, 0x6(t1)             /* read the next word.              */
    bal     wait_loop               /* go wait for the interrupt.       */
    nop                             /* delay slot.                      */
    not     gp                      /* we should see ONE trap+interrupt. */
    bne     gp, t6, err             /* is it so ??                      */
    addi    t6, 1                   /* increment the interrupt count.   */
    not     gp                      /* invert the global counter.       */
    addu    t1, 8                   /* point to next double word.       */
    andi    t3, t1, 0x18            /* check the double offset.         */
    bnez    t3, 1b                  /* loopback for whole line.         */
    addu    t5, 8                   /* next data pattern.               */
    
/*
 *  [3.3] ECC error read test, do following test for a line.
 *        - byte read, 
 */
    la      t1, (TESTADDR|mem_base) /* loadup test addr.                */    
    move    t5, s0                  /* and the data pattern.            */
1:  li      t3, 32                  /* this is the most byte offset.    */
    not     t4, t5                  /* create upper bits.               */
2:  dsllv   t4, t4, t3              /* create lower order 16 bits       */
    dsrl32  a1, t3, 24              /* remove all the ff's              */
    lbu     a0, 0x0(t1)             /* read the word from the line.     */
    bal     wait_loop               /* go wait for the interrupt.       */
    nop                             /* delay slot.                      */
    not     gp                      /* we should see ONE trap+interrupt. */
    bne     gp, t6, err             /* is it so ??                      */
    addi    t6, 1                   /* increment the interrupt count.   */
    not     gp                      /* invert the global counter.       */
    addi    t1, 1                   /* point to next byte.              */
    andi    t4, t1, 0x3             /* check the byte offset.           */
    bnez    t4, 2b                  /* go for next byte in this word.   */
    addi    t3, 8                   /* move to next byte.               */
    li      t3, 32                  /* point back to byte 0             */
2:  dsllv   t4, t5, t3              /* shift to bit<63:32>              */
    dsrl32  a1, t3, 24              /* remove all the ff's              */
    lbu     a0, 0x0(t1)             /* read the word from the line.     */
    bal     wait_loop               /* go wait for the interrupt.       */
    nop                             /* delay slot.                      */
    not     gp                      /* we should see ONE trap+interrupt. */
    bne     gp, t6, err             /* is it so ??                      */
    addi    t6, 1                   /* increment the interrupt count.   */
    not     gp                      /* invert the global counter.       */
    addi    t1, 1                   /* point to next byte.              */
    andi    t4, t1, 0x3             /* check the byte offset.           */
    bnez    t4, 2b                  /* go for next byte in this word.   */
    addi    t3, 8                   /* move to next byte.               */
    andi    t3, t1, 0x18            /* check the double word offset.    */
    bnez    t3, 1b                  /* move to next doubleword.         */
    addu    t5, 8                   /* next data pattern.               */
    
/*
 *  [4.0] ECC error write test, do a word write for a whole lines.
 *        notes:  write to any word of the line will cause ecc 
 *                correction.
 */
    li      t9, 0x12                /* indicate its write cehck bit err */
    li      gp, 0xdeadbeef          /* re-initialize the interrupt.     */
    la      t1, (TESTADDR|mem_base) /* loadup test addr.                */    
    li      t2, 0x1c                /* last word of the line.           */
    li      t5, 1                   /* initialize the interupt counter  */
    addu    t3, t1, t2              /* form the test address.           */
1:  sw      t3, 0x0(t3)             /* write to memory.                 */
    bal     wait_loop               /* wait interrupt to happen.        */
    nop                             /* delay slot.                      */
    not     t4, gp                  /* see if interrupt happened.       */
    bne     t4, t5, err             /* interrupt does not happened.     */
    addi    t5, 1                   /* increment the counter.           */
    
    addu    a0, s0, s1              /* the error address (without membase)*/
    move    a1, zero                /* do check bit first.              */
    move    a2, s2                  /* the bit at fault.                */
    bal     error_ecc               /* generate a bad ecc at testaddr.  */
    move    a3, zero                /* initialize the line.             */

    sub     t2, 4                   /* decrement the counter.           */
    bgez    t2, 1b                  /* loop for a line.                 */
    addu    t3, t1, t2              /* next address.                    */

/*
 *  [4.0.1] Verify the previous 8 word write, turn off the ecc error.
 */
    sd      zero, MEMCntl(t0)       /* disable the ECC checking.        */
    ld      t2, MEMCntl(t0)         /* sync the previous write.         */
    nop                             /* give one nop.                    */
    li      t2, 0x1c                /* the last word of the line.       */
    addu    t3, t1, t2              /* the test address.                */
1:  lwu     t4, 0x0(t3)             /* read from memory.                */
    andi    t5, t2, 0x18            /* check the double word address.   */
    beq     t5, s1, 2f              /* this is the error double word.   */
    andi    t6, t2, 0x4             /* check the word offset.           */
    bne     t3, t4, err             /* we read back the wrong data.     */
    
2:  beqz    t6, 3f                  /* check which word we are checking */
    addu    t5, s0, s1              /* try to create the data pattern.  */
    not     t5                      /* invert the data pattern.         */
    dsll32  t5, t5, 0               /* move to <63:32>                  */
    dsrl32  t5, t5, 0               /* move it back to <31:0>           */
3:  bne     t4, t5, err             /* its wrong data we read back.     */
    sub     t2, 4                   /* point to next word.              */
    bgez    t2, 1b                  /* loop for a line ( 8 words )      */
    addu    t3, t1, t2              /* next word address.               */
    
    li      t2, ECCENABLE           /* re-enable the ecc checking.      */
    sd      t2, MEMCntl(t0)         /* write to memcontrol reg.         */
    ld      t2, MEMCntl(t0)         /* sync the write.                  */

/*
 *  [4.1] ECC error write test, do a halfword write for a whole lines.
 *        notes:  write to any word of the line will cause ecc 
 *                correction.
 */
    li      gp, 0xdeadbeef          /* re-initialize the interrupt.     */
    la      t1, (TESTADDR|mem_base) /* loadup test addr.                */    
    li      t2, 0x1c                /* last word of the line.           */
    li      t5, 1                   /* initialize the interupt counter  */
    addu    t3, t1, t2              /* form the test address.           */
1:  sh      t3, 0x0(t3)             /* write to memory.                 */
    bal     wait_loop               /* wait interrupt to happen.        */
    nop                             /* delay slot.                      */
    not     t4, gp                  /* see if interrupt happened.       */
    bne     t4, t5, err             /* interrupt does not happened.     */
    addi    t5, 1                   /* increment the counter.           */
    
    addu    a0, s0, s1              /* the error address (without membase)*/
    move    a1, zero                /* do check bit first.              */
    move    a2, s2                  /* the bit at fault.                */
    bal     error_ecc               /* generate a bad ecc at testaddr.  */
    move    a3, zero                /* initialize the line.             */

    sub     t2, 2                   /* decrement the counter.           */
    bgez    t2, 1b                  /* loop for a line.                 */
    addu    t3, t1, t2              /* next address.                    */
    
/*
 *  [4.1.1] Verify the previous 16 halfword write, turn off the ecc error.
 */
    sd      zero, MEMCntl(t0)       /* disable the ECC checking.        */
    ld      t2, MEMCntl(t0)         /* sync the previous write.         */
    nop                             /* give on nop here.                */
    li      t2, 0x1e                /* the last halfword of the line.   */
    addu    t3, t1, t2              /* the test address.                */
    
1:  lhu     t4, 0x0(t3)             /* read from memory.                */
    andi    t5, t2, 0x18            /* check the double word address.   */
    bne     t5, s1, 2f              /* this is the error double word.   */
    addu    t6, s0, s1              /* create the error data pattern.   */
    
    not     t5, t6                  /* invert the address.              */
    dsll32  t5, t5, 0               /* move to bits<63:32>              */
    or      t5, t5, t6              /* complete the 64 bit data pattern.*/
    andi    t6, t2, 0x6             /* extract the halfword address.    */
    sll     t6, 3                   /* create corresponding lengths.    */
    dsllv   t5, t5, t6              /* to extract the right halfword.   */
    dsrl32  t5, t5, 16              /* move to bits<15:0>               */
    bne     t4, t5, err             /* not the halfword we expected.    */
    nop                             /* get nothing else to do here.     */
    b       3f                      /* move to next byte.               */
    nop                             /* branch delay slot.               */
    
2:  dsll32  t5, t3, 16              /* extract bits<15:0>               */
    dsrl32  t5, t5, 16              /* this is the expected data.       */
    bne     t4, t5, err             /* not what we expected.            */
    nop                             /* branch delay slot.               */
    
3:  sub     t2, 2                   /* point to next halfword.          */
    bgez    t2, 1b                  /* loop for a whole line.           */
    addu    t3, t1, t2              /* next halfword address.           */
    
    li      t2, ECCENABLE           /* re-enable the ecc checking.      */
    sd      t2, MEMCntl(t0)         /* write to memcontrol reg.         */
    ld      t2, MEMCntl(t0)         /* sync the write.                  */
    
/*
 *  [4.2] ECC error write test, do a byte write for a whole line.
 *        notes:  write to any word of the line will cause ecc 
 *                correction.
 */
    li      gp, 0xdeadbeef          /* re-initialize the interrupt.     */
    la      t1, (TESTADDR|mem_base) /* loadup test addr.                */    
    li      t2, 0x1f                /* last byte of the line.           */
    li      t5, 1                   /* initialize the interupt counter  */
    addu    t3, t1, t2              /* form the test address.           */
1:  sb      t3, 0x0(t3)             /* write to memory.                 */
    bal     wait_loop               /* wait interrupt to happen.        */
    nop                             /* delay slot.                      */
    not     t4, gp                  /* see if interrupt happened.       */
    bne     t4, t5, err             /* interrupt does not happened.     */
    addi    t5, 1                   /* increment the counter.           */
    
    addu    a0, s0, s1              /* the error address (without membase)*/
    move    a1, zero                /* do check bit first.              */
    move    a2, s2                  /* the bit at fault.                */
    bal     error_ecc               /* generate a bad ecc at testaddr.  */
    move    a3, zero                /* initialize the line.             */

    sub     t2, 2                   /* decrement the counter.           */
    bgez    t2, 1b                  /* loop for a line.                 */
    addu    t3, t1, t2              /* next address.                    */
    
/*
 *  [4.2.1] Verify the previous byte writes, turn off the ecc error.
 */
    sd      zero, MEMCntl(t0)       /* disable the ECC checking.        */
    ld      t2, MEMCntl(t0)         /* sync the previous write.         */
    nop                             /* give one nop here.               */
    li      t2, 0x1f                /* the last halfword of the line.   */
    addu    t3, t1, t2              /* the test address.                */
    
1:  lbu     t4, 0x0(t3)             /* read from memory.                */
    andi    t5, t2, 0x18            /* check the double word address .  */
    bne     t5, s1, 2f              /* this is the error double word.   */
    addu    t6, s0, s1              /* create the error data pattern.   */
    
    not     t5, t6                  /* invert the address.              */
    dsll32  t5, t5, 0               /* move to bits<63:32>              */
    or      t5, t5, t6              /* complete the 64 bit data pattern.*/
    andi    t6, t2, 0x7             /* extract the byte     address.    */
    sll     t6, 3                   /* create corresponding lengths.    */
    dsllv   t5, t5, t6              /* to extract the right halfword.   */
    dsrl32  t5, t5, 24              /* move to bits<7:0>                */
    bne     t4, t5, err             /* not the halfword we expected.    */
    nop                             /* get nothing else to do here.     */
    b       3f                      /* move to next byte.               */
    nop                             /* branch delay slot.               */
    
2:  dsll32  t5, t3, 24              /* extract bits<7:0>                */
    dsrl32  t5, t5, 24              /* this is the expected data.       */
    bne     t4, t5, err             /* not what we expected.            */
    nop                             /* branch delay slot.               */
    
3:  sub     t2, 1                   /* point to next byte               */
    bgez    t2, 1b                  /* loop for a whole line.           */
    addu    t3, t1, t2              /* next halfword address.           */
    
    li      t2, ECCENABLE           /* re-enable the ecc checking.      */
    sd      t2, MEMCntl(t0)         /* write to memcontrol reg.         */
    ld      t2, MEMCntl(t0)         /* sync the write.                  */    

/*
 *  [5] Shift the bit at fault.
 */
    srl     s2, 1                   /* shift it to the right by 1 bit.  */
    bgtz    s2, 9b                  /* do the whole thing again.        */
    nop                             /* branch delay slot.               */
    
/*
 *  [6] Shift the double word at fault.
 */
    sub     s1, 8                   /* shift to next double word.       */
    bgez    s1, 9b                  /* do it all over again.            */
    addi    s2, zero, 0x80          /* reset the bit at fault.          */
    
/*
 *  [7] We done!
 */
3:  break   0    
    nop
    b       3b
    nop
    
err:    
    break    1
    nop
    b       err
    nop
    
.end mem_ECCtst1

/*
 *  We will loop here to make sure interrupt will happen.
 */
.globl wait_loop
.ent   wait_loop, 1
wait_loop:

/*
 *  [0] Loadup a wait constant. 0xff.
 */
    li      k0, 0x4f                /* wait 0xff instructions.          */
1:  bnez    k0, 1b                  /* loop here until it zero.         */
    sub     k0, 1                   /* decrement the counter.           */

/*
 *  [1] Now we do return stuff.
 */
    jr      ra                      /* jump on register r31.            */
    nop                             /* branch delay slot.               */
    
.end wait_loop


/*
 *  Create incorrect ecc check bits.
 *  register usage:
 *                  a0: Address at fault, single bit error.
 *                  a1: error in checkbits - 0, or
 *                      error in data      - 1.
 *                  a2: the error bit position. (xor value)
 *                  a3: initialize the line or not.          
 *                      (0 yes, otherwise no)
 */
.globl error_ecc
.ent   error_ecc, 1
error_ecc:

/*
 *  [0] Save all the input parameter, and disable the ECC checking.
 */
    move    s5, a0                  /* save the test address.           */
    move    s6, a1                  /* save the error type.             */
    move    s7, a2                  /* save the error bit/bits.         */
    sd      zero, MEMCntl(t0)       /* disable the ecc checking.        */

/*
 *  [1] Block write to initialize a line in the memory.
 */
    bnez    a3, 1f                  /* see if we need to initialize the */
    li      a0, 8                   /* the line.                        */
    move    a1, s0                  /* load the physical test address.  */
    move    a2, zero                /* set to zero.                     */
    break   3                       /* block write function call.       */
    nop                             /* this should initialize the ECC.  */
    
/*
 *  [2] Bring the target line into cache.
 */
1:  lui     a3, 0x8000              /* use kseg0 addr space.            */
    or      a3, a3, s5              /* got the kseg0 test addr.         */
    cache   (HitI|PD), 0x0(a3)      /* invalid the line in cache.       */
    nop                             /* give one/two nops.               */
    nop                             /* give one/two nops.               */
    ld      a0, 0x0(a3)             /* bring the line in cache.         */
    nop                             /* give one nop to settle down.     */

/*
 *  [3] Save the generated ECC check bits.    
 */
    cache   (HWB|PD), 0x0(a3)       /* write the data back.             */
    li	    a1, 0x9                 /* give 30 nops.                    */
1:  bgtz    a1, 1b		    /* loop here about 20 instructions. */
    sub	    a1, 1                   /* decrement the counter.           */
    ld      s4, EccCheckbits(t0)    /* Save the ECC check bits.         */
    
/*
 *  [4] Create incorrect check bit in the check bits.
 *      data in       a0
 *      mem_addr in   a1
 *      check bits in a2
 */
1:  bgtz    s6, 1f                  /* what kind of error, data/checkbit*/
    li      a1, 12                  /* the shift offset for check bits. */
    andi    a2, a3, 0x18            /* see which double word at fault.  */
    srl     a2, 1                   /* shift right by 1 bit.            */
    sub     a1, a1, a2              /* now we have the correct position.*/
    sllv    a1, s7, a1              /* move the error check bit in place*/
    b       2f                      /* go write the data back.          */
    xor     a2, a1, s4              /* now we have the error check bits */
1:  move    a2, s4                  /* we shall use the generated check bits */
    xor     a0, a0, s7              /* this is the data with error bits. */
2:  li      k0, ECCREPL             /* set the memory control register  */
    sd      k0, MEMCntl(t0)         /* to do ecc replacement.           */
    sd      a2, EccReplchkbits(t0)  /* set the ecc replace check bits.  */
    ld      k0, EccReplchkbits(t0)  /* sync all those mem reg writes.   */
    sd      a0, 0x0(a3)             /* update cache with new data.      */
    bne     a2, k0, err             /* Unexpected error condition.      */
    cache   (HWI|PD), 0x0(a3)       /* cause a block write to memory.   */
    nop                             /* one nop should be enough.        */
    li      k0, ECCENABLE           /* up those registers.              */
    sd      k0, MEMCntl(t0)         /* turn on ecc checking for the     */
    ld      a2, MEMCntl(t0)         /* sync the write.                  */
    
/*
 *  [4] Restore all the registers and return.
 */
    move    a0, s4                  /* return the right ecc check bits. */
    move    a1, s6                  /* the a1.                          */
    jr      ra                      /* return to caller.                */
    move    a2, s7                  /* the a2.                          */
    
.end error_ecc

/*
 *  Interrupt handler routine for ECC interrupt.
 */
.globl ecc_interrupt
.ent   ecc_interrupt, 1
ecc_interrupt:

/*
 *  [0] Check the crime interrupt status register.
 */
    ld      k0, CrimeIntrStatus(t0) /* read the crime int status        */
    lui     k1, MEMerr_INT >> 16    /* setup the mem int mask.          */
    bne     k0, k1, err             /* there are something else ??      */
    nop                             /* branch delay slot.               */
    
/*
 *  [1] Check memory error status register.
 */
    sub     k0, t9, 1               /* what kind of ecc error           */
    bgtz    k0, 1f                  /* a read modify write ??           */
    ld      k1, MEMErrSTATUS(t0)    /* read the mem err status.         */
    lui     k0, ECCWRERR >> 16      /* its read error.                  */
    and     k0, k0, k1              /* make sure wr bit not set.        */
    bnez    k0, err                 /* something wrong.                 */
    nop                             /* branch delay slot.               */
    bne     a0, a1, err             /* is the read data we expected ??  */
    nop                             /* branch delay slot.               */
    b       2f                      /* go check hard ecc error.         */
    lui     k0, HARDERR >> 16       /* setup hard err mask.             */
    
1:  lui     k0, ECCRDERR >> 16      /* check ecc read modify write.     */
    and     k0, k0, k1              /* does it ??                       */
    bnez    k0, err                 /* go check multiple error.         */
    lui     k0, HARDERR >> 16       /* see if hard error was set.       */
    
2:  and     k0, k0, k1              /* and hard error should not set.   */
    bnez    k0, err                 /* Oope! something wrong.           */
    nop                             /* branch delay slot.               */
    lui     k0, MHARDERR >> 16      /* check multiple hard error bit.   */
    and     k0, k0, k1              /* and it should not get set.       */
    bnez    k0, err                 /* error if set                     */
    nop                             /* branch delay slot.               */
    lui     k0, SOFTERR >> 16       /* check soft error bit.            */
    and     k0, k0, k1              /* it should get set.               */
    beqz    k0, err                 /* if not set - error.              */
    nop                             /* delay slot.                      */
    lui     k0, INVDADDR >> 16      /* check invalid address error.     */
    and     k0, k0, k1              /* it should not get set.           */
    bnez    k0, err                 /* error if set.                    */
 
/*
 *  [2] Check the memory error address register.
 */
    ld      k0, MEMErrADDR(t0)      /* now check the memory err addr.   */
    ld      k1, EccSyndrome(t0)     /* read the syndrome register.      */
    bne     k0, s0, err             /* is it the right address ??       */
    srlv    v0, k1, s1              /* shift to right place.            */
    beqz    v0, err                 /* should not be zero.              */
    nop                             /* this is it, goahead and clear    */
    sd      zero, MEMErrSTATUS(t0)  /* the memory error status reg.     */

/*
 *  [3] Now acknowledge the correct interrupt.
 */
    not     k0, gp                  /* check the interrupt indicator.   */
    li      k1, 0xdeadbeef          /* this is the initial value.       */
    beq     k0, k1, 1f              /* this is the first one.           */
    addi    gp, zero, 1             /* set to 1.                        */
    addi    gp, k0, 1               /* increase the indicator.          */
1:  not     gp                      /* invert the indicator.            */
        
/*
 *  [4] I think we are done with the ecc error here.
 */
    eret
    nop
    
.end ecc_interrupt
    
/*
 * ===================================================
 * $Date: 1997/08/18 20:40:15 $
 * $Revision: 1.1 $
 * $Author: philw $
 * ===================================================
#
# $Log: mem_ECCtst11.s,v $
# Revision 1.1  1997/08/18 20:40:15  philw
# updated file from bonsai/patch2039 tree
#
# Revision 1.2  1996/10/31  21:50:22  kuang
# Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
#
# Revision 1.1  1996/04/04  23:49:54  kuang
# Ported from petty_crime design verification test suite
#
# Revision 1.2  1995/06/23  19:40:50  kuang
# This is initial check in the test are not fully verified yet.
#
# Revision 1.2  1995/06/14  21:01:21  kuang
# *** empty log message ***
#
# Revision 1.1  1995/06/09  22:18:33  kuang
# Initial revision
#
 */

/* END OF FILE */
