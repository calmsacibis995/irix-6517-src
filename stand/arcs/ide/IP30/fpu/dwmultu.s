/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP30/fpu/RCS/dwmultu.s,v 1.2 1996/11/08 03:02:17 rattan Exp $ */

#include <regdef.h>

/* Multiply 64-bit integers in ta1/ta0 and ta3/ta2 to produced 128-bit
   product in t3/t2/t1/t0.  Little-endian register order.  Bashes
   a3/a2.  ta1/ta0 and ta3/ta2 unchanged.  63 cycles. */
.globl _dwmultu
.ent _dwmultu
_dwmultu:
	.frame	sp, 0, ra
	multu	ta0, ta2		# x0 * y0
	## 10 cycle interlock
	mflo	t0		# lo(x0 * y0)
	mfhi	t1		# hi(x0 * y0)
	not	a3, t1
	## 1 nop
	multu	ta1, ta2		# x1 * y0
	## 10 cycle interlock
	mflo	a2		# lo(x1 * y0)
	mfhi	t2		# hi(x1 * y0)
	sltu	a3, a3, a2	# carry(hi(x0 * y0) + lo(x1 * y0))
	addu	t1, a2		# hi(x0 * y0) + lo(x1 * y0)
	multu	ta0, ta3		# x0 * y1
	add	t2, a3		# hi(x1 * y0) + carry
	not	a3, t1
	## 8 cycle interlock
	mflo	a2		# lo(x0 * y1)
	mfhi	t3		# hi(x0 * y1)
	sltu	a3, a3, a2	# carry((hi(x0 * y0) + lo(x1 * y0)) + lo(x0 * y1))
	addu	t1, a2		# hi(x0 * y0) + lo(x1 * y0) + lo(x0 * y1)
	multu	ta1, ta3		# x1 * y1
	add	t2, a3		# hi(x1 * y0) + carry + carry
	not	a2, t2
	sltu	a2, a2, t3	# carry(hi(x1 * y0) + hi(x0 * y1))
	addu	t2, t3		# hi(x1 * y0) + hi(x0 * y1))
	not	a3, t2
	## 5 cycle interlock
	mfhi	t3		# hi(x1 * y1)
	add	t3, a2		# hi(x1 * y1) + carry(hi(x1 * y0) + hi(x0 * y1))
	mflo	a2		# lo(x1 * y1)
	sltu	a3, a3, a2	# carry((hi(x1 * y0) + hi(x0 * y1)) + lo(x1 * y1))
	add	t3, a3
	addu	t2, a2		# hi(x1 * y0) + hi(x0 * y1) + lo(x1 * y1)
	j	ra
.end _dwmultu
