/***********************************************************************\
*       File:           libasm.s                                        *
*                                                                       *
*       Contains subroutines which allow C functions to manipulate      *
*       the machine's low-level state.  In some cases, these            *
*       functions do little more than provide a C interface to          *
*       the machine's native assembly language.                         *
*                                                                       *
\***********************************************************************/


#ident "$Revision: 1.36 $"

#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include "ip19prom.h"
#include "pod.h"

/*
 * 	Because current versions of the C compiler can't
 *      deal with 64-bit transfers, we need to provide the following
 *      glue routines.  64-bit PIOs have the advantage of being 
 *      endian-independent, so we use them a lot.
 */

/* The following routines replace the isc_* routines.  The R4000 can't
 * isolate its caches so all we need are routines to execute the appropriate
 * load and store instructions.  These routines don't depend on the size of
 * int char and long, and they don't rely on volatile.  Doubleword versions
 * have also been added for the r4k.
 */

/* Load unsigned byte */
LEAF(load_ubyte)
	.set 	noreorder
	j	ra
	lbu	v0,0(a0)	# (BD)
	END(load_ubyte)

/* Load unsigned half */
LEAF(load_uhalf)
	.set 	noreorder
	j	ra
	lhu	v0,0(a0)	# (BD)
	END(load_uhalf)

/* Load unsigned word */
LEAF(load_uword)
	.set 	noreorder
	j	ra
	lwu	v0,0(a0)	# (BD)
	END(load_uword)

LEAF(load_word)
	.set 	noreorder
	j	ra
	lw	v0,0(a0)	# (BD)
	END(load_word)

LEAF(load_double)
	.set 	noreorder
	j	ra
	ld	v0,0(a0)	# (BD)
	END(load_double)

LEAF(load_double_los)
	.set 	noreorder
	ld	v0,0(a0)	# Load the actual value
	nop			# Hang out for a cycle
	dsll	v0,32		# Wipe out the top 32 bits
	j	ra
	dsra	v0, 32		# Move the low 32 bits back, sign extending
	END(load_double_los)

LEAF(load_double_lo)
	.set 	noreorder
	ld	v0,0(a0)	# Load the actual value
	nop			# Hang out for a cycle
	dsll	v0,32		# Wipe out the top 32 bits
	j	ra
	dsrl	v0,32		# Move the low 32 bits back into position
	END(load_double_lo)

LEAF(load_double_hi)
	.set 	noreorder
	ld	v0,0(a0)
	nop
	j	ra
	dsrl	v0,32		# (BD)
	END(load_double_hi)

LEAF(load_double_bloc)
	.set	noreorder
	ld	v0,0(a0)
	nop
	dsll	v0, 16
	dsll	v0, 8
	dsrl	v0, 16
	j	ra
	dsrl	v0, 16
	END(load_double_bloc)

LEAF(store_byte)
	.set 	noreorder
	j	ra
	sb	a1,0(a0)	# (BD)
	END(store_byte)

LEAF(store_half)
	.set 	noreorder
	j	ra
	sh	a1,0(a0)	# (BD)
	END(store_half)

LEAF(store_word)
	.set 	noreorder
	j	ra
	sw	a1,0(a0)	# (BD)
	END(store_word)

LEAF(store_double_lo)
	.set 	noreorder
	j	ra
	sd	a1,0(a0)	# (BD)
	END(store_double_lo)

LEAF(break_point)
	.set	noreorder
	break	0x4d
	nop
	j	ra
	nop			# (BD)
	END(break_point)

/*
 * load_lwin_half
 * load_lwin_word
 *	Given a region index, these routines read either a halfword or
 * 	a word from the given large window.  They are used primarily
 *	for manipulating the flash eprom.
 * 
 * On entry:
 *	a0 = region #.
 *	a1 = offset within region.
 */

