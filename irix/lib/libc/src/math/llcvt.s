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
#ident "$Id: llcvt.s,v 1.5 1996/08/06 23:45:00 vegas Exp $"

#include <regdef.h>
#include <sys/asm.h>

#ifdef _MIPSEL
#	define PLSW a0
#	define PMSW a1
#	define RLSW v0
#	define RMSW v1
#else	/* _MIPSEB */
#	define PMSW a0
#	define PLSW a1
#	define RMSW v0
#	define RLSW v1
#endif

/* Convert from 64-bit signed integer to double precision. */
/* double ll_to_d(ll) */

.globl __ll_to_d
.ent __ll_to_d
__ll_to_d:
	.frame	sp, 0, ra
	mtc1	PMSW, $f0
	mtc1	PLSW, $f2
	move	t0, PLSW
	li.d	$f8, 4294967296.0	/* 2^32 */
	cvt.d.w	$f0
	cvt.d.w	$f2
	mul.d	$f0, $f8
	bgez	t0, 12f
	/* sign bit of LSW was set */
	add.d	$f2, $f8		/* correct LSW conversion */
12:	add.d	$f0, $f2		/* add low part to high */
	j	ra
.end __ll_to_d

/* Convert from 64-bit unsigned integer to double precision. */
/* double ull_to_d(ll) */

.globl __ull_to_d
.ent __ull_to_d
__ull_to_d:
	.frame	sp, 0, ra
	mtc1	PMSW, $f0
	mtc1	PLSW, $f2
	move	t0, PLSW
	move	t1, PMSW
	li.d	$f8, 4294967296.0	/* 2^32 */
	cvt.d.w	$f0
	cvt.d.w	$f2
	bgez	t1, 12f
	/* sign bit of MSW was set */
	add.d	$f0, $f8		/* correct MSW conversion */
12:	mul.d	$f0, $f8
	bgez	t0, 14f
	/* sign bit of LSW was set */
	add.d	$f2, $f8		/* correct LSW conversion */
14:	add.d	$f0, $f2		/* add low part to high */
	j	ra
.end __ull_to_d

/* Convert from 64-bit signed integer to single precision. */
/* float ll_to_f(ll) */

.globl __ll_to_f
.ent __ll_to_f
__ll_to_f:
	.frame	sp, 0, ra
	cfc1	ta0, $31			/* save FCSR */
	ctc1	$0, $31			/* set safe FCSR */
	mtc1	PMSW, $f2
	mtc1	PLSW, $f4
	move	t0, PLSW
	li.d	$f8, 4294967296.0	/* 2^32 */
	cvt.d.w	$f2
	cvt.d.w	$f4
	mul.d	$f2, $f8		/* shift up MSW value */
	bgez	t0, 12f
	/* sign bit of LSW was set */
	add.d	$f4, $f8		/* correct LSW convert */
12:	add.d	$f0, $f2, $f4		/* add low part to high */
	cfc1	t1, $31			/* check for inexact */
	and	t1, 4
	bne	t1, 0, 14f
	/* conversion to double was exact */
	ctc1	ta0, $31
	cvt.s.d	$f0
	j	ra
14:	/* conversion to double was not exact, so cvt.s.d would be double round */
	li	t0, 1			/* mode = round-to-0 */
	ctc1	t0, $31
	cvt.s.d	$f0, $f2		/* move 8 bits from high to low double */
	cvt.d.s	$f0
	sub.d	$f6, $f2, $f0
	add.d	$f4, $f6
	mfc1	t2, $f1			/* add value that pushes high 24 bits */
	srl	t2, 20			/* to low end of double */
	addu	t2, (53-24)
	sll	t2, 20
	mtc1	t2, $f9
	mtc1	$0, $f8
	ctc1	ta0, $31			/* restore FCSR */
	add.d	$f0, $f8		/* add bias */
	add.d	$f0, $f4		/* add low part with round to 24
					   bits of precision */
	sub.d	$f0, $f8		/* remove bias */
	cvt.s.d	$f0
	j	ra
.end __ll_to_f 

/* Convert from 64-bit unsigned integer to single precision. */
/* float ull_to_f(ll) */

