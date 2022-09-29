/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:41:56 $
 * -------------------------------------------------------------
 */
#if !defined(IP32)
#define IP32 1
#endif

#include <sys/asm.h>
#include <sys/regdef.h>
#include <cp0regdef.h>
#include <cacheops.h>
#include <debugcard.h>
#include <asm_support.h>
#include <mooseaddr.h>
#include <crimereg.h>
#include <sbd.h>
#include <mace.h>
#include <ds17287.h>

.extern  BlockRD_error 0
.extern  bootup        0
.extern  tlb_init      0
.extern  cache_init    0
.extern  printf        0

/*
 * Firmware dispatch codes
 */
#define FW_HARD_RESET   0   /* Power on or hard reset*/
#define FW_SOFT_RESET   1   /* Soft reset or NMI */
#define FW_EIM          2   /* Enter Interactive Mode */
#define FW_HALT         3   /* Halt */
#define FW_POWERDOWN    4   /* Power down */
#define FW_RESTART      5   /* Restart */
#define FW_REBOOT       6   /* Reboot */

/*
 * NOREORDER FOR THE WHOLE FILE.
 */
.set noreorder

/*
 * -------------------------------------------------------------
 * Setup power control bits.
 * shut down the power supply.  Dallas docs indicate that the following
 * conditions must hold for kickstart to work:
 *
 *	dv 2/1/0 in register a must have a value of 01xb
 *	((kf & kse) != 1) && ((wf & wie) != 1)
 *      kse == 1 to enable powerup via the power switch
 *
 * -------------------------------------------------------------
 */
.globl power_off
.ent   power_off
power_off:
    la      a0, PHYS_TO_K1(ISA_FLASH_NIC_REG) /* used to flush the wbuf */
    la      t8, RTDS_CLOCK_ADDR     /* loadup the ISA_RTC_BASE+7        */
    lbu     t9, DS_OFFSET(0,DS_A_REG)(t8)  /* read the register a       */
    li      s0, ~(DS_REGA_DV2|DS_REGA_DV0) /* clear DV2 and DV0         */
    and     s0, t9, s0              /* clear DV2, DV0                   */
    ori     s0, DS_REGA_DV1         /* set DV1 in registera             */
    ori     t9, s0, DS_REGA_DV0     /* select BANK 1 for next read      */
    sb      t9, DS_OFFSET(0,DS_A_REG)(t8)  /* set dv(0,1,2) to (1,1,0)  */
                                    /* SET EXTENED COTROL REG 4B        */
    ld      zero, (a0)              /* flush the write buffer.          */
    lbu     t9, DS_OFFSET(0,DS_RAM)+DS_BANK1_XCTRLB(t8)
    ori     t9, DS_XCTRLB_ABE|DS_XCTRLB_KSE /* set ABE and KSE.         */
    sb      t9, DS_OFFSET(0,DS_RAM)+DS_BANK1_XCTRLB(t8)
                                    /* SET ENTENDED CONTROL REG 4A.     */
    ld      zero, (a0)              /* flush the write buffer.          */
    lbu     t9, DS_BANK1_XCTRLA+DS_OFFSET(0,DS_RAM)(t8)
    li      t0, ~(DS_XCTRLA_KF|DS_XCTRLA_WF) /* KE and WF bit mask      */
    and     t9, t9, t0              /* clear KF and WF bits.            */
    sb      t9, DS_BANK1_XCTRLA+DS_OFFSET(0,DS_RAM)(t8)
    ld      zero, (a0)              /* flush the write buffer.          */
                                    /* SHUT DOWN THE POWER SUPPLY       */
    ori     t9, DS_XCTRLA_PAB       /* write PAB in x cntrol a reg.     */
    sb      t9, DS_BANK1_XCTRLA+DS_OFFSET(0,DS_RAM)(t8)
    sb      s0, DS_OFFSET(0,DS_A_REG)(t8)    /* clear DV0 in rega.      */
    ld      t0, (a0)                /* flush the write buffer.          */
    xori    t0, ISA_LED_RED|ISA_LED_GREEN /* set the approiate value.   */
    sd      t0, 0x0(a0)             /* write it back                    */
    lui     t8, 0x4000              /* loadup a big number              */
    dsll32  t8, 0                   /* create a 64 bit data.            */
1:  bnez    t8, 1b                  /* loop here until t8 == 0          */
    sub     t8, 1                   /* decrement t8                     */
    b       power_off               /* lets try again.                  */
    nop                             /* branch delay slot.               */
    
.end power_off


/*
 * -------------------------------------------------------------
 *  Turn on LEDS for IP32
 * ------------------------------------------------------------- 
 */
.globl IP32LED_ON
.ent   IP32LED_ON
IP32LED_ON: 
    subu    sp, framesize           /* create current stack frame.      */
    sd      AT,regsave-0x00(sp)     /* save AT                          */
    sd      t0,regsave-0x10(sp)     /* save t0                          */
    sd      t1,regsave-0x18(sp)     /* save t1                          */
    sd      t2,regsave-0x20(sp)     /* save t2                          */

    la      t0, PHYS_TO_K1(ISA_FLASH_NIC_REG) /* used to flush the wbuf */
    ld      t1, 0x0(t0)             /* read the original value          */
    andi    t2, a0, ISA_LED_RED|ISA_LED_GREEN /* extract the led bits.  */
    not     t2                      /* and invert the value.            */
    and     t1, t1, t2              /* now turn it on.                  */
    sd      t1, 0x0(t0)             /* then write it back to mace.      */
    
    ld      AT,regsave-0x00(sp)     /* save AT                          */
    ld      t0,regsave-0x10(sp)     /* save t0                          */
    ld      t1,regsave-0x18(sp)     /* save t1                          */
    ld      t2,regsave-0x20(sp)     /* save t2                          */
    jr      ra                      /* return to caller.                */
    addiu   sp, framesize           /* restore previous stack frame     */
    
.end   IP32LED_ON

/*
 * -------------------------------------------------------------
 *  Turn off LEDS for IP32
 * ------------------------------------------------------------- 
 */
