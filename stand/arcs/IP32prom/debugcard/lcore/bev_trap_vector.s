/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:41:58 $
 * -------------------------------------------------------------
 *  descriptions:   Trap vectors table for BEV = 1
 *  DebugCARD memory layout:
 *  
 * 0x800000000    ----------------------  Top of the cache
 *                |                    |
 *                |                    |     ^
 *                |                    |     | DebugCARD stack.
 *                |                    |     | 
 *                |--------------------|  <--- (Cache_size/2 - 0x40)
 *                | 2 lines, 16 words/ |     | DebugCARD globalparms.
 *                | 8 double words.    |     v  
 *                |--------------------|  <--- (Cache_size/2)
 *                |                    |
 *                |                    |
 *                |                    |
 *                |                    |
 *     0x???K     ----------------------  Bottom of the cache
 *  
 *  fp:   usr interrupt indicator.
 *  sp:   stack pointer.
 *  gp:   globalparm.
 *        gp + 0x00: Reserved and uninitialized.
 *        gp + 0x04: prinary cache size / 2.
 *        gp + 0x08: global interrupt flag
 *        gp + 0x0c: user interrupt handler address.
 *        gp + 0x10: Interrupt Count.
 *        gp + 0x14: Saved stack pointer.
 *        gp + 0x18: user specified interrupt flag.
 *        gp + 0x1c: The LOWPROM address toggled flag.
 *        gp + 0x20: Reserved and uninitialized.
 *        gp + 0x24: Reserved and uninitialized.
 *        gp + 0x28: Reserved and uninitialized.
 *        gp + 0x2c: Reserved and uninitialized.
 *        gp + 0x30: Reserved and uninitialized.
 *        gp + 0x34: Reserved and uninitialized.
 *        gp + 0x38: Reserved and uninitialized.
 *        gp + 0x3c: Reserved and uninitialized.
 */
 
#include <sys/asm.h>
#include <sys/regdef.h>
#include <cp0regdef.h>
#include <cacheops.h>
#include <debugcard.h>
#include <crimereg.h>
#include <asm_support.h>

/*
 *  Define all external functions.
 */
.extern  UARTinit      0
.extern  UARTstatus    0
.extern  DUMPregs      0
.extern  printf        0
.extern  cmdloop       0
.extern  block_error   0

/*
 * Macros to switch from 0xbfc00000 base addr to 0xbfe00000 
 * base address.
 */
#define  HIPROM(X)  la k0, X ; jr k0 ; nop ; X: nop

/*
 *  DebugCard power on entry point.
 */
 
    .globl  bootup
    .ent    bootup
bootup:
    .set      noreorder
/*
 *  [0] align to 0xbfc0,0000 for reset/NMI/soft_reset trap entries.
 *      register usage:
 *      k0, k1
 */
    HIPROM  (RESET)                 /* switch to 0xbfe00000 base addr.  */
    mtc0    zero, Cause             /* Clear the IP[0],IP[1] in Casue   */
    nop                             /* need 1 nops                      */
    mfc0    k0, Config              /* Read the config registers.       */
    nop                             /* need 3 to 4 nops                 */
    li      k1, ~0x7                /* need 3 to 4 nops                 */
    and     k1, k0, k1              /* remove k0 field.                 */
    ori     k1, k1, 3               /* set config reg to cacheable,non- */
    mtc0    k1, Config              /* coherent, writeback.             */
    nop                             /* cp0 delay slot.                  */
    mfc0    k0, Status              /* Read the status register.        */
    li	    k1, ~0x1f               /* set KSU,ERL,EXL,IE mask.         */
    and	    k0, k0, k1              /* set to kernel mode.              */
    lui	    k1, (0x10000 >>16)      /* turn on no parity checking.      */
    or	    k0, k0, k1              /* in Status register.              */
    li      k1, 0x0000ff00          /* load the IM mask                 */
    not     k1                      /* invert the mask.                 */
    and     k0, k0, k1              /* disable all interrupts.          */
    ori     k0, 0x400               /* only allow *INT[0]               */
    mtc0    k0, Status              /* move to COP0 Status register.    */
    li      k1, 0x100000            /* check the SR bit.                */
    nop                             /* 2nd nop for mfc0                 */
    nop                             /* 3d  nop for mfc0                 */
    and     k1, k0, k1              /* check bit 20 of SR register.     */
    bnez    k1, validate_intr       /* we see a NMI or soft reset.      */
    nop                             /* branch delay slot.               */
                                    /* ================================ */
reset:                              /* entry point for general reset.   */
    move    gp, zero                /* zap the gp register.             */
    bal     cp0_init                /* initialize cp0 registers.        */
    move    fp, zero                /* make sure we do not have fp set  */
    bal     tlb_init                /* initialize TLBs.                 */
    nop                             /* branch delay slot.               */
    jal     cache_init              /* initialize caches and sp and     */
    nop                             /* gp for the rest of the world.    */
    bal     zap_registers           /* initialize R4X00 registers.      */
    nop                             /* branch delay slot.               */
    jal     UARTinit                /* initialize R4X00 registers.      */
    nop                             /* branch delay slot.               */
    jal     cmdloop                 /* at this point we have a program  */
    nop                             /* stack to work with.              */
                                    /* ================================ */
validate_intr:                      /* entry point of validate interrupt*/
#if 0
    li      k1, INTR_INDICATOR      /* loadup the expected interrupt    */
    bne     fp, k1, unexpected_intr /* Check the fp register see if     */
    nop                             /* the trap/interrupt is expected.  */
#endif
    lw      k1, USERINTR(gp)        /* load the trap handler addr       */
    nop                             /* read delay slot.                 */
    beqz    k1, unexpected_intr     /* Check the fp register see if     */
    nop                             /* k0 has the error msg address.    */
#if 0
    sw      zero, USERINTR(gp)      /* load the trap handler addr       */
#endif
    b       go_interrupt            /* this is expected interrupt/trap. */
    nop                             /* branch delay slot.               */
    
