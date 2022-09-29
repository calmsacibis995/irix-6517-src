
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * sl_csu.s -- Serial Loader prom startup
 *
 * The serial loader must occupy less than 16KB.  It is stored in the 16KB
 * protected block of the FLASH.
 */
#include <asm.h>
#include <regdef.h>
#include <sys/cpu.h>
#include <sys/fpu.h>
#include <sys/IP32.h>
#include <sys/ds17287.h>
#include <sys/sbd.h>
#include <flash.h>

/*
 * The actual address of the serial loader stack is not public but
 * it must be less than one page and top must be page aligned.
 */
#define SLSTACK_BASE 0x80000000
#define SLSTACK_SIZE 0x1000


/* Values to identify exception types for BEV exception handling */
#define BEV_UTLB	0
#define BEV_XUTLB	1		
#define BEV_CACHE	2		
#define BEV_GENERAL	3

/* Initial config register values */
#define INIT_CONFIG CONFIG_IB|CONFIG_DB|CONFIG_NONCOHRNT
#define PC_BLOCKSIZE 32
#define PC_LN2_BLOCKSIZE 5
	
#define NO_CACHE_TESTS
#define NO_UTC 
/* #define NO_NVRAM */
	
	
/* Macro to simplify NVRAM addressing */	
#define _NVRAM(o) DS_OFFSET(0,DS_RAM+o)

/* R10000's implementation number.     */
#define C0_IMP_R10000    0x09

/*******
 * start	Start of FLASH PROM
 */
LEAF(start)	# Actually we are not a leaf routine.  We claim to be
		# so as to improve the output from the disassembler.  We aren't
		# really managing the registers according to the standard
		# call interface.  That is true for all routines in this
		# module.

		# we simply reserve the header, then branch past all the
		# exception vector processing.  This "wastes" about
		# 512 bytes of PROM - but it helps maintenance a lot.

		# perhaps some fixed size tables could be put in the gap
		# if the waste is too offensive to bear.
		

  #	SEGMENT_START	# reserve space for the header ...
	.set noreorder;					
	b	1f
	nop
	.space	FS_HDR_LEN
1:							


	b	promStart
	.set reorder
	END(start)

  # ---  512 - headersize + 16  bytes of dead space here ---
       
/*********************
 * prom_CALLback_entry
 *
 * Must end up at offset 0x100.  The symbol bev_utlbmiss_vector is
 * used only so that a post assembly tool can check the offset.
 */
    .align  8                       /* should align to 0xbfc00100       */
    .globl  CALLback
CALLback:
    .set    noreorder
    mtc0    a0, C0_LLADDR 
    .set    reorder
    lui     t8, 0xb400
    sd      a0, 0x270(t8)
    la      t8,PHYS_TO_K1(CRM_MEM_CONTROL)
    sd	    zero,0(t8)
    b       prom_cont1
    
/*********************
 * bev_utlbmiss_vector
 *
 * Must end up at offset 0x200.  The symbol bev_utlbmiss_vector is
 * used only so that a post assembly tool can check the offset.
 */
	.align 9	# should align to 0xbfc00200
	.globl	bev_utlbmiss_vector
	
LEAF(bev_utlbmiss_vector)
	beq	fp,zero,bev_exception
	li	a0,BEV_UTLB
	jr	fp
	END(bev_utlbmiss_vector)

/*
 * Patterns for FLASH access tests
 * By following the bev utlb exception vector, they are guaranteed to be
 * at offset 0x210 from the beginning of the prom.
 * Note that this limits the code above to exactly four instructions
 */
	.align 4	# should align to 0xbfc00210
	.word	0xFFFFFFFF
	.word	0x00000000
	.word	0x55555555
	.word	0xAAAAAAAA
	
  # --- 0x60 bytes of dead space here ...

	.align 7	# should align to 0xbfc00280
			
/**********************
 * bev_xutlbmiss_vector
 *
 * Must be at offset 0x0280.
 * The symbol bev_xutlbmiss_vector is used only so that a post assembly tool
 * can check the offset.
 */
	.globl	bev_xutlbmiss_vector

