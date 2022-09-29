/*
 * IP22 specific assembly routines; cpuid always 0, also make semaphore
 * macros a no-op.
 */
#ident "$Revision: 1.1 $"

#include <regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/crime.h>
#include <sys/mace.h>

/*	dummy routines whose return value is unimportant (or no return value).
	Some return reasonable values on other machines, but should never
	be called, or the return value should never be used on other machines.
*/
LEAF(read_reg64)
	ld	a0,0(a0)
	dsrl32	v0,a0,0
	dsll32	a0,0
	.set noreorder
	j	ra
	dsrl32	v1,a0,0
	.set reorder
	END(read_reg64)

LEAF(write_reg64)
	dsll32	a1,0
	dsrl32	a1,0
	dsll32	a0,0
	or	a0,a1
	.set noreorder
	j	ra
	sd	a0,0(a2)
	.set reorder
	END(write_reg64)

LEAF(get_crm_rev)
	ld	v0,CRM_ID|K1BASE
	.set noreorder
	j	ra
	and	v0,CRM_ID_REV
	.set reorder
	END(get_crm_rev)

/**********
 * flushbus
 *---------
 * flush the write buffer in 
 * CRIME and MACE
 */
LEAF(wbflush)
XLEAF(flushbus)
    la      a0, PHYS_TO_K1(CRM_BASEADDR) /* Clear the crime writebuf    */
    ld      zero, 0x8(a0)           /* read the crime control register. */
    ld      zero, 0x200(a0)         /* read the memory control register */
    la      a0, PHYS_TO_K1(MACE_ISA) /* Clear the ISA bus.              */
    ld      zero, 0x0008(a0)        /* read the ISA NIC FLASH register. */
    jr      ra                      /* done!                            */
    END     (wbflush)
     
/*****************
 * Read_C0_LLaddr
 *----------------
 * Read the C0 load and link register.
 */
    .set    noreorder
    .globl  Read_C0_LLADDR
    .ent    Read_C0_LLADDR
Read_C0_LLADDR:
    mfc0    v0, C0_LLADDR
    nop
    nop
    jr	    ra
    nop
    .end    Read_C0_LLADDR
    .set    reorder
    
/*****************
 * Read_C0_Config
 *----------------
 * Read the C0 config register.
 */
    .set    noreorder
    .globl  Read_C0_Config
    .ent    Read_C0_Config
Read_C0_Config:
    mfc0    v0, C0_CONFIG
    jr	    ra
    nop
    .end    Read_C0_Config
    .set    reorder
    
    
/******************
 * Write_C0_Config
 *-----------------
 * Read the C0 config register.
 */
    .set    noreorder
    .globl  Write_C0_Config
    .ent    Write_C0_Config
Write_C0_Config:
    nop
    mtc0    a0, C0_CONFIG
    nop
    nop
    jr	    ra
    nop
    .end    Write_C0_Config
    .set    reorder
    
    
/**************    
 * Read_C0_PRId  
 *-------------    
 * Read the C0 PRId register.
 */
    .set    noreorder
    .globl  Read_C0_PRId
    .ent    Read_C0_PRId
Read_C0_PRId:
    mfc0    v0, C0_PRID
    jr	    ra
    nop
    .end    Read_C0_PRId
    .set    reorder

/***********
 * softreset
 *----------
 * Write crime control register to cause a system
 * wide softreset.
 */
LEAF(crm_softreset)
    la      a0, PHYS_TO_K1(CRM_CONTROL) /* loadup crime control addr.   */
    li      a1, CRM_CONTROL_SOFT_RESET  /* do a softreset.              */
    sd      a1, (a0)                /* write to crime control register. */
    bal     crm_deadloop            /* loop here for good.              */
    b       crm_softreset           /* we can not reset the system.     */
    END     (crm_softreset)
     
/***********
 * hardreset
 *----------
 * Write crime control register to cause a system
 * wide hardreset.
 */
LEAF(crm_hardreset)
    la      a0, PHYS_TO_K1(CRM_CONTROL) /* loadup crime control addr.   */
    li      a1, CRM_CONTROL_HARD_RESET  /* do a hardreset.              */
    sd      a1, (a0)                /* write to crime control register. */
    bal     crm_deadloop            /* loop here for good.              */
    b       crm_hardreset           /* we can not reset the system.     */
    END     (crm_hardreset)
	
/***********
 * _deadloop
 *----------
 * Toggle LEDs and return.
 */
LEAF(crm_deadloop)
    la      a0, PHYS_TO_K1(ISA_FLASH_NIC_REG) /* loadup NIC/LED addr    */
    ld      a1, (a0)                      /* read crime orig LEDs       */
    xori    a1, ISA_LED_RED|ISA_LED_GREEN /* toggle LEDS                */
    sd      a1, (a0)                /* write it back.                   */
    lui     a2, 0x0008              /* loop here for a while.           */
    dsll32  a2, 0                   /* create a big number.             */
    .set    noreorder    
1:  bgtz    a2, 1b                  /* a tight loop.                    */
    sub     a2, 1                   /* decrement the wait count.        */
    .set    reorder
    jr      ra                      /* return                           */
    END     (crm_deadloop)

/********
 * led_on
 *-------
 * Turn on leds.
 */
    .globl  led_on
    .ent    led_on
led_on:
    subu    sp, 0x40
    sd      AT, 0x38(sp)
    sd      t0, 0x30(sp)
    sd      t1, 0x28(sp)
    
    la      t0, PHYS_TO_K1(ISA_FLASH_NIC_REG) 
    ld      t1, 0x0(t0)