/*
 *  [1] Debugbug card are not ready to handle VA yet @*&^%$#@!~!@#$%^&*@
 *      aligned to 0xbfc0,0200
 */
    .align  9                       /* align to 0xbfc0,0200.            */
    HIPROM  (TLBREFILL)             /* switch to 0xbfe00000 base addr.  */
    la      k0, tlb_miss_error1     /* load the error message addr.     */
    j       validate_intr           /* go varify the trap is expected   */
    nop                             /* or not.                          */
    
/*
 *  [2] align to 0xbfc0,0280,
 *      Debugbug card are not ready to handle VA yet @*&^%$#@!~
 */
    .align  7                       /* align to 0xbfc0,0280.            */
    HIPROM  (XTLBREFILL)            /* switch to 0xbfe00000 base addr.  */    
    la      k0, tlb_miss_error2     /* load the error message addr.     */
    j       validate_intr           /* go varify the trap is expected   */
    nop                             /* or not.                          */
    
/*
 *  [3] align to 0xbfc0,0300 for cache error entry.
 *      Notes, BUS Error may get presented in the form of cache error,
 *      we need to make sure that no funny bit get set in the cache 
 *      error register, before we say its cache error.      
 */
    .align  8                       /* align to 0xbfc0,0300.            */
    HIPROM  (CACHEERROR)            /* switch to 0xbfe00000 base addr.  */    
    lui     k0, CrimeBASE >>16      /* read the crime interrupt status  */
    ld      k1, CrimeIntrStatus(k0) /* register,                        */
    nop                             /* read delay slot.                 */
    lui     k0, MEMerr_INT >>16     /* Does memory post an interrupt ?? */
    and     k0, k0, k1              /* and with mem error mask.         */
    bnez    k0, 1f                  /* Now we have to make sure its ECC */
    lui     k0, CrimeBASE >>16      /* error, load the crime base again */
    la      k0, cache_error_msg     /* load the cache error message     */
    j       unexpected_intr         /* it is an unexpected cache error  */
    nop                             /* expected or not.                 */
1:  mfc0    k0, CacheErr            /* read the cache error register.   */
    nop                             /* 1 nop,                           */
    nop                             /* 2 nop,                           */
    lui     k1, CacheErr_EE >>16    /* Check the EE field in cache      */
    and     k1, k0, k1              /* error register.                  */
    la      k0, cache_error_msg     /* loadup cache error message.      */
    beqz    k1, unexpected_intr     /* its not ecc error.               */
    nop                             /* branch delay slot.               */
    la      k0, ecc_error_msg       /* load the ECC error message.      */
    j       validate_intr           /* go varify the trap is expected   */
    nop                             /* branch delay slot.               */
    
    
/*
 *  [4] align to 0xbfc0,0380 for general exceptions.
 *      branch to where fp pointed at, or doe if fp = 0x0
 *  now we need to verify that which break instruction was
 *      executed, for break 0 and break 1 will be handler by 
 *  the triton c model and we need to simulate the rest.
 *  [01] break 2 - 32 bytes block read.
 *                     a0 has to be 1 (for compatability.     
 *                     a1 starting address for the block read.
 *                     a2 = zero if checking is desired.      
 *  [02] break 3 - 32 bytes block write.
 *                     a0 has to be 1 (for compatability.     
 *                     a1 starting address for the block read.
 *                     a2 ignored.
 */
    .align  7                       /* align to 0xbfc0,0380.            */
    HIPROM  (COMMONTRAP)            /* switch to 0xbfe00000 base addr.  */    
    mfc0    k0, Cause               /* check the cause register.        */
    nop                             /* Cop0 delay slot.                 */
    nop                             /* Cop0 delay slot.                 */
    nop                             /* Cop0 delay slot.                 */
    andi    k1, k0, Bp              /* see if its break point exec      */
    li      k0, Bp                  /* check the Break point condition. */
    beq     k1, k0, 2f              /* we must do some more work for    */
    nop                             /* break instruction.               */
    la      k0, common_trap_error   /* load the error message addr.     */
    j       validate_intr           /* go varify the trap is expected   */
    nop                             /* or not.                          */
    
/*  
 *  [4.1] check the break code.
 *  Here we assume that it will never in the delay slot
 *  to simplify the issure, so EPC will always point to 
 *  the break instruction. and when we finished it will point to 
 *  the instruction next to the "break" instruction.
 */
2:  mfc0    k1, EPC                 /* read the EPC                     */
    nop                             /* cop0 delay slot.                 */
    nop                             /* cop0 delay slot.                 */
    nop                             /* cop0 delay slot.                 */
    lw      k0, 0x0(k1)             /* read the break instruction.      */
    addu    fp, k1, 4               /* point to next instruction.       */
    mtc0    fp, EPC                 /* update the return address.       */
    srl     k0, k0, 16              /* get bits<19:16> to bits<3:0>     */
    andi    k0, k0, 0xf             /* separate the break code.         */
    li      k1, 2                   /* check for block read.            */
    beq     k0, k1, block_read      /* its break 2, lets do block read  */
    subu    fp, 4                   /* fp point to the "break"          */
    li      k1, 3                   /* check for block write.           */
    beq     k0, k1, block_write     /* its break 3, do block write.     */
    nop                             /* branch delay slot.               */
    mtc0    fp, EPC                 /* reinstall the EPC which point to */
    la      k0, break_error_msg     /* the "break" instruction.         */
    j       unexpected_intr         /* its an unsupported break         */
    nop                             /* instruction code.                */
    
    .end    bootup

/*
 *  Jump to user interrupt handler routine.
 */
 
#define     int_frame    288        /* 32*8 (registers) + 4*8 (outargs) */
#define     foffset      280

    .ent    go_interrupt
    .set    noreorder