LEAF(bev_xutlbmiss_vector)
	beq	fp,zero,bev_exception
	li	a0,BEV_XUTLB
	jr	fp
	END(bev_xutlbmiss_vector)


  # 0x70 bytes of dead space here ...

	.align 8	# should align to 0xbfc00300
	
/*********************
 * bev_cacheerr_vector
 *
 * Must be at offset 0x0300.
 * The symbol bev_cacheerr_vector is used only so that a post assembly tool
 * can check the offset.
 */

	.globl	bev_cacheerr_vector

LEAF(bev_cacheerr_vector)
	beq	fp,zero,bev_exception
	li	a0,BEV_CACHE
	jr	fp
	END(bev_cacheerr_vector)


  # 0x70 bytes of dead space here ...

	.align 7	# should align to 0xbfc00380
	
		
/*********************
 * bev_general_vector
 *
 * Must be at offset 0x0380.
 * The symbol bev_general_vector is used only so that a post assembly tool
 * can check the offset.
 */
	.globl	bev_general_vector

LEAF(bev_general_vector)
	beq	fp,zero,bev_exception
	li	a0,BEV_GENERAL
	jr	fp
	END(bev_general_vector)

		
/***************
 * bev_exception	Default exception handler
 */
LEAF(bev_exception)
	.set	noreorder
	mfc0	t0,C0_SR
	mfc0	t1,C0_CAUSE
	mfc0	t2,C0_CACHE_ERR
	mfc0	t3,C0_EPC
	.set	reorder
1:
	b	1b
	END(bev_exception)


  #	-	-	-	-	-	-	-	-
  #
  #		BEGIN PROM CODE HERE
  #
  #	-	-	-	-	-	-	-	-

  # get here from branch from branch around header at 0xbfc00000 -
  # i.e. we're four instructions (two branches, two branch delay
  # slots) from bootstrap start.

LEAF(promStart)

  # Save t8 in the COMPARE and LLADDR coprocessor registers.  If this is
  # a soft reset, we are more likely interested in t8 than in COMPARE and
  # LLADDR.  Save t9 in NVRAM

#if 0
    .set    noreorder	
    mtc0    t8,C0_COMPARE
    dsrl32  t8,t8,0
    mtc0    t8,C0_LLADDR
    .set    reorder
#endif

/*
 *  Zap Integer Unit logical register.
 */
    .set    noreorder	
    .set    noat
    move    $1,  zero
    .set    at
    move    $2,  zero
    move    $3,  zero
    move    $4,  zero
    move    $5,  zero
    move    $6,  zero
    move    $7,  zero
    move    $8,  zero
    move    $9,  zero
    move    $10, zero
    move    $11, zero
    move    $12, zero
    move    $13, zero
    move    $14, zero
    move    $15, zero
    move    $16, zero
    move    $17, zero
    move    $18, zero
    move    $19, zero
    move    $20, zero
    move    $21, zero
    move    $22, zero
    move    $23, zero
    move    $24, zero
    move    $25, zero
    move    $26, zero
    move    $27, zero
    move    $28, zero
    move    $29, zero
    move    $30, zero
    move    $31, zero

/*
 *  Zap Floating Point Unit logical register.
 */
#if 0
    mfc0    t0, C0_PRID
    li      t1, C0_IMP_R10000       /* the R10000 IMP number.           */
    mfc0    t2, C0_SR               /* read the status register.        */
    andi    t0, C0_IMPMASK          /* get the inplementation num       */
    srl     t0, C0_IMPSHIFT         /* shift to bit 0.                  */
    bne     t0, t1, prom_cont1      /* its not R10000                   */
    nop                             /* a delay slot.                    */
    lui     t0, 0x0400              /* check the FR bits.               */
    and     t0, t2                  /* extract the FR bits.             */
    mtc1    zero, $f0
    mtc1    zero, $f1
    mtc1    zero, $f2
    mtc1    zero, $f3
    mtc1    zero, $f4
    mtc1    zero, $f5
    mtc1    zero, $f6
    mtc1    zero, $f7
    mtc1    zero, $f8
    mtc1    zero, $f9
    mtc1    zero, $f10
    mtc1    zero, $f11
    mtc1    zero, $f12
    mtc1    zero, $f13
    mtc1    zero, $f14
    mtc1    zero, $f15
    beqz    t0, prom_cont1
    nop
    mtc1    zero, $f16
    mtc1    zero, $f17
    mtc1    zero, $f18
    mtc1    zero, $f19
    mtc1    zero, $f20
    mtc1    zero, $f21
    mtc1    zero, $f22
    mtc1    zero, $f23
    mtc1    zero, $f24
    mtc1    zero, $f25
    mtc1    zero, $f26
    mtc1    zero, $f27
    mtc1    zero, $f28
    mtc1    zero, $f29
    mtc1    zero, $f30
    mtc1    zero, $f31
    nop
    nop
