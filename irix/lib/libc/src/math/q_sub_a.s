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
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/q_sub_a.s,v 1.2 1997/04/17 23:00:34 vegas Exp $ */

#include <regdef.h>
#include <sys/asm.h>

#define	Swap	$fcc0
#define	Smallzz	$fcc1
#define	Zneg	$fcc2
#define	Zzneg	$fcc3

#define	xhi	$f12
#define	xlo	$f13
#define	yhi	$f14
#define	ylo	$f15
#define	qulp	$f12
#define	nqulp	$f13
#define	z	$f0
#define	tmp	$f1
#define	zz	$f2
#define	dzero	$f3
#define	lo	$f4
#define	absyhi	$f5
#define	rem	$f5
#define	w	$f6
#define	ww	$f7
#define	absxlo	$f8
#define	absylo	$f9
#define	u	$f8
#define	uu	$f9
#define	absz	$f10
#define	abszz	$f11

#define	iyhi	t0
#define	ixhi	t1
#define	iz	t2
#define	iw	ta1
#define	iqulp	ta2

.rdata

fourth:		.dword	0x3fd0000000000000
four:		.dword	0x4010000000000000
half:		.dword	0x3fe0000000000000
inf:		.dword	0x7ff0000000000000
twop914:	.dword	0x7910000000000000
fzero:		.dword	0x0000000000000000


	PICOPT
	.text

LEAF(__q_sub)

	# interchange x and y if xpt(xhi) < xpt(yhi)
	# interchange xlo and ylo if |xlo| < |ylo|

	neg.d	yhi, yhi
	neg.d	ylo, ylo
	dmfc1	ixhi, xhi
	dmfc1	iyhi, yhi
	dsrl32	ixhi, 20
	dsrl32	iyhi, 20
	abs.d	absxlo, xlo
	abs.d	absylo, ylo
	andi	ixhi, 0x7ff
	andi	iyhi, 0x7ff
	c.lt.d	Swap, absxlo, absylo
	dmtc1	$0, dzero	# build a zero in the f.p. unit
	sltu	t2, ixhi, iyhi
	movn	ixhi, iyhi, t2
	movn.d	$f7, xhi, t2
	movt.d	$f6, xlo, Swap
	movn.d	xhi, yhi, t2
	movt.d	xlo, ylo, Swap
	movn.d	yhi, $f7, t2
	movt.d	ylo, $f6, Swap
	sltiu	t3, ixhi, 0x7fd
	beq	t3, $0, bigarg	# branch if xpt(xhi) >= 0x7fd

	/* add xhi and yhi.  Then use the Kahan summation
	 * formula to add xlo and ylo to this sum.
	 * Finally, round the result to 107 bits by adding
	 * and subtracting a magic constant.
	 * In case of possible overflow, the operands must
	 * be scaled.
	 */

	add.d	z, xhi, yhi	# z = xhi + yhi
	add.d	u, xlo, ylo
	sub.d	zz, xhi, z
	sub.d	uu, xlo, u
	add.d	zz, yhi		# zz = xhi - z + yhi
	add.d	uu, ylo

	add.d	lo, zz, u	# lo = zz + u

	add.d	w, z, lo	# w = z + lo
	sub.d	rem, zz, lo
	sub.d	ww, z, w
	add.d	rem, u		# rem = zz - lo + u
	add.d	rem, uu
	add.d	ww, lo		# ww = z - w + lo

	add.d	ww, rem		# ww = ww + (rem + uu)

	add.d	z, w, ww	# z = w + ww
 	dmfc1	iz, z
 	sub.d	zz, w, z
	add.d	zz, ww		# zz = w - z + ww

 	c.lt.d	Zzneg, zz, dzero
 	abs.d	abszz, zz
 	dsrl32	iw, iz, 20
 	andi	iqulp, iw, 0x7ff
 	addi	iqulp, -54
 	dsll32	iqulp, 20
 	blez	iqulp, done
 	dmtc1	iqulp, qulp	# build a quarter ulp of z
 	dsll32	iw, 20		# w = twofloor(z)
 	c.lt.d	Smallzz, abszz, qulp
 	neg.d	nqulp, qulp
 	sltu	ta3, iw, iz
 	beq	ta3, $0, fix	# branch if z is a power of 2
 
 	movt.d	qulp, nqulp, Zzneg
 	movf.d	qulp, dzero, Smallzz

	# round zz if it's less than 1/4 ulp of z

 	add.d	w, zz, qulp
 	sub.d	zz, w, qulp
	j	ra

done:
	j	ra

fix:
  	USE_ALT_CP(v0)
  	SETUP_GPX64_L(v0, v1, l1)

	/* If z is a power of two, and z and zz have
	 * opposite signs, then we must adjust the size
	 * of qulp.
	 */

	c.lt.d	Zneg, z, dzero
	bc1t	Zneg, zneg
	bc1t	Zzneg, zzneg

	/* z >= 0.0, zz >= 0.0 */

 	movf.d	qulp, dzero, Smallzz

	# round zz if it's less than 1/4 ulp of z

	add.d	tmp, zz, qulp
	sub.d	zz, tmp, qulp
	j	ra

