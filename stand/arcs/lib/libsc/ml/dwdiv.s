/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsc/ml/RCS/dwdiv.s,v 1.2 1994/11/09 21:59:09 sfc Exp $ */

#include	"ml.h"
#include <regdef.h>

#ifdef _MIPSEL
#	define LSH 0
#	define MSH 2
#	define LQUO 0(a0)
#	define MQUO 4(a0)
#	define LREM 0(a1) 
#	define MREM 4(a1)
#	define LDEN a2
#	define MDEN a3
#	define LSOR 16(sp)
#	define MSOR 20(sp)
#endif
#ifdef _MIPSEB
#	define MSH 0
#	define LSH 2
#	define MQUO 0(a0)
#	define LQUO 4(a0)
#	define MREM 0(a1)
#	define LREM 4(a1)
#	define MDEN a2
#	define LDEN a3
#	define MSOR 16(sp)
#	define LSOR 20(sp)
#endif

.text

/* dwdiv (dw *quotient, dw *remainder, dw dividend, dw divisor); */
.globl __dwdiv
.ent __dwdiv
__dwdiv:
	.frame	sp, 0, ra
	move	t0, LDEN	/* dividend */
	move	t1, MDEN
	lw	t2, LSOR	/* divisor */
	lw	t3, MSOR
	div	zero, t0, t2	/* early start for first case */
	/* test if both dividend and divisor are signed 32-bit values */
#ifdef __sgi
	sra	t5, t3, 31
	sra	t4, t1, 31
	bne	t3, t5, 2f	/* divisor not 32-bit */
#else
	sra	t4, t0, 31
	sra	t5, t2, 31
	bne	t3, t5, 2f	/* divisor not 32-bit */
#endif
	/* divisor is 32-bit signed */
	beq	t2, -1, 9f	/* handle -1 as special case because
				   of overflow */
	bne	t1, t4, 1f	/* dividend not 32-bit */
	/* simply use 32-bit divide instruction */
	mflo	v0
	mfhi	t4
	sw	v0, LQUO
	sra	v1, v0, 31
	sw	v1, MQUO
	sw	t4, LREM
	sra	t5, t4, 31
	sw	t5, MREM
	j	ra

	/* divisor 32-bit signed, dividend not */
1:	srl	t4, t2, 16
	bltz	t1, 2f
	bne	t4, 0, 2f

	/* dividing 64-bit positive value by 16-bit unsigned value */
#ifdef _MIPSEL
	sll	v0, MDEN, 16
#else
	move	v0, MDEN
#endif
	srl	v0, v0, 16	/* get 16-bit MSH value from MDEN */
	div	zero, v0, t2
	mflo	v1
	mfhi	v0
	sh	v1, MSH+MQUO
	sll	v0, 16
#ifdef _MIPSEB
	sll	t0, MDEN, 16
#else
	move	t0, MDEN
#endif
	srl	t0, t0, 16	/* get 16-bit LSH value from MDEN */
	or	v0, t0
	div	zero, v0, t2
	mflo	v1
	mfhi	v0
	sh	v1, LSH+MQUO
	sll	v0, 16
#ifdef _MIPSEL
	sll	t0, LDEN, 16
#else
	move	t0, LDEN
#endif
	srl	t0, t0, 16	/* get 16-bit MSH value from LDEN */
	or	v0, t0
	div	zero, v0, t2
	mflo	v1
	mfhi	v0
	sh	v1, MSH+LQUO
	sll	v0, 16
#ifdef _MIPSEB
	sll	t0, LDEN, 16
#else
	move	t0, LDEN
#endif
	srl	t0, t0, 16	/* get 16-bit LSH value from LDEN */
	or	v0, t0
	divu	zero, v0, t2
	mflo	v1
	mfhi	v0
	sh	v1, LSH+LQUO
	sw	zero, MREM
	sw	v0, LREM
	j	ra

	/* convert to double, works okay if precision < 2^53 */
2:	li.d	$f8, 4294967296.0	/* 2^32 */
	cfc1	v1, $31
	li	t5, 1
	ctc1	t5, $31			/* clear sticky bits and round to 0 */

	mtc1	t0, $f0	
	mtc1	t1, $f2
	cvt.d.w	$f0			/* convert LDEN to double */
	cvt.d.w	$f2			/* convert MDEN to double */
	bgez	t0, 22f
	add.d	$f0, $f8		/* treat sign-bit as 2^32 */
22:	mul.d	$f2, $f8		/* shift MDEN to real value */
	add.d	$f0, $f2		/* add to get real dividend value */

	mtc1	t2, $f4
	mtc1	t3, $f6
	cvt.d.w	$f4			/* convert LSOR to double */
	cvt.d.w	$f6			/* convert MSOR to double */
	bgez	t2, 24f
	add.d	$f4, $f8		/* handle sign-bit */
24:	mul.d	$f6, $f8
	add.d	$f4, $f6		/* now have full divisor */
	cfc1	t9, $31
	and	t9, 4			/* mask all but inexact sticky bit */
	bne	t9, 0, 3f

	div.d	$f2, $f0, $f4
	li.d	$f6, 4503599627370496.0
	mfc1	t9, $f3
	bgez	t9, 26f
	neg.d	$f6
26:	add.d	$f2, $f6
	mfc1	t4, $f2
	mfc1	t5, $f3
	and	t5, 0x000fffff

	multu	t4, t2
	mflo	t6
	mfhi	t7
	multu	t4, t3
	mflo	t8
	addu	t7, t8
	multu	t5, t2
	mflo	t8
	addu	t7, t8

	sltu	t9, t0, t6
	subu	t6, t0, t6
	subu	t7, t1, t7
	subu	t7, t9

	sw	t4, LQUO
	sw	t5, MQUO
	sw	t6, LREM
	sw	t7, MREM

	ctc1	v1, $31
	j	ra