go_interrupt:
    subu    sp, int_frame           /* save some kernel space regs.     */
    sd      AT,foffset-0x00(sp)     /* save AT                          */
    sd      v0,foffset-0x08(sp)     /* save v0                          */
    sd      v1,foffset-0x10(sp)     /* save v1                          */
    sd      ra,foffset-0x18(sp)     /* save ra                          */
    sd      t8,foffset-0x20(sp)     /* save t8                          */
    sd      t9,foffset-0x28(sp)     /* save t9                          */
    sd      k0,foffset-0x30(sp)     /* save k0                          */
    sd      k1,foffset-0x38(sp)     /* save k1                          */
    sd      s0,foffset-0x40(sp)     /* save s0                          */
    sd      s1,foffset-0x48(sp)     /* save s1                          */
    sd      s2,foffset-0x50(sp)     /* save s2                          */
    sd      s3,foffset-0x58(sp)     /* save s3                          */
    sd      s4,foffset-0x60(sp)     /* save s4                          */
    sd      s5,foffset-0x68(sp)     /* save s5                          */
    sd      s6,foffset-0x70(sp)     /* save s6                          */
    sd      s7,foffset-0x78(sp)     /* save s7                          */
    sd      s8,foffset-0x80(sp)     /* save s8                          */
    sd      t0,foffset-0x88(sp)     /* save t0                          */
    sd      t1,foffset-0x90(sp)     /* save t1                          */
    sd      t2,foffset-0x98(sp)     /* save t2                          */
    sd      t3,foffset-0xa0(sp)     /* save t3                          */
    sd      t4,foffset-0xa8(sp)     /* save t4                          */
    sd      t5,foffset-0xb0(sp)     /* save t5                          */
    sd      t6,foffset-0xb8(sp)     /* save t6                          */
    sd      t7,foffset-0xc0(sp)     /* save t7                          */

    jalr    k1                      /* branch to interrupt.             */
    nop                             /* delay slot.                      */

    ld      AT,foffset-0x00(sp)     /* load AT                          */
    ld      v0,foffset-0x08(sp)     /* load v0                          */
    ld      v1,foffset-0x10(sp)     /* load v1                          */
    ld      ra,foffset-0x18(sp)     /* load ra                          */
    ld      t8,foffset-0x20(sp)     /* load t8                          */
    ld      t9,foffset-0x28(sp)     /* load t9                          */
    ld      k0,foffset-0x30(sp)     /* load k0                          */
    ld      k1,foffset-0x38(sp)     /* load k1                          */
    ld      s0,foffset-0x40(sp)     /* load s0                          */
    ld      s1,foffset-0x48(sp)     /* load s1                          */
    ld      s2,foffset-0x50(sp)     /* load s2                          */
    ld      s3,foffset-0x58(sp)     /* load s3                          */
    ld      s4,foffset-0x60(sp)     /* load s4                          */
    ld      s5,foffset-0x68(sp)     /* load s5                          */
    ld      s6,foffset-0x70(sp)     /* load s6                          */
    ld      s7,foffset-0x78(sp)     /* load s7                          */
    ld      s8,foffset-0x80(sp)     /* load s8                          */
    ld      t0,foffset-0x88(sp)     /* load t0                          */
    ld      t1,foffset-0x90(sp)     /* load t1                          */
    ld      t2,foffset-0x98(sp)     /* load t2                          */
    ld      t3,foffset-0xa0(sp)     /* load t3                          */
    ld      t4,foffset-0xa8(sp)     /* load t4                          */
    ld      t5,foffset-0xb0(sp)     /* load t5                          */
    ld      t6,foffset-0xb8(sp)     /* load t6                          */
    ld      t7,foffset-0xc0(sp)     /* load t7                          */
    addu    sp, int_frame           /* restore the stack pointer.       */
    eret	                    /* return from excception.          */
    nop                             /* one more delay slot.             */
    .end    go_interrupt


    .ent    cp0_init
cp0_init:
    .set    noreorder
/*
 *  initialize all cp0 registers and set general registers to 0x0.
 */
    mtc0    zero, Wired             /* clear the TLB wired register.    */
    mtc0    zero, Context           /* clear the TLB context register.  */
    mtc0    zero, Index             /* clear the TLB index   register.  */
    mtc0    zero, PageMask          /* clear the TLB PageMask register. */
    mtc0    zero, EntryLo0          /* clear the TLB entrylo register.  */
    mtc0    zero, EntryLo1          /* clear the TLB entrylo register.  */
    mtc0    zero, EntryHi           /* clear the TLB entryHi register.  */
    mtc0    zero, TagLo             /* clear the cache taglo register.  */
    mtc0    zero, TagHi             /* clear the cache tagHi register.  */
    mtc0    zero, ErrorEPC          /* clear the cache error PC.        */
    mtc0    zero, WatchLo           /* clear the watchlo register.      */
    mtc0    zero, WatchHi           /* clear the watchhi register.      */
    mtc0    zero, Count             /* clear count register.            */
    jr      ra                      /* return.                          */
    nop                             /* branch delay slot.               */
    .end   cp0_init
    
/*
 *  unexpected interrupt/trap - save all registers
 *  for later dump to screen.
 */
#define     ufsize 0x280            /* the current stack frame size     */
#define     uregs  0x260            /* the stack starting address.      */
#define     sarea  0x20             /* the reg save ending area,        */
    .globl unexpected_intr
    .ent   unexpected_intr
