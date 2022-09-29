/***********************************************************************\
*       File:           cache.s                                         *
*                                                                       *
*       Code for testing, initializing, and invalidating the		*
*	primary instruction (I), primary data (D), and secondary 	*
*	data (S) caches.						*
*                                                                       *
\***********************************************************************/

#ident "$Revision: 1.24 $"

#include <asm.h>

#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/kldiag.h>

#include "cache.h"

#define	K0_BASE		0xa800000000000000

/*
 * The following patterns of data are used for cache testing. Each entry
 * Has the form:
 *
 *             +----------+----------+-------+-------------+------------+
 *             |  Tag Hi  |  Tag Lo  |  ECC  |   Reserved  |    Data    |
 *             +----------+----------+-------+-------------+------------+
 *  Byte        0        0 0        0  0    1  1          1 1          2
 *  Offset      0        3 4        7  8    1  2          5 6          3
 *
 * Tag Hi:	Pattern to test the upper 32-bits of the tag with.
 * Tag Lo:	Patterns to test lower 32-bits of tag with.
 * ECC:	 	Pattern to use in ECC register.
 * Data:	64 bits of data to test cache data fields.
 */

#define	CP_TAG		0		/* Double word access */
#define	CP_TAG_HI	0		/* Word Access, high order 32 bits */
#define	CP_TAG_LO	4		/* Word access, low order 32 bits */
#define	CP_ECC		8
#define	CP_DATA		16
#define	CP_DATA_HI	16		/* First 4 bytes for 4-byte access */
#define	CP_DATA_LO	20		/* Last 4 bytes for 4-byte access */
#define	CP_SIZE		24		/* Total size of entry */

/*
 * Macro: CACHE_PATTERN
 * Purpose: Form a cache pattern entry.
 */
#define	CACHE_PATTERN(taghi, taglo, ecc, datahi, datalo) \
	.word	taghi; .word taglo; .word ecc; .word 0; .word datahi; .word datalo

#define SCACHE_PATTERN(taghi, taglo, ecc, data) \
	CACHE_PATTERN(taghi & CTS_TAGHI_MASK, taglo & CTS_TAGLO_MASK, ecc, data, data)

#define	ICACHE_PATTERN(taghi, taglo, ecc, data) \
	CACHE_PATTERN(taghi & CTP_ICACHE_TAGHI_MASK, taglo & CTP_ICACHE_TAGLO_MASK,\
		      ecc & 1, data & 0xf, data)
#define	DCACHE_PATTERN(taghi, taglo, ecc, data) \
	CACHE_PATTERN(taghi & CTP_DCACHE_TAGHI_MASK & 0x0fffffff, taglo & CTP_DCACHE_TAGLO_MASK,\
		      ecc & 0xf, data, data)

	.data
	.align	8
scache_patternStart:
#ifndef	SABLE
#ifdef TOO_SLOW
	SCACHE_PATTERN(0xffffffff, 0xffffffff, 0xff, 0xffffffff)
#endif
	SCACHE_PATTERN(0xaaaaaaaa, 0xaaaaaaaa, 0xaa, 0xaaaaaaaa)
	SCACHE_PATTERN(0x55555555, 0x55555555, 0x55, 0x55555555)
#ifdef TOO_SLOW
	SCACHE_PATTERN(0x00000000, 0x00000000, 0x00, 0x00000000)
#endif
#endif /* SABLE */
scache_patternEnd:

icache_patternStart:
#if !defined(SABLE)
	ICACHE_PATTERN(0xffffffff, 0xffffffff, 0xff, 0xffffffff)
	ICACHE_PATTERN(0xaaaaaaaa, 0xaaaaaaaa, 0xaa, 0xaaaaaaaa)
	ICACHE_PATTERN(0x55555555, 0x55555555, 0x55, 0x55555555)
	ICACHE_PATTERN(0x00000000, 0x00000000, 0x00, 0x00000000)
#endif /* SABLE */
icache_patternEnd:

dcache_patternStart:
#ifndef SABLE
	DCACHE_PATTERN(0xffffffff, 0xffffffff, 0xff, 0xffffffff)
	DCACHE_PATTERN(0xaaaaaaaa, 0xaaaaaaaa, 0xaa, 0xaaaaaaaa)
	DCACHE_PATTERN(0x55555555, 0x55555555, 0x55, 0x55555555)
	DCACHE_PATTERN(0x00000000, 0x00000000, 0x00, 0x00000000)
#endif /* SABLE */
dcache_patternEnd:

#undef	CACHE_PATTERN
#undef	SCACHE_PATTERN
#undef	ICACHE_PATTERN
#undef	DCACHE_PATTERN

	.text
	.set	noreorder
	.set	at

/*
 * Function: cache_size_s
 * Purpose: To read and return the size of the secondary cache
 * Parameters: none
 * Returns: Cache size in bytes in v0
 * Clobbers: v0,v1
 */

LEAF(cache_size_s)
XLEAF(size_2nd_cache)
	MFC0(v1, C0_CONFIG)		# Pick of config register value
	and	v1,CONFIG_SS		# isolate cache size
	srl	v1,CONFIG_SS_SHFT
	add	v1,CONFIG_SCACHE_POW2_BASE
	li	v0,1
	j	ra
	 sll	v0,v1			# Calculate # bytes
	END(cache_size_s)

/*
 * Function: cache_size_i
 * Purpose: To read and return the size of the primary instruction cache
 * Parameters: none
 * Returns: Cache size in bytes in v0
 * Clobbers: v0,v1
 */

LEAF(cache_size_i)
	MFC0(v1, C0_CONFIG)		# Pick of config register value
	and	v1,CONFIG_IC		# isolate cache size
	srl	v1,CONFIG_IC_SHFT
	add	v1,CONFIG_PCACHE_POW2_BASE
	li	v0,1
	j	ra
	 sll	v0,v1			# Calculate # bytes
	END(cache_size_i)

/*
 * Function: cache_size_d
 * Purpose: To read and return the size of the primary data cache
 * Parameters: none
 * Returns: Cache size in bytes in v0
 * Clobbers: v0,v1
 */

LEAF(cache_size_d)
	MFC0(v1, C0_CONFIG)		# Pick up config register value
	and	v1,CONFIG_DC		# isolate cache size
	srl	v1,CONFIG_DC_SHFT
	add	v1,CONFIG_PCACHE_POW2_BASE
	li	v0,1
	j	ra
	 sll	v0,v1			# Calculate # bytes
	END(cache_size_d)

/*
 * Function:	cache_test_i
 * Purpose:	verify we can write and read the Icache.
 * Parameters:	None
 * Returns:	v0 = 0 - success
 *		   = !0- failed
 * Notes:	The data patterns 0xaaaaaaaaa and 0x55555555 are used
 * 	        to try and check for bits stuck to 0 or 1.
 */