.globl IP32LED_OFF
.ent   IP32LED_OFF
IP32LED_OFF: 
    subu    sp, framesize           /* create current stack frame.      */
    sd      AT,regsave-0x00(sp)     /* save AT                          */
    sd      t0,regsave-0x10(sp)     /* save t0                          */
    sd      t1,regsave-0x18(sp)     /* save t1                          */
    sd      t2,regsave-0x20(sp)     /* save t2                          */

    la      t0, PHYS_TO_K1(ISA_FLASH_NIC_REG) /* used to flush the wbuf */
    ld      t1, 0x0(t0)             /* read the original value          */
    andi    t2, a0, ISA_LED_RED|ISA_LED_GREEN /* extract the led bits.  */
    or      t1, t1, t2              /* now turn it off                  */
    sd      t1, 0x0(t0)             /* then write it back to mace.      */
    
    ld      AT,regsave-0x00(sp)     /* save AT                          */
    ld      t0,regsave-0x10(sp)     /* save t0                          */
    ld      t1,regsave-0x18(sp)     /* save t1                          */
    ld      t2,regsave-0x20(sp)     /* save t2                          */
    jr      ra                      /* return to caller.                */
    addiu   sp, framesize           /* restore previous stack frame     */

.end   IP32LED_OFF

/*
 * -------------------------------------------------------------
 *  Set the interrupt enable bit in status register.
 * ------------------------------------------------------------- 
 */
.globl enable_interrupt
.ent   enable_interrupt
enable_interrupt:
    mfc0    a0, Status              /* read the status register.        */
    nop	                            /* wait for a while                 */
    nop	                            /* wait for a while                 */
    nop	                            /* wait for a while                 */
    ori	    a0, a0, 1               /* enable interrupt.                */
    mtc0    a0, Status              /* update the status register.      */
    jr      ra
    nop
.end   enable_interrupt

/*
 * -------------------------------------------------------------
 *  Clear the interrupt enable bit in status register.
 * ------------------------------------------------------------- 
 */
.globl disable_interrupt
.ent   disable_interrupt
disable_interrupt:
    mfc0    a0, Status              /* read the status register.        */
    nop	                            /* wait for a while                 */
    nop	                            /* wait for a while                 */
    nop	                            /* wait for a while                 */
    li	    a1, 1		    /* the interrupt bit mask           */
    not	    a1                      /* the and/or mask                  */
    and	    a0, a0, a1              /* clear the interrupt enable bit.  */
    mtc0    a0, Status              /* update the status register.      */
    jr      ra
    nop
.end   disable_interrupt


/*
 * -------------------------------------------------------------
 *  Initialize a line in cache and return the virtual cache line
 *  index in v0.
 * -------------------------------------------------------------
 */
.globl 	line_in_cache
.globl 	line_in_cache
.ent    line_in_cache
line_in_cache:
    subu    sp, framesize           /* create current stack frame.      */
    sd      AT,regsave-0x00(sp)     /* save AT                          */
    sd      ra,regsave-0x08(sp)     /* save ra                          */
    sd      t0,regsave-0x10(sp)     /* save t0                          */
    sd      t1,regsave-0x18(sp)     /* save t1                          */
    sd      t2,regsave-0x20(sp)     /* save t2                          */
    sd      t3,regsave-0x28(sp)     /* save t3                          */
    sd      t4,regsave-0x30(sp)     /* save t4                          */
 #
 # Create testaddr in the cache, set 1.
 #  t0: cache line virtual index.
 #  t2: physical test address.
 #  t4: kseg0 test address.
 #
    mtc0    zero, TagHi             /* initialize the TagHi register.   */
    sll     t0, a0, 3               /* remove the upper 3 bit.          */
    srl     t2, t0, 15              /* move paddr<35:12> to P<23:0>     */
    sll     t2, 8                   /* then to PtagLo<31:8>             */
    ori     t2, 0x3 <<6             /* create a dirty exclusive line.   */
    mtc0    t2, TagLo               /* move to TagLo register.          */
    srl     t0, 3                   /* remove the upper 3 bits<31:29>   */
    move    t2, t0                  /* this is the physical test addr.  */
    lui     t1, kseg0 >>16          /* this is KSEG0 base.              */
    lwu     t3, PCACHESET(gp)       /* read pcache set size.            */
    or      t4, t0, t1              /* save KSEG0 test address.         */
    or      t0, t3, t4              /* this is the cache virtual index  */
    cache   (PD|IST), 0x00(t0)      /* we have a dirty exclusive line., */
 # 
 # Prepare data in the pcache.
 #
    li      t1, 4                   /* 4 double in a line.              */
1:  not     t3, t2                  /* create the upper 32 bits.        */
    dsll32  t3, t3, 0               /* move in place.                   */
    or      t3, t3, t2              /* or with lower 32 bits.           */
    sd      t3, 0x0(t4)             /* write to cache line.             */
    addu    t2, 8                   /* next data pattern.               */
    sub     t1, 1                   /* descrment counter.               */
    bgtz    t1, 1b                  /* loop for 4 words.                */
    addu    t4, 0x8                 /* point to next.                   */
 #
 # Flush the cache line - this will caused a block write to SysAD.
 #
    move    v0, t0                  /* return the virtual index.        */
 #
 # And we done.
 #
    ld      AT,regsave-0x00(sp)     /* save $4                          */
    ld      ra,regsave-0x08(sp)     /* save $4                          */
    ld      t0,regsave-0x10(sp)     /* save $5                          */
    ld      t1,regsave-0x18(sp)     /* save $5                          */
    ld      t2,regsave-0x20(sp)     /* save $6                          */
    ld      t3,regsave-0x28(sp)     /* save $7                          */
    ld      t4,regsave-0x30(sp)     /* save $7                          */
    jr      ra                      /* return                           */
    addu    sp, framesize           /* restore stack pointer.           */
.end	line_in_cache

/*
 * -------------------------------------------------------------
 *  Simulate r4k model block write - break 3 instruction.
 *  a0 = starting address.
 * -------------------------------------------------------------
 */
