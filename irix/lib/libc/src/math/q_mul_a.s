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
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/q_mul_a.s,v 1.2 1997/04/17 23:00:32 vegas Exp $ */

#include <regdef.h>
#include <sys/asm.h>

#define	Zneg	$fcc0
#define	Zzneg	$fcc1
#define	Smallzz	$fcc2

#define	xhi	$f12
#define	qulp	$f12
#define	xlo	$f13
#define	nqulp	$f13
#define	abszz	$f11
#define	yhi	$f14
#define	ylo	$f15
#define	xfactor	$f16
#define	yfactor	$f17
#define	z	$f0
#define	zz	$f2
#define	lo	$f4
#define	rem	$f11
#define	ulp	$f5
#define	w	$f5
#define	ww	$f6
#define	c1	$f0
#define	dzero	$f1
#define	p	$f1
#define	hx	$f2
#define	tx	$f3
#define	q	$f4
#define	hy	$f5
#define	ty	$f6
#define	u	$f7
#define	uu	$f8
#define	tmp1	$f9
#define	tmp2	$f10
#define	a	$f2
#define	v	$f3
#define	vv	$f4

#define	xptxhi	t0
#define	xptyhi	t1
#define	xpthi	ta0
#define	xptlo	ta1
#define	iz	t2
#define	iw	ta1
#define	iqulp	ta2
#define	scaleup	ta0
#define	scaledown ta1

.rdata

const1:		.dword	0x41a0000002000000
fzero:		.dword	0x0000000000000000
half:		.dword	0x3fe0000000000000
inf:		.dword	0x7ff0000000000000
one:		.dword	0x3ff0000000000000
twopm590:	.dword	0x1b10000000000000
twop590:	.dword	0x64d0000000000000
qnan:		.dword  0x7ff1000000000000


	PICOPT
	.text

LEAF(__q_mul)

  	USE_ALT_CP(v0)
  	SETUP_GPX64_L(v0, v1, l1)

	# screen large and small args

	dmfc1	xptxhi, xhi
	dmfc1	xptyhi, yhi
	dsrl32	xptxhi, 20
	dsrl32	xptyhi, 20
	andi	xptxhi, 0x7ff
	andi	xptyhi, 0x7ff
	sltu	t2, xptxhi, xptyhi
	move	xpthi, xptxhi
	move	xptlo, xptyhi
	movn	xpthi, xptyhi, t2	# larger exponent to xpthi
	movn	xptlo, xptxhi, t2	# smaller exponent to xptlo
	sltiu	t3, xpthi, 0x5fd
	beq	t3, $0, scale	# branch if xpthi >= 0x5fd
	sltiu	t3, xptlo, 0x21b
	bne	t3, $0, scale	# branch if xptlo < 0x21b

	/* Use Dekker's algorithm to compute the exact product
	 * (u, uu) of xhi and yhi.
	 */

	l.d	c1, const1
	mul.d	p, xhi, c1	# p = xhi*c1

	sub.d	hx, xhi, p
	add.d	hx, p		# hx = (xhi - p) + p
	sub.d	tx, xhi, hx	# tx = xhi - hx

	dmtc1	$0, dzero	# build a zero in the f.p. unit
	mul.d	q, yhi, c1	# q = yhi*c1

	sub.d	hy, yhi, q
	add.d	hy, q		# hy = (yhi - q) + q
	sub.d	ty, yhi, hy	# ty = yhi - hy

	mul.d	u, xhi, yhi	# u = xhi*yhi

	mul.d	uu, hx, hy
	sub.d	uu, u
	mul.d	tmp1, hx, ty
	add.d	uu, tmp1
	mul.d	tmp2, hy, tx
	add.d	uu, tmp2
	mul.d	tmp1, tx, ty
	add.d	uu, tmp1	# uu = hx*hy - u + hx*ty + hy*tx + tx*ty

	/* Now use the Kahan summation formula to add xhi*ylo, xlo*yhi,
	 * and xlo*ylo to (u, uu).
	 */

	mul.d	a, xhi, ylo	# a = xhi*ylo

	add.d	ww, uu, a	# ww = uu + a

	sub.d	rem, uu, ww
	add.d	rem, a		# rem = (uu - ww) + a

	add.d	v, u, ww	# v = u + ww
	sub.d	vv, u, v
	add.d	vv, ww		# vv = u - v + ww

	mul.d	tmp1, xlo, yhi
	add.d	a, tmp1, rem	# a = xlo*yhi + rem

	add.d	uu, vv, a	# uu = vv + a

	sub.d	rem, vv, uu
	add.d	rem, a		# rem = (vv - uu) + a

	add.d	w, v, uu	# w = v + uu
	sub.d	ww, v, w
	add.d	ww, uu		# ww = v - w + uu

	mul.d	tmp1, xlo, ylo
	add.d	a, tmp1, rem	# a = xlo*ylo + rem

	add.d	vv, ww, a	# vv = ww + a

	add.d	z, w, vv	# z = w + vv
 	dmfc1	iz, z
	sub.d	zz, w, z
	add.d	zz, vv		# zz = w - z + vv

	/* Finally, round (z, zz) to 107 bits if necessary by
	 * adding and subtracting a quarter ulp of z to zz.
	 */
back:
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
 	sltu	ta3, xptlo, t2	
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

	add.d	tmp1, zz, qulp
	sub.d	zz, tmp1, qulp
	j	ra

zneg:	/* z < 0.0 */
	bc1t	Zzneg, fin	# branch if zz < 0.0