LEAF(load_lwin_half)
	.set 	noreorder
	addi	a0, 0x10	# Convert to physical offset
	dsll	a0, 30		# Shift it up
	add	a0, a1		# Merge in the offset
	lui	a1, 0x9000
	dsll	a1, 16		# Build the uncached xkphys address
	dsll	a1, 16
	add	a0, a1		# Make LWIN into a real address
	j	ra		# Return
	lhu	v0, 0(a0)	# (BD) Perform the actual read
	END(load_lwin_half)

LEAF(load_lwin_word)
	.set	noreorder
	addi	a0, 0x10	# Convert to physical offset
	dsll	a0, 30		# Shift it up
	add	a0, a1		# Merge in the offset
	lui	a1, 0x9000
	dsll	a1, 16		# Build the uncached xkphys address
	dsll	a1, 16
	add	a0, a1		# Make LWIN into a real address
	j	ra
	lw	v0, 0(a0)
	END(load_lwin_word)

/*
 * store_lwin_half
 * store_lwin_word
 *	Given the region of a large window, this routine writes a
 *	halfword to an offset in the large window.  Used primarily
 *	for manipulating the flash eprom.
 *
 * On entry:
 *	a0 = region #.
 *	a1 = offset.
 *	a2 = value to write.
 */

LEAF(store_lwin_half)
	.set	noreorder
	addi	a0, 0x10
	dsll	a0, 30
	add	a0, a1
	lui	a1, 0x9000
	dsll	a1, 16
	dsll	a1, 16
	add	a0, a1
	j	ra
	sh	a2, 0(a0)
	END(store_lwin_half)

LEAF(store_lwin_word)
	.set	noreorder
	addi	a0, 0x10
	dsll	a0, 30
	add	a0, a1
	lui	a1, 0x9000
	dsll	a1, 16
	dsll	a1, 16
	add	a0, a1
	j	ra
	sw	a2, 0(a0)
	END(store_lwin_half)


/*
 * Routine get_char
 * 	Given an address, returns the character at that
 *	address.
 */

LEAF(get_char)
        mfc0    v0, C0_CONFIG
        nop
        srl     v0, CONFIG_BE_SHFT
        and     v0, 1
	beqz	v0, 1f
	nop
	lwr	v0, 0(a0)		# Read the byte (big endian)
	j	ra
	and	v0, 0xff		# (BD) Mask out rest of word 	
1:
	lwl	v0, 0(a0)		# Read the byte (little endian)
	srl	v0, 24			# Shift the byte into place
	j	ra			
	and	v0, 0xff		# (BD) Mask out rest of word 	
END(get_char)

LEAF(get_prid)
	mfc0	v0, C0_PRID
	j	ra
	nop
	END(get_prid)

LEAF(set_BSR)
	.set noreorder
	mtc1	a0, BSR_REG 
	nop
	j	ra
	nop
	END(set_BSR)

LEAF(get_BSR)
	.set noreorder
	mfc1	v0, BSR_REG 
	nop
	j	ra
	nop
	END(get_BSR)

LEAF(set_epcuart_base)
	.set noreorder
	mtc1	a0, DUARTBASE_REG
	nop
	j	ra
	nop
	END(set_epcuart_base)

/*
 * write_earom
 * 	Writes a value into the EAROM in a safe manner.  According
 * 	to the Catalyst specs, it takes up to 10 ms for a write to
 *	complete; as a result, we have to spin reading the value 
 *	until it becomes what we want. 
 */

LEAF(write_earom)
	.set	noreorder
	move	a3, ra

	sd	a1, 0(a0)		# Write out the byte
	nop
	nop
1:
	ld	a2, 0(a0)		# Spin until the write completes 
	nop
	bne	a2, a1, 1b		
	nop

	li	a0, EV_WCOUNT0_LOC	# EAROM Write count address
	ld	v0, 0(a0)		# Get LSB
	ld	v1, 8(a0)		# Get MSB
	andi	v0, 0xff		# Mask off low byte of count
	andi	v1, 0xff		# Mask off high byte of count
	sll	v1, 8			# Shift high byte into position
	or	v0, v0, v1		# Combine bytes
	addi	v0, 1			# Add one
	andi	v1, v0, 0xff		# Mask off low byte

	sd	v1, 0(a0)		# Store it
	nop
	nop
