/***********************************************************************\
*       File:           libasm.s                                        *
*                                                                       *
*       Contains subroutines which allow C functions to manipulate      *
*       the machine's low-level state.  In some cases, these            *
*       functions do little more than provide a C interface to          *
*       the machine's native assembly language.                         *
*                                                                       *
\***********************************************************************/


#ident "$Revision: 1.5 $"
	
#include <asm.h>
	
#include <sys/mips_addrspace.h>
#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>

#include <sys/EVEREST/IP25.h>

#include "ml.h"
#include "ip25prom.h"
#include "pod.h"

LEAF(initCPUSpeed)
/*
 * Function:	initCPUSpeed
 * Purpose: 	Determines the speed of the current in MHz.
 * Parameters:	none
 * Returns:	Nothing
 */
#if	!defined(SABLE)
        .set	noreorder
#       define	NSEC_PER_SEC	1000000000 /* nano-seconds per second */
        dli	a0,EV_RTC
        ld	a1,0(a0)
        daddu	a1,CYCLE_PER_SEC/8	/* about 1/8 secs should be good */
        MTC0(zero, C0_COUNT)		/* Start counting */
1:
        ld	v0,0(a0)		/* Watch it count ... */
        blt	v0,a1,1b
        nop
        MFC0(v0, C0_COUNT)		/* Pick up new count */

        /*
	 * Now multiply by 8 (<< 3) to get number of count register
	 * tics per second, each of which corresponds to 2 processor
	 * clocks. Divide by 1 MHz to get MHz / 2. This leaves 
	 * the computation at (X << 3) / 1000000. We store MHz/2 to
	 * allow a 300MHz number to fit in the same "byte" field.
	 */
	dsll	v0,3
	ddivu	v0,1000000
	mflo	v0
        j	ra
        nop
        .set	reorder
#else
	j	ra
	ori	v0,zero,100
#endif
        END(initCPUSpeed)

LEAF(getHexChar)
/*
 * Function:	getHexChar
 * Purpose:	Convert binary hex to hex character.
 * Parameters:	a0 - Binary number
 * Returns:	v0 - Character representing lower 4-bits of a0.
 */
	.set	noreorder
	and	a0,0xf		/* Isolate character */
	bgt	a0,9,1f		/* Check for character */
	nop
	jr	ra
	add	v0,a0,0x30	/* + '0' */
1:	jr	ra
	add	v0,a0,0x61-0xa	/* + 'a' */
	END(getHexChar)

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
	daddi	a0, 0x10	# Convert to physical offset
	dsll	a0, 30		# Shift it up
	dadd	a0, a1		# Merge in the offset
	lui	a1, 0x9000
	dsll	a1, 32		# Build the uncached xkphys address
	dadd	a0, a1		# Make LWIN into a real address
	j	ra		# Return
	lhu	v0, 0(a0)	# (BD) Perform the actual read
	END(load_lwin_half)

LEAF(load_lwin_word)
	.set	noreorder
	daddi	a0, 0x10	# Convert to physical offset
	dsll	a0, 30		# Shift it up
	dadd	a0, a1		# Merge in the offset
	lui	a1, 0x9000
	dsll	a1, 32		# Build the uncached xkphys address
	dadd	a0, a1		# Make LWIN into a real address
	j	ra
	lw	v0, 0(a0)
	END(load_lwin_word)

/*
 * Same as above, but also stores at a2 for a3 counts.
 * Returns checksum.
 * For fast IO4prom copy.
 */
LEAF(load_lwin_half_store_mult)
	.set 	noreorder
	move	v0, zero	# init checksum
	daddi	a0, 0x10	# Convert to physical offset
	dsll	a0, 30		# Shift it up
	dadd	a0, a1		# Merge in the offset
	lui	a4, 0x9000
	dsll	a4, 32		# Build the uncached xkphys address
	dadd	a0, a4		# Make LWIN into a real address