.globl 	BlockWR  
.globl 	BlockInit
    .ent    BlockInit
BlockInit:
BlockWR:
    subu    sp, framesize           /* create current stack frame.      */
    sd      AT,regsave-0x00(sp)     /* save AT                          */
    sd      ra,regsave-0x08(sp)     /* save ra                          */
    sd      t0,regsave-0x10(sp)     /* save t0                          */
    sd      t1,regsave-0x18(sp)     /* save t1                          */
    sd      t2,regsave-0x20(sp)     /* save t2                          */
    sd      t3,regsave-0x28(sp)     /* save t3                          */
    sd      t4,regsave-0x30(sp)     /* save t4                          */
 #
 # Create testaddr in the cache, set 1.
 #  t0: cache line virtual index.
 #  t2: physical test address.
 #  t4: kseg0 test address.
 #
    mtc0    zero, TagHi             /* initialize the TagHi register.   */
    sll     t0, a0, 3               /* remove the upper 3 bit.          */
    srl     t2, t0, 15              /* move paddr<35:12> to P<23:0>     */
    sll     t2, 8                   /* then to PtagLo<31:8>             */
    ori     t2, 0x3 <<6             /* create a dirty exclusive line.   */
    mtc0    t2, TagLo               /* move to TagLo register.          */
    srl     t0, 3                   /* remove the upper 3 bits<31:29>   */
    move    t2, t0                  /* this is the physical test addr.  */
    lui     t1, kseg0 >>16          /* this is KSEG0 base.              */
    lwu     t3, PCACHESET(gp)       /* read pcache set size.            */
    or      t4, t0, t1              /* save KSEG0 test address.         */
    or      t0, t3, t4              /* this is the cache virtual index  */
    cache   (PD|IST), 0x00(t0)      /* we have a dirty exclusive line., */
 # 
 # Prepare data in the pcache.
 #
    li      t1, 4                   /* 4 double in a line.              */
1:  not     t3, t2                  /* create the upper 32 bits.        */
    dsll32  t3, t3, 0               /* move in place.                   */
    or      t3, t3, t2              /* or with lower 32 bits.           */
    sd      t3, 0x0(t4)             /* write to cache line.             */
    addu    t2, 8                   /* next data pattern.               */
    sub     t1, 1                   /* descrment counter.               */
    bgtz    t1, 1b                  /* loop for 4 words.                */
    addu    t4, 0x8                 /* point to next.                   */
 #
 # Flush the cache line - this will caused a block write to SysAD.
 #
    cache   (IWI|PD), 0x0(t0)       /* Flush the cache line.            */
    la      t1, CrimeBASE           /* load the mem base addr.          */
    ld      v0, EccCheckbits(t1)    /* read the correct check bits.     */
 #
 # And we done.
 #
    ld      AT,regsave-0x00(sp)     /* save $4                          */
    ld      ra,regsave-0x08(sp)     /* save $4                          */
    ld      t0,regsave-0x10(sp)     /* save $5                          */
    ld      t1,regsave-0x18(sp)     /* save $5                          */
    ld      t2,regsave-0x20(sp)     /* save $6                          */
    ld      t3,regsave-0x28(sp)     /* save $7                          */
    ld      t4,regsave-0x30(sp)     /* save $7                          */
    jr      ra                      /* return                           */
    addu    sp, framesize           /* restore stack pointer.           */
.end	BlockInit 	

/*
 *  Simulate r4k model block write - break 3 instruction.
 *          a0 = starting address.
 *          a1 = error pattern.
 *          a2 = number of dwords in fault.
 */
 
.globl 	BlockErr 
.ent   	BlockErr

BlockErr:
    subu    sp, framesize           /* create current stack frame.      */
    sd      AT,regsave-0x00(sp)     /* save $4                          */
    sd      ra,regsave-0x08(sp)     /* save $4                          */
    sd      t0,regsave-0x10(sp)     /* save $5                          */
    sd      t1,regsave-0x18(sp)     /* save $5                          */
    sd      t2,regsave-0x20(sp)     /* save $6                          */
    sd      t3,regsave-0x28(sp)     /* save $7                          */
    sd      t4,regsave-0x30(sp)     /* save $7                          */
    sd      t5,regsave-0x38(sp)     /* save $7                          */
    sd      t6,regsave-0x40(sp)     /* save $7                          */
 #
 # Create testaddr in the cache, set 1.
 #  t0: cache line virtual index.
 #  t2: physical test address.
 #  t4: kseg0 test address.
 #
    mtc0    zero, TagHi             /* initialize the TagHi register.   */
    sll     t0, a0, 3               /* remove the upper 3 bit.          */
    srl     t2, t0, 15              /* move paddr<35:12> to P<23:0>     */
    sll     t2, 8                   /* then to PtagLo<31:8>             */
    ori     t2, 0x3 <<6             /* create a dirty exclusive line.   */
    mtc0    t2, TagLo               /* move to TagLo register.          */
    srl     t0, 3                   /* remove the upper 3 bits<31:29>   */
    move    t2, t0                  /* this is the physical test addr.  */
    move    t5, a1                  /* save the bits.                   */
    move    t6, a2                  /* save the dwords.                 */
    lui     t1, kseg0 >>16          /* this is KSEG0 base.              */
    lwu     t3, PCACHESET(gp)       /* read pcache set size.            */
    or      t4, t0, t1              /* save KSEG0 test address.         */
    or      t0, t3, t4              /* this is the cache virtual index  */
    cache   (PD|IST), 0x00(t0)      /* we have a dirty exclusive line., */
 # 
 # Prepare data in the pcache.
 #
    li      t1, 4                   /* 4 double in a line.              */
1:  not     t3, t2                  /* create the upper 32 bits.        */
    dsll32  t3, t3, 0               /* move in place.                   */
    or      t3, t3, t2              /* or with lower 32 bits.           */
    blez    t6, 2f                  /* how many double word at fault.   */
    sub     t6, 1                   /* decrement the counter.           */
    xor     t3, t3, t5              /* create the error data pattern.   */
