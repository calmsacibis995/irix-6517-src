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
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsc/ml/RCS/dwmul.s,v 1.2 1994/11/09 21:59:14 sfc Exp $ */

#include	"ml.h"
#include <regdef.h>
/* sgi: align things: thus we must have a different set of macros */
#ifdef sgi
#ifdef _MIPSEL
#	define RLSW 0(a0)
#	define RMSW 4(a0)
#	define ALSW a2
#	define AMSW a3
#	define BLSW 16(2p)
#	define BMSW 20(sp)
#else /* _MIPSEB */
#	define RMSW 0(a0)
#	define RLSW 4(a0)
#	define AMSW a2
#	define ALSW a3
#	define BMSW 16(sp)
#	define BLSW 20(sp)
#endif
#	define BINT a1  /* unused reg in critical spot */

#else
#ifdef _MIPSEL
#	define RLSW 0(a0)
#	define RMSW 4(a0)
#	define ALSW a1
#	define AMSW a2
#	define BLSW a3
#	define BMSW 16(sp)
#else /* _MIPSEB */
#	define RMSW 0(a0)
#	define RLSW 4(a0)
#	define AMSW a1
#	define ALSW a2
#	define BMSW a3
#	define BLSW 16(sp)
#endif
#	define BINT a3
#endif

.text

/* dwmul (dw *result, dw a, dw b)	*/

.globl __dwmul
.ent __dwmul
__dwmul:
	.frame	sp, 0, ra
	move	t0, ALSW
#ifdef sgi
	lw	t2, BLSW
#else
#ifdef _MIPSEL
	move	t2, BLSW
#else
	lw	t2, BLSW
#endif
#endif
	multu	t0, t2
	move	t1, AMSW
#ifdef sgi
	lw	t3, BMSW
#else
#ifdef _MIPSEL
	lw	t3, BMSW
#else
	move	t3, BMSW
#endif
#endif
	mflo	v0
	mfhi	v1
	multu	t1, t2
	mflo	t5
	addu	v1, t5
	multu	t0, t3
	mflo	t5
	addu	v1, t5
	sw	v0, RLSW
	sw	v1, RMSW
	j	ra
.end __dwmul

/* dwmuli (dw *result, dw a, unsigned b) */
.globl __dwmuli
.ent __dwmuli
__dwmuli:
	.frame	sp, 0, ra
#ifdef sgi
	lw	BINT,16(sp)   /* alignment moves this off to a1. */
#endif
	multu	ALSW, BINT
	mflo	v0
	mfhi	v1
	multu	AMSW, BINT
	mflo	t5
	addu	v1, t5
	sw	v0, RLSW
	sw	v1, RMSW
	j	ra
.end __dwmuli

/* Create wrapper routines that translate long long to dw */

/***** Wrapper functions now obsolete in v3.10

#define LL2	__ll_mul
#define DW2	__dw_mul
#include "wrapper.i"
#undef LL2
#undef DW2

#define LL2	__ll_lshift
#define DW2	__dw_lshift
#include "wrapper.i"
#undef LL2
#undef DW2

#define LL2	__ll_rshift
#define DW2	__dw_rshift
#include "wrapper.i"

*****/

