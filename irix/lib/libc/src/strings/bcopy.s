/*
 * Fast bcopy code which supports overlapped copies.
 */
#ident "$Id: bcopy.s,v 1.25 1997/01/27 07:23:59 jwag Exp $"

#include <asm.h>
#include <regdef.h>
#include <sgidefs.h>

.extern _blk_fp 4			# extern _bcopy_fp flag

SAVREG2=	2			# save s2 & v0 regs
SZFRAME2=	(((NARGSAVE+SAVREG2)*SZREG)+ALSZ)&ALMASK
S2OFF=		(SZFRAME2-(1*SZREG))
V0OFF=		(SZFRAME2-(2*SZREG))

SAVREG6=	6			# save gp, ra, a0, a1, a2, v0 regs
SZFRAME6=	(((NARGSAVE+SAVREG6)*SZREG)+ALSZ)&ALMASK
RAOFF=		(SZFRAME6-(1*SZREG))
A0OFF=		(SZFRAME6-(2*SZREG))	
A1OFF=		(SZFRAME6-(3*SZREG))
A2OFF=		(SZFRAME6-(4*SZREG))
VOOFF=		(SZFRAME6-(5*SZREG))
GPOFF=		(SZFRAME6-(6*SZREG))

/*
 * char *bcopy(from, to, count);
 *	unsigned char *from, *to;
 *	unsigned int count;
 *
 * OR
 *
 * void *memcpy/memmove(to, from, count);
 *	unsigned char *to, *from;
 *	unsigned int count;
 *
 * Both functions return "to"
 */

/* registers used */
	
.weakext bcopy, _bcopy
LEAF(_bcopy)
	move	a3, a0		/* temp holding area for source addr */
	move	a0, a1		/* a0 == dest address */
	move	a1, a3		/* a1 == src address */
	END(_bcopy)

#    define	to	a0
#    define	from	a1
#    define	count	a2
LEAF(memcpy)
XLEAF(memmove)

#define NBPW	SZREG

#ifdef _MIPSEB
#    define LWS REG_LLEFT
#    define LWB REG_LRIGHT
#    define SWS REG_SLEFT
#    define SWB REG_SRIGHT
#else
#    define LWS REG_LRIGHT
#    define LWB REG_LLEFT
#    define SWS REG_SRIGHT
#    define SWB REG_SLEFT
#endif /* _MIPSEB */

#if (SZREG == 8)
#    define ULW	uld
#    define USW	usd
#else
#    define ULW	ulw
#    define USW	usw
#endif /* SZREG */

#define LW	REG_L
#define SW	REG_S
#define ADDU	PTR_ADDU
#define ADDIU	PTR_ADDIU
#define SUBU	PTR_SUBU
#define LSUBU	LONG_SUBU

#define MINFPCOPY 256		/* _bcopy_fp threshold value */
#define	MINCOPY	4*NBPW		/* block bcopy threshold value */

/* macro: ULW16,  LW16,  SW16  : 16 words load/store
 *        ULWF16, LWF16, SWF16 : forward copy
 *        ULWB16, LWB16, SWB16 : backward copy
 */

#if (_MIPS_ISA == _MIPS_ISA_MIPS4)
/* These defines are currently optimized for use on Shiva when doing
 * non-FP copies in Mips4 libraries. Shouldn't hurt other non-FP
 * -mips4 users since we're simply using the 16-reg loop earlier (at
 * 8 cachelines instead of 160) and we have a couple of prefetch
 * instructions in the loop (which may not be optimal on non-Shiva but
 * shouldn't hurt).
 */	

	.extern __memcpy_r8000
		
#define THRESHOLD 8		/* threshold value to use 16-reg loop */
#define ULW16(x,y,s)		\
	ULW t0, s*(y+ 0)*NBPW(x); ULW t1, s*(y+ 1)*NBPW(x); \
	.set	nocheckprefetch	;  \
	pref	4,s*3*16*NBPW(x);			  \
	.set	checkprefetch	; \
	ULW t2, s*(y+ 2)*NBPW(x); ULW t3, s*(y+ 3)*NBPW(x); \
	ULW ta0,s*(y+ 4)*NBPW(x); ULW ta1,s*(y+ 5)*NBPW(x); \
	ULW ta2,s*(y+ 6)*NBPW(x); ULW ta3,s*(y+ 7)*NBPW(x); \
	ULW t8, s*(y+ 8)*NBPW(x); ULW t9, s*(y+ 9)*NBPW(x); \
	ULW v0, s*(y+10)*NBPW(x); ULW v1, s*(y+11)*NBPW(x); \
	ULW s0, s*(y+12)*NBPW(x); ULW s1, s*(y+13)*NBPW(x); \
	ULW s2, s*(y+14)*NBPW(x); ULW AT, s*(y+15)*NBPW(x)
#define ULWF16(x)	ULW16(x,0,1)
#define ULWB16(x)	ULW16(x,1,-1)

/* Carefully optimized for Shiva (R10000).  Note that we touch one item for
 * for each d-cache line before the prefetch.  This increases the bandwidth
 * for out-of-cache copies from 72 MB/s to about 80 MB/s.
 */	
