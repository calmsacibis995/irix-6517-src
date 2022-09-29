/**************************************************************************
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 **************************************************************************/

#ident	"$Revision: 1.5 $"

#include <ml/ml.h>

#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
/*
 * This procedure ASSUMES the 6.X compilers are used since it uses those
 * temporary register conventions which DIFFER from the ucode based compilers
 * in the use and naming.
 *
 * in_cksum_sub(addr, len, prev_chksum)
 * unsigned long *addr;
 * unsigned int len, prev_chksum;
 *
 * Calculates a 16 bit ones-complement checksum.
 *
 * NOTE: This procedure always adds even address bytes to the high order
 * 8 bits of the 16 bit checksum and odd address bytes are added to the
 * low order 8 bits of the 16 bit checksum.
 *
 * Main loop costs 4 instr's per word, 1 ld and 3 math instructions
 * Retires 128 bytes/loop iteration
 *
 * a0 = Address of buffer to checksum (32 or 64 bit, doesn't matter)
 * a1 = 32-bit size of buffer in bytes
 * a2 = 32-bit checksum with bottom 16-bits containing previous checksum
 */
LEAF(in_cksum_sub)
	.set noreorder
	move	v0,a2		# copy previous checksum
	bne	a1,zero,11f	# if zero count exit stage-right immediately
	and	v1,a0,0x7	# Starting addr on 64-bit word boundary ?
	j	ra		# exit
	and	v1,a0,0x7	# Starting addr on 64-bit word boundary ?
	
11:	beq	v1,zero,4f	# yes so handle wrapping carries and start loop
        and     v1,a0,0x3       # Starting addr on 32-bit word boundary ?
        beq     v1,zero,3f      # yes go to code for 32-bit word alignment
        and     v1,a0,1         # check for odd-byte address
        beq     v1,zero,2f      # already on a 16-bit halfword boundry ?
        nop
 #
 # Arrive here with simple byte aligned address in a0
 #
	lbu	t0,0(a0)	# fetch the leading byte
	daddu	a0,1		# inc src address
#ifdef MIPSEL
	dsll    t0,8		# shift byte into top half of 16-bit word
#endif MIPSEL
	daddu	v0,t0		# Add that byte to the checkum in v0
	sltu	v1,v0,t0	# Test for carry in the addition
	daddu	v0,v1		# Add the carry if there was any
	subu	a1,1		# decrement byte count
 #
 # Arrive here with a 16-bit word aligned address in a0
 #
2:	blt	a1,2,77f	# branch if NOT enough byte count
	and	v1,a1,0x1	# Test for at least 1-byte left

	lhu	t0,0(a0)	# load 16-bit halfword
	daddu	a0,2		# inc src address to get 32-bit word aligned
	daddu	v0,t0		# accumulate checksum
	subu	a1,2		# decrement byte count

	blt	a1,2,77f	# branch if NOT enough byte count
	and	v1,a1,0x1	# Test for at least 1-byte left

	and	v1,a0,0x2	# check for not yet 32-byte aligned address
	beq	v1,zero,3f	# branch if we're on a 32-byte word boundary ?
	nop
	lhu	t0,0(a0)	# load 16-bit halfword
	daddu	a0,2		# inc src address to get 32-bit word aligned
	daddu	v0,t0		# accumulate checksum
	subu	a1,2		# decrement byte count
 #
 # Arrive here with a 32-bit word aligned address in a0
 #
3:	blt	a1,4,75f	# branch if NOT enough byte count
	and	v1,a1,0x2	# Test for at least 2-bytes left

	lwu	t0,0(a0)	# load 32-bit word
	daddu	a0,4		# incr src address to get 32-bit word aligned
	daddu	v0,t0		# accumulate checksum
	subu	a1,4		# decrement byte count

	blt	a1,4,75f	# branch if NOT enough byte count
	and	v1,a1,0x2	# Test for at least 2-bytes left

	and	v1,a0,0x4	# check for not yet 32-byte aligned address
	beq	v1,zero,31f	# branch if on 64-bit word boundary ?
	nop

	lwu	t0,0(a0)	# load 32-bit word
	daddu	a0,4		# inc src address to get 32-bit word aligned
	daddu	v0,t0		# accumulate checksum
	subu	a1,4		# decrement byte count
 #
 # Fold via a couple of additions the 64-bit checksum down into 16-bits
 # before starting the main-loop to ensure a correct checksum
 #
