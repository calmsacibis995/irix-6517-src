#ident	"$Revision: 1.42 $"

/*
 * libasm.s - assembler code for widely used standalone functions
 */

#include <ml.h>
#include <asm.h>
#include <regdef.h>
#include <sys/signal.h>
#include <sys/fpu.h>
#include <sys/sbd.h>
#include <ml.h>

#define	NBPW		4
#define	BLKSIZE		(SZREG * 8)	/* bzero/bcopy this amount at a time */

/*
 * void bcopy(from, to, count);
 *	unsigned char *from, *to;
 *	unsigned long count;
 */

#define	MINCOPY	15

/* registers used */
#define	from	a0
#define	to	a1
#define	count	a2

#ifdef	_MIPSEB
#define	UNALIGNED_LOAD(reg,offset,src)				\
	REG_LLEFT	reg,(offset)(src);			\
	REG_LRIGHT	reg,(offset)+SZREG-1(src)
#define	UNALIGNED_STORE(reg,offset,dst)				\
	REG_SLEFT	reg,(offset)(dst);			\
	REG_SRIGHT	reg,(offset)+SZREG-1(dst)
#else	/* _MIPSEL */
#define	UNALIGNED_LOAD(reg,offset,src)				\
	REG_LRIGHT	reg,(offset)(src);			\
	REG_LLEFT	reg,(offset)+SZREG-1(src)
#define	UNALIGNED_STORE(reg,offset,dst)				\
	REG_SRIGHT	reg,(offset)(dst);			\
	REG_SLEFT	reg,(offset)+SZREG-1(dst)
#endif	/* _MIPSEL */

LEAF(bcopy)
	beq	count,zero,ret		# Test for zero count
	beq	from,to,ret		# Test for from == to

	/* use backwards copying code if from < to < from+count */
	blt	to,from,goforwards	# If to < from then use forwards copy
	PTR_ADDU	v0,from,count	# v0 = from+count
	blt	to,v0,gobackwards	# If to < v0 then use backwards copy

/*****************************************************************************/

/*
 * Forward copy code.  Check for pointer alignment and try to get both
 * pointers aligned on long boundary.
 */
goforwards:
	/* small byte counts use byte at a time copy */
	blt	count,MINCOPY,f_bytecopy

	/*
	 * check whether the pointers are long alignable by checking the
	 * lowest log2(SZREG) bits
	 */
	and	v0,from,SZREG-1
	and	v1,to,SZREG-1
	bne	v0,v1,f_nonaligned_blk	# low bits are different

/*
 * Pointers are long alignable, and may already be aligned.
 * Since v0 == v1, we need only check what value v0 has to see how
 * to get aligned.  Also, since we have eliminated tiny copies, we
 * know that the count is large enough to encompass the alignment copies.
 */
	beq	v0,zero,f_aligned_blk	# v0==0 => already aligned

	li	t0,SZREG
	LONG_SUBU	v1,t0,v1	# v1 = # bytes needed to get aligned
#ifdef	_MIPSEB
	REG_LLEFT	v0,0(from)	# Fetch partial long
	REG_SLEFT	v0,0(to)	# Store partial long
#else	/* _MIPSEL */
	REG_LRIGHT	v0,0(from)	# Fetch partial long
	REG_SRIGHT	v0,0(to)	# Store partial long
#endif	/* _MIPSEL */

	PTR_ADDU	from,v1		# advance src pointer
	PTR_ADDU	to,v1		# advance dst pointer
	LONG_SUBU	count,v1	# update remaining byte count

/*
 * Once we are here, the pointers are aligned on long boundaries.
 * Begin copying in large chunks.
 */
	.set	noreorder
f_aligned_blk:
	and	v0,count,~(BLKSIZE-1)	# number of BLKSIZE chunks
	beq	v0,zero,f_aligned_halfblk
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_ADDU	v0,from		# terminal src pointer

