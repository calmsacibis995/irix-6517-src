/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991 MIPS Computer Systems, Inc.            |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 52.227-7013.   |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Drive                                |
 * |         Sunnyvale, CA 94086                               |
 * |-----------------------------------------------------------|
 */
#ident "$Id: lldivremi.s,v 1.2 1994/10/04 19:09:20 jwag Exp $"

#include <regdef.h>

/* ulldivremi (long long *quotient, long long *remainder, 
 * 		unsigned long long dividend, unsigned short divisor); */

#ifdef _MIPSEL
#	define LSH 0
#	define MSH 2
#	define LQUO 0(a0)
#	define MQUO 4(a0)
#	define LREM 0(a1) 
#	define MREM 4(a1)
#	define LDEN a2
#	define MDEN a3
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
#endif
#define ISOR 16(sp)

.text

/* This is a short-cut routine that assumes divisor is 16-bit positive
 * and dividend is 64-bit positive.   There is no error-checking to
 * ensure this, so be careful when you call it. */
.globl __ull_divremi
.ent __ull_divremi
__ull_divremi:
	.frame	sp, 0, ra
	move	t0, LDEN	/* dividend */
	move	t1, MDEN
	lw	t2, ISOR	/* divisor */
	div	zero, t0, t2	/* early start for first case */
	/* test if both dividend and divisor are signed 32-bit values */
	sra	ta0, t0, 31
	/* divisor is 32-bit signed */
	bne	t1, ta0, 1f	/* dividend not 32-bit */
	/* simply use 32-bit divide instruction */
	mflo	v0
	mfhi	ta0
	sw	v0, LQUO
	sra	v1, v0, 31
	sw	v1, MQUO
	sw	ta0, LREM
	sra	ta1, ta0, 31
	sw	ta1, MREM
	j	ra

1:	/* dividing 64-bit positive value by 16-bit unsigned value */
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
.end __ull_divremi