31:	dsrl32	v1, v0, 0	# v1 gets top 32-bits of checksum rt-justifed
	lui	t3, 0xffff	# t3 gets 48-bit mask in top bits
	dsrl32	t3,t3, 0	# t3 now has 32-bit mask in bottom bits
	and	v0, v0, t3	# v0 gets bottom 32-bits of checksum
	daddu	v0, v0, v1	# v0 is sum of the two 32-bit halves

	dsrl32	t0, v0, 0	# t0 gets carry bits of 32-bit addition result
	addu	v0, v0, t0	# v0 now has chksum with carry if any existed

	dsrl	v1, v0,16	# v1 gets top 16-bits of 32-bit chksum rt-just
	and	v1, 0xffff	# save 16-bits worth
	and	v0, 0xffff	# v0 gets bottom 16-bits, right-justified

	daddu	v0, v0, v1	# Add two 16-bit halves in case we had a carry
	dsrl	t0, v0, 16	# t0 gets carry bits of 16-bit addition result
	and	t0, 0xffff	# save only 16-bits worth of carry bits
	daddu	v0, v0, t0	# v0 now has chksum plus carry if any existed
	and	v0, 0xffff	# v0 to keep bottom 16-bits

4:	blt	a1, 128,60f	# enough data for 128-byte loop iteration ?
	and	v1,a1,0x40	# Check for 64-bytes left
 #
 # Main loop retires 128 byte per iteration
 # Loop costs 4 instr's per 64-bit Word, 1 ld and 3 math instructions.
 # An unrolled lhu loop also costs 4 instr's per word, but 2 are loads.
 # NOTE: The starting address in a0 MUST be 64-bit aligned when we get here.
 #
5:
#ifdef R10000
	pref	0,384(a0)	# Prefetch 3 cache lines ahead ala 384 bytes
#endif /* R10000 */
	ld	t0,0(a0)	# on 64-bit word boundary so start inner loop
	daddu	v0,t0		# Add to previous checksum
	sltu	v1,v0,t0	# Test for carry in the addition
	daddu	v0,v1		# Add the carry if there was any

	ld	t1,8(a0)	# Load 64-bit word to add to running checksum
	daddu	v0,t1		# Add to previous checksum
	sltu	v1,v0,t1	# Test for carry and set v1 appropriately
	daddu	v0,v1		# Add carry if one existed

	ld	t2,16(a0)
	daddu	v0,t2
	sltu	v1,v0,t2
	daddu	v0,v1

	ld	t3,24(a0)
	daddu	v0,t3
	sltu	v1,v0,t3
	daddu	v0,v1

	ld	t0,32(a0)
	daddu	v0,t0
	sltu	v1,v0,t0
	daddu	v0,v1

	ld	t1,40(a0)
	daddu	v0,t1
	sltu	v1,v0,t1
	daddu	v0,v1

	ld	t2,48(a0)
	daddu	v0,t2
	sltu	v1,v0,t2
	daddu	v0,v1

	ld	t3,56(a0)
	daddu	v0,t3
	sltu	v1,v0,t3
	daddu	v0, v1

	ld	t0,64(a0)	# Do bytes 64-95 from here down
	daddu	v0,t0
	sltu	v1,v0,t0
	daddu	v0, v1

	ld	t1,72(a0)
	daddu	v0,t1
	sltu	v1,v0,t1
	daddu	v0, v1

	ld	t2,80(a0)
	daddu	v0,t2
	sltu	v1,v0,t2
	daddu	v0, v1

	ld	t3,88(a0)
	daddu	v0,t3
	sltu	v1,v0,t3
	daddu	v0, v1

	ld	t0,96(a0)       # Do bytes 96-128 from here down
	daddu	v0,t0
	sltu	v1,v0,t0
	daddu	v0, v1

	ld	t1,104(a0)
	daddu	v0,t1
	sltu	v1,v0,t1
	daddu	v0, v1

	ld	t2,112(a0)
	daddu	v0,t2
	sltu	v1,v0,t2
	daddu	v0, v1

	ld	t3,120(a0)
	daddu	v0,t3
	sltu	v1,v0,t3
	daddu	v0, v1

	subu	a1,128		# subtract 128 bytes from length

	bge	a1,128,5b	# Go back if len >= 128 bytes left to checksum
	daddu	a0,128		# increment src address by 128-bytes

	beq	a1, zero,80f	# Exit if length exact multiple of 128-bytes
	and	v1,a1,0x40	# Check for 64-bytes left
 #
 # Arrive here with < 128 bytes remaining so check if multiple of 64 remain
 # NOTE: The starting address in a0 MUST be 64-bit aligned as always
 #
