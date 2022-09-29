/**************************************************************************
 *									  *
 *		 Copyright (C) 1989-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/* Copyright(C) 1986, MIPS Computer Systems */


#include "ml/ml.h"

#define BZERO64

/* It turns out better to think of LWS/LWB and SWS/SWB as
 *	smaller-vs-bigger address rather than left-vs-right.
 *	Such a representation makes the code endian-independent. 
 */

#ifdef _MIPSEB
#    define LWS lwl
#    define LWB lwr
#    define LDS ldl
#    define LDB	ldr
#    define SWS swl
#    define SWB swr
#    define SDS	sdl
#    define SDB	sdr
#else
#    define LWS lwr
#    define LWB lwl
#    define LDS	ldr
#    define LDB	ldl
#    define SWS swr
#    define SWB swl
#    define SDS	sdr
#    define SDB	sdl
#endif /* _MIPSEB */

/*
 * Normal version of copyin.
 *
 * int copyin(user_src, kernel_dst, bcount)
 *	long user_src, kernel_dst;
 *	long bcount;
 */
LOCALSZ=	1			# Save ra
COPYIOFRM=	FRAMESZ((NARGSAVE+LOCALSZ)*SZREG)
RAOFF=		COPYIOFRM-(1*SZREG)
NESTED(copyin, COPYIOFRM, zero)
	PTR_SUBU sp,COPYIOFRM
	REG_S	ra,RAOFF(sp)

#if CELL 
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	lbu  	t1,K_TYPE(t0)
	bne	t1,KT_XTHREAD,1f    	# If not xthread goto normal copy
	PTR_L	t2,X_INFO(t0)		# xt_info field set indicates message
	beq	t2,0,1f    	 	# If not message thread goto normal
	jal	do_ucopy_copyin		# Call copyin callback func
	j 	2f
1:
#endif

	/*
	 * Test parameters for sanity: protect against copies to kernel
	 * addresses.  We could compare against K0BASE for the high end,
	 * but MAXHIUSRATTACH is more conservative.
	 */
	bltz	a2,cerror		# if (bcount < 0) goto cerror;
	bltz	a0,cerror		# if (user_src < 0) goto cerror;
	PTR_ADDU v0,a0,a2		# v0 = user_src + bcount
	LI	t0,MAXHIUSRATTACH	# no sign extension in 64 bit mode
	bgtu	v0,t0,cerror		# if user_src + bcount > MAXHIUSRATTACH
					#	goto cerror;

	.set	noreorder
	/* store dest (t0) is a struct dependancy and cannot speculate */
	AUTO_CACHE_BARRIERS_DISABLE
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_COPYIO		# LDSLOT
	jal	bcopy
	sh	v0,K_NOFAULT(t0)	# BDSLOT

	PTR_L	t0,VPDA_CURKTHREAD(zero)
	move	v0,zero			# LDSLOT
	sh	zero,K_NOFAULT(t0)
	AUTO_CACHE_BARRIERS_ENABLE

2:
	.set	reorder
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,COPYIOFRM
	j	ra
	END(copyin)

/*
 * Normal version of copyout.
 *
 * int copyout(kernel_src, user_dst, bcount);
 * long kernel_src, user_dst, bcount;
 */
#if defined(R10000_SPECULATION_WAR) && (! defined(MH_R10000_SPECULATION_WAR))
#define COPYOUT_BCOPY	nowar_bcopy	/* safe, as dst is mapped */
#else
#define COPYOUT_BCOPY	bcopy
#endif
NESTED(copyout, COPYIOFRM, zero)
	PTR_SUBU sp,COPYIOFRM
	REG_S	ra,RAOFF(sp)

#if CELL
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	lbu  	t1,K_TYPE(t0)
	bne	t1,KT_XTHREAD,1f    	# If not xthread goto normal copy
	PTR_L	t2,X_INFO(t0)		# xt_info field set indicates message
	beq	t2,0,1f    	 	# If not message thread goto normal
	jal	do_ucopy_copyout	# Call copyout callback func
	j 	2f
1:
#endif

	/*
	 * Test parameters for sanity: protect against copies to kernel
	 * addresses.  We could compare against K0BASE for the high end,
	 * but MAXHIUSRATTACH is more conservative.
	 */
	bltz	a2,cerror		# if (bcount < 0) goto cerror;
	bltz	a1,cerror		# if (user_dst < 0) goto cerror;
	PTR_ADDU v0,a1,a2		# v0 = user_dst + bcount
	LI	t0,MAXHIUSRATTACH	# no sign extension in 64 bit mode
	bgtu	v0,t0,cerror		# if user_dst + bcount > MAXHIUSRATTACH
					#	goto cerror;

	.set	noreorder
	/* store dest (t0) is a struct dependancy and cannot speculate */
	AUTO_CACHE_BARRIERS_DISABLE
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_COPYIO		# LDSLOT
	jal	COPYOUT_BCOPY
	sh	v0,K_NOFAULT(t0)	# BDSLOT

	PTR_L	t0,VPDA_CURKTHREAD(zero)
	move	v0,zero			# LDSLOT
	sh	zero,K_NOFAULT(t0)
	AUTO_CACHE_BARRIERS_ENABLE

2:
	.set	reorder
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,COPYIOFRM
	j	ra
	END(copyout)

/*
 * Byte swapping version of copyin.
 *
 * int swcopyin(user_src, kernel_dst, bcount)
 *	long user_src, kerner_dst;
 *	long bcount;
 */
NESTED(swcopyin, COPYIOFRM, zero)
	PTR_SUBU sp,COPYIOFRM
	REG_S	ra,RAOFF(sp)

	/*
	 * Test parameters for sanity: protect against copies to kernel
	 * addresses.  We could compare against K0BASE for the high end,
	 * but MAXHIUSRATTACH is more conservative.
	 */
	bltz	a2,cerror		# if (bcount < 0) goto cerror;
	bltz	a0,cerror		# if (user_src < 0) goto cerror;
	PTR_ADDU v0,a0,a2		# v0 = user_src + bcount
	LI	t0,MAXHIUSRATTACH	# no sign extension in 64 bit mode
	bgtu	v0,t0,cerror		# if user_src + bcount > MAXHIUSRATTACH
					#	goto cerror;

	.set	noreorder
	/* store dest (t0) is a struct dependancy and cannot speculate */
	AUTO_CACHE_BARRIERS_DISABLE
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_COPYIO		# LDSLOT
	jal	swbcopy
	sh	v0,K_NOFAULT(t0)	# BDSLOT

	PTR_L	t0,VPDA_CURKTHREAD(zero)
	move	v0,zero			# LDSLOT
	sh	zero,K_NOFAULT(t0)
	AUTO_CACHE_BARRIERS_ENABLE

	.set	reorder
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,COPYIOFRM
	j	ra
	END(swcopyin)

/*
 * Byte swapping version of copyout().
 *
 * int swcopyout(kernel_src, user_dst, bcount);
 * long kernel_src, user_dst, bcount;
 */