2:
	ld	a2, 0(a0)		# Spin until the write completes
	nop
	bne	a2, v1, 2b
	nop

	srl	v1, v0, 8		# Shift high byte back down
	andi	v1, 0xff		# Mask off extraneous bits

	sd	v1, 8(a0)		# Store high byte (BD)
	nop
	nop
3:
	ld	a2, 8(a0)		# Spin until the write completes
	nop
	bne	a2, v1, 3b
	nop

	j	a3	
	nop	
	END(write_earom)


/*
 * delay for a given number of clicks, where a click is defined
 * as being rough 950 nanoseconds (really 1/(2^20) seconds).
 */

#define CLICK_LENGTH	50

LEAF(delay)
	.set	noreorder
	li	a1, EV_RTCFREQ0_LOC	# Load real time clock freq addr
	ld	a2, 24(a1)		# Read MSB of RTC frequency
	nop
	nop
	ld	a3, 16(a1)		# Load byte of rtc freq.
	sll	a2, 8			# Make space for it.
	or	a2, a3			# Merge it in
	ld	a3, 8(a1)		# Load byte of rtc freq.
	sll	a2, 8			# Make space for it
	or	a2, a3			# Merge it in
	ld	a3, 0(a1)		# Load LSB of RTC freq.
	sll	a2, 8			# Make space for it
	or	a2, a3			# Merge it in
	bne	a2, zero, 1f		# Test if RTC freq. is zero 
	srl	a2, a2, 20		# Divide by 2^20
	li	a2, CLICK_LENGTH 	# Set default RTC freq.
1:
	dmultu	a0, a2			# Multiply #clicks by click length
	nop
	nop
	mflo	a0			# Grab the delay time
	li	a1, EV_RTC
	ld	a2, 0(a1)		# Read the real time clock
	nop
	addu	a0, a2			# Calculate the delay time

2:
	ld	a3, 0(a1)		# Read the Real time clock
	nop
	subu	a3, a0			# Compute time difference
	bltz	a3, 2b
	nop

	li	a1, EV_LED_BASE
	li	a2, 4
	sd	a2, 0(a1)
	nop

	j	ra			# Return
	nop
	END(delay)


/*
 * clrmem(lo,hi): word clear hi > lo+32 word
 */
LEAF(clrmem)
1:
	sw	zero,0(a0)
	sw	zero,4(a0)
	sw	zero,8(a0)
	sw	zero,12(a0)
	sw	zero,16(a0)
	sw	zero,20(a0)
	sw	zero,24(a0)
	sw	zero,28(a0)
	addu	a0,32			# BDSLOT: inc dst, NOTE after test
	blt	a0,a1,1b
	j	ra
XLEAF(clrmemend)
	END(clrmem)


/*
 * set_SR() -- set the status register, return current sr
 */
LEAF(set_SR)
	.set	noreorder
	mfc0	v0,C0_SR
	nop
	mtc0	a0,C0_SR
	j	ra
	nop
	.set	reorder
	END(set_SR)

LEAF(get_SR)
	.set	noreorder
	mfc0	v0,C0_SR
	nop
	.set	reorder
	j	ra
	END(get_SR)

LEAF( set_Cause )
	.set	noreorder
	mtc0	a0,C0_CAUSE
	nop
	.set	reorder
	j	ra
	END(set_Cause) 

/*
 * invaltlb(i): Invalidate the ith ITLB entry.
 * called whenever a specific TLB entry needs to be invalidated.
 */
LEAF(invaltlb)
	.set	noreorder
	sll	a0,TLBINX_INXSHIFT
	mtc0	a0,C0_INX
	mtc0	zero,C0_TLBLO_0
	mtc0	zero,C0_TLBLO_1
	li	a0,K0BASE&TLBHI_VPNMASK
	mtc0	a0,C0_TLBHI		# invalidate entry
	NOP_0_3
	c0	C0_WRITEI
	j	ra
	nop				# BDSLOT
	.set	reorder
	END(invaltlb)