2:  sd      t3, 0x0(t4)             /* write to cache line.             */
    addu    t2, 8                   /* next data pattern.               */
    sub     t1, 1                   /* descrment counter.               */
    bgtz    t1, 1b                  /* loop for 4 words.                */
    addu    t4, 0x8                 /* point to next.                   */
 #
 # Flush the cache line - this will caused a block write to SysAD.
 #
    cache   (IWI|PD), 0x0(t0)       /* Flush the cache line.            */
    la      t1, CrimeBASE           /* load the mem base addr.          */
    ld      v0, EccCheckbits(t1)    /* return the correct check bits.   */
 #
 # And we done.
 #
    ld      AT,regsave-0x00(sp)     /* save $4                          */
    ld      ra,regsave-0x08(sp)     /* save $4                          */
    ld      t0,regsave-0x10(sp)     /* save $5                          */
    ld      t1,regsave-0x18(sp)     /* save $5                          */
    ld      t2,regsave-0x20(sp)     /* save $6                          */
    ld      t3,regsave-0x28(sp)     /* save $7                          */
    ld      t4,regsave-0x30(sp)     /* save $7                          */
    ld      t5,regsave-0x38(sp)     /* save $7                          */
    ld      t6,regsave-0x40(sp)     /* save $7                          */
    jr      ra                      /* return                           */
    addu    sp, framesize           /* delete current stack frame.      */
	
.end	BlockErr

/*
 * -------------------------------------------------------------
 *  Simulate r4k model block read
 *          a0 = starting address.
 *          a1 = number of bits at fault.
 *          a2 = fault data bits.     
 * -------------------------------------------------------------
 */
 
.globl 	BlockRD  
.ent   	BlockRD 
BlockRD:
    subu    sp, framesize           /* create current stack frame..     */
    sd      AT,regsave-0x00(sp)     /* save $4                          */
    sd      ra,regsave-0x08(sp)     /* save $4                          */
    sd      t0,regsave-0x10(sp)     /* save $5                          */
    sd      t1,regsave-0x18(sp)     /* save $5                          */
    sd      t2,regsave-0x20(sp)     /* save $6                          */
    sd      t3,regsave-0x28(sp)     /* save $7                          */
    sd      t4,regsave-0x30(sp)     /* save $7                          */
    sd      t5,regsave-0x38(sp)     /* save $7                          */
    sd      t6,regsave-0x40(sp)     /* save $7                          */
    sd      s0,regsave-0x48(sp)     /* save $7                          */
    sd      s1,regsave-0x50(sp)     /* save $7                          */
    sd      s2,regsave-0x58(sp)     /* save $7                          */
    sd      s3,regsave-0x60(sp)     /* save $7                          */
 #
 # Create testaddr in the cache, set 1.
 #
    mtc0    zero, TagHi             /* initialize the TagHi register.   */
    sll     t0, a0, 3               /* remove the upper 3 bit.          */
    srl     t2, t0, 15              /* move paddr<35:12> to P<23:0>     */
    sll     t2, 8                   /* then to PtagLo<31:8>             */
    mtc0    t2, TagLo               /* move to TagLo register.          */
    srl     t0, 3                   /* remove the upper 3 bits<31:29>   */
    move    t2, t0                  /* this is the test address.        */
    lui     t1, kseg0 >>16          /* this is KSEG0 base.              */
    lwu     s3, PCACHESET(gp)       /* read pcache set size.            */
    or      t0, t0, t1              /* we have cacheable test address.  */
    or      s3, s3, t0              /* this is the cache virtual index  */
    cache   (PD|IST), 0x00(s3)      /* make sure line is invalid.       */
 #
 # Save input parameters.
 #
    move    s0, a0                  /* save a0.                         */
    move    s1, a1                  /* save a1.                         */
    move    s2, a2                  /* save a2.                         */
 #
 # A primary cache miss caused a block read in the SysAD, then check the 
 # expected data that returned by Crime.
 #
    ld      t3, 0x0(t0)             /* a cache miss.                    */
    not     t4, t2                  /* create data pattern.             */
    dsll32  t4, t4, 0               /* move to upper 32 bit.            */
    or      t4, t4, t2              /* we have data pattern             */
    sub     t5, s1, 1               /* check what kind of fault.        */
    blez    t5, 2f                  /* its single bits ecc errors.      */
    nop                             /* branch delay slot.               */
    xor     t4, t4, s2              /* create faulty data pattern.      */
2:  beq     t3, t4, 3f              /* is it what we expected ??        */
    li      t5, 3                   /* 3 double remain to be tested     */
    bal     9f                      /* end of the world.                */
    nop                             /* branch delay slot.               */
3:  addu    t2, t2, 8               /* create next data pattern.        */
7:  addu    t0, t0, 8               /* point to next dword.             */
    ld      t3, 0x0(t0)             /* a cache hit.                     */
    not     t4, t2                  /* create data pattern.             */
    dsll32  t4, t4, 0               /* move to upper 32 bit.            */
    or      t4, t4, t2              /* we have 64 bit data pattern.     */
    beq     t3, t4, 4f              /* error happened.                  */
    sub     t5, 1                   /* decrement the counter.           */
    bal     9f                      /* end of the world.                */
    nop                             /* delay slot.                      */
4:  bgtz    t5, 7b                  /* loopback for next double word.   */
    addu    t2, 8                   /* next data pattern.               */
 #  
 # We are done with the block read.
 #
    cache   (PD|IST), 0x00(s3)      /* make the line invalid again.     */
    ld      AT,regsave-0x00(sp)     /* save $4                          */
    ld      ra,regsave-0x08(sp)     /* save $4                          */
    ld      t0,regsave-0x10(sp)     /* save $5                          */
    ld      t1,regsave-0x18(sp)     /* save $5                          */
    ld      t2,regsave-0x20(sp)     /* save $6                          */
    ld      t3,regsave-0x28(sp)     /* save $7                          */
    ld      t4,regsave-0x30(sp)     /* save $7                          */
    ld      t5,regsave-0x38(sp)     /* save $7                          */
    ld      t6,regsave-0x40(sp)     /* save $7                          */
    ld      s0,regsave-0x48(sp)     /* save $7                          */
    ld      s1,regsave-0x50(sp)     /* save $7                          */
    ld      s2,regsave-0x58(sp)     /* save $7                          */
    ld      s3,regsave-0x60(sp)     /* save $7                          */
    jr      ra                      /* return                           */
    addu    sp, framesize           /* restore stack pointer.           */
 #
 # We see an error during the block read cmd.
 #
