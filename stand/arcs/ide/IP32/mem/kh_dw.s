#ident "$Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP32/mem/RCS/kh_dw.s,v 1.2 1997/05/15 16:08:31 philw Exp $"

/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or	    | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.						    | */
/* ------------------------------------------------------------------ */

#include "sys/asm.h"
#include "sys/regdef.h"
#include "sys/sbd.h"
/*
#include "machine/mach_ops.h"
#include "machine/cp0.h"
#include "machine/cpu.h"
#include "machine/cpu_board.h"
*/

		.globl	kh_dw

		.align	3
		.comm	dwd_wrtbuf, 8
		.comm	dwd_rdbuf, 8
		.comm	dwd_tmpbuf, 8

		.ent	kh_dw

onespat:
		.word	0xaaaaaaaa
		.word	0xaaaaaaaa

/************************************************************************
*   kh_dw()								*
*	kh_dw() test the D-cache with the Knaizuk Hartmann 		*
*	algorithm.							*
*	register usage:							*
*		a0: beginning word address				*
*		a1: ending word address					*
*									*
*		t0: actual data read from memory location		*
*		t1: zero or -1 data pattern				*
*		t2: variable address pointer				*
*		t4: error code 						*
*									*
*		v0: zero for pass or t4 for error code			*
************************************************************************/
	.text

	kh_dw:

		subu	sp,4		# reserve stack frame
		.frame	sp,4,ra
		sw	ra,0(sp)	# save return addr

		.set noreorder		
		mfc0	t3, C0_SR	# save cp0 status reg
		nop
		.set reorder

/*
 * Set partitions 1 and 2 to 0's.
 */
		move	t1,zero			# set expected data
		addu	t2,a0,16		# initialize address pointer to
						#   partition 2
$40:
		sd	t1,-8(t2)		# write partition 1 data
		sd	t1,(t2)			# write partition 2 data 
		addu	t2,t2,24		# bump address 3 double words to
						#   next partition 2 
		bleu	t2,a1,$40		# done yet?
		

		/*
		 * Check if last word is in partition 1.
		 */
		subu	t2,t2,8			# drop address pointer to
						#   partition 1
		bne	t2,a1,$42		# is last word in partition 1
		sd	t1,(t2)			# write to partition 1
/*
 * Set partition 0 to ones.
 */
$42:
		la	t5, onespat		# set expected data
		ld	t1, (t5)
		move	t2,a0			# re-initialize pointer
						# to partition 0
$44:
		sd	t1,(t2)			# write partition 0 data
		addu	t2,t2,24		# bump pointer address up 3
						#   double words to partition 0 again
		bleu	t2,a1,$44		# done yet?
/*
 * Verify partition 1 is still 0's.
 */
		move	t1,zero			# set expected data
		addu	t2,a0,8			# re-initialize pointer to
						#   partition 1
$46:
		ld	t0,(t2)			# read data
		beq	t0,t1,$48		# is it still 0?

		li	t4,1
		b	$76			# branch on error.
$48:
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 1 again
		bleu	t2,a1,$46		# done yet?
/*
 * Set partition 1 to 1's.
 */
		la	t5, onespat		# set expected data
		ld	t1, (t5)
		addu	t2,a0,8			# set address pointer to
						#   partition 1 start
$50:
		sd	t1,(t2)			# write ones
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 1 again
		bleu	t2,a1,$50		# done yet?

/*
 * Verify that partition 2 is 0's.
 */
		move	t1,zero			# set expected data
		addu	t2,a0,16		# re-initialize address pointer
						#   to partition 2
$52:
		ld	t0,(t2)			# read location
		beq	t0,t1,$54		# is it still 0?

		li	t4,2
		b	$76			# branch on error.
$54:
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 2 again
		bleu	t2,a1,$52		# done yet?

/*
 * Verify that partitions 0 and 1 are still 1's.
 */
		la	t5, onespat		# set expected data
		ld	t1, (t5)
		addu	t2,a0,8			# set address pointer to
						#   partition 1 start
$56:
		ld	t0,-8(t2)		# read partition 0 data
		bne	t0,t1,$58		# still 1's?

		ld	t0,(t2)			# read partition 1 data
		bne	t0,t1,$59		# still 1's?

		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 1 again
		bleu	t2,a1,$56		# done yet?
		b	$60
$58:
		subu	t2,t2,8			# set t2 to the failing addr
$59:
		li	t4,3
		b	$76
$60:
		/*
		 * Check if last word is in partition 0.
		 */

		subu	t2, 8			# drop address pointer to
						#   partition 0
		bne	t2,a1,$62		# is last word in partition 0 ?
		ld	t0,(t2)			# read last word data
		bne	t0,t1,$59		# branch on error.
/*
 * Set partition 0 to 0's.
 */
$62:
		move	t1,zero			# set expected data
		move	t2,a0			# re-initialize pointer to
						#   partition 0
$64:
		sd	t1,(t2)			# write partition 0 data
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 0 again
		bleu	t2,a1,$64		# done yet?

/*
 * Check partition 0 for 0's.
 */
		move	t2,a0			# re-initialize address pointer
						#   to partition 0
$66:
		ld	t0,(t2)			# read partition 0 data
		beq	t0,t1,$68		# is it still 0?

		li	t4,4
		b	$76
$68:
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 0 again
		bleu	t2,a1,$66		# done yet?
/*
 * Set partition 2 to 1's
 */
		la	t5, onespat		# set expected data
		ld	t1, (t5)
		addu	t2,a0,16		# re-initialize address pointer
						#   to partition 2
$70:
		sd	t1,(t2)			# write partition 2 data
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 2 again
		bleu	t2,a1,$70		# done yet?

/*
 * Check partition 2 for 1's.
 */
		addu	t2,a0,16		# re-initialize address pointer
						#   to partition 2
$72:
		ld	t0,(t2)			# read partition 2 data
		beq	t0,t1,$74		# is it still 1's?

		li	t4,5
		b	$76
$74:
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 2 again
		bleu	t2,a1,$72		# done yet?

		move	v0,zero			# return zero for success


		lw	ra,0(sp)	# restore return addr.
		addu	sp,4		# pop stack frame
		j	ra
$76:
		.set noreorder
		mtc0	t3,C0_SR	# restore to cp0
		nop
		.set reorder
		la	a0, dwd_tmpbuf
		sw	t2,(a0)		# failing address
		la	a0, dwd_wrtbuf	# 
		sd	t1, (a0)	# expected data
		la	a0, dwd_rdbuf	# 
		sd	t0, (a0)	# actual failing data
		move	v0,t4		# error code

		lw	ra,0(sp)	# restore return addr.
		addu	sp,4		# pop stack frame
		j	ra

		.end	kh_dw