LEAF(cache_test_i)
	/* Figure out the number of lines */

	move	t0,ra			/* save our way home */
	jal	cache_size_i
	 nop
	move	ra,t0			/* And Back! */
	divu	t0,v0,CACHE_ILINE_SIZE*2 /* # lines in t0/# ways */

	dla	v0,icache_patternStart /* Set first pattern */
1:
	/*
	 * Check if we are done at the top of the loop. This means that
	 * we can have NO test patterns if desired.
	 */
	dla	v1,icache_patternEnd
	bge	v0,v1,testIcacheData
	 nop				/* %%% */
	lw	v1,CP_TAG_HI(v0)	/* Pick up pattern */
	lw	t3,CP_TAG_LO(v0)

	/*
	 * Loop through all lines and write the TAG and Data. Note that
	 * the parity may be incorrect, but we ONLY access the tags at
	 * this point using the cache instruction which ignores parity.
	 */

	move	t1,t0			/* # lines */
	dli	t2,K0_BASE

	MTC0(v1, C0_TAGHI)
	MTC0(t3, C0_TAGLO)

2:
	ICACHE(C_IST, 0(t2))		/* Hit both ways */
	ICACHE(C_IST, 1(t2))

	sub	t1,1
	bgtz	t1,2b			/* See if more to do ... */
	 daddu	t2,CACHE_ILINE_SIZE

	/*
	 * At this point, the tags have been written. Read them
	 * back to see what happened. Note that above, we have
	 * saved expected values in v0/v1.
	 */

	move	t1,t0			/* Number of lines */
	dli	t2,K0_BASE
3:
	ICACHE(C_ILT, 0(t2))		/* one way	*/
	MFC0(a0,C0_TAGHI)
	bne	a0,v1,testIcacheFailAddrWay0
	 nop
	MFC0(a1,C0_TAGLO)
	bne	a1,t3,testIcacheFailAddrWay0
	 nop
	ICACHE(C_ILT, 1(t2))		/* or the other */
	MFC0(a0,C0_TAGHI)
	bne	a0,v1,testIcacheFailAddrWay1
	 nop
	MFC0(a1,C0_TAGLO)
	bne	a1,t3,testIcacheFailAddrWay1
	 nop

	sub	t1,1
	bgtz	t1,3b
	 daddu	t2,CACHE_ILINE_SIZE

	/*
	 * Move onto next test pattern.
	 */
	j	1b
	 dadd	v0,CP_SIZE

testIcacheData:
	/*
	 * Now test the data part of the ICACHE, t0 still contains the
	 * # of lines.
	 */
	dla	v0,icache_patternStart

1:
	/*
	 * Check if we are done at the top of the loop. This means that
	 * we can have NO test patterns if desired.
	 */
	dla	v1,icache_patternEnd
	bge	v0,v1,testIcacheDone
	 nop				/* %%% */

	lw	ta1,CP_DATA_HI(v0)	/* Pick up pattern */
	lw	ta2,CP_DATA_LO(v0)
	lw	ta3,CP_ECC(v0)		/* ECC info */

	/*
	 * Loop through all lines and write the Data. Note that
	 * the parity may be incorrect, but we ONLY access the data at
	 * this point using the cache instruction which ignores parity.
	 */

	move	t1,t0			/* # lines */
	dli	t2,K0_BASE

	MTC0(ta1, C0_TAGHI)
	MTC0(ta2, C0_TAGLO)
	MTC0(ta3, C0_ECC)

	/*
	 * Fill the line with the pattern in both "ways". t2 has the
	 * virtual address of the start of the cache line.
	 */
2:
	li	ta0,CACHE_ILINE_SIZE/4
4:
	ICACHE(C_ISD, 0(t2))
	ICACHE(C_ISD, 1(t2))

3:					/* Loop over all words in line */
	ICACHE(C_ILD, 0(t2))
	MFC0(a1, C0_TAGHI)
	MFC0(a2, C0_TAGLO)
	MFC0(a3, C0_ECC)
	bne	a1,ta1,testIcacheFailDataWay0
	 nop
	bne	a2,ta2,testIcacheFailDataWay0
	 nop
	bne	a3,ta3,testIcacheFailDataWay0
	 nop

	ICACHE(C_ILD, 1(t2))
	MFC0(a1, C0_TAGHI)
	MFC0(a2, C0_TAGLO)
	MFC0(a3, C0_ECC)
	bne	a1,ta1,testIcacheFailDataWay1
	 nop
	bne	a2,ta2,testIcacheFailDataWay1
	 nop
	bne	a3,ta3,testIcacheFailDataWay1
	 nop

	sub	ta0,1
	bgtz	ta0,4b			/* more words in line */
	 daddiu	t2,4			/* Up 4 bytes */

	sub	t1,1
	bgtz	t1,2b			/* See if more to do ... */
	 nop

	/*
	 * Move onto next test pattern.
	 */
	j	1b
	 daddu	v0,CP_SIZE

testIcacheDone:
	j	ra
	 li	v0, KLDIAG_PASSED

testIcacheFailAddrWay0:
	j	ra
	 li	v0, KLDIAG_ICACHE_ADDR_WAY0

testIcacheFailAddrWay1:
	j	ra
	 li	v0, KLDIAG_ICACHE_ADDR_WAY1

testIcacheFailDataWay0:
	j	ra
	 li	v0, KLDIAG_ICACHE_DATA_WAY0

testIcacheFailDataWay1:
	j	ra
	 li	v0, KLDIAG_ICACHE_DATA_WAY1

	END(cache_test_i)

/*
 * Function: 	cache_test_d
 * Purpose: 	Verify we can write/read the Dcache
 * Parameters: 	none
 * Returns:	v0 = 0 - success
 *		v0 !=0 = failed
 */

LEAF(cache_test_d)
	/* figure out # of lines */

	move	t0,ra			/* save way back */
	jal	cache_size_d
	 nop
	move	ra,t0			/* Restore return path */
	divu	t0,v0,CACHE_DLINE_SIZE*2 /* # lines in t0/#ways */

	dla	v0,dcache_patternStart
1:
	/*
	 * Check if we are done at the top of the loop. This means that
	 * we can have NO test patterns if desired.
	 */
	dla	v1,dcache_patternEnd
	bge	v0,v1,testDcacheData
	 nop
	lw	v1,CP_TAG_HI(v0)	/* Pick up pattern */
	lw	t3,CP_TAG_LO(v0)

	/*
	 * Loop through all lines and write the TAG and Data. Note that
	 * the parity may be incorrect, but we ONLY access the tags at
	 * this point using the cache instruction which ignores parity.
	 */

	move	t1,t0			/* # lines */
	dli	t2,K0_BASE

	MTC0(v1, C0_TAGHI)
	MTC0(t3, C0_TAGLO)
2:
	DCACHE(C_IST, 0(t2))		/* Hit both ways */
	DCACHE(C_IST, 1(t2))

	sub	t1,1
	bgtz	t1,2b			/* See if more to do ... */
	 daddu	t2,CACHE_DLINE_SIZE

	/*
	 * At this point, the tags have been written. Read them
	 * back to see what happened. Note that above, we have
	 * saved expected values in v1/t3.
	 */

	move	t1,t0			/* Number of lines */
	dli	t2,K0_BASE
