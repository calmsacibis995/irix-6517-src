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
#ident "$Id: ldexp.s,v 1.18 1996/06/27 20:36:11 danc Exp $"

/*
 * double ldexp (val, n)
 * double val;
 * int n;
 *
 * Ldexp returns val*2**n, if that result is in range.
 * If underflow occurs, it returns zero.  If overflow occurs,
 * it returns a value of appropriate sign and largest
 * possible magnitude.  In case of either overflow or underflow,
 * errno is set to ERANGE.  Note that errno is not modified if
 * no error occurs.
 */

.weakext ldexp _ldexp

#include <regdef.h>
#include <sys/asm.h>
#include <errno.h>

.rdata

fzero:		.dword	0x0000000000000000
one:		.dword	0x3ff0000000000000
two:		.dword	0x4000000000000000
twop52:		.dword	0x4330000000000000
_local_inf:	.dword	0x7ff0000000000000
_local_qnan:	.dword	0x7ff1000000000000

	.text

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

LEAF(_ldexp)

	SETUP_GP
	mfc1	t0, $f13		# copy MSW of val
	cfc1	t4, $31			# fetch fp csr
	sll	t1, t0, 1		# shift off sign bit of val
	and	t5, t4, 0xfffff07f
	ctc1	t5, $31			# disable exceptions
					# fp csr must be restored before returning!

	srl	t1, 21			# t1 = exponent of val
	beq	t1, 0, small		# branch if exponent(val) == 0
	beq	t1, 0x7ff, big		# branch if exponent(val) == 0x7ff
1:
	beq	a2, 0, 2f		# if n == 0, return val
	bge	a2, 0x7fe, over		# branch if n >= 0x7fe
	addu	t2, t1, a2
	ble	t2, 0, under		# branch if exponent of val*2^n <= 0
	bge	t2, 0x7ff, over		# branch if exponent of val*2^n >= 0x7ff
	sll	a2, 20
	addu	t0, a2
	mtc1	t0, $f13
2:
	mov.d	$f0, $f12
	ctc1	t4, $31			# restore fp csr
	j	ra

under:	/* exponent(val*2^n) <= 0 */

	/* set exponent of val to 0x001 and adjust n */

	li	t2, 0x001
	subu	t2, t1
	subu	a2, t2			# adjust n
	sll	t2, 20
	addu	t0, t2
	mtc1	t0, $f13

	/* set n = max(-54, n); result is the same */

	bge	a2, -54, 5f
	li	a2, -54
	b	5f

over:	/* exponent(val*2^n) >= 0x7ff; set result to
	   val*2^n, and set errno to ERANGE
	*/

	/* set exponent of val to 0x7fe and multiply by 2.0 */

	li	t2, 0x7fe
	subu	t2, t1
	sll	t2, 20
	addu	t0, t2
	mtc1	t0, $f13

	li.d	$f0, 2.0
	mul.d	$f0, $f12

	li      t0,ERANGE
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	ctc1	t4, $31			# restore fp csr
	j	ra

small:	/* exponent(val) == 0 */

	/* depending on the mode of the processor, 1.0*arg
	 * may be zero
	 */

	li.d	$f0, 1.0
	mul.d	$f12, $f0

	li.d	$f0, 0.0
	c.eq.d	$f0, $f12
	bc1f	3f			# return if arg == +/-0.0

	mov.d	$f0, $f12
	ctc1	t4, $31			# restore fp csr
	j	ra

3:
	bne	a2, 0, 4f		# if n == 0, return val
	mov.d	$f0, $f12
	ctc1	t4, $31			# restore fp csr
	j	ra

4:
	bgt	a2, 52, 7f		# branch if n > 52

	/* if n < -53, set n = -53; result is the same */

	bge	a2, -53, 5f
	li	a2, -53

5:
	/* just form 2^n and multiply */

	addi	a2, 0x3ff
	sll	a2, 20
	mtc1	a2, $f1
	mtc1	$0, $f0
	mul.d	$f0, $f12		# form 2^n*val
	li.d	$f12, 0.0
	c.eq.d	$f0, $f12
	bc1t	6f			# branch if 2^n*val == 0
	ctc1	t4, $31			# restore fp csr
	j	ra

6:
	/* result underflows; set errno = ERANGE */

	li      t0,ERANGE
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	ctc1	t4, $31			# restore fp csr
	j	ra

7:
	/* form val*2^52, and set n = n - 52 */

	l.d	$f0, twop52
	subu	a2, 52
	mul.d	$f12, $f0

	/* val is now a normal, so continue */

	mfc1	t0, $f13		# copy MSW of val
	sll	t1, t0, 1		# shift off sign bit of val
	srl	t1, 21			# t1 = exponent of val
	b	1b