unexpected_intr:                    /* The trap/interrupt was not       */
    .set    noreorder
    subu     sp, ufsize             /* expected, save enough room for   */
    sd     zero,uregs-0xf8(sp)      /* saving all registers.            */
    sd       $1,uregs-0xf0(sp)      /* saving all registers.            */
    sd       $2,uregs-0xe8(sp)      /* saving all registers.            */
    sd       $3,uregs-0xe0(sp)      /* saving all registers.            */
    sd       $4,uregs-0xd8(sp)      /* saving all registers.            */
    sd       $5,uregs-0xd0(sp)      /* saving all registers.            */
    sd       $6,uregs-0xc8(sp)      /* saving all registers.            */
    sd       $7,uregs-0xc0(sp)      /* saving all registers.            */
    sd       $8,uregs-0xb8(sp)      /* saving all registers.            */
    sd       $9,uregs-0xb0(sp)      /* saving all registers.            */
    sd      $10,uregs-0xa8(sp)      /* saving all registers.            */
    sd      $11,uregs-0xa0(sp)      /* saving all registers.            */
    sd      $12,uregs-0x98(sp)      /* saving all registers.            */
    sd      $13,uregs-0x90(sp)      /* saving all registers.            */
    sd      $14,uregs-0x88(sp)      /* saving all registers.            */
    sd      $15,uregs-0x80(sp)      /* saving all registers.            */
    sd      $16,uregs-0x78(sp)      /* saving all registers.            */
    sd      $17,uregs-0x70(sp)      /* saving all registers.            */
    sd      $18,uregs-0x68(sp)      /* saving all registers.            */
    sd      $19,uregs-0x60(sp)      /* saving all registers.            */
    sd      $20,uregs-0x58(sp)      /* saving all registers.            */
    sd      $21,uregs-0x50(sp)      /* saving all registers.            */
    sd      $22,uregs-0x48(sp)      /* saving all registers.            */
    sd      $23,uregs-0x40(sp)      /* saving all registers.            */
    sd      $24,uregs-0x38(sp)      /* saving all registers.            */
    sd      $25,uregs-0x30(sp)      /* saving all registers.            */
    sd      $26,uregs-0x28(sp)      /* saving all registers.            */
    sd      $27,uregs-0x20(sp)      /* saving all registers.            */
    sd      $28,uregs-0x18(sp)      /* saving all registers.            */
    addu    a0, sp, ufsize          /* restore original sp              */
    sd      a0, uregs-0x10(sp)      /* saving sp $29                    */
    sd      $30,uregs-0x08(sp)      /* saving all registers.            */
    sd      $31,uregs-0x00(sp)      /* saving all registers.            */
                                    /* ================================ */
    mfc0    a0, $0                  /* save cp0 registers.              */
    mfc0    a1, $1                  /* save cp0 registers.              */
    mfc0    a2, $2                  /* save cp0 registers.              */
    mfc0    a3, $3                  /* save cp0 registers.              */
    sw      a0,uregs-0x17c(sp)      /* save cp0(0)                      */
    sw      a1,uregs-0x178(sp)      /* save cp0(1)                      */
    sw      a2,uregs-0x174(sp)      /* save cp0(2)                      */
    sw      a3,uregs-0x170(sp)      /* save cp0(3)                      */
    mfc0    a0, $4                  /* save cp0 registers.              */
    mfc0    a1, $5                  /* save cp0 registers.              */
    mfc0    a2, $6                  /* save cp0 registers.              */
    mfc0    a3, zero                /* save cp0 registers.              */
    sw      a0,uregs-0x16c(sp)      /* save cp0(4)                      */
    sw      a1,uregs-0x168(sp)      /* save cp0(5)                      */
    sw      a2,uregs-0x164(sp)      /* save cp0(6)                      */
    sw      a3,uregs-0x160(sp)      /* save cp0(7)                      */
    mfc0    a0, $8                  /* save cp0 registers.              */
    mfc0    a1, $9                  /* save cp0 registers.              */
    mfc0    a2, $10                 /* save cp0 registers.              */
    mfc0    a3, $11                 /* save cp0 registers.              */
    sw      a0,uregs-0x15c(sp)      /* save cp0(8)                      */
    sw      a1,uregs-0x158(sp)      /* save cp0(9)                      */
    sw      a2,uregs-0x154(sp)      /* save cp0(10)                     */
    sw      a3,uregs-0x150(sp)      /* save cp0(11)                     */
    mfc0    a0, $12                 /* save cp0 registers.              */
    mfc0    a1, $13                 /* save cp0 registers.              */
    mfc0    a2, $14                 /* save cp0 registers.              */
    mfc0    a3, $15                 /* save cp0 registers.              */
    sw      a0,uregs-0x14c(sp)      /* save cp0(12)                     */
    sw      a1,uregs-0x148(sp)      /* save cp0(13)                     */
    sw      a2,uregs-0x144(sp)      /* save cp0(14)                     */
    sw      a3,uregs-0x140(sp)      /* save cp0(15)                     */
    mfc0    a0, $16                 /* save cp0 registers.              */
    mfc0    a1, $17                 /* save cp0 registers.              */
    move    a2, zero                /* save cp0(18)                     */
    move    a3, zero                /* save cp0(19)                     */
    sw      a0,uregs-0x13c(sp)      /* save cp0(16)                     */
    sw      a1,uregs-0x138(sp)      /* save cp0(17)                     */
    sw      a2,uregs-0x134(sp)      /* save cp0(18)                     */
    sw      a3,uregs-0x13c(sp)      /* save cp0(19)                     */
    mfc0    a0, $20                 /* save cp0 registers.              */
    move    a1, zero                /* save cp0 (21)                    */
    move    a2, zero                /* save cp0 (22)                    */
    move    a3, zero                /* save cp0 (23)                    */
    sw      a0,uregs-0x12c(sp)      /* save cp0(20)                     */
    sw      a1,uregs-0x128(sp)      /* save cp0(21)                     */
    sw      a2,uregs-0x124(sp)      /* save cp0(22)                     */
    sw      a3,uregs-0x12c(sp)      /* save cp0(23)                     */
    move    a0, zero                /* save cp0 (24)                    */
    move    a1, zero                /* save cp0 (25)                    */
    mfc0    a2, $26                 /* save cp0 registers.              */
    mfc0    a3, $27                 /* save cp0 registers.              */
    sw      a0,uregs-0x11c(sp)      /* save cp0(16)                     */
    sw      a1,uregs-0x118(sp)      /* save cp0(17)                     */
    sw      a2,uregs-0x114(sp)      /* save cp0(18)                     */
    sw      a3,uregs-0x110(sp)      /* save cp0(19)                     */
    mfc0    a0, $28                 /* save cp0 registers.              */
    mfc0    a1, $29                 /* save cp0 registers.              */
    mfc0    a2, $30                 /* save cp0 registers.              */
    move    a3, zero                /* save cp0 (31)                    */
    sw      a0,uregs-0x10c(sp)      /* save cp0(28)                     */
    sw      a1,uregs-0x108(sp)      /* save cp0(29)                     */
    sw      a2,uregs-0x104(sp)      /* save cp0(30)                     */
    sw      a3,uregs-0x100(sp)      /* save cp0(31)                     */
                                    /* ================================ */
    la      t0, CrimeBASE           /* load the crime base address.     */
    ld      a0, 0x00(t0)            /* read id register.                */
    ld      a1, 0x08(t0)            /* read crime control register.     */
    ld      a2, 0x10(t0)            /* read interrupt status register.  */
    ld      a3, 0x18(t0)            /* read interrupt enable register.  */
    ld      s0, 0x20(t0)            /* read software interrupt reg.     */
    ld      s1, 0x28(t0)            /* read hardware interrupt reg.     */
    ld      s2, 0x30(t0)            /* read watchdog timer              */
    ld      s3, 0x38(t0)            /* read crime timer.                */
    ld      s4, 0x40(t0)            /* read cpu error register.         */
    ld      s5, 0x48(t0)            /* read cpu/vice error status.      */
    ld      s6, 0x50(t0)            /* read cpu/vice error enable.      */
    ld      s7, 0x58(t0)            /* read vice error address.         */
    sd      a0,uregs-0x250(sp)      /* save id register.                */
    sd      a1,uregs-0x248(sp)      /* save crime control register,     */
    sd      a2,uregs-0x240(sp)      /* save interrupt status register,  */
    sd      a3,uregs-0x238(sp)      /* save interrupt enable register.  */
    sd      s0,uregs-0x230(sp)      /* save software interrupt reg.     */
    sd      s1,uregs-0x228(sp)      /* save hardware interrupt reg.     */
    sd      s2,uregs-0x220(sp)      /* save watcher timer.              */
    sd      s3,uregs-0x218(sp)      /* save crime timer.                */
    sd      s4,uregs-0x210(sp)      /* save cpu error register.         */
    sd      s5,uregs-0x208(sp)      /* save cpu/vice error status.      */
    sd      s6,uregs-0x200(sp)      /* save cpu/vice error enable.      */
    sd      s7,uregs-0x1f8(sp)      /* save vice error address.         */
                                    /* ================================ */
    la      t0, MEMcntlBASE         /* load the crime base address.     */
    ld      a0, 0x00(t0)            /* read mem control/status reg.     */
    ld      a1, 0x08(t0)            /* read bank0 control register.     */
    ld      a2, 0x10(t0)            /* read bank1 control register.     */
    ld      a3, 0x18(t0)            /* read bank2 control register.     */
    ld      s0, 0x20(t0)            /* read bank3 control register.     */
    ld      s1, 0x28(t0)            /* read bank4 control register.     */
    ld      s2, 0x30(t0)            /* read bank5 control register.     */
    ld      s3, 0x38(t0)            /* read bank6 control register.     */
    ld      s4, 0x40(t0)            /* read bank7 control register.     */
    ld      s5, 0x48(t0)            /* refresh counter.                 */
    ld      s6, 0x50(t0)            /* memory error status register.    */
    ld      s7, 0x58(t0)            /* memory error addr register.      */
    ld      v0, 0x60(t0)            /* ECC Syndrome.                    */
    ld      v1, 0x68(t0)            /* ECC Generated check bits         */
    ld      t2, 0x70(t0)            /* ECC Replacement check bits.      */
    sd      a0,uregs-0x1f0(sp)      /* save mem control/status reg.     */
    sd      a1,uregs-0x1e8(sp)      /* save bank0 control register.     */
    sd      a2,uregs-0x1e0(sp)      /* save bank1 control register,     */
    sd      a3,uregs-0x1d8(sp)      /* save bank2 control register.     */
    sd      s0,uregs-0x1d0(sp)      /* save bank3 control register.     */
    sd      s1,uregs-0x1c8(sp)      /* save bank4 control register.     */
    sd      s2,uregs-0x1c0(sp)      /* save bank5 control register.     */
    sd      s3,uregs-0x1b8(sp)      /* save bank6 control register.     */
    sd      s4,uregs-0x1b0(sp)      /* save bank7 control register.     */
    sd      s5,uregs-0x1a8(sp)      /* save mem error status register.  */
    sd      s6,uregs-0x1a0(sp)      /* save mem error addr register.    */
    sd      s7,uregs-0x198(sp)      /* save vice error address.         */
    sd      v0,uregs-0x190(sp)      /* save ECC Syndrome.               */
    sd      v1,uregs-0x188(sp)      /* save ECC generated check bits    */
    sd      t2,uregs-0x180(sp)      /* save ECC replacement check bits  */
                                    /* ================================ */