60:	beq	v1,zero,70f	# Branch if less than 64-bytes remain
	and	v1,a1,0x20	# Check for 32-bytes left
	
	ld	t0,0(a0)	# on 64-bit word boundary so start inner loop
	daddu	v0,t0		# Add to previous checksum
	sltu	v1,v0,t0	# Test for carry in the addition
	daddu	v0,v1		# Add the carry if there was any

	ld	t1,8(a0)	# Load 64-bit word to add to running checksum
	daddu	v0,t1		# Add to previous checksum
	sltu	v1,v0,t1	# Test for carry and set v1 appropriately
	daddu	v0,v1		# Add carry if one existed

	ld	t2,16(a0)
	daddu	v0,t2
	sltu	v1,v0,t2
	daddu	v0,v1

	ld	t3,24(a0)
	daddu	v0,t3
	sltu	v1,v0,t3
	daddu	v0,v1

	ld	t0,32(a0)
	daddu	v0,t0
	sltu	v1,v0,t0
	daddu	v0,v1

	ld	t1,40(a0)
	daddu	v0,t1
	sltu	v1,v0,t1
	daddu	v0,v1

	ld	t2,48(a0)
	daddu	v0,t2
	sltu	v1,v0,t2
	daddu	v0,v1

	ld	t3,56(a0)
	daddu	v0,t3
	sltu	v1,v0,t3
	daddu	v0, v1
	
	subu	a1,64		# subtract 64 bytes from length
	daddu	a0,64		# increment src address by 64-bytes

	beq	a1, zero,80f	# Exit if length exact multiple of 64-bytes
	and	v1,a1,0x20	# Check for 32-bytes left

70:	beq	v1,zero,71f	# Branch if less than 32-byte remain
	and	v1,a1,0x10	# Check for 16-bytes left
 #
 # Handle tail case where at least 32-bytes remain after the main loop executed
 #
	ld	t0, 0(a0)	# on 64-bit word boundary so do 32-byte
	daddu	v0, t0		# Add to previous checksum
	sltu	v1, v0, t0	# Test for carry in the addition
	daddu	v0, v1		# Add the carry if there was any

	ld	t1, 8(a0)	# Load 64-bit word to add to running checksum
	daddu	v0, t1		# Add to previous checksum
	sltu	v1, v0, t1	# Test for carry and set register appropriately
	daddu	v0, v1		# Add carry if one existed

	ld	t2, 16(a0)
	daddu	v0, t2
	sltu	v1, v0, t2
	daddu	v0, v1

	ld	t3, 24(a0)
	daddu	v0, t3
	sltu	v1, v0, t3
	daddu	v0, v1
	daddu	a0,32		# increment src address

	subu	a1,32		# subtract 32 bytes from length
	beq	a1, zero,80f	# Exit stage-right if no more bytes to checksum

	and	v1,a1,0x10	# Check for 16-bytes left
71:	beq	v1,zero,72f	# Branch if less than 16-bytes remain
	and	v1,a1,0x8	# Check for 8-bytes left
 #
 # Handle tail case where at least 16-bytes remain
 #
	ld	t0, 0(a0)	# on 64-bit word boundary so do 32-byte
	daddu	v0, t0		# Add to previous checksum
	sltu	v1, v0, t0	# Test for carry in the addition
	daddu	v0, v1		# Add the carry if there was any

	ld	t1, 8(a0)	# Load 64-bit word to add to running checksum
	daddu	v0, t1		# Add to previous checksum
	sltu	v1, v0, t1	# Test for carry and set register appropriately
	daddu	v0, v1		# Add carry if one existed
	daddu	a0,16		# increment src address

	subu	a1,16		# subtract 16 bytes from length
	beq	a1, zero,80f	# Exit stage-right if no more bytes to checksum
 #
 # Handle tail case where at least 8-bytes remain
 #
	and	v1,a1,0x8	# Check for 8-bytes left