9:  sd      t3, localargs-0x00(sp)  /* save the observed data pattern.  */
    sd      t4, localargs-0x08(sp)  /* save the expected data pattern.  */
    move    a3, ra                  /* the instruction where error were */
    move    a2, t0                  /* the test address.                */
    addu    a1, sp, localargs       /* the addr of observed data.       */
    j       BlockRD_error           /* call BlockRD error routine.      */
    addu    a0, sp, localargs-0x08  /* the addr of expected data.       */
                                    /* the caller provided a rtn addr.  */
.end        BlockRD 

/*
 * -------------------------------------------------------------
 * advance EPC to next instruction.
 * -------------------------------------------------------------
 */
.globl	    advance_epc
.ent	    advance_epc
advance_epc:
    .set    noat
1:  move    a1, AT                  /* save the dame at register.       */
    .set    at
    mfc0    a0, EPC                 /* read the epc.                    */
    nop	                            /* wait for 4 cpu cycles.           */
    nop	                            /* wait for 4 cpu cycles.           */
    nop	                            /* wait for 4 cpu cycles.           */
    nop	                            /* wait for 4 cpu cycles.           */
    addu    a0, 4                   /* point to next interuction.       */
    mtc0    a0, EPC                 /* put it back to epc.              */
    .set    noat
    move    AT, a1                  /* and restore the at register.     */
    .set    at
    nop	                            /* wait for 4 cpu cycles.           */
    nop	                            /* wait for 4 cpu cycles.           */
    jr	    ra                      /* return.                          */
    nop	                            /* wait for 4 cpu cycles.           */
.end	advance_epc   
    
/*
 * -------------------------------------------------------------
 * advance Error EPC to next instruction.
 * -------------------------------------------------------------
 */
.globl 	advance_error_epc
.ent   	advance_error_epc
advance_error_epc:
    .set    noat
1:  move    a1, AT                  /* save the dame at register.       */
    .set    at
    mfc0    a0, ErrorEPC            /* read the error_epc.              */
    nop	                            /* wait for 4 cpu cycles.           */
    nop	                            /* wait for 4 cpu cycles.           */
    nop	                            /* wait for 4 cpu cycles.           */
    nop	                            /* wait for 4 cpu cycles.           */
    addu    a0, 4                   /* point to next interuction.       */
    mtc0    a0, ErrorEPC            /* put it back to erorr_epc.        */
    .set    noat
    move    AT, a1                  /* and restore the at register.     */
    .set    at
    nop	                            /* wait for 4 cpu cycles.           */
    nop	                            /* wait for 4 cpu cycles.           */
    jr	    ra                      /* return.                          */
    nop	                            /* wait for 4 cpu cycles.           */
.end	advance_error_epc   
    
    
/*
 * -------------------------------------------------------------
 * Save current stack pointer for feather interrupt.
 * -------------------------------------------------------------
 */
.globl 	save_sp 
.ent   	save_sp 
save_sp:
    jr	    ra                      /* return to caller                 */
    sw	    sp, SAVED_SP(gp)        /* save current sp                  */
.end	save_sp 
    
/*
 * -------------------------------------------------------------
 * Restore previous saved stack pointer.
 * -------------------------------------------------------------
 */
.globl 	restore_sp 
.ent   	restore_sp 
restore_sp:
    lw	    sp, SAVED_SP(gp)        /* save current sp                  */
    jr	    ra                      /* return to caller                 */
    nop	                            /* branch delay slot.               */
.end	restore_sp 

/*
 * -------------------------------------------------------------
 * read the cause register.
 * -------------------------------------------------------------
 */
.globl 	rd_cause
.ent   	rd_cause
rd_cause:
    mfc0    v0, Cause               /* read the epc.                    */
    nop	                            /* one                              */
    nop	                            /* two                              */
    nop	                            /* three                            */
    jr	    ra                      /* four.                            */
    nop	                            /* four and half.                   */
.end	rd_cause
    
/*
 * -------------------------------------------------------------
 * Turn on  bev bit.
 * -------------------------------------------------------------
 */
.globl 	bevON 
.ent   	bevON 
bevON:
    mfc0    a0, Status              /* read the epc.                    */
    nop	                            /* one                              */
    nop	                            /* two                              */
    nop	                            /* three                            */
    lui     a1, 0x40                /* bit 22 (BEV bit)                 */
    or      a0, a0, a1              /* turn it off.                     */
    mtc0    a0, Status              /* and write it right back.         */
    nop	                            /* one                              */
    nop	                            /* two                              */
    nop	                            /* three                            */
    jr	    ra                      /* four.                            */
    nop	                            /* four and half.                   */
.end	bevON
    
/*
 * -------------------------------------------------------------
 * Turn off bev bit.
 * -------------------------------------------------------------
 */
.globl 	bevOFF
.ent   	bevOFF
bevOFF:
    mfc0    a0, Status              /* read the epc.                    */
    nop	                            /* one                              */
    nop	                            /* two                              */
    nop	                            /* three                            */
    lui     a1, 0x40                /* bit 22 (BEV bit)                 */
    not     a1                      /* invert the bit mask.             */
    and     a0, a0, a1              /* turn it off.                     */
    mtc0    a0, Status              /* and write it right back.         */
    nop	                            /* one                              */
    nop	                            /* two                              */
    nop	                            /* three                            */
    jr	    ra                      /* four.                            */
    nop	                            /* four and half.                   */
.end	bevOFF
    
/*
 * -------------------------------------------------------------
 * write the status register.
 * -------------------------------------------------------------
 */