1:  move    a0, k0                  /* pass the error message string.   */
    addiu   a1, sp, uregs-0xf8      /* and IU  registeer save area.     */
    addiu   a2, sp, uregs-0x17c     /* and CP0 registeer save area.     */
    jal     DUMPregs                /* there is no return.              */
    addiu   a3, sp, uregs-0x250     /* and CP0 registeer save area.     */
    
9:  b       9b                      /* loop forever for now.            */
    nop                             /* delay slot.                      */
    .end   unexpected_intr
    
    
    .ent    zap_registers
zap_registers:
    .set    noreorder
/*
 *  initialize all cp0 registers and set general registers to 0x0.
 */
 
    move    $2, zero                /* initialize general register 2    */
    move    $3, zero                /* initialize general register 3    */
    move    $4, zero                /* initialize general register 4    */
    move    $5, zero                /* initialize general register 5    */
    move    $6, zero                /* initialize general register 6    */
    move    $7, zero                /* initialize general register 7    */
    move    $8, zero                /* initialize general register 8    */
    move    $9, zero                /* initialize general register 9    */
    move   $10, zero                /* initialize general register 10   */
    move   $11, zero                /* initialize general register 11   */
    move   $12, zero                /* initialize general register 12   */
    move   $13, zero                /* initialize general register 13   */
    move   $14, zero                /* initialize general register 14   */
    move   $15, zero                /* initialize general register 15   */
    move   $16, zero                /* initialize general register 16   */
    move   $17, zero                /* initialize general register 17   */
    move   $18, zero                /* initialize general register 18   */
    move   $19, zero                /* initialize general register 19   */
    move   $20, zero                /* initialize general register 20   */
    move   $21, zero                /* initialize general register 21   */
    move   $22, zero                /* initialize general register 22   */
    move   $23, zero                /* initialize general register 23   */
    move   $24, zero                /* initialize general register 24   */
    move   $25, zero                /* initialize general register 25   */
    move   $26, zero                /* initialize general register 26   */
    move   $27, zero                /* initialize general register 27   */
                                    /* we skip $28 and $29 - gp and sp  */
    jr      ra                      /* return.                          */
    move   $30, zero                /* initialize general register 30   */
    .end    zap_registers


    .ent    block_read