#define LW16(x,y,s)		\
	.set	noreorder	; \
	LW t0, s*(y+ 0)*NBPW(x); \
	LW ta0,s*(y+ 4)*NBPW(x); \
	LW t8, s*(y+ 8)*NBPW(x); \
	LW s0, s*(y+12)*NBPW(x); \
	.set	nocheckprefetch	;  \
	pref	4,s*3*16*NBPW(x);			  \
	.set	checkprefetch	;  \
	LW t1, s*(y+ 1)*NBPW(x); \
	LW t2, s*(y+ 2)*NBPW(x); LW t3, s*(y+ 3)*NBPW(x); \
	LW ta1,s*(y+ 5)*NBPW(x); \
	LW ta2,s*(y+ 6)*NBPW(x); LW ta3,s*(y+ 7)*NBPW(x); \
	LW t9, s*(y+ 9)*NBPW(x); \
	LW v0, s*(y+10)*NBPW(x); LW v1, s*(y+11)*NBPW(x); \
	LW s1, s*(y+13)*NBPW(x); \
	LW s2, s*(y+14)*NBPW(x); LW AT, s*(y+15)*NBPW(x); \
	.set	reorder
#define LWF16(x)	LW16(x,0,1)
#define LWB16(x)	LW16(x,1,-1)

#define SW16(x,y,s)		\
	.set	noreorder	;  \
	SW t0, s*(y+ 0)*NBPW(x); SW t1, s*(y+ 1)*NBPW(x); \
	.set	nocheckprefetch	; \
	pref	7,s*3*16*NBPW(x);			  \
	.set	checkprefetch	; \
	SW t2, s*(y+ 2)*NBPW(x); SW t3, s*(y+ 3)*NBPW(x); \
	SW ta0,s*(y+ 4)*NBPW(x); SW ta1,s*(y+ 5)*NBPW(x); \
	SW ta2,s*(y+ 6)*NBPW(x); SW ta3,s*(y+ 7)*NBPW(x); \
	SW t8, s*(y+ 8)*NBPW(x); SW t9, s*(y+ 9)*NBPW(x); \
	SW v0, s*(y+10)*NBPW(x); SW v1, s*(y+11)*NBPW(x); \
	SW s0, s*(y+12)*NBPW(x); SW s1, s*(y+13)*NBPW(x); \
	SW s2, s*(y+14)*NBPW(x); SW AT, s*(y+15)*NBPW(x); \
	.set	reorder
#define SWF16(x)	SW16(x,0,1)
#define SWB16(x)	SW16(x,1,-1)

#else	/* !MIPS4 */		

#if (_MIPS_ISA == _MIPS_ISA_MIPS3)
#define THRESHOLD 32		/* threshold value to use 16-reg loop */
#else	
#define THRESHOLD 160		/* threshold value to use 16-reg loop */
#endif	
	
#define ULW16(x,y,s)		\
	ULW t0, s*(y+ 0)*NBPW(x); ULW t1, s*(y+ 1)*NBPW(x); \
	ULW t2, s*(y+ 2)*NBPW(x); ULW t3, s*(y+ 3)*NBPW(x); \
	ULW ta0,s*(y+ 4)*NBPW(x); ULW ta1,s*(y+ 5)*NBPW(x); \
	ULW ta2,s*(y+ 6)*NBPW(x); ULW ta3,s*(y+ 7)*NBPW(x); \
	ULW t8, s*(y+ 8)*NBPW(x); ULW t9, s*(y+ 9)*NBPW(x); \
	ULW v0, s*(y+10)*NBPW(x); ULW v1, s*(y+11)*NBPW(x); \
	ULW s0, s*(y+12)*NBPW(x); ULW s1, s*(y+13)*NBPW(x); \
	ULW s2, s*(y+14)*NBPW(x); ULW AT, s*(y+15)*NBPW(x)
#define ULWF16(x)	ULW16(x,0,1)
#define ULWB16(x)	ULW16(x,1,-1)
#define LW16(x,y,s)		\
	LW t0, s*(y+ 0)*NBPW(x); LW t1, s*(y+ 1)*NBPW(x); \
	LW t2, s*(y+ 2)*NBPW(x); LW t3, s*(y+ 3)*NBPW(x); \
	LW ta0,s*(y+ 4)*NBPW(x); LW ta1,s*(y+ 5)*NBPW(x); \
	LW ta2,s*(y+ 6)*NBPW(x); LW ta3,s*(y+ 7)*NBPW(x); \
	LW t8, s*(y+ 8)*NBPW(x); LW t9, s*(y+ 9)*NBPW(x); \
	LW v0, s*(y+10)*NBPW(x); LW v1, s*(y+11)*NBPW(x); \
	LW s0, s*(y+12)*NBPW(x); LW s1, s*(y+13)*NBPW(x); \
	LW s2, s*(y+14)*NBPW(x); LW AT, s*(y+15)*NBPW(x)
#define LWF16(x)	LW16(x,0,1)
#define LWB16(x)	LW16(x,1,-1)

