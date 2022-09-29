/***********************************************************************\
*       File:           libasm.s                                        *
*                                                                       *
*       Contains subroutines which allow C functions to manipulate      *
*       the machine's low-level state.  In some cases, these            *
*       functions do little more than provide a C interface to          *
*       the machine's native assembly language.                         *
*                                                                       *
\***********************************************************************/


#ident "$Revision: 1.60 $"

#include "ml.h"
#include <asm.h>
#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include "ip21prom.h"
#include "pod.h"


/*
 * If we have a config read  (from physical addr 18008000 to 1800ffff),
 * disable interrupts, then delay in order to allow any pending
 * writebacks to complete (and we must insure that no additional
 * writebacks are initiated).  Then read the config register until
 * we get the same data twice.
 */
#if TFP_CC_REGISTER_READ_WAR
#define CC_READ_WAR			\
	dli	t0, EV_SYSCONFIG;	\
	beq	a0, t0, 7f;		\
	nop;				\
	dli	t0, EV_CONFIGREG_BASE;	\
	dsubu	t0, a0, t0;		\
	bltz	t0, 9f;			\
	nop;				\
	dsubu	t0, 0x08000;		\
	bgez	t0, 9f;			\
	nop;				\
	.align	7;			\
7:					\
	DMFC0(t1, C0_SR);		\
	ori	t0, t1, SR_IE;		\
	xori	t0, SR_IE;		\
	DMTC0(t0, C0_SR);		\
					\
1:	li	t3, 4000;		\
2:	daddi	t3, -1;			\
	bgez	t3, 2b;			\
	NOP_SSNOP;			\
					\
	ld	v0, 0(a0);		\
	dli	t2, EV_RTC;		\
					\
	li	t3, 4000;		\
2:	daddi	t3, -1;			\
	bgez	t3, 2b;			\
	NOP_SSNOP;			\
					\
	ld	t2, (t2);		\
					\
	li	t3, 4000;		\
2:	daddi	t3, -1;			\
	bgez	t3, 2b;			\
	NOP_SSNOP;			\
					\
	ld	t0, 0(a0);		\
					\
	li	t3, 4000;		\
2:	daddi	t3, -1;			\
	bgez	t3, 2b;			\
	NOP_SSNOP;			\
					\
	beq	t0, t2, 1b;		\
	nop;				\
	bne	v0, t0, 1b;		\
	nop;				\
					\
	DMTC0(t1, C0_SR);		\
	b	6f;			\
	nop;				\
9:					\
	ld	v0,0(a0);		\
6:

#else

#define CC_READ_WAR			\
	ld	v0,0(a0)

#endif /* TFP_CC_REGISTER_READ_WAR */

/* The following routines replace the isc_* routines.  The TFP can't
 * isolate its caches so all we need are routines to execute the appropriate
 * load and store instructions.  These routines don't depend on the size of
 * int char and long, and they don't rely on volatile.  Doubleword versions
 * have also been added.
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

LEAF(load_word)
	.set 	noreorder
	j	ra
	lw	v0,0(a0)	# (BD)
	END(load_word)

LEAF(load_double)
	.set 	noreorder
	CC_READ_WAR
	j	ra
	nop
	END(load_double)

LEAF(load_double_los)
	.set 	noreorder
	CC_READ_WAR
	dsll	v0,32		# Wipe out the top 32 bits
	j	ra
	dsra	v0,32		# Move the low 32 bits back, sign extending
	END(load_double_los)

LEAF(load_double_lo)
	.set 	noreorder
	CC_READ_WAR
	dsll	v0,32		# Wipe out the top 32 bits
	j	ra
	dsrl	v0,32		# Move the low 32 bits back into position
	END(load_double_lo)

LEAF(load_double_hi)
	.set 	noreorder
	CC_READ_WAR
	j	ra
	dsrl	v0,32		# (BD)
	END(load_double_hi)

LEAF(load_double_nowar)
	.set 	noreorder
	ld	v0,0(a0)
	j	ra
	nop
	END(load_double_nowar)

LEAF(load_double_los_nowar)
	.set 	noreorder
	ld	v0,0(a0)
	dsll	v0,32		# Wipe out the top 32 bits
	j	ra
	dsra	v0,32		# Move the low 32 bits back, sign extending
	END(load_double_los_nowar)

LEAF(load_double_lo_nowar)
	.set 	noreorder
	ld	v0,0(a0)
	dsll	v0,32		# Wipe out the top 32 bits
	j	ra
	dsrl	v0,32		# Move the low 32 bits back into position
	END(load_double_lo_nowar)

LEAF(load_double_hi_nowar)
	.set 	noreorder
	ld	v0,0(a0)
	j	ra
	dsrl	v0,32		# (BD)
	END(load_double_hi_nowar)

LEAF(load_double_bloc)
	.set	noreorder
	ld	v0, 0(a0)
	dsll	v0, 24
	j	ra
	dsrl	v0, 32		# (BD)
	END(load_double_bloc)

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
	bgt	a3, 0, 1b	# loop if not done
	daddi	a2, 2		# (BD) next short

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
        DMFC0(v0, C0_CONFIG)
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

LEAF(get_prid)
	.set noreorder
	DMFC0(v0, C0_PRID)
	andi	t0, v0, 0xff
	bne	t0, zero, 9f	# if revision non-zero, go report it
	nop
	
	/* Revision field is zero, use bits in IC field of CONFIG register
	 * to correctly determine revision.
	 */
	DMFC0(t0,C0_CONFIG)
	and	t1, t0, CONFIG_IC
	dsrl	t1, CONFIG_IC_SHFT	# IC field now isolated
	lb	t2, revtab(t1)		# load minor revision
	daddu	v0, t2			# add into implementation
