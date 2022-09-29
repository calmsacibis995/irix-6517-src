#ident "$Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP32/mem/RCS/khparity.s,v 1.2 1997/05/15 16:08:37 philw Exp $"

/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or	    | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.						    | */
/* ------------------------------------------------------------------ */

#include "sys/asm.h"
#include "sys/regdef.h"
/*#include "machine/mach_ops.h"  */
/*#include "machine/cp0.h" */
#include "sys/cpu.h"
/*#include "machine/cpu_board.h" */
#include "sys/sbd.h"
#define CALL(f)                                                         \
        la      t0,f;                                                   \
        addu    t0,s0;                                                  \
        jal     t0;                                                     \
        nop




oddparity:
		.word	0x01010101
		.word	0x01010101
		.globl	khparity
		.ent	khparity

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

khparity:
		subu	sp,4		# reserve stack frame
		.frame	sp,4,ra
		sw	ra,0(sp)	# save return addr

		.set noreorder		
		mfc0	t3, C0_SR	# save cp0 status reg
		nop
		.set reorder

/*
 * Set partitions 1 and 2 to oddparity
 */
		la	t2, oddparity		# load address of data
		ld	t1, (t2)
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
 * Set partition 0 to ones.(even parity)
 */
$42:
		li	t1,-1			# set expected data
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

$48:
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 1 again
		bleu	t2,a1,$46		# done yet?
/*
 * Set partition 1 to 1's.
 */
		li	t1,-1			# set expected data
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

		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 2 again
		bleu	t2,a1,$52		# done yet?

/*
 * Verify that partitions 0 and 1 are still 1's.
 */
		li	t1,-1			# set expected data
		addu	t2,a0,8			# set address pointer to
						#   partition 1 start
$56:
		ld	t0,-8(t2)		# read partition 0 data

		ld	t0,(t2)			# read partition 1 data

		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 1 again
		bleu	t2,a1,$56		# done yet?

		/*
		 * Check if last word is in partition 0.
		 */

		subu	t2, 8			# drop address pointer to
						#   partition 0
		bne	t2,a1,$62		# is last word in partition 0 ?
		ld	t0,(t2)			# read last word data
/*
 * Set partition 0 to oddparity
 */
$62:
		la	t2, oddparity
		ld	t1, (t2)
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
$68:
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 0 again
		bleu	t2,a1,$66		# done yet?
/*
 * Set partition 2 to 1's
 */
		li	t1,-1			# set data pattern
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
$74:
		addu	t2,t2,24		# bump address pointer 3 words
						#   to partition 2 again
		bleu	t2,a1,$72		# done yet?


		move	v0,zero			# return zero for success


		lw	ra,0(sp)	# restore return addr.
		addu	sp,4		# pop stack frame
		j	ra

		.end	khparity









