#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _MIPS_SIM_ABI32)
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
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsc/ml/RCS/llmul.s,v 1.3 1995/11/20 08:51:24 jeffs Exp $ */

#include	"ml.h"
#include <regdef.h>

/* long long __ll_mul (long long a, long long b) */

#ifdef _MIPSEL
#	define RLSW v0
#	define RMSW v1
#	define ALSW a0
#	define AMSW a1
#	define BLSW a2
#	define BMSW a3
#else /* _MIPSEB */
#	define RMSW v0
#	define RLSW v1
#	define AMSW a0
#	define ALSW a1
#	define BMSW a2
#	define BLSW a3
#endif

.text

/* note that result of AMSW * BMSW overflows 64bits, so it is ignored. */

.globl __ll_mul
.ent __ll_mul
__ll_mul:
	.frame	sp, 0, ra
	multu	ALSW, BLSW
	mflo	RLSW
	mfhi	RMSW
	multu	AMSW, BLSW
	mflo	t0
	addu	RMSW, t0
	multu	ALSW, BMSW
	mflo	t0
	addu	RMSW, t0
	j	ra
.end __ll_mul
#endif
