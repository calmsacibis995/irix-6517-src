
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <asm.h>
#include <regdef.h>
#include <flash.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/crime.h>
#include <sys/mace.h>
#include <sys/IP32.h>


/*
 *  POST1:         When defined will execute the post1kseg1 in kseg1.
 *  KSEG0UNCACH:   When defined kseg0 will run uncached.
 *  FWinMEM:       This is for Simulations, so we don't copy FW to memory.
 *                 we prload it.
 */
#if	defined(IP32SIM)
#define FWinMEM
#undef  POST1
#undef  DoMTEClear
#else
#define POST1
#define DoMTEClear
#undef  FWinMEM
#endif

/* #define POST1                */
/* #define KSEG0UNCACH          */
/* #define FWinMEM              */

/*
 * Post1 skeleton
 *
 *
 * Called with one argument, a pointer to the firmware segment.
 * After configuring and testing memory and cache, load the firmware
 * segment into memory.
 *
 * NOTE: All post1 code must be position independent.
 *
 * NOTE: Although the post1 test code may test an add-on secondary cache
 *      it must leave it turned OFF as the remainder of the firmware does not
 *	deal with such caches.  In particular, the "flush" routine later in
 *      this code does not deal with add-on secondary caches.
 */

 # SEGMENT_START	# reserve space for the header ...

LEAF(post1Header)
	.set noreorder;					
	b	1f
	nop
	.space	FS_HDR_LEN
1:							
	.set reorder
	END(post1Header)

#define post1FSIZE 48
NESTED(post1,post1FSIZE,ra)	# post1(functionCode,segmentHeader*,resetCount)

   # We sort of conform to the calling standard.
   # We save the arguments and all the s? registers in case we return.
   # We don't allocate any temporaries on the stack.

    subu    sp,post1FSIZE
    sw	    ra,post1FSIZE-4(sp)	        # Save return address on stack
    sw	    s7,post1FSIZE-12(sp)	# Save s7
    sw	    s6,post1FSIZE-16(sp)	# Save s6
    sw	    s5,post1FSIZE-20(sp)	# Save s5
    sw	    s4,post1FSIZE-24(sp)	# Save s4
    sw	    s3,post1FSIZE-28(sp)	# Save s3
    sw	    s2,post1FSIZE-32(sp)	# Save s2
    sw	    s1,post1FSIZE-36(sp)	# Save s1
    sw	    s0,post1FSIZE-40(sp)        # Save s0
    sw	    a0,post1FSIZE+0(sp)	        # Save a0 Function code
    sw	    a1,post1FSIZE+4(sp)	        # Save a1 Segment addr
    sw	    a2,post1FSIZE+8(sp)	        # Save a2 reset count

#if 0 
  #
  # We do post1 only if the function == reset.

    beq	    a0, FW_HARD_RESET, 1f   /* its hardreset.                   */
    beq	    a0, FW_SOFT_RESET, 1f   /* its softreset                    */
    b	    post1_end               /* skip post1 entirely.             */
#endif

  #
  # Call SizeMEM to initialize the memory control register.

1:  jal	    SizeMEM                 /* Call to size system memory.      */

  #
  # Call mte_zero to initialize low memory
#if defined(DoMTEClear)
    li      a0, LINEAR_BASE         /* the physical RAMBASE             */
    li      a1, 0x8000              /* the end of memory.               */
    jal	    mte_zero                /* Call to initialize ECC.          */
#endif

  #
  # Check the KSG1 memory space for the sloader/post1 stack, and for 
  # post1 kseg0 code space - make the test run faster.
  
    jal	    DupSLStack              /* test mem[0] to mem[0x1000]       */
    la      t0, 0xa0001000          /* save stack pointer to 0x1008     */
    sd      sp, 0x08(t0)            /* so we can retrive it later.      */
    lui     t1, 0xa000              /* now we switch to kseg1 stack.    */
    or      sp, sp, t1              /* done.                            */

 # Cleanup PD/SD cache to remove any possible ECC errors.
  
    jal     IP32processorTCI        /* cleanup caches                   */

