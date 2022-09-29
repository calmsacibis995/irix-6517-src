/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "$Id: bzero.s,v 1.23 1997/03/14 00:33:32 cleo Exp $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

#include "sys/regdef.h"
#include "sys/asm.h"

.extern _blk_fp 4			# extern _bcopyfp flag

SAVREG6=	6			# save gp, ra, v0, a0, a1, a2 regs
SZFRAME=	(((NARGSAVE+SAVREG6)*SZREG)+ALSZ)&ALMASK
GPOFF=		(SZFRAME-(1*SZREG))
RAOFF=		(SZFRAME-(2*SZREG))
V0OFF=		(SZFRAME-(3*SZREG))
A0OFF=		(SZFRAME-(4*SZREG))	
A1OFF=		(SZFRAME-(5*SZREG))
A2OFF=		(SZFRAME-(6*SZREG))

#define NBPW	SZREG

#ifdef _MIPSEB
#    define SWS REG_SLEFT
#    define SWB REG_SRIGHT
#else
#    define SWS REG_SRIGHT
#    define SWB REG_SLEFT
#endif /* _MIPSEB */

#define LW	REG_L
#define SW	REG_S
#define ADDU	PTR_ADDU
#define ADDIU	PTR_ADDIU
#define SUBU	PTR_SUBU
#define LADDU	LONG_ADDU
#define LSUBU	LONG_SUBU

#define MINFPZERO	384	/* _bzero_fp threshold value */
#define	MINSET		8

/*
 * _bzero(dst, bcount)
 * Zero block of memory
 *
 * Calculating MINZERO, assuming 50% cache-miss on non-loop code:
 * Overhead =~ 18 instructions => 63 (81) cycles
 * Byte zero =~ 16 (24) cycles/word for 08M44 (08V11)
 * Word zero =~ 3 (6) cycles/word for 08M44 (08V11)
 * If I-cache-miss nears 0, MINZERO ==> 4 bytes; otherwise, times are:
 * breakeven (MEM) = 63 / (16 - 3) =~ 5 words
 * breakeven (VME) = 81 / (24 - 6)  =~ 4.5 words
 * Since the overhead is pessimistic (worst-case alignment), and many calls
 * will be for well-aligned data, and since Word-zeroing at least leaves
 * the zero in the cache, we shade these values (18-20) down to 8
 */

.weakext bzero, _bzero
.weakext blkclr, _blkclr
.weakext memset, __memset
LEAF(_bzero)
XLEAF(_blkclr)
	move	a2, a1			/* move count to 3rd param */
	move	a1, zero		/* setup ch == zero */
	END(_bzero)

	/* FALL THROUGH to memset */
	
/*
 * memset(dst, c, bcount)
 * set block of memory with blanks
 *
 * Calculating MINSET, assuming 10% cache-miss on non-loop code:
 * Overhead =~ 18 instructions => 28 (30) cycles
 * Byte set =~ 12 (24) cycles/word for 08M44 (08V11)
 * Word set =~ 3 (5) cycles/word for 08M44 (08V11)
 * If I-cache-miss nears 0, MINSET ==> 4 bytes; otherwise, times are:
 * breakeven (MEM) = 28 / (12 - 3) =~ 3 words
 * breakeven (VME) = 30 / (24 - 5)  =~ 1.5 words
 * Since the overhead is pessimistic (worst-case alignment), and many calls
 * will be for well-aligned data, and since Word-set at least leaves
 * the set in the cache, we shade these values (6-12) down to 8 bytes
 */
#define ch	a1
#define bc	a2

LEAF(memset)
XLEAF(__memset)

	move	v0,a0			# return first argument
	slti	t0,bc,MINSET		# threshold of blkset
	bnez	t0,byteset		# use block bzero
	andi	v1,a0,NBPW-1		# number of bytes to be aligned

	beqz	ch,2f			# memset(s,0,n) => skip making pattern
	andi	ch, 0xff		# mask out sign extension
	sll	t0,ch,8
	or	ch,t0			# construct a half word pattern
#if (SZREG == 8)
	dsll	t0,ch,16		# avoid sign extension
	or	ch,t0
	dsll	t0,ch,32		# construct a whole double word pattern
#else
	sll	t0,ch,16		# construct a whole word pattern
#endif
	or	ch,t0

2:	beqz	v1,blkset		# already aligned
	addi	v1,-NBPW		# no. of bytes til aligned
	SWS	ch,0(a0)		# bzero/memset to aligned word
	LADDU	bc,v1			# adjust count
	SUBU	a0,v1			# adjust dst addr

/*
 * try 8 word aligned block
 */
blkset:
	and	ta0,bc,~(8*NBPW-1)	# number of 8*NBPW blocks
	sltiu	t0,bc,MINFPZERO		# threshold of _bzero_fp
	bnez	t0,1f			# check for using _bzero_fp(TFP)

	SETUP_GPX(t8)			# expensive checking of _blk_fp flag
	SUBU	sp,SZFRAME
	SETUP_GPX64(GPOFF,t8)
	SAVE_GP(GPOFF)
	lw	a3,_blk_fp		# check _blk_fp flag
	bnez	a3,do_fpbzero		# do _bzero_fp

nmlbzero:
	RESTORE_GP64
	ADDIU	sp,SZFRAME

	and	ta0,bc,~(8*NBPW-1)	# number of 8*NBPW blocks