.globl 	wr_status
.ent   	wr_status
wr_status:
    mtc0    a0, Status              /* read the epc.                    */
    nop	                            /* one                              */
    nop	                            /* two                              */
    nop	                            /* three                            */
    jr	    ra                      /* four.                            */
    nop	                            /* four and half.                   */
.end	wr_status
    

/*
 * -------------------------------------------------------------
 * read the status register.
 * -------------------------------------------------------------
 */
.globl 	rd_status
.ent   	rd_status
rd_status:
    mfc0    v0, Status              /* read the epc.                    */
    nop	                            /* one                              */
    nop	                            /* two                              */
    nop	                            /* three                            */
    jr	    ra                      /* four.                            */
    nop	                            /* four and half.                   */
.end	rd_cause
    

/*
 * -------------------------------------------------------------
 * write to EntryHi register.
 * -------------------------------------------------------------
 */
.globl 	wr_entryhi
.ent   	wr_entryhi
wr_entryhi:
    mtc0    a0, EntryHi             /* write to entry hi.               */
    jr	    ra                      /* return to caller.                */
    nop	                            /* four and half.                   */
.end	wr_entryhi    


/*
 * -------------------------------------------------------------
 * write to EntryLo registers.
 * -------------------------------------------------------------
 */
.globl 	wr_entrylo0
.ent   	wr_entrylo0
wr_entrylo0:
    mtc0    a0, EntryLo0            /* write to entry hi.               */
    jr	    ra                      /* return to caller.                */
    nop	                            /* four and half.                   */
.end	wr_entrylo0    
 
.globl 	wr_entrylo1
.ent   	wr_entrylo1
wr_entrylo1:
    mtc0    a0, EntryLo1            /* write to entry hi.               */
    jr	    ra                      /* return to caller.                */
    nop	                            /* four and half.                   */
.end	wr_entrylo1    


/*
 * -------------------------------------------------------------
 * write to pagemask register.
 * -------------------------------------------------------------
 */
.globl 	wr_pagemask
.ent   	wr_pagemask
wr_pagemask:
    mtc0    a0, PageMask            /* write to entry hi.               */
    jr	    ra                      /* return to caller.                */
    nop	                            /* four and half.                   */
.end	wr_pagemask    
    
 
/*
 * -------------------------------------------------------------
 * write to pagemask register.
 * -------------------------------------------------------------
 */
.globl 	wr_index
.ent   	wr_index
wr_index:
    mtc0    a0, Index               /* write to entry hi.               */
    jr	    ra                      /* return to caller.                */
    nop	                            /* four and half.                   */
.end	wr_index


/*
 * -------------------------------------------------------------
 * TLB write index.
 * -------------------------------------------------------------
 */
.globl 	tlbwi
.ent   	tlbwi
tlbwi:
    tlbwi                           /* write tlbv index.                */
    jr	    ra                      /* return to caller.                */
    nop	                            /* four and half.                   */
.end	tlbwi

/*
 * -------------------------------------------------------------
 * Read the BadVaar register.
 * -------------------------------------------------------------
 */
.globl 	rd_badva
.ent   	rd_badva
rd_badva:
    mfc0    v0, BadVAddr            /* read the epc.                    */
    nop	                            /* one                              */
    nop	                            /* two                              */
    nop	                            /* three                            */
    jr	    ra                      /* four.                            */
    nop	                            /* four and half.                   */
.end	rd_badva

/*
 * -------------------------------------------------------------
 * Use count register count to count 2m times
 * -------------------------------------------------------------
 */
.globl	    count2m
.ent	    count2m
count2m:
    subu    sp, framesize           /* save 10 registers.               */
    sd      t0,regsave-0x10(sp)     /* save t0                          */
    sd      ra,regsave-0x18(sp)     /* save return address.             */
    sd      AT,regsave-0x20(sp)     /* save at register.                */
    sd      t1,regsave-0x28(sp)     /* save t0                          */
    sd      t2,regsave-0x30(sp)     /* save t0                          */
    
    mfc0    t0, Count               /* read the count register.         */
    nop                             /* one cp0 delay slot.              */
    nop                             /* one cp0 delay slot.              */
    nop                             /* one cp0 delay slot.              */
    nop                             /* one cp0 delay slot.              */
    mtc0    zero, Count             /* zap the count.                   */
    nop                             /* one cp0 delay slot.              */
    nop                             /* one cp0 delay slot.              */
    nop                             /* one cp0 delay slot.              */
    lui     t2, 0x1f                /* 2m instructions.                 */
1:  mfc0    t1, Count               /* zap the count.                   */
    nop                             /* one cp0 delay slot.              */
    nop                             /* one cp0 delay slot.              */
    nop                             /* one cp0 delay slot.              */
    nop                             /* one cp0 delay slot.              */
    blt     t1, t2, 1b              /* loop for 2 m instructions        */
    nop
    
    mtc0    t0, Count               /* restore count register.          */
    ld      t0,regsave-0x10(sp)     /* save t0                          */
    ld      ra,regsave-0x18(sp)     /* save return address.             */
    ld      AT,regsave-0x20(sp)     /* save at register.                */
    ld      t1,regsave-0x28(sp)     /* save t0                          */
    ld      t2,regsave-0x30(sp)     /* save t0                          */
    jr      ra
    addu    sp, framesize           /* save 10 registers.               */
    
.end        count2m    
    
/*
 * -------------------------------------------------------------
 * Read in the fmt string of the printf on a 32bit basis 
 * -------------------------------------------------------------
 */
.globl	    reformat
.ent	    reformat 
reformat:
    subu    sp, 30*8                /* save 10 registers.               */
    sd	    AT, 0x10(sp)            /* save the t0 register.            */
    sd	    ra, 0x18(sp)            /* save the t0 register.            */
    sd	    t0, 0x20(sp)            /* save the t0 register.            */
    sd	    t1, 0x28(sp)            /* save the t1 register.            */
    sd	    t2, 0x30(sp)            /* save the t2 register.            */
    sd	    t3, 0x38(sp)            /* save the t2 register.            */
    
    move    t0, a0                  /* save the fmt address.            */
    
    move    t2, gp                  /* the string pool in the stack     */
    move    v0, gp                  /* return the beginning of the str. */