/* BLKSIZE byte at a time loop */
1:	CACHE_BARRIER
	REG_L	t0,0*SZREG(from)
	REG_L	t1,1*SZREG(from)
	REG_L	t2,2*SZREG(from)
	REG_L	t3,3*SZREG(from)
	REG_L	ta0,4*SZREG(from)
	REG_L	ta1,5*SZREG(from)
	REG_L	ta2,6*SZREG(from)
	REG_L	ta3,7*SZREG(from)
	PTR_ADDU	from,BLKSIZE	# advance src pointer
	REG_S	t0,0*SZREG(to)
	REG_S	t1,1*SZREG(to)
	REG_S	t2,2*SZREG(to)
	REG_S	t3,3*SZREG(to)
	REG_S	ta0,4*SZREG(to)
	REG_S	ta1,5*SZREG(to)
	REG_S	ta2,6*SZREG(to)
	REG_S	ta3,7*SZREG(to)
	bne	from,v0,1b
	PTR_ADDU	to,BLKSIZE	# BDSLOT, advance dst pointer

/* has at most BLKSIZE-1 bytes left */
f_aligned_halfblk:
	and	v0,count,BLKSIZE/2
	beq	v0,zero,f_aligned_quarterblk
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_ADDU	from,v0		# advance src pointer
	PTR_ADDU	to,v0		# advance dst pointer

	CACHE_BARRIER
	REG_L	t0,-4*SZREG(from)
	REG_L	t1,-3*SZREG(from)
	REG_L	t2,-2*SZREG(from)
	REG_L	t3,-1*SZREG(from)
	REG_S	t0,-4*SZREG(to)
	REG_S	t1,-3*SZREG(to)
	REG_S	t2,-2*SZREG(to)
	REG_S	t3,-1*SZREG(to)

/* has at most BLKSIZE/2-1 bytes left */
f_aligned_quarterblk:
	and	v0,count,BLKSIZE/4
	beq	v0,zero,f_aligned_long
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_ADDU	from,v0		# advance src pointer
	PTR_ADDU	to,v0		# advance dst pointer

	CACHE_BARRIER
	REG_L	t0,-2*SZREG(from)
	REG_L	t1,-1*SZREG(from)
	REG_S	t0,-2*SZREG(to)
	REG_S	t1,-1*SZREG(to)

/* has at most BLKSIZE/4-1 bytes left */
f_aligned_long:
	and	v0,count,SZREG
	beq	v0,zero,f_aligned_rest
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count

	CACHE_BARRIER
	PTR_ADDU	from,v0		# advance src pointer
	REG_L	t0,-1*SZREG(from)
	PTR_ADDU	to,v0		# advance dst pointer
	REG_S	t0,-1*SZREG(to)

/* has at most SZREG-1 bytes left */
f_aligned_rest:
	beq	count,zero,ret

	PTR_ADDU	from,count	# BDSLOT, advance src pointer
	CACHE_BARRIER
#ifdef	_MIPSEB
	REG_LRIGHT	t0,-1(from)
	PTR_ADDU	to,count	# advance dst pointer
	REG_SRIGHT	t0,-1(to)
#else	/* _MIPSEL */
	REG_LLEFT	t0,-1(from)
	PTR_ADDU	to,count	# advance dst pointer
	REG_SLEFT	t0,-1(to)
#endif	/* _MIPSEL */

	j	ra
	nop				# BDSLOT

/*
 * if we jump here then the pointers are not aligned on long boundary.
 * use sdl/sdr/ldl/ldr/swl/swr/lwl/lwr instructions instead of a pure
 * byte copy loop to save both instruction and memory access cycles.
 */
f_nonaligned_blk:
	and	v0,count,~(BLKSIZE-1)	# number of BLKSIZE chunks
	beq	v0,zero,f_nonaligned_halfblk
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_ADDU	v0,from		# terminal src pointer

/* BLKSIZE byte at a time loop */
1:	CACHE_BARRIER
	UNALIGNED_LOAD(t0,0*SZREG,from)
	UNALIGNED_LOAD(t1,1*SZREG,from)
	UNALIGNED_LOAD(t2,2*SZREG,from)
	UNALIGNED_LOAD(t3,3*SZREG,from)
	UNALIGNED_LOAD(ta0,4*SZREG,from)
	UNALIGNED_LOAD(ta1,5*SZREG,from)
	UNALIGNED_LOAD(ta2,6*SZREG,from)
	UNALIGNED_LOAD(ta3,7*SZREG,from)
	PTR_ADDU	from,BLKSIZE	# advance src pointer
	UNALIGNED_STORE(t0,0*SZREG,to)
	UNALIGNED_STORE(t1,1*SZREG,to)
	UNALIGNED_STORE(t2,2*SZREG,to)
	UNALIGNED_STORE(t3,3*SZREG,to)
	UNALIGNED_STORE(ta0,4*SZREG,to)
	UNALIGNED_STORE(ta1,5*SZREG,to)
	UNALIGNED_STORE(ta2,6*SZREG,to)
	UNALIGNED_STORE(ta3,7*SZREG,to)
	bne	from,v0,1b
	PTR_ADDU	to,BLKSIZE	# BDSLOT, advance dst pointer

