/* block_div.s
 * This file defines void block_div(struct test_block *)
 */
#include "div2.h"
#include "regdef.h"

	.text
	.globl	block_div
	.ent	block_div
	.frame	sp, 40, $31
	.mask	0x80000000, -48
/*	.set	noreorder */
block_div:
	daddiu	sp, -80
	sd	$31, 0(sp)
	s.d	$f24, 8(sp)
	s.d	$f25, 16(sp)
	s.d	$f26, 24(sp)
	s.d	$f27, 32(sp)
	s.d	$f28, 40(sp)
	s.d	$f29, 48(sp)
	s.d	$f30, 56(sp)
	s.d	$f31, 64(sp)
	l.d	$f0, (numerator+0)(a0)
	l.d	$f1, (numerator+8)(a0)
	l.d	$f2, (numerator+16)(a0)
	l.d	$f3, (numerator+24)(a0)
	l.d	$f4, (denominator+0)(a0)
	l.d	$f5, (denominator+8)(a0)
	l.d	$f6, (denominator+16)(a0)
	l.d	$f7, (denominator+24)(a0)
	div.d	$f8, $f0, $f4	
	div.d	$f9, $f0, $f5
	div.d	$f10, $f0, $f6	
	div.d	$f11, $f0, $f7
	s.d	$f8, (result+0)(a0)
	s.d	$f9, (result+8)(a0)
	div.d	$f12, $f1, $f4	
	div.d	$f13, $f1, $f5
	s.d	$f10, (result+16)(a0)
	s.d	$f11, (result+24)(a0)
	div.d	$f14, $f1, $f6	
	div.d	$f15, $f1, $f7	
	s.d	$f12, (result+32)(a0)
	s.d	$f13, (result+40)(a0)
	div.d	$f16, $f2, $f4	
	div.d	$f17, $f2, $f5	
	s.d	$f14, (result+48)(a0)
	s.d	$f15, (result+56)(a0)
	div.d	$f18, $f2, $f6	
	div.d	$f19, $f2, $f7	
	s.d	$f16, (result+64)(a0)
	s.d	$f17, (result+72)(a0)
	div.d	$f20, $f3, $f4	
	div.d	$f21, $f3, $f5	
	s.d	$f18, (result+80)(a0)
	s.d	$f19, (result+88)(a0)
	div.d	$f22, $f3, $f6	
	div.d	$f23, $f3, $f7	
	s.d	$f20, (result+96)(a0)
	s.d	$f21, (result+104)(a0)
	s.d	$f22, (result+112)(a0)
	s.d	$f23, (result+120)(a0)
	li	t4, 0x1000000
	ctc1	t4, $31
	move	t4, zero
	msub.d	$f8, $f0, $f8, $f4
	msub.d	$f9, $f0, $f9, $f5
	msub.d	$f10, $f0, $f10, $f6
	msub.d	$f11, $f0, $f11, $f7
	msub.d	$f12, $f1, $f12, $f4
	msub.d	$f13, $f1, $f13, $f5
	msub.d	$f14, $f1, $f14, $f6
	msub.d	$f15, $f1, $f15, $f7
	msub.d	$f16, $f2, $f16, $f4
	msub.d	$f17, $f2, $f17, $f5
	s.d	$f8, (remainder+0)(a0)
	s.d	$f9, (remainder+8)(a0)
	msub.d	$f18, $f2, $f18, $f6
	msub.d	$f19, $f2, $f19, $f7
	s.d	$f10, (remainder+16)(a0)
	s.d	$f11, (remainder+24)(a0)
	msub.d	$f20, $f3, $f20, $f4
	msub.d	$f21, $f3, $f21, $f5
	s.d	$f12, (remainder+32)(a0)
	s.d	$f13, (remainder+40)(a0)
	msub.d	$f22, $f3, $f22, $f6
	msub.d	$f23, $f3, $f23, $f7
	/* Find minimum remainder */
	l.d	$f24, (min_rem+0)(a0)
	l.d	$f25, (min_rem+8)(a0)
	l.d	$f26, (min_rem+16)(a0)
	l.d	$f27, (min_rem+24)(a0)
	s.d	$f14, (remainder+48)(a0)
	s.d	$f15, (remainder+56)(a0)
	s.d	$f16, (remainder+64)(a0)
	s.d	$f17, (remainder+72)(a0)
	s.d	$f18, (remainder+80)(a0)
	s.d	$f19, (remainder+88)(a0)
	s.d	$f20, (remainder+96)(a0)
	s.d	$f21, (remainder+104)(a0)
	s.d	$f22, (remainder+112)(a0)
	s.d	$f23, (remainder+120)(a0)
	c.le.d	cc0, $f8, $f24
	c.le.d	cc1, $f12, $f25
	c.le.d	cc2, $f16, $f26
	c.le.d	cc3, $f20, $f27
	movt.d	$f24, $f8, cc0
	movt.d	$f25, $f12, cc1
	movt.d	$f26, $f16, cc2
	movt.d	$f27, $f20, cc3
	c.le.d	cc4, $f9, $f24
	c.le.d	cc5, $f13, $f25
	c.le.d	cc6, $f17, $f26
	c.le.d	cc7, $f21, $f27
	movt.d	$f24, $f9, cc4
	movt.d	$f25, $f13, cc5
	movt.d	$f26, $f17, cc6
	movt.d	$f27, $f21, cc7
	c.le.d	cc0, $f10, $f24
	c.le.d	cc1, $f14, $f25
	c.le.d	cc2, $f18, $f26
	c.le.d	cc3, $f22, $f27
	movt.d	$f24, $f10, cc0
	movt.d	$f25, $f14, cc1
	movt.d	$f26, $f18, cc2
	movt.d	$f27, $f22, cc3
	c.le.d	cc4, $f11, $f24
	c.le.d	cc5, $f15, $f25
	c.le.d	cc6, $f19, $f26
	c.le.d	cc7, $f23, $f27
	movt.d	$f24, $f11, cc4
	movt.d	$f25, $f15, cc5
	movt.d	$f26, $f19, cc6
	movt.d	$f27, $f23, cc7
	/* Find if any of the msub's were inexact.
	 * This is down here so that the stores can
	 * be spread across the computations above.
	 * Compares cannot generate an inexact flag.
	 */
	cfc1	t4, $31
	andi	t4, 4
	sw	t4, inexact(a0)
	/* Save away the minimum remainders found. */
	s.d	$f24, (min_rem+0)(a0)
	s.d	$f25, (min_rem+8)(a0)
	s.d	$f26, (min_rem+16)(a0)
	s.d	$f27, (min_rem+24)(a0)
	/* We'll be checking if any (abs.d(test.rem[][]) > halfulp*test.num[])
	 * later, and we've done a fair bit of the work already in finding
	 * the minimum remainder, so stash it away.
	 */
	abs.d	$f28,$f24
	abs.d	$f29,$f25
	abs.d	$f30,$f26
	abs.d	$f31,$f27
	/* find maximum remainder */
	l.d	$f24, (max_rem+0)(a0)
	l.d	$f25, (max_rem+8)(a0)
	l.d	$f26, (max_rem+16)(a0)
	l.d	$f27, (max_rem+24)(a0)
	c.le.d	cc0, $f8, $f24
	c.le.d	cc1, $f12, $f25
	c.le.d	cc2, $f16, $f26
	c.le.d	cc3, $f20, $f27
	movf.d	$f24, $f8, cc0
	movf.d	$f25, $f12, cc1
	movf.d	$f26, $f16, cc2
	movf.d	$f27, $f20, cc3
	c.le.d	cc4, $f9, $f24
	c.le.d	cc5, $f13, $f25
	c.le.d	cc6, $f17, $f26
	c.le.d	cc7, $f21, $f27
	movf.d	$f24, $f9, cc4
	movf.d	$f25, $f13, cc5
	movf.d	$f26, $f17, cc6
	movf.d	$f27, $f21, cc7
	c.le.d	cc0, $f10, $f24
	c.le.d	cc1, $f14, $f25
	c.le.d	cc2, $f18, $f26
	c.le.d	cc3, $f22, $f27
	movf.d	$f24, $f10, cc0
	movf.d	$f25, $f14, cc1
	movf.d	$f26, $f18, cc2
	movf.d	$f27, $f22, cc3
	c.le.d	cc4, $f11, $f24
	c.le.d	cc5, $f15, $f25
	c.le.d	cc6, $f19, $f26
	c.le.d	cc7, $f23, $f27
	movf.d	$f24, $f11, cc4
	movf.d	$f25, $f15, cc5
	movf.d	$f26, $f19, cc6
	movf.d	$f27, $f23, cc7
	s.d	$f24, (max_rem+0)(a0)
	s.d	$f25, (max_rem+8)(a0)
	s.d	$f26, (max_rem+16)(a0)
	s.d	$f27, (max_rem+24)(a0)

	/* Collect maximum absolute remainder values in $f28..$f31 */
	abs.d	$f24, $f24
	abs.d	$f25, $f25
	abs.d	$f26, $f26
	abs.d	$f27, $f27
	c.le.d	cc0, $f24, $f28
	c.le.d	cc1, $f25, $f29
	c.le.d	cc2, $f26, $f30
	c.le.d	cc3, $f27, $f31
	movf.d	$f28, $f24, cc0
	movf.d	$f29, $f25, cc1
	movf.d	$f30, $f26, cc2
	movf.d	$f31, $f27, cc3

	/* Check if absolute value of any remainders were excessive by
	 * checking if the absolute value of the largest were excessive.
	 * Use f24..f27 to hold "excessive" boundaries.
	 */
	l.d	$f27, (halfulp)(a0)
	mul.d	$f24, $f27, $f0
	mul.d	$f25, $f27, $f1
	mul.d	$f26, $f27, $f2
	mul.d	$f27, $f27, $f3
	c.le.d	cc4, $f28, $f24
	c.le.d	cc5, $f29, $f25
	c.le.d	cc6, $f30, $f26
	c.le.d	cc7, $f31, $f27
	move	t4, zero
	li	t5, 1
	movf	t4, t5, cc4
	movf	t4, t5, cc5
	movf	t4, t5, cc6
	movf	t4, t5, cc7
	sw	t4, big_rem(a0)
		
	l.d	$f24, 8(sp)
	l.d	$f25, 16(sp)
	l.d	$f26, 24(sp)
	l.d	$f27, 32(sp)
	l.d	$f28, 40(sp)
	l.d	$f29, 48(sp)
	l.d	$f30, 56(sp)
	l.d	$f31, 64(sp)
	ld	$31, 0(sp)
	daddiu	sp, 80
	j	$31
	nada
/*	.set	reorder */
	.end	block_div
