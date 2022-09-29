/***********************************************************************\
*	File:		mdir_asm.s					*
*									*
*	This file contains mdir routines in handcoded			*
*	assembly so that they run as fast as possible.			*
*									*
*	Would it be possible to use the BTE to further speed them up?	*
*									*
\***********************************************************************/

#ident	"$Revision: 1.9 $"

#include <asm.h>

#include <sys/regdef.h>
#include <sys/sbd.h>

#include "mdir.h"

	.text
	.set	at
	.set	reorder

#define T_S4D(r, A, B, C, D)			\
	.set	noreorder;			\
	sd	A, 0(r);			\
	sd	B, 8(r);			\
	sd	C, 16(r);			\
	sd	D, 24(r);			\
	.set	reorder

#define T_L4D(r, A, B, C, D)			\
	.set	noreorder;			\
	ld	A, 0(r);			\
	ld	B, 8(r);			\
	ld	C, 16(r);			\
	ld	D, 24(r);			\
	.set	reorder

#define T_S4W(r, A, B, C, D)			\
	.set	noreorder;			\
	sw	A, 0(r);	/* Skips */	\
	sw	B, 8(r);	/* every */	\
	sw	C, 16(r);	/* other */	\
	sw	D, 24(r);	/* word. */	\
	.set	reorder

/*
 * mdir_bddir_fill
 *
 *  Writes a specified value into each directory/protection
 *  entry in a specified range.
 *
 *  a0  End address in back door space, multiple of 1024
 *  a1  End address in back door space, multiple of 1024
 *  a2  Value to write into all directory entry low words
 *  a3  Value to write into all directory entry high words
 *  a4  Value to write into all protection entries
 */

LEAF(mdir_bddir_fill)
1:
	bgeu	a0, a1, 10f		# Check if done

	li	v0, 64			# Fill 64 double-word prot. entries
2:
	T_S4D(a0, a4, a4, a4, a4)	# Unrolled 4 times

	daddu	a0, 32
	sub	v0, 4
	bnez	v0, 2b

	li	v0, 32			# Fill 32 quad-word dir. entries
3:
	T_S4D(a0, a2, a3, a2, a3)	# Unrolled 2 times

	daddu	a0, 32
	sub	v0, 2
	bnez	v0, 3b

	b	1b			# Go back for another block
10:
	j	ra
	END(mdir_bddir_fill)

/*
 * mdir_bdecc_fill
 *
 *  Writes a specified value into an area of ECC memory (every other word).
 *
 *  a0  Start address in back door space, multiple of 32
 *  a1  End address in back door space, multiple of 32
 *  a2  Value to write into each ECC word
 */

LEAF(mdir_bdecc_fill)
	bgeu	a0, a1, 10f		# Pre-check if done
1:
	T_S4W(a0, a2, a2, a2, a2)	# Unrolled 4 times

	daddu	a0, 32			# (Every other word)
	bltu	a0, a1, 1b

10:
	j	ra
	END(mdir_bdecc_fill)

/*
 * mdir_copy_prom
 *
 *   Copies PROM data to a specific memory address.
 *
 *   a0  Source address in PROM (word-aligned)
 *   a1  Destination address in RAM (word-aligned)
 *   a2  Length in bytes (will be rounded up to a multiple of 64)
 */

LEAF(mdir_copy_prom)
	beqz	a2, 8f			# Done if NULL copy

	daddu	a2, 63			# Round up to a multiple of 64
	and	a2, ~63

1:
	ld	t0, 0(a0)
	ld	t1, 8(a0)
	ld	t2, 16(a0)
	ld	t3, 24(a0)
	ld	ta0, 32(a0)
	ld	ta1, 40(a0)
	ld	ta2, 48(a0)
	ld	ta3, 56(a0)

	daddu	a0, 64

	sd	t0, 0(a1)
	sd	t1, 8(a1)
	sd	t2, 16(a1)
	sd	t3, 24(a1)
	sd	ta0, 32(a1)
	sd	ta1, 40(a1)
	sd	ta2, 48(a1)
	sd	ta3, 56(a1)

	daddu	a1, 64

	dsubu	a2, 64
	bnez	a2, 1b
8:
	j	ra
	END(mdir_copy_prom)