block_read:
    .set    noreorder
/*
 *  Simulate r4k model block read - break 2 instruction.
 *          a0 = 8
 *          a1 = starting address.
 *          a2 = 0 if checking is desired.
 *          a3 = cache index for test address.
 *             
 */
    subu    sp, framesize           /* create local stack frame.        */
    sd      ra, regsave-0x00(sp)    /* save return address.             */
    sd      a1, regsave-0x08(sp)    /* save a1                          */
    sd      a2, regsave-0x10(sp)    /* save a2                          */
    sd      a3, regsave-0x18(sp)    /* save a3                          */
    sd      v0, regsave-0x20(sp)    /* save v0                          */
    sd      v1, regsave-0x28(sp)    /* save v1                          */
    sd      AT, regsave-0x30(sp)    /* save AT                          */
 #  
 # [01] Create the cacheable test address.
 #      a0 -> cacheable test address.
 #      a1 -> physical address of the test address.
 # 
    mfc0    v0, Config              /* read the Config register,        */
    sll     a1, a1, 3               /* remove bits<31:29>               */
    srl     a1, 3                   /* done.                            */
    lui     a0, 0x8000              /* or in the cacheable prefix.      */
    or      a0, a0, a1              /* save the starting address.       */
 #  
 # [02] Create the corresponding cache tags.
 #      a3 -> cache index for test address.
 #      We are here making an importance assuption, both R4600 and R5000
 #      are two way set associative primary so the test address can fall in 
 #      either line of the set as long as the virtual index match the set,
 #      since we already allocate the first half of the cache so we'll
 #      have to use the second set.
 #      BUT, as we all know the hardware implementation may not work as 
 #      we may expected so this can be a BIG problem and we'll have to verify 
 #      how two way set associcate primary cache replacement algorithm works.
 #      -- as far as I can tell its not a true two way set associate cache.
 #
    li      a3, DC                  /* load the cache size mask.        */
    and     a3, a3, v0              /* isolate the DC from Config reg.  */
    srl     a3, 6                   /* Shift into right position.       */
    li      v0, 0x1000              /* the base cache size.             */
    sllv    a3, v0, a3              /* got the size in a3.              */
    srl     a3, 1                   /* half of the cache point.         */
    or      a3, a0, a1              /* cache index for test address.    */
 #
 # [03] Initialize the cache tags for test address.
 #
    mtc0    zero, TagHi             /* set the TagHi register.          */
    srl     v0, a1, 12              /* generate physical addr<35:12>    */
    sll     v0, 8                   /* and move to <31:8>               */
    ori     v0, 0x2                 /* and set the F (FIFO - 4600/5000) */
    mtc0    v0 TagLo                /* set the TagLo register.          */
    nop                             /* give them some nops              */
    nop                             /* give them some nops              */
    nop                             /* give them some nops              */
    cache   (PD|IST), 0x0(a3)       /* invalid the cache.               */
    nop                             /* better wait for a while.         */
    nop                             /* better wait for a while.         */
 #    
 # [04] Now we should be able to do the block read for 
 #      the test address.
 #
    ld      v1, 0x0(a0)             /* read miss - cause a block read.  */
    bnez    a2, 3f                  /* see if we need to check data.    */
    not     v0, a1                  /* complement current address.      */
    dsll32  v0, v0, 0               /* move to bits<64:32>              */
    or      v0, v0, a1              /* the expected data.               */
    bne     v1, v0, 2f              /* error if not compare.            */
    move    a2, a1                  /* branch delay slot.               */
    addu    a2, a1, 0x8             /* point to next double word.       */
    ld      v1, 0x8(a0)             /* read the second double word.     */
    not     v0, a2                  /* complement current address.      */
    dsll32  v0, v0, 0               /* move to bits<64:32>              */
    or      v0, v0, a2              /* the expected data.               */
    bne     v1, v0, 2f              /* block_error if not compare.      */
    nop                             /* branch delay slot.               */
    addu    a2, a1, 0x10            /* point to next double word.       */
    ld      v1, 0x10(a0)            /* read the thirs double word.      */
    not     v0, a2                  /* complement current address.      */
    dsll32  v0, v0, 0               /* move to bits<64:32>              */
    or      v0, v0, a2              /* the expected data.               */
    bne     v1, v0, 2f              /* block_error if not compare.      */
    nop                             /* branch delay slot.               */
    addu    a2, a1, 0x18            /* point to next double word.       */
    ld      v1, 0x18(a0)            /* read the thirs double word.      */
    not     v0, a2                  /* complement current address.      */
    dsll32  v0, v0, 0               /* move to bits<64:32>              */
    or      v0, v0, a2              /* the expected data.               */
    bne     v1, v0, 2f              /* block_error if not compare.      */
    nop                             /* branch delay slot.               */
    b       3f                      /* test passed, go invalidate the   */
    li      a0, 1                   /* cache and return.                */
2:  sd      a3, localargs-0x00(sp)  /* save the a3 register.            */
    mfc0    a3, EPC                 /* read the EPC from cop0           */
    sd      v0, localargs-0x08(sp)  /* the expected data pattern        */
    sd      v1, localargs-0x10(sp)  /* the observed data pattern        */
    addu    a0, sp, localargs-0x08  /* the addr of expected data.       */
    addu    a1, sp, localargs-0x10  /* the addr of observed data.       */
    jal     block_error             /* test failed, go print the error  */
    subu    a3, 4                   /* point to the break instruction   */
    ld      a3, localargs-0x00(sp)  /* restore a3 register.             */
    move    a0, zero                /* indicate test failed.            */