1:  lwu	    t1, 0x0(t0)             /* read the first word              */
    addu    t0, 4                   /* point top next word.             */
    sw	    t1, 0x0(t2)             /* write it to cache.               */
    bnez    t0, 1b                  /* a zero word			*/
    addu    t2, 4                   /* point to next word in cache.     */
    
    ld	    AT, 0x10(sp)            /* save the t0 register.            */
    ld	    ra, 0x18(sp)            /* save the t0 register.            */
    ld	    t0, 0x20(sp)            /* save the t0 register.            */
    ld	    t1, 0x28(sp)            /* save the t1 register.            */
    ld	    t2, 0x30(sp)            /* save the t2 register.            */
    ld	    t3, 0x38(sp)            /* save the t2 register.            */
    jr	    ra                      /* return                           */
    subu    sp, 30*8                /* save 10 registers.               */
    
.end	    reformat

/*
 * -------------------------------------------------------------
 * Turn on the ENLOW address space.
 * -------------------------------------------------------------
 */
.globl	    enablelow
.ent	    enablelow
enablelow:
    lwu     a0, LowPROMFlag(gp)     /* Load the prom switch flag.       */
    lui	    a1, ENLOW>>16           /* cleared before we proceed.       */
    beqz    a0, 1f                  /* Low prom address space was off   */
    li      v0, 1                   /* Go turn it on                    */
    jr      ra                      /* No action is required.           */
    move    v0, zero                /* and indicate it in return value  */
1:  sb	    a1, 0x0(a1)             /* Turn on ENLOW space.             */
    jr	    ra                      /* return.                          */
    sw      v0, LowPROMFlag(gp)     /* and also turn on the flag        */
    
.end	    enablelow

/*
 * -------------------------------------------------------------
 * Turn off the ENLOW address space.
 * -------------------------------------------------------------
 */
.globl	    disablelow
.ent	    disablelow
disablelow:
    lwu     a0, LowPROMFlag(gp)     /* Load the prom switch flag.       */
    lui	    a1, ENLOW>>16           /* cleared before we proceed.       */
    bnez    a0, 1f                  /* Low prom address space was on    */
    li      v0, 1                   /* Go turn it off                   */
    jr      ra                      /* No action is required.           */
    move    v0, zero                /* and indicate it in return value  */
1:  sb	    a1, 0x0(a1)             /* Turn on ENLOW space.             */
    jr	    ra                      /* return.                          */
    sw      zero, LowPROMFlag(gp)   /* and also turn off the flag.      */
    
.end	    disablelow

/*
 * -------------------------------------------------------------
 * PROM callback vector test.
 * -------------------------------------------------------------
 */
.globl	    _PROMcallback
.ent	    _PROMcallback
_PROMcallback:
    mtc0    a0, LLAddr              /* Saved the CallBackFunctioncode.  */
    jal     cache_invalidate        /* invalidate all primary cache     */
    nop                             /* entries.                         */
    jal     tlb_init                /* invalidate all tlb entries.      */
    nop                             /* branch delay slot.               */
    mfc0    a1, Status              /* load current status register.    */
    li      a0, ~0x1b               /* setup IE,EXL,and KSU bits mask.  */
    and     a1, a0, a1              /* clear IE,EXL,and KSU bits.       */
    lui     a0, (0x400000>>16)      /* set the BEV bit mask.            */
    not     a0                      /* invalidate the mask.             */
    and     a1, a0, a1              /* turn off the BEV bit.            */
    mtc0    a1, Status              /* setup hardreset status.          */
    lui     t1, ENLOW>>16           /* load the PROM/FLASH switch.      */
    sd      t1, 0x0(t1)             /* turn on the FLASH access.        */
    mfc0    a0, LLAddr              /* restore the FunctionCode.        */
    lui     t0, FLASH_ALIAS>>16     /* load the hard reset address.     */
    jr      t0                      /* branch to reset vector.          */
    nop                             /* two nops                         */
1:  b       1b                      /* die here                         */
    nop                             /* padding.                         */

.end	    _jump2Flash

/*
 * -------------------------------------------------------------
 * Double word read, a0 will have the address.
 * -------------------------------------------------------------
 */
.globl	    _jump2Flash
.ent	    _jump2Flash
_jump2Flash:
    jal     cache_invalidate        /* invalidate all primary cache     */
    nop                             /* entries.                         */
    jal     tlb_init                /* invalidate all tlb entries.      */
    nop                             /* branch delay slot.               */
    la      a0, _jump2Flash         /* save the entry address.          */
    mtc0    a0, LLAddr              /* also set LLAddr to _jump2Flash.  */
    nop                             /* a nop is required                */
    mfc0    a1, Status              /* load current status register.    */
    li      a0, ~0x1b               /* setup IE,EXL,and KSU bits mask.  */
    and     a1, a0, a1              /* clear IE,EXL,and KSU bits.       */
    lui     a0, (0x400000>>16)      /* set the BEV bit mask.            */
    or      a1, a0, a1              /* turn on BEV bit.                 */
    mtc0    a1, Status              /* setup hardreset status.          */
    lui     t1, ENLOW>>16           /* load the PROM/FLASH switch.      */
    sd      t1, 0x0(t1)             /* turn on the FLASH access.        */
    lui     t0, FLASH_ALIAS>>16     /* load the hard reset address.     */
    jr      t0                      /* branch to reset vector.          */
    nop                             /* two nops                         */
1:  b       1b                      /* die here                         */
    nop                             /* padding.                         */

.end	    _jump2Flash

/*
 * -------------------------------------------------------------
 * Double word read, a0 will have the address.
 * -------------------------------------------------------------
 */