#else
/*
 * set_fpc_csr sets the fpc_csr and returns the old fpc_csr.
 */
set_fpc_csr:
    .set    noreorder
    mfc0    v1, C0_SR
    nop
    nop
    li	    a1,SR_CU1|SR_KADDR|SR_FR
    mtc0    a1, C0_SR
    nop
    nop
    nop
    mtc1    zero, $f0
    mtc1    zero, $f1
    mtc1    zero, $f2
    mtc1    zero, $f3
    mtc1    zero, $f4
    mtc1    zero, $f5
    mtc1    zero, $f6
    mtc1    zero, $f7
    mtc1    zero, $f8
    mtc1    zero, $f9
    mtc1    zero, $f10
    mtc1    zero, $f11
    mtc1    zero, $f12
    mtc1    zero, $f13
    mtc1    zero, $f14
    mtc1    zero, $f15
    mtc1    zero, $f16
    mtc1    zero, $f17
    mtc1    zero, $f18
    mtc1    zero, $f19
    mtc1    zero, $f20
    mtc1    zero, $f21
    mtc1    zero, $f22
    mtc1    zero, $f23
    mtc1    zero, $f24
    mtc1    zero, $f25
    mtc1    zero, $f26
    mtc1    zero, $f27
    mtc1    zero, $f28
    mtc1    zero, $f29
    mtc1    zero, $f30
    mtc1    zero, $f31
    nop
    nop
    ctc1    zero, fpc_csr
    nop
    nop
    ctc1    zero,$31   
    nop 
    nop
    mtc0    v1, C0_SR
    nop
    .set    reorder
#endif

/*
 *  Here is the place where all the firmware callback should
 *  endup.
 *  Note: Do not remove following labels. 
 *        prom_CALLback_entry  and prom_cont1
 */
prom_cont1:
    
#ifndef NO_NVRAM
	la	t8,PHYS_TO_K1(RTC_BASE)
	sb	t9,_NVRAM(RTC_SAVE_REG+0)(t8)	# Save byte 0 of t9
	srl	t9,t9,8		
	sb	t9,_NVRAM(RTC_SAVE_REG+1)(t8)	# Save byte 1 of t9
	srl	t9,t9,8
	sb	t9,_NVRAM(RTC_SAVE_REG+2)(t8)	# Save byte 2 of t9
	srl	t9,t9,8
	sb	t9,_NVRAM(RTC_SAVE_REG+3)(t8)	# Save byte 3 of t9
	srl	t9,t9,8
	sb	t9,_NVRAM(RTC_SAVE_REG+4)(t8)	# Save byte 4 of t9
	srl	t9,t9,8
	sb	t9,_NVRAM(RTC_SAVE_REG+5)(t8)	# Save byte 5 of t9
	srl	t9,t9,8
	sb	t9,_NVRAM(RTC_SAVE_REG+6)(t8)	# Save byte 6 of t9
	srl	t9,t9,8
	sb	t9,_NVRAM(RTC_SAVE_REG+7)(t8)	# Save byte 7 of t9
#endif
	
#if 1        
  #ifndef NO_LEDS
  # Set the LED to amber (both RED and GREEN on)
  #	ori	t9,ISA_LED_RED|ISA_LED_GREEN|ISA_NIC_DEASSERT
  # li      t8, ISA_LED_RED|ISA_LED_GREEN|ISA_NIC_DEASSERT
  
    la      t8, PHYS_TO_K1(ISA_FLASH_NIC_REG)
    ld      t9, (t8)
    li      t8, ISA_LED_RED|ISA_LED_GREEN
    not     t8
    and     t9, t9, t8
    la      t8, PHYS_TO_K1(ISA_FLASH_NIC_REG)
    sd      t9, (t8)
  