#if 0
    andi    a0, ISA_LED_RED|ISA_LED_GREEN
    ori     t1, ISA_LED_RED|ISA_LED_GREEN
    xor     t1, a0
    sd      t1, 0x0(t0)
#else
    andi    a0, ISA_LED_RED|ISA_LED_GREEN
    ori     t1, ISA_LED_RED|ISA_LED_GREEN
    not     a0
    and     t1, a0
    sd      t1, 0x0(t0)
#endif
    
    ld      AT, 0x38(sp)
    ld      t0, 0x30(sp)
    ld      t1, 0x28(sp)
    jr      ra
    addiu   sp, 0x40
    .end    led_on

/*********
 * led_off
 *--------
 * Turn off leds
 */
    .globl  led_off
    .ent    led_off
led_off:
    subu    sp, 0x40
    sd      AT, 0x38(sp)
    sd      t0, 0x30(sp)
    sd      t1, 0x28(sp)

    la      t0, PHYS_TO_K1(ISA_FLASH_NIC_REG) 
    ld      t1, 0x0(t0)
    or      t1, t1, a0
    sd      t1, 0x0(t0)
    
    ld      AT, 0x38(sp)
    ld      t0, 0x30(sp)
    ld      t1, 0x28(sp)
    jr      ra
    addiu   sp, 0x40
    .end    led_off


/*
 * this value is chosen because the crime counter has an input
 * frequency of 66Mhz while the 8254 on IP22 has an input frequency
 * of 1Mhz.
 */
#define COUNT (0x164 * 66)
#define TRIAL_COUNT 4
	
/******************
 * ticksper1024inst
 *-----------------
 * returns number of CRM_TIME ticks in the
 * time it takes to execute 1024 instructions.
 *-----------------
 * register usage:
 *	v0 -- number of ticks
 *	t0 -- Address of CRM_TIME register
 *	t1 -- Count of instructions to execute
 *	t2 -- Starting number of ticks
 *	t3 -- scratch
 */	
    .globl  ticksper1024inst
    .ent    ticksper1024inst
ticksper1024inst:
    la      t0, CRM_TIME|K1BASE
    la      t3, 1f
 #
 # prime the cache
 #
#if 0
    .set    noreorder
    cache   CACH_PI|C_FILL, 0(t3)
    cache   CACH_PI|C_FILL, 32(t3)
    cache   CACH_PI|C_FILL, 64(t3)
    .set    reorder
#endif
    
1:  li      t1, 512
    ld      t2, 0(t0)
    
    .set    noreorder
2:  bgt     t1, zero, 2b
    subu    t1, 1
    .set    reorder
    
    ld      v0, 0(t0)
    dsubu   v0, t2
    jr      ra
    .end    ticksper1024inst

/********************************
 * cpuclkper100ticks(&tick_count)
 *-------------------------------
 * returns the number of cpu clocks
 * required for COUNT ticks of the CRIME time base register.  tick_count
 * will contain the actual number of ticks of the CRIME timer (which will
 * not be 100).
 *
 * We prime the cache before beginning measurement and then take the
 * average of 5 trials.
 *-------------------------------
 * register usage:
 *	t0 -- limit timer count
 *	t1 -- address of crime time register
 *	t2 -- current value of CRM_TIME
 *	t3 -- loop count register
 *	t4 -- scratch
 *	t5 -- count increment
 *	t6 -- flag to indicate a wrap on the crime time counter
 *	t7 -- total of CRM_TIME ticks
 *      t8 -- total of C0_COUNT ticks
 *-------------------------------
 * We make no provision for checking for wrap around on the CRM_TIME
 * register or C0_COUNT since each of these registers will take several
 * minutes from boot to wrap around (about 30 for C0_COUNT, much much 
 * longer for CRM_TIME -- 71079 minutes).
 *-------------------------------
 * NB:	 it is OK to write C0_COUNT here since we have yet to initialize 
 *       the clock.
 */
    .globl  cpuclkper100ticks
    .ent    cpuclkper100ticks
cpuclkper100ticks:
    .set    noreorder
    mtc0    zero, C0_COUNT
    .set    reorder
    la      t1, CRM_TIME|K1BASE
    li      t3, TRIAL_COUNT-1
    la      t4, 1f
    li      t7, 0
    li      t8, 0
    
 #
 # now suck in the code we are about to execute into the
 # cache so that we can avoid cache fill induced delays
 # during the execution.
 #
    .set    noreorder
#if 0
    cache   CACH_PI|C_FILL,   0(t4)
    cache   CACH_PI|C_FILL,  32(t4)
    cache   CACH_PI|C_FILL,  64(t4)
    cache   CACH_PI|C_FILL,  96(t4)
    cache   CACH_PI|C_FILL, 128(t4)
#endif
    
1:  mtc0    zero, C0_COUNT
    sd      zero, 0(t1)
    li      t0, COUNT
    
2:  ld      t2, 0(t1)
    mfc0    v0, C0_COUNT
    sltu    t4, t2,t0
    bne     t4, zero, 2b
    nop
    .set    reorder

    daddu   t7, t2		# total CRM_TIME ticks for all runs
    addu    t8, v0		# total C0_COUNT ticks for all runs

    .set    noreorder
    bne     t3, zero, 1b
    addi    t3, 0xffff
    .set    reorder

    li      t3, TRIAL_COUNT

    divu    t8, t3
    mflo    v0                  # average of runs

    divu    t7, t3
    mflo    t4                  # average of runs
    sw      t4, 0(a0)

    jr      ra
    .end    cpuclkper100ticks