.globl	    _fwCBtst
.ent	    _fwCBtst
_fwCBtst:
    mtc0    a0, LLAddr              /* save the callback function code. */
    nop	                            /* a nop is required                */
    jal     cache_invalidate        /* invalidate all primary cache     */
    nop                             /* entries.                         */
    jal     tlb_init                /* invalidate all tlb entries.      */
    nop                             /* branch delay slot.               */
    mfc0    a1, Status              /* load current status register.    */
    li      a0, ~0x1b               /* setup IE,EXL,and KSU bits mask.  */
    and     a1, a0, a1              /* clear IE,EXL,and KSU bits.       */
    lui     a0, (0x400000>>16)      /* set the BEV bit mask.            */
    not     a0                      /* turn it off.                     */
    and     a1, a0, a1              /* turn off BEV bit.                */
    mtc0    a1, Status              /* setup hardreset status.          */
    lui     a1, ENLOW>>16           /* load the PROM/FLASH switch.      */
    sd      a1, 0x0(a1)             /* turn on the FLASH access.        */
    mfc0    a0, LLAddr              /* restore FW callback function code*/
    lui     t0, FLASH_ALIAS>>16     /* load the hard reset address.     */
    jr      t0                      /* branch to reset vector.          */
    nop                             /* two nops                         */
1:  b       1b                      /* die here                         */
    nop                             /* padding.                         */
    
.end	    _fwCBtst

/*
 * -------------------------------------------------------------
 * Double word read, a0 will have the address.
 * -------------------------------------------------------------
 */
.globl	    _fwTVtst
.ent	    _fwTVtst
_fwTVtst:
    mtc0    a0, LLAddr              /* save the callback address  code. */
    nop	                            /* a nop is required                */
    jal     cache_invalidate        /* invalidate all primary cache     */
    nop                             /* entries.                         */
    jal     tlb_init                /* invalidate all tlb entries.      */
    nop                             /* branch delay slot.               */
    mfc0    a1, Status              /* load current status register.    */
    li      a0, ~0x1b               /* setup IE,EXL,and KSU bits mask.  */
    and     a1, a0, a1              /* clear IE,EXL,and KSU bits.       */
    lui     a0, (0x400000>>16)      /* set the BEV bit mask.            */
    not     a0                      /* turn it off.                     */
    and     a1, a0, a1              /* turn off BEV bit.                */
    mtc0    a1, Status              /* setup hardreset status.          */
    lui     a1, ENLOW>>16           /* load the PROM/FLASH switch.      */
    sd      a1, 0x0(a1)             /* turn on the FLASH access.        */
    mfc0    a0, LLAddr              /* restore FW callback function code*/
    nop                             /* wait for a while                 */
    nop                             /* wait for a while                 */
    nop                             /* wait for a while                 */
    nop                             /* wait for a while                 */
    jr      a0                      /* branch to reset vector.          */
    nop                             /* two nops                         */
1:  b       1b                      /* die here                         */
    nop                             /* padding.                         */
    
.end	    _fwTVtst

/*
 * -------------------------------------------------------------
 * Compare two 64 bit registers.
 * -------------------------------------------------------------
 */
.globl	    cmp32
.ent	    cmp32
cmp32:
    subu    sp, framesize           /* decrement stack pointer.         */
    sb	    t0, regsave-0x08(sp)    /* save t0                          */
    sb	    t1, regsave-0x10(sp)    /* save t1                          */
    dsll32  t0, a0, 0               /* remove the upper 32 bits.        */
    dsrl32  t0, 0                   /* move back to original position   */
    dsll32  t1, a1, 0               /* remove the upper 32 bits.        */
    dsrl32  t1, 0                   /* move back to original position   */
    bne	    t0, t1, 1f              /* compare the address.             */
    move    v0, zero                /* return error status.             */
    li	    v0, 1                   /* return ture condition.l          */
1:  ld	    t0, regsave-0x08(sp)    /* restore t0.                      */
    ld	    t1, regsave-0x10(sp)    /* restore t1.                      */
    j	    ra                      /* return                           */
    addu    sp, framesize           /* delete current stack frame.      */
.end	    cmp32    


/*
 * -------------------------------------------------------------
 * Double word read, a0 will have the address.
 * -------------------------------------------------------------
 */
.globl	    ld64
.ent	    ld64
ld64:
    ld	    v0, 0x0(a0)             /* doubleword read.                 */
    dsll32  v1, v0, 0
    dsrl32  v1, v1, 0               /* get the 0x0 data.                */
    jr	    ra
    dsrl32  v0, v0, 0
    
.end	    ld64

/*
 * -------------------------------------------------------------
 * Double word write a2 will have the address, a0 hold 0x0 data 
 *                                             a1 hold 0x4 data 
 * -------------------------------------------------------------
 */
.globl	    sd64
.ent	    sd64
sd64:
    dsll32  a0, a0, 0
    dsll32  a1, a1, 0
    dsrl32  a1, a1, 0
    or	    a0, a0, a1
    j	    ra
    sd	    a0, 0x0(a2) 
    
.end	    sd64

/*
 * -------------------------------------------------------------
#
#  $Log: asm_support.s,v $
#  Revision 1.1  1997/08/18 20:41:56  philw
#  updated file from bonsai/patch2039 tree
#
# Revision 1.8  1996/10/31  21:51:43  kuang
# Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
#
# Revision 1.8  1996/10/04  20:09:05  kuang
# Fixed some general problem in the diagmenu area
#
# Revision 1.7  1996/04/12  02:19:59  kuang
# Added line_in_cache function to support ECC diags
#
# Revision 1.6  1996/04/04  23:45:37  kuang
# Fixed the An if directive is not terminated properly problem
#
# Revision 1.5  1996/04/04  23:31:26  kuang
# Removed diag_normal_exit and diag_error_exit routines
#
# Revision 1.4  1996/04/04  23:17:22  kuang
# Added more diagnostic support and some general cleanup
#
# Revision 1.3  1996/03/25  22:21:20  kuang
# Added power off option to the main menu
#
# Revision 1.2  1995/12/30  03:27:51  kuang
# First moosehead lab bringup checkin, corresponding to 12-29-95 d15
#
# Revision 1.1  1995/11/15  00:42:41  kuang
# initial checkin
#
# Revision 1.2  1995/11/14  23:04:12  kuang
# Final cleanup for ptool checkin
#
 * -------------------------------------------------------------
 */

/* END OF FILE */
