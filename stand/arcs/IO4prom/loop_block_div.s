#if TFP

/* block_div.s
 * This file defines void block_div(struct test_block *)
 */

#define numerator       0x0
#define denominator     0x20
#define result  	0x40
#define remainder       0xc0
#define denom_step      0x140
#define inexact 	0x158
#define big_rem 	0x15c
#define min_rem 	0x168
#define max_rem 	0x188
#define ds      	0x148
#define points  	0x150
#define halfulp 	0x160

#define zero	$0	/* wired zero */
#define AT	$at	/* assembler temp */
#define v0	$2	/* return value */
#define v1	$3
#define a0	$4	/* argument registers */
#define a1	$5
#define a2	$6
#define a3	$7
#define t0	$8	/* caller saved */
#define t1	$9
#define t2	$10
#define t3	$11
#define t4	$12
#define t5	$13
#define t6	$14
#define t7	$15
#define s0	$16	/* callee saved */
#define s1	$17
#define s2	$18
#define s3	$19
#define s4	$20
#define s5	$21
#define s6	$22
#define s7	$23
#define t8	$24	/* code generator */
#define t9	$25
#define k0	$26	/* kernel temporary */
#define k1	$27
#define gp	$28	/* global pointer */
#define sp	$29	/* stack pointer */
#define fp	$30	/* frame pointer */
#define ra	$31	/* return address */

#define r0	$0
#define r1	$1
#define r2	$2
#define r3	$3
#define r4	$4
#define r5	$5
#define r6	$6
#define r7	$7
#define r8	$8
#define r9	$9
#define r10	$10
#define r11	$11
#define r12	$12
#define r13	$13
#define r14	$14
#define r15	$15
#define r16	$16
#define r17	$17
#define r18	$18
#define r19	$19
#define r20	$20
#define r21	$21
#define r22	$22
#define r23	$23
#define r24	$24
#define r25	$25
#define r26	$26
#define r27	$27
#define r28	$28
#define r29	$29
#define r30	$30
#define r31	$31

#define fp0	$f0
#define fp1	$f1
#define fp2	$f2
#define fp3	$f3
#define fp4	$f4
#define fp5	$f5
#define fp6	$f6
#define fp7	$f7
#define fp8	$f8
#define fp9	$f9
#define fp10	$f10
#define fp11	$f11
#define fp12	$f12
#define fp13	$f13
#define fp14	$f14
#define fp15	$f15
#define fp16	$f16
#define fp17	$f17
#define fp18	$f18
#define fp19	$f19
#define fp20	$f20
#define fp21	$f21
#define fp22	$f22
#define fp23	$f23
#define fp24	$f24
#define fp25	$f25
#define fp26	$f26
#define fp27	$f27
#define fp28	$f28
#define fp29	$f29
#define fp30	$f30
#define fp31	$f31

#define cc0	$fcc0
#define cc1	$fcc1
#define cc2	$fcc2
#define cc3	$fcc3
#define cc4	$fcc4
#define cc5	$fcc5
#define cc6	$fcc6
#define cc7	$fcc7

#define fconfig $0
#define fsr	$31


	.text
	.globl	loop_block_div
	.ent	loop_block_div
	.frame	sp, 40, $31
	.mask	0x80000000, -48
/*	.set	noreorder */
	.set	reorder
loop_block_div:
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
	dmfc1	t1, $f7
	ld	t2, denom_step(a0)