1:
	lhu	t0, 0(a0)	# Perform the actual read
	sh	t0, 0(a2)	# Perform the actual write
	addu	v0, t0
	daddi	a0, 2		# next short
	addi	a3, -2		# decrement count
	daddi	a2, 2		# (BD) next short
	bgt	a3, 0, 1b	# loop if not done
	nop
	j	ra		# Return
	nop
	END(load_lwin_half_store_mult)

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
	daddi	a0, 0x10
	dsll	a0, 30
	dadd	a0, a1
	lui	a1, 0x9000
	dsll	a1, 32
	dadd	a0, a1
	j	ra
	sh	a2, 0(a0)
	END(store_lwin_half)

LEAF(store_lwin_word)
	.set	noreorder
	daddi	a0, 0x10
	dsll	a0, 30
	dadd	a0, a1
	lui	a1, 0x9000
	dsll	a1, 32
	dadd	a0, a1
	j	ra
	sw	a2, 0(a0)
	END(store_lwin_word)

/*
 * store_load_lwin_word
 *	Given the region of a large window, this routine writes a
 *	word to an offset in the large window and then reads it
 *	back. Used for speeding up the POD tests.
 *
 * On entry:
 *	a0 = region #.
 *	a1 = offset.
 *	a2 = value to write.
 */

LEAF(store_load_lwin_word)
	.set	noreorder
	daddi	a0, 0x10
	dsll	a0, 30
	dadd	a0, a1
	lui	a1, 0x9000
	dsll	a1, 32
	dadd	a0, a1
	sw	a2, 0(a0)
	j	ra
	lw	v0, 0(a0)
	END(store_load_lwin_word)

/*
 * Routine get_char
 * 	Given an address, returns the character at that
 *	address.
 */

LEAF(get_char)
        MFC0(v0, C0_CONFIG)
        dsrl    v0, CONFIG_BE_SHFT	# shift BE to bit position 0	
        and     v0, 1
	beqz	v0, 1f
	nop
	lwr	v0, 0(a0)		# Read the byte (big endian)
	j	ra
	and	v0, 0xff		# (BD) Mask out rest of word 	
1:
	lwl	v0, 0(a0)		# Read the byte (little endian)
	dsrl	v0, 24			# Shift the byte into place
	j	ra			
	and	v0, 0xff		# (BD) Mask out rest of word 	
	END(get_char)

LEAF(set_BSR)
	.set noreorder
	DMTBR(a0, BR_BSR)
	j	ra
	nop
	END(set_BSR)

LEAF(get_BSR)
	.set noreorder
	DMFBR(v0, BR_BSR)
	j	ra
	nop
	END(get_BSR)

LEAF(get_ERTOIP)
	.set noreorder
	j	ra
	DMFBR(v0, BR_ERTOIP)
	.set	reorder
	END(get_ERTOIP)

LEAF(set_ERTOIP)
	.set noreorder
	j	ra
	DMTBR(a0, BR_ERTOIP)
	.set	reorder
	END(set_ERTOIP)	

LEAF(set_epcuart_base)
	.set noreorder
	DMTBR(a0, BR_DUARTBASE)
	j	ra
	nop
	END(set_epcuart_base)

LEAF(get_epcuart_base)
	.set noreorder
	DMFBR(v0, BR_DUARTBASE)
	j       ra
	nop
END(get_epcuart_base)


/*
 * delay for a given number of clicks, where a click is defined
 * as being rough 950 nanoseconds (really 1/(2^20) seconds).
 */

LEAF(delay)
	.set	noreorder
	dli	a2, 47619048 >> 20	/* Tics per us */
1:
	dmultu	a0, a2			/* Delay time */
	mflo	a0			/* Grab RTC tics */
	
	dli	a1, EV_RTC
	ld	a2, 0(a1)		# Read the real time clock
	daddu	a0, a2			# Calculate the delay time
2:
	ld	a2, 0(a1)		# Read the Real time clock
	dsubu	a2, a0			# Compute time difference
#ifndef SABLE
	bltz	a2, 2b
	nop
#endif
	j	ra			# Return
	nop
	END(delay)

