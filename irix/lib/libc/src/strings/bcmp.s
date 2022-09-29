/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: bcmp.s,v 1.11 1999/11/22 17:04:21 leedom Exp $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/* bcmp(s1, s2, n) */

#ifdef	ISBCMP
.weakext bcmp, _bcmp
#else
/* memcmp is ansi defined so no weak symbol is needed */
#endif

#include "sys/regdef.h"
#include "sys/asm.h"

/*
 * bcmp(src, dst, bcount)
 *
 * MINCMP is minimum number of byte that its worthwhile to try and
 * align cmp into word transactions
 *
 * Calculating MINCMP
 * Overhead =~ 15 instructions => 90 cycles
 * Byte cmp =~ 38 cycles/word
 * Word cmp =~ 17 cycles/word
 * Breakeven =~ 16 bytes
 *
 * There's a bug in the R12KS CPU that will sometimes cause a LWR or LWL
 * instruction to corrupt the portion of the destination register that it's
 * not supposed to touch.  In general LWR and LWL instructions are always
 * paired so this isn't a problem.  bcmp() was being fancy with the initial
 * compare of unaligned comparisons when the two source strings had matching
 * misalignments and only doing single LWL's (LWR's for little endian) for the
 * comparisons.  Since we know that the bcmp() is for at least 16 bytes at the
 * leading comparison point, it's easy to just put the paired instruction in
 * with very little performance impact.
 */
#define	MINCMP	16
#define	NBPW	4

#ifdef	ISBCMP
LEAF(_bcmp)
#else
LEAF(memcmp)
#endif
	xor	v0,a0,a1
	blt	a2,MINCMP,bytecmp	# too short, just byte cmp
	and	v0,NBPW-1
	PTR_SUBU	t8,zero,a0	# number of bytes til aligned
	bne	v0,zero,unalgncmp	# src and dst not alignable
/*
 * src and dst can be simultaneously word aligned
 */
	and	t8,NBPW-1
	PTR_SUBU	a2,t8
	beq	t8,zero,wordcmp		# already aligned
#ifdef MIPSEB
	lwl	v0,0(a0)		# cmp unaligned portion
	lwr	v0,3(a0)		# WAR: the R12KS needs LWL/LWR paired
	lwl	v1,0(a1)
	lwr	v1,3(a1)		# WAR: the R12KS needs LWL/LWR paired
#endif
#ifdef MIPSEL
	lwr	v0,0(a0)
	lwl	v0,3(a0)		# WAR: the R12KS needs LWL/LWR paired
	lwr	v1,0(a1)
	lwl	v1,3(a1)		# WAR: the R12KS needs LWL/LWR paired
#endif
	PTR_ADDU	a0,t8
	PTR_ADDU	a1,t8
	bne	v0,v1,cmpne

/*
 * word cmp loop
 */
wordcmp:
	and	a3,a2,~(NBPW-1)
	PTR_SUBU	a2,a3
	beq	a3,zero,bytecmp
	PTR_ADDU	a3,a0		# src1 endpoint
1:	lw	v0,0(a0)
	lw	v1,0(a1)
	PTR_ADDU	a0,NBPW		# 1st BDSLOT
	PTR_ADDU	a1,NBPW		# 2nd BDSLOT (asm doesn't move)
	bne	v0,v1,cmpne
	bne	a0,a3,1b		# at least one more word
	b	bytecmp

/*
 * deal with simultaneously unalignable cmp by aligning one src
 */
unalgncmp:
	PTR_SUBU	a3,zero,a1	# calc byte cnt to get src2 aligned
	and	a3,NBPW-1
	PTR_SUBU	a2,a3
	beq	a3,zero,partaligncmp	# already aligned
	PTR_ADDU	a3,a0		# src1 endpoint
1:	lbu	v0,0(a0)
	lbu	v1,0(a1)
	PTR_ADDU	a0,1
	PTR_ADDU	a1,1
	bne	v0,v1,cmpne
	bne	a0,a3,1b

/*
 * src unaligned, dst aligned loop
 */
partaligncmp:
	and	a3,a2,~(NBPW-1)
	PTR_SUBU	a2,a3
	beq	a3,zero,bytecmp
	PTR_ADDU	a3,a0
1:
#ifdef MIPSEB
	lwl	v0,0(a0)
	lwr	v0,3(a0)
#endif
#ifdef MIPSEL
	lwr	v0,0(a0)
	lwl	v0,3(a0)
#endif
	lw	v1,0(a1)
	PTR_ADDU	a0,NBPW
	PTR_ADDU	a1,NBPW
	bne	v0,v1,cmpne
	bne	a0,a3,1b

/*
 * brute force byte cmp loop
 */
bytecmp:
	PTR_ADDU	a3,a2,a0	# src1 endpoint; BDSLOT
	ble	a2,zero,cmpdone
1:	lbu	v0,0(a0)
	lbu	v1,0(a1)
	PTR_ADDU	a0,1
	PTR_ADDU	a1,1
	bne	v0,v1,cmpne
	bne	a0,a3,1b
cmpdone:
	move	v0,zero	
	j	ra

cmpne:
#ifndef ISBCMP
	sltu	a2,v1,v0
	bne	a2,zero,9f
	li	v0,-1
	j	ra
9:
#endif
	li	v0,1
	j	ra
#ifdef	ISBCMP
.end _bcmp
#else
.end memcmp
#endif