denominator_loop:
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
	li	t4, 0x1000000
	l.d	$f24, (min_rem+0)(a0)
	l.d	$f25, (min_rem+8)(a0)
	l.d	$f26, (min_rem+16)(a0)
	l.d	$f27, (min_rem+24)(a0)
	s.d	$f20, (result+96)(a0)
	s.d	$f21, (result+104)(a0)
	ctc1	t4, $31
	msub.d	$f8, $f0, $f8, $f4
	msub.d	$f9, $f0, $f9, $f5
	s.d	$f22, (result+112)(a0)
	s.d	$f23, (result+120)(a0)
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
	s.d	$f14, (remainder+48)(a0)
	s.d	$f15, (remainder+56)(a0)
	c.le.d	cc0, $f8, $f24
	c.le.d	cc1, $f12, $f25
	s.d	$f16, (remainder+64)(a0)
	s.d	$f17, (remainder+72)(a0)
	c.le.d	cc2, $f16, $f26
	c.le.d	cc3, $f20, $f27
	s.d	$f18, (remainder+80)(a0)
	s.d	$f19, (remainder+88)(a0)
	movt.d	$f24, $f8, cc0
	movt.d	$f25, $f12, cc1
	s.d	$f20, (remainder+96)(a0)
	s.d	$f21, (remainder+104)(a0)
	movt.d	$f26, $f16, cc2
	movt.d	$f27, $f20, cc3
	s.d	$f22, (remainder+112)(a0)
	s.d	$f23, (remainder+120)(a0)
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
	cfc1	t6, $31
	andi	t6, 4
	sw	t6, inexact(a0)
	/* Save away the minimum remainders found. */
	s.d	$f24, (min_rem+0)(a0)
	s.d	$f25, (min_rem+8)(a0)
	/* We'll be checking if any (abs.d(test.rem[][]) > halfulp*test.num[])
	 * later, and we've done a fair bit of the work already in finding
	 * the minimum remainder, so stash it away.
	 */
	abs.d	$f28,$f24
	abs.d	$f29,$f25
	s.d	$f26, (min_rem+16)(a0)
	s.d	$f27, (min_rem+24)(a0)
	abs.d	$f30,$f26
	abs.d	$f31,$f27
	/* find maximum remainder */
	l.d	$f24, (max_rem+0)(a0)
	l.d	$f25, (max_rem+8)(a0)
	c.le.d	cc0, $f8, $f24
	c.le.d	cc1, $f12, $f25
	l.d	$f26, (max_rem+16)(a0)
	l.d	$f27, (max_rem+24)(a0)
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
	s.d	$f24, (max_rem+0)(a0)
	s.d	$f25, (max_rem+8)(a0)
	movf.d	$f26, $f19, cc6
	movf.d	$f27, $f23, cc7
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
	l.d	$f11, (halfulp)(a0)
	mul.d	$f8, $f11, $f0
	mul.d	$f9, $f11, $f1
	mul.d	$f10, $f11, $f2
	mul.d	$f11, $f11, $f3
	movf.d	$f30, $f26, cc2
	movf.d	$f31, $f27, cc3

	/* Check if absolute value of any remainders were excessive by
	 * checking if the absolute value of the largest were excessive.
	 * Use f8..f11 to hold "excessive" boundaries.
	 */
	c.le.d	cc4, $f28, $f8
	c.le.d	cc5, $f29, $f9
	c.le.d	cc6, $f30, $f10
	c.le.d	cc7, $f31, $f11
	move	t4, zero
	li	t5, 1
	movf	t4, t5, cc4
	movf	t4, t5, cc5
	movf	t4, t5, cc6
	movf	t4, t5, cc7
	sw	t4, big_rem(a0)

	/* Update Denominators and ds
	 */
	daddu	t1,t1,t2
	sd	t1,(denominator+0)(a0)
	dmtc1	t1, $f4
	daddu	t1,t1,t2
	sd	t1,(denominator+8)(a0)
	dmtc1	t1, $f5
	daddu	t1,t1,t2
	sd	t1,(denominator+16)(a0)
	dmtc1	t1, $f6
	daddu	t1,t1,t2
	sd	t1,(denominator+24)(a0)
	dmtc1	t1, $f7
	ld	t5, ds(a0)
	ld	t7, points(a0)
	daddi	t5, t5, 4
	sd	t5, ds(a0)
	slt	t7, t5, t7

	/* If any remainders were inexact or too large, return, otherwise,
	 * loop.
	 */
	bne	t4, zero, DropOut
	bne	t6, zero, DropOut
	bne	t7, zero, denominator_loop
	
DropOut:
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
	.end	loop_block_div

#endif