#endif

  # Fetch the status register, check for BEV.

    .set    noreorder	
    mfc0    t8,C0_SR
    .set    reorder

    li      t9,SR_BEV	
    and     t8,t8,t9
    bne     t8,zero,_bev_on	# Branch if BEV is set

 # No BEV bit. Entry must be a firmware callback.
 # Set BEV, mask interrupts, branch to reset common

    li      t8,SR_BEV
    .set    noreorder
    mtc0    t8,C0_SR
    .set    reorder
    
    
 # Verify that we have a valid function code in a0.
 
    .set    noreorder

    li      t9, FW_EIM
    beq     a0, t9, reset_common
    
    li      t8, FW_HALT
    beq     a0, t8, reset_common
    
    li      t9, FW_POWERDOWN
    beq     a0, t9, reset_common
    
    li      t8, FW_SOFT_RESET
    beq     a0, t8, crm_softreset
    
    li      t9, FW_RESTART
    beq     a0, t9, reset_common 
    
    li      t8, FW_HARD_RESET
    beq     a0, t8, crm_hardreset
    
    li      t9, FW_INIT       
    beq     a0, t9, reset_common 
    
    li      t8, FW_REBOOT
    beq     a0, t8, reset_common 
    nop
    
   # a0 does not have an valid callback function code,  
   # treat it as soft reset so we can do recover if necessary.
   
    b       crm_hardreset
    nop
    
    .set    reorder
    

 # BEV is set, Increment the reset count in NVRAM
 # and check for SR	
_bev_on:

#ifndef NO_NVRAM
    la      t8,PHYS_TO_K1(RTC_BASE)
    lb      t9,_NVRAM(RTC_RESET_CTR)(t8)
    addi    t9,t9,1
    sb      t9,0(t8)
#endif
    .set    noreorder
    mfc0    t8,C0_SR
    .set    reorder
    li      t9,SR_SR
    and     t8,t8,t9
    bne     t8,zero,soft_reset	# Branch if SR is set

 # BEV is set, SR is not set.  It's a hard reset.
 # Set the function code and fall into reset common
 
    li      a0,FW_HARD_RESET
    .set    noreorder
    mtc0    a0, C0_LLADDR 
    .set    reorder
    b       reset_common

/*
 * reset_common is common to all reset paths although the soft reset path
 * saves all the registers to memory before returning here.
 */
XLEAF(reset_common)

	
  # a0:	Firmware function code

    move    s7,a0
		
  # Init the status register (disable cache parity and ECC errors;  turn
  #    on COP0 and COP1, set the BEV bit).
  #
  # Clear the cause register, the PGMASK register, and the
  #    watch registers.
  #
  # In the config register, set the primary I and D cache lines sizes
  #    and turn on the cache.
  #
  # Clear fp, establishing default BEV exception handling.
	
#define PROM_SR SR_CU0|SR_CU1|SR_FR|SR_KX|SR_BEV|SR_DE

    .set    noreorder
#if 0
    li      v0,SR_DE|SR_CU0|SR_CU1|SR_BEV
#else
    li      v0,PROM_SR
#endif
    mtc0    v0,C0_SR
    nop
    mfc0    t0,C0_CONFIG
    li      v0, 7
    not     v0
    and     t0, v0
    ori     t0, CONFIG_NONCOHRNT
    mtc0    t0,C0_CONFIG
    mtc0    zero,C0_CAUSE	
    mtc0    zero,C0_PGMASK	
    mtc0    zero,C0_WATCHLO	
    mtc0    zero,C0_WATCHHI
    move    fp,zero
    .set    reorder

    