/* 
	kxset(int kxbit)
		Sets the KX bit in the status register to the value passed in.
*/
LEAF(kxset)
        .set    noreorder
        mfc0    k0, C0_SR		# Get SR value (BD)
        bne     a0, zero, 1f
	nop
        li      a0, ~SR_KX
        and     k0, a0			# Turn off KX bit
        mtc0    k0, C0_SR		# Store to SR
        j       ra
        nop				# (BD)
1:
        ori     k0, SR_KX		# Turn on KX bit
        mtc0    k0, C0_SR		# Store to SR
        j       ra
        nop				# (BD)
        .set    reorder
END(kxset)

/*
        int u64lw(int bloc, int offset)
        a0 = bloc
        a1 = offset
        *v0 := *(bloc <<8 + offset)
*/
							
LEAF(u64lw)
        .set    noreorder
	li	v0, 0x00100000		# Start of processor local stuff
	li	v1, 0xfff00000		# Mask off bottom 20 bits of bloc
	and	v1, v1, a0
	bne	v0, v1, 1f		# Address covered by proc. local stuff?
	lui	v1, 0x0ff0		# Load address of hidden RAM (BD)
	or	a0, v1, a0		# Change address to 40'h0ff0000000)
1:
        dsll	a0, 8                   # a0 := Bits 39-8 of the address (BD)
        daddu	a0, a1                  # a0 := Full physical address

	# dli doesn't work in PROM so load start of uncached kphysseg space
	lui	a1, 0x9000		# a1 = 0x90000000
	dsll	a1, a1, 16		# a1 = 0x900000000000
	dsll	a1, a1, 16		# a1 = 0x9000000000000000

        or      a0, a1, a0              # Uncached address of word
        lw      v0, 0(a0)               # Do load.
        j       ra
        nop				# (BD)
        .set    reorder
END(u64lw)


/*
        int u64sw(int bloc, int offset, int data)
        a0 = bloc
        a1 = offset
	a2 = data
*/

LEAF(u64sw)
        .set    noreorder
	li	v0, 0x00100000		# Start of processor local stuff
	li	v1, 0xfff00000		# Mask off bottom 20 bits of bloc
	and	v1, v1, a0		
	bne	v0, v1, 1f		# Address covered by proc. local stuff?
	lui	v1, 0x0ff0		# Load address of hidden RAM (BD)
	or	a0, v1, a0		# Change address to 40'h0ff0000000)
1:
        dsll	a0, 8			# a0 := Bits 39-8 of the address
        daddu	a0, a1			# a0 := Full physical address

	# dli doesn't work in PROM so load start of uncached kphysseg space
	lui	a1, 0x9000		# a1 = 0x90000000
	dsll	a1, a1, 16		# a1 = 0x900000000000
	dsll	a1, a1, 16		# a1 = 0x9000000000000000

        or      a0, a1, a0              # Uncached address of word
        sw      a2, 0(a0)               # Do store
        j       ra
        nop
        .set    reorder
END(u64sw)


/*
        int u64ld(int bloc, int offset)
        a0 = bloc
        a1 = offset
        *v0 := (int)*(long long)(bloc <<8 + offset)
*/
							
LEAF(u64ld)
        .set    noreorder
	li	v0, 0x00100000		# Start of processor local stuff
	li	v1, 0xfff00000		# Mask off bottom 20 bits of bloc
	and	v1, v1, a0
	bne	v0, v1, 1f		# Address covered by proc. local stuff?
	lui	v1, 0x0ff0		# Load address of hidden RAM (BD)
	or	a0, v1, a0		# Change address to 40'h0ff0000000)
1:
        dsll     a0, 8                  # a0 := Bits 39-8 of the address
        or      a0, a1                  # a0 := Full physical address

	# dli doesn't work in PROM so load start of uncached kphysseg space
	lui	a1, 0x9000		# a1 = 0x90000000
	dsll	a1, a1, 16		# a1 = 0x900000000000
	dsll	a1, a1, 16		# a1 = 0x9000000000000000

        or      a0, a1, a0              # Uncached address of word
        ld      v0, 0(a0)               # Do load.
        j       ra
        nop
        .set    reorder