/* has at most BLKSIZE-1 bytes left */
f_nonaligned_halfblk:
	and	v0,count,BLKSIZE/2
	beq	v0,zero,f_nonaligned_quarterblk
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_ADDU	from,v0		# advance src pointer
	PTR_ADDU	to,v0		# advance dst pointer

	CACHE_BARRIER
	UNALIGNED_LOAD(t0,-4*SZREG,from)
	UNALIGNED_LOAD(t1,-3*SZREG,from)
	UNALIGNED_LOAD(t2,-2*SZREG,from)
	UNALIGNED_LOAD(t3,-1*SZREG,from)
	UNALIGNED_STORE(t0,-4*SZREG,to)
	UNALIGNED_STORE(t1,-3*SZREG,to)
	UNALIGNED_STORE(t2,-2*SZREG,to)
	UNALIGNED_STORE(t3,-1*SZREG,to)

/* has at most BLKSIZE/2-1 bytes left */
f_nonaligned_quarterblk:
	and	v0,count,BLKSIZE/4
	beq	v0,zero,f_nonaligned_long
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_ADDU	from,v0		# advance src pointer
	PTR_ADDU	to,v0		# advance dst pointer

	CACHE_BARRIER
	UNALIGNED_LOAD(t0,-2*SZREG,from)
	UNALIGNED_LOAD(t1,-1*SZREG,from)
	UNALIGNED_STORE(t0,-2*SZREG,to)
	UNALIGNED_STORE(t1,-1*SZREG,to)

/* has at most BLKSIZE/4-1 bytes left */
f_nonaligned_long:
	and	v0,count,SZREG
	beq	v0,zero,f_bytecopy
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count

	CACHE_BARRIER
	PTR_ADDU	from,v0		# advance src pointer
	UNALIGNED_LOAD(t0,-1*SZREG,from)
	PTR_ADDU	to,v0		# advance dst pointer
	UNALIGNED_STORE(t0,-1*SZREG,to)

/*
 * has at most SZREG-1 bytes left.  since the pointers have random alignment
 * and the remaining bytes may span across 2 longs, sdl/sdr/ldl/ldr and
 * swl/swr/lwl/lwr cannot be used.  fallthrough to the byte copy loop
 */
/*
 * Byte at a time copy code.  This is used when the pointers are not
 * alignable, when the byte count is small, or when cleaning up any
 * remaining bytes on a larger transfer.
 */
f_bytecopy:
	beq	count,zero,ret		# If count is zero, then we are done
	PTR_ADDU	v1,from,count	# BDSLOT, final src pointer

99:	CACHE_BARRIER
	lb	v0,0(from)		# v0 = *from
	PTR_ADDU	from,1		# advance src pointer
	sb	v0,0(to)		# Store byte
	bne	from,v1,99b		# Loop until done
	PTR_ADDU	to,1		# advance dst pointer

ret:	j	ra			# return to caller
	nop				# BDSLOT
	.set	reorder

/*****************************************************************************/

/*
 * Backward copy code.  Check for pointer alignment and try to get both
 * pointers aligned on a long boundary.
 */
gobackwards:
	PTR_ADDU	from,count	# Advance to end + 1
	PTR_ADDU	to,count	# Advance to end + 1

	blt	count,MINCOPY,b_bytecopy

	/*
	 * check whether the pointers are long alignable by checking the
	 * lowest log2(SZREG) bits
	 */
	and	v0,from,SZREG-1
	and	v1,to,SZREG-1
	bne	v0,v1,b_nonaligned_blk	# low bits are different