72:	beq	v1,zero,73f	# Branch if less than 8-bytes remain
	and	v1,a1,0x4	# Check for 4-bytes left

	ld	t0, 0(a0)	# on 64-bit word boundary so do 8-byte
	daddu	a0,8		# increment src address
	daddu	v0, t0		# Add to previous checksum
	sltu	v1, v0, t0	# Test for carry in the addition
	daddu	v0, v1		# Add the carry if there was any

	subu	a1,8		# subtract 8 bytes from length
	beq	a1, zero,80f	# Exit stage-right if no more bytes to checksum
 #
 # Handle tail case where at least 4-bytes remain
 #
	and	v1,a1,0x4	# Check for 4-bytes left
73:	beq	v1,zero,75f	# Branch if less than 4-bytes remain
	and	v1,a1,0x2	# Check for 2-bytes left

74:	lwu	t0,0(a0)	# Load 32-bit word of trailing bytes
	daddu	a0,4		# increment src address
	daddu	v0, t0		# Add to previous checksum
	sltu	v1, v0, t0	# Test for carry in the addition
	daddu	v0, v1		# Add the carry if there was any

	subu	a1,4		# subtract 4 bytes from length
	beq	a1, zero,80f	# Exit stage-right if no more bytes to checksum
 #
 # Handle tail case where at least 2-bytes remain
 #
	and	v1,a1,0x2	# Check for 2-bytes left
75:	beq	v1,zero,77f	# Branch if less than 2-bytes remain
	and	v1,a1,0x1	# Check for one byte left

	lhu	t0,0(a0)	# Load 16-bit word of trailing bytes
	daddu	a0,2		# increment src address
	daddu	v0, t0		# Add to previous checksum
	sltu	v1, v0, t0	# Test for carry in the addition
	daddu	v0, v1		# Add the carry if there was any

	subu	a1,2		# subtract 2 bytes from length
	beq	a1, zero,80f	# Exit stage-right if no more bytes to checksum
 #
 # Handle tail case where one byte remains
 #
	and	v1,a1,0x1	# Check for one byte left
77:	beq	v1,zero,80f	# Branch if nothing remains
	nop

	lbu	t0,0(a0)	# load the trailing byte
	daddu	a0,1		# incr src address
#ifdef MIPSEB
	dsll	t0,8		# justify for big-endian machines
#endif MIPSEB
	daddu	v0,t0		# add in the odd byte
	sltu	v1, v0, t0	# v1 gets any carry generated from the addition
	daddu	v0, v1		# Add the carry if there was any
 #
 # Fold via the appropriate additions the 64-bit checksum into 16-bits
 #
80:	dsrl32	v1, v0, 0	# v1 gets top 32-bits of checksum rt-justifed
	lui	t3, 0xffff	# t3 gets 48-bit mask in top bits
	dsrl32	t3,t3, 0	# t3 now has 32-bit mask in bottom bits
	and	v0, v0, t3	# v0 gets bottom 32-bits of checksum
	daddu	v0, v0, v1	# v0 is sum of the two 32-bit halves

	dsrl32	t0, v0, 0	# t0 gets carry bits of 32-bit addition result
	addu	v0, v0, t0	# v0 now has chksum with carry if any existed

	dsrl	v1, v0,16	# v1 gets top 16-bits of 32-bit chksum rt-just
	and	v1, 0xffff	# save 16-bits worth
	and	v0, 0xffff	# v0 gets bottom 16-bits, right-justified

	daddu	v0, v0, v1	# Add two 16-bit halves in case we had a carry
	dsrl	t0, v0, 16	# t0 gets carry bits of 16-bit addition result
	and	t0, 0xffff	# save only 16-bits worth of carry bits
	daddu	v0, v0, t0	# v0 now has chksum plus carry if any existed
	j	ra		# exit
	and	v0, 0xffff	# v0 to keep bottom 16-bits
	END(in_cksum_sub)
#else
/*
 * This assembly code ASSUMES the ucode compilers are used since it uses
 * those conventions for temporary registers.
 *
 * in_cksum_sub(addr, len, prev_chksum)
 * unsigned int *addr, len, prev_chksum;
 *
 * Calculates a 16 bit ones-complement checksum.
 *
 * NOTE: This procedure always adds even address bytes to the high order
 * 8 bits of the 16 bit checksum and odd address bytes are added to the
 * low order 8 bits of the 16 bit checksum.
 *
 * Main loop costs 4 instr's per word, 1 lw and 3 math instructions
 * An unrolled lhu loop also costs 4 instr's per word, but 2 are loads.
 *
 * a0 = 32-bit Address of buffer to checksum
 * a1 = 32-bit byte count of buffer
 * a2 = 32-bit integer with bottom 16-bits containing previous checksum
 */