#ifndef NO_UTC
  #
  # Fetch the UST and save it in NVRAM
  #
    la      t8,PHYS_TO_K1(MACE_UST)
    ld      t9,(t8)
    la      t8,PHYS_TO_K1(RTC_BASE)
    sb      t9,_NVRAM(RTC_SAVE_UST+0)(t8)
    srl     t9,t9,8
    sb      t9,_NVRAM(RTC_SAVE_UST+1)(t8)
    srl     t9,t9,8
    sb      t9,_NVRAM(RTC_SAVE_UST+2)(t8)
    srl     t9,t9,8
    sb      t9,_NVRAM(RTC_SAVE_UST+3)(t8)
#endif
	
/*
 * reset the supio chip and disable the DMA channel for
 * both s1,s2 Tx and Rx.
 */
#define S1Tx_CNTL  0x00008000        
#define S1Rx_CNTL  0x00008020        
#define S2Tx_CNTL  0x0000C000        
#define S2Rx_CNTL  0x0000C020        
#define CHANL_RST  0x00000400        
#define ACE_FCR    0x00000207        
#define Rx_FRST    0x00000002        
#define Tx_FRST    0x00000004        

#if 1
_reset_uartfifo:
    la      t8, PHYS_TO_K1(ISA_SER1_BASE)
    li      t9, Rx_FRST|Tx_FRST     /* reset both receive and xmit buffer */
    sb      t9, ACE_FCR(t8)         /* reset serial port 1              */
    la      t8, PHYS_TO_K1(ISA_SER2_BASE)
    sb      t9, ACE_FCR(t8)         /* reset serial port 2              */
_reset_dma:
    la      t9, PHYS_TO_K1(MACE_ISA)
    li      t8, CHANL_RST
    sd      t8, S1Tx_CNTL(t9)       /* reset channel S1Tx               */
    sd      t8, S1Rx_CNTL(t9)       /* reset channel S1Rx               */
    sd      t8, S2Tx_CNTL(t9)       /* reset channel S2Tx               */
    sd      t8, S2Rx_CNTL(t9)       /* reset channel S2Rx               */
_reset_supio:
    la      t8, PHYS_TO_K1(ISA_RINGBASE) /* the address for the reset register. */
    li      t9, 1                   /* bit 0: reset the superio chip.   */
    sd      t9, 0x0(t8)             /* RESET                            */
    li      t9, 0x100               /* wait for a while                 */
    .set    noreorder               /* disable reordering.              */
1:  sub     t9, 1                   /* decrement t8.                    */
    bnez    t9, 1b                  /* loop until t8 == 0x0             */
    nop                             /* delay slot.                      */
    .set    reorder                 /* enable reorder                   */
    sd      t9, 0x0(t8)             /* write a 0 to bit 1.              */
#endif

/*
 * Turn off any watchdog timers
 */
_watchdog_timer_off:


#if 0
/*
 * Configure and test the serial loader stack.  We do this by invalidating
 * a region in the primary Dcache, performing a cursory test and then pointing
 * sp to the top of the region.
 */
 
    
	li	s0,SLSTACK_BASE		# Set stack base

	# NOTE:	 simulator hack.  If the address of init_slstack is >0
	# we are in useg space and must be running on the simulator.  In
	# that case we stay on our current stack.
	
	la	t0,init_slstack
	bgez	t0,1f
	addi	sp,s0,SLSTACK_SIZE	# Set top of stack (not on simulator)
1:	li	a0,0xAAAAAAAA
        bal     init_slstack_orig
#else 
/*
 * Configure and test the serial loader stack.  We do this by invalidating
 * a region in the primary Dcache, performing a cursory test and then pointing
 * sp to the top of the region.
 */
    jal	    processor_tci           /* in lib/libmsk/IP32tci.s          */
    li	    s0, SLSTACK_BASE        /* Set stack base                   */
    addi    sp, s0, SLSTACK_SIZE    /* Set top of stack                 */
    jal	    init_slstack            /* initialize the sloader stack     */
        
#endif    

/* Flood stack with patterns to verify that it works */

#ifndef NO_CACHE_TESTS
	li	a0,0x55555555
	bal	floodCheck
	li	a0,0xFFFFFFFF
	bal	floodCheck
	move	a0,zero
	bal	floodCheck
#endif
	
	# Call the serial loader routine.  It will verify FLASH.  If FLASH
	# is ok, it won't return.  If FLASH is corrupted, sloader will
	# reload it and may return to us and we just start the whole thing over
	# again.

	move	a0,s7				# a0 <- functionCode