.globl __ull_to_f
.ent __ull_to_f
__ull_to_f:
	.frame	sp, 0, ra
	cfc1	ta0, $31			/* save FCSR */
	ctc1	$0, $31			/* set safe FCSR */
	mtc1	PMSW, $f2
	mtc1	PLSW, $f4
	move	t0, PLSW
	move	t1, PMSW
	li.d	$f8, 4294967296.0	/* 2^32 */
	cvt.d.w	$f2
	cvt.d.w	$f4
	bgez	t1, 12f
	/* sign bit of MSW was set */
	add.d	$f2, $f8		/* correct MSW convert */
12:	mul.d	$f2, $f8		/* shift up MSW value */
	bgez	t0, 14f
	/* sign bit of LSW was set */
	add.d	$f4, $f8		/* correct LSW convert */
14:	add.d	$f0, $f2, $f4		/* add low part to high */
	cfc1	t1, $31			/* check for inexact */
	and	t1, 4
	bne	t1, 0, 14f
	/* conversion to double was exact */
	ctc1	ta0, $31
	cvt.s.d	$f0
	j	ra
14:	/* conversion to double was not exact, so cvt.s.d would be double round */
	li	t0, 1			/* mode = round-to-0 */
	ctc1	t0, $31
	cvt.s.d	$f0, $f2		/* move 8 bits from high to low double */
	cvt.d.s	$f0
	sub.d	$f6, $f2, $f0
	add.d	$f4, $f6
	mfc1	t2, $f1			/* add value that pushes high 24 bits */
	srl	t2, 20			/* to low end of double */
	addu	t2, (53-24)
	sll	t2, 20
	mtc1	t2, $f9
	mtc1	$0, $f8
	ctc1	ta0, $31			/* restore FCSR */
	add.d	$f0, $f8		/* add bias */
	add.d	$f0, $f4		/* add low part with round to 24
					   bits of precision */
	sub.d	$f0, $f8		/* remove bias */
	cvt.s.d	$f0
	j	ra
.end __ull_to_f 

/* Convert from double precision to 64-bit integer. */
/* This is a common routine for signed and unsigned case;
 * the only difference is the max value, which is passed in $f4. */
/* C rules require truncating the value */
/* longlong dtoll (double); */
.globl __dtoll
.ent __dtoll
__dtoll:
	.frame	sp, 0, ra
	cfc1	ta2, $31
	li	ta3, 1				/* round to zero (truncate) */
	ctc1	ta3, $31
	mfc1	t8, $f13
	li.d	$f2, 4.5035996273704960e+15	/* 2^52 */
	bltz	t8, 10f

	/* x >= 0 */
	c.ult.d	$f12, $f2
	bc1f	20f

	/* 0 <= x < 2^52 -- needs rounding */
	/* x + 2^52 may be = 2^53 after rounding -- this still works */
	add.d	$f0, $f12, $f2			/* round */
	mfc1	RLSW, $f0
	mfc1	RMSW, $f1
	subu	RMSW, ((52+1023)<<20)
	b	50f

10:	/* x < 0 */
	neg.d	$f2
	c.ult.d	$f2, $f12
	bc1f	30f

	/* -2^52 < x < 0 -- needs rounding */
	/* x - 2^52 may be = -2^53 after rounding -- this still works */
	add.d	$f0, $f12, $f2			/* round */
	mfc1	RLSW, $f0
	mfc1	RMSW, $f1
	subu	RMSW, ((52+1023+2048)<<20)
	/* double word negate */
	sltu	t3, RLSW, 1
	negu	RLSW
	not	RMSW
	addu	RMSW, t3
	b	50f