LEAF(in_cksum_sub)
	.set noreorder
	move	v0,a2		# copy previous checksum
	bne	a1,zero,1f	# zero count so left stage left quickly
	and	v1,a0,3		# Starting address on a 32-bit word boundary ?
	j	ra		# exit
	and	v1,a0,3		# Test for address on a 32-bit word boundary ?
 #
 # Arrive here to handle the leading bytes prior to the full loop
 #
1:      and     v1,a0,3         # Starting address on a 32-bit word boundary ?
        beq     v1,zero,3f      # branch if on word boundary to wrap carries
        and     v1,a0,1         # check if byte address to start computation
        beq     v1,zero,2f      # branch if already on a halfword boundry ?
        nop

        lbu     t8,0(a0)        # fetch the byte
        addu    a0,1            # increment source address by one
#ifdef MIPSEL
        sll     t8,8            # shift left on little-endian machines
#endif MIPSEL
        addu    v0,t8           # add byte to existing checksum in v0
        sltu    v1,v0,t8        # check for carry in the addition
        addu    v0,v1           # Add the carry if there was one
        b       1b
        subu    a1,1            # decrement byte count in branch delay slot
 #
 # Arrive here to handle the leading bytes starting
 # on a 16-bit halfword boundary up to the 32-bit word boundary address
 #
2:	blt	a1, 2, 77f	# check for case when zero or one byte left
	and	v1,a1,0x1	# Check for one byte left

	lhu	t0,0(a0)	# load halfword
	addu	a0,2		# increment src address for 16-bit word

	addu	v0,t0		# add halfword to checksum in v0
	sltu	v1,v0,t0	# check for carry in the addition
	addu	v0,v1		# Add the carry if there was one
	subu	a1,2		# decrement count
 #
 # Arrive here to fold checksum into 16-bit after handling leading bytes
 #
3:	srl	v1,v0,16	# add in all previous wrap around carries
	and	v0,0xffff	# save bottom 16-bits of checksum
	addu	v0,v1		# add the two 16-bit halves together
	srl	v1,v0,16	# wrap-arounds could cause carry, also
	addu	v0,v1		# may happen so add the carry in that case
	and	v0,0xffff	# Reduce the checksum to 16-bits
 #
 # Arrive here with starting address on a 32-bit word boundary
 #
4:	blt	a1, 32, 70f	# enough bytes to do at least one iteration ?
	and	v1,a1,0x10	# Check for 16-bytes left
 #
 # Main loop costs 4 instr's per word, 1 lw and 3 math instructions.
 # NOTE: An unrolled lhu loop also costs 4 instr's per word, but 2 are loads.
 #
5:	lw	t0, 0(a0)	# Begin inner loop on 32-bit word boundary
	lw	t1, 4(a0)	# Load 8 32-bit words to compute ip checksum
	lw	t2, 8(a0)
	lw	t3, 12(a0)
	lw	t4, 16(a0)
	lw	t5, 20(a0)
	lw	t6, 24(a0)
	lw	t7, 28(a0)

	addu	v0, t0		# Do the 32-bit addition for the checksum
	sltu	v1, v0, t0	# Test for carry and set register appropriately
	addu	v0, v1		# Add carry if one existed

	addu	v0, t1
	sltu	v1, v0, t1
	addu	v0, v1

	addu	v0, t2
	sltu	v1, v0, t2
	addu	v0, v1

	addu	v0, t3
	sltu	v1, v0, t3
	addu	v0, v1

	addu	v0, t4
	sltu	v1, v0, t4
	addu	v0, v1

	addu	v0, t5
	sltu	v1, v0, t5
	addu	v0, v1

	addu	v0, t6
	sltu	v1, v0, t6
	addu	v0, v1

	addu	v0, t7
	sltu	v1, v0, t7
	addu	v0, v1

	subu	a1, 32		# subtract 32 bytes from length
	bge	a1, 32, 5b	# Go back if len >= 32-bytes left to checksum
	addu	a0, 32		# increment src address by 32-bytes

 	bne	a1, zero, 70f	# Go to tail code if loop count NOT exhausted
	and	v1,a1,0x10	# Check for 16-bytes left
 #
 # Arrive here when byte count was an exact multiple of 32 bytes.
 # This is the fast exit case so now fold/add the 64-bit accumulated
 # checksum back into 16-bits to return.
 #
	srl	v1,v0,16	# add in all previous wrap around carries
	and	v0,0xffff
	addu	v0,v1
	srl	v1,v0,16	# wrap-arounds could cause carry
	addu	v0,v1		# so add in any posible carry
	j	ra		# exit
	and	v0,0xffff	# ensure returned checksum is only 16-bits
 #
 # Handle tail case where at least 16-bytes remain
 #