END(u64ld)


/*
        int u64sd(int bloc, int offset, int data)
        a0 = bloc
        a1 = offset
	a2 = data
*/

LEAF(u64sd)
        .set    noreorder
	li	v0, 0x00100000		# Start of processor local stuff
	li	v1, 0xfff00000		# Mask off bottom 20 bits of bloc
	and	v1, v1, a0
	bne	v0, v1, 1f		# Address covered by proc. local stuff?
	lui	v1, 0x0ff0		# Load address of hidden RAM (BD)
	or	a0, v1, a0		# Change address to 40'h0ff0000000)
1:
        dsll     a0, 8                  # a0 := Bits 39-8 of the address
        or      a0, a1                  # a0 := Full physical address

	# dli doesn't work in PROM so load start of uncached kphysseg space
	lui	a1, 0x9000		# a1 = 0x90000000
	dsll	a1, a1, 16		# a1 = 0x900000000000
	dsll	a1, a1, 16		# a1 = 0x9000000000000000

        or      a0, a1, a0              # Uncached address of word
        sd      a2, 0(a0)               # Do store
        j       ra
        nop
        .set    reorder
END(u64sd)

/* Get the endianness of this cpu from the config register */
LEAF(getendian)
	.set    noreorder
	mfc0    v0,C0_CONFIG
	nop
	srl     v0,CONFIG_BE_SHFT
	and     v0,1
	xor     v0,1                    # big = 0, little = 1
	j       ra
	nop
	.set    reorder
END(getendian)


/*  pod_jump() - jump to an address possibly flushing and invalidating the
 *	caches.
 *  a0: the address to jump to.
 *  a1: first parameter to pass
 *  a2: second parameter to pass
 *  a3: 1 = flush and invalidate caches, 0 = just jump
 *
 *  Return value:
 *	pod_jump is sneaky.  In order to avoid saving its return address, it
 *	does a jump rather than a jump and link to the routine called.  This
 *	means that the return value is whatever the called function leaves in
 *	v0.
 */
LEAF(pod_jump)
	.set noreorder
	beq	a3, zero, 1f		# Flush and invalidate caches?
	move	Ra_save0, ra		# Save the ra (BD)

	jal	pon_flush_dcache
	nop				# (BD)
	jal	pon_flush_scache
	nop				# (BD)

	jal	pon_invalidate_dcache
	nop				# (BD)
	jal	pon_invalidate_scache
	nop				# (BD)

1:
	move	ra, Ra_save0		# Restore ra
	move	t0, a0			# clear out a0
	move	a0, a1			# Set up parameter 1
	move	a1, a2
	move	a2, zero
	j	t0
	move	a3, zero
	# Never returns
	END(pod_jump)


/* a0 = sregs addr
   a1 = function to call
   a2 = paramater
 */
LEAF(save_and_call)
	.set reorder
	addiu	a0, 4
	li	a3, ~7
	and	a0, a3

	sd	s0, 0(a0)
	sd	s1, 8(a0)
	sd	s2, 16(a0)
	sd	s3, 24(a0)
	sd	s4, 32(a0)
	sd	s5, 40(a0)
	sd	s6, 48(a0)
	sd	s7, 56(a0)
	sd	fp, 64(a0)	# fp == s8
	sd	ra, 72(a0)

	move	s7, a0

	move	a0, a2

	jal	a1

	move	a0, s7
	ld	s0, 0(a0)
	ld	s1, 8(a0)
	ld	s2, 16(a0)
	ld	s3, 24(a0)
	ld	s4, 32(a0)
	ld	s5, 40(a0)
	ld	s6, 48(a0)
	ld	s7, 56(a0)
	ld	fp, 64(a0)	# fp == s8
	ld	ra, 72(a0)

	j	ra
END(save_and_call)