20:	/* x >= 2^52 or NaN */
	/* compare against $f4, which is either 2^63-1 or 2^64-1 */
	mfc1	RLSW, $f12
	mfc1	t1, $f13
	c.ult.d	$f12, $f4
	bc1f	40f
	
	/* 2^52 <= x < 2^63/4 */
	li	t2, (1<<20)		/* hidden bit in high word */
	subu	t3, t2, 1		/* mask for high word */
	and	RMSW, t1, t3		/* mask out exponent */
	or	RMSW, t2		/* add back hidden bit */
	srl	t0, t1, 20		/* shift = exponent - (52+bias) */
	subu	t0, 1023+52		/* ... */
	negu	ta0, t0			/* high |= low >> (32-shift) */
	srl	ta1, RLSW, ta0		/* ... */
	sll	RMSW, t0		/* high <<= shift */
	sll	RLSW, t0		/* low  <<= shift */
	beq	t0, 0, 50f		/* if shift = 0, that's all */
	or	RMSW, ta1		/* else add bits shifted out of low */
	b	50f

30:	/* x <= -2^52 or NaN */

	li.d	$f2, -9.2233720368547758e+18	/* -2^63 */
	mfc1	RLSW, $f12
	mfc1	t1, $f13
	c.ule.d	$f2, $f12
	bc1f	40f
	
	/* -2^63 <= x <= -2^52 */
	li	t2, (1<<20)		/* hidden bit in high word */
	subu	t3, t2, 1		/* mask for high word */
	and	RMSW, t1, t3		/* mask out exponent */
	or	RMSW, t2		/* add back hidden bit */
	srl	t0, t1, 20		/* shift = exponent - (52+bias) */
	subu	t0, 52+1023+2048	/* ... */
	negu	ta0, t0			/* high |= low >> (32-shift) */
	srl	ta1, RLSW, ta0		/* ... */
	sll	RMSW, t0		/* high <<= shift */
	sll	RLSW, t0		/* low  <<= shift */
	beq	t0, 0, 32f		/* if shift = 0, that's all */
	or	RMSW, ta1		/* else add bits shifted out of low */
32:	/* double word negate */
	sltu	t3, RLSW, 1
	negu	RLSW
	not	RMSW
	addu	RMSW, t3
	b	50f

40:	/* x is NaN or x < -2^63 or x >= 2^63/4 */
	/* raise Invalid */
	li	RMSW, 0x7fffffff	/* signed case */
	li	RLSW, 0xffffffff
	li.d	$f2, 9.223372036854775807e+18	/* 2^63-1 */
	c.ueq.d	$f2, $f4
	bc1t	42f
	li	RMSW, 0xffffffff	/* unsigned case */
42:
	cfc1	t0, $31
	or	t0, ((1<<16)|(1<<6))
	ctc1	t0, $31
	b	50f
50:
	cfc1	ta3, $31
	and	ta3, -4
	or	ta2, ta3
	ctc1	ta2, $31
	j	ra
.end __dtoll

/* Convert from double precision to 64-bit signed integer. */
/* C rules require truncating the value */
/* longlong d_to_ll (double); */
.globl __d_to_ll
.ent __d_to_ll
__d_to_ll:
	.set noreorder
	.cpload	t9
	.set reorder
	subu    sp, 32
	sw      ra, 28(sp)
	.cprestore 24
	.mask   0x80000000, -4
	.frame  sp, 32, ra
	li.d	$f4, 9.223372036854775807e+18	/* 2^63-1 */
	jal     __dtoll
	/* use v0,v1 that are already set */
	lw      ra, 28(sp)
	addu    sp, 32
	j       ra
.end __d_to_ll

/* longlong f_to_ll (float) */
.globl __f_to_ll
.ent __f_to_ll
__f_to_ll:
	.set noreorder
	.cpload	t9
	.set reorder
	subu    sp, 32
	sw      ra, 28(sp)
	.cprestore 24
	.mask   0x80000000, -4
	.frame  sp, 32, ra
	cvt.d.s $f12, $f12
	jal     __d_to_ll
	/* use v0,v1 that are already set */
	lw      ra, 28(sp)
	addu    sp, 32
	j       ra
.end __f_to_ll

/* Convert from double precision to 64-bit unsigned integer. */
/* C rules require truncating the value */
/* ulonglong d_to_ull (double); */
.globl __d_to_ull
.ent __d_to_ull
__d_to_ull:
	.set noreorder
	.cpload	t9
	.set reorder
	subu    sp, 32
	sw      ra, 28(sp)
	.cprestore 24
	.mask   0x80000000, -4
	.frame  sp, 32, ra
	li.d	$f4, 1.8446744073709551615e+19 	/* 2^64-1 */
	jal     __dtoll
	/* use v0,v1 that are already set */
	lw      ra, 28(sp)
	addu    sp, 32
	j       ra
