/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP32/fpu/RCS/dwmultu.s,v 1.2 1997/05/15 16:07:48 philw Exp $ */

#include <regdef.h>

/* Multiply 64-bit integers in t5/t4 and t7/t6 to produced 128-bit
   product in t3/t2/t1/t0.  Little-endian register order.  Bashes
   a3/a2.  t5/t4 and r7/t6 unchanged.  63 cycles. */
.globl _dwmultu
.ent _dwmultu
_dwmultu:
	.frame	sp, 0, ra
	multu	t4, t6		# x0 * y0
	## 10 cycle interlock
	mflo	t0		# lo(x0 * y0)
	mfhi	t1		# hi(x0 * y0)
	not	a3, t1
	## 1 nop
	multu	t5, t6		# x1 * y0
	## 10 cycle interlock
	mflo	a2		# lo(x1 * y0)
	mfhi	t2		# hi(x1 * y0)
	sltu	a3, a3, a2	# carry(hi(x0 * y0) + lo(x1 * y0))
	addu	t1, a2		# hi(x0 * y0) + lo(x1 * y0)
	multu	t4, t7		# x0 * y1
	add	t2, a3		# hi(x1 * y0) + carry
	not	a3, t1
	## 8 cycle interlock
	mflo	a2		# lo(x0 * y1)
	mfhi	t3		# hi(x0 * y1)
	sltu	a3, a3, a2	# carry((hi(x0 * y0) + lo(x1 * y0)) + lo(x0 * y1))
	addu	t1, a2		# hi(x0 * y0) + lo(x1 * y0) + lo(x0 * y1)
	multu	t5, t7		# x1 * y1
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
