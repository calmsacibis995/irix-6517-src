/*
 * Fast bcopy code which supports overlapped copies.
 *
 * Written by: Kipp Hickman
 * Completely rewritten by: Iain McClatchie 8/25/94
 *
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/TFP/RCS/tfpbcopy.s,v $
 * $Revision: 1.12 $
 * $Date: 1995/04/19 06:02:47 $
 */

#if IP26
#include <sys/cpu.h>
#endif
#include <asm.h>
#include <regdef.h>

/* It turns out better to think of LWS/LWB and SWS/SWB as
 *	smaller-vs-bigger address rather than left-vs-right.
 *	Such a representation makes the code endian-independent. 
 */

#ifdef _MIPSEB
#    define LWS lwl
#    define LWB lwr
#    define LDS ldl
#    define LDB	ldr
#    define SWS swl
#    define SWB swr
#    define SDS	sdl
#    define SDB	sdr
#else
#    define LWS lwr
#    define LWB lwl
#    define LDS	ldr
#    define LDB	ldl
#    define SWS swr
#    define SWB swl
#    define SDS	sdr
#    define SDB	sdl
#endif /* _MIPSEB */

/*
 * char *bcopy(from, to, count);
 *	unsigned char *from, *to;
 *	unsigned long count;
 *
 * OR
 *
 * void *memcpy/memmove(to, from, count);
 *	void *to, *from;
 *	unsigned long count;
 *
 * Both functions return "to"
 * 
 * Zero length copy is now handled by the short copy code.
 * Copies on top of one another are handled correctly and
 * not special cased.
 */

/* MINCOPY must be at least 7 due to speculative loads in the
 * windup and winddown code. */
#define	MINCOPY	8

/* registers used */


LEAF(bcopy)
XLEAF(ovbcopy)

#define	from	a0
#define	to	a1
#define	count	a2
#define fromend	t8
#define toend	t9

/* Register usage:
 * v0 - 
 * v1 - 
 * a0 - from/to
 * a1 - to/from
 * a2 - count
 * a3 - 
 * a4 - 
 * a5 - 
 * a6 - Stop condition for unaligned inner loop
 * a7 - Stop condition for unaligned inner loop
 * t0 -
 * t1 - 
 * t2 - 
 * t3 - saved return value
 * t8 - fromend = from + count
 * t9 - toend = to + count
 */
#if IP26
	andi	v1,a1,TCC_LINESIZE-1
 	andi	v0,a0,TCC_LINESIZE-1
	andi	t0,a2,(4*TCC_LINESIZE)-1
	or	v0,v0,v1
	or	v0,v0,t0
	bnez	v0,1f
	j	_pagecopy
1:
#endif	/* IP26 */
	.set	noreorder
	slt	t0,from,to		# [0]
	PTR_ADDU fromend,from,count	# [0]
	PTR_ADDU toend, to, count	# [1]

/* Use backwards copying code if to > from.  No more complicated
 * decision is needed because backwards copies are as fast as forwards
 * copies.  (Actually, I think they take 1 or 2 more cycles.)
 */
	bne	t0,zero,gobackwards	# [1]
	move	t3,to			# [2] Save to in t3

/*****************************************************************************/

/*
 * Forward copy code.  Align destination pointer to 8 byte boundary, check
 * if this also aligns source pointer.
 */
goforwards:
	/* small byte counts use byte at a time copy */
	and	v1,to,7			# [0] v1 := to & 7
	blt	count,MINCOPY,forwards_bytecopy # [0]
/* Regardless of whether the copy is aligned or not, we want to start
 * out by aligning the destination, bumping from and to, and decrementing
 * the count.  There are at least 8 bytes to copy, so we can safely
 * use SDS to retire the unaligned part of the destination.
 */
	and	v0,from,7		# [1] v0 := from & 7
	LDS	a4,0(from)		# [1] Can't do this load until we're sure count > 0
	LDB	a4,7(from)		# [2]
	subu	a3,v1,8			# [2] a3 = - (# bytes to get aligned)
	PTR_SUBU from, a3		# [2]
	SDS	a4,0(to)		# [3]
	PTR_SUBU to, a3			# [3]
	addu	count, a3		# [3]
	and	t1, count, 7		# [4]
	bne	v0,v1,fwd_unaligned	# [4] low bits are different
	sub	a3, count, t1		# [5]
	move	v0,zero			# [5]
	beq	a3,zero,forwards_last7	# [6] go do last 7 bytes
	li	v1,8			# [6]