#ifndef NO_NVRAM
	la	t0,PHYS_TO_K1(RTC_BASE)		#
	lb	a1,_NVRAM(RTC_RESET_CTR)(t0)	# a1 <- resetCount
#else
	li	a1,1
#endif
	addi	sp,-8				# Room on stack for a0 and a1
	jal	sloader
	b	start
	END(promStart)

/**************
 * init_slstack
 *-------------
 * Initializes the loader stack area.
 * This code sizes the primary D cache using the config register, and then
 * invalidates it and marks it dirty.  The result is a block of
 * data space allocated in the primary D cache.  This space can be used
 * as the stack by the serial loader and other code.  It's important that
 * users of this space don't "fall out" of the initialized cache area as
 * then we would get a cache miss and an attempt to do a cache writeback
 * followed by a cache fill and if that occurs before the memory is
 * initialized and cleared, an error will likely result.
 */
#if 0
LEAF(init_slstack_orig)
	.set	noreorder
	mfc0	t1,C0_CONFIG		# Get the config register
	.set	reorder
	andi	t1,CONFIG_DC		# Extract ln2(cache-size/4096)
	srl	t1,t1,CONFIG_DC_SHFT	#   from config register
	addi	t0,t1,12		# Compute ln2(cache-size)
	li	t1,1
	sllv	t1,t1,t0		# cache-size in bytes

  # s0:	Base of stack
  # t1:	cache size		
	
	li	t0,K1BASE		# Initial index/address value
	or	t0,t0,s0		#   is K0_TO_K1($s0)
	addu	t1,t1,t0		# Max index/address
	li	t2,0x1ffff<<12		# Mask to generate physical addr
	
  # t0:	 index, also used to generate TAGLO physical address
  # t1:	 max index
  # t2:	 Phys Mask
  # t3:	 temporary
	
	.set	noreorder
	mtc0	zero,C0_TAGHI		# TAGHI is zero for all entries
1:	and	t3,t0,t2		# Generate PTagLO from index
	srl	t3,t3,4
	ori	t3,PDIRTYEXCL
	mtc0	t3,C0_TAGLO
	cache	CACH_PD|C_IST,0(t0)	# Write TAGHI, TAGLO to tag
	.set	reorder
	addi	t0,PC_BLOCKSIZE		# Advance index by one block
	bne	t0,t1,1b		# Loop until we hit max index
	jr	ra
	END(init_slstack_orig)		
#endif

/************
 * soft_reset	Save the registers in a safe place
 */
LEAF(soft_reset)

  # We enter with the reset count in t9.  If not equal to one, set the
  # function code to hard reset and return to reset common.

#ifndef NO_NVRAM
    la      t8,PHYS_TO_K1(RTC_BASE)
    lbu     t9,_NVRAM(RTC_RESET_CTR)(t8)
#endif

    li      t8,1
    beq     t8,t9,_save_state
    b       reset_common
    li      a0,FW_HARD_RESET

/*
 *  Add code to save state here
 */
_save_state:
			
  # All done.  Set the function code to soft reset and return to reset_common
	
    b       reset_common	
    li      a0,FW_SOFT_RESET	
    END     (soft_reset)


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
	
/************
 * floodCheck
 *-----------
 * Primitive routine to flood stack with pattern
 *-----------
 *     a0	Value
 *     s0 	Bottom of stack address
 *     sp       Top of stack address
 */
LEAF(floodCheck)
	
  #	bgez	sp,3f		# Exit immediately if sp in useg (simulator)
	b	3f
	move	t0,s0
1:	sw	a0,(t0)
	addi	t0,4
	bne	sp,t0,1b
	move	t0,s0
2:	lw	t1,(t0)
	bne	t1,a0,cacheCheckError
	addi	t0,4
	bne	t0,sp,2b
3:	jr	ra
	END(floodCheck)
	
/*
 * Get here for any detected cache error
 */
LEAF(cacheCheckError)
	beq	fp,zero,bev_exception
	b	cacheCheckError
	END(cacheCheckError)