/*
 * Pointers are long alignable, and may already be aligned.  Since v0 == v1,
 * we need only check what value v0 has to see how to get aligned.  Also,
 * since we have eliminated tiny copies, we know that the count is large
 * enough to encompass the alignment copies.
 */
	beq	v0,zero,b_aligned_blk	# v0==0 => already aligned
#ifdef	_MIPSEB
	REG_LRIGHT	v0,-1(from)	# Fetch partial long
	REG_SRIGHT	v0,-1(to)	# Store partial long
#else	/* _MIPSEL */
	REG_LLEFT	v0,-1(from)	# Fetch partial long
	REG_SLEFT	v0,-1(to)	# Store partial long
#endif	/* _MIPSEL */
	PTR_SUBU	from,v1		# advance src pointer
	PTR_SUBU	to,v1		# advance dst pointer
	LONG_SUBU	count,v1	# update remaining byte count

/*
 * Once we are here, the pointers are aligned on long boundaries
 * Begin copying in large chunks.
 */
	.set	noreorder
b_aligned_blk:
	and	v0,count,~(BLKSIZE-1)	# number of BLKSIZE chunks
	beq	v0,zero,b_aligned_halfblk
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_SUBU	v0,from,v0	# terminal src pointer

/* BLKSIZE byte at a time loop */
1:	CACHE_BARRIER
	REG_L	t0,-1*SZREG(from)
	REG_L	t1,-2*SZREG(from)
	REG_L	t2,-3*SZREG(from)
	REG_L	t3,-4*SZREG(from)
	REG_L	ta0,-5*SZREG(from)
	REG_L	ta1,-6*SZREG(from)
	REG_L	ta2,-7*SZREG(from)
	REG_L	ta3,-8*SZREG(from)
	PTR_SUBU	from,BLKSIZE	# decrement src pointer
	REG_S	t0,-1*SZREG(to)
	REG_S	t1,-2*SZREG(to)
	REG_S	t2,-3*SZREG(to)
	REG_S	t3,-4*SZREG(to)
	REG_S	ta0,-5*SZREG(to)
	REG_S	ta1,-6*SZREG(to)
	REG_S	ta2,-7*SZREG(to)
	REG_S	ta3,-8*SZREG(to)
	bne	from,v0,1b
	PTR_SUBU	to,BLKSIZE	# BDSLOT, decrement dst pointer

/* has at most BLKSIZE-1 bytes left */
b_aligned_halfblk:
	and	v0,count,BLKSIZE/2
	beq	v0,zero,b_aligned_quarterblk
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_SUBU	from,v0		# decrement src pointer
	PTR_SUBU	to,v0		# decrement dst pointer

	CACHE_BARRIER
	REG_L	t0,3*SZREG(from)
	REG_L	t1,2*SZREG(from)
	REG_L	t2,1*SZREG(from)
	REG_L	t3,0*SZREG(from)
	REG_S	t0,3*SZREG(to)
	REG_S	t1,2*SZREG(to)
	REG_S	t2,1*SZREG(to)
	REG_S	t3,0*SZREG(to)

/* has at most BLKSIZE/2-1 bytes left */
b_aligned_quarterblk:
	and	v0,count,BLKSIZE/4
	beq	v0,zero,b_aligned_long
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_SUBU	from,v0		# decrement src pointer
	PTR_SUBU	to,v0		# decrement dst pointer

	CACHE_BARRIER
	REG_L	t0,1*SZREG(from)
	REG_L	t1,0*SZREG(from)
	REG_S	t0,1*SZREG(to)
	REG_S	t1,0*SZREG(to)

/* has at most BLKSIZE/4-1 bytes left */
b_aligned_long:
	and	v0,count,SZREG
	beq	v0,zero,b_aligned_rest
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count

	CACHE_BARRIER
	PTR_SUBU	from,v0		# decrement src pointer
	REG_L	t0,0*SZREG(from)
	PTR_SUBU	to,v0		# decrement dst pointer
	REG_S	t0,0*SZREG(to)

/* has at most SZREG-1 bytes left */
b_aligned_rest:
	beq	count,zero,1f

	PTR_SUBU	from,count	# BDSLOT, final src pointer
	CACHE_BARRIER
#ifdef	_MIPSEB
	REG_LLEFT	t0,0(from)
	PTR_SUBU	to,count	# final dst pointer
	REG_SLEFT	t0,0(to)