3:
	DCACHE(C_ILT, 0(t2))		/* one way	*/
	MFC0(a2,C0_TAGHI)
	bne	a2,v1,testDcacheFailAddrWay0
	 nop
	MFC0(a1,C0_TAGLO)
	bne	a1,t3,testDcacheFailAddrWay0
	 nop
	DCACHE(C_ILT, 1(t2))		/* or the other */
	MFC0(a2,C0_TAGHI)
	bne	a2,v1,testDcacheFailAddrWay1
	 nop
	MFC0(a1,C0_TAGLO)
	bne	a1,t3,testDcacheFailAddrWay1
	 nop

	sub	t1,1
	bgtz	t1,3b
	 daddu	t2,CACHE_DLINE_SIZE

	/*
	 * Move onto next test pattern.
	 */
	j	1b
	 daddu	v0,CP_SIZE

testDcacheData:
	/*
	 * Now test the data part of the ICACHE, t0 still contains the
	 * # of lines.
	 */
	dla	v0,dcache_patternStart

1:
	/*
	 * Check if we are done at the top of the loop. This means that
	 * we can have NO test patterns if desired.
	 */
	dla	v1,dcache_patternEnd
	bge	v0,v1,testDcacheDone
	 nop
	lw	v1,CP_DATA_LO(v0)	/* Pick up pattern */
	lw	t3,CP_ECC(v0)

	/*
	 * Loop through all lines and write the Data. Note that
	 * the parity may be incorrect, but we ONLY access the data at
	 * this point using the cache instruction which ignores parity.
	 */

	move	a3,t0			/* # lines */
	dli	t2,K0_BASE

	DMTC0(v1, C0_TAGLO)
	DMTC0(t3, C0_ECC)
2:
	/*
	 * Fill the line with the pattern in both "ways". t2 has the
	 * virtual address of the start of the cache line.
	 */
	li	a0,CACHE_DLINE_SIZE/4

	DCACHE(C_ISD, 0(t2))
	DCACHE(C_ISD, 1(t2))

3:					/* Loop over all words in line */
	DCACHE(C_ILD, 0(t2))
	MFC0(a1,C0_ECC)
	bne	a1,t3,testDcacheFailDataWay0
	 nop
	MFC0(a2,C0_TAGLO)
	bne	a2,v1,testDcacheFailDataWay0
	 nop

	DCACHE(C_ILD, 1(t2))
	MFC0(a1,C0_ECC)
	bne	a1,t3,testDcacheFailDataWay1
	 nop
	MFC0(a2,C0_TAGLO)
	bne	a2,v1,testDcacheFailDataWay1
	 nop

	sub	a0,1
	bgtz	a0,3b			/* more words in line */
	 nop

	sub	a3,1
	bgtz	a3,2b			/* See if more to do ... */
	 daddu	t2,CACHE_DLINE_SIZE

	/*
	 * Move onto next test pattern.
	 */
	j	1b
	 daddu	v0,CP_SIZE

testDcacheDone:
	j	ra
	 li	v0, KLDIAG_PASSED

testDcacheFailAddrWay0:
	j	ra
	 li	v0, KLDIAG_DCACHE_ADDR_WAY0

testDcacheFailAddrWay1:
	j	ra
	 li	v0, KLDIAG_DCACHE_ADDR_WAY1

testDcacheFailDataWay0:
	j	ra
	 li	v0, KLDIAG_DCACHE_DATA_WAY0

testDcacheFailDataWay1:
	j	ra
	 li	v0, KLDIAG_DCACHE_DATA_WAY1

	END(cache_test_d)

/*
 * Function:	cache_inval_i
 * Purpose:	To invalidate the primary icache and leave it consistent.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses t0,t1,v0,v1
 */

LEAF(cache_inval_i)
	move	t0,ra			/* Remember way home */
	jal	cache_size_i
	 nop
	move	ra,t0			/* Back to its place */
	divu	v0,CACHE_ILINE_SIZE*2	/* # of lines/ways */

	/*
	 * For each cache line, we build a valid looking tag, except
	 * that the state indicates the line is invalid. As it turns out,
	 * parity fields are even, and "0" is an invalid line.
	 */

	MTC0(zero, C0_TAGLO)
	MTC0(zero, C0_TAGHI)		/* Tag registers */
	DMTC0(zero, C0_ECC)		/* 0's - even parity */

	/* first the tags */

	move	t0,zero
	dli	t1,K0_BASE
1:
	ICACHE(C_IST, 0(t1))		/* way 0 */
	ICACHE(C_IST, 1(t1))		/* way 1 */
	daddu	t1, CACHE_ILINE_SIZE
	add	t0,1
	ble	t0,v0,1b
	 nop

	/*
	 * Now for safety, the data - even parity, so taglo/hi are
	 * still ok.
	 */

	dli	t1,K0_BASE
	dli	t0,CACHE_ILINE_SIZE
	mult	v0,t0			/* Total # bytes per way */
	mflo	v0
1:
	ICACHE(C_ISD, 0(t1))
	ICACHE(C_ISD, 1(t1))
	sub	v0,4			/* count -= word size */
	daddu	t1,4
	bgtz	v0,1b
	 nop

	j	ra
	 nop
	END(cache_inval_i)

/*
 * Function:	cache_inval_d
 * Purpose:	To invalidate the primary dcache and leave it consistent.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses v0,v1 t0/t1 registers.
 */

LEAF(cache_inval_d)
	move	t0,ra			/* Remember way home */
	jal	cache_size_d
	nop
	 move	ra,t0			/* Back to its place */
	divu	v0,CACHE_DLINE_SIZE*2	/* # of lines/ways */

	/*
	 * For each cache line, we build a valid looking tag, except
	 * that the state indicates the line is invalid. As it turns out,
	 * parity fields are even, and "0" is an invalid line - except
	 * for the statemod field, which must be "NORMAL" or 1.
	 */

	dli	t0,(CTP_STATEMOD_N << CTP_STATEMOD_SHFT) >> 32

	MTC0(zero, C0_TAGLO)
	MTC0(t0, C0_TAGHI)		/* Tag registers */
	DMTC0(zero, C0_ECC)		/* 0's - even parity */

	/* first the tags */

	move	t0,zero
	dli	t1,K0_BASE
1:
	DCACHE(C_IST, 0(t1))		/* way 0 */
	DCACHE(C_IST, 1(t1))		/* way 1 */
	daddu	t1, CACHE_DLINE_SIZE
	add	t0,1
	ble	t0,v0,1b
	 nop

	/*
	 *  Now for safety, the data. TAGLO and ECC used, but even
	 *  parity so 0's works out well.
	 */

	dli	t1,K0_BASE
	dli	t0,CACHE_DLINE_SIZE
	mult	v0,t0			/* total bytes per way */
	mflo	v0