#define SW16(x,y,s)		\
	SW t0, s*(y+ 0)*NBPW(x); SW t1, s*(y+ 1)*NBPW(x); \
	SW t2, s*(y+ 2)*NBPW(x); SW t3, s*(y+ 3)*NBPW(x); \
	SW ta0,s*(y+ 4)*NBPW(x); SW ta1,s*(y+ 5)*NBPW(x); \
	SW ta2,s*(y+ 6)*NBPW(x); SW ta3,s*(y+ 7)*NBPW(x); \
	SW t8, s*(y+ 8)*NBPW(x); SW t9, s*(y+ 9)*NBPW(x); \
	SW v0, s*(y+10)*NBPW(x); SW v1, s*(y+11)*NBPW(x); \
	SW s0, s*(y+12)*NBPW(x); SW s1, s*(y+13)*NBPW(x); \
	SW s2, s*(y+14)*NBPW(x); SW AT, s*(y+15)*NBPW(x)
#define SWF16(x)	SW16(x,0,1)
#define SWB16(x)	SW16(x,1,-1)
#endif /* MIPS4 */	

/* macro: ULW16_12,  LW16_12,  SW16_12  : 12 words load/store
 *        ULWF16_12, LWF16_12, SWF16_12 : forward copy
 *        ULWB16_12, LWB16_12, SWB16_12 : backward copy
 *
 *        ULW16_4,  LW16_4,  SW16_4  : 4 words load/store
 *        ULWF16_4, LWF16_4, SWF16_4 : forward copy
 *        ULWB16_4, LWB16_4, SWB16_4 : backward copy
 */
#define ULW16_12(x,y,s)		\
	ULW8(x,y,s); \
	ULW t8, s*(y+ 8)*NBPW(x); ULW t9, s*(y+ 9)*NBPW(x); \
	ULW v1, s*(y+10)*NBPW(x); ULW AT, s*(y+11)*NBPW(x)
#define ULWF16_12(x)	ULW16_12(x,0,1)
#define ULWB16_12(x)	ULW16_12(x,1,-1)

#define LW16_12(x,y,s)		\
	LW8(x,y,s); \
	LW t8, s*(y+ 8)*NBPW(x); LW t9, s*(y+ 9)*NBPW(x); \
	LW v1, s*(y+10)*NBPW(x); LW AT, s*(y+11)*NBPW(x)
#define LWF16_12(x)	LW16_12(x,0,1)
#define LWB16_12(x)	LW16_12(x,1,-1)

#define SW16_12(x,y,s)		\
	SW8(x,y,s); \
	SW t8, s*(y+ 8)*NBPW(x); SW t9, s*(y+ 9)*NBPW(x); \
	SW v1, s*(y+10)*NBPW(x); SW AT, s*(y+11)*NBPW(x)
#define SWF16_12(x)	SW16_12(x,0,1)
#define SWB16_12(x)	SW16_12(x,1,-1)

#define ULW16_4(x,y,s)		\
	ULW t0, s*(y+12)*NBPW(x); ULW t1, s*(y+13)*NBPW(x); \
	ULW t2, s*(y+14)*NBPW(x); ULW t3, s*(y+15)*NBPW(x)
#define ULWF16_4(x)	ULW16_4(x,0,1)
#define ULWB16_4(x)	ULW16_4(x,1,-1)

#define LW16_4(x,y,s)		\
	LW t0, s*(y+12)*NBPW(x); LW t1, s*(y+13)*NBPW(x); \
	LW t2, s*(y+14)*NBPW(x); LW t3, s*(y+15)*NBPW(x)
#define LWF16_4(x)	LW16_4(x,0,1)
#define LWB16_4(x)	LW16_4(x,1,-1)

#define SW16_4(x,y,s)		\
	SW t0, s*(y+12)*NBPW(x); SW t1, s*(y+13)*NBPW(x); \
	SW t2, s*(y+14)*NBPW(x); SW t3, s*(y+15)*NBPW(x)
#define SWF16_4(x)	SW16_4(x,0,1)
#define SWB16_4(x)	SW16_4(x,1,-1)


/* macro: ULW8,  LW8,  SW8  : 8 words load/store
 *        ULWF8, LWF8, SWF8 : forward copy
 *        ULWB8, LWB8, SWB8 : backward copy
 */
#define ULW8(x,y,s)		\
	ULW t0, s*(y+0)*NBPW(x); ULW t1, s*(y+1)*NBPW(x); \
	ULW t2, s*(y+2)*NBPW(x); ULW t3, s*(y+3)*NBPW(x); \
	ULW ta0,s*(y+4)*NBPW(x); ULW ta1,s*(y+5)*NBPW(x); \
	ULW ta2,s*(y+6)*NBPW(x); ULW ta3,s*(y+7)*NBPW(x)
#define ULWF8(x)	ULW8(x,0,1)
#define ULWB8(x)	ULW8(x,1,-1)

#define LW8(x,y,s)		\
	LW t0, s*(y+0)*NBPW(x); LW t1, s*(y+1)*NBPW(x); \
	LW t2, s*(y+2)*NBPW(x); LW t3, s*(y+3)*NBPW(x); \
	LW ta0,s*(y+4)*NBPW(x); LW ta1,s*(y+5)*NBPW(x); \
	LW ta2,s*(y+6)*NBPW(x); LW ta3,s*(y+7)*NBPW(x)