#else	/* _MIPSEL */
	REG_LRIGHT	t0,0(from)
	PTR_SUBU	to,count	# final dst pointer
	REG_SRIGHT	t0,0(to)
#endif	/* _MIPSEL */

1:
	j	ra
	nop				# BDSLOT

/*
 * if we jump here then the pointers are not aligned on long boundary.
 * use sdl/sdr/ldl/ldr/swl/swr/lwl/lwr instructions instead of a pure
 * byte copy loop to save both instruction and memory access cycles.
 */
b_nonaligned_blk:
	and	v0,count,~(BLKSIZE-1)	# number of BLKSIZE chunks
	beq	v0,zero,b_nonaligned_halfblk
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_SUBU	v0,from,v0	# terminal src pointer

/* BLKSIZE byte at a time loop */
1:	UNALIGNED_LOAD(t0,-1*SZREG,from)
	UNALIGNED_LOAD(t1,-2*SZREG,from)
	UNALIGNED_LOAD(t2,-3*SZREG,from)
	UNALIGNED_LOAD(t3,-4*SZREG,from)
	UNALIGNED_LOAD(ta0,-5*SZREG,from)
	UNALIGNED_LOAD(ta1,-6*SZREG,from)
	UNALIGNED_LOAD(ta2,-7*SZREG,from)
	UNALIGNED_LOAD(ta3,-8*SZREG,from)
	PTR_SUBU	from,BLKSIZE	# decrement src pointer
	CACHE_BARRIER
	UNALIGNED_STORE(t0,-1*SZREG,to)
	UNALIGNED_STORE(t1,-2*SZREG,to)
	UNALIGNED_STORE(t2,-3*SZREG,to)
	UNALIGNED_STORE(t3,-4*SZREG,to)
	UNALIGNED_STORE(ta0,-5*SZREG,to)
	UNALIGNED_STORE(ta1,-6*SZREG,to)
	UNALIGNED_STORE(ta2,-7*SZREG,to)
	UNALIGNED_STORE(ta3,-8*SZREG,to)
	bne	from,v0,1b
	PTR_SUBU	to,BLKSIZE	# BDSLOT, decrement dst pointer

/* has at most BLKSIZE-1 bytes left */
b_nonaligned_halfblk:
	and	v0,count,BLKSIZE/2
	beq	v0,zero,b_nonaligned_quarterblk
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_SUBU	from,v0		# decrement src pointer
	PTR_SUBU	to,v0		# decrement dst pointer

	CACHE_BARRIER
	UNALIGNED_LOAD(t0,3*SZREG,from)
	UNALIGNED_LOAD(t1,2*SZREG,from)
	UNALIGNED_LOAD(t2,1*SZREG,from)
	UNALIGNED_LOAD(t3,0*SZREG,from)
	UNALIGNED_STORE(t0,3*SZREG,to)
	UNALIGNED_STORE(t1,2*SZREG,to)
	UNALIGNED_STORE(t2,1*SZREG,to)
	UNALIGNED_STORE(t3,0*SZREG,to)

/* has at most BLKSIZE/2-1 bytes left */
b_nonaligned_quarterblk:
	and	v0,count,BLKSIZE/4
	beq	v0,zero,b_nonaligned_long
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count
	PTR_SUBU	from,v0		# decrement src pointer
	PTR_SUBU	to,v0		# decrement dst pointer

	CACHE_BARRIER
	UNALIGNED_LOAD(t0,1*SZREG,from)
	UNALIGNED_LOAD(t1,0*SZREG,from)
	UNALIGNED_STORE(t0,1*SZREG,to)
	UNALIGNED_STORE(t1,0*SZREG,to)

/* has at most BLKSIZE/4-1 bytes left */
b_nonaligned_long:
	and	v0,count,SZREG
	beq	v0,zero,b_bytecopy
	LONG_SUBU	count,v0	# BDSLOT, update remaining byte count

	CACHE_BARRIER
	PTR_SUBU	from,v0		# decrement src pointer
	UNALIGNED_LOAD(t0,0*SZREG,from)
	PTR_SUBU	to,v0		# decrement dst pointer
	UNALIGNED_STORE(t0,0*SZREG,to)