NESTED(swcopyout, COPYIOFRM, zero)
	PTR_SUBU sp,COPYIOFRM
	REG_S	ra,RAOFF(sp)

	/*
	 * Test parameters for sanity: protect against copies to kernel
	 * addresses.  We could compare against K0BASE for the high end,
	 * but MAXHIUSRATTACH is more conservative.
	 */
	bltz	a2,cerror		# if (bcount < 0) goto cerror;
	bltz	a1,cerror		# if (user_dst < 0) goto cerror;
	PTR_ADDU v0,a1,a2		# v0 = user_dst + bcount
	LI	t0,MAXHIUSRATTACH	# no sign extension in 64 bit mode
	bgtu	v0,t0,cerror		# if user_dst + bcount > MAXHIUSRATTACH
					#	goto cerror;

	.set	noreorder
	/* store dest (t0) is a struct dependancy and cannot speculate */
	AUTO_CACHE_BARRIERS_DISABLE
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_COPYIO		# LDSLOT
	jal	swbcopy
	sh	v0,K_NOFAULT(t0)	# BDSLOT

	PTR_L	t0,VPDA_CURKTHREAD(zero)
	move	v0,zero			# LDSLOT
	sh	zero,K_NOFAULT(t0)
	AUTO_CACHE_BARRIERS_ENABLE

	.set	reorder
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,COPYIOFRM
	j	ra
	END(swcopyout)

/*
 * Zero user memory.
 *
 * int uzero(user_dst, bcount);
 * long user_dst, bcount;
 */
NESTED(uzero, COPYIOFRM, zero)
	PTR_SUBU sp,COPYIOFRM
	REG_S	ra,RAOFF(sp)

#if CELL
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	lbu  	t1,K_TYPE(t0)
	bne	t1,KT_XTHREAD,1f    	# If not xthread goto normal copy
	PTR_L	t2,X_INFO(t0)		# xt_info field set indicates message
	beq	t2,0,1f    	 	# If not message thread goto normal
	jal	do_ucopy_zero		# Call zero callback func
	j 	2f
1:
#endif

	/*
	 * Test parameters for sanity: protect against zeros of kernel
	 * addresses.  We could compare against K0BASE for the high end,
	 * but MAXHIUSRATTACH is more conservative.
	 */
	bltz	a1,cerror		# if (bcount < 0) goto cerror;
	bltz	a0,cerror		# if (user_dst < 0) goto cerror;
	PTR_ADDU v0,a0,a1		# v0 = user_dst + bcount
	LI	t0,MAXHIUSRATTACH	# no sign extension in 64 bit mode
	bgtu	v0,t0,cerror		# if user_dst + bcount > MAXHIUSRATTACH
					#	goto cerror;

	.set	noreorder
	/* store dest (t0) is a struct dependancy and cannot speculate */
	AUTO_CACHE_BARRIERS_DISABLE
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_COPYIO		# LDSLOT
	jal	bzero
	sh	v0,K_NOFAULT(t0)	# BDSLOT

	PTR_L	t0,VPDA_CURKTHREAD(zero)
	move	v0,zero			# LDSLOT
	sh	zero,K_NOFAULT(t0)
	AUTO_CACHE_BARRIERS_ENABLE

2:
	.set	reorder
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,COPYIOFRM
	j	ra
	END(uzero)

NESTED(cerror, COPYIOFRM, zero)
	li	v0,-1
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,COPYIOFRM
	j	ra
	END(cerror)

#ifdef _MIPSEL
/* swapl - swap bytes within a buffer of words
 * buffer must be word aligned, or an address error will be generated
 * first arg is address, second is number of words (not bytes).
 */
LEAF(swapl)			
	.set	noreorder
 	beq	zero,a1,2f # be paranoid
	nop
1:	lw	t0,0(a0) 	# t0 = abcd
	subu	a1,1
	sll	v0,t0,24	# v0 = d000
	srl	v1,t0,24	# v1 = 000a
	or	v0,v0,v1	# v0 = d00a
	and	v1,t0,0xff00	# v1 = 00c0
	sll	v1,v1,8		# v1 = 0c00
	or	v0,v0,v1	# v0 = dc0a
	srl	v1,t0,8		# v1 = 0abc
	and	v1,v1,0xff00	# v1 = 00b0
	or	v0,v0,v1	# v0 = dcba
	sw	v0,0(a0)
 	bne	zero,a1,1b
	addiu	a0,4
2:
	.set	reorder
	j	ra
	END(swapl)

/* swaps - swap bytes within a buffer of halfwords
 * buffer must be halfword aligned, or an address error will be generated
 * first arg is address, second is number of halfwords (not bytes).
 */
LEAF(swaps)			
	.set	noreorder
 	beq	zero,a1,2f # be paranoid
	nop
1:	lhu	t0,0(a0) 	# t0 = 00ab
	subu	a1,1
	srl	v0,t0,8		# v0 = 000a
	sll	v1,t0,8		# v1 = 0ab0
	or	v0,v1		# v0 = 0aba
	sh	v0,0(a0)
 	bne	zero,a1,1b
	addiu	a0,2
2:
	.set	reorder
	j	ra
	END(swaps)

#endif /* _MIPSEL */

/* bzero(dst, bcount)
 * Zero a block of memory.
 *	This code assumes most blocks are aligned and larger than 12 bytes.
 *	This code is used so often that it is generally in the cache.
 *
 *	We let the assembler reorder instructions, since the choices it makes
 *	in this particular code favor the common, aligned big-block case.
 *	Use many registers to give the assembler plenty of choices to move
 *	things up.
 */
#define	dst	a0
#define	count	a1

LEAF(bzero)
XLEAF(blkclr)
#if IP20 || IP22
	PTR_S	a0,kv_initial_to
	PTR_S	a1,initial_count
#endif	/* IP20 || IP22 */
#if IP32
	or	v0,a0,a1
EXPORT(bzero_cdx_entry)
	.set noreorder				# if aligned well use
	li	v0,1				# the R5000 cdx blkfill
	bnez	v0,1f
	nop
	j	__cdx_blkfill
	move	a2,zero
	.set reorder
1:
#endif /* IP32 */
#if IP26 || IP28 || (IP30 && (HEART_COHERENCY_WAR || HEART_INVALIDATE_WAR))
#ifdef IP26
	andi	v0,a0,TCC_LINESIZE-1		# if aligned well use
	andi	v1,a1,(8*TCC_LINESIZE)-1	# prefetching zero
#else
	andi	v0,a0,CACHE_SLINE_SIZE-1	# if aligned well use
	andi	v1,a1,(4*CACHE_SLINE_SIZE)-1	# blocked/unrolled zero
#endif
	or	v0,v0,v1
	.set	noreorder			# assume small copy for R10K
	bnezl	v0,bzero_nopage
	nop					# BDSLOT
	.set	reorder
	j	_pagezero
EXPORT(bzero_nopage)
#endif
#ifdef BZERO64
	.set	noreorder
	slti	v0,count,8
	bne	v0,zero,bytezero	# long enough to make the code work
	PTR_SUBU v1,zero,dst

/* The following code is a little tricky.  We will zero between one
 * and eight bytes, depending upon the dst, in order to get aligned
 * on a 64-bit boundary.  If the address is already aligned, we
 * end up zeroing the first word once here, not updating dst or count
 * (acutally add/sub zero) then zeroing again in one of the loops below.
 */
	CACHE_BARRIER			# barrier for 0(dst)
	SDS	zero,0(dst)
	and	v0,v1,7			# number of bytes til aligned
	subu	count,v0
	PTR_ADDU dst,v0

/* When we get here, we are aligned on 64-bit boundary */

blkzero:
#if TFP
	and	a3,count,~(16-1)
	beq	a3,zero,8f
	dmtc1	zero,$f4		# BDSLOT, $f4 = 0

	daddu	t1,dst,a3		# t1 = last aligned address
	daddiu	t2,dst,16		# t2 = dst + 16
	daddiu	t3,dst,32		# t3 = dst + 32
	daddiu	t1,16			# t1 = end + 16

	.align	4
32:
/*
 * WARNING: Code to recover from multiple bit errors in memory ASSUMES
 * it knows which registers are used here.  Check ml/error.c before
 * modifying this code.
 */