1:
	DCACHE(C_ISD, 0(t1))
	DCACHE(C_ISD, 1(t1))
	sub	v0,4			/* count -= word size */
	daddu	t1,4
	bge	v0,zero,1b
	 nop

	j	ra
	 nop
	END(cache_inval_d)

/*
 * Function:	cache_inval_s
 * Purpose:	To invalidate the secondary cache, both duplicate and T5 tags.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses v0,v1,t0
 */

LEAF(cache_inval_s)
	move	t0,ra				/* Save return address */
	jal	cache_size_s			/* go get cache size */
	 nop
	move	ra,t0				/* Back to normal */

	ddivu	t0,v0,CACHE_SLINE_SIZE*2	/* # lines / way */
	/*
	 * Set T5 cache tag in TAGLO/TAGHI registers.
	 */
	MTC0(zero, C0_TAGLO)
	MTC0(zero, C0_TAGHI)
	MTC0(zero, C0_ECC)
	/*
	 * For each cache line, store an invalid tag with valid ECC,
	 */
	dli	v0,K0_BASE
1:
	SCACHE(C_IST, 0(v0))			/* T5:	way 0 */
	SCACHE(C_IST, 1(v0))			/* T5:	way 1 */
	SCACHE(C_ISD, 0x0(v0))
	SCACHE(C_ISD, 0x1(v0))
	SCACHE(C_ISD, 0x10(v0))
	SCACHE(C_ISD, 0x11(v0))
	SCACHE(C_ISD, 0x20(v0))
	SCACHE(C_ISD, 0x21(v0))
	SCACHE(C_ISD, 0x30(v0))
	SCACHE(C_ISD, 0x31(v0))
	SCACHE(C_ISD, 0x40(v0))
	SCACHE(C_ISD, 0x41(v0))
	SCACHE(C_ISD, 0x50(v0))
	SCACHE(C_ISD, 0x51(v0))
	SCACHE(C_ISD, 0x60(v0))
	SCACHE(C_ISD, 0x61(v0))
	SCACHE(C_ISD, 0x70(v0))
	SCACHE(C_ISD, 0x71(v0))

	daddiu	v0,CACHE_SLINE_SIZE
	sub	t0,1
	bgez	t0,1b
	 nop

	j	ra
	 nop
	END(cache_inval_s)

/*
 * Function:	cache_test_s
 * Purpose:	To test the secondary cache, without writing back to
 *		main memory.
 * Parameters:	none
 * Returns: v0 - 0 OK, !0 failed
 */

LEAF(cache_test_s)
	move 	a5,a0				/* save the cregs address */
	move	t0,ra				/* save ra */
	jal	cache_size_s			/* scache size */
	 nop
	move	ra,t0				/* restore ra */
	divu	a4,v0,CACHE_SLINE_SIZE*2	/* 2-way remember */

	/*
	 * check end of pattern at top of loop.
	 * allows for test to be skipped.
	 */
	dla	v0,scache_patternStart
1:
	dla	v1,scache_patternEnd
	bge	v0,v1,testScacheTagAddr
	 nop

	/*
	 * First test all of the cache tags.
	 * write way0 and inverted pattern to way1
	 * Must be done in order to flush traces, in
	 * case an addr/data line is open on the SRAM.
	 */
	dli	v1,K0_BASE
	move	a3,a4				/* # pairs of lines */
	lw	t1,CP_TAG_LO(v0)
	lw	t2,CP_TAG_HI(v0)
	li	t3,CTS_TAGLO_MASK		/* tagLo mask */
	li	t0,CTS_TAGHI_MASK & 0xffff	/* tagHi mask, not MRU */
2:
	xor	t1,t3
	xor	t2,t0
	MTC0(t1, C0_TAGLO)
	MTC0(t2, C0_TAGHI)			/* top 32 bits */
	SCACHE(C_IST, 0(v1))			/* store tag way 0 */

	xor	t1,t3
	xor	t2,t0
	MTC0(t1, C0_TAGLO)
	MTC0(t2, C0_TAGHI)			/* top 32 bits */
	SCACHE(C_IST, 1(v1))			/* store tag way 1 */

	subu	a3,1
	bgtz	a3,2b
	 daddiu	v1,CACHE_SLINE_SIZE		/* next lines */

	/*
	 * read back all tags
	 * Must be done in order to flush traces, in
	 * case an addr/data line is open on the SRAM.
	 */
	dli	v1,K0_BASE
	move	a3,a4				/* # pairs of lines */
2:
	xor	t1,t3
	xor	t2,t0
	SCACHE(C_ILT, 0(v1))			/* load tag way 0 */
	MFC0(a1, C0_TAGLO)
	MFC0(a2, C0_TAGHI)
	bne	t1,a1,testScacheFailedTagWay0
	 nop
	bne	t2,a2,testScacheFailedTagWay0
	 nop

	xor	t1,t3
	xor	t2,t0
	SCACHE(C_ILT, 1(v1))			/* load tag way 1 */
	MFC0(a1, C0_TAGLO)
	MFC0(a2, C0_TAGHI)
	bne	t1,a1,testScacheFailedTagWay1
	 nop
	bne	t2,a2,testScacheFailedTagWay1
	 nop

	subu	a3,1				/* valid size for debug */
	bgtz	a3,2b
	 daddiu	v1,CACHE_SLINE_SIZE		/* next lines */
	b	1b
	 daddiu	v0,CP_SIZE			/* next test pattern */

	/*
	 * now test tag with addr pattern
	 * write way0 and different pattern to way1
	 * Must be done in order to flush traces, in
	 * case an addr/data line is open on the SRAM.
	 * This will find aliasing effects.
	 */
testScacheTagAddr:
	dli	v1,K0_BASE
	move	a3,a4				/* # pairs of lines */
	move	t1,zero
	move	t2,zero
2:
	and	t1,t3				/* prevent tagLo ECC overflow */
	MTC0(t1, C0_TAGLO)
	MTC0(t2, C0_TAGHI)			/* top 32 bits */
	SCACHE(C_IST, 0(v1))			/* store tag way 0 */
	addiu	t1,(1 << CTS_TAG_SHFT) + 1	/* next tagLo value and ECC */
	addiu	t2,1				/* next tagHi value */

	MTC0(t1, C0_TAGLO)
	MTC0(t2, C0_TAGHI)			/* top 32 bits */
	SCACHE(C_IST, 1(v1))			/* store tag way 1 */
	addiu	t1,(1 << CTS_TAG_SHFT) + 1	/* next tagLo value and ECC */
	addiu	t2,1				/* next tagHi value */

	subu	a3,1
	bgtz	a3,2b
	 daddiu	v1,CACHE_SLINE_SIZE		/* next lines */

	/*
	 * read back all tags and check
	 * Must be done in order to flush traces, in
	 * case an addr/data line is open on the SRAM.
	 */
	dli	v1,K0_BASE
	move	a3,a4				/* # pairs of lines */
	move	t1,zero
	move	t2,zero