/*
 * Once we are here, the pointers are aligned on 8 byte boundaries,
 * and there are at least 8 bytes left to copy.
 * Begin copying in 8B chunks, with an FP register.
 */

	.align	4
forwards_8:
	ldxc1	$f4, v0(from)		# [0]
	sdxc1	$f4, v0(to)		# [0]
	beq	v1, a3, forwards_last7	# [0]
	LONG_ADDU	v0,16		# [0]
	ldxc1	$f4, v1(from)		# [1]
	sdxc1	$f4, v1(to)		# [1]
	bne	v0, a3, forwards_8	# [1]
	LONG_ADDU	v1,16		# [1]

/*
 * We are now guaranteed that there are 0 to 7 bytes left to be copied,
 * and both pointers are aligned.
 * We generate the address of the last byte, and let half of a load
 * and store unaligned do the work.
 */	
forwards_last7:
	beq	t1, zero, ret		# [0]
	LDB	t0, -1(fromend)		# [0]	This is okay, we know count was originally >= 1
	SDB	t0, -1(toend)		# [1]
	j	ra			# [1]
	move	v0, t3			# [1]

/*
 * Forward large unaligned copy code.  At this point, the destination
 * pointer is aligned.
 */
	.align	4
fwd_unaligned:	
	beq	a3, zero, fwd_unalign_last7	# [0]
	PTR_ADDU a6, from, a3			# [0]
	li	a4, 8				# [1]
	and	v0, from, 7			# [1]
	sub	v0, a4, v0			# [2]
	and	a7, count, ~15			# [2]
	PTR_ADDU t0, from, v0			# [3]
	sll	v0,3				# [4]
	PTR_ADDU a7, from, a7			# [4]
	move	t1, to				# [5]
	PTR_ADDU t2, to, 16			# [5]
/* This loop should do unaligned copies at 8 bytes/2 cycles on TFP.
 * count is number of bytes to copy.  count >= 8
 * to   is 8 byte aligned destination
 * t1	is 8 byte aligned destination, pointer gets incremented.
 * t2	is t1+16
 * v0	is (8-from&7) == alignment amount
 * from is unaligned source
 * t0	is from+v0
 * a6	is from + (count & ~7), stop condition for 16n byte copies
	doesn't match if (count & 8) == 8
 * a7	is from + (count & ~15), stop condition for 16n+8 byte copies
	if (count & 8) == 0, this would match, but a6 matches first
 */
 
	ld	a4, 0(t0)			# [6]
	beq	from, a7, fwd_unalign_loop_end0 #[6]
	dsrlv	a4, a4, v0			# [6]
	.align	4
fwd_unalign_loop:
	ldl	a4, 0(from)			# [0]
	ld	a5, 8(t0)			# [0]
	PTR_ADDU	from, 16		# [0]
	nada
	sd	a4, 0(t1)			# [1]
	dsrlv	a5, a5, v0			# [1]
	beq	from, a6, fwd_unalign_loop_end1 #[1]
	PTR_ADDU	t1, 32			# [2]
	nada
	ldl	a5, -8(from)			# [2]
	ld	a4, 16(t0)			# [2]
	sd	a5, -8(t2)			# [3]
	beq	from, a7, fwd_unalign_loop_end2 #[3]
	dsrlv	a4, a4, v0			# [3]
	ldl	a4, 0(from)			# [4]
	ld	a5, 24(t0)			# [4]
	PTR_ADDU	t0, 32			# [4]
	PTR_ADDU	from, 16		# [4]
	sd	a4, 0(t2)			# [5]
	dsrlv	a5, a5, v0			# [5]
	beq	from, a6, fwd_unalign_loop_end3 #[5]
	PTR_ADDU	t2, 32			# [6]
	nada
	ldl	a5, -8(from)			# [6]
	ld	a4, 0(t0)			# [6]
	sd	a5, -8(t1)			# [7]
	bne	from, a7, fwd_unalign_loop	# [7]
	dsrlv	a4, a4, v0			# [7]

	# These jump targets are automatically aligned because the loop has a multiple of
	# four instructions.