#if ECC_RECOVER
EXPORT(bzero_stores)
#endif /* ECC_RECOVER */
	sdc1	$f4,-16(t2)
	sdc1	$f4,-8(t2)
	beq	t3,t1,8f
	daddiu	t2,t3,16		# BDSLOT

	sdc1	$f4,-16(t3)
	sdc1	$f4,-8(t3)
	bne	t2,t1,32b
	daddiu	t3,t2,16		# BDSLOT

8:	dadd	dst, a3			# update destination address
	and	v1, count, 8
	beq	v1, zero, 7f
	and	v0, count, 7
	sdc1	$f4, (dst)
	daddi	dst, 8

7:	PTR_ADDU dst,v0
	beq	v0,zero,zdone
	nop
	j	ra
	SDB	zero,-1(dst)

#else /* !TFP */
	and	a3,count,~(32-1)
	beq	a3,zero,16f
	PTR_ADDU a3,a3,dst

32:	PTR_ADDIU dst,32
#if ECC_RECOVER
EXPORT(bzero_stores)
#endif /* ECC_RECOVER */
	CACHE_BARRIER			# barrier for X(dst)
	sd	zero,-32(dst)
#if R10000				/* hit d$ banks right */
	sd	zero,-16(dst)
	sd	zero,-24(dst)
#else
	sd	zero,-24(dst)
	sd	zero,-16(dst)
#endif
#if IP25 || IP27 || IP30		/* R10000 machines with prefetch */
EXPORT(bzero_pref)
	pref	7, 5*128(dst)		# prefech 5 cachelines from now
#endif	/* IP25 || IP27 and not IP28/IP30 */
	bne	dst,a3,32b
	sd	zero,-8(dst)		# BDSLOT
/* We know we have fewer than 32 bytes remaining, so we do limited
 *	adjustments of the count.  This code has overhead that is always
 *	less than the original MIPS code, and is often much less.
 * The overhead from here down is <10+n instructions, where n is the number
 *	of bits in the count.  The simple byte-loop requires about 2.75*c
 *	instructions of overhead, where c is the count.  (You have to count
 *	3 of the sb instructions per word as overhead).  Ignoring I-cache
 *	misses, the break even point is around 5 bytes.
 */
16:	and	v0,count,16
	beq	v0,zero,8f
	and	v1,count,8

	CACHE_BARRIER			# barrier for 0(dst)
	sd	zero,0(dst)
	sd	zero,8(dst)
	PTR_ADDIU dst,16

8:	beq	v1,zero,7f
	and	v0,count,7		# BDSLOT
	CACHE_BARRIER			# barrier for 0(dst)
	sd	zero,0(dst)
	PTR_ADDIU dst,8

7:	PTR_ADDU dst,v0
	beq	v0,zero,zdone
	nop
	CACHE_BARRIER			# barrier for -1(dst)
	j	ra
	SDB	zero,-1(dst)
#endif
	.set	reorder
#else
	PTR_SUBU v1,zero,dst

	blt	count,7,bytezero	# long enough to make the code work

	and	v1,NBPW-1		# number of bytes til aligned
	beq	v1,zero,blkzero		# already aligned
	SWS	zero,0(dst)
	subu	count,v1
	PTR_ADDU dst,v1

/* zero a 32 byte, aligned block.
 *
 * Extra cycles help some machines, presumably because it keeps us from
 *	overrunning the write buffer.
 */
blkzero:
	and	a3,count,~(32-1)
	beq	a3,zero,16f
	PTR_ADDU a3,a3,dst
32:	sw	zero,0(dst)
	sw	zero,4(dst)
	sw	zero,8(dst)
	sw	zero,12(dst)
	sw	zero,16(dst)
	sw	zero,20(dst)
	sw	zero,24(dst)
	sw	zero,28(dst)
	PTR_ADDIU dst,32		#(as moves this way up)
	bne	dst,a3,32b

/* We know we have fewer than 32 bytes remaining, so we do limited
 *	adjustments of the count.  This code has overhead that is always
 *	less than the original MIPS code, and is often much less.
 * The overhead from here down is <10+n instructions, where n is the number
 *	of bits in the count.  The simple byte-loop requires about 2.75*c
 *	instructions of overhead, where c is the count.  (You have to count
 *	3 of the sb instructions per word as overhead).  Ignoring I-cache
 *	misses, the break even point is around 5 bytes.
 */
16:	and	v0,count,16
	beq	v0,zero,8f
	sw	zero,0(dst)
	sw	zero,4(dst)
	sw	zero,8(dst)
	sw	zero,12(dst)
	PTR_ADDIU dst,16

8:	and	v1,count,8
	beq	v1,zero,4f
	sw	zero,0(dst)
	sw	zero,4(dst)
	PTR_ADDIU dst,8

4:	and	v0,count,4
	beq	v0,zero,3f
	sw	zero,0(dst)
	PTR_ADDIU dst,4

3:	and	v1,count,3
	addu	dst,v1
	beq	v1,zero,zdone
	SWB	zero,-1(dst)
	j	ra
#endif	/* !BZERO64 */

bytezero:
	PTR_ADDU a3,dst,count
	ble	count,zero,zdone
1:	sb	zero,0(dst)
	PTR_ADDIU dst,1
	bne	dst,a3,1b
zdone:	j	ra

XLEAF(bzero_end)
	END(bzero)

LEAF(bzerror)
	j	ra
END(bzerror)

#undef	dst
#undef	count


/* bcmp(src, dst, count)
 *
 * Most comparisions are short, and most are aligned.  The answer is found
 * in the first few bytes or not until the end of the strings.
 *
 * There's a bug in the R12KS CPU that will sometimes cause a LWR or LWL
 * instruction to corrupt the portion of the destination register that it's
 * not supposed to touch.  In general LWR and LWL instructions are always
 * paired so this isn't a problem.  bcmp() was being fancy with the initial
 * and trailing compares of unaligned comparisons when the two source strings
 * had matching misalignments and only doing single LWL's for the initial and
 * single LWR's for the trailing comparisons.  Since we know that the bcmp()
 * is for at least 11 bytes at the leading comparison point and at least 7
 * bytes at the trailing comparison point, it's easy to just put the paired
 * instruction in with very little performance impact.
 */
#define	src	a0
#define	dst	a1
#define	count	a2

LEAF(bcmp)
	xor	v0,src,dst
	blt	count,11,bytecmp	# too short, just byte cmp

	and	v0,NBPW-1
	PTR_SUBU t8,zero,src		# number of bytes til aligned
	bne	v0,zero,unalgncmp	# src and dst not alignable

/* since it is possible, word-align src and dst
 */
	and	t8,NBPW-1
	beq	t8,zero,wordcmp		# already aligned
	subu	count,t8
	LWS	t0,0(src)		# cmp unaligned portion
	LWB	t0,3(src)		# WAR: the R12KS needs LWL/LWR paired
	LWS	t1,0(dst)
	LWB	t1,3(dst)		# WAR: the R12KS needs LWL/LWR paired
	PTR_ADDU src,t8
	PTR_ADDU dst,t8
	bne	t0,t1,cmpne


/* do 4 words at a time
 *  One hassle here is avoiding unneeded fetches, which would cause
 *  unneed cache misses.
 */
wordcmp:
	and	v0,count,~(16-1)
	beq	v0,zero,8f
16:	lw	t0,0(src)
	lw	t1,0(dst)
	PTR_ADDIU src,16
	PTR_ADDIU dst,16
	bne	t0,t1,cmpne

	lw	t0,4-16(src)
	lw	t1,4-16(dst)
	subu	v0,16
	bne	t0,t1,cmpne

	lw	t0,8-16(src)		# nothing to fill the delay slots
	lw	t1,8-16(dst)
	bne	t0,t1,cmpne

	lw	t0,12-16(src)
	lw	t1,12-16(dst)
	bne	t0,t1,cmpne
	bne	v0,zero,16b