2:
	and	t1,t3				/* compare only valid bits */
	and	t2,t0
	SCACHE(C_ILT, 0(v1))			/* load tag way 0 */
	MFC0(a1, C0_TAGLO)
	MFC0(a2, C0_TAGHI)
	and	a1,t3
	and	a2,t0
	bne	t1,a1,testScacheFailedTagWay0
	 nop
	bne	t2,a2,testScacheFailedTagWay0
	 nop
	addiu	t1,(1 << CTS_TAG_SHFT) + 1	/* next tagLo value and ECC */
	addiu	t2,1				/* next tagHi value */

	SCACHE(C_ILT, 1(v1))			/* load tag way 1 */
	MFC0(a1, C0_TAGLO)
	MFC0(a2, C0_TAGHI)
	and	a1,t3
	and	a2,t0
	bne	t1,a1,testScacheFailedTagWay1
	 nop
	bne	t2,a2,testScacheFailedTagWay1
	 nop
	addiu	t1,(1 << CTS_TAG_SHFT) + 1	/* next tagLo value and ECC */
	addiu	t2,1				/* next tagHi value */

	subu	a3,1
	bgtz	a3,2b
	 daddiu	v1,CACHE_SLINE_SIZE		/* next lines */

	/*
	 * The INDEX STORE_DATA for the secondary cache only stores
	 * 2 of the 4 words in each quad word. Thus, for each quad
	 * word, we must first test the first 2 words, then the second
	 * 2. We double check the ECC at both phases - just for fun.
	 * v1 is really a boolean value that indicates if we are on the
	 * upper 8 bytes or lower eight bytes.
	 */
testScacheData:
	dla	v0,scache_patternStart
1:
	dla	v1,scache_patternEnd
	bge	v0,v1,testScacheAddrs
	 nop
	dli	v1,8				/* # bytes left in quad */
2:
	dli	t3,K0_BASE
	daddu	t3,v1				/* Which quad word */

3:
	lw	a3,CP_ECC(v0)			/* ECC bits */
	DMTC0(a3, C0_ECC)
	lw	a1, CP_DATA_LO(v0)
	lw	a2, CP_DATA_HI(v0)
	MTC0(a1,C0_TAGLO)
	MTC0(a2,C0_TAGHI)

	/*
	 * Compute # of stores to execute. Each secondary cache line
	 * is made up of some number of quadwords. For the first pass,
	 * write the first 8-bytes of the quadwords for the entire
	 * cache, and in the second pass write the last 8-bytes of
	 * each quadword. # Quadwords is (#lines x #quads per line)
	 */

	li	a0,CACHE_SLINE_SIZE/CACHE_SLINE_SUBSIZE
	multu	a0,a4				/* total sublines per way */
	mflo	a0
4:
	SCACHE(C_ISD, 0(t3))			/* Way 0 */
	SCACHE(C_ISD, 1(t3))			/* Way 1 */
	SCACHE(C_ISD, CACHE_SLINE_SUBSIZE+0(t3)) /* Unroll */
	SCACHE(C_ISD, CACHE_SLINE_SUBSIZE+1(t3)) /* Unroll */
	subu	a0, 2
	bgtz	a0,4b
	 daddiu	t3,2*CACHE_SLINE_SUBSIZE	/* Next subline */

	/*
	 * Now read back and verify the results with what we wrote. At
	 * this point the expected values are:
	 * ECC - a3, taghi - a2, taglo - a1
	 */
	dli	t3,K0_BASE
	daddu	t3,v1
	li	a0,CACHE_SLINE_SIZE/CACHE_SLINE_SUBSIZE
	multu	a0,a4				/* Total sublines */
	mflo	a0
4:
	SCACHE(C_ILD, 0(t3))			/* Way 0 */
	MFC0(t0, C0_ECC)
	MFC0(t1, C0_TAGHI)
	MFC0(t2, C0_TAGLO)
	bne	t0,a3,testScacheFailedWay0
	 nop
	bne	t1,a2,testScacheFailedWay0
	 nop
	bne	t2,a1,testScacheFailedWay0
	 nop
	SCACHE(C_ILD, 1(t3))			/* Way 1 */
	MFC0(t0, C0_ECC)
	MFC0(t1, C0_TAGHI)
	MFC0(t2, C0_TAGLO)
	bne	t0,a3,testScacheFailedWay1
	 nop
	bne	t1,a2,testScacheFailedWay1
	 nop
	bne	t2,a1,testScacheFailedWay1
	 nop

	SCACHE(C_ILD, CACHE_SLINE_SUBSIZE+0(t3)) /* Unroll */
	MFC0(t0, C0_ECC)
	MFC0(t1, C0_TAGHI)
	MFC0(t2, C0_TAGLO)
	bne	t0,a3,testScacheFailedWay0
	 nop
	bne	t1,a2,testScacheFailedWay0
	 nop
	bne	t2,a1,testScacheFailedWay0
	 nop
	SCACHE(C_ILD, CACHE_SLINE_SUBSIZE+1(t3)) /* Unroll */
	MFC0(t0, C0_ECC)
	MFC0(t1, C0_TAGHI)
	MFC0(t2, C0_TAGLO)
	bne	t0,a3,testScacheFailedWay1
	 nop
	bne	t1,a2,testScacheFailedWay1
	 nop
	bne	t2,a1,testScacheFailedWay1
	 subu	a0, 2				/* Note: special BDSLOT */

	bgtz	a0,4b
	 daddiu	t3,2*CACHE_SLINE_SUBSIZE

	bnez	v1,2b
	 move	v1,zero

	/*
	 * End of outer loop.
	 */

	b	1b
	 daddiu	v0,CP_SIZE			/* Next pattern */

	/*
	 * Do address test, writing then reading back the address used to
	 * access the array. This should catch the wrong cache size being
	 * set.
	 */
testScacheAddrs:
	dli	v1,8				/* # bytes left in quad */
2:
	dli	t3,K0_BASE
	daddu	t3,v1				/* Which quad word */
3:
	DMTC0(zero, C0_ECC)			/* Don't care about this guy*/
	/*
	 * compute # of stores to execute.
	 */
	li	a0,CACHE_SLINE_SIZE/CACHE_SLINE_SUBSIZE
	multu	a0,a4				/* total sublines per way */
	mflo	a0

4:
	MTC0(t3,C0_TAGLO)
	daddiu	a2,t3,4
	MTC0(a2,C0_TAGHI)
	SCACHE(C_ISD, 0(t3))			/* Way 0 */
	SCACHE(C_ISD, 1(t3))			/* Way 1 */
	subu	a0, 1
	bgtz	a0,4b
	 daddiu	t3,CACHE_SLINE_SUBSIZE		/* Next subline */

	/*
	 * Now read back and verify the results with what we wrote.
	 */
	dli	t3,K0_BASE
	daddu	t3,v1
	li	a0,CACHE_SLINE_SIZE/CACHE_SLINE_SUBSIZE
	multu	a0,a4				/* Total sublines */
	mflo	a0
	move	t0,zero
	move	a3,zero
	dli	v0,0xffffffff