/*
 * has at most SZREG-1 bytes left.  since the pointers have random alignment
 * and the remaining bytes may span across 2 longs, sdl/sdr/ldl/ldr and
 * swl/swr/lwl/lwr cannot be used.  fallthrough to the byte copy loop
 */
/*
 * Byte at a time copy code.  This is used when the pointers are not
 * alignable, when the byte count is small, or when cleaning up any
 * remaining bytes on a larger transfer.
 */
b_bytecopy:
	beq	count,zero,ret		# If count is zero quit
	PTR_SUBU	from,1		# Reduce by one (point at byte)
	PTR_SUBU	to,1		# Reduce by one (point at byte)
	PTR_SUBU	v1,from,count	# v1 = original from - 1

99:	lb	v0,0(from)		# v0 = *from
	CACHE_BARRIER
	PTR_SUBU	from,1		# backup pointer
	sb	v0,0(to)		# Store byte
	bne	from,v1,99b		# Loop until done
	PTR_SUBU	to,1		# BDSLOT: backup pointer

	j	ra			# return to caller
	nop				# BDSLOT
	.set	reorder
	END(bcopy)

#undef	UNALIGNED_LOAD
#undef	UNALIGNED_STORE
#undef	from
#undef	to
#undef	count

/*
 * bzero(dst, bcount)
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
 * the zero in the cache, we shade these values (18-20) down to 12
 */
#define	MINZERO		15

LEAF(bzero)
	.set	noreorder
	blt	a1,MINZERO,zero_byte
	PTR_SUBU	v1,zero,a0		# number of bytes til aligned
	and	v1,SZREG-1
	beq	v1,zero,zero_blk		# already aligned
	LONG_SUBU	a1,v1			# BDSLOT

	CACHE_BARRIER
#ifdef	_MIPSEB
	REG_SLEFT	zero,0(a0)
#else	/* _MIPSEL */
	REG_SRIGHT	zero,0(a0)
#endif	/* _MIPSEL */

	PTR_ADDU	a0,v1

/*
 * zero BLKSIZE byte, aligned block
 */
zero_blk:
	and	a3,a1,~(BLKSIZE-1)		# BLKSIZE byte chunks
	beq	a3,zero,zero_halfblk
	LONG_SUBU	a1,a3			# BDSLOT
	PTR_ADDU	a3,a0			# dst endpoint

1:	PTR_ADDU	a0,BLKSIZE
	CACHE_BARRIER
	REG_S	zero,-8*SZREG(a0)
	REG_S	zero,-7*SZREG(a0)
	REG_S	zero,-6*SZREG(a0)
	REG_S	zero,-5*SZREG(a0)
	REG_S	zero,-4*SZREG(a0)
	REG_S	zero,-3*SZREG(a0)
	REG_S	zero,-2*SZREG(a0)
	bne	a0,a3,1b
	REG_S	zero,-1*SZREG(a0)

zero_halfblk:
	and	a3,a1,BLKSIZE/2
	beq	a3,zero,zero_quarter_blk
	LONG_SUBU	a1,a3			# BDSLOT

	CACHE_BARRIER
	PTR_ADDU	a0,BLKSIZE/2
	REG_S	zero,-4*SZREG(a0)
	REG_S	zero,-3*SZREG(a0)
	REG_S	zero,-2*SZREG(a0)
	REG_S	zero,-1*SZREG(a0)


zero_quarter_blk:
	and	a3,a1,BLKSIZE/4
	beq	a3,zero,zero_long
	LONG_SUBU	a1,a3			# BDLSOT

	CACHE_BARRIER
	PTR_ADDU	a0,BLKSIZE/4
	REG_S	zero,-2*SZREG(a0)
	REG_S	zero,-1*SZREG(a0)

zero_long:
	and	a3,a1,SZREG			# longword chunks
	beq	a3,zero,zero_rest
	LONG_SUBU	a1,a3			# BDSLOT 

	CACHE_BARRIER
	PTR_ADDU	a0,SZREG
	REG_S	zero,-1*SZREG(a0)

zero_rest:
	beq	a1,zero,zerodone
	PTR_ADDU	a0,a1			# BDSLOT
	CACHE_BARRIER
#ifdef _MIPSEB
	REG_SRIGHT	zero,-1(a0)
