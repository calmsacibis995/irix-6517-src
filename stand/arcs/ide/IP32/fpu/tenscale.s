/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP32/fpu/RCS/tenscale.s,v 1.2 1997/05/15 16:07:55 philw Exp $ */

#include <regdef.h>

/* t3/t2/t1/t0 = t5/t4 * 10^t0, return binary exponent of 10^t0 in v1. */
/* Bashes a3/a2. */

.globl _tenscale
.ent _tenscale
_tenscale:
	bltz	t0, 10f
	subu	t0, 27
	bgtz	t0, 10f
	sll	t0, 1
	lh	v1, _ptenexp+27*2(t0)
	sll	t0, 2
	ld	t6, _pten+27*8(t0)
	j	_dwmultu

10:	subu	sp, 16
	sd	t4, 0(sp)
	sw	ra, 8(sp)
	sra	t3, t0, 4
	sll	t3, 2
	lw	t3, _ptenround(t3)
	sll	t2, t0, 1
	sll	t3, t2
	sra	t3, 30
	sw	t3, 12(sp)
	bltz	t0, 20f
	li	t1, (27-1)*2
11:	addu	t1, 1*2
	subu	t0, 28
	bgez	t0, 11b
12:	lh	v1, _ptenexp(t1)
	sll	t1, 2
#ifdef MIPSEL
	ld	t6, _pten(t1)
#else
	lw	t6, _pten+0(t1)
	lw	t7, _pten+4(t1)
#endif
	sll	t0, 1
	lh	t2, _ptenexp+28*2(t0)
	addu	v1, t2
	addu	v1, 1
	sll	t0, 2
#ifdef MIPSEL
	ld	t4, _pten+28*8(t0)
#else
	lw	t4, _pten+0+28*8(t0)
	lw	t5, _pten+4+28*8(t0)
#endif
	jal	_dwmultu
	bltz	t3, 13f
	sll	t3, 1
	srl	t0, t2, 31
	or	t3, t0
	sll	t2, 1
	srl	t0, t1, 31
	or	t2, t0
	sll	t1, 1
	subu	v1, 1
13:	lw	t0, 12(sp)
	srl	t1, 31
	addu	t6, t2, t1
	addu	t6, t0
	move	t7, t3
	ld	t4, 0(sp)
	lw	ra, 8(sp)
	addu	sp, 16
	j	_dwmultu

20:	li	t1, 0
21:	subu	t1, 1*2
	addu	t0, 28
	bltz	t0, 21b
	subu	t0, 28
	b	12b
.end _tenscale

.rdata
.align 3
	.word 0xfbd14d6e, 0xe1afa13a /* -364 */
	.word 0x4d8d98b8, 0xe3e27a44 /* -336 */
	.word 0x3d1a45df, 0xe61acf03 /* -308 */
	.word 0x8f5c22ca, 0xe858ad24 /* -280 */
	.word 0x23ee8bcb, 0xea9c2277 /* -252 */
	.word 0x4a314ebe, 0xece53cec /* -224 */
	.word 0x172aace5, 0xef340a98 /* -196 */
	.word 0xbc3f8ca2, 0xf18899b1 /* -168 */
	.word 0xdec3f126, 0xf3e2f893 /* -140 */
	.word 0xf065d37d, 0xf64335bc /* -112 */
	.word 0x88747d94, 0xf8a95fcf /* -84 */
	.word 0xbe068d2f, 0xfb158592 /* -56 */
	.word 0x8300ca0e, 0xfd87b5f2 /* -28 */
