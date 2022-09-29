/* :set ts=8 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <regdef.h>
#include <asm.h>
#include "blink.h"


/* blink2(int x)
 *   blink two digits on the led in the following format
 *   blink green, blink amber with the most significant digit, repeat 
 *   end with a green on
 *   ie 
 *	blink(32)
 *		green amber amber amber green amber amber green
 *
 * Notes: s0 - blink #, s1 - delay, trash s0-s4 
 */
LEAF(blink2)
	.set noreorder
	.set noat

.align 2
#ifdef ODD_INST
	nop
#endif

	/* LI	s2, PHYS_TO_K1(HPC3_WRITE1)	*/
	lui	s3, 0x9000
	nop
	dsll	s3, 16
	nop
	dsll	s3, 16
	nop
	lui	s2, 0x1fbd
	nop
	ori	s2, 0x9870
	nop
	or	s2, s3, s2
	nop

	/* divide up number */
	ori	s3, zero, 10
	nop
	div	zero, s0, s3			# don't add any extra div code	
	nop
	mfhi	s0				# remainder
	nop
	mflo	s4				# quotient
	nop

number:	
	move	s3, zero			# turn off led
	nop
	sw	s3, 0(s2)
	nop

	move	s3, s1				# delay
	nop
92:	subu	s3, 1
	nop
	bnez	s3, 92b
	nop					# BDSlot

	ori	s3, zero, 0x10			# green led
	nop
	sw	s3, 0(s2)
	nop

	move	s3, s1				# delay
	nop
93:	subu	s3, 1
	nop
	bnez	s3, 93b
	nop

count:	
	move	s3, zero			# turn off led
	nop
	sw	s3, 0(s2)
	nop

	move	s3, s1				# delay
	nop
94:	subu	s3, 1
	nop
	bnez	s3, 94b
	nop					# BDSlot


	beq	s4, zero, fini			# are we finished with number
	nop
	subu	s4, 1
	nop

	ori	s3, zero, 0x20			# amber led
	nop
	sw	s3, 0(s2)

	move	s3, s1				# delay
	nop
95:	subu	s3, 1
	nop
	bnez	s3, 95b
	nop

	b	count
	nop

fini:
	ori	s3, zero, 10
	nop
	beq	s0, s3, done			# if true, done
	nop
	move	s4, s0
	nop
	move	s0, s3
	nop
	j	number
	nop
	
done:
	ori	s3, zero, 0x10			# turn on green led
	nop
	sw	s3, 0(s2)
	nop

	move	s3, s1				# delay
	nop
96:	subu	s3, 1
	nop
	bnez	s3, 96b
	nop

	j	ra
	nop

	.set	reorder
	.set	at
	END(blink2)


/* check to see if t2 contains either all 5's or all a's depending on the
 * LSBit of a0 (0-a, 1-5), trash s5-s7 */

LEAF(check)
	.set	noreorder
	.set	noat

.align 2
#ifdef ODD_INST
	nop
#endif

	ori	s5, zero, 1		# place 1 into s5
	nop

repeat:
	andi	s6, t2, 1		# place LSBit into s6 
	nop
	xor	s7, a0, s6		# is it correct?
	nop
	beqz	s7, next		# if so, next bit
	nop
	
	/* at this point, we have a bad bit */
	move	s0, s5			# blink bit
	nop
	lui	s1, WAITCOUNT
	nop
	jal	blink2
	nop
	
next:
	addi	s5, s5, 1		# add 1 to bit count
	nop
	ori	s6, zero, 64		# 64 bit count max 
	nop
	beq	s5, s6, cfini		# have we completed all the bits yet
	nop
	dsrl	t2, t2, 1		# next bit
	nop
	xori	a0, a0, 1		# flip bit
	nop
	j	repeat
	nop

cfini:
	j	ra
	nop

	.set	reorder
	.set	at
	END(check)