/* Here we know we have < 16 bytes to finish.
 *  Use many registers to let the assembler fill the delay slots.
 */
8:	and	t9,count,8
	and	t8,count,4
	beq	t9,zero,4f

	lw	t0,0(src)
	lw	t1,0(dst)
	PTR_ADDIU src,8
	PTR_ADDIU dst,8
	bne	t0,t1,cmpne

	lw	t0,4-8(src)
	lw	t1,4-8(dst)
	bne	t0,t1,cmpne

4:	and	t9,count,3
	beq	t8,zero,3f

	lw	t0,0(src)
	lw	t1,0(dst)
	PTR_ADDIU src,4
	PTR_ADDIU dst,4
	bne	t0,t1,cmpne

/* We have 0 to 3 bytes remaining to compare, starting at a word boundary.
 *  We know the original length was >7, so we could go ahead and compare
 *  partial words even if there are only 0 bytes remaining, saving the
 *  loop overhead.  Since most comparisions are of even numbers of words,
 *  we do not cheat that way.
 */
3:	PTR_ADDU src,t9
	beq	t9,zero,cmpeq
	PTR_ADDU dst,t9
	LWS	t0,-4(src)		# WAR: the R12KS needs LWL/LWR paired
	LWB	t0,-1(src)
	LWS	t1,-4(dst)		# WAR: the R12KS needs LWL/LWR paired
	LWB	t1,-1(dst)
	sne	v0,t0,t1
	j	ra

cmpne:	li	v0,1
	j	ra

/*
 * deal with simultaneously unalignable cmp by aligning one src
 *  Assume this is rare, and do not unroll it.
 */
unalgncmp:
	PTR_SUBU a3,zero,dst		# calc byte cnt to get dst aligned
	and	a3,NBPW-1
	subu	count,a3
	beq	a3,zero,partaligncmp	# already aligned
	PTR_ADDU a3,src			# src endpoint
1:	lbu	v0,0(src)
	lbu	v1,0(dst)
	PTR_ADDIU src,1
	PTR_ADDIU dst,1
	bne	v0,v1,cmpne
	bne	src,a3,1b

/*
 * src unaligned, dst aligned loop
 *  Assume this is rare, and do not unroll it.
 */
partaligncmp:
	and	a3,count,~(NBPW-1)
	subu	count,a3
	beq	a3,zero,bytecmp
	PTR_ADDU a3,src
4:
	LWS	v0,0(src)
	LWB	v0,3(src)
	lw	v1,0(dst)
	PTR_ADDIU src,NBPW
	PTR_ADDIU dst,NBPW
	bne	v0,v1,cmpne
	bne	src,a3,4b

/*
 * brute force byte cmp loop
 */
bytecmp:
	PTR_ADDU a3,count,src	# src endpoint; BDSLOT
	ble	count,zero,cmpeq
1:	lbu	v0,0(src)
	lbu	v1,0(dst)
	PTR_ADDIU src,1
	PTR_ADDIU dst,1
	bne	v0,v1,cmpne
	bne	src,a3,1b

cmpeq:	move	v0,zero
	j	ra


	END(bcmp)


/*
 * addupc(pc, &(struct prof), ticks, use_32bit)
 * return value
 *  -1  addupc() failed because pc was outside the offset range in u_prof
 *   0  either addupc() succeeded or it failed due to invalid buffer address
 */
LEAF(addupc)
	PTR_L	v1,PR_OFF(a1)		# base of profile region
	PTR_SUBU a0,v1			# corrected pc
	bltz	a0,1f			# below of profile region
	lw	v0,PR_SCALE(a1)		# fixed point scale factor
	bne	v0,2,2f			# if scale == 2, only use 1st bucket
	li	v0,0
	b	3f
2:	multu	v0,a0
	mflo	v0			# shift 64 bit result right 16
	srl	v0,16
	mfhi	v1
	sll	v1,16
	or	v0,v1
3:
	lw	v1,PR_SIZE(a1)
	PTR_L	a0,PR_BASE(a1)		# base of profile buckets
	bne	a3,zero,4f		# 32-bit buckets?

	/* 16-bit buckets */
	and	v0,~1
	bgeu	v0,v1,1f		# above profile region
	PTR_ADDU v0,a0
	bltz	v0,adderr		# outside kuseg

	.set	noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v1,NF_ADDUPC		# LDSLOT
	CACHE_BARRIER_AT(0,v0)		# barrier for v0 (t0 is dependent)
	sh	v1,K_NOFAULT(t0)

	andi	t1,v0,2			# short- or int-aligned?
	ori	v0,v0,2
	bne	t1,zero,7f
	xori	v0,2			# BDSLOT -- align to int
	sll	a2,16			# adjust increment value