/*
        int u64lw(int bloc, int offset)
        a0 = bloc
        a1 = offset
        *v0 := *(bloc <<8 + offset)
*/
							
LEAF(u64lw)
        .set    noreorder
	li	v0, 0x00100000
	and	v1, a0, 0xfff00000
	bne	v0,v1,1f
	lui	v1, 0x0ff0		/* DELAY */
	or	a0,v1
1:	
        dsll	a0, 8                   # a0 := Bits 39-8 of the address (BD)
        daddu	a0, a1                  # a0 := Full physical address
	LI	a1, K1BASE
        or      a0, a1, a0              # Uncached address of word
        j       ra
        lw      v0, 0(a0)               # Do load.
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
	li	v0, 0x00100000
	and	v1, a0, 0xfff00000
	bne	v0,v1,1f
	lui	v1, 0x0ff0		/* DELAY */
	or	a0,v1
1:	
        dsll	a0, 8			# a0 := Bits 39-8 of the address
        daddu	a0, a1			# a0 := Full physical address
	LI	a1, K1BASE
        or      a0, a1, a0              # Uncached address of word
        j       ra
        sw      a2, 0(a0)               # Do store
        .set    reorder
	END(u64sw)


/* Get the endianness of this cpu from the config register */
LEAF(getendian)
	.set    noreorder
	MFC0(v0,C0_CONFIG)
	dsrl     v0,CONFIG_BE_SHFT	# shift BE to bit position 0
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
	move	s6, a0			# Save args
	move	s7, a1			# Kludge: this code knows what regs
	move	s8, a2			# are not trashed by cache code
	move	Ra_save0, ra		# Save the ra (BD)

	beq	a3, zero, 1f		# Flush and invalidate caches?
	nop
	jal	invalidateCCtags
	nop
	jal	invalidateScache
	nop
	jal	invalidateIDcache
	nop				# (BD)

	/* Margie added to allow multiple runs of niblet */
	jal flushTlb
	nop
1:
	move	ra, Ra_save0		# Restore ra
	move	a0, s7			# Set up parameters
	move	a1, s8
	move	a2, zero
	j	s6
	move	a3, zero		# (BD)
	# Never returns
	END(pod_jump)


/* a0 = sregs addr
   a1 = function to call
   a2 = paramater
 */
LEAF(save_and_call)
	.set reorder
	daddiu	a0, 4
	dli	a3, ~7
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

	daddiu	a0, 4
	dli	a3, ~7
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

	daddiu	a0, 4
	dli	a3, ~7
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

LEAF(save_gprs)
	sd	AT, R1_OFF(a0)
	sd	v0, R2_OFF(a0)
	sd	v1, R3_OFF(a0)
	/* No point in saving a0 */
	sd	a1, R5_OFF(a0)
	sd	a2, R6_OFF(a0)
	sd	a3, R7_OFF(a0)
	sd	ta0, R8_OFF(a0)
	sd	ta1, R9_OFF(a0)
	sd	ta2, R10_OFF(a0)
	sd	ta3, R11_OFF(a0)
	sd	t0, R12_OFF(a0)
	sd	t1, R13_OFF(a0)
	sd	t2, R14_OFF(a0)
	sd	t3, R15_OFF(a0)
	sd	s0, R16_OFF(a0)
	sd	s1, R17_OFF(a0)
	sd	s2, R18_OFF(a0)
	sd	s3, R19_OFF(a0)
	sd	s4, R20_OFF(a0)
	sd	s5, R21_OFF(a0)
	sd	s6, R22_OFF(a0)
	sd	s7, R23_OFF(a0)
	sd	t8, R24_OFF(a0)
	sd	t9, R25_OFF(a0)
	sd	k0, R26_OFF(a0)
	sd	k1, R27_OFF(a0)
	sd	gp, R28_OFF(a0)
	sd	sp, R29_OFF(a0)
	sd	fp, R30_OFF(a0)
	/* No point in saving ra */
	j	ra
	END(save_gprs)

        .data
	.globl	hexdigit
hexdigit:
	.ascii	"0123456789abcdef"