4:
	and	a1,t3,v0
	daddiu	a2,t3,4
	and	a2,v0

	SCACHE(C_ILD, 0(t3))			/* Way 0 */
	MFC0(t1, C0_TAGHI)
	MFC0(t2, C0_TAGLO)
	bne	t1,a2,testScacheFailedWay0
	 nop
	bne	t2,a1,testScacheFailedWay0
	 nop
	SCACHE(C_ILD, 1(t3))			/* Way 1 */
	MFC0(t1, C0_TAGHI)
	MFC0(t2, C0_TAGLO)
	bne	t1,a2,testScacheFailedWay1
	 nop
	bne	t2,a1,testScacheFailedWay1
	 subu	a0, 1				/* Note: special BDSLOT */

	bgtz	a0,4b
	 daddiu	t3,CACHE_SLINE_SUBSIZE

	bnez	v1,2b
	 move	v1,zero				/* DELAY */

testScacheSuccess:
	j	ra
	 li	v0, KLDIAG_PASSED

testScacheFailedWay0:
	CACHE_TEST_GPR_SAVE			/* save the registers */
	j	ra
	 li	v0, KLDIAG_SCACHE_DATA_WAY0

testScacheFailedWay1:
	CACHE_TEST_GPR_SAVE			/* save the registers */
	j	ra
	 li	v0, KLDIAG_SCACHE_DATA_WAY1

testScacheFailedTagWay0:
	CACHE_TEST_GPR_SAVE			/* save the registers */
	j	ra
	 li	v0, KLDIAG_SCACHE_TAG_WAY0

testScacheFailedTagWay1:
	CACHE_TEST_GPR_SAVE			/* save the registers */
	j	ra
	 li	v0, KLDIAG_SCACHE_TAG_WAY1

	END(cache_test_s)

/*
 * Function:	cache_flush
 * Purpose:	To force writeback to memory of any dirty data in the primary
 *		data cache or secondary cache, and invalidate both caches.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses v0,v1,t0
 */

LEAF(cache_flush)
	move	t0,ra				/* Save return address */
	jal	cache_size_s			/* go get cache size */
	 nop
	move	ra,t0				/* Back to normal */

	ddivu	t0,v0,CACHE_SLINE_SIZE*2	/* # lines / way */
	/*
	 * Set T5 cache tag in TAGLO/TAGHI registers.
	 */
	MTC0(zero, C0_TAGLO)
	MTC0(zero, C0_TAGHI)
	MTC0(zero, C0_ECC)
	/*
	 * For each cache line, store an invalid tag with valid ECC,
	 */
	dli	v0,K0_BASE
1:
	SCACHE(C_IWBINV, 0(v0))			/* T5:	way 0 */
	SCACHE(C_IWBINV, 1(v0))			/* T5:	way 1 */
	sub	t0,1
	bgez	t0,1b
	 daddiu	v0,CACHE_SLINE_SIZE

	j	ra
	 nop
	END(cache_flush)

/*
 * Function:	cache_unflush(ulong base, ulong size)
 * Purpose:	References a range of memory so it's all in the cache.
 * Parameters:	a0=base, a1=size in bytes (double-word alignment)
 * Returns:	nothing
 * Notes:	uses v0,v1,t0
 */

LEAF(cache_unflush)
1:
	beqz	a1, 1f
	 nop
	ld	v0, 0(a0)
	daddu	a0, 8
	b	1b
	 dsubu	a1, 8
1:
	j	ra
	 nop
	END(cache_unflush)

/*
 * Function:	cache_dirty(a0=vaddr, a1=paddr, a2=size)
 * Purpose:	Set up the primary data cache to have valid, dirty,
 *		exclusive writable cache lines starting at vaddr/paddr,
 *		for a length equal to size bytes (up to full D-cache size).
 * Note:	This data must never be written back to memory.
 *		Cached memory should never be addressed outside the
 *		resulting virtual address range.  Call cache_inval_d
 *		to clean up when done.
 */

LEAF(cache_dirty)
	move	a3, ra				/* Save RA */
	move	t3, a0				/* Stack vaddr */

	divu	a2, CACHE_DLINE_SIZE		/* Convert size to lines */

	/*
	 * Build tag for virtual address in t3, physical address -1.
	 * State, SC way, State Parity, LRU are constant. We use:
	 *
	 *	State Mod	= 1 <normal>
	 *	State		= dirty, excl, inconsistent <CTP_STATE_DEI>
	 *	State Parity	= EVEN_PARITY(state) = <constant>
	 *	LRU 		= 0
	 *	SC way		= 0
	 *
	 *	TAG[39:36]	= <variable>
	 *	TAG[35:12]	= <variable>
	 *	Tag Parity	= <variable>
	 */

	dli	a0, CTP_STATE_DE
	jal	parity				/* Compute state parity */
	 nop

	dsll	v0, CTP_STATEPARITY_SHFT
	dli	t1, (CTP_STATE_DE << CTP_STATE_SHFT) + \
		    (CTP_STATEMOD_N << CTP_STATEMOD_SHFT)
	or	t1, v0				/* State with parity */

	/*
	 * t1 holds the constant parts of the tag, with parity.
	 * The actual TAG address must now be computed and loaded.
	 * Initialize as much of the first way as needed.
	 */

	jal	cache_size_d
	 nop
	divu	t2, v0, CACHE_DLINE_SIZE * 2	/* Cache lines per way */

	bgt	a2, t2, 1f			/* Limit to size */
	 nop
	move	t2, a2
1:
	sub	a2, t2				/* Reduce lines remaining */

1:
	blez	t2, 3f
	 nop

	dsrl	t0, a1, 4
	and	t0, CTP_TAG_MASK		/* Physical address */
	jal	parity
	 move	a0, t0
	dsll	v0, CTP_TAGPARITY_SHFT
	or	t0, v0				/* Tag with tag parity */
	or	t0, t1				/* Tag complete with parity */

	/* Now set the dcache entry for the specified line */

	MTC0(t0, C0_TAGLO)			/* Low order 32 bits */
	dsrl	t0, 32
	MTC0(t0, C0_TAGHI)			/* High orger 32 bits */
	DCACHE(C_IST, 0(t3))			/* Set tag way 0 */

	/*
	 * Must be sure data in cache line does not have a parity error.
	 */

	DMTC0(zero, C0_ECC)			/* Even parity, ECC=0  */
	MTC0(zero, C0_TAGLO)			/* 0 data */

	daddiu	a4, t3, CACHE_DLINE_SIZE	/* end point */