fwd_unalign_loop_end0:
	ldl	a4, 0(from)		# [0]
	sd	a4, 0(t1)		# [1]
	b	fwd_unalign_last7	# [1]
	and	v0, count, 7		# [1]

fwd_unalign_loop_end1:
	ldl	a5, -8(from)		# [0]
	sd	a5, -24(t1)		# [1]
	b	fwd_unalign_last7	# [1]
	and	v0, count, 7		# [1]
	
fwd_unalign_loop_end2:
	ldl	a4, 0(from)		# [0]
	sd	a4, -16(t1)		# [1]
	b	fwd_unalign_last7	# [1]
	and	v0, count, 7		# [1]
	
	nop	# This aligns the fwd_unalign_last7 jump target
fwd_unalign_loop_end3:
	ldl	a5, -8(from)		# [0]
	and	v0, count, 7		# [0]
	sd	a5, -8(t1)		# [1]
	
/* Now we have [0,7] bytes to move, the destination pointer is aligned,
 * and the source pointer is unaligned.  Since we may have some overlap,
 * it is not legal to store over already-copied parts of the destination.
 * Note that since "count" has not been properly decremented, only the
 * bottom three bits (in v0) are valid.  One sdr should do it. */

	.align 4
fwd_unalign_last7:
	LDS	a4, -8(fromend)		# [0]	This is okay, we know count was originally >= 7
	and	v0, count, 7		# [0]
	beq	v0, zero, ret		# [1]
	LDB	a4, -1(fromend)		# [1]	This is okay, we know count was originally >= 7
	SDB	a4, -1(toend)		# [2]
	j	ra			# [2]
	move	v0, t3			# [2]
	
/*
 * Byte at a time copy code.  This is used when 0 <= byte count < MINCOPY(8).
 * This was difficult enough to get to 1 byte/cycle that completely unrolling
 * it just seemed easier.
 */
	.align	4
	nop
forwards_bytecopy:
	beq	count, zero, ret	# [0]
	PTR_ADDU	t0,from,1	# [0]
	lb	v0,0(from)		# [0]
	sb	v0,0(to)		# [1]
	beq	t0,fromend,ret		# [1]
	PTR_ADDU	t0,1		# [1]
	lb	v0,1(from)		# [1]
	sb	v0,1(to)		# [2]
	beq	t0,fromend,ret		# [2]
	PTR_ADDU	t0,1		# [2]
	lb	v0,2(from)		# [2]
	sb	v0,2(to)		# [3]
	beq	t0,fromend,ret		# [3]
	PTR_ADDU	t0,1		# [3]
	lb	v0,3(from)		# [3]
	sb	v0,3(to)		# [4]
	beq	t0,fromend,ret		# [4]
	PTR_ADDU	t0,1		# [4]
	lb	v0,4(from)		# [4]
	sb	v0,4(to)		# [5]
	beq	t0,fromend,ret		# [5]
	PTR_ADDU	t0,1		# [5]
	lb	v0,5(from)		# [5]
	sb	v0,5(to)		# [6]
	beq	t0,fromend,ret		# [6]
	nop				# [6]
	lb	v0,6(from)		# [6]
	sb	v0,6(to)		# [7]
ret:	j	ra			# [0] return to caller
	move	v0,t3			# [0] Set v0 to old "to" pointer


/*****************************************************************************/

/*
 * Backward copy code.  Check for pointer alignment and try to get both
 * pointers aligned on a long boundary.
 */
	.align	4
gobackwards:
	/* small byte counts use byte at a time copy */
	blt	count,MINCOPY,backwards_bytecopy # [0]
	and	v1,toend,7			# [0] v1 := toend & 7 (# bytes to get aligned)
	LDS	a4,-8(fromend)		# [0] Can't do this load until we're sure count > 0
	and	v0,fromend,7		# [1] v0 := fromend & 7
	li	t1,8			# [1]
	movz	v1,t1,v1		# [2] If low bits zero, we've moved the first 8 bytes.
	movz	v0,t1,v0		# [2]