7:
	ll	v1,0(v0)		# add ticks to bucket
	addu	v1,a2
	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	sc	v1,0(v0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	v1,zero,7b
#else
	beq	v1,zero,7b
#endif
	nop
	b	5f
	.set	reorder

4:	/* 32-bit buckets */
	and	v0,~3			# mask off lower bits
	bgeu	v0,v1,1f		# above profile region
	PTR_ADDU v0,a0
	bltz	v0,adderr		# outside kuseg

	.set	noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v1,NF_ADDUPC		# LDSLOT
	CACHE_BARRIER_AT(0,v0)		# barrier for v0 (t0 is dependent)
	sh	v1,K_NOFAULT(t0)
6:
	ll	v1,0(v0)		# add ticks to bucket
	addu	v1,a2
	# Try to set the new one
	AUTO_CACHE_BARRIERS_DISABLE
	sc	v1,0(v0)	
	AUTO_CACHE_BARRIERS_ENABLE
#ifdef R10K_LLSC_WAR
	beql	v1,zero,6b
#else
	beq	v1,zero,6b
#endif
	nop
	.set	reorder
5:
	sh	zero,K_NOFAULT(t0)
	li	v0,0
	j	ra

1:	li	v0,-1
	j	ra
	END(addupc)

LEAF(adderr)
	li	v0,0
	sw	zero,PR_SCALE(a1)
	j	ra
	END(adderr)

LEAF(fubyte)
XLEAF(fuibyte)
#if (_MIPS_SZLONG == 32)
XLEAF(fulong)
#endif
	.set	noreorder
	bltz	a0,uerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_FSUMEM		# LDSLOT
	AUTO_CACHE_BARRIERS_DISABLE	# t0 is dependent
	sh	v0,K_NOFAULT(t0)
	lbu	v0,0(a0)
	j	ra			# LDSLOT
	sh	zero,K_NOFAULT(t0)	# BDSLOT
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	END(fubyte)

/*
 *	upath(from, to, maxbufsize)
 *      Read in a pathname from user space.
 *      RETURNS:
 *              -1 - if supplied address was not valid
 *              -2 - if pathname length is > maxbufsize - 1
 *              length otherwise (including '\0')
 *	Assume maxbufsize > 0
 */
LEAF(upath)
	.set	noreorder
	bltz	a0,uerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_FSUMEM		# LDSLOT
	AUTO_CACHE_BARRIERS_DISABLE	# t0 is dependent
	sh	v0,K_NOFAULT(t0)
	AUTO_CACHE_BARRIERS_ENABLE
	move	v1,a2
1:
	CACHE_BARRIER			# barrier for incrementing a1
	lbu	v0,0(a0)
	LONG_SUBU a2,1			# LDSLOT
	beq	v0,zero,2f		# return length
	sb	v0,0(a1)		# BDSLOT
	PTR_ADDU a0,1
	bne	a2,zero,1b
	PTR_ADDU a1,1			# BDSLOT
	b	3f
	li	v0,-2			# BDSLOT
2:
	LONG_SUBU v0,v1,a2	
3:
	AUTO_CACHE_BARRIERS_DISABLE	# t0 is dependent
	j	ra
	sh	zero,K_NOFAULT(t0)	# BDSLOT
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	END(upath)

/*
 * we don't worry about flushing the write buffer, because we assume that
 * the s* routines are ONLY called for talking to user address space which
 * we assume is either not mapped or mapped to real live memory
 */
LEAF(subyte)
XLEAF(suibyte)
	.set	noreorder
	bltz	a0,uerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_FSUMEM		# LDSLOT
	CACHE_BARRIER			# barrier for a0 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	sb	a1,0(a0)
	sh	zero,K_NOFAULT(t0)
	.set	reorder
	move	v0,zero
	j	ra
	END(subyte)

LEAF(fuword)
XLEAF(fuiword)
	.set	noreorder
	bltz	a0,uerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_FSUMEM		# LDSLOT
	AUTO_CACHE_BARRIERS_DISABLE	# t0 is dependent
	sh	v0,K_NOFAULT(t0)
	lw	v0,0(a0)
	sh	zero,K_NOFAULT(t0)	# LDSLOT
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	j	ra
	END(fuword)

/*
 * Get unsigned 32 bit vector (a0 = errptr, a1 = srcv, a2 = dstv, a3 = cnt)
 */
LEAF(sfu32v)
	.set	noreorder
	bltz	a1,suerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_SUERROR		# LDSLOT
	AUTO_CACHE_BARRIERS_DISABLE	# t0 is dependent
	sh	v0,K_NOFAULT(t0)
	AUTO_CACHE_BARRIERS_ENABLE
1:
	CACHE_BARRIER			# protect incrementing a2 in loop
	lw	v0,0(a1)
	subu	a3,4
	sw	v0,0(a2)
	addu	a1,4
	bne	zero,a3,1b
	addu	a2,4

	AUTO_CACHE_BARRIERS_DISABLE	# t0 is dependent
	sh	zero,K_NOFAULT(t0)	# LDSLOT
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	li	v0,1
	j	ra
	END(sfu32v)

/*
 * Get signed 32 bit number and return in v0 (on fault return zero)
 */
LEAF(sfu32)
	.set	noreorder
	bltz	a1,suerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_SUERROR		# LDSLOT
	AUTO_CACHE_BARRIERS_DISABLE	# t0 is dependent on PTR_L
	sh	v0,K_NOFAULT(t0)
	lw	v0,0(a1)
	sh	zero,K_NOFAULT(t0)	# LDSLOT
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	j	ra
	END(sfu32)

/*
 * Put unsigned 32 bit number and return TRUE on success
 */
LEAF(spu32)
	.set	noreorder
	bltz	a1,suerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_SUERROR		# LDSLOT
	CACHE_BARRIER			# barrier for a1 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	sw	a2,0(a1)
	sh	zero,K_NOFAULT(t0)
	.set	reorder
	li	v0,1
	j	ra
	END(spu32)

/*
 * On error tramp, set @(a0) to EFAULT and return zero in v0.
 */
LEAF(suerror)
	li	v1,EFAULT		# put EFAULT in arg
	sw	v1,0(a0)
	li	v0,0
	j	ra
	END(suerror)

#if (_MIPS_SZLONG == 64)
LEAF(fulong)
	.set	noreorder
	bltz	a0,uerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_FSUMEM		# LDSLOT
	AUTO_CACHE_BARRIERS_DISABLE	# t0 is dependent on PTR_L
	sh	v0,K_NOFAULT(t0)
	ld	v0,0(a0)
	sh	zero,K_NOFAULT(t0)	# LDSLOT
	AUTO_CACHE_BARRIERS_ENABLE
	.set	reorder
	j	ra
	END(fulong)
#endif

LEAF(suword)
XLEAF(suiword)
	.set	noreorder
	bltz	a0,uerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_FSUMEM		# LDSLOT
	CACHE_BARRIER			# barrier for a0 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	sw	a1,0(a0)
	sh	zero,K_NOFAULT(t0)
	.set	reorder
	move	v0,zero
	j	ra
	END(suword)


LEAF(suhalf)
	.set	noreorder
	bltz	a0,uerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_FSUMEM		# LDSLOT
	CACHE_BARRIER			# barrier for a0 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	sh	a1,0(a0)
	sh	zero,K_NOFAULT(t0)
	.set	reorder
	move	v0,zero
	j	ra
	END(suhalf)

#if (_MIPS_SZLONG == 64)
LEAF(sulong)
	.set	noreorder
	bltz	a0,uerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_FSUMEM		# LDSLOT
	CACHE_BARRIER			# barrier for a0 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	sd	a1,0(a0)
	sh	zero,K_NOFAULT(t0)
	.set	reorder
	move	v0,zero
	j	ra
	END(sulong)
#endif

/*
 * Fetch an instruction word at a KSEG2 address-
 * used for loadable drivers which may have hit the
 * R4K badvaddr chip bug.
 */
LEAF(fkiword)
	.set	noreorder
	li	v0,NF_BADVADDR		
	sw	v0,VPDA_NOFAULT(zero)
	lw	v0,0(a0)
	sw	zero,VPDA_NOFAULT(zero)		# LDSLOT
	.set	reorder
	j	ra
	END(fkiword)

LEAF(uerror)
	li	v0,-1			# error return
	j	ra
	END(uerror)



LEAF(strlen)
	move	v0,a0		# save beginning pointer
1:	lb	v1,0(a0)	# look at byte
	PTR_ADDIU a0,1		# advance current pointer
	bne	v1,zero,1b	# check for null byte
	PTR_SUBU v0,a0,v0	# byte count including null byte
	LONG_SUBU v0,1		# exclude null byte
	j	ra
	END(strlen)

/*
 * The following routines uload_word(), uload_half(), uload_uhalf(),
 * ustore_word() and ustore_half() load and store unaligned items.
 * The "addr" parameter is the address at which the reference is to be
 * made.  For load routines the value is returned indirectly through
 * the "pword" parameter.  For store routines the "value" parameter
 * is stored.  All routines indicate an error by returning a non-zero
 * value.  If no error occurs a zero is returned.
 * ASSUME that test for KUSEG has already occurred
 */

/*
 * int uload_word(caddr_t addr, k_machreg_t *pword)
 */
LEAF(uload_word)
	.set noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_FIXADE		# LDSLOT
	CACHE_BARRIER			# barrier for a1 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	ulw	v1,0(a0)
	sh	zero,K_NOFAULT(t0)
	sreg	v1,0(a1)
	.set reorder			# after sreg to avoid extra BARRIER
	move	v0,zero
	j	ra
	END(uload_word)

/*
 * int uload_half(caddr_t addr, k_machreg_t *pword)
 */
LEAF(uload_half)
	.set	noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_FIXADE		# LDSLOT
	CACHE_BARRIER			# barrier for a1 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	# 3.3 as gets this wrong (no nop); so expand it# ulh	v1,0(a0)
	.set noat
	lb	v1,0(a0)
	lbu	AT,1(a0)
	sll	v1,v1,8
	or	v1,v1,AT
	.set at
	# end expansion of ulh
	sh	zero,K_NOFAULT(t0)
	sreg	v1,0(a1)
	.set	reorder			# after sreg to avoid extra BARRIER
	move	v0,zero
	j	ra
	END(uload_half)

/*
 * int uload_uhalf(caddr_t addr, k_machreg_t *pword)
 */
LEAF(uload_uhalf)
	.set	noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_FIXADE		# LDSLOT
	CACHE_BARRIER			# barrier for a1 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	# 3.3 as gets this wrong (no nop); so expand it# ulhu	v1,0(a0)
	.set noat
	lbu	v1,0(a0)
	lbu	AT,1(a0)
	sll	v1,v1,8
	or	v1,v1,AT
	.set at
	# end expansion of ulhu
	sh	zero,K_NOFAULT(t0)
	sreg	v1,0(a1)
	.set	reorder			# after sreg to avoid extra BARRIER
	move	v0,zero
	j	ra
	END(uload_uhalf)

/*
 * int uload_uword(caddr_t addr, k_machreg_t *pword)
 */
LEAF(uload_uword)
	.set noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_FIXADE		# LDSLOT
	CACHE_BARRIER			# barrier for a1 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	ulwu	v1,0(a0)
	sh	zero,K_NOFAULT(t0)
	CACHE_BARRIER			# guard a1
	sreg	v1,0(a1)
	.set reorder			# after sreg to avoid extra BARRIER
	move	v0,zero
	j	ra
	END(uload_uword)

/*
 * int uload_double(caddr_t addr, k_machreg_t *pword)
 */
LEAF(uload_double)
	.set	noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_FIXADE		# LDSLOT
	CACHE_BARRIER			# barrier for a1 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
	uld	v1,0(a0)
	sh	zero,K_NOFAULT(t0)
	sd	v1,0(a1)
	.set	reorder			# after sd to avoid extra BARRIER
	move	v0,zero
	j	ra
	END(uload_double)

/*
 * ustore_word(caddr_t addr, k_machreg_t value)
 */
LEAF(ustore_double)
	.set noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_FIXADE		# LDSLOT
	CACHE_BARRIER			# barrier for a0 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	/* 'value' is a long long, and _MIPS_SIM_ABI32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	dsll32	a2,0
	or	a1,a2,a3
#endif
	usd	a1,0(a0)
	sh	zero,K_NOFAULT(t0)
	.set reorder
	move	v0,zero
	j	ra
	END(ustore_double)

/*
 * ustore_word(caddr_t addr, k_machreg_t value)
 */
LEAF(ustore_word)
	.set noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_FIXADE		# LDSLOT
	CACHE_BARRIER			# barrier for a0 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	/* 'value' is a long long, and _MIPS_SIM_ABI32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	usw	a3,0(a0)
#else
	usw	a1,0(a0)
#endif
	sh	zero,K_NOFAULT(t0)
	.set reorder
	move	v0,zero
	j	ra
	END(ustore_word)

/*
 * ustore_half(caddr_t addr, k_machreg_t value)
 */
LEAF(ustore_half)
	.set noreorder
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	li	v0,NF_FIXADE		# LDSLOT
	CACHE_BARRIER			# barrier for a0 (t0 is dependent)
	sh	v0,K_NOFAULT(t0)
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	/* 'value' is a long long, and _MIPS_SIM_ABI32 passes
	 * long longs in 2 registers. The register pair starts on
	 * an even register, hence the low order word is in reg a3.
	 */
	ush	a3,0(a0)
#else
	ush	a1,0(a0)
#endif
	sh	zero,K_NOFAULT(t0)
	.set reorder
	move	v0,zero
	j	ra
	END(ustore_half)

LEAF(fixade_error)
	move	v0,gp
	j	ra
	END(fixade_error)



/* void bcopy(from, to, count);
 *	unsigned char *from, *to;
 *	unsigned long count;
 */

#define	MINCOPY	12

/* registers used */
#define	from	a0
#define	to	a1
#define	count	a2

#if !TFP

/* Use backwards copying code if the from and to regions overlap.
 *   Do not worry about zero-length or other silly copies.  They are not
 *   worth the time to optimize.
 */
LEAF(bcopy)
XLEAF(ovbcopy)
#if IP20 || IP22
	PTR_S	a0,kv_initial_from
	PTR_S	a1,kv_initial_to
	PTR_S	a2,initial_count
#endif	/* IP20 || IP22 */
#ifdef IP32
	or	v0,a0,a1			# if aligned well use the
	or	v0,a2				# faster R5000 cdx block copy
EXPORT(bcopy_cdx_entry)
	.set noreorder
	li	v0,1
	bnez	v0,1f
	nop
	j	__cdx_blkcopy
	nop
	.set reorder
1:
#endif	/* IP32 */
#if IP28 || (IP30 && (HEART_COHERENCY_WAR || HEART_INVALIDATE_WAR))
	andi	v1,a1,CACHE_SLINE_SIZE-1	# if aligned well use the
 	andi	v0,a0,CACHE_SLINE_SIZE-1	# faster blocked/unrolled
	andi	t0,a2,(4*CACHE_SLINE_SIZE)-1	# and cache tuned copy
	or	v0,v0,v1
	or	v0,v0,t0
	.set	noreorder			# assume small copy for R10K
	bnezl	v0,bcopy_nopage
	nop					# BDSLOT
	.set	reorder
	j	_pagecopy
EXPORT(bcopy_nopage)
#endif

	PTR_ADDU v0,from,count		# v0 := from + count
	ble	to,from,goforwards	# If to <= from then copy forwards
	blt	to,v0,gobackwards	# backwards if from<to<from+count

/* Forward copy code.  Check for pointer alignment and try to get both
 * pointers aligned on a long boundary.
 */
goforwards:
	blt	count,MINCOPY,fbcopy
/* If possible, align source & destination on 64-bit boundary. */
	and	v0,from,7
	and	v1,to,7
	li	a3,8
	bne	v0,v1,align32		# low bits are different

/* Pointers 64-bit alignable (may be aligned).  Since v0 == v1, we need only
 * check what value v0 has to see how to get aligned.  Also, since we have
 * eliminated tiny copies, we know that the count is large enough to
 * encompass the alignment copies.
 */
	beq	v0,zero,1f		# If v0==0 then aligned
	subu	a3,a3,v1		# a3 = # bytes to get aligned
	LDS	v0,0(from)
	SDS	v0,0(to)		# copy partial word
	PTR_ADDU from,a3
	PTR_ADDU to,a3
	subu	count,a3
1:
/* When we get here, source and destination are 64-bit aligned.  Check if
 * we have at least 64 bytes to move.
 */
	and	a3,count,~(64-1)
	beq	a3,zero,forwards	# go do 32-bit copy
	PTR_ADDU a3,a3,to
64:
	/* Splitting d$ banks is faster on the R10000 */
#if R10000
	ld t0,0(from);	ld t2,16(from)
#if IP25 || IP27 || IP30
	.set	noreorder
EXPORT(bcopy_pref1)
	pref	4,384(from)		# R10000 machines with prefetch */
	.set	reorder
#endif /* IP25 || IP27 || IP30 (no IP28) */		
	ld t1,8(from);	ld t3,24(from)
	ld ta0,32(from);ld ta2,48(from)
	ld ta1,40(from);ld ta3,56(from)
#if ECC_RECOVER
	.set	noreorder
EXPORT(bcopy_stores)
#endif /* ECC_RECOVER */
	sd t0,0(to);	sd t2,16(to)
	sd t1,8(to);	sd t3,24(to)
	sd ta0,32(to);	sd ta2,48(to)
	sd ta1,40(to);	sd ta3,56(to)
#if ECC_RECOVER
	.set	reorder
#endif /* ECC_RECOVER */
#else
	ld t0,0(from);	ld t1,8(from);	ld t2,16(from);	ld t3,24(from)
	ld ta0,32(from);ld ta1,40(from);ld ta2,48(from);ld ta3,56(from)
#if ECC_RECOVER
	.set	noreorder
EXPORT(bcopy_stores)
#endif /* ECC_RECOVER */
	sd t0,0(to);	sd t1,8(to);	sd t2,16(to);	sd t3,24(to)
#if ECC_RECOVER
	.set	reorder
#endif /* ECC_RECOVER */
	sd ta0,32(to);	sd ta1,40(to);	sd ta2,48(to);	sd ta3,56(to)
#endif	/* R10000 */
	PTR_ADDU from,64
	PTR_ADDU to,64

#if !defined(IP25) && !defined(IP27) && !defined(IP30)
	bne	a3,to,64b
#else	/* IP25 || IP27 || IP30 */
	beq	a3,to,pref64end
	
	/* we unroll r10k another time so we have a big loop which brings
	 * in cachelines.  This way we can also perform two different
	 * prefetch operations (one for source and one for destination).
	 */

	ld t0,0(from);   ld t2,16(from)
	.set	noreorder
EXPORT(bcopy_pref2)
	pref	7,384(to)
	.set	reorder
	ld t1,8(from);  ld t3,24(from)
	ld ta0,32(from);  ld ta2,48(from);  ld ta1,40(from);  ld ta3,56(from)
	sd t0,0(to);      sd t2,16(to);     sd t1,8(to);      sd t3,24(to)
	sd ta0,32(to);    sd ta2,48(to);    sd ta1,40(to);    sd ta3,56(to)
	PTR_ADDU from,64
	PTR_ADDU to,64
	bne	a3,to,64b
pref64end:		
#endif /* IP25 || IP27 || IP30 */
	
	and	count,64-1	# still have to copy non-64 multiple bytes
	b	forwards		# complete with 32-bit copy

align32:
	and	v0,from,3
	and	v1,to,3
	li	a3,4
	bne	v0,v1,fmcopy		# low bits are different

/* Pointers are alignable and may be aligned.  Since v0 == v1, we need only
 * check what value v0 has to see how to get aligned.  Also, since we have
 * eliminated tiny copies, we know that the count is large enough to
 * encompass the alignment copies.
 */
	beq	v0,zero,forwards	# If v0==0 then aligned
	subu	a3,a3,v1		# a3 = # bytes to get aligned
	LWS	v0,0(from)
	SWS	v0,0(to)		# copy partial word
	PTR_ADDU from,a3
	PTR_ADDU to,a3
	subu	count,a3

/* Once we are here, the pointers are aligned on 32-bit boundaries
 */
forwards:


	and	a3,count,~(32-1)
	beq	a3,zero,16f
	PTR_ADDU a3,a3,to
32:
	lw t0,0(from);   lw t1,4(from);   lw t2,8(from);   lw t3,12(from)
	lw ta0,16(from);  lw ta1,20(from);  lw ta2,24(from);  lw ta3,28(from)
	sw t0,0(to);     sw t1,4(to);     sw t2,8(to);     sw t3,12(to)
	sw ta0,16(to);    sw ta1,20(to);    sw ta2,24(to);    sw ta3,28(to)
	PTR_ADDU from,32
	PTR_ADDU to,32
	bne	a3,to,32b

/* We know we have fewer than 32 bytes remaining, so we do no more
 *	adjustments of the count.
 */
16:	and	v0,count,16
	beq	v0,zero,8f
	lw t0,0(from);   lw t1,4(from);   lw t2,8(from);   lw t3,12(from)
	sw t0,0(to);     sw t1,4(to);     sw t2,8(to);     sw t3,12(to)
	PTR_ADDU from,16
	PTR_ADDU to,16

8:	and	v1,count,8
	beq	v1,zero,4f
	lw	t0,0(from)
	lw	t1,4(from)
	sw	t0,0(to)
	sw	t1,4(to)
	PTR_ADDU from,8
	PTR_ADDU to,8

4:	and	v0,count,4
	beq	v0,zero,3f
	lw	t0,0(from)
	sw	t0,0(to)
	PTR_ADDU from,4
	PTR_ADDU to,4

3:	and	v1,count,3
	PTR_ADDU from,v1
	beq	v1,zero,ret
	PTR_ADDU to,v1
	LWB	t0,-1(from)
	SWB	t0,-1(to)
	j	ra


fmcopy:
/* Missaligned, non-overlap copy of many bytes. This happens too often.
 *  Align the destination for machines with write-thru caches.
 *
 *  This code is always for machines that prefer nops between stores.
 *
 * Here v1=low bits of destination, a3=4.
 */
	beq	v1,zero,fmcopy4		# If v1==0 then destination is aligned
	subu	a3,a3,v1		# a3 = # bytes to align destination
	subu	count,a3
	PTR_ADDU a3,to
1:	lb	v0,0(from)
	PTR_ADDU from,1
	sb	v0,0(to)
	PTR_ADDU to,1
	bne	to,a3,1b

fmcopy4:
	and	a3,count,~(16-1)
	beq	a3,zero,8f
	PTR_ADDU a3,a3,to
16:	LWS t0,0(from);  LWB t0,0+3(from)
	LWS t1,4(from);  LWB t1,4+3(from);  sw t0,0(to)
	LWS t2,8(from);  LWB t2,8+3(from);  sw t1,4(to)
	LWS t3,12(from); LWB t3,12+3(from); sw t2,8(to)
					    sw t3,12(to)
	PTR_ADDU from,16
	PTR_ADDU to,16
	bne	a3,to,16b

8:	and	v1,count,8
	beq	v1,zero,4f
	LWS t0,0(from);  LWB t0,0+3(from)
	LWS t1,4(from);  LWB t1,4+3(from);  sw t0,0(to)
					    sw t1,4(to)
	PTR_ADDU from,8
	PTR_ADDU to,8

4:	and	v0,count,4
	and	count,3
	beq	v0,zero,fbcopy
	LWS t0,0(from);  LWB t0,0+3(from);  sw t0,0(to)
	PTR_ADDU from,4
	PTR_ADDU to,4


/* Byte at a time copy code.  This is used when the byte count is small.
 */
fbcopy:
	PTR_ADDU a3,from,count		# a3 = end+1
	beq	count,zero,ret		# If count is zero, then we are done

1:	lb	v0,0(from)		# v0 = *from
	PTR_ADDU from,1			# advance pointer
	sb	v0,0(to)		# Store byte
	PTR_ADDU to,1			# advance pointer
	bne	from,a3,1b		# Loop until done
ret:	j	ra			# return to caller


/*****************************************************************************/

/*
 * Backward copy code.  Check for pointer alignment and try to get both
 * pointers aligned on a long boundary.
 */
gobackwards:
	PTR_ADDU from,count		# Advance to end + 1
	PTR_ADDU to,count		# Advance to end + 1

	/* small byte counts use byte at a time copy */
	blt	count,MINCOPY,backwards_bytecopy
	and	v0,from,3		# v0 := from & 3
	and	v1,to,3			# v1 := to & 3
	beq	v0,v1,backalignable	# low bits are identical
/*
 * Byte at a time copy code.  This is used when the pointers are not
 * alignable, when the byte count is small, or when cleaning up any
 * remaining bytes on a larger transfer.
 */
backwards_bytecopy:
	beq	count,zero,ret		# If count is zero quit
	PTR_SUBU from,1			# Reduce by one (point at byte)
	PTR_SUBU to,1			# Reduce by one (point at byte)
	PTR_SUBU v1,from,count		# v1 := original from - 1

99:	lb	v0,0(from)		# v0 = *from
	PTR_SUBU from,1			# backup pointer
	sb	v0,0(to)		# Store byte
	PTR_SUBU to,1			# backup pointer
	bne	from,v1,99b		# Loop until done
	j	ra			# return to caller

/*
 * Pointers are alignable, and may be aligned.  Since v0 == v1, we need only
 * check what value v0 has to see how to get aligned.  Also, since we have
 * eliminated tiny copies, we know that the count is large enough to
 * encompass the alignment copies.
 */
backalignable:
	beq	v0,zero,backwards	# If v0==v1 && v0==0 then aligned
	beq	v0,3,back_copy3		# Need to copy 3 bytes to get aligned
	beq	v0,2,back_copy2		# Need to copy 2 bytes to get aligned

/* need to copy 1 byte */
	lb	v0,-1(from)		# get one byte
	PTR_SUBU from,1			# backup pointer
	sb	v0,-1(to)		# store one byte
	PTR_SUBU to,1			# backup pointer
	subu	count,1			#  and reduce count
	b	backwards		# Now pointers are aligned

/* need to copy 2 bytes */
back_copy2:
	lh	v0,-2(from)		# get one short
	PTR_SUBU from,2			# backup pointer
	sh	v0,-2(to)		# store one short
	PTR_SUBU to,2			# backup pointer
	subu	count,2			#  and reduce count
	b	backwards

/* need to copy 3 bytes */
back_copy3:
	lb	v0,-1(from)		# get one byte
	lh	v1,-3(from)		#  and one short
	PTR_SUBU from,3			# backup pointer
	sb	v0,-1(to)		#  store one byte
	sh	v1,-3(to)		#   and one short
	PTR_SUBU to,3			# backup pointer
	subu	count,3			#  and reduce count
	/* FALLTHROUGH */
/*
 * Once we are here, the pointers are aligned on long boundaries.
 * Begin copying in large chunks.
 */
backwards:

/* 32 byte at a time loop */
backwards_32:
	blt	count,32,backwards_16	# do 16 bytes at a time
	lw	v0,-4(from)
	lw	v1,-8(from)
	lw	t0,-12(from)
	lw	t1,-16(from)
	lw	t2,-20(from)
	lw	t3,-24(from)
	lw	ta0,-28(from)
	lw	ta1,-32(from)		# Fetch 8*4 bytes
	PTR_SUBU from,32		# backup from pointer now
	sw	v0,-4(to)
	sw	v1,-8(to)
	sw	t0,-12(to)
	sw	t1,-16(to)
	sw	t2,-20(to)
	sw	t3,-24(to)
	sw	ta0,-28(to)
	sw	ta1,-32(to)		# Store 8*4 bytes
	PTR_SUBU to,32			# backup to pointer now
	subu	count,32		# Reduce count
	b	backwards_32		# Try some more

/* 16 byte at a time loop */
backwards_16:
	blt	count,16,backwards_4	# Do rest in words
	lw	v0,-4(from)
	lw	v1,-8(from)
	lw	t0,-12(from)
	lw	t1,-16(from)
	PTR_SUBU from,16		# backup from pointer now
	sw	v0,-4(to)
	sw	v1,-8(to)
	sw	t0,-12(to)
	sw	t1,-16(to)
	PTR_SUBU to,16			# backup to pointer now
	subu	count,16		# Reduce count
	b	backwards_16		# Try some more

/* 4 byte at a time loop */
backwards_4:
	blt	count,4,backwards_bytecopy	# Do rest
	lw	v0,-4(from)
	PTR_SUBU from,4			# backup from pointer
	sw	v0,-4(to)
	PTR_SUBU to,4			# backup to pointer
	subu	count,4			# Reduce count
	b	backwards_4
XLEAF(bcopy_end)
	END(bcopy)

#endif	/* !TFP */

#undef	from
#undef	to
#undef	count

/*
 * This code ASSUMES the following:
 *	- count is even
 *	- from & to do **not** overlap
 *
 * void swbcopy(from, to, count);
 *	unsigned char *from, *to;
 *	unsigned int count;
 */
#define	from	a0
#define	to	a1
#define	count	a2


LEAF(swbcopy)
	beq	count,zero,2f		# Test for zero count
	beq	from,to,2f		# Test for from == to
	/*
	 * Copy bytes, two at a time.
	 */
1:	lb	v0,0(from)
	lb	v1,1(from)
	PTR_ADDU from,2
	sb	v1,0(to)
	sb	v0,1(to)
	PTR_ADDU to,2
	subu	count,2
	bgt	count,zero,1b

2:	j	ra
	END(swbcopy)
#undef	from
#undef	to
#undef	count

#ifdef IPMHSIM
EXPORT(orb_rmw)
 	j	ra
EXPORT(orh_rmw)
	j	ra
EXPORT(orw_rmw)
	j	ra
EXPORT(andb_rmw)
 	j	ra
EXPORT(andh_rmw)
	j	ra
EXPORT(andw_rmw)
	j	ra
#endif /* IPMHSIM */	
	
/*
 * int rtlock_ownerstamp(caddr_t user_addr, unsigned int pid)
 *
 * Note: This routine assumes a 64bit data field in the rtlock structure,
 *	 where the high 32bits represent the owner and the low 32bits
 *       (not modified here) represent the wait count. 	 		
 */	
LEAF(rtlock_ownerstamp)
	.set	noreorder
	bltz	a0,uerror
	PTR_L	t0,VPDA_CURKTHREAD(zero) # BDSLOT
	li	v0,NF_FSUMEM		# LDSLOT
	CACHE_BARRIER			# t0 dependent, and top factors a0
	sh	v0,K_NOFAULT(t0)
1:	ll	t1, 0(a0)
	or	t1, a1, zero
	/* t0 dependancy on VPDA_CURKTHREAD, and a0 covered by barrier above */
	AUTO_CACHE_BARRIERS_DISABLE
	sc	t1, 0(a0)
#ifdef R10K_LLSC_WAR
	beql	t1, zero, 1b
#else
	beqz	t1, 1b
#endif
	nop
	sh	zero,K_NOFAULT(t0)
	AUTO_CACHE_BARRIERS_ENABLE
	j	ra
	move	v0, zero
	.set	reorder
END(rtlock_ownerstamp)

/*
 *
 * int dumpcopy(kernel_src, kernel_dst, bcount)
 *	long kernel_src, kernel_dst;
 *	long bcount;
 */
NESTED(dumpcopy, COPYIOFRM, zero)
	PTR_SUBU sp,COPYIOFRM
	REG_S	ra,RAOFF(sp)
	.set	noreorder

	AUTO_CACHE_BARRIERS_DISABLE
	PTR_L	t0,VPDA_CURKTHREAD(zero)
	beq	t0,zero, 1f
	li	v0,NF_DUMPCOPY		# LDSLOT
	b	2f
	sh      v0,K_NOFAULT(t0)
	/*	
	 * If we're not being called from a thread, we're being called
	 * from the error handling code and interrupts are disabled.
	 */
1:	sw	v0,VPDA_NOFAULT(zero)
2:	
	jal	bcopy
	 nop

	PTR_L	t0,VPDA_CURKTHREAD(zero)
	beq	t0,zero, 3f
	move	v0,zero			# BDSLOT
	b	4f
	sh      zero,K_NOFAULT(t0)
	/*	
	 * If we're not being called from a thread, we're being called
	 * from the error handling code and interrupts are disabled.
	 */
3:	sw	zero,VPDA_NOFAULT(zero)
4:	
	AUTO_CACHE_BARRIERS_ENABLE

	.set	reorder
	REG_L	ra,RAOFF(sp)
	PTR_ADDU sp,COPYIOFRM
	j	ra
	END(dumpcopy)