#if defined(POST1)  
  #
  # Check the KSG1 memory space for the post executable.
  # upon memtest complete copy post1 to memory and go cache.
  
    la      a0, POST1KSEG1_IMAGE    /* staring address.                 */
    la      a1, POST1KSEG1_IMAGE_END /* ending address.                 */
    la      a2, 0xa0004000          /* the target address.              */
    la      a3, 0x0                 /* tmp register.                    */
    jal	    CopyPOST1               /* test mem[0] to mem[0x1000]       */
	                            /* and copy the SL stack to it.     */

  #
  # Jump to POST1 control, its now in the memory at location
  # 0xa0004000 or 0x80004000, depend on the POST1VA2 when build.
  # and post1cntl will figure it out during the run time, we just assume
  # kseg1.

    la	    a0, 0xa0004000          /* the kseg1 post1diags entry point */
    jal	    a0			    /* call post1diags.                 */
  
#endif  

  # Call post1 test and initialization code.  This code may move the
  # stack but on return, the stack must contain the same contents as before.
  # We don't assume that any register contents other than sp are maintained
  # across the call to post1Go.
  #         jal	post1Go

    la      t0, 0xa0001000          /* load the original stack pointer. */
    ld      t0, 0x08(t0)            /* so we can retrive it later.      */
    lui     t1, 0x2000              /* now we switch to kseg0 stack.    */
    xor     t1, t1, sp              /* check the running stack pointer. */
    bne     t0, t1, exit            /* some internal error detected.    */
#if 0                               /* don't switch back to kseg0       */
    move    sp, t0                  /* but stay in kseg1 address space  */
#endif 

#if 0
 # Turn on the RED led again, postkseg1 may have changed the led color.
 
    la      t0, PHYS_TO_K1(ISA_FLASH_NIC_REG) /* load the LED address.  */
    ld      t1, 0x0(t0)             /* read the original value          */
    li      t2, ~ISA_LED_GREEN      /* turn on RED led.                 */
    and     t1, t1, t2              /* now led should have red=0        */
    ori	    t1, ISA_LED_RED         /* also turn on the green one.      */
    sd      t1, 0x0(t0)             /* and write it to mace             */
#else 
 # Turn led to amber.
 
    la      t0, PHYS_TO_K1(ISA_FLASH_NIC_REG) /* load the LED address.  */
    ld      t1, 0x0(t0)             /* read the original value          */
    li      t2, ~(ISA_LED_GREEN|ISA_LED_RED)  /* turn on RED led.       */
    and     t1, t1, t2              /* now led should have red,green=0  */
    sd      t1, 0x0(t0)             /* and write it to mace             */
#endif
    
post1_end:
#if defined(FWinMEM)
 # In simulation we do not really want to copy fw into memory if 
 # we can preload the memory with FW image.
 
    lw      a0, post1FSIZE+0(sp)    /* FunctionCode                     */
    lw      a1, post1FSIZE+8(sp)    /* ResetCount                       */
#if defined(KSEG0UNCACH)
    la      t0, 0xa1000000          /* load the starting address.       */
#else
    la      t0, 0x81000000          /* load the starting address.       */
    jalr    t0                      /* branch to FW in memory           */
#endif
    b       exit                    /* we should never get here.        */
    