/* Regardless of whether the copy is aligned or not, we want to start
 * out by aligning the destination, bumping fromend and toend, and decrementing
 * the count.  There are at least 8 bytes to copy, so we can safely
 * use SDS to retire the unaligned part of the destination.
 */
	LDB	a4,-1(fromend)		# [2]
	PTR_SUBU fromend, v1		# [3]
	SDB	a4,-1(toend)		# [3]
	PTR_SUBU toend, v1		# [3]
	subu	count, v1		# [4]
	li	a4,-8			# [4]
	andi	t1, count, 7		# [5]
	bne	v0,v1,back_unaligned	# [5] low bits are different
	subu	a3, t1, count		# [6]
	beq	a3,zero,backwards_last7	# [6] go do last 7 bytes
	li	a5,-16			# [7]

/*
 * Once we are here, the pointers are aligned on 8 byte boundaries,
 * and there are at least 8 bytes left to copy.
 * Begin copying in 8B chunks, with an FP register.
 */

	.align	4
backwards_8:
	ldxc1	$f4, a4(fromend)	# [0]
	sdxc1	$f4, a4(toend)		# [0]
	beq	a4, a3, backwards_last7 #[0]
	LONG_SUBU	a4,16		# [0]
	ldxc1	$f4, a5(fromend)	# [1]
	sdxc1	$f4, a5(toend)		# [1]
	bne	a5, a3, backwards_8	# [1]
	LONG_SUBU	a5,16		# [1]

/*
 * We are now guaranteed that there are 0 to 7 bytes left to be copied,
 * and that both pointers are aligned.
 * We generate the address of the first byte, and let half of a load
 * and store unaligned do the work.
 */	
backwards_last7:
	beq	t1, zero, ret
	LDS	t0, 0(from)		# This is okay, we know count > 0
	SDS	t0, 0(to)
	j	ra
	move	v0, t3

/*
 * Backwards large unaligned copy code.  Destination is aligned.
 * a3 is -(count & -7)
 */
back_unaligned:	
	beq	a3, zero, back_unalign_last7	# [0]
	PTR_ADDU a6, fromend, a3		# [0]
	li	a4, 8				# [1]
	and	v0, fromend, 7			# [1]
	sub	v0, a4, v0			# [2]
	and	a7, count, ~15			# [2]
	PTR_ADDU t0, fromend, v0		# [3]
	sll	v0,3				# [4]
	PTR_SUBU a7, fromend, a7		# [4]
	move	t1, toend			# [5]
	PTR_SUBU t2, toend, 16			# [5]
/* This loop should do unaligned copies at 8 bytes/2 cycles on TFP.
 * count is number of bytes to copy.  count >= 8
 * toend   is 8 byte aligned destination end + 1
 * t1	is 8 byte aligned destination end + 1, pointer gets decremented.
 * t2	is t1-16
 * v0	is (8-fromend&7) == alignment amount
 * fromend is unaligned source end + 1
 * t0	is fromend+v0
 * a6	is fromend - (count & ~7), stop condition for 16n byte copies
	doesn't match if (count & 8) == 8
 * a7	is fromend - (count & ~15), stop condition for 16n+8 byte copies
	if (count & 8) == 0, this would match, but a6 matches first
 */

	ld	a4, -8(t0)			# [6]
	beq	fromend, a7, back_unalign_loop_end0 #[6]
	dsrlv	a4, a4, v0			# [6]
	.align	4