3:  cache   (PD|HitI), 0x0(a3)      /* invalidate the cache line.       */
 #
 # [05] And we are done with the break 2 instruction.
 #
    ld      ra, regsave-0x00(sp)    /* load a1                          */
    ld      a1, regsave-0x08(sp)    /* load a1                          */
    ld      a2, regsave-0x10(sp)    /* load a2                          */
    ld      a3, regsave-0x18(sp)    /* load a3                          */
    ld      v0, regsave-0x20(sp)    /* load v0                          */
    ld      v1, regsave-0x28(sp)    /* load v1                          */
    ld      AT, regsave-0x30(sp)    /* load AT                          */
    addiu   sp, framesize           /* delete current stack frame.      */
    eret                            /* return.                          */
    nop                             /* just in case.                    */
    .end    block_read
    
    
    .ent    block_write
block_write:
    .set    noreorder
/*
 *  Simulate r4k model block write - break 3 instruction.
 *          a0 = 8
 *          a1 = starting address.
 *          a2 = ignored
 */
    subu    sp, framesize           /* create local stack frame.        */
    sd      a0,regsave-0x00(sp)     /* save a0                          */
    sd      a1,regsave-0x08(sp)     /* save a1                          */
    sd      a2,regsave-0x10(sp)     /* save a2                          */
    sd      a3,regsave-0x18(sp)     /* save a3                          */
    sd      v0,regsave-0x20(sp)     /* save v0                          */
    sd      v1,regsave-0x28(sp)     /* save v1                          */
    sd      AT,regsave-0x30(sp)     /* save AT                          */
 #  
 # [01] Create the cacheable test address.
 #      a0 -> cacheable test address.
 #      a1 -> physical address of the test address.
 # 
    mfc0    v0, Config              /* read the Config register,        */
    sll     a1, a1, 3               /* remove bits<31:29>               */
    srl     a1, 3                   /* done.                            */
    lui     a0, 0x8000              /* or in the cacheable prefix.      */
    or      a0, a0, a1              /* save the starting address.       */
 #  
 # [02] Create the corresponding cache tags.
 #      a3 -> cache index for test address.
 #      We are here making an importance assuption, both R4600 and R5000
 #      are two way set associative primary so the test address can fall in 
 #      either line of the set as long as the virtual index match the set,
 #      since we already allocate the first half of the cache so we'll
 #      have to use the second half.
 #      BUT, as we all know the hardware implementation may not work as 
 #      we may expected so this can be a BIG problem and we'll have to verify 
 #      how two way set associcate primary cache replacement algorithm works.
 #      -- as far as I can tell its not a true two way set associate cache.
 #
    li      a3, DC                  /* load the cache size mask.        */
    and     a3, a3, v0              /* isolate the DC from Config reg.  */
    srl     a3, 6                   /* Shift into right position.       */
    li      v0, 0x1000              /* the base cache size.             */
    sllv    a3, v0, a3              /* got the size in a3.              */
    srl     a3, 1                   /* half of the cache point.         */
    or      a3, a0, a3              /* cache index for test address.    */
 #
 # [03] Initialize the cache tags for test address.
 #
    mtc0    zero, TagHi             /* set the TagHi register.          */
    srl     v0, a1, 12              /* generate physical addr<35:12>    */
    sll     v0, 8                   /* and move to <31:8>               */
    ori     v0, 0x3<<6              /* set state to dirty exclusive.    */
    mtc0    v0 TagLo                /* set the TagLo register.          */
    nop                             /* give them some nops              */
    nop                             /* give them some nops              */
    nop                             /* give them some nops              */
    cache   (PD|IST), 0x0(a3)       /* create the line in cache.        */
    nop                             /* better wait for a while.         */
    nop                             /* better wait for a while.         */
 #    
 # [04] Now we should be able to do the block read for 
 #      the test address.
 #
    not     v0, a1                  /* not the address.                 */
    dsll32  v0, v0, 0               /* move to bits<63:32>              */
    or      v0, v0, a1              /* the first data created.          */
    sd      v0, 0x0(a0)             /* first double word.               */
    addu    v1, a1, 8               /* next double word.                */
    not     v0, v1                  /* !!!                              */
    dsll32  v0, v0, 0               /* move to bits<64:32>              */
    or      v0, v0, v1              /* the second data created.         */
    sd      v0, 0x8(a0)             /* second double word.              */
    addu    v1, a1, 0x10            /* next double word.                */
    not     v0, v1                  /* !!!                              */
    dsll32  v0, v0, 0               /* move to bits<64:32>              */
    or      v0, v0, v1              /* the third  data created.         */
    sd      v0, 0x10(a0)            /* third  double word.              */
    addu    v1, a1, 0x18            /* next double word.                */
    not     v0, v1                  /* !!!                              */
    dsll32  v0, v0, 0               /* move to bits<64:32>              */
    or      v0, v0, v1              /* the forth  data created.         */
    sd      v0, 0x18(a0)            /* forth  double word.              */
    nop                             /* we need a break, ...             */
    cache   (PD|IWI), 0x0(a3)       /* write back the line.             */
    nop                             /* just in case.                    */
 #
 # [05] And we are done with the break 2 instruction.
 #
    ld      a0, regsave-0x00(sp)    /* load a0                          */
    ld      a1, regsave-0x08(sp)    /* load a1                          */
    ld      a2, regsave-0x10(sp)    /* load a2                          */
    ld      a3, regsave-0x18(sp)    /* load a3                          */
    ld      v0, regsave-0x20(sp)    /* load v0                          */
    ld      v1, regsave-0x28(sp)    /* load v1                          */
    ld      AT, regsave-0x30(sp)    /* load AT                          */
    addu    sp, framesize           /* delete current stack frame.      */
    eret                            /* return.                          */
    nop                             /* just in case.                    */
    .end    block_write


    .globl  global_interrupt
    .ent    global_interrupt
global_interrupt:
    .set      noreorder
/*
 *  Return global interrupt flags
 */
    jr      ra                      /* return to caller and             */
    addiu   v0, gp, GLOBALINTR      /* load the gloabl interrupt flag   */
    .end    global_interrupt
    
    
    .globl  usrinterrupt_flag
    .ent    usrinterrupt_flag
usrinterrupt_flag:
    .set      noreorder
/*
 *  Return global interrupt flags
 */
    bnez    a0, 1f                  /* 0 mean write, 1 otherwise write  */
    move    v0, zero                /* zap the return value.            */
    jr      ra                      /* return for the write request.    */
    sw      a1, USRINTRFLG(gp)      /* Doing a write.                   */