.end __d_to_ull

/* ulonglong f_to_ull (float) */
.globl __f_to_ull
.ent __f_to_ull
__f_to_ull:
	.set noreorder
	.cpload	t9
	.set reorder
	subu    sp, 32
	sw      ra, 28(sp)
	.cprestore 24
	.mask   0x80000000, -4
	.frame  sp, 32, ra
	cvt.d.s $f12, $f12
	jal     __d_to_ull
	/* use v0,v1 that are already set */
	lw      ra, 28(sp)
	addu    sp, 32
	j       ra
.end __f_to_ull

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

#define	FP_RN	0
#define	FP_RZ	1

.rdata

dzero:		.dword	0x0000000000000000
magic:
twop52:		.dword	0x4330000000000000
twop63:		.dword	0x43e0000000000000
twop63m:	.dword	0x43dfffffffffffff
corr:		.dword	0x4088000000000000
mtwop52:	.dword	0xc330000000000000
mtwop63:	.dword	0xc3e0000000000000

szero:		.word	0x00000000
smagic:
stwop23:	.word	0x4b000000
stwop63:	.word	0x5f000000
stwop63m:	.word	0x5effffff
scorr:		.word	0x52c00000
mstwop23:	.word	0xcb000000
mstwop63:	.word	0xdf000000

.text

/* routine __f_trunc_ll_f
 * Truncates a float to a long long and converts it back to a float.
 * arguments outside the interval [-2.0**63, 2.0**63) cause an invalid
 * operation exception and return (float) 0x7fffffffffffffffll.
 */

LEAF(__f_trunc_ll_f)
	SETUP_GP

	/* change rounding mode to round to zero */

	cfc1	t0, $31			# fetch fp csr
	and	v0, t0, 3		# save rm bits
	xor	t0, v0
	ori	t0, FP_RZ		# t0 = fp csr with rm = round to zero
	ctc1	t0, $31

	c.un.s	$f12, $f12
	l.s	$f0, szero
	bc1t	badarg1			# branch if arg is a NaN

	c.lt.s	$f12, $f0
	l.s	$f0, stwop23
	bc1t	neg1			# branch if arg is negative

	c.le.s	$f0, $f12
	bc1t	1f			# branch if 2**23 <= arg

	/* shift the fractional bits of the arg off the end by 
	 * adding and subtracting a magic constant (2.0**23)
	 */

	add.s	$f12, $f0
	sub.s	$f0, $f12, $f0
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

1:
	/* test if 2.0**63 <= arg	*/

	l.s	$f0, stwop63
	c.le.s	$f0, $f12
	bc1t	badarg1			# branch if 2.0**63 <= arg

	cfc1	t0, $31
	mov.s	$f0, $f12		# result = arg
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

neg1:
	l.s	$f0, mstwop23
	c.le.s	$f12, $f0
	l.s	$f0, smagic
	bc1t	2f			# branch if arg <= -2**23

	/* shift the fractional bits of the arg off the end by 
	 * subtracting and adding a magic constant (2.0**23)
	 */

	sub.s	$f12, $f0
	add.s	$f0, $f12, $f0
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31		# restore rounding mode
	j	ra

2:
	/* test if arg < -2.0**63 	*/

	l.s	$f0, mstwop63
	c.lt.s	$f12, $f0
	bc1t	badarg1			# branch if arg < -2.0**63

	cfc1	t0, $31
	mov.s	$f0, $f12		# result = arg
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

badarg1:
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31		# restore rounding mode
	cvt.w.s	$f12, $f12	# this will generate an invalid operation exception
	l.s	$f0, stwop63m	# return (float) 0x7fffffffffffffffll
	l.s	$f12, scorr
	add.s	$f0, $f12
	j	ra

END(__f_trunc_ll_f)