#if (_MIPS_ISA == _MIPS_ISA_MIPS4)
	/* If we get here we KNOW that we're MIPS4 and NOT an R8000
	 * so it's safe to use the prefetch instruction (i.e. we're a T5).
	 * NOTES:
	 *	(1) Loop only moves 4 at a time because that is enough
	 *	    to have acceptable performance
	 *	(2) Performs multiple prefetchs on the same secondary cache
	 *	    line since on non-cacheline aligned bzeros this gives
	 *	    the best performance (also brings into primary caches).
	 */
	andi	ta1,bc,4*NBPW		# 4 more NBPW remaining? BDSLOT
	beqz	ta0,4f
	ADDU	ta0,a0

	.set	noreorder
	.set	nocheckprefetch
8:	SW	ch,0*NBPW(a0)		# do 4*NBPW loop
	SW	ch,1*NBPW(a0)
	SW	ch,2*NBPW(a0)
	SW	ch,3*NBPW(a0)
	ADDIU	a0,4*NBPW
	bne     ta0,a0,8b
	pref	7,5*128(a0)		# BDSLOT (prefetch)
	b	wd_4
	nop
	.set	checkprefetch
	.set	reorder
#endif /* MIPS4 */

	/* This code may be executed by an R8000 and needs to remain
	 * "prefetch-free" since that instruction causes problems
	 * on R8000s.
	 */

1:	andi	ta1,bc,4*NBPW		# 4 more NBPW remaining? BDSLOT
	beqz	ta0,4f
	ADDU	ta0,a0
8:	SW	ch,0*NBPW(a0)		# do 8*NBPW loop
	SW	ch,1*NBPW(a0)
	SW	ch,2*NBPW(a0)
	SW	ch,3*NBPW(a0)
	SW	ch,4*NBPW(a0)
	SW	ch,5*NBPW(a0)
	SW	ch,6*NBPW(a0)
	SW	ch,7*NBPW(a0)
	ADDIU	a0,8*NBPW
	bne     ta0,a0,8b

wd_4:
4:	andi	ta2,bc,2*NBPW		# 2 more NBPW remaining? BDSLOT
	beqz	ta1,2f
	SW	ch,0*NBPW(a0)		# do 4*NBPW
	SW	ch,1*NBPW(a0)
	SW	ch,2*NBPW(a0)
	SW	ch,3*NBPW(a0)
	ADDIU	a0,4*NBPW

wd_2:
2:	andi	ta3,bc,1*NBPW		# 1 more NBPW remaining? BDSLOT
	beqz	ta2,1f
	SW	ch,0*NBPW(a0)		# do 2*NBPW
	SW	ch,1*NBPW(a0)
	ADDIU	a0,2*NBPW

1:	andi	ta0,bc,NBPW-1		# less than 1 NBPW remaining? BDSLOT
	beqz	ta3,3f
	SW	ch,0*NBPW(a0)		# do 1*NBPW
	ADDIU	a0,1*NBPW

3:	ADDU	a0,ta0
	beqz	ta0,9f
	SWB	ch,-1(a0)		# do partial word
9:	j	ra


/*
 * do bye-by-bye bzero(memset) if count < MINZERO(MINSET).
 * On IP22, we will paid I-cache missing (-6%) for small data block to
 * favor others (+4%).
 */
byteset:
	.set	noreorder
	blez	bc,ret			# nothing to do!
	ADDU	a3,bc,a0		# dst endpoint (BDSLOT)
1:	ADDIU	a0,1
	bne	a0,a3,1b
	sb	ch,-1(a0)		# small loop for count < MINSET
	.set	reorder
ret:	j	ra

/* bzero using fp registers, currently only available for TFP
 */
do_fpbzero:
	bgtz	a3,call_bzero_fp

init_blk_fp:				# _blk_fp hasn't been set up
	SW	ra,RAOFF(sp)
	SW	v0,V0OFF(sp)
	SW	a0,A0OFF(sp)
	SW	a1,A1OFF(sp)
	SW	a2,A2OFF(sp)

	jal	_blk_init		# call _blk_init

	LW	ra,RAOFF(sp)
	LW	v0,V0OFF(sp)
	LW	a0,A0OFF(sp)
	LW	a1,A1OFF(sp)
	LW	a2,A2OFF(sp)

	lw	a3,_blk_fp		# check _blk_fp flag
	beqz	a3,nmlbzero		# go back to do normal bcopy
		
call_bzero_fp:
#if (SZREG == 4)
	/* make sure double word(8-byte) alignment for NBPW(SZREG) == 4.
	 */
	and	v1,a0,(2*NBPW-1)	# check dw (8 bytes) alignment
	beqz	v1,9f

	sw	ch,0(a0)		# align to dw
	ADDIU	a0,NBPW
	LSUBU	bc,NBPW
9:
#endif /* SZREG == 4 */
	/* since MINFPZERO >> 32 bytes, bc is still > 32 bytes at here.
	 */
	andi	a3,bc,(32-1)		# residual bytes after _bzero_fp
	LSUBU	bc,a3

	SW      ra,RAOFF(sp)		# save ra in the stack

	jal	_memset_fp		# call _memset_fp
	
	LW      ra,RAOFF(sp)

	RESTORE_GP64
	ADDIU	sp,SZFRAME

	move	bc,a3
#if (SZREG == 8)
	andi	ta2,bc,2*NBPW		# BDSLOT
	bnez	bc,wd_2			# finish rest of bytes
#else
	andi	ta1,bc,4*NBPW		# BDSLOT
	bnez	bc,wd_4			# finish rest of bytes
#endif /* SZREG == 8 */
	j	ra			# return to caller

	END(memset)
