/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP32/fpu/RCS/dtoa.s,v 1.2 1997/05/15 16:07:47 philw Exp $ */

#include <regdef.h>

/* _dtoa (buffer, ndigit, x, fflag) */
/* Store sign, ndigits of x, and null in buffer.  Digits are in ascii.
   1 <= ndigits <= 17.  Return exponent.  If fflag set then generate
   Fortran F-format; i.e. ndigits after decimal point. */
.globl _dtoa
.ent _dtoa
_dtoa:
	.frame	sp, 0, t9
	move	t9, ra

	/* Sign */
	li	t1, 32
#if MIPSEL
	bgez	a3, 1f
	li	t1, 45
1:	sll	t5, a3, 1
	srl	t0, a2, 31
	or	t5, t0
	sll	t4, a2, 1
#else
	bgez	a2, 1f
	li	t1, 45
1:	sll	t5, a2, 1
	srl	t0, a3, 31
	or	t5, t0
	sll	t4, a3, 1
#endif
	sb	t1, (a0)
	addu	a0, 1

	/* log10 approximation */
	li	t2, 0x4D104D42 >> 21	# log10(2), shifted so . between hi/lo
	subu	t1, t5, 1023 << 21
	bgez	t1, 2f
	addu	t2, 1
2:	mult	t1, t2
	srl	t8, t5, 21
	beq	t8, 0, 10f
	beq	t8, 2047, 20f

	/* Convert to fixed point number in range [1,100) by multiplication
	   by appropriate power of 10. */
	sll	t5, 11
	srl	t5, 11
	or	t5, 1 << 21
	mfhi	v0			# exponent

15:	neg	t0, v0
	jal	_tenscale
	subu	t8, 1023+20
	addu	t8, v1
	neg	t8

	/* Extract first digit */
	srl	a2, t3, t8
	sll	a3, a2, t8
	subu	t3, a3

	/* Handle F-format */
	lw	t4, 16(sp)
	beq	t4, 0, 19f
	sltu	t4, a2, 10
	subu	t4, v0
	subu	a1, t4
	addu	a1, 2
	bgtz	a1, 16f
#ifdef sgi
	/* ok. format indicates NO digits will be produced.  All
	   we want to do is "round", if necessary. */
	bltu	a2, 10, 99f
	divu	a2, 10
99:
	/* correct digit in a2 to direct rounding.  We know here
	   that we are rounding "up" to +- 1, so half is dropped. 
	   (round to even (0))
	*/
	addu	a2, 48		/* make ascii digit. */
	sb	a2, (a0)	/* store the single digit. */
	addu	a0, 1
	sb	$0, (a0)	/* null teminate. */
	bltu	a2, 48+6, 6f	/* was it lt 6? - round down */
	/* round up.  Set the last digit to '9' and
	   later logic will realize we are rounding into 'sign' pos. */
	li	a2, 9+48
	subu	t1, a0, 1
	sb	a2, (t1)
	b	7f
#else
	li	a1, 1
	b	19f
#endif
16:	ble	a1, 17, 19f
	li	a1, 17
19:
	addu	a1, a0

	bltu	a2, 10, 3f
	/* log10 approximation was low by 1, so we got first "digit" > 9 */
	addu	v0, 1
#if 1	/* is this necessary?? */
	divu	a3, a2, 10
	remu	a2, 10
	addu	a3, 48
#else	/* or will this suffice?? */
	li	a3, 48+1
	subu	a2, 10
#endif
	sb	a3, (a0)
	addu	a0, 1
	bne	a0, a1, 3f
	bltu	a2, 5, 6f
	bgtu	a2, 5, 7f
	b	55f
3:	addu	a2, 48
	sb	a2, (a0)
	addu	a0, 1
	beq	a0, a1, 5f

	/* Now produce digits by multiplying fraction by 10 and taking
	   integer part.  Actually multiply 5 (it's easier) and take
	   integer part from decreasing bit positions. */