back_unalign_loop:
	ldl	a4, -8(fromend)			# [0]
	ld	a5, -16(t0)			# [0]
	PTR_SUBU	fromend, 16		# [0]
	nada
	sd	a4, -8(t1)			# [1]
	dsrlv	a5, a5, v0			# [1]
	beq	fromend, a6, back_unalign_loop_end1 #[1]
	PTR_SUBU	t1, 32			# [2]
	nada
	ldl	a5, 0(fromend)			# [2]
	ld	a4, -24(t0)			# [2]
	sd	a5, 0(t2)			# [3]
	beq	fromend, a7, back_unalign_loop_end2 #[3]
	dsrlv	a4, a4, v0			# [3]
	ldl	a4, -8(fromend)			# [4]
	ld	a5, -32(t0)			# [4]
	PTR_SUBU	t0, 32			# [4]
	PTR_SUBU	fromend, 16		# [4]
	sd	a4, -8(t2)			# [5]
	dsrlv	a5, a5, v0			# [5]
	beq	fromend, a6, back_unalign_loop_end3 #[5]
	PTR_SUBU	t2, 32			# [6]
	nada
	ldl	a5, 0(fromend)			# [6]
	ld	a4, -8(t0)			# [6]
	sd	a5, 0(t1)			# [7]
	bne	fromend, a7, back_unalign_loop	# [7]
	dsrlv	a4, a4, v0			# [7]

back_unalign_loop_end0:
	ldl	a4, -8(fromend)		# [0]
	and	v0, count, 7		# [0]
	b	back_unalign_last7	# [0]
	sd	a4, -8(t1)		# [1]
	
back_unalign_loop_end1:
	ldl	a5, 0(fromend)		# [0]
	and	v0, count, 7		# [0]
	b	back_unalign_last7	# [0]
	sd	a5, 0(t2)		# [1]
	
back_unalign_loop_end2:
	ldl	a4, -8(fromend)		# [0]
	and	v0, count, 7		# [0]
	b	back_unalign_last7	# [0]
	sd	a4, -8(t2)		# [1]
	
	nop	# This aligns the back_unalign_last7 jump target
back_unalign_loop_end3:
	ldl	a5, 0(fromend)		# [0]
	and	v0, count, 7		# [0]
	sd	a5, 0(t1)		# [1]

/* Now we have [0,7] bytes to move, the destination pointer is aligned,
 * and the source pointer is unaligned.  Since we may have some overlap,
 * it is not legal to store over already-copied parts of the destination.
 * Note that "count" has not been properly decremented, so just the low
 * three bits are valid. */

	.align 4
back_unalign_last7:
	and	v0, count, 7		# [0]
	LDS	a4, 0(from)		# [0]
	beq	v0, zero, ret		# [1]
	LDB	a4, 7(from)		# [1]	This is okay, we know count was originally >= 7
	SDS	a4, 0(to)		# [2]
	j	ra			# [2]
	move	v0, t3			# [2]
	
/*
 * Byte at a time copy code.  This is used when 0 < byte count < MINCOPY(8).
 * This was difficult enough to get to 1 byte/cycle that completely unrolling
 * it just seemed easier.
 */
backwards_bytecopy:
	beq	count, zero, ret	# [0]
	PTR_ADDU	v1,t3,1		# [0] v1 = original "to" + 1
	lb	v0,-1(fromend)		# [0]
	sb	v0,-1(toend)		# [1]
	beq	v1,toend,ret		# [1]
	PTR_ADDU	v1,1		# [1]
	lb	v0,-2(fromend)		# [1]
	sb	v0,-2(toend)		# [2]
	beq	v1,toend,ret		# [2]
	PTR_ADDU	v1,1		# [2]
	lb	v0,-3(fromend)		# [2]
	sb	v0,-3(toend)		# [3]
	beq	v1,toend,ret		# [3]
	PTR_ADDU	v1,1		# [3]
	lb	v0,-4(fromend)		# [3]
	sb	v0,-4(toend)		# [4]
	beq	v1,toend,ret		# [4]
	PTR_ADDU	v1,1		# [4]
	lb	v0,-5(fromend)		# [4]
	sb	v0,-5(toend)		# [5]
	beq	v1,toend,ret		# [5]
	PTR_ADDU	v1,1		# [5]
	lb	v0,-6(fromend)		# [5]
	sb	v0,-6(toend)		# [6]
	beq	v1,toend,ret		# [6]
	nop				# [6]
	lb	v0,-7(fromend)		# [6]
	sb	v0,-7(toend)		# [7]
	j	ra			# [7] return to caller
	move	v0,t3			# [7] Set v0 to old "to" pointer

       	.set	reorder
XLEAF(bcopy_end)
	END(bcopy)