#endif

  # Get address of segment header.  Check the segment is a Xcopy segment.
  # (Later we may support Xcompress and/or X*PIC
	
	lw	t0,post1FSIZE+4(sp)	# Fetch segment addr
	lbu	t1,FS_SEGTYPE(t0)
	li	t2,FS_Xcopy
	bne	t2,t1,exit		# Unsupported segment type.

  # Determine address of segment body.

	move	s0,t0                   # point at header for 1st blk
	move	s1,zero

  #
  # Construct descriptors of protected regions of memory.  During
  # the firmware load we are not to modify these regions.  One region is
  # page the stack is in.  The other region is this code itself.
  #
	la	s4,post1Lower		# Code descriptor
	la	s5,post1Upper
	
	move	s7,sp			# Stack descriptor
	addiu	s7,s7,4095		#   Round up to top of stack
	li	t0,~4095		# 
	and	s7,s7,t0		#
	addu	s6,s7,t0		#   Bottom is down one page from top

	li	t0,TO_PHYS_MASK		# Convert all to physical addr's
	and	s7,s7,t0
	and	s6,s6,t0
	and	s5,s5,t0
	and	s4,s4,t0
		
  # s7:	upper bound of protected stack area  (Physical)
  # s6: lower bound of protected stack area  (Physical)
  # s5: upper bound of protected program area (Physical)
  # s4:	lower bound of protected program area (Physical)
  #
  # s1:	segment checksum
  # s0:	address of block

  # Copy each flash block to RAM, updating checksum as we go.
  #
  # Note that there is some chance that the image we are loading is corrupted
  # and a corrupted address or length in block headers will cause the code
  # to attempt to overwrite itself.  We guard against this by checking for
  # overwrite using the pointers in s4..s7.

	.set	noreorder
post1Lower:	# Lower bound of region protected from overwrite
	nop
	nop
	.set	reorder

  # While storing, we could get a cache fill from a block with bad
  # parity.  Just turn off ECC checking.

	la      t0,PHYS_TO_K1(CRM_MEM_CONTROL)
	sd	zero,0(t0)

  # The first block must be copied headers and all; subsequent
  # blocks are copied withou their headers.  Furthermore, the
  # block header in the first block follows the segment header
  # registers s2 and s3 remember the state of all that ...

  # A note on the checksum.  The checksum computation here includes
  # the header and the header checksum.  The checksum computation in 
  # the flashbuilder does not, but that doesn't matter since the 
  # the header cancels itself out of the computation.  Howver, we
  # must be careful not to count the first block header twice.  All
  # this ugliness says we need some more re-design
  
	li	s2,FS_HDR_LEN
	move	s3,zero

nextBlock:
	addu	s2,s0,s2
	lw	a0,FSB_ADDR(s2)		# Get block destination addr
	beq	s3,zero,1f		# don't chksum block hdr 1st time
	addu	s1,s1,a0
1:
	lw	a1,FSB_LENGTH(s2)	# Get block length
	move	s2,zero			# next time, no header
	beq	s3,zero,1f
	addu	s1,s1,a1
1:
	beq	a1,zero,finished	# Zero length means we are finished

	#### 	Should add check that the block lies within the FLASH
	####    segment bounds.
			
  # Call validate to see if the block overwrites a protected area.  If so, exit
	
	bal	validate
	bne	v0,zero,exit

  # Set up the registers for the block move loop
  	
	addu	t0,s0,s3
	li	s3,FSB_DATA		# next time, move past block hdr
#if 0	
	move	t1,a0
	move	t2,a1
#else
  # Force it to memory.
    lui	    t2, 0xa000
    or	    t1, a0, t2
    move    t2, a1
#endif

  # a0:	 block destination address
  # a1:	 block length
  # t0:	 block source address
  # t1:	 block destination address (copy of a0)
  # t2:	 block length (copy of a1)
  # t3:	 temporary

  # Copy words from source to destination, updating checksum, addr's and count.
  # Use the the copy of a0 and a1 for the move. Preserve the originals for
  # flush.

	.set	noreorder		# Needed because of checksum overflows.
1:	lw	t3,0(t0)		# Fetch value
	addu	s1,s1,t3		# Add to running checksum
	sw	t3,0(t1)		# Store
	lw	t3,4(t0)
	addu	s1,s1,t3
	sw	t3,4(t1)
	lw	t3,8(t0)
	addu	s1,s1,t3
	sw	t3,8(t1)
	lw	t3,12(t0)
	addu	s1,s1,t3
	sw	t3,12(t1)
	.set	reorder
	addiu	t0,16			# Increment src ptr
	addiu	t1,16			# Increment dst ptr
	addiu	t2,-16			# Decrement byte count
	bne	t2,zero,1b		# Branch if bytecount is non-zero
    
  # Advance the block pointer (s0) to the next block (now t0)
  # Call flush to flush the cache for this block.
	
	move	s0,t0