3:	break	0			/* fp op raised exception */

	/* divisor = -1 */
9:	negu	t0
	sw	t0, LQUO
	sltu	t2, t0, 1
	not	t1
	addu	t1, t2
	sw	t1, MQUO
	sw	zero, LREM
	sw	zero, MREM
	j	ra
.end __dwdiv

/* dwdivu (dw *quotient, dw *remainder, dw dividend, dw divisor); */
/* unsigned division */
.globl __dwdivu
.ent __dwdivu
__dwdivu:
	.frame	sp, 0, ra
	move	t0, LDEN	/* dividend */
	move	t1, MDEN
	lw	t2, LSOR	/* divisor */
	lw	t3, MSOR
	divu	zero, t0, t2	/* early start for first case */
	/* test if both dividend and divisor are 32-bit values */
	sra	t4, t1, 31
	bnez	t3, 2f	/* divisor not 32-bit */
	/* divisor is 32-bit */
	bnez	t1, 1f	/* dividend not 32-bit */
	/* simply use 32-bit divide instruction */
	mflo	v0
	mfhi	t4
	sw	v0, LQUO
	sw	zero, MQUO
	sw	t4, LREM
	sw	zero, MREM
	j	ra

	/* divisor 32-bit, dividend not */
1:	srl	t4, t2, 16
	bne	t4, 0, 2f

	/* dividing 64-bit positive value by 16-bit unsigned value */
#ifdef _MIPSEL
	sll	v0, MDEN, 16
#else
	move	v0, MDEN
#endif
	srl	v0, v0, 16	/* get 16-bit MSH value from MDEN */
	divu	zero, v0, t2
	mflo	v1
	mfhi	v0
	sh	v1, MSH+MQUO
	sll	v0, 16
#ifdef _MIPSEB
	sll	t0, MDEN, 16
#else
	move	t0, MDEN
#endif
	srl	t0, t0, 16	/* get 16-bit LSH value from MDEN */
	or	v0, t0
	divu	zero, v0, t2
	mflo	v1
	mfhi	v0
	sh	v1, LSH+MQUO
	sll	v0, 16
#ifdef _MIPSEL
	sll	t0, LDEN, 16
#else
	move	t0, LDEN
#endif
	srl	t0, t0, 16	/* get 16-bit MSH value from LDEN */
	or	v0, t0
	divu	zero, v0, t2
	mflo	v1
	mfhi	v0
	sh	v1, MSH+LQUO
	sll	v0, 16
#ifdef _MIPSEB
	sll	t0, LDEN, 16
#else
	move	t0, LDEN
#endif
	srl	t0, t0, 16	/* get 16-bit LSH value from LDEN */
	or	v0, t0
	divu	zero, v0, t2
	mflo	v1
	mfhi	v0
	sh	v1, LSH+LQUO
	sw	zero, MREM
	sw	v0, LREM
	j	ra

2:	li.d	$f8, 4294967296.0
	cfc1	v1, $31
	li	t5, 1
	ctc1	t5, $31

	mtc1	t0, $f0
	mtc1	t1, $f2
	cvt.d.w	$f0
	cvt.d.w	$f2
	bgez	t0, 22f
	add.d	$f0, $f8
22:	mul.d	$f2, $f8
	add.d	$f0, $f2

	mtc1	t2, $f4
	mtc1	t3, $f6
	cvt.d.w	$f4
	cvt.d.w	$f6
	bgez	t2, 24f
	add.d	$f4, $f8
24:	mul.d	$f6, $f8
	add.d	$f4, $f6
	cfc1	t9, $31
	and	t9, 4
	bne	t9, 0, 3f

	div.d	$f2, $f0, $f4
	li.d	$f6, 4503599627370496.0
	mfc1	t9, $f3
	bgez	t9, 26f
	neg.d	$f6
26:	add.d	$f2, $f6
	mfc1	t4, $f2
	mfc1	t5, $f3
	and	t5, 0x000fffff

	multu	t4, t2
	mflo	t6
	mfhi	t7
	multu	t4, t3
	mflo	t8
	addu	t7, t8
	multu	t5, t2
	mflo	t8
	addu	t7, t8

	sltu	t9, t0, t6
	subu	t6, t0, t6
	subu	t7, t1, t7
	subu	t7, t9

	sw	t4, LQUO
	sw	t5, MQUO
	sw	t6, LREM
	sw	t7, MREM

	ctc1	v1, $31
	j	ra

	/* if dividend < divisor then quo = 0, rem = dividend */
3:	lw	t0, MSOR
	bgtu	MDEN, t0, 36f
	bltu	MDEN, t0, 32f
	/* MDEN == MSOR */
	lw	t0, LSOR
	bgeu	LDEN, t0, 36f
32:	sw	zero, LQUO
	sw	zero, MQUO
	sw	LDEN, LREM
	sw	MDEN, MREM
	j	ra

	/* dividend or divisor too big; have to do division the hard way */
36:	break	0
.end __dwdivu

/* Create wrapper functions that translate long longs to dw */

/***** Wrapper functions now obsolete in v3.10

#define LL2	__ll_div
#define DW2	__dw_div
#include "wrapper.i"
#undef LL2
#undef DW2
#define LL2	__ull_div
#define DW2	__udw_div
#include "wrapper.i"
#undef LL2
#undef DW2

#define LL2	__ll_rem
#define DW2	__dw_rem
#include "wrapper.i"
#undef LL2
#undef DW2

#define LL2	__ull_rem
#define DW2	__udw_rem
#include "wrapper.i"
#undef LL2
#undef DW2

#define LL2	__ll_mod
#define DW2	__dw_mod
#include "wrapper.i"

*****/