big:	/* exponent(val) == 0x7ff */

	c.un.d	$f12, $f12
	bc1t	nan			# branch if val is a Nan

	li      t0,ERANGE
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif

	mov.d	$f0, $f12		# result = val
	ctc1	t4, $31			# restore fp csr
	j	ra

nan:	/* val is a Nan */

	li      t0,EDOM
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif

	l.d	$f0, _local_qnan	# result = quiet NaN
	ctc1	t4, $31			# restore fp csr
	j	ra

END(_ldexp)

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

LEAF(_ldexp)

	USE_ALT_CP(v0)
	SETUP_GP64(v0,_ldexp)

	dmfc1	t0, $f12		# copy val
	cfc1	a2, $31			# fetch fp csr
	and	a3, a2, 0xfffff07f
	ctc1	a3, $31			# disable exceptions
					# fp csr must be restored before returning!

	dsrl32	t1, t0, 20
	andi	t1, 0x7ff		# t1 = exponent of val
	beq	t1, 0, small		# branch if exponent(val) == 0
	beq	t1, 0x7ff, big		# branch if exponent(val) == 0x7ff
1:
	beq	a1, 0, 2f		# if n == 0, return val
	bge	a1, 0x7fe, over		# branch if n >= 0x7fe
	daddu	t2, t1, a1
	ble	t2, 0, under		# branch if exponent of val*2^n <= 0
	bge	t2, 0x7ff, over		# branch if exponent of val*2^n >= 0x7ff
	dsll32	a1, 20
	daddu	t0, a1
	dmtc1	t0, $f12
2:
	mov.d	$f0, $f12
	ctc1	a2, $31			# restore fp csr
	j	ra

under:	/* exponent(val*2^n) <= 0 */

	/* set exponent of val to 0x001 and adjust n */

	dli	t2, 0x001
	dsubu	t2, t1
	dsubu	a1, t2			# adjust n
	dsll32	t2, 20
	daddu	t0, t2
	dmtc1	t0, $f12

	/* set n = max(-54, n); result is the same */

	bge	a1, -54, 5f
	dli	a1, -54
	b	5f

over:	/* exponent(val*2^n) >= 0x7ff; set result to
	   val*2^n, and set errno to ERANGE
	*/

	/* set exponent of val to 0x7fe and multiply by 2.0 */

	dli	t2, 0x7fe
	dsubu	t2, t1
	dsll32	t2, 20
	daddu	t0, t2
	dmtc1	t0, $f12

	l.d	$f0, two
	mul.d	$f0, $f12

	li      t0,ERANGE
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	ctc1	a2, $31			# restore fp csr
	j	ra

small:	/* exponent(val) == 0 */

	/* depending on the mode of the processor, 1.0*arg
	 * may be zero
	 */

	l.d	$f0, one
	mul.d	$f12, $f0

	l.d	$f0, fzero
	c.eq.d	$f0, $f12
	bc1f	3f			# return if arg == +/-0.0

	mov.d	$f0, $f12
	ctc1	a2, $31			# restore fp csr
	j	ra

3:
	bne	a1, 0, 4f		# if n == 0, return val
	mov.d	$f0, $f12
	ctc1	a2, $31			# restore fp csr
	j	ra

4:
	bgt	a1, 52, 7f		# branch if n > 52

	/* if n < -53, set n = -53; result is the same */

	bge	a1, -53, 5f
	li	a1, -53

5:
	/* just form 2^n and multiply */

	daddi	a1, 0x3ff
	dsll32	a1, 20
	dmtc1	a1, $f0
	mul.d	$f0, $f12		# form 2^n*val
	l.d	$f12, fzero
	c.eq.d	$f0, $f12
	bc1t	6f			# branch if 2^n*val == 0
	ctc1	a2, $31			# restore fp csr
	j	ra

6:
	/* result underflows; set errno = ERANGE */

	li      t0,ERANGE
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif
	ctc1	a2, $31			# restore fp csr
	j	ra

7:
	/* form val*2^52, and set n = n - 52 */

	l.d	$f0, twop52
	dsubu	a1, 52
	mul.d	$f12, $f0

	/* val is now a normal, so continue */

	dmfc1	t0, $f12		# copy val
	dsrl32	t1, t0, 20		# t1 = exponent of val
	andi	t1, 0x7ff		# t1 = exponent of val
	b	1b

big:	/* exponent(val) == 0x7ff */

	c.un.d	$f12, $f12
	bc1t	nan			# branch if val is a Nan

	li      t0,ERANGE
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)

	mov.d	$f0, $f12		# result = val
	ctc1	a2, $31			# restore fp csr
	j	ra

nan:	/* val is a Nan */

	li      t0,EDOM
#if _ABIAPI
	sw	t0,errno
#else
        PTR_L   t1, __errnoaddr
        sw      t0,0(t1)
#endif

	l.d	$f0, _local_qnan	# result = quiet NaN
	ctc1	a2, $31			# restore fp csr
	j	ra

END(_ldexp)

#endif