zneg:	/* z < 0.0 */
	bc1t	Zzneg, fin	# branch if zz < 0.0

zzneg:	/* z and zz have opposite signs */

	l.d	tmp, half
	mul.d	qulp, tmp	# adjust size of qulp(z)

	c.lt.d	Smallzz, abszz, qulp
	neg.d	nqulp, qulp
fin:
	movt.d	qulp, nqulp, Zzneg
	movf.d	qulp, dzero, Smallzz

	# round zz if it's less than 1/4 ulp of z

	add.d	tmp, zz, qulp
	sub.d	zz, tmp, qulp
	j	ra

bigarg:
  	USE_ALT_CP(v0)
  	SETUP_GPX64_L(v0, v1, l2)

	sltiu	t3, ixhi, 0x7ff
	beq	t3, $0, infnan	# branch if xpt(xhi) = 0x7ff
	l.d	tmp, twop914
	abs.d	absyhi, yhi
	c.lt.d	absyhi, tmp
	bc1t	small_y		# branch if |yhi| < 2**914

	/* avoid overflow in intermediate computations by
	 * computing 4.0*(.25*x + .25*y)
	 */

	l.d	tmp, fourth
	mul.d	xhi, tmp
	mul.d	xlo, tmp
	mul.d	yhi, tmp
	mul.d	ylo, tmp

	add.d	z, xhi, yhi	# z = xhi + yhi
	sub.d	zz, xhi, z
	add.d	zz, yhi		# zz = xhi - z + yhi

	add.d	lo, zz, xlo	# lo = zz + xlo

	add.d	w, z, lo	# w = z + lo
	sub.d	rem, zz, lo
	sub.d	ww, z, w
	add.d	rem, xlo	# rem = zz - lo + xlo
	add.d	rem, ylo
	add.d	ww, lo		# ww = z - w + lo

	add.d	ww, rem		# ww = ww + (rem + ylo)

	add.d	z, w, ww	# z = w + ww
	dmfc1	iz, z
 	sub.d	zz, w, z
	add.d	zz, ww		# zz = w - z + ww

	c.lt.d	Zzneg, zz, dzero
	abs.d	abszz, zz
	dsrl32	iw, iz, 20
	andi	iqulp, iw, 0x7ff
	addi	iqulp, -54
	dsll32	iqulp, 20
	blez	iqulp, rescale
	dmtc1	iqulp, qulp	# build a quarter ulp of z
	dsll32	iw, 20		# w = twofloor(z)
	c.lt.d	Smallzz, abszz, qulp
	neg.d	nqulp, qulp
	sltu	ta3, iw, iz	
	beq	ta3, $0, fix2	# branch if z is a power of 2

	movt.d	qulp, nqulp, Zzneg
	movf.d	qulp, dzero, Smallzz

	# round zz if it's less than 1/4 ulp of z

	add.d	w, zz, qulp
	sub.d	zz, w, qulp

rescale:
	l.d	tmp, four
	mul.d	z, tmp		# z *= 4.0
	abs.d	absz, z

	l.d	tmp, inf
	c.eq.d	tmp, absz
	bc1t	fix3		# branch if z == +/-inf
	l.d	tmp, four
	mul.d	zz, tmp		# zz *= 4.0
	j	ra

fix2:
	/* If z is a power of two, and z and zz have
	 * opposite signs, then we must adjust the size
	 * of qulp.
	 */

	c.lt.d	Zneg, z, dzero
	bc1t	Zneg, zneg2
	bc1t	Zzneg, zzneg2

	/* z >= 0.0, zz >= 0.0 */

	movf.d	qulp, dzero, Smallzz

	# round zz if it's less than 1/4 ulp of z

	add.d	tmp, zz, qulp
	sub.d	zz, tmp, qulp
	b	rescale

zneg2:	/* z < 0.0 */
	bc1t	Zzneg, fin2	# branch if zz < 0.0

zzneg2:	/* z and zz have opposite signs */

	l.d	tmp, half
	mul.d	qulp, tmp

	c.lt.d	Smallzz, abszz, qulp
	neg.d	nqulp, qulp
fin2:
	movt.d	qulp, nqulp, Zzneg
	movf.d	qulp, dzero, Smallzz

	# round zz if it's less than 1/4 ulp of z

	add.d	tmp, zz, qulp
	sub.d	zz, tmp, qulp
	b	rescale


small_y:
	mov.d	z, xhi
	mov.d	zz, xlo
	j	ra		# result is xhi + xlo

fix3:
	l.d	zz, fzero	# set low part of result to zero
	j	ra

infnan:
	add.d	z, xhi, yhi
	l.d	zz, fzero
	j	ra

END(__q_sub)