/* routine __f_round_ll_f
 * Rounds a float to a long long and converts it back to a float.
 * arguments outside the interval [-2.0**63, 2.0**63) cause an invalid
 * operation exception and return (float) 0x7fffffffffffffffll.
 */

LEAF(__f_round_ll_f)
	SETUP_GP

	/* change rounding mode to round to zero */

	cfc1	t0, $31			# fetch fp csr
	and	v0, t0, 3		# save rm bits
	xor	t0, v0
	ori	t0, FP_RN		# t0 = fp csr with rm = round to nearest
	ctc1	t0, $31

	c.un.s	$f12, $f12
	l.s	$f0, szero
	bc1t	badarg2			# branch if arg is a NaN

	c.lt.s	$f12, $f0
	l.s	$f0, stwop23
	bc1t	neg2			# branch if arg is negative

	c.le.s	$f0, $f12
	bc1t	1f			# branch if 2**23 <= arg

	/* shift the fractional bits of the arg off the end by 
	 * adding and subtracting a magic constant (2.0**23)
	 */

	add.s	$f12, $f0
	sub.s	$f0, $f12, $f0
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

1:
	/* test if 2.0**63 <= arg	*/

	l.s	$f0, stwop63
	c.le.s	$f0, $f12
	bc1t	badarg2			# branch if 2.0**63 <= arg

	cfc1	t0, $31
	mov.s	$f0, $f12		# result = arg
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

neg2:
	l.s	$f0, mstwop23
	c.le.s	$f12, $f0
	l.s	$f0, smagic
	bc1t	2f			# branch if arg <= -2**23

	/* shift the fractional bits of the arg off the end by 
	 * subtracting and adding a magic constant (2.0**23)
	 */

	sub.s	$f12, $f0
	add.s	$f0, $f12, $f0
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31		# restore rounding mode
	j	ra

2:
	/* test if arg < -2.0**63 	*/

	l.s	$f0, mstwop63
	c.lt.s	$f12, $f0
	bc1t	badarg2			# branch if arg < -2.0**63

	cfc1	t0, $31
	mov.s	$f0, $f12		# result = arg
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

badarg2:
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31		# restore rounding mode
	cvt.w.s	$f12, $f12	# this will generate an invalid operation exception
	l.s	$f0, stwop63m	# return (float) 0x7fffffffffffffffll
	l.s	$f12, corr
	add.s	$f0, $f12
	j	ra

END(__f_round_ll_f)

/* routine __d_trunc_ll_d
 * Truncates a double to a long long and converts it back to a double.
 * arguments outside the interval [-2.0**63, 2.0**63) cause an invalid
 * operation exception and return (double) 0x7fffffffffffffffll.
 */

LEAF(__d_trunc_ll_d)
	SETUP_GP

	/* change rounding mode to round to zero */

	cfc1	t0, $31			# fetch fp csr
	and	v0, t0, 3		# save rm bits
	xor	t0, v0
	ori	t0, FP_RZ		# t0 = fp csr with rm = round to zero
	ctc1	t0, $31

	c.un.d	$f12, $f12
	l.d	$f0, dzero
	bc1t	badarg3			# branch if arg is a NaN

	c.lt.d	$f12, $f0
	l.d	$f0, twop52
	bc1t	neg3			# branch if arg is negative

	c.le.d	$f0, $f12
	bc1t	1f			# branch if 2**52 <= arg

	/* shift the fractional bits of the arg off the end by 
	 * adding and subtracting a magic constant (2.0**52)
	 */

	add.d	$f12, $f0
	sub.d	$f0, $f12, $f0
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

1:
	/* test if 2.0**63 <= arg	*/

	l.d	$f0, twop63
	c.le.d	$f0, $f12
	bc1t	badarg3			# branch if 2.0**63 <= arg

	cfc1	t0, $31
	mov.d	$f0, $f12		# result = arg
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

neg3:
	l.d	$f0, mtwop52
	c.le.d	$f12, $f0
	l.d	$f0, magic
	bc1t	2f			# branch if arg <= -2**52

	/* shift the fractional bits of the arg off the end by 
	 * subtracting and adding a magic constant (2.0**52)
	 */

	sub.d	$f12, $f0
	add.d	$f0, $f12, $f0
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31		# restore rounding mode
	j	ra