LEAF(save_sregs)

	addiu	a0, 4
	li	a3, ~7
	and	a0, a3

	sd	s0, 0(a0)
	sd	s1, 8(a0)
	sd	s2, 16(a0)
	sd	s3, 24(a0)
	sd	s4, 32(a0)
	sd	s5, 40(a0)
	sd	s6, 48(a0)
	sd	s7, 56(a0)
	sd	fp, 64(a0)	# fp == s8

	j	ra

	END(save_sregs)
	
LEAF(restore_sregs)

	addiu	a0, 4
	li	a3, ~7
	and	a0, a3

	ld	s0, 0(a0)
	ld	s1, 8(a0)
	ld	s2, 16(a0)
	ld	s3, 24(a0)
	ld	s4, 32(a0)
	ld	s5, 40(a0)
	ld	s6, 48(a0)
	ld	s7, 56(a0)
	ld	fp, 64(a0)	# fp == s8

	j	ra

	END(restore_sregs)

LEAF(load_gprs)
	ld	r1, R1_OFF(a0)
	ld	r2, R2_OFF(a0)
	ld	r3, R3_OFF(a0)
	/* No point in loading a0 */
	ld	r5, R5_OFF(a0)
	ld	r6, R6_OFF(a0)
	ld	r7, R7_OFF(a0)
	ld	r8, R8_OFF(a0)
	ld	r9, R9_OFF(a0)
	ld	r10, R10_OFF(a0)
	ld	r11, R11_OFF(a0)
	ld	r12, R12_OFF(a0)
	ld	r13, R13_OFF(a0)
	ld	r14, R14_OFF(a0)
	ld	r15, R15_OFF(a0)
	ld	r16, R16_OFF(a0)
	ld	r17, R17_OFF(a0)
	ld	r18, R18_OFF(a0)
	ld	r19, R19_OFF(a0)
	ld	r20, R20_OFF(a0)
	ld	r21, R21_OFF(a0)
	ld	r22, R22_OFF(a0)
	ld	r23, R23_OFF(a0)
	ld	r24, R24_OFF(a0)
	ld	r25, R25_OFF(a0)
	ld	r26, R26_OFF(a0)
	ld	r27, R27_OFF(a0)
	ld	r28, R28_OFF(a0)
	ld	r29, R29_OFF(a0)
	ld	r30, R30_OFF(a0)
	/* Loading ra is a bad idea. */
	j	ra
	END(load_gprs)

LEAF(save_gprs)
	sd	r1, R1_OFF(a0)
	sd	r2, R2_OFF(a0)
	sd	r3, R3_OFF(a0)
	/* No point in saving a0 */
	sd	r5, R5_OFF(a0)
	sd	r6, R6_OFF(a0)
	sd	r7, R7_OFF(a0)
	sd	r8, R8_OFF(a0)
	sd	r9, R9_OFF(a0)
	sd	r10, R10_OFF(a0)
	sd	r11, R11_OFF(a0)
	sd	r12, R12_OFF(a0)
	sd	r13, R13_OFF(a0)
	sd	r14, R14_OFF(a0)
	sd	r15, R15_OFF(a0)
	sd	r16, R16_OFF(a0)
	sd	r17, R17_OFF(a0)
	sd	r18, R18_OFF(a0)
	sd	r19, R19_OFF(a0)
	sd	r20, R20_OFF(a0)
	sd	r21, R21_OFF(a0)
	sd	r22, R22_OFF(a0)
	sd	r23, R23_OFF(a0)
	sd	r24, R24_OFF(a0)
	sd	r25, R25_OFF(a0)
	sd	r26, R26_OFF(a0)
	sd	r27, R27_OFF(a0)
	sd	r28, R28_OFF(a0)
	sd	r29, R29_OFF(a0)
	sd	r30, R30_OFF(a0)
	/* No point in saving ra */
	j	ra
	END(save_gprs)

	.globl	hexdigit
hexdigit:
	.ascii	"0123456789abcdef"
	.globl	crlf
crlf:
	.asciiz	"\r\n"