#if 0        
	bal	flush
#endif        
	b	nextBlock

  # Copy finished.
  # 
  # Add in the checksum.  If the result is zero, branch to the loaded code.
  # Otherwise fall through to the exit code that returns to the caller.
  #
  # Note that once we call the loaded code, there's no attempt to provide
  # a way back.  If the loaded code fails, it's up to it to deal with the
  # problem, there's safe no way to return to here or to the serial loader.
  # 
finished:
#if 0
  # Never turn on ECC in PROM
	la	t3,PHYS_TO_K1(CRM_MEM_CONTROL)	# Turn ECC checks back on.
	li	t0,CRM_MEM_CONTROL_ECC_ENA
	sd	t0,(t3)
#endif  

	lw	t3,FSB_DATA(s0)		# Fetch checksum
	addu	s1,s1,t3		# Add it to our running checksum
	bne	s1,zero,exit		# Must be zero

  # If we get here, it's because the block length is zero and we have a
  # valid checksum.
  #
  # A "memory" address of the terminating zero length block is the execution
  # address for the code.  The memory address is still in a0 from above.
  #
  # Pass the function code and the reset count forward when calling the
  # firmware.
  
  #
  # Following KSEG0UNCACH define will start execute code out of kseg1 address
  # space instead of kseg0.
  
    .set    noreorder   
    mfc0    t0, C0_CONFIG
    nop 
    li      t1, ~0x7
    nop
    and     t1, t0, t1
#if defined(KSEG0UNCACH)
    ori     t1, 2
#else
    ori     t1, 3
#endif  
    mtc0    t1, C0_CONFIG
    nop
    nop
    nop
    nop
    .set    reorder

    jal     IP32processorTCI        /* cleanup caches                   */
    
    move    t0,a0               # Save the target address.
    lw      a0,post1FSIZE+0(sp)	# FunctionCode 
    lw      a1,post1FSIZE+8(sp)	# ResetCount
    jr      t0			# Call the firmware
	
  # If we get here, some problem has been detected.  We simply exit back
  # to the serial loader.
 
exit:	
	lw	ra,post1FSIZE-4(sp)	# Restore return address from stack
	lw	s7,post1FSIZE-12(sp)	# Restore s7
	lw	s6,post1FSIZE-16(sp)	# Restore s6
	lw	s5,post1FSIZE-20(sp)	# Restore s5
	lw	s4,post1FSIZE-24(sp)	# Restore s4
	lw	s3,post1FSIZE-28(sp)	# Restore s3
	lw	s2,post1FSIZE-32(sp)	# Restore s2
	lw	s1,post1FSIZE-36(sp)	# Restore s1
	lw	s0,post1FSIZE-40(sp)	# Restore s0
	addu	sp,post1FSIZE
	li	v0,1			# Failed return
	jr	ra
	END(post1)


/**********
 * validate	checks for overwrite attempts
 *---------
 *
 * returns 0 if valid, non-zero if invalid
 *
 */
LEAF(validate)
	andi	v0,a0,3			# Check word aligned
	bne	v0,zero,2f		#
	andi	v0,a1,3			# Check even word count
	bne	v0,zero,2f		#

  # Address checks are made in "physical" space to simplify k0/k1 overlap
  # detection.

	addu	t0,a0,a1		# Generate upper bound address of block
	li	t2,TO_PHYS_MASK		#
	and	t1,t1,t2
	and	t0,a0,t2
	 
  # t0:	BL	Block lower bound (Physical)
  # t1:	BU      Block upper bound (Physical)
  # s4:	PBL1	Protected block lower bound (1)
  # s5:	PBU1	Protected block upper bound (1)
  # s6:	PBL2	Protected block lower bound (2)
  # s7:	PBU2	Protected block upper bound (2)
	
	sltu	v0,t1,s6		# v0 = (BU<PBL2)
	beq	v0,zero,1f
	sltu	v0,s7,t0		# v0 = (PBU2<BL)
	bne	v0,zero,2f

1:	sltu	v0,t1,s4		# v0 = (BU<PBL1)	
	beq	v0,zero,2f
	sltu	v0,s5,t0		# v0 = (PBU1<BL)