4:	srl	t7, t0, 30
	sll	t5, t0, 2
	not	t6, t5
	sltu	t6, t6, t0
	addu	t4, t7, t6
	addu	t0, t5

	srl	t7, t1, 30
	sll	t5, t1, 2
	not	t6, t5
	sltu	t6, t6, t1
	addu	t1, t5
	addu	t7, t6
	not	t6, t4
	sltu	t6, t6, t1
	addu	t1, t4
	addu	t4, t7, t6

	srl	t7, t2, 30
	sll	t5, t2, 2
	not	t6, t5
	sltu	t6, t6, t2
	addu	t2, t5
	addu	t7, t6
	not	t6, t4
	sltu	t6, t6, t2
	addu	t2, t4
	addu	t4, t7, t6

	sll	t5, t3, 2
	addu	t3, t5
	addu	t3, t4

	subu	t8, 1
	srl	a2, t3, t8
	sll	a3, a2, t8
	subu	t3, a3
	addu	a2, 48
	sb	a2, (a0)
	addu	a0, 1
	bne	a0, a1, 4b

5:	/* digit production complete.  now round */
	sb	$0, (a0)
	subu	t8, 1
	srl	a3, t3, t8
	beq	a3, 0, 6f
55:
	bne	t2, 0, 7f
	subu	a3, t3, 1
	bne	t1, 0, 7f
	and	a3, t3
	bne	t0, 0, 7f
	and	a2, 1
	bne	a3, 0, 7f
	bne	a2, 0, 7f

6:	/* round down, i.e. done */
	j	t9

7:	/* round up (in ascii!) */
	subu	t1, a0, 1
75:
.set noreorder
	lbu	t0, (t1)
	beq	t0, 48+9, 8f
	bltu	t0, 48, 9f		/* ' ' and '-' both < '0' */
	 addu	t0, 1
.set reorder
	sb	t0, (t1)
	j	t9
8:	li	t0, 48
	sb	t0, (t1)
	subu	t1, 1
	b	75b
9:	/* tried to round into sign position */
	li	t0, 48+1
	sb	t0, 1(t1)
	li	t0, 48
	sb	t0, 0(a0)
	sb	$0, 1(a0)
	addu	v0, 1
	j	t9
	
10:	/* output 0 or denorm */
	bne	t5, 0, 12f
	bne	t4, 0, 12f
	/* zero */
	lw	t0, 16(sp)		/* f format flag */
	li	t0, 48
	addu	a1, t0
	blez	a1, 40f
	addu	a1, a0
11:	sb	t0, (a0)
	addu	a0, 1
	bne	a0, a1, 11b
40:	sb	$0, (a0)
	li	v0, 0
	j	t9

12:	/* denorm */
	/* normalize with slow but small loop (denorm speed is unimportant) */
	li	t8, -1022
	li	t2, 1 << 21
13:	subu	t8, 1
	sll	t5, 1
	srl	t0, t4, 31
	or	t5, t0
	sll	t4, 1
	and	t0, t5, t2
	beq	t0, 0, 13b
14:	/* log10 approximation */	
	sll	t1, t8, 20
	addu	t1, t5
	subu	t1, t2
	li	t2, (0x4D104D42 >> 20) + 1
	mult	t1, t2
	addu	t8, 1023
	mfhi	v0
	b	15b

20:	/* output +-Infinity or Nan */
	addu	a1, a0
	la	t0, nan
	bne	t4, 0, 22f
	sll	t1, t5, 11
	bne	t1, 0, 22f
	la	t0, infinity
22:	lbu	t1, (t0)
	addu	a0, 1
	sb	t1, -1(a0)
	beq	t1, 0, 23f
	addu	t0, 1
	bne	a0, a1, 22b
23:	li	v0, 0x80000000
	j	t9
.end _dtoa

.rdata
infinity:
	.asciiz "Infinity"
nan:
	.asciiz "NaN"