1:  jr      ra                      /* return for the read request.     */
    lw      v0, USRINTRFLG(gp)      /* Doing the read request.          */
    .end    usrinterrupt_flag
    
    
    .globl  regist_interrupt
    .ent    regist_interrupt
regist_interrupt:
    .set      noreorder
/*
 *  We'll have to find out what kind processor we are running, and 
 *  initialize the tlb entry accordingly.
 *  For TLB entries - 
 *          4600 = 48 entries.
 *          4700 = 48 entries.
 *        Triton = 48 entries.
 */
    subu    sp, framesize           /* create local stack frame.        */
    sd      AT,regsave-0x00(sp)     /* save AT                          */
    sw      a0, USERINTR(gp)        /* save user interrupt routine.     */
    addiu   v0, gp, GLOBALINTR      /* return global_interrupt addr.    */
    ld      AT,regsave-0x00(sp)     /* load AT                          */
    jr      ra                      /* return                           */
    addu    sp, framesize           /* delete current stack frame.      */
    .end    regist_interrupt


    .globl  unregist_interrupt
    .ent    unregist_interrupt
unregist_interrupt:
    .set      noreorder
/*
 *  Unregister a trap/interrupt handler.
 */
    jr      ra                      /* return                           */
    sw      zero, USERINTR(gp)      /* zap USERINTR flags               */
    .end    regist_interrupt


    .globl  tlb_init
    .ent    tlb_init
tlb_init:
    .set      noreorder
/*
 *  We'll have to find out what kind processor we are running, and 
 *  initialize the tlb entry accordingly.
 *  For TLB entries - 
 *          4600 = 48 entries.
 *          4700 = 48 entries.
 *        Triton = 48 entries.
 */
    mtc0    zero, PageMask          /* set 4k page size.                */
    move    k0, zero                /* set the index.                   */
    li      a0, 0x1000              /* set 4k page size.                */
1:  mtc0    k0, Index               /* set the index register.          */
    mtc0    a0, EntryHi             /* set virtual index.               */
    srl     a1, a0, 12              /* get bit <35:12> to position      */
    sll     a1, a1, 6               /* <29:6> in entrylo register.      */
    mtc0    a1, EntryLo0            /* set the first TLB entry.         */
    addi    a0, a0, 0x1000          /* point to next page.              */
    srl     a1, a0, 12              /* get bit <35:12> to position      */
    sll     a1, a1, 6               /* <29:6> in entrylo register.      */
    mtc0    a1, EntryLo1            /* set the second TLB entry.        */
    addi    k0, k0, 0x1             /* increment the index.             */
    tlbwi                           /* write to TLB array.              */
    blt     k0, 48, 1b              /* loop for 48 pairs TLB entries.   */
    addi    a0, a0, 0x1000          /* point to next pair of TLBs.      */
    
#if defined(DEBUGCARDSTACK)
/*
 *  Now hardwired 8 pair of tlbs for local memory - primary cache, 
 *  for debugcard monitor stack.
 *  VA[0] --> PA[0x800000000], used by debugcard stack.
 */
    move    a0, zero                /* the VPN2                         */
    lui     a1, (0x20000000 >>16)   /* the PFN, set addr bit 35         */
    ori     a1, 0x1e                /* set to Cacheable write-back      */
    move    k0, zero                /* the TLB index.                   */
    li      k1, 8                   /* map 8 pairs of TLBs              */
1:  mtc0    a1, EntryLo0            /* set the TLB[0,0]                 */
    mtc0    k0, Index               /* set the TLB index to first pair. */
    mtc0    a0, EntryHi             /* set the virtual address VPN2     */
    addiu   a1, 0x1000              /* 4k increment.                    */
    mtc0    a1, EntryLo1            /* set the TLB[0,1]                 */
    nop                             /* give few nops.                   */
    nop                             /* give few nops.                   */
    tlbwi                           /* write to tlb arraay.             */
    add     k0, 1                   /* poiint to next pair.             */
    addiu   a0, 0x2000              /* prepare next TLB's VPN2          */
    blt     k0, k1, 1b              /* Are we done for 8 pair ??        */
    addiu   a1, 0x1000              /* increment the PFN.               */
    
    li      a0, 8                   /* 2 pairs of TLB are hardwired.    */
    mtc0    a0, Wired               /* update the TLB wired register    */
    mtc0    a0, Index               /* and the index register.          */
#endif

/*
 *  We done, return
 */
    jr      ra                      /* Done with all 48 entries.        */
    move    v0, zero                /* return 0.                        */
    .end    tlb_init


/*
 *  Here are all the error messages.
 */
tlb_miss_error1:
    .asciiz "\nUnexpected TLB miss exception."
    
tlb_miss_error2:
    .asciiz "\nUnexpected xTLB miss exception."
    
cache_error_msg:
    .asciiz "\nUnexpected cache error."
    
ecc_error_msg:
    .asciiz "\nUnexpected memory ECC error."
    
common_trap_error:
    .asciiz "\nUnexpected general expception/interrupt."
    
break_inst_error:
    .asciiz "\nUnsupported break instruction."
    
break_error_msg:
    .asciiz "\nUnsupported break instruction."
    
/*
 * -------------------------------------------------------------
#
#  $Log: bev_trap_vector.s,v $
#  Revision 1.1  1997/08/18 20:41:58  philw
#  updated file from bonsai/patch2039 tree
#
# Revision 1.4  1996/10/31  21:51:47  kuang
# Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
#
# Revision 1.4  1996/10/04  20:09:07  kuang
# Fixed some general problem in the diagmenu area
#
# Revision 1.3  1996/04/04  23:17:23  kuang
# Added more diagnostic support and some general cleanup
#
# Revision 1.2  1995/12/30  03:28:05  kuang
# First moosehead lab bringup checkin, corresponding to 12-29-95 d15
#
# Revision 1.1  1995/11/15  00:42:43  kuang
# initial checkin
#
# Revision 1.3  1995/11/14  23:05:40  kuang
# Final cleanup for ptool checkin
#
 * -------------------------------------------------------------
 */

/* END OF FILE */