#else	/* _MIPSEL */
	REG_SLEFT	zero,-1(a0)
#endif	/* _MIPSEL */

	j	ra
	nop

zero_byte:
	ble	a1,zero,zerodone
	PTR_ADDU	a1,a0			# dst endpoint
	
1:	PTR_ADDU	a0,1
	CACHE_BARRIER
	bne	a0,a1,1b
	sb	zero,-1(a0)			# BDSLOT
	.set	reorder

zerodone:
	j	ra
	END(bzero)

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

LEAF(bcmp)
	xor	v0,a0,a1
	blt	a2,MINCMP,bytecmp	# too short, just byte cmp
	and	v0,NBPW-1
	PTR_SUBU	t8,zero,a0		# number of bytes til aligned
	bne	v0,zero,unalgncmp	# src and dst not alignable
/*
 * src and dst can be simultaneously word aligned
 */
	and	t8,NBPW-1
	PTR_SUBU	a2,t8
	beq	t8,zero,wordcmp		# already aligned
#ifdef	_MIPSEB
	lwl	v0,0(a0)		# cmp unaligned portion
	lwr	v0,3(a0)		# WAR: the R12KS needs LWL/LWR paired
	lwl	v1,0(a1)
	lwr	v1,3(a1)		# WAR: the R12KS needs LWL/LWR paired
#else	/* _MIPSEL */
	lwr     v0,0(a0)
	lwl	v0,3(a0)		# WAR: the R12KS needs LWL/LWR paired
	lwr     v1,0(a1)
	lwl	v1,3(a1)		# WAR: the R12KS needs LWL/LWR paired
#endif	/* _MIPSEL */
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
	PTR_ADDU	a3,a0				# src1 endpoint
1:	lw	v0,0(a0)
	lw	v1,0(a1)
	PTR_ADDU	a0,NBPW				# 1st BDSLOT
	PTR_ADDU	a1,NBPW				# 2nd BDSLOT (asm doesn't move)
	bne	v0,v1,cmpne
	bne	a0,a3,1b			# at least one more word
	b	bytecmp

/*
 * deal with simultaneously unalignable cmp by aligning one src
 */
unalgncmp:
	PTR_SUBU	a3,zero,a1		# calc byte cnt to get src2 aligned
	and	a3,NBPW-1
	PTR_SUBU	a2,a3
	beq	a3,zero,partaligncmp	# already aligned
	PTR_ADDU	a3,a0			# src1 endpoint
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
#ifdef	_MIPSEB
	lwl	v0,0(a0)
	lwr	v0,3(a0)
#else	/* _MIPSEL */
	lwr     v0,0(a0)
	lwl     v0,3(a0)
#endif	/* _MIPSEL */
	lw	v1,0(a1)
	PTR_ADDU	a0,NBPW
	PTR_ADDU	a1,NBPW
	bne	v0,v1,cmpne
	bne	a0,a3,1b

/*
 * brute force byte cmp loop
 */
bytecmp:
	PTR_ADDU	a3,a2,a0			# src1 endpoint; BDSLOT
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
	li	v0,1
	j	ra
	END(bcmp)

/*
 * _cksum1(addr, len, prevcksum)
 *
 * Calculates a 16 bit ones-complement checksum.
 * Note that for a big-endian machine, this routine always adds even
 * address bytes to the high order 8 bits of the 16 bit checksum and
 * odd address bytes are added to the low order 8 bits of the 16 bit checksum.
 * For little-endian machines, this routine always adds even address bytes
 * to the low order 8 bits of the 16 bit checksum and the odd address bytes
 * to the high order 8 bits of the 16 bit checksum.
 */
LEAF(_cksum1)
	move	v0,a2		# copy previous checksum
	beq	a1,zero,4f	# count exhausted
	and	v1,a0,1
	beq	v1,zero,2f	# already on a halfword boundry
	lbu	t8,0(a0)
	PTR_ADDU	a0,1
	PTR_ADDU	v0,t8
	PTR_SUBU	a1,1
	b	2f

1:	lhu	t8,0(a0)
	PTR_ADDU	a0,2
	PTR_ADDU	v0,t8
	PTR_SUBU	a1,2