zzneg:	/* z and zz have opposite signs */

	l.d	tmp1, half
	mul.d	qulp, tmp1	# adjust size of qulp(z)

	c.lt.d	Smallzz, abszz, qulp
	neg.d	nqulp, qulp
fin:
	movt.d	qulp, nqulp, Zzneg
	movf.d	qulp, dzero, Smallzz

	# round zz if it's less than 1/4 ulp of z

	add.d	tmp1, zz, qulp
	sub.d	zz, tmp1, qulp
	j	ra

scale:
	sltiu	t2, xptxhi, 0x7ff
	beq	t2, $0, inf_nan
	sltiu	t2, xptyhi, 0x7ff
	beq	t2, $0, inf_nan

	l.d	dzero, fzero
	c.eq.d	xhi, dzero
	bc1t	hiprod
	c.eq.d	yhi, dzero
	bc1t	hiprod

	li	scaleup, 0
	li	scaledown, 0
	l.d	xfactor, one
	l.d	yfactor, one

	sltiu	t2, xptxhi, 0x21b
	beq	t2, $0, 1f

	l.d	tmp1, twop590
	mul.d	xhi, tmp1
	mul.d	xlo, tmp1
	l.d	xfactor, twopm590
	li	scaleup, 1

1:
	sltiu	t2, xptyhi, 0x21b
	beq	t2, $0, 1f

	l.d	tmp1, twop590
	mul.d	yhi, tmp1
	mul.d	ylo, tmp1
	l.d	yfactor, twopm590
	li	scaleup, 1

1:
	sltiu	t2, xptxhi, 0x5fd
	bne	t2, $0, 1f

	l.d	tmp1, twopm590
	mul.d	xhi, tmp1
	mul.d	xlo, tmp1
	l.d	xfactor, twop590
	li	scaledown, 1

1:
	sltiu	t2, xptyhi, 0x5fd
	bne	t2, $0, 1f

	l.d	tmp1, twopm590
	mul.d	yhi, tmp1
	mul.d	ylo, tmp1
	l.d	yfactor, twop590
	li	scaledown, 1

1:
	beq	scaleup, $0, 1f
	beq	scaledown, $0, 1f
	l.d	xfactor, one
	l.d	yfactor, one

1:
	/* Use Dekker's algorithm to compute the exact product
	 * (u, uu) of xhi and yhi.
	 */

	l.d	c1, const1
	mul.d	p, xhi, c1	# p = xhi*c1

	sub.d	hx, xhi, p
	add.d	hx, p		# hx = (xhi - p) + p
	sub.d	tx, xhi, hx	# tx = xhi - hx

	dmtc1	$0, dzero	# build a zero in the f.p. unit
	mul.d	q, yhi, c1	# q = yhi*c1

	sub.d	hy, yhi, q
	add.d	hy, q		# hy = (yhi - q) + q
	sub.d	ty, yhi, hy	# ty = yhi - hy

	mul.d	u, xhi, yhi	# u = xhi*yhi

	mul.d	uu, hx, hy
	sub.d	uu, u
	mul.d	tmp1, hx, ty
	add.d	uu, tmp1
	mul.d	tmp2, hy, tx
	add.d	uu, tmp2
	mul.d	tmp1, tx, ty
	add.d	uu, tmp1	# uu = hx*hy - u + hx*ty + hy*tx + tx*ty

	/* Now use the Kahan summation formula to add xhi*ylo, xlo*yhi,
	 * and xlo*ylo to (u, uu).
	 */

	mul.d	a, xhi, ylo	# a = xhi*ylo

	add.d	ww, uu, a	# ww = uu + a

	sub.d	rem, uu, ww
	add.d	rem, a		# rem = (uu - ww) + a

	add.d	v, u, ww	# v = u + ww
	sub.d	vv, u, v
	add.d	vv, ww		# vv = u - v + ww

	mul.d	tmp1, xlo, yhi
	add.d	a, tmp1, rem	# a = xlo*yhi + rem

	add.d	uu, vv, a	# uu = vv + a

	sub.d	rem, vv, uu
	add.d	rem, a		# rem = (vv - uu) + a

	add.d	w, v, uu	# w = v + uu
	sub.d	ww, v, w
	add.d	ww, uu		# ww = v - w + uu

	mul.d	tmp1, xlo, ylo
	add.d	a, tmp1, rem	# a = xlo*ylo + rem

	add.d	vv, ww, a	# vv = ww + a

	add.d	z, w, vv	# z = w + vv
	sub.d	zz, w, z
	add.d	zz, vv		# zz = w - z + vv

	/* Rescale z and zz before rounding */

	mul.d	w, z, xfactor	# w = z*xfactor
	mul.d	w, yfactor	# w *= yfactor

	c.eq.d	w, dzero
	bc1t	inf_zero	# branch if w is 0.0
	l.d	tmp1, inf
	abs.d	tmp2, w		# tmp2 = fabs(w)
	c.eq.d	tmp1, tmp2
	bc1t	inf_zero	# branch if w is inf
	mul.d	ww, zz, xfactor # ww = zz*xfactor
	mul.d	ww, yfactor	# ww *= yfactor

	add.d	z, w, ww	# z = w + ww
	dmfc1	iz, z
	sub.d	zz, w, z
	add.d	zz, ww		# zz = w - z + ww
	b	back

hiprod:
	mul.d	z, xhi, yhi
	mov.d	zz, dzero
	j	ra

inf_nan:
	c.un.d	xhi, yhi
	bc1f	hiprod
	l.d	z, qnan
	l.d	zz, fzero
	j	ra


inf_zero:
	mov.d	z, w
	mov.d	zz, dzero
	j	ra

END(__q_mul)

