/*
 * Miscellaneous assembly language functions
 */
#include "ml.h"
#include <asm.h>
#include <sys/sbd.h>
#include <regdef.h>
#include <setjmp.h>
#include <sys/EVEREST/epc.h>
#include "sl.h"

/*
 * getprid
 *	Get the processor revision id register so we know which segment to
 *	load!
 */
LEAF(getprid)
	.set	noreorder
	nop
	DMFC0(v0, C0_PRID)
	nop; nop; nop; nop
	andi	t0, v0, C0_IMPMASK	# need special case for TFP
	li	t1, 0x1000
	beq	t0, t1, tfp_prid	# go handle TFP encoded revision
	nop
	j	ra
	nop

	/* TFP revision info may be encoded in the IC field of the CONFIG
	 * register, so check for this case and return modified value.
	 */
tfp_prid:
	DMFC0(v0,C0_PRID)
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
	 * 	IC field:  3 7 6 5 4 2 1 0
	 *  	major rev  0 1 1 1 1 1 1 1
	 *      minor rev  0 0 1 2 3 4 5 6	
	 */
revtab:
	.word	0x16151400
	.word	0x13121110
	.text
	END(getprid)

/*
 * getsp
 *	Get the stack pointer
 */
LEAF(getsp)
	.set	noreorder
	j	ra
	move	v0, sp
	.set reorder
	END(getsp)

LEAF(jump_addr)
	.set noreorder
	j	a0
	nop
	.set reorder
	END(jump_addr)
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
	addu	v0, t0
	daddi	a0, 2		# next short
	sh	t0, 0(a2)	# Perform the actual write
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
 * setjmp(jmp_buf) -- save current context for non-local goto's
 * return 0
 */
LEAF(setjmp)
	sd      ra, JB_PC*8(a0)
	sd      sp, JB_SP*8(a0)
	sd      fp, JB_FP*8(a0)
	sd      s0, JB_S0*8(a0)
	sd      s1, JB_S1*8(a0)
	sd      s2, JB_S2*8(a0)
	sd      s3, JB_S3*8(a0)
	sd      s4, JB_S4*8(a0)
	sd      s5, JB_S5*8(a0)
	sd      s6, JB_S6*8(a0)
	sd      s7, JB_S7*8(a0)
	move    v0, zero
	j       ra
	END(setjmp)


/*
 * setfault(jump_buf, *old_buf)
 *      If jump_buf is non-zero, sets up the jumpbuf and switches
 *      on exception handling.
 */
LEAF(setfault)
	.set    noreorder
	.set    noat
	dmfc1	t0, NOFAULT_REG
	nop
	sd      t0, 0(a1)               # Save previous fault buffer

	dmtc1	a0, NOFAULT_REG
	nop
	bnez    a0, setjmp
	nop
	j       ra
	nop
	END(setfault)

/*
 * restorefault(old_buf)
 *      Simply sets NOFAULT_REG to the value passed.  Used to restore
 *      previous fault buffer value.
 */
LEAF(restorefault)
	.set    noreorder
	j       ra
	dmtc1	a0, NOFAULT_REG		# (BD)
	END(restorefault)


.data
	.align 3
	.globl  hexdigit
hexdigit:
	.ascii  "0123456789abcdef"