2:
	DCACHE(C_ISD, 0(t3))			/* Store 0 in way 0 */
	daddiu	t3, 4				/* next word */
	bltu	t3, a4, 2b
	 nop

	daddiu	a1, CACHE_DLINE_SIZE		/* Bump physical address */

	b	1b
	 sub	t2, 1

3:

	/*
	 * Initialize the second way (if more space is needed)
	 */

	move	t2, a2				/* Remaining lines */

1:
	blez	t2, 3f
	 nop

	dsrl	t0, a1, 4
	and	t0, CTP_TAG_MASK		/* Physical address */
	jal	parity
	 move	a0, t0
	dsll	v0, CTP_TAGPARITY_SHFT
	or	t0, v0				/* Tag with tag parity */
	or	t0, t1				/* Tag complete with parity */

	/* Now set the dcache entry for the specified line */

	MTC0(t0, C0_TAGLO)			/* Low order 32 bits */
	dsrl	t0, 32
	MTC0(t0, C0_TAGHI)			/* High orger 32 bits */
	DCACHE(C_IST, 1(t3))			/* Set tag way 1 */

	/*
	 * Must be sure data in cache line does not have a parity error.
	 */

	DMTC0(zero, C0_ECC)			/* Even parity, ECC=0  */
	MTC0(zero, C0_TAGLO)			/* 0 data */

	daddiu	a4, t3, CACHE_DLINE_SIZE	/* end point */
2:
	DCACHE(C_ISD, 1(t3))			/* Store 0 in way 1 */
	daddiu	t3, 4				/* next word */
	bltu	t3, a4, 2b
	 nop

	daddiu	a1, CACHE_DLINE_SIZE		/* Bump physical address */

	b	1b
	 sub	t2, 1

3:

	j	a3				/* sp is ready */
	 nop

	END(cache_dirty)

/*
 * Function:	cache_get_i
 * Purpose:	Fetch an instruction line from the icache.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - ptr to buffer for data NULL indicates tag only
 * Returns:	v0 - tag
 *		a1 - if non-null, points to buffer.
 */

LEAF(cache_get_i)
	move	a3,a0
	/*
	 * If a1 is non-null, read the data.
	 */
	beqz	a1,2f
	 nop

	daddiu	a2,a0,CACHE_ILINE_SIZE
1:
	ICACHE(C_ILD, 0(a0))
	MFC0(v0, C0_TAGLO)
	sw	v0,0(a1)
	daddiu	a0,4			/* Next word - vaddr */
	daddiu	a1,4			/* Next word - buffer */
	blt	a0,a2,1b
	 nop
2:
	ICACHE(C_ILT, 0(a3))
	MFC0(v0, C0_TAGHI)
	MFC0(v1, C0_TAGLO)
	dsll	v0,32
	j	ra
	 or	v0,v1
	END(cache_get_i)

/*
 * Function:	cache_get_d
 * Purpose:	Fetch primary data cache line.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - ptr to buffer for data NULL indicates tag only
 * Returns:	v0 - tag
 *		a1 - if non-null, points to buffer.
 */

LEAF(cache_get_d)
	move	a3,a0
	/*
	 * If a1 is NULL, skip the reading of the data.
	 */
	beqz	a1,2f
	 nop

	move	a2,a0
	daddiu	a2,CACHE_DLINE_SIZE

1:
	DCACHE(C_ILD, 0(a0))
	MFC0(v0, C0_TAGLO)
	sw	v0,0(a1)
	daddiu	a0,4			/* Next word - vaddr */
	daddiu	a1,4			/* Next word - buffer */
	blt	a0,a2,1b
	 nop
2:
	/* Now the tag */
	DCACHE(C_ILT, 0(a3))
	MFC0(v0, C0_TAGHI)
	MFC0(v1, C0_TAGLO)
	dsll	v0,32
	j	ra
	 or	v0,v1

	END(cache_get_d)

/*
 * Function:	cache_get_s
 * Purpose:	Fetch a cache line from the secondary cache.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - pointer to T5 tags area
 *		a2 - pointer to Duplicate tags area
 *		a3 - ptr to buffer for data
 *		     NULL indicates tag only
 * Returns:	nothing
 *		a1 - if non-null, points to buffer.
 */

LEAF(cache_get_s)
	beqz	a1,1f			/* No T5 Tag */
	 nop

	/* Store T5 Tag */

	SCACHE(C_ILT, 0(a0))
	MFC0(v0, C0_TAGHI)
	MFC0(v1, C0_TAGLO)
	dsll	v0, 32
	and	v1,0x00000000ffffffff
	or	v0,v1
	sd	v0, 0(a1)

1:
	beqz	a2, 1f			/* No Duplicate Tag */
	 nop

	move	t0,ra
	jal	cache_size_s
	 nop
	move	ra,t0

	srl	v0,1			/* div by 2 for # ways */
	sub	v0,1			/* cache_size_s/2 - 1 */
	and	v0,a0,v0		/* V0 - normalizes virtual address */

	divu	v0,CACHE_SLINE_SIZE	/* Compute index */
	sll	v0,4			/* Index into duplicate tags */
	and	v1,a0,1			/* Look at way */
	sll	v1,3			/* add 8-bytes for way 1 */
	daddu	v1,v0			/* Offset into TAGS */
#if 0
/* XXX fixme XXX */
	dli	a1,EV_BTRAM_BASE	/* tag ram base */
	daddu	a1,v1			/* address of TAG */
	ld	v0,0(a1)		/* duplicate tag */
	sd	v0,0(a2)		/* and store for the caller */
#endif

1:
	beqz	a3,1f			/* No data required */
	 nop

	/* OK - lets get the data */

	li	v0,CACHE_SLINE_SIZE/8	/* # fetches */

2:
	SCACHE(C_ILD, 0(a0))
	MFC0(v1, C0_TAGHI)
	sw	v1,(a3)
	MFC0(v1, C0_TAGLO)
	sw	v1,4(a3)
	sub	v0,1
	daddu	a3,8
	daddu	a0,8
	bgez	v0,2b
	 nop
1:
	/* All done... */
	j	ra
	 nop
	END(cache_get_s)

/*
 * Function:	cache_tag_ecc
 * Purpose:	Compute the secondary cache tag ECC value given
 *		the tag (without the ECC).
 * Parameters:	a0 - The 26-bit cache tag value.
 * Returns:	v0 - ECC value.
 *
 * Notes: The following table is from the "R10000 User's Manual", in the
 *	  section on secondary cache error protection handling. It indicates
 *	  the bits selected to look at for each of the check bits.
 */

	.data

tagECC:
	.dword	0x00a8f888
	.dword	0x0114ff04
	.dword	0x02620f42
	.dword	0x029184f0
	.dword	0x010a40ff
	.dword	0x0245222f
	.dword	0x01ff1111
tagECC_end:

	.text

LEAF(cache_tag_ecc)
	move	v0,zero			/* Start out with 0 result */
	dla	a1,tagECC		/* Current bit selector pointer */