2:
	/* test if arg < -2.0**63 	*/

	l.d	$f0, mtwop63
	c.lt.d	$f12, $f0
	bc1t	badarg3			# branch if arg < -2.0**63

	cfc1	t0, $31
	mov.d	$f0, $f12		# result = arg
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

badarg3:
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31		# restore rounding mode
	cvt.w.d	$f12, $f12	# this will generate an invalid operation exception
	l.d	$f0, twop63m	# return (double) 0x7fffffffffffffffll
	l.d	$f12, corr
	add.d	$f0, $f12
	j	ra

END(__d_trunc_ll_d)

/* routine __d_round_ll_d
 * Rounds a double to a long long and converts it back to a double.
 * arguments outside the interval [-2.0**63, 2.0**63) cause an invalid
 * operation exception and return (double) 0x7fffffffffffffffll.
 */

LEAF(__d_round_ll_d)
	SETUP_GP

	/* change rounding mode to round to nearest */

	cfc1	t0, $31			# fetch fp csr
	and	v0, t0, 3		# save rm bits
	xor	t0, v0
	ori	t0, FP_RN		# t0 = fp csr with rm = round to nearest
	ctc1	t0, $31

	c.un.d	$f12, $f12
	l.d	$f0, dzero
	bc1t	badarg4			# branch if arg is a NaN

	c.lt.d	$f12, $f0
	l.d	$f0, twop52
	bc1t	neg4			# branch if arg is negative

	c.le.d	$f0, $f12
	bc1t	1f			# branch if 2**52 <= arg

	/* shift the fractional bits of the arg off the end by 
	 * adding and subtracting a magic constant (2.0**52)
	 */

	add.d	$f12, $f0
	sub.d	$f0, $f12, $f0
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

1:
	/* test if 2.0**63 <= arg	*/

	l.d	$f0, twop63
	c.le.d	$f0, $f12
	bc1t	badarg4			# branch if 2.0**63 <= arg

	cfc1	t0, $31
	mov.d	$f0, $f12		# result = arg
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

neg4:
	l.d	$f0, mtwop52
	c.le.d	$f12, $f0
	l.d	$f0, magic
	bc1t	2f			# branch if arg <= -2**52

	/* shift the fractional bits of the arg off the end by 
	 * subtracting and adding a magic constant (2.0**52)
	 */

	sub.d	$f12, $f0
	add.d	$f0, $f12, $f0
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31		# restore rounding mode
	j	ra

2:
	/* test if arg < -2.0**63 	*/

	l.d	$f0, mtwop63
	c.lt.d	$f12, $f0
	bc1t	badarg4			# branch if arg < -2.0**63

	cfc1	t0, $31
	mov.d	$f0, $f12		# result = arg
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31			# restore rounding mode
	j	ra

badarg4:
	cfc1	t0, $31
	srl	t0, 2
	sll	t0, 2
	or	t0, v0
	ctc1	t0, $31		# restore rounding mode
	cvt.w.d	$f12, $f12	# this will generate an invalid operation exception
	l.d	$f0, twop63m	# return (double) 0x7fffffffffffffffll
	l.d	$f12, corr
	add.d	$f0, $f12
	j	ra

END(__d_round_ll_d)

#else

.text

LEAF(__f_trunc_ll_f)

	trunc.l.s $f0, $f12
	cvt.s.l	$f0, $f0
	j	ra

END(__f_trunc_ll_f)

LEAF(__f_round_ll_f)

	round.l.s $f0, $f12
	cvt.s.l	$f0, $f0
	j	ra

END(__f_round_ll_f)

LEAF(__d_trunc_ll_d)

	trunc.l.d $f0, $f12
	cvt.d.l	$f0, $f0
	j	ra

END(__d_trunc_ll_d)

LEAF(__d_round_ll_d)

	round.l.d $f0, $f12
	cvt.d.l	$f0, $f0
	j	ra

END(__d_round_ll_d)

#endif