2:	bge	a1,2,1b
	beq	a1,zero,3f	# no trailing byte
	lbu	t8,0(a0)
	sll	t8,8
	PTR_ADDU	v0,t8
3:	srl	v1,v0,16	# add in all previous wrap around carries
	and	v0,0xffff
	PTR_ADDU	v0,v1
	srl	v1,v0,16	# wrap-arounds could cause carry, also
	PTR_ADDU	v0,v1
	and	v0,0xffff
4:	j	ra
	END(_cksum1)

/*
 * nuxi_s and nuxi_l -- byte swap short and long
 */
LEAF(nuxi_s)			# a0 = ??ab
	srl	v0,a0,8		# v0 = 0??a
	and	v0,0xff		# v0 = 000a
	sll	v1,a0,8		# v1 = ?ab0
	or	v0,v1		# v0 = ?aba
	and	v0,0xffff	# v0 = 00ba
	j	ra
	END(nuxi_s)

LEAF(nuxi_l)			# a0 = abcd
	sll	v0,a0,24	# v0 = d000
	srl	v1,a0,24	# v1 = 000a
	or	v0,v0,v1	# v0 = d00a
	and	v1,a0,0xff00	# v1 = 00c0
	sll	v1,v1,8		# v1 = 0c00
	or	v0,v0,v1	# v0 = dc0a
	srl	v1,a0,8		# v1 = 0abc
	and	v1,v1,0xff00	# v1 = 00b0
	or	v0,v0,v1	# v0 = dcba
	j	ra
	END(nuxi_l)

/* swapl - swap bytes within a buffer of words
 * buffer must be word aligned, or an address error will be generated
 * first arg is address, second is number of words (not bytes).
 */
LEAF(swapl)			
	.set	noreorder
	beq	zero,a1,2f
	nop
1:	CACHE_BARRIER
	lw	t0,0(a0) 	# t0 = abcd
	PTR_SUBU	a1,1
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
	PTR_ADDIU	a0,4
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
	beq	zero,a1,2f
	nop
1:	CACHE_BARRIER
	lhu	t0,0(a0) 	# t0 = 00ab
	PTR_SUBU	a1,1
	srl	v0,t0,8		# v0 = 000a
	sll	v1,t0,8		# v1 = 0ab0
	or	v0,v1		# v0 = 0aba
	sh	v0,0(a0)
	bne	zero,a1,1b
	PTR_ADDIU	a0,2
2:
	.set	reorder
	j	ra
	END(swaps)


/* 
 * define a passel of register-manipulation routines, providing C code
 *   access to generic machine registers ('generic' means registers
 *   that are common to all mips processors--r2k, r3k, and r4k).
 * XLEAF resolves a.k.a. names.
 * All set_* routines return the previous value.
 * Processor-specific register routines are defined in libsk/ml/r*kasm.s.
 * This file contains:
 *   user registers: get_pc and get_sp
 *   C0 registers: get_sr/set_sr, get_cause/set_cause, and get_prid
 *   fp registers: get_fpsr, set_fpsr
 */

LEAF(getpc)
XLEAF(get_pc)
	move	v0,ra
	j	ra
	END(getpc)

LEAF(get_sp)
XLEAF(getsp)
	move	v0,sp
	j	ra
	END(get_sp)


/* Drop into the debugger by executing a breakpoint.
 *
 * If a0 contains a pointer to the string "ring", symmon assumes
 * the breakpoint was generated by ^A on the keyboard.
 *
 * If a0 contains a pointer to the string "quiet", symmon won't
 * print the "Unexpected Breakpoint" message (e.g. when dbgstop 
 * is set).
 */
LEAF(exec_brkpt)
XLEAF(debug)
	break	BRK_KERNELBP		# drop into debug monitor
	j	ra
	END(exec_brkpt)

/*
 * Shortcut to calling debug("quiet") from assembler code
 */
LEAF(quiet_debug)
	LA	a0,quiet
	break	BRK_KERNELBP		# drop into debug monitor
	j	ra
	END(quiet_debug)

/* 
 * memcpy(to, from, size)
 * call bcopy(from, to, size)
 */
LEAF(memcpy)

	or	v0, a0, zero
	or	a0, a1, zero
	or	a1, v0, zero
	j	bcopy
	END(memcpy)
	.data
quiet:	.asciiz	"quiet"