#define LWF8(x)	LW8(x,0,1)
#define LWB8(x)	LW8(x,1,-1)

#define SW8(x,y,s)		\
	SW t0, s*(y+0)*NBPW(x); SW t1, s*(y+1)*NBPW(x); \
	SW t2, s*(y+2)*NBPW(x); SW t3, s*(y+3)*NBPW(x); \
	SW ta0,s*(y+4)*NBPW(x); SW ta1,s*(y+5)*NBPW(x); \
	SW ta2,s*(y+6)*NBPW(x); SW ta3,s*(y+7)*NBPW(x)
#define SWF8(x)	SW8(x,0,1)
#define SWB8(x)	SW8(x,1,-1)


/* macro: ULW4,  LW4,  SW4  : 4 words load/store
 *        ULWF4, LWF4, SWF4 : forward copy
 *        ULWB4, LWB4, SWB4 : backward copy
 */
#define ULW4(x,y,s)		\
	ULW t0,s*(y+0)*NBPW(x); ULW t1,s*(y+1)*NBPW(x); \
	ULW t2,s*(y+2)*NBPW(x); ULW t3,s*(y+3)*NBPW(x)
#define ULWF4(x)	ULW4(x,0,1)
#define ULWB4(x)	ULW4(x,1,-1)

#define LW4(x,y,s)		\
	LW t0,s*(y+0)*NBPW(x); LW t1,s*(y+1)*NBPW(x); \
	LW t2,s*(y+2)*NBPW(x); LW t3,s*(y+3)*NBPW(x)
#define LWF4(x)	LW4(x,0,1)
#define LWB4(x)	LW4(x,1,-1)

#define SW4(x,y,s)		\
	SW t0,s*(y+0)*NBPW(x); SW t1,s*(y+1)*NBPW(x); \
	SW t2,s*(y+2)*NBPW(x); SW t3,s*(y+3)*NBPW(x)
#define SWF4(x)	SW4(x,0,1)
#define SWB4(x)	SW4(x,1,-1)


/* macro: ULW2,  LW2,  SW2  : 2 words load/store
 *        ULWF2, LWF2, SWF2 : forward copy
 *        ULWB2, LWB2, SWB2 : backward copy
 */
#define ULW2(x,y,s)		\
	ULW t0,s*(y+0)*NBPW(x); ULW t1,s*(y+1)*NBPW(x)
#define ULWF2(x)	ULW2(x,0,1)
#define ULWB2(x)	ULW2(x,1,-1)

#define LW2(x,y,s)		\
	LW t0,s*(y+0)*NBPW(x); LW t1,s*(y+1)*NBPW(x)
#define LWF2(x)		LW2(x,0,1)
#define LWB2(x)		LW2(x,1,-1)

#define SW2(x,y,s)		\
	SW t0,s*(y+0)*NBPW(x); SW t1,s*(y+1)*NBPW(x)
#define SWF2(x)		SW2(x,0,1)
#define SWB2(x)		SW2(x,1,-1)


/*****************************************************************************/

	.set	at
	move	v0,to			# save to in v0
	beq	from,to,ret		# test for from == to

	/* use backwards copying code if the from and to regions overlap */
	bltu	to,from,goforwards	# If to < from then use forwards copy
	ADDU	t8,from,count		# t8 := from + count
	bltu	to,t8,gobackwards	# backwards if from<to<from+count

/*
 * Forward copy code.  Check for pointer alignment and try to get both
 * pointers aligned on a long boundary.
 */
goforwards:
	bgeu	count,MINCOPY,frwd_blkcopy

/*
 * Unaligned word likely copy. This is used when the byte count is
 * less than MINCOPY.
 */
	bltu	count,NBPW,2f
1:	ULW	t8,0(from)		# do as many words as possible
	USW	t8,0(to)
	LSUBU	count,NBPW
	ADDIU	from,NBPW
	ADDIU	to,NBPW
	bgeu	count,NBPW,1b