2:	jr	ra
	END(validate)


/*******
 * flush   Flush cache
 *------
 * a0:	 Start of block
 * a1:	 Length of block
 */ 
LEAF(flush)
	# Exit if range is not cached
	srl	t0,a0,29
	bne	t0,0x4,flush_exit

	# Determine cache configuration
	
	.set	noreorder
	mfc0	t0,C0_CONFIG		# Get config bits
	.set	reorder
	andi	t4,t0,CONFIG_DB
	li	t1,16			# PD block size <- 16
	beq	t4,zero,1f		# Bif CONFIG_DB==1 (32 byte block)
	li	t1,32			# PD block size <- 32
	
1:	srl	t4,t0,16		# Test CONFIG_SC
	andi	t0,t4,CONFIG_SC>>16
	beq	t0,zero,2f	
	andi	t4,t4,CONFIG_SB>>16
	srl	t4,t4,CONFIG_SB_SHFT-16
	addi	t4,t4,4			# t4 = ln2(secondary block size)
	li	t3,1
	sllv	t4,t3,t4		# t4 = secondary block size
	slt	t3,t4,t1		# t3 = t4<t1
	beq	t3,zero,2f
	move	t1,t4			# t1 = min(t1,t4)
2:	move	t2,a0
	add	t3,t2,a1	
	add	t3,t3,t1
	
  # t0:	0=> primary cache, !0=> secondary cache
  # t1:	cache block size  (minimum of PD or SD cache size)
  # t2:	block pointer
  # t3:	upper bound of block + cache block size
  # t4:	temporary

  # Note that we increment the upper bound by one cache block size.  Since
  # we advance by cache block size, this insures we write back the last block.
  # Note also that if the secondary cache block is substantually larger than
  # the primary cache block, we will be issuing unneeded writeback op's
  # against the secondary cache.  It's not worth trying to avoid this.
	
	.set	noreorder
flush_loop:	
	cache	CACH_PD|C_HWB,0(t2)	# PC hit, writeback
	beq	t0,zero,1f		# Skip if no SC
	cache	CACH_SD|C_HWB,0(t2)	# SC hit, writeback
1:	addu	t2,t2,t1		# Advance by cache line
	sltu	t4,t2,t3		# Test t2<t2
	bne	t4,zero,flush_loop
	nop
flush_exit:	
	jr	ra
	nop
post1Upper:	# Upper bound of region protected from overwrite
	END(flush)
	.set	reorder


/*********
 * post1Go
 *--------
 * Dummy post1 diagnostics section.  This code will probably be replaced
 * with an external routine.  A single argument, FunctionCode is passed in
 * a0.
 */
#define post1goFSIZE 16
NESTED(post1Go,post1goFSIZE,ra)	# post1Go(FunctionCode)

  # If FunctionCode == FW_SOFT_RESET
  #	Confirm valid memory configuration.
  #     If invalid, set FunctionCode to FW_HARD_RESET
  #
  # If Functioncode == FW_HARD_RESET
  #	Configure memory
  #	Test small region of KSEG1 memory equal to size of post1 module
  #
  # Copy stack to KSEG1
  # Switch to KSEG1 stack
  # Invalidate primary I-cache
  # Flush secondary cache and/or turn it off
  # Copy Post1 to KSEG1 RAM
  # Branch to self in KSEG0 RAM
  #
  # If FunctionCode == FW_HARD_RESET
  #	Test memory used by firmware
  #	Flood all of memory with bad00bad
  #	   (Don't damage stack or post1 code)
  #     Additional tests
  
  #
  # Size mthe memory, and upon return s0 will have the grand total
  # of memory size, and it will not return if there are no memory found.

  # NOTE: The following algorithm can be used to copy the stack:
  #
  #      newTSP = topOfNewStack;
  #      oldTSP = (sp+4095) & ~4095;
  #      do
  #        *--newTSP = *--oldTSP;
  #      while (newTSP!=sp);
  #      sp = newTSP;


  	jr	ra

   	END(post1go)

    