9:
	.set	reorder
	j	ra
	.rdata
	/* Sequence of minor revisions from IC field of CONFIG register:
	 * 	IC field:  0 1 2 3 4 5 6 7
	 *  	major rev  0 0 0 1 2 0 2 1
	 *      minor rev  0 0 0 1 2 0 1 2
	 */
revtab:
	.word	0x00000011
	.word	0x22002112
	.text
	END(get_prid)

LEAF(set_BSR)
	.set noreorder
	DMTC0(a0, BSR_REG)
	j	ra
	nop
	END(set_BSR)

LEAF(get_BSR)
	.set noreorder
	DMFC0(v0, BSR_REG)
	j	ra
	nop
	END(get_BSR)

LEAF(set_epcuart_base)
	.set noreorder
	DMTC0(a0, DUARTBASE_REG)
	j	ra
	nop
	END(set_epcuart_base)

/*
 * delay for a given number of clicks, where a click is defined
 * as being rough 950 nanoseconds (really 1/(2^20) seconds).
 */

LEAF(delay)
	.set	noreorder
	dli	a2, EV_RTCFREQ
	srl	a2, a2, 20		# Divide by 2^20
1:
	dmultu	a0, a2			# Multiply #clicks by click length
	nop
	nop
	mflo	a0			# Grab the delay time
	dli	a1, EV_RTC
	ld	a2, 0(a1)		# Read the real time clock
	nop
	daddu	a0, a2			# Calculate the delay time

2:
	ld	a3, 0(a1)		# Read the Real time clock
	nop
	dsubu	a3, a0			# Compute time difference
#ifndef SABLE
	bltz	a3, 2b
	nop
#endif

	dli	a1, EV_LED_BASE
	li	a2, 4
	sd	a2, 0(a1)
	nop

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
        dsll	a0, 8                   # a0 := Bits 39-8 of the address (BD)
        daddu	a0, a1                  # a0 := Full physical address
	LI	a1, K0BASE
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
        dsll	a0, 8			# a0 := Bits 39-8 of the address
        daddu	a0, a1			# a0 := Full physical address
	LI	a1, K0BASE
        or      a0, a1, a0              # Uncached address of word
        sw      a2, 0(a0)               # Do store
        j       ra
        nop
        .set    reorder
	END(u64sw)


/* Get the endianness of this cpu from the config register */
LEAF(getendian)
	.set    noreorder
	DMFC0(v0,C0_CONFIG)
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

	beq	a3, zero, 1f		# Flush and invalidate caches?
	move	Ra_save0, ra		# Save the ra (BD)

	jal	pon_invalidate_IDcaches
	nop				# (BD)

	/* Margie added to allow multiple runs of niblet */
	jal flush_entire_tlb
	nop
1:
	move	ra, Ra_save0		# Restore ra
	move	a0, s7			# Set up parameters
	move	a1, s8
#if 0
sw zero, 0(zero)
#endif
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

LEAF(load_gprs)
	ld	AT, R1_OFF(a0)
	ld	v0, R2_OFF(a0)
	ld	v1, R3_OFF(a0)
	/* No point in loading a0 */
	ld	a1, R5_OFF(a0)
	ld	a2, R6_OFF(a0)
	ld	a3, R7_OFF(a0)
	ld	ta0, R8_OFF(a0)
	ld	ta1, R9_OFF(a0)
	ld	ta2, R10_OFF(a0)
	ld	ta3, R11_OFF(a0)
	ld	t0, R12_OFF(a0)
	ld	t1, R13_OFF(a0)
	ld	t2, R14_OFF(a0)
	ld	t3, R15_OFF(a0)
	ld	s0, R16_OFF(a0)
	ld	s1, R17_OFF(a0)
	ld	s2, R18_OFF(a0)
	ld	s3, R19_OFF(a0)
	ld	s4, R20_OFF(a0)
	ld	s5, R21_OFF(a0)
	ld	s6, R22_OFF(a0)
	ld	s7, R23_OFF(a0)
	ld	t8, R24_OFF(a0)
	ld	t9, R25_OFF(a0)
	ld	k0, R26_OFF(a0)
	ld	k1, R27_OFF(a0)
	ld	gp, R28_OFF(a0)
	ld	sp, R29_OFF(a0)
	ld	fp, R30_OFF(a0)
	/* Loading ra is a bad idea. */
	j	ra
	END(load_gprs)

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

	.globl	hexdigit
hexdigit:
	.ascii	"0123456789abcdef"
	.globl	crlf
crlf:
	.asciiz	"\r\n"
	.globl  spc
spc:
	.asciiz "  "