2:	beq	count,zero,ret		# If count is zero, then we are done
	lbu	t0,0(from)		# finish partial word
	sb	t0,0(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t1,1(from)
	sb	t1,1(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t2,2(from)
	sb	t2,2(to)
#if (SZREG == 8)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t0,3(from)
	sb	t0,3(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t1,4(from)
	sb	t1,4(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t2,5(from)
	sb	t2,5(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t0,6(from)
	sb	t0,6(to)
#endif	/* SZREG == 8 */

ret:	j	ra

/*
 * forward block copy
 */
frwd_blkcopy:
#if (_MIPS_ISA == _MIPS_ISA_MIPS4)
	SETUP_GPX(t8)			# expensive checking of _blk_fp flag
	SUBU	sp,SZFRAME6
	SETUP_GPX64(GPOFF,t8)
	SAVE_GP(GPOFF)
	lw	a3,_blk_fp		# check _blk_fp flag
	bnez	a3,do_fpbcopy		# do _bcopy_fp
	RESTORE_GP64
	ADDIU	sp,SZFRAME6
#endif /* !MIPS4 */
	and	t8,from,NBPW-1		# t8 := from & NBPW-1
	and	t9,to,NBPW-1		# t9 := to & NBPW-1 
	li	a3,NBPW
	bne	t8,t9,frwd_unalign	# low bits are different

/*
 * Pointers are alignable, and may be aligned.  Since t8 == t9, we need only
 * check what value t8 has to see how to get aligned.  Also, since we have
 * eliminated tiny copies, we know that the count is large enough to
 * encompass the alignment copies.
 */
frwd_align:
	beq	t8,zero,forwards	# If t8==t9 && t8==0 then aligned
	LSUBU	a3,t9			# a3 = # bytes to get aligned
	LWS	t8,0(from)
	SWS	t8,0(to)		# copy partial word
	ADDU	from,a3
	ADDU	to,a3
	LSUBU	count,a3

/*
 * Once we are here, the pointers are aligned on long boundaries.
 * Begin copying in large chunks.
 */
forwards:
	and	a3,count,~(16*NBPW-1)	# number of 16*NBPW blocks
	sltiu	t0,count,MINFPCOPY	# threshold of _bzero_fp
	bnez	t0,1f			# check for using _bzero_fp(TFP)

#if (_MIPS_ISA != _MIPS_ISA_MIPS4)
	SETUP_GPX(t8)			# expensive checking of _blk_fp flag
	SUBU	sp,SZFRAME6
	SETUP_GPX64(GPOFF,t8)
	SAVE_GP(GPOFF)
	lw	a3,_blk_fp		# check _blk_fp flag
	bnez	a3,do_fpbcopy		# do _bcopy_fp
	
nmlbcpy:
	RESTORE_GP64
	ADDIU	sp,SZFRAME6
#endif /* !MIPS4 */

	and	a3,count,~(16*NBPW-1)	# number of 16*NBPW blocks
	
1:	andi	t8,count,8*NBPW		# 8 more words remaining? BDSLOT
	beqz	a3,8f
	ADDU	a3,to
	sub	t9,count,16*NBPW*THRESHOLD # worth using 16 regs?
	bltz	t9,12f

	SUBU	sp,SZFRAME2
	mthi	s0
	mtlo	s1			# save s0 & s1 in the hi & lo regs
	SW	s2,S2OFF(sp)		# save s2 & v0 in the stack
	SW	v0,V0OFF(sp)

16:	.set	noat
	LWF16(from)			# load 16 words
	SWF16(to)			# store 16 words
	.set	at

	ADDIU	from,16*NBPW
	ADDIU	to,16*NBPW
	bne	a3,to,16b

	LW	v0,V0OFF(sp)		# restore s2 & v0 from the stack
	LW	s2,S2OFF(sp)
	mfhi	s0			# restore s0 & s1 from the hi&lo regs
	mflo	s1
	ADDIU	sp,SZFRAME2
	and	t8,count,8*NBPW		# 8 more words remaining? BDSLOT
	b	8f

12:	.set	noat
	LWF16_12(from)			# load 12 words
	SWF16_12(to)			# store 12 words
	LWF16_4(from)			# load next 4 words
	SWF16_4(to)			# store next 4 words
	.set    at

	ADDIU	from,16*NBPW
	ADDIU	to,16*NBPW
	bne     a3,to,12b

	and	t8,count,8*NBPW		# 8 more words remaining?

8:	and	t9,count,4*NBPW		# 4 more words remaining? BDSLOT
	beq	t8,zero,4f
	LWF8(from)			# load 8 words
	SWF8(to)			# store 8 words
	ADDIU	from,8*NBPW
	ADDIU	to,8*NBPW

4:	and	t8,count,2*NBPW		# 2 more words remaining? BDSLOT
	beq	t9,zero,2f
	LWF4(from)			# load 4 words
	SWF4(to)			# store 4 words
	ADDIU	from,4*NBPW
	ADDIU	to,4*NBPW

afrwd_2:
2:	and	t9,count,1*NBPW		# 1 more words remaining? BDSLOT
	beq	t8,zero,1f
	LWF2(from)			# load 2 words
	SWF2(to)			# store 2 words
	ADDIU	from,2*NBPW
	ADDIU	to,2*NBPW

afrwd_1:
1:	and	count,NBPW-1		# now we have < 1 word remaining
	beq	t9,zero,3f
	LW	t0,0(from)		# load 1 word
	SW	t0,0(to)		# store 1 word
	ADDIU	from,NBPW
	ADDIU	to,NBPW

3:	ADDU	from,count
	beq	count,zero,9f		# If count is zero, then we are done
	ADDU	to,count
	LWB	t8,-1(from)		# finish partial word
	SWB	t8,-1(to)
9:	j	ra

/* Missaligned, non-overlap copy of many bytes. This happens too often.
 *  Align the destination for machines with write-thru caches.
 *
 * Here t9=low bits of destination, a3=4.
 */
frwd_unalign:
	beq	t9,zero,10f		# If t9==0 then destination is aligned
	ULW	t8,0(from)		# do partial word
	SWS	t8,0(to)
	LSUBU	a3,a3,t9		# a3 = # bytes to align destination
	ADDU	from,a3
	ADDU	to,a3
	LSUBU	count,a3

10:	and	a3,count,~(16*NBPW-1)	# do as many 16 words/time as possible
	and	t8,count,8*NBPW		# 8 more words remaining? BDSLOT
	beq	a3,zero,8f
	ADDU	a3,a3,to
	sub	t9,count,16*NBPW*THRESHOLD # worth using 16 regs?
	bltz	t9,12f

	SUBU	sp,SZFRAME2
	mthi	s0			# save s0 & s1 in the hi & lo regs
	mtlo	s1
	SW	s2,S2OFF(sp)		# save s2 & v0 in the stack
	SW	v0,V0OFF(sp)

16:	.set	noat
	ULWF16(from)			# load 16 unaligned words
	SWF16(to)			# store 16 words
	.set	at

	ADDIU	from,16*NBPW
	ADDIU	to,16*NBPW
	bne	a3,to,16b

	LW	v0,V0OFF(sp)		# restore s2 & v0 from the stack
	LW	s2,S2OFF(sp)
	mfhi	s0			# restore s0 & s1 from the hi&lo regs
	mflo	s1
	ADDIU	sp,SZFRAME2
	and	t8,count,8*NBPW		# 8 more words remaining? BDSLOT
	b	8f

12:	.set	noat
	ULWF16_12(from)			# load 12 unaligned words
	SWF16_12(to)			# store 12 words
	ULWF16_4(from)			# load next 4 unaligned words
	SWF16_4(to)			# store next 4 words
	.set    at

	ADDIU	from,16*NBPW
	ADDIU	to,16*NBPW
	bne     a3,to,12b

	and	t8,count,8*NBPW		# 8 more words remaining?

8:	and	t9,count,4*NBPW		# 4 more words remaining? BDSLOT
	beq	t8,zero,4f
	ULWF8(from)			# load 8 unaligned words
	SWF8(to)			# store 8 words
	ADDIU	from,8*NBPW
	ADDIU	to,8*NBPW

4:	and	t8,count,2*NBPW		# 2 more words remaining? BDSLOT
	beq	t9,zero,2f
	ULWF4(from)			# load 4 unaligned words
	SWF4(to)			# store 4 words
	ADDIU	from,4*NBPW
	ADDIU	to,4*NBPW

2:	and	t9,count,1*NBPW		# 1 more words remaining? BDSLOT
	beq	t8,zero,1f
	ULWF2(from)			# load 2 unaligned words
	SWF2(to)			# store 2 words
	ADDIU	from,2*NBPW
	ADDIU	to,2*NBPW

1:	and	count,NBPW-1		# less than 1 word remaining? BDSLOT
	beq	t9,zero,3f
	ULW	t0, 0*NBPW(from)	# load 1 unaligned word
	SW	t0, 0*NBPW(to)		# store 1 word
	ADDIU	from,NBPW
	ADDIU	to,NBPW

3:	ADDU	from,count
	beq	count,zero,9f		# If count is zero, then we are done
	ADDU	to,count
	ULW	t8,-1*NBPW(from)	# finish partial word
	SWB	t8,-1(to)
9:	j	ra

/*
 * Backward copy code.  Check for pointer alignment and try to get both
 * pointers aligned on a long boundary.
 */
gobackwards:
	ADDU	from,count		# Advance to end + 1
	ADDU	to,count		# Advance to end + 1

	bgeu	count,MINCOPY,bkwd_blkcopy
/*
 * Unaligned word likely copy. This is used when the byte count is
 * less than MINCOPY.
 */
	bltu	count,NBPW,2f
1:	ULW	t8,-1*NBPW(from)	# do as many words as possible
	USW	t8,-1*NBPW(to)
	LSUBU	count,NBPW
	SUBU	from,NBPW
	SUBU	to,NBPW
	bgeu	count,NBPW,1b

2:	beq	count,zero,ret		# If count is zero, then we are done
	lbu	t0,-1(from)		# finish partial word
	sb	t0,-1(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t1,-2(from)
	sb	t1,-2(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t2,-3(from)
	sb	t2,-3(to)
#if (SZREG == 8)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t0,-4(from)
	sb	t0,-4(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t1,-5(from)
	sb	t1,-5(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t2,-6(from)
	sb	t2,-6(to)
	LSUBU	count,1
	beq	count,zero,ret
	lbu	t0,-7(from)
	sb	t0,-7(to)
#endif	/* SZREG == 8 */

9:	j	ra			# return to caller

/*
 * backward block copy
 */
bkwd_blkcopy:
#if (_MIPS_ISA == _MIPS_ISA_MIPS4)
	SETUP_GPX(t8)			# expensive checking of _blk_fp flag
	SUBU	sp,SZFRAME6
	SETUP_GPX64(GPOFF,t8)
	SAVE_GP(GPOFF)
	lw	a3,_blk_fp		# check _blk_fp flag
	bnez	a3,back_do_fpbcopy		# do _bcopy_fp
	RESTORE_GP64
	ADDIU	sp,SZFRAME6
#endif /* !MIPS4 */
	and	t8,from,NBPW-1		# t8 := from & NBPW-1
	and	t9,to,NBPW-1		# t9 := to & NBPW-1
	bne	t8,t9,bkwd_unalign	# low bits are different

/*
 * Pointers are alignable, and may be aligned.  Since t8 == t9, we need only
 * check what value t8 has to see how to get aligned.  Also, since we have
 * eliminated tiny copies, we know that the count is large enough to
 * encompass the alignment copies.
 */
bkwd_align:
	beq	t8,zero,backwards	# If t8==t9 && t8==0 then aligned
	LWB	t8,-1(from)
	SWB	t8,-1(to)		# copy partial word
	SUBU	from,t9			# t9 = # bytes to get aligned
	SUBU	to,t9
	LSUBU	count,t9

/*
 * Once we are here, the pointers are aligned on long boundaries.
 * Begin copying in large chunks.
 */
backwards:
	and	a3,count,~(16*NBPW-1)	# do as many 16 words/time as possible
	and	t8,count,8*NBPW		# 8 more words remaining? BDSLOT
	beq	a3,zero,8f
	SUBU	a3,to,a3
	LSUBU	t9,count,16*NBPW*THRESHOLD # worth using 16 regs?
	bltz	t9,12f

	SUBU	sp,SZFRAME2
	mthi	s0
	mtlo	s1			# save s0 & s1 in the hi & lo regs
	SW	s2,S2OFF(sp)		# save s2 & v0 in the stack
	SW	v0,V0OFF(sp)

16:	.set	noat
	LWB16(from)			# backward load 16 words
	SWB16(to)			# backward store 16 words
	.set	at
	SUBU	from,16*NBPW
	SUBU	to,16*NBPW
	bne	a3,to,16b

	LW	v0,V0OFF(sp)
	LW	s2,S2OFF(sp)		# restore s2 & v0 from the stack
	mfhi	s0			# restore s0 & s1 from the hi&lo regs
	mflo	s1
	ADDIU	sp,SZFRAME2
	and	t8,count,8*NBPW		# 8 more words remaining? BDSLOT
	b	8f

12:	.set	noat
	LWB16_12(from)			# backward load 12 words
	SWB16_12(to)			# backward store 12 words
	LWB16_4(from)			# backward load next 4 words
	SWB16_4(to)			# backward store next 4 words
	.set	at

	SUBU	from,16*NBPW
	SUBU	to,16*NBPW
	bne	a3,to,12b

	and	t8,count,8*NBPW		# 8 more words remaining?

8:	and	t9,count,4*NBPW		# 4 more words remaining? BDSLOT
	beq	t8,zero,4f
	LWB8(from)			# backward load 8 words
	SWB8(to)			# backward store 8 words
	SUBU	from,8*NBPW
	SUBU	to,8*NBPW

4:	and	t8,count,2*NBPW		# 2 more words remaining? BDSLOT
	beq	t9,zero,2f
	LWB4(from)			# backward load 4 words
	SWB4(to)			# backward store 4 words
	SUBU	from,4*NBPW
	SUBU	to,4*NBPW

2:	and	t9,count,1*NBPW		# 1 more words remaining? BDSLOT
	beq	t8,zero,1f
	LWB2(from)			# backward load 2 words
	SWB2(to)			# backward store 2 words
	SUBU	from,2*NBPW
	SUBU	to,2*NBPW

1:	and	count,NBPW-1		# now we have < 1 word remaining
	beq	t9,zero,3f
	LW	t0,-1*NBPW(from)	# backward load 1 word
	SW	t0,-1*NBPW(to)		# backward store 1 word
	SUBU	from,NBPW
	SUBU	to,NBPW

3:	SUBU	from,count
	beq	count,zero,9f		# If count is zero, then we are done
	SUBU	to,count
	LWS	t8,0(from)		# finish partial word
	SWS	t8,0(to)
9:	j	ra

/* Missaligned, non-overlap copy of many bytes. This happens too often.
 *  Align the destination for machines with write-thru caches.
 *
 * Here t9=low bits of destination.
 */
bkwd_unalign:
	beq	t9,zero,10f		# If t9==0 then destination is aligned
	ULW	t8,-1*NBPW(from)
	SWB	t8,-1(to)
	SUBU	from,t9
	SUBU	to,t9
	LSUBU	count,t9

10:	and	a3,count,~(16*NBPW-1)	# do as many 16 words/time as possible
	and	t8,count,8*NBPW		# 8 more words remaining? BDSLOT
	beq	a3,zero,8f
	SUBU	a3,to,a3
	LSUBU	t9,count,16*NBPW*THRESHOLD # worth using 16 regs?
	bltz	t9,12f

	SUBU	sp,SZFRAME2
	mthi	s0			# save s0 & s1 in the hi & lo regs
	mtlo	s1
	SW	s2,S2OFF(sp)		# save s2 & v0 in the stack
	SW	v0,V0OFF(sp)

16:	.set	noat	
	ULWB16(from)			# backward load 16 unaligned words
	SWB16(to)			# backward store 16 words
	.set	at
	SUBU	from,16*NBPW
	SUBU	to,16*NBPW
	bne	a3,to,16b

	LW	v0,V0OFF(sp)		# restore s2 & v0 from the stack
	LW	s2,S2OFF(sp)
	mfhi	s0			# restore s0 & s1 from the hi&lo regs
	mflo	s1
	ADDIU	sp,SZFRAME2
	and	t8,count,8*NBPW		# 8 more words remaining? BDSLOT
	b	8f

12:	.set	noat	
	ULWB16_12(from)			# backward load 12 unaligned words
	SWB16_12(to)			# backward store 12 words
	ULWB16_4(from)			# backward load next 4 unaligned words
	SWB16_4(to)			# backward store 4 words
	.set	at

	SUBU	from,16*NBPW
	SUBU	to,16*NBPW
	bne	a3,to,12b

	and	t8,count,8*NBPW		# 8 more words remaining? BDSLOT

8:	and	t9,count,4*NBPW		# 4 more words remaining? BDSLOT
	beq	t8,zero,4f
	ULWB8(from)			# backward load 8 unaligned words
	SWB8(to)			# backward store 8 words
	SUBU	from,8*NBPW
	SUBU	to,8*NBPW

4:	and	t8,count,2*NBPW		# 2 more words remaining? BDSLOT
	beq	t9,zero,2f
	ULWB4(from)			# backward load 4 unaligned words
	SWB4(to)			# backward store 4 words
	SUBU	from,4*NBPW
	SUBU	to,4*NBPW

2:	and	t9,count,1*NBPW		# 1 more words remaining? BDSLOT
	beq	t8,zero,1f
	ULWB2(from)			# backward load 2 unaligned words
	SWB2(to)			# backward store 2 words
	SUBU	from,2*NBPW
	SUBU	to,2*NBPW

1:	and	count,NBPW-1		# less than 1 word remaining? BDSLOT
	beq	t9,zero,3f
	ULW	t0,-1*NBPW(from)	# backward load 1 unaligned word
	SW	t0,-1*NBPW(to)		# backward store 1 word
	SUBU	from,NBPW
	SUBU	to,NBPW

3:	SUBU	from,count
	beq	count,zero,9f		# If count is zero, then we are done
	SUBU	to,count
	ULW	t8,0(from)		# finish partial word
	SWS	t8,0(to)
9:	j	ra			# return to caller

#if (_MIPS_ISA == _MIPS_ISA_MIPS4)	
back_do_fpbcopy:
	/* need to restore the original source & destination */
	SUBU	from,count
	SUBU	to,count
do_fpbcopy:
	bltz	a3,init_blk_fp
	LA	a3,__memcpy_r8000
	RESTORE_GP64
	ADDIU	sp,SZFRAME6
	jr	a3

init_blk_fp:				# _blk_fp hasn't been set up
	SW	ra,RAOFF(sp)
	SW	a0,A0OFF(sp)
	SW	a1,A1OFF(sp)
	SW	a2,A2OFF(sp)
	jal	_blk_init		# call _blk_init

	LW	ra,RAOFF(sp)
	LW	a0,A0OFF(sp)
	LW	a1,A1OFF(sp)
	LW	a2,A2OFF(sp)
	RESTORE_GP64
	ADDIU	sp,SZFRAME6
	b	memcpy
		
#else
/* bcopy using fp registers, currently only available for TFP
 */
do_fpbcopy:
	bgtz	a3,call_bcopy_fp

init_blk_fp:				# _blk_fp hasn't been set up
	SW	ra,RAOFF(sp)
	SW	a0,A0OFF(sp)
	SW	a1,A1OFF(sp)
	SW	a2,A2OFF(sp)
	SW	v0,VOOFF(sp)		# BDSLOT
	jal	_blk_init		# call _blk_init

	LW	ra,RAOFF(sp)
	LW	a0,A0OFF(sp)
	LW	a1,A1OFF(sp)
	LW	a2,A2OFF(sp)
	LW	v0,VOOFF(sp)		# BDSLOT
	lw	a3,_blk_fp		# check _blk_fp flag
	beqz	a3,nmlbcpy		# go back to do normal bcopy
		
call_bcopy_fp:
#if (SZREG == 4)
	/* make sure double word(8-byte) alignment for NBPW(SZREG) == 4.
	 */
	and	v1,from,(2*NBPW-1)	# check dw (8 bytes) alignment
	and	t8,to,(2*NBPW-1)	# check dw (8 bytes) alignment
	bne	v1,t8,nmlbcpy		# too bad!
	beqz	v1,9f			# both are dw aligned

	lw	t8,0(from)		# align to dw
	ADDIU	from,NBPW
	sw	t8,0(to)
	ADDIU	to,NBPW
	LSUBU	count,NBPW
9:
#endif /* SZREG == 4 */
	/* since MINFPCOPY >> 16 bytes, count is still > 16 bytes at here.
	 */
	andi	a3,count,(16-1)		# residual bytes after _bcopy_fp

	SW	ra,RAOFF(sp)
	jal	_memcpy_fp		# call _memcpy_fp
	LW	ra,RAOFF(sp)

	RESTORE_GP64
	ADDIU	sp,SZFRAME6

#if (SZREG == 8)
	and	t9,count,1*NBPW		# BDSLOT
	bnez	count,afrwd_1		# finish rest of bytes
#else
	and	t8,count,2*NBPW		# BDSLOT
	bnez	count,afrwd_2		# finish rest of bytes
#endif /* SZREG == 8 */

	j	ra			# return to caller
#endif /* !MIPS4 */
	
	END(memcpy)