70:	beq	v1,zero,72f	# go on checking end-loop byte count cases
	and	v1,a1,0x8	# Check for 8-bytes left

	lw	t0, 0(a0)	# on 32-bit word boundary so do 16-bytes
	lw	t1, 4(a0)	# Load 32-bit word to add to running checksum
	lw	t2, 8(a0)	# on 32-bit word boundary so do 32-byte
	lw	t3, 12(a0)	# Load 32-bit word to add to running checksum
	addu	a0,16		# increment src address

	addu	v0, t0		# Add to previous checksum
	sltu	v1, v0, t0	# Test for carry and set register appropriately
	addu	v0, v1		# Add the carry if one was generated

	addu	v0, t1		# Add to previous checksum
	sltu	v1, v0, t1	# Test for carry and set register appropriately
	addu	v0, v1		# Add carry if one was generated

	addu	v0, t2		# Add to previous checksum
	sltu	v1, v0, t2	# Test for carry in the addition
	addu	v0, v1		# Add the carry if one was generated

	addu	v0, t3		# Add to previous checksum
	sltu	v1, v0, t3	# Test for carry and set register appropriately
	addu	v0, v1		# Add carry if one was generated
 #
 # Handle tail case where at least 8-bytes remain
 #
	and	v1,a1,0x8	# Check for 8-bytes left
72:	beq	v1,zero,73f	# go on checking end-loop byte count cases
	and	v1,a1,0x4	# Check for 4-bytes left

	lw	t0, 0(a0)	# get 4 bytes
	lw	t1, 4(a0)	# get another 4 bytes
	addu	a0,8		# increment src address

	addu	v0, t0		# Add to previous checksum
	sltu	v1, v0, t0	# Test for carry in the addition
	addu	v0, v1		# Add the carry if there was any

	addu	v0, t1		# Add to previous checksum
	sltu	v1, v0, t1	# Test for carry in the addition
	addu	v0, v1		# Add the carry if there was any

 #
 # Handle tail case where at least 4-bytes remain
 #
	and	v1,a1,0x4	# Check for 4-bytes left
73:	beq	v1,zero,75f	# go on checking end-loop byte count cases
	and	v1,a1,0x2	# Check for 2-bytes left

	lw	t0,0(a0)	# Load 32-bit word of trailing bytes
	addu	a0,4		# increment src address

	addu	v0, t0		# Add to previous checksum
	sltu	v1, v0, t0	# Test for carry in the addition
	addu	v0, v1		# Add the carry if there was any
 #
 # Handle tail case where at least 2-bytes remain
 #
	and	v1,a1,0x2	# Check for 2-bytes left
75:	beq	v1,zero,77f	# go on checking end-loop byte count cases
	and	v1,a1,0x1	# Check for one byte left

	lhu	t0,0(a0)	# Load 16-bit word of trailing bytes
	addu	a0,2		# increment src address

	addu	v0, t0		# Add to previous checksum
	sltu	v1, v0, t0	# Test for carry in the addition
	addu	v0, v1		# Add the carry if there was any
 #
 # Handle tail case where 1-byte remains
 #
	and	v1,a1,0x1	# Check for one byte left
77:	beq	v1,zero,80f	# go on checking end-loop byte count cases
	nop

	lbu	t0,0(a0)	# load the trailing byte
	addu	a0,1		# incr src address
#ifdef MIPSEB
	sll	t0,8		# justify for big-endian machines
#endif MIPSEB
	addu	v0,t0		# add in the odd byte
	sltu	v1, v0, t0	# v1 gets any carry generated from the addition
	addu	v0, v1		# Add the carry if there was any

80:	srl	v1,v0,16	# add in all previous wrap around carries
	and	v0,0xffff
	addu	v0,v1
	srl	v1,v0,16	# wrap-arounds could cause carry, also
	addu	v0,v1
	j	ra
	and	v0,0xffff	# ensure it's only 16-bit value returned
	END(in_cksum_sub)
 #
 # end of (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
 #
#endif