_pten:
	.word 0x00000000, 0x80000000 /* 0 */
	.word 0x00000000, 0xa0000000 /* 1 */
	.word 0x00000000, 0xc8000000 /* 2 */
	.word 0x00000000, 0xfa000000 /* 3 */
	.word 0x00000000, 0x9c400000 /* 4 */
	.word 0x00000000, 0xc3500000 /* 5 */
	.word 0x00000000, 0xf4240000 /* 6 */
	.word 0x00000000, 0x98968000 /* 7 */
	.word 0x00000000, 0xbebc2000 /* 8 */
	.word 0x00000000, 0xee6b2800 /* 9 */
	.word 0x00000000, 0x9502f900 /* 10 */
	.word 0x00000000, 0xba43b740 /* 11 */
	.word 0x00000000, 0xe8d4a510 /* 12 */
	.word 0x00000000, 0x9184e72a /* 13 */
	.word 0x80000000, 0xb5e620f4 /* 14 */
	.word 0xa0000000, 0xe35fa931 /* 15 */
	.word 0x04000000, 0x8e1bc9bf /* 16 */
	.word 0xc5000000, 0xb1a2bc2e /* 17 */
	.word 0x76400000, 0xde0b6b3a /* 18 */
	.word 0x89e80000, 0x8ac72304 /* 19 */
	.word 0xac620000, 0xad78ebc5 /* 20 */
	.word 0x177a8000, 0xd8d726b7 /* 21 */
	.word 0x6eac9000, 0x87867832 /* 22 */
	.word 0x0a57b400, 0xa968163f /* 23 */
	.word 0xcceda100, 0xd3c21bce /* 24 */
	.word 0x401484a0, 0x84595161 /* 25 */
	.word 0x9019a5c8, 0xa56fa5b9 /* 26 */
	.word 0xf4200f3a, 0xcecb8f27 /* 27 */
	.word 0xcfe20766, 0xd0cf4b50 /* 55 */
	.word 0x2aabd62c, 0xd2d80db0 /* 83 */
	.word 0xc1d1ea96, 0xd4e5e2cd /* 111 */
	.word 0x9292d603, 0xd6f8d750 /* 139 */
	.word 0x28069da4, 0xd910f7ff /* 167 */
	.word 0xe9d0696a, 0xdb2e51bf /* 195 */
	.word 0x6b947519, 0xdd50f199 /* 223 */
	.word 0xbd342cf7, 0xdf78e4b2 /* 251 */
	.word 0xbbd26451, 0xe1a63853 /* 279 */
	.word 0x63a198e5, 0xe3d8f9e5 /* 307 */

	.word 0x000000cc
	.word 0x00000000
	.word 0x00000014
	.word 0x45010004
	.word 0x01000000
	.word 0x030cc0cc
	.word 0x30011010
	.word 0x00040444
	.word 0x030300c0
	.word 0x3c3c0c03
	.word 0xcf30cf0c
	.word 0x00c00c00
	.word 0x00000000
	.word 0x00005001
	.word 0x44000010
	.word 0x00100400
	.word 0x00040100
	.word 0x00140001
	.word 0x10000000
	.word 0x00300300
	.word 0x00333c30
	.word 0xcf00303c
_ptenround:
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00003000
	.word 0x03000000
	.word 0x00050010
	.word 0x11554000
	.word 0x04000401
	.word 0x44400100
	.word 0x04001104
	.word 0x04100000
	.word 0x00000000
	.word 0x000cf000
	.word 0x00000003
	.word 0x00cc30c0
	.word 0x0030c004
	.word 0x44010141
	.word 0x40000001
	.word 0x40000000


	.half -1210 /* -364 */
	.half -1117 /* -336 */
	.half -1024 /* -308 */
	.half -931 /* -280 */
	.half -838 /* -252 */
	.half -745 /* -224 */
	.half -652 /* -196 */
	.half -559 /* -168 */
	.half -466 /* -140 */
	.half -373 /* -112 */
	.half -280 /* -84 */
	.half -187 /* -56 */
	.half -94 /* -28 */
_ptenexp:
	.half 0 /* 0 */
	.half 3 /* 1 */
	.half 6 /* 2 */
	.half 9 /* 3 */
	.half 13 /* 4 */
	.half 16 /* 5 */
	.half 19 /* 6 */
	.half 23 /* 7 */
	.half 26 /* 8 */
	.half 29 /* 9 */
	.half 33 /* 10 */
	.half 36 /* 11 */
	.half 39 /* 12 */
	.half 43 /* 13 */
	.half 46 /* 14 */
	.half 49 /* 15 */
	.half 53 /* 16 */
	.half 56 /* 17 */
	.half 59 /* 18 */
	.half 63 /* 19 */
	.half 66 /* 20 */
	.half 69 /* 21 */
	.half 73 /* 22 */
	.half 76 /* 23 */
	.half 79 /* 24 */
	.half 83 /* 25 */
	.half 86 /* 26 */
	.half 89 /* 27 */
	.half 182 /* 55 */
	.half 275 /* 83 */
	.half 368 /* 111 */
	.half 461 /* 139 */
	.half 554 /* 167 */
	.half 647 /* 195 */
	.half 740 /* 223 */
	.half 833 /* 251 */
	.half 926 /* 279 */
	.half 1019 /* 307 */