cte_loop:				/* Loop for all bits set */

	dla	a2, tagECC_end
	beq	a1,a2,cte_done
	 nop
	move	v1,zero			/* Result is always 0 to start */
	lwu	a2, 0(a1)
	and	a2,a0			/* Select the bits we want */
1:
	beqz	a2,2f			/* Done with this one */
	 nop
	xor	v1,a2
	srl	a2,1
	b	1b
	 nop
2:
	/*
         * Shift the running result, and or the new bit into the bottom
         * position. This means we actually do bit 6 first, and bit 0 last.
         */
	and	v1, 0x1			/* Pick up bit */
	sll	v0,1			/* Shift into position */
	or	v0,v1			/* And put into result */
	daddiu	a1, 4			/* Next check bit please...  */
	b	cte_loop		/* Next one */
	 nop

cte_done:				/* All done... */
	j	ra
	 nop

	END(cache_tag_ecc)

#if 0
/*
 * Function:	cache_copy_pic
 * Purpose:	To copy position-independent code to the cache.
 * Parameters:	a0 - pointer to address to copy
 *		a1 - length (# of bytes to copy - must be multiple of 4.
 *		a2 - Virtual address in D-cache for copied code
 *		a3 - Physical address in D-cache for copied code
 * Returns:	v0 - virtual address corresponding to first instruction
 *		     as pointed to by a0.
 * Notes:	Build texts in primary dcache, then flushes to secondary
 *		cache.
 */
LEAF(cache_copy_pic)
	move	s2, a2
	move	s3, a3

	move	a3,ra			/* save RA */
	move	t0,a0

	daddiu	t1, a1, CACHE_DLINE_SIZE-1
	and	t1, ~(CACHE_DLINE_SIZE - 1)	/* Mutiple of d-lines */
	divu	t3, t1, CACHE_DLINE_SIZE	/* # primary d-lines. */

	dli	a0, CTP_STATE_DE
	jal	parity
	 nop

	/* Build constant part of dtag into t2 */

	dsll	v0,CTP_STATEPARITY_SHFT
	dli	t2,(CTP_STATE_DE<<CTP_STATE_SHFT) + \
		   (CTP_STATEMOD_N<<CTP_STATEMOD_SHFT)
	or	t2,v0				/* State with parity */

	/*
	 * Loop through the primary d-cache lines marking the tags.
	 */
	move	ta2,t3
	move	t8, s2				/* Start of virt address */
	move	t9, s3				/* Start of phys address */
1:
	dsrl	ta3, t9, 4
	and	ta3, CTP_TAG_MASK		/* Physical address portion */
	move	a0, ta3
	jal	parity
	 nop
	dsll	v0,CTP_TAGPARITY_SHFT
	or	ta3,v0
	or	ta3,t2				/* Tag - with parity */

	MTC0(ta3, C0_TAGLO)
	dsrl	ta3,32
	MTC0(ta3, C0_TAGHI)
	DCACHE(C_IST, 0(t8))

	daddiu	t8, CACHE_DLINE_SIZE
	daddiu	t9, CACHE_DLINE_SIZE
	sub	ta2, 1
	bgtz	ta2, 1b
	 nop

	/*
	 * Now compute and store the secondary cache tags - first round
	 * up # bytes to mutiple of secondary cache lines.
	 */

	daddiu	ta2, t1, CACHE_SLINE_SIZE - 1
	and	ta2, ~(CACHE_SLINE_SIZE - 1)	/* # secondary bytes */

	move	t8, s2				/* Start of virt address */
	move	t9, s3				/* Start of phys address */

	/*
	 * Set the physical address bits - the secondary cache state
	 * is marked XXX to allow a cache HIT in the secondary cache when
	 * we flush the primary cache.
	 */
1:
	dsrl	ta3, t9, 4
	and	ta3, CTS_TAG_MASK		/* Physical address bits */

	or	ta3, 3 << CTS_STATE_SHFT
	or	ta3, /* VIDX %%% */ 0

	jal	cache_tag_ecc
	 move	a0, ta3
	or	ta3,v0				/* Or in ECC */

	/* TAG is now in ta3 with ECC set - write it and continue. */

	MTC0(ta3, C0_TAGLO)
	dsrl	ta3, 32
	MTC0(ta3, C0_TAGHI)
	SCACHE(C_IST, 0(t8))
	MTC0(zero, C0_TAGLO)			/* Clear other way */
	MTC0(zero, C0_ECC)
	SCACHE(C_IST, 1(t8))

	daddiu	t9, CACHE_SLINE_SIZE		/* Increment physical addr */
	daddiu	t8, CACHE_SLINE_SIZE		/* Increment virtual addr */
	sub	ta2, CACHE_SLINE_SIZE
	bgtz	ta2, 1b
	 nop

	/*
	 * At this point, the primary D-cache tags are marked dirty
	 * exclusive, and the secondary cache tags are marked the same.
	 * We now copy the data into the primary data cache. Since parity
	 * is on a byte basis, we store words - no read/modify/write
	 * is required by the R10K.  Here we assume the length is a
	 * MULTIPLE NUMBER OF d-cache lines.
	 */

	move	t8, s2				/* Destination */

	move	ta3, t0				/* Source */
	move	ta1, t1				/* Length */
1:
	lw	v0,0(ta3)
	sw	v0,0(t8)
	daddiu	ta3, 4
	daddiu	t8, 4
	sub	ta1,4
	bgtz	ta1,1b
	 nop

	/*
	 * Now flush the newly copied data out of the data cache,
	 * and into the secondary cache.
	 */

	move	t8, s2
	move	ta1,t1				/* Length */
1:
	DCACHE(C_HWBINV, 0(t8))			/* Flush from d-cache */
	ICACHE(C_HINV, 0(t8))			/* Be sure out of i-cache */
	daddiu	t8, CACHE_DLINE_SIZE
	sub	ta1, CACHE_DLINE_SIZE
	bgtz	ta1, 1b
	 nop

	/*
	 * Everything is now flushed to secondary cache - we can return
	 * to the caller and let them know all is OK.
	 */

	j	a3
	 move	v0, s2
	END(cache_copy_pic)
#endif

/*
 * void exec_sscache_asm(i64 base, i64 taglo, i64 taghi)
 */

LEAF(exec_sscache_asm)
        MTC0(a1, C0_TAGLO)
        MTC0(a2, C0_TAGHI)

        SCACHE(C_ISD, 0(a0))

        jr      ra
        END(exec_sscache_asm)

/*
 * void exec_sstag_asm(i64 base, i64 taglo, i64 taghi, int way)
 */

LEAF(exec_sstag_asm)
        MTC0(a1, C0_TAGLO)
        MTC0(a2, C0_TAGHI)
	bnez a3, 1f
	nop

      	SCACHE(C_IST, 0(a0)) 	/* way 0 */
	jr 	ra
	nop

1:     	SCACHE(C_IST, 1(a0))     /* way 1 */
        jr      ra
        END(exec_sstag_asm)
	
