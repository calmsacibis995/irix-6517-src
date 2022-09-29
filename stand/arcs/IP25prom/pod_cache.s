/*
 * File: pod_cache.s
 * Purpose: Cache testing, initializeation and error processing routine for 
 *	    R10K boot prom.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <asm.h>

#include <sys/regdef.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>

#include <sys/EVEREST/IP25.h>
#include <sys/EVEREST/evdiag.h>

#include "ip25prom.h"
#include "prom_leds.h"
#include "pod.h"
#include "cache.h"

#define	K0_BASE	0xa800000000000000

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
#define	CP_SIZE 	24		/* Total size of entry */

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

#define	SCCTAG_PATTERN(tag)	.dword	(tag & 0x00ffffff)
#define	SCC_SIZE	8
	
	.data

	.align 	8
scache_patternStart:
#if	!defined(SABLE )
	SCACHE_PATTERN(0xaaaaaaaa, 0xaaaaaaaa, 0x2aa, 0xaaaaaaaa)
	SCACHE_PATTERN(0x55555555, 0x55555555, 0x155, 0x55555555)
	SCACHE_PATTERN(0x00000000, 0x00000000, 0x000, 0x00000000)
#endif
scache_patternEnd:

icache_patternStart:
#if !defined(SABLE)
	ICACHE_PATTERN(0xffffffff, 0xffffffff, 0xff, 0xffffffff)
	ICACHE_PATTERN(0xaaaaaaaa, 0xaaaaaaaa, 0xaa, 0xaaaaaaaa)
	ICACHE_PATTERN(0x55555555, 0x55555555, 0x55, 0x55555555)
	ICACHE_PATTERN(0x00000000, 0x00000000, 0x00, 0x00000000)
#endif
icache_patternEnd:

dcache_patternStart:
#ifndef SABLE
	DCACHE_PATTERN(0xffffffff, 0xffffffff, 0xff, 0xffffffff)
	DCACHE_PATTERN(0xaaaaaaaa, 0xaaaaaaaa, 0xaa, 0xaaaaaaaa)
	DCACHE_PATTERN(0x55555555, 0x55555555, 0x55, 0x55555555)
	DCACHE_PATTERN(0x00000000, 0x00000000, 0x00, 0x00000000)
#endif
dcache_patternEnd:

scctag_patternStart:
#ifndef	SABLE
	SCCTAG_PATTERN(0xffffffffffffffff)
	SCCTAG_PATTERN(0xaaaaaaaaaaaaaaaa)
	SCCTAG_PATTERN(0x5555555555555555)
	SCCTAG_PATTERN(0x0000000000000000)
#endif
scctag_patternEnd:	

#undef	CACHE_PATTERN
#undef	SCACHE_PATTERN
#undef	ICACHE_PATTERN
#undef	DCACHE_PATTERN
#undef	SCCTAG_PATTERN

	.text
	.set	reorder
	.set	at
	
/*
 * Function: sCacheSize
 * Purpose: To read and return the size of the secondary cache
 * Parameters:
 * Returns: Cache size in bytes in v0
 * Clobbers: v0,v1
 */
LEAF(sCacheSize)
        .set	noreorder
	MFC0(v1, C0_CONFIG)		# Pick of config register value
	and	v1,CONFIG_SS		# isolate cache size
	srl	v1,CONFIG_SS_SHFT
	add	v1,CONFIG_SCACHE_POW2_BASE
	li	v0,1
	j	ra
	sll	v0,v1			# Calculate # bytes
        END(sCacheSize)
/*
 * Function: iCacheSize
 * Purpose: To read and return the size of the primary instruction cache
 * Parameters: none
 * Returns: Cache size in bytes in v0
 * Clobbers: v0,v1
 */
LEAF(iCacheSize)
	.set	noreorder
	MFC0(v1, C0_CONFIG)		# Pick of config register value
	and	v1,CONFIG_IC		# isolate cache size
	srl	v1,CONFIG_IC_SHFT
	add	v1,CONFIG_PCACHE_POW2_BASE
	li	v0,1
	j	ra
	sll	v0,v1			# Calculate # bytes
        END(iCacheSize)

/*
 * Function: dCacheSize
 * Purpose: To read and return the size of the primary data cache
 * Parameters: none
 * Returns: Cache size in bytes in v0
 * Clobbers: v0,v1
 */
LEAF(dCacheSize)
        .set	noreorder
	MFC0(v1, C0_CONFIG)		# Pick up config register value
	and	v1,CONFIG_DC		# isolate cache size
	srl	v1,CONFIG_DC_SHFT	
	add	v1,CONFIG_PCACHE_POW2_BASE
	li	v0,1
	j	ra
	sll	v0,v1			# Calculate # bytes
        END(dCacheSize)

/*
 * Function:	testIcache
 * Purpose:	verify we can write and read the Icache.
 * Parameters:	None
 * Returns:	v0 = 0 - success
 *		   = !0- failed
 * Notes:	The data patterns 0xaaaaaaaaa and 0x55555555 are used
 * 	        to try and check for bits stuck to 0 or 1.
 */
LEAF(testIcache)
	.set	noreorder

	LEDS(PLED_TESTICACHE)

	/* Figure out the number of lines */

	move	t0,ra			/* save our way home */
	jal	iCacheSize
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
	MFC0(a1,C0_TAGLO)
	bne	a1,t3,testIcacheFailAddrWay0
	ICACHE(C_ILT, 1(t2))		/* or the other */
	MFC0(a0,C0_TAGHI)
	bne	a0,v1,testIcacheFailAddrWay1
	MFC0(a1,C0_TAGLO)
	bne	a1,t3,testIcacheFailAddrWay1

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
	nop
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

        ICACHE(C_ILD, 1(t2))
        MFC0(a1, C0_TAGHI)
        MFC0(a2, C0_TAGLO)
	MFC0(a3, C0_ECC)
        bne	a1,ta1,testIcacheFailDataWay1
	nop
        bne	a2,ta2,testIcacheFailDataWay1
        nop
	bne	a3,ta3,testIcacheFailDataWay1

        sub	ta0,1
        bgtz	ta0,4b			/* more words in line */
        daddiu 	t2,4			/* DELAY: Up 4 bytes */

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
	move	v0,zero			/* success */

testIcacheFailAddrWay0:
	MESSAGE(a2, "I-cache tag compare error:	way 0:"); 
	b	testIcacheFailAddr
	nop
testIcacheFailAddrWay1:
	MESSAGE(a2, "I-cache tag compare error:	way 1:"); 
testIcacheFailAddr:
	/*
	 * Tag compare error:
	 * a0/a1 - taghi/taglo read values
	 * v1,t3 - taghi/taglo expected values
	 * t2 - address
	 */
	dsll	a0,32
	dsll	a1,32
	dsrl	a1,32
	or	t0,a1,a0		/* 64-bit read back value */

	dsll	v1,32
	dsll	t3,32
	dsrl	t3,32
	or	t3,v1			/* 64-bit expected value */

	move	t1,ra			/* Save return address */
	
	PMESSAGE_PTR(a2); PMESSAGE(" address: 0x"); PHEX(t2)
	PMESSAGE("\n\r\twrote: 0x"); PHEX(t3);
	PMESSAGE(" read: 0x"); PHEX(t0);
	PMESSAGE("\n\r")
	
	j	t1
	ori	v0,zero,EVDIAG_ICACHE_ADDR
	
testIcacheFailDataWay0:
	MESSAGE(a0, "\r\n  I-cache data compare error: way 0:")
	b	testIcacheFailData
	nop
testIcacheFailDataWay1:	
	MESSAGE(a0, "\r\n  I-cache data compare error: way 1:")
testIcacheFailData:
	/*
	 * Data Compare error:
	 * v0 - address of pattern in table.
	 * a1/a2/a3 - read HI/LO/ECC
	 * t2 - address
	 */

	dsll	a1,32
	dsll	a2,32
	dsrl	a2,32
	or	a2,a1			/* 64-bit read value */
	
	move	t0,ra
	move	t1,a1			/* Gets lots in print calls */
	move	t3,v0			/* Save pointer to table */
	move	k0,a3			/* zapped in HEX output */
	
	PMESSAGE_PTR(a0); PMESSAGE(" address: 0x"); PHEX(t2)
	PMESSAGE("\n\r\twrote(data/ecc): 0x");
		lwu	a0,CP_DATA_HI(t3)
		lwu	a1,CP_DATA_LO(t3)
		or	a0,a1
		PHEX(a0)
		PMESSAGE("/0x");
		lwu	a0,CP_ECC(t3)
		PHEX32(a0)
	
	PMESSAGE(" read(data/ecc): 0x")
		PHEX32(a2); PMESSAGE("/0x"); PHEX32(k0)
	
	j	t0
	ori	v0,zero,EVDIAG_ICACHE_DATA
	END(testIcache)

/*
 * Function: 	testDcache
 * Purpose: 	Verify we can write/read the Dcache
 * Parameters: 	none
 * Returns:	v0 = 0 - success
 *		v0 !=0 = failed
 */
LEAF(testDcache)
	.set	noreorder
	LEDS(PLED_TESTDCACHE)

	/* figure out # of lines */

	move	t0,ra			/* save way back */
	jal	dCacheSize
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
	daddu	t2,CACHE_DLINE_SIZE
	bgtz	t1,2b			/* See if more to do ... */
	nop

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
	MFC0(a1,C0_TAGLO)		/* DELAY */
	bne	a1,t3,testDcacheFailAddrWay0
	DCACHE(C_ILT, 1(t2))		/* or the other */
	MFC0(a2,C0_TAGHI)
	bne	a2,v1,testDcacheFailAddrWay1
	MFC0(a1,C0_TAGLO)		/* DELAY */
	bne	a1,t3,testDcacheFailAddrWay1

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
        MFC0(a2,C0_TAGLO)		/* DELAY */
        bne	a2,v1,testDcacheFailDataWay0
	nop

        DCACHE(C_ILD, 1(t2))
        MFC0(a1,C0_ECC)
        bne	a1,t3,testDcacheFailDataWay1
        MFC0(a2,C0_TAGLO)		/* DELAY */
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
	move	v0,zero			/* success */
	
testDcacheFailAddrWay0:
        MESSAGE(a0, "\r\n  D-cache tag compare error: Way 0:")
        b	testDcacheFailAddr
	nop
testDcacheFailAddrWay1:
        MESSAGE(a0, "\r\n  D-cache tag compare error: Way 1:")
testDcacheFailAddr:
	/*
	 * Dump information on the failure:
	 *	a1/a2 - read taglo/taghi
	 *      t3/v1 - expected taglo/taghi
	 *	t2 - address
	 */
 
        dsll	a2,32
        dsll	a1,32
        dsrl	a1,32
        or	a2,a1				/* 64-bit read value */

        dsll	v1,32
        dsll	t3,32
        dsrl	t3,32
        or	t3,v1			/* 64-bit expected value */

        move	t1, ra

	PMESSAGE_PTR(a0); PMESSAGE(" address: 0x"); PHEX(t2);
        PMESSAGE("\n\r\t wrote: 0x"); PHEX(t3);
        PMESSAGE(" read: 0x"); PHEX(a2);
        PMESSAGE("\n\r");
	j	t1
	ori	v0,zero,EVDIAG_DCACHE_ADDR	/* DELAY */

testDcacheFailDataWay0:
	MESSAGE(a0, "\r\n  D-cache data compare error: Way 0:")
	b	testDcacheFailData
	nop
testDcacheFailDataWay1:
	MESSAGE(a0, "\r\n  D-cache data compare error: Way 1:")
testDcacheFailData:	
	/*
	 * Dump information on the failure:
	 *	a1/a2 - read ecc/taglo
	 *      t3/v1 - expected ecc/taglo
	 *	t2 - address
	 */
	move	a3,a1			/* A1 - clobber by I/O */
	move	t0,v1			/* Clobbered by I/O */
	
        move	t1, ra

	PMESSAGE_PTR(a0); PMESSAGE(" address: 0x"); PHEX(t2);
        PMESSAGE("\n\r\t wrote(data/parity): 0x"); PHEX32(t0); 
		PMESSAGE("/0x"); PHEX32(t3);
        PMESSAGE(" read(data/parity): 0x"); PHEX32(a2);
		PMESSAGE("/0x"); PHEX32(a3);
        PMESSAGE("\n\r");
	
	j	t1
	ori	v0,zero,EVDIAG_DCACHE_DATA	/* DELAY */

testDcacheFail:
	
	END(testDcache)


	.text

/*
 * Function:	invalidateIcache
 * Purpsoe:	To invalidate the primary icache and leave it in a consistent
 * 		state.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses t0,t1,v0,v1
 */
LEAF(invalidateIcache)
	.set	noreorder
        LEDS(PLED_INVICACHE)
	move	t0,ra			/* Remeber way home */
	jal	iCacheSize
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
	 * Now for safty, the data - even parity, so taglo/hi are
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
	daddu	t1,4			/* DELAY: address += word size */
	bgtz	v0,1b
	nop
	
	j	ra
	nop
	END(invalidateIcache)
	
/*
 * Function:	invalidateDcache
 * Purpsoe:	To invalidate the primary icache and leave it in a consistent
 * 		state.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses v0,v1 t0/t1 registers.
 */
LEAF(invalidateDcache)
	.set	noreorder
        LEDS(PLED_INVDCACHE)
	move	t0,ra			/* Remeber way home */
	jal	dCacheSize
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
	 *  Now for safty, the data. TAGLO and ECC used, but even
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
	daddu	t1,4			/* DELAY: address += word size */
	bge	v0,zero,1b
	nop

	j	ra
	nop
	END(invalidateDcache)

/*
 * Function:	invalidateIDcache
 * Purpose:	To invalidate the primary I & D cache and leave them 
 * 		in a consistent	state.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses t0/t1 registers.
 */
LEAF(invalidateIDcache)
	move	t2,ra
	jal	invalidateIcache
	nop
	jal	invalidateDcache
	nop
	j	t2
	nop
	END(invalidateIDcache)
	
LEAF(invalidateCCtags)
/*
 * Function:	invalidateCCtags
 * Purpose:	To invalidate the SCC duplicate tags.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses .... lots
 *		This routine copies a segment of code to the Scache and
 *		then the icache.
 */
       .set	noreorder
#if !defined(SABLE)
       move 	s7,ra
       LEDS(PLED_INVCCTAGS)
        /*
         * Due to a CC chip "feature", we must configure the cache
         * as maximum size (for shiva) and clear all the duplicate
	 * tags - even if the cache is not full size.
	 */

	EV_GET_SPNUM(k0,k1)			/* our address */
	EV_GET_PROCREG(k0, k1, EV_CFG_CACHE_SZ, a4)
	dli	a0,5
	EV_GET_SPNUM(k0,k1)			/* our address */
	EV_SET_PROCREG(k0, k1, EV_CFG_CACHE_SZ, a0)
	GET_CC_REV(a0, a1)
	bne	a0,zero,invalidateCCtagsUncached
	nop
        /*
	 * Now copy the code segment to the Icache, and run it.
	 */
        dla	a0,1f
        dla	a1,2f
        jal	copyToICache
        dsubu	a1,a0				/* DELAY: */
	b	invalidateCCtagsDoit
	nop
invalidateCCtagsUncached:	
	dla	v0, 1f
invalidateCCtagsDoit:
	
        /* v0 - has cached address of routine on return */
        
	li	a0,4*1024*1024/CACHE_SLINE_SIZE
	dli	a1,EV_BTRAM_BASE
        jal	v0
        nop

        j	2f
        nop     				/* And continue on */
        /* Pad out to start of secondary cache line */
        .align	7
1:	
        /*
         * THIS CODE MAY BE EXECUTED OUT OF ICACHE.
         */
        nop
	sd	zero,0(a1)
	daddiu	a1,CTD_SIZE
	sub	a0,1
	bgtz	a0,1b
	nop
	/*
	 * These instructions ensure that all store to the
	 * CC tags are complete before we do a read (instruction
	 * fetch). The GET_PROCREG makes sure of this, the SET_PROCREG
	 * is required to reset the cache size to its correct value.
	 */
	EV_GET_SPNUM(k0,k1)			/* our address */
	EV_SET_PROCREG(k0, k1, EV_CFG_CACHE_SZ, a4)
	EV_GET_SPNUM(k0,k1)			/* our address */
	EV_GET_PROCREG(k0, k1, EV_CFG_CACHE_SZ, a4)
        /*
	 * Now serialize and be sure the 4 decoded instructions do not cause
	 * a cache prefetch.
	 */
        DMFC0(zero, C0_SR)
        j	ra
        nop
        nop
        nop
        /* Pad out to end of secondary cache line */
        .align	7
2:
        move	ra,s7
#endif
        j	ra
        nop
        END(invalidateCCtags)

/*
 * Function:	invalidateScache
 * Purpose:	To invalidate the secondary cache, both duplicate and
 *		T5 tags.
 * Parameters:	none
 * Returns:	nothing
 * Notes:	uses v0,t0
 */
LEAF(invalidateScache)
	.set	noreorder

        LEDS(PLED_INVSCACHE)
#if !defined(SABLE)
	move	t0,ra				/* Save return address */

	jal	sCacheSize			/* go get cache size */
	nop					/* DELAY */
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
#endif

	j	ra
	nop
	END(invalidateScache)

/*
 * Function:	testScache
 * Purpose:	To test the secondary cache, without writting back to
 *		main memory. This does NOT touch CC duplicate tags.
 * Parameters:	none
 * Returns: v0 - 0 OK, !0 failed
 */
LEAF(testScache)
	LEDS(PLED_TESTSCACHE)
	.set	noreorder

	/* Figure out # of lines */

	move	t0,ra				/* save return address */
	jal	sCacheSize			/* pick up scache size */
	nop					/* DELAY */
	move	ra,t0				/* Put back in nice place */
	divu	gp,v0,CACHE_SLINE_SIZE*2	/* 2-way remember */

	dla	v0,scache_patternStart
1:
	/*
	 * Check if done at top of loop. This means that we can have
	 * no test patterns if desired.
	 */


	dla	v1,scache_patternEnd
	bge	v0,v1,testScacheData
	nop
	
	/*
	 * First test all of the cache tags.
	 */
	dli	v1,K0_BASE
	move	a3,gp				/* Count # pairs of lines */
	lw	t1,CP_TAG_LO(v0)
	lw	t2,CP_TAG_HI(v0)
	MTC0(t1, C0_TAGLO)
	MTC0(t2, C0_TAGHI)			/* top 32 bits */
2:
	SCACHE(C_IST, 0(v1))			/* Store tag - way 0 */
	SCACHE(C_IST, 1(v1))			/*           - way 1 */
	subu	a3,1
	daddiu	v1,CACHE_SLINE_SIZE		/* Next line(s) please */
	bgtz	a3,2b
	nop

	/*
	 * Read back all tags, note that the "correct" value of the 64-bit
	 * tag is in t1.
	 */
	dli	v1,K0_BASE
	move	a3,gp				/* Count # pairs of lines */
2:

	SCACHE(C_ILT, 0(v1))			/* WAY 0 */
	MFC0(a1, C0_TAGLO)
	MFC0(a2, C0_TAGHI)
	bne	t1,a1,testScacheFailedTagWay0
	nop
	bne	t2,a2,testScacheFailedTagWay0
	nop
	
	SCACHE(C_ILT, 1(v1))			/* WAY 1 */
	MFC0(a1, C0_TAGLO)
	MFC0(a2, C0_TAGHI)
	bne	t1,a1,testScacheFailedTagWay1
	nop
	bne	t2,a2,testScacheFailedTagWay1
	nop

	subu	a3, 1
	daddiu	v1,CACHE_SLINE_SIZE
	bgtz	a3,2b
	nop
	daddiu	v0,CP_SIZE			/* DELAY */
	b	1b				/* Back to top of loop */
	nop
	
testScacheData:		
	/*
	 * The INDEX STORE_DATA for the secondary cache only stores
	 * 2 of the 4 words in each quad word. Thus, for each quad
	 * word, we must first test the first 2 words, then the second
	 * 2. We double check the ECC at both phases - just for fun.
	 * v1 is really a boolean value that indicates if we are on the
	 * upper 8 bytes or lower eight bytes.
	 */
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
	multu	a0,gp				/* total sublines per way */
        mflo	a0
4:	
	SCACHE(C_ISD, 0(t3))			/* Way 0 */
	SCACHE(C_ISD, 1(t3))			/* Way 1 */
	subu	a0, 1
	daddiu	t3,CACHE_SLINE_SUBSIZE		/* Next subline */
	bgtz	a0,4b
	nop	
	/*
	 * Now read back and verify the results with what we wrote. At
	 * this point the expected values are:
	 * ECC - a3, taghi - a2, taglo - a1
	 */
	dli	t3,K0_BASE
	daddu	t3,v1
	li	a0,CACHE_SLINE_SIZE/CACHE_SLINE_SUBSIZE
	multu	a0,gp				/* Total sublines */
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
	
	subu	a0,1				/* Next offset into sublines*/
	bgtz	a0,4b
	daddiu	t3,CACHE_SLINE_SUBSIZE

	bnez	v1,2b
	move	v1,zero				/* DELAY */

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
	multu	a0,gp				/* total sublines per way */
        mflo	a0

4:		
	move	a1,t3
	daddiu	a2,t3,4
	MTC0(a1,C0_TAGLO)
	MTC0(a2,C0_TAGHI)
	SCACHE(C_ISD, 0(t3))			/* Way 0 */
	SCACHE(C_ISD, 1(t3))			/* Way 1 */
	subu	a0, 1
	daddiu	t3,CACHE_SLINE_SUBSIZE		/* Next subline */
	bgtz	a0,4b
	nop

	/*
	 * Now read back and verify the results with what we wrote.
	 */
	dli	t3,K0_BASE
	daddu	t3,v1
	li	a0,CACHE_SLINE_SIZE/CACHE_SLINE_SUBSIZE
	multu	a0,gp				/* Total sublines */
        mflo	a0
	move	t0,zero
	move	a3,zero
4:		
	move	a1,t3
	daddiu	a2,t3,4
	dli	v0,0xffffffff
	and	a1,v0
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
	nop
	
	subu	a0,1				/* Next offset into sublines*/
	bgtz	a0,4b
	daddiu	t3,CACHE_SLINE_SUBSIZE

	bnez	v1,2b
	move	v1,zero				/* DELAY */

testScacheSuccess:
	j	ra
	move	v0,zero
	
testScacheFailedWay0:
	MESSAGE(a0, "\r\n  2ndry data compare error: Way 0:")
	b	testScacheFailed
	nop
testScacheFailedWay1:
	MESSAGE(a0, "\r\n  2ndry data compare error: Way 1:")
testScacheFailed:
	/*
	 * For debug proms, attempt to dump information to
	 * CC serial port. At this point:
	 *	t3 - address
	 *	t0 - read ECC value
	 *	t1/t2 - read taghi/taglo values (8 bytes)
	 *	a3 - expected ECC value
	 *	a1/a2 - expected taghi/taglo values (8 bytes)
	 */

	dsll	t1,32
	dsll	t2,32
	dsrl	t2,32
	or	t1,t2

	dsll	a1,32
	dsll	a2,32
	dsrl	a2,32
	or	k1,a2,a1
	move	k0,a3

        move	t2,ra

	PMESSAGE_PTR(a0); PMESSAGE(" address: "); PHEX(t3); 
        PMESSAGE("\n\r\twrote(data/ecc): 0x"); PHEX(k1); 
        PMESSAGE("/"); PHEX32(k0); 
        PMESSAGE("\n\r\tread (data/ecc): 0x"); PHEX(t1);
        PMESSAGE("/"); PHEX32(t0); 
       
	PMESSAGE("\n\r")
	
	j	t2
	ori	v0,zero,EVDIAG_SCACHE_DATA	/* DELAY */

testScacheFailedTagWay0:
	MESSAGE(a0, "\r\n  2ndry Tag compare error: way 0:")
	b	testScacheFailedTag
	nop
testScacheFailedTagWay1:
	MESSAGE(a0, "\r\n  2ndry Tag compare error: way 1:")
testScacheFailedTag:	
	/*
	 * For debug proms, attempt to dump information to
	 * CC serial port. At this point,
	 *	v1 - address
	 *	a1/a2 - pattern read back
	 *	t1/t2 - pattern written
	 */
	dsll	a2,32				/* Top 32 bits */
	dsll	a1,32
	dsrl	a1,32				/* isolate lower 32 bits */
	or	k0,a2,a1			/* a2 - read 64-bit value */
	
	dsll	t2,32				/* Top 32 bits */
	dsll	t1,32
	dsrl	t1,32				/* isolate lower 32 bits */
	or	t1,t2				/* t1 - expected value */

	move	t2,ra				/* Save way home */
	move	k1,v1

	PMESSAGE_PTR(a0); PMESSAGE(" address: "); PHEX(k1); 
        PMESSAGE("\r\n\twrote: 0x"); PHEX(t1);
        PMESSAGE("\r\n\tread:  0x"); PHEX(k0);
	PMESSAGE("\n\r")

	j	t2
	ori	v0,zero,EVDIAG_SCACHE_TAG	/* DELAY */
	
	.set	reorder
	END(testScache)

/*
 * Function:	testCCtags
 * Purpose:	verify the duplicate cache tags in the SCC
 * Parameters:	none
 * Returns:	0 - OK
 *		1 - failed
 * Notes:	The SCC duplicate tages are addressed as follows:
 *			line	way	address (EV_BTRAM_BASE +)
 *			0	0	0
 *			0	1	8
 *			1	0	16
 *			1	1	24
 *			...
 * Thus, we look at the SCC tags as 1 way for this test, and having
 * a size of 8 (CTD_SIZE).
 *
 */
        .align	7		/* Align on scache boundary */
LEAF(testCCtags)
	.set	noreorder
#if !defined(SABLE)
	/*
	 * Compute SCC cache tag mask ---- use the SR because it has the
	 * log2 value as it stands.
	 */

	MFC0(v0,C0_CONFIG)
	and	v0,CONFIG_SS
	srl	v0,CONFIG_SS_SHFT

	/*
	 * SCC needs to know the size of the secondary cache at
	 * this point to correctly mask bits and generate
	 * Parity.
	 */	

	dli	t2,0x00ffffff
	srl	t2,v0
	sll	t2,v0				/* Mask for tags */
	
	li	v1,5
	sub	v0,v1,v0			/* SCC value:CACHE_SZ */
	EV_GET_SPNUM(k0,k1)			/* our address */
	EV_SET_PROCREG(k0, k1, EV_CFG_CACHE_SZ, v0)

	/*
	 * We assume the maximum cache size for the purpose of
	 * the testing. The total number of lines is used.
	 */
	li	v0,1024*1024*4/CACHE_SLINE_SIZE
		
	dla	t0,scctag_patternStart
1:
	dla	t1,scctag_patternEnd
	bge	t0,t1,testCCtagsSuccess		/* Done */
	nop

	/*
	 * Fill tag rams with pattern.
	 */	
	move	a2,v0				/* # lines */
	ld	v1,0(t0)			/* Current pattern */
	and	v1,t2
	dli	a0,EV_BTRAM_BASE
2:
	sd	v1,0(a0)
	daddiu	a0,CTD_SIZE
	sub	a2,1
	bgtz	a2,2b
	nop
	
	/*
	 * Due to an SCC problem, we can not read the CC tags to soon after
	 * a write from the CPU. For that reason, we serialize, and cause a
	 * stall - by loading the cache_size config register.
	 */
	EV_GET_SPNUM(k0,k1)			/* Cause a stall */
	EV_GET_PROCREG(k0, k1, EV_CFG_CACHE_SZ, zero)
	MFC0(zero, C0_SR)			/* serialize */
	
	/*
	 * Check patterns in tag rams.
	 */
	dli	a0,EV_BTRAM_BASE
	move	a2,v0				/* Count */
2:
 	ld	t1,0(a0)
	beq	v1,t1,3f
	DMFC0(zero, C0_SR)			/* Serialize */
	DMFC0(zero, C0_SR)			/* Serialize */
	dla	t3,testCCtagsFail
	jr	t3
	nop
3:	
	daddiu	a0,CTD_SIZE			/* DELAY: */
	sub	a2,1				/* DELAY: */
	bgtz	a2,2b
	nop
	b	1b				/* Check for more */
	daddiu	t0,SCC_SIZE
	
#endif /* !defined(SABLE) */
	
testCCtagsSuccess:
        /*
	 * Up to 4 instructions after the serialization instruction
	 * may be speculated since they are already decoded etc. Thus, 
	 * we put nops is just ro be sure we know what is going on.
	 */
        DMFC0(zero, C0_SR)			/* Serialize */
        nop
        nop
	j	ra
	move	v0,zero
	
        .align	7		/* Pad out Align on scache boundary */

testCCtagsFail:
	.set	noreorder
	/*
	 * Attempt to dump the address/wrote/read value to the CC uart. 
	 * At this point, registers are as follows:
	 *	a0 - address
	 *	v1 - pattern written
	 *	t1 - pattern read
	 */

	move	t3, ra				/* save way home */
	move	t2,a0				/* address */

	move	t0,v1				/* Pattern written */
	PMESSAGE("\r\n  CC Tag Compare Error: Address: ")
	PHEX(t2)
	PMESSAGE("\n\r\tWrote: ")
	PHEX32(t0)
	PMESSAGE( "\r\n\tRead: ")
	PHEX32(t1)
	PMESSAGE("\n\r")
	move	ra,t3				/* Restore way home */

	j	ra
	ori	v0,zero,EVDIAG_BUSTAG_DATA
	.globl	testCCtags_END
testCCtags_END:	
	.set	reorder
	END(testCCtags)

/*
 * Function:	parity
 * Purpose:	Compute parity of 64-bit value
 * Parameters:	a0 - 64-bit value to compute parity on
 * 		a1 - 0 for EVEN parity, 1 for ODD parity
 * Returns:	v0 - parity bit in LSB
 * Notes:	uses a0,a1,v0
 */
LEAF(parity)
	.set	noreorder
	dsrl	v0, a0, 32
	xor	a0, v0
	dsrl	v0, a0, 16
	xor	a0, v0
	dsrl	v0, a0, 8
	xor	a0, v0
	dsrl	v0, a0, 4
	xor	a0, v0
	dsrl	v0, a0, 2
	xor	a0, v0
	dsrl	v0, a0, 1
	xor	a0, v0
	xor	v0, a0, a1
	j	ra
	 and	v0, 1
	.set	reorder
	END(parity)

/*
 * Function:	initDcacheStack
 * Purpose:	Set up the primary data cache to has valid (zero'd)
 *		exlusive dirty cache lines starting at
 *	 	POD_STACKADDR. Also set stack pointer.
 * Parameters:	none
 * Returns:	sp updated.
 * Notes:	The size of the stack is sizeof(dcache) / 2, ie, we don't
 * 		bother to initialize both ways.
 *
 * 	        Virtual address of stack = POD_STACKADDR
 *		Physical address of stack = POD_STACKPADDR
 */
LEAF(initDcacheStack)
	.set	noreorder
	move	a3,ra				/* Save RA */
	LEDS(PLED_MAKESTACK)
	dli	t2,POD_STACKSIZE/CACHE_DLINE_SIZE /* #lines */
	dli	sp,POD_STACKVADDR		/* Start of stack */
	dli	a2,POD_STACKPADDR		/* Stack physical address */
	/*
	 * Build tag for virtual address in sp, physical address in
	 * a2. State, SC way, State Parity, LRU are constant. We use:
	 *
	 *	State Mod	= 1 <normal>
	 *	State		= dirty, exclusive, inconsistent <CTP_STATE_DEI>
	 *	State Parity	= EVEN_PARITY(state) = <constant>
	 *	LRU 		= 0
	 *	SC way		= 0
	 *
	 *	TAG[39:36]	= <variable>
	 *	TAG[35:	12]	= <variable>
	 *	Tag Parity	= <variable>
	 */
	dli	a0,CTP_STATE_DE
	jal	parity				/* Compute state parity */
	move 	a1,zero				/* DELAY: Even please */

	dsll	v0,CTP_STATEPARITY_SHFT
	dli	t1,(CTP_STATE_DE<<CTP_STATE_SHFT) + \
		   (CTP_STATEMOD_N<<CTP_STATEMOD_SHFT)
	or	t1,v0				/* State with parity */
	/*
	 * t1 holds the constant parts of the tag, with parity. The actual
	 * TAG address must now be computed and loaded.
	 */
1:
	dsrl	t0,a2,4				/* NOTYET */
	and	t0,CTP_TAG_MASK			/* Physical address */
						/*   in correct place */
	move	a0,t0
	jal	parity
	move	a1,zero				/* DELAY: even parity */
	dsll	v0,CTP_TAGPARITY_SHFT
	or	t0,v0				/* Tag with tag parity */
	or	t0,t1				/* Tag complete with parity */

	/* Now set the dcache entry for the specified line */

	MTC0(t0,C0_TAGLO)			/* Low order 32 bits */
	dsrl	t0,32
	MTC0(t0,C0_TAGHI)			/* High orger 32 bits */
	DCACHE(C_IST, 0(sp))			/* Set tag! */

	/* 
	 * Must be sure data in cache line does not have a parity
   	 * error.
	 */

	DMTC0(zero, C0_ECC)			/* Even parity, ECC=0  */
	MTC0(zero, C0_TAGLO)			/* 0 data */

	li	a1,CACHE_DLINE_SIZE-4		/* # of stores */
2:	
	daddu	a0,a1,sp			/* Virtual address of word */
	DCACHE(C_ISD, 0(a0))			/* Store 0 */
	daddi	a1,-4				/* DELAY: next word */
	bgez	a1,2b
	nop

	daddiu	sp,CACHE_DLINE_SIZE		/* Bump virtual	address */
	daddiu	a2,CACHE_DLINE_SIZE		/* Bump physical address */
	sub	t2,1
	bgtz	t2,1b				/* next or are we done ? */
	nop					

	dli	sp,POD_STACKVADDR+POD_STACKSIZE-8
	j	a3				/* Home James */
	nop
	.set	reorder
	END(initDcacheStack)
/*
 * Function:	iLine
 * Purpose:	Fetch an instruction line from the icache.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - ptr to il_t buffer
 * Returns:	a1-> filled in.
 */
LEAF(iLine)
	.set	noreorder

	ICACHE(C_ILT, 0(a0))		/* Store Tag */
	MFC0(v0, C0_TAGLO)
	MFC0(v1, C0_TAGHI)
	dsll	v1,32
	or	v0,v1
	sd	v0,IL_TAG(a1)

	daddiu	t0,a1,IL_DATA		/* Data pointer   */
	daddiu	t1,a1,IL_PARITY		/* Parity pointer */

	daddiu	a2,a0,CACHE_ILINE_SIZE
1:		
	ICACHE(C_ILD, 0(a0))
	MFC0(v0, C0_TAGLO)
	MFC0(v1, C0_TAGHI)
	dsll	v1,32
	or	v0,v1
	sd	v0,0(t0)
	
	MFC0(v0, C0_ECC)
	sb	v0,0(t1)
	daddiu	t0,8			/* 8-bytes stored */
	daddiu	a0,4
	blt	a0, a2, 1b
	daddiu	t1,1			/* 1-bit of parity */
	
	j	ra
	nop
	.set	reorder
	END(iLine)

/*
 * Function:	dLine
 * Purpose:	Fetch primary data cache line.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - ptr to dl_t
 * Returns:	a1--> filled in.
 */
LEAF(dLine)
	.set	noreorder
	
	DCACHE(C_ILT, 0(a0))		/* Read and store tag */
	MFC0(v0, C0_TAGHI)
	MFC0(v1, C0_TAGLO)
	dsll	v0,32
	or	v0,v1
	sd	v0,DL_TAG(a1)

	daddiu	a2,a0,CACHE_DLINE_SIZE
	daddiu	t0,a1,DL_DATA		/* Data pointer */
	daddiu	t1,a1,DL_ECC		/* ECC pointer */

1:	
	DCACHE(C_ILD, 0(a0))
	MFC0(v0, C0_TAGLO)
	sw	v0,0(t0)
	MFC0(v0, C0_ECC)
	sb	v0,0(t1)
	daddiu	a0,4			/* Next word - vaddr */
	daddiu	t0,4			/* Next word - buffer */
	blt	a0,a2,1b
	daddiu	t1,1			/* DELAY: Next ECC field */

	j	ra
	nop
	
	.set	reorder
	END(dLine)
	
/*
 * Function:	sLine
 * Purpose:	Fetch a cache line from the secondary cache.
 * Parameters:	a0 - vaddr (low order bit signals way)
 *		a1 - pointer to sl_t area.
 * Returns:	nothing
 */
LEAF(sLine)

	.set	noreorder

	SCACHE(C_ILT, 0(a0))		/* Pick up T5 TAG */
	MFC0(v0, C0_TAGHI)
	MFC0(v1, C0_TAGLO)
	dsll	v0,32
	dsll	v1,32			/* Clear high order 32 bits */
	dsrl	v1,32
	or	v0,v1
	sd	v0, SL_TAG(a1)
	
	move	t0,ra			/* Pick up CC tag */
	jal	sCacheSize
	nop
	move	ra,t0

	srl	v0,1			/* div by 2 for # ways */
	sub	v0,1			/* scachesize/2 - 1 */
	and	v0,a0,v0		/* V0 - normalizes virtual address */

	divu	v0,CACHE_SLINE_SIZE	/* Compute index */
	sll	v0,4			/* Index into duplicate tags */
	and	v1,a0,1			/* Look at way */
	sll	v1,3			/* add 8-bytes for way 1 */
	daddu	v1,v0			/* Offset into TAGS */
	daddu	v1,EV_BTRAM_BASE	/* address of TAG */
#if SABLE
	move	v0,zero
#else
	ld	v0,0(v1)		/* duplicate tag */
#endif
	sd	v0,SL_CCTAG(a1)		/* and store for the caller */

	/* OK - lets get the data */

	li	v0,CACHE_SLINE_SIZE/16	/* # fetches */
	daddiu	t0,a1,SL_DATA
	daddiu	t1,a1,SL_ECC
2:
	SCACHE(C_ILD, 0(a0))
	MFC0(v1, C0_TAGLO)		/* Store 8-bytes of data */
	sw	v1,0(t0)
	MFC0(v1, C0_TAGHI)
	sw	v1,4(t0)
	MFC0(v1, C0_ECC)
	sh	v1,0(t1)		/* Store ECC for this 8 bytes */
	
	SCACHE(C_ILD, 8(a0))
	MFC0(v1, C0_TAGLO)		/* Store 8-bytes of data */
	sw	v1,8(t0)
	MFC0(v1, C0_TAGHI)
	sw	v1,12(t0)
	
	sub	v0,1
	daddu	a0,16			/* Increment address */
	daddu	t0,16			/* Increment data array. */
	bgt	v0,zero,2b
	daddiu	t1,2			/* DELAY: Increment ECC array */
1:
	/* All done ... */
	j	ra
	nop
	.set	reorder
	END(sLine)

LEAF(stagECC)
/*
 * Function:	stagECC
 * Purpose:	Compute the secondary cache tag ECC value given
 *		the tag (without the ECC).
 * Parameters:	a0 - The 26-bit cache tag value.
 * Retruns:	v0 - ECC value.
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
        .set	noreorder

        move	v0,zero			/* Start out with 0 result */
        dla	a1,tagECC		/* Current bit selector pointer */

stagECC_loop:				/* Loop for all bits set */

        dla	a2, tagECC_end
        beq	a1,a2,stagECC_loopDone
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
        daddiu	a1, 4			/* Next check bit please ...  */
        b	stagECC_loop		/* Next one */
        nop

stagECC_loopDone:			/* All done ... */
	j	ra
        nop
	
	END(stagECC)

LEAF(copyToICache)
/*
 * Function:	copyToIcache
 * Purpose:	To copy PIC code to the icache, execute it, and return.
 * Parameters:	a0 - pointer to address to copy
 *		a1 - length (# of bytes to copy - must be multiple of
 *		     4.
 * Returns:	v0 - virtual address corresponding to first instruction
 *		     as pointed to by a0.
 * Notes:	Build texts in primary dcache, then flushes to secondary
 *		cache.
 */
	.set	noreorder
	move	a3,ra			/* save RA */
        move	t0,a0

	/*
	 * Be sure to fill an entire s-line, otherwise, we could get
	 * errors on non-initialized parts of the sline since the T5
	 * likes to prefetch stuff.
	 */

	daddiu	t1, a1, CACHE_SLINE_SIZE-1
	and	t1, ~(CACHE_SLINE_SIZE - 1)	/* Mutiple of s-lines */
	divu	t3, t1, CACHE_DLINE_SIZE	/* # primary d-lines. */

	LEDS(PLED_INITICACHE)			/* Do after a0/a1 saved */

	dli	a0, CTP_STATE_DE
	jal	parity
	move	a1,zero

	/* Build constant part of dtag into t2 */

	dsll	v0,CTP_STATEPARITY_SHFT
	dli	t2,(CTP_STATE_DE<<CTP_STATE_SHFT) + \
		   (CTP_STATEMOD_N<<CTP_STATEMOD_SHFT)
	or	t2,v0				/* State with parity */

	/*
	 * Loop through the primary d-cache lines marking the tags.
	 */
	move	ta2,t3
	dli	t8, POD_CODEVADDR		/* Start of virt address */
	dli	t9, POD_CODEPADDR		/* Start of phys address */
1:
	dsrl	ta3, t9, 4
	and	ta3, CTP_TAG_MASK		/* Physical address portion */
	move	a0, ta3
	jal	parity
	move	a1,zero
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
	dli	t9, POD_CODEPADDR
        dli	t8, POD_CODEVADDR

	/*
	 * Set the phsyical address bits - the secondary cache state
	 * is marked XXX to allow a cache HIT in the secondary cache when 
	 * we flush the primary cache.
	 */
1:	
	dsrl	ta3, t9, 4
	and	ta3, CTS_TAG_MASK		/* Physical address bits */
		
	or	ta3, 3 << CTS_STATE_SHFT
	or	ta3, /* VIDX %%% */ 0

	jal	stagECC
	move	a0, ta3
	or	ta3,v0				/* Or in ECC */

	/* TAG is now in ta3 with ECC set - write it and continue. */

	MTC0(ta3, C0_TAGLO)
	dsrl	ta3, 32
	MTC0(ta3, C0_TAGHI)
	SCACHE(C_IST, 0(t8))
	
        MTC0(zero, C0_TAGLO)			/* Clear other way */
	MTC0(zero, C0_TAGHI)
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
	dli	t8, POD_CODEVADDR		/* Destination */
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
	
	dli	t8, POD_CODEVADDR
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

	dli	v0,POD_CODEVADDR
	j	a3
	nop
	END(copyToICache)
	
LEAF(cacheFlush)
/*
 * Routine:	cacheFlush
 * Purpose:	To flush the current contents of the entire cache.
 * Parameters:	none
 * Returns:	Nothing
 * Notes:	Uses v0, v1, a0, a1
 */
	.set	reorder
	move	a0,ra				/* Save way back */
	jal	sCacheSize
	dsrl	v0,1				/* divide by 2 - 2-way */
	dli	v1,K0BASE			/* Use unmapped space */
	daddu	v0,v1				/* Ending address */

	.set	noreorder
1:	
	SCACHE(C_HWBINV, 0(v1))			/* Way 0 */
	SCACHE(C_HWBINV, 1(v1))			/* Way 1 */
	daddu	v1,CACHE_SLINE_SIZE
	bltu	v1,v0,1b
	nop
	.set	reorder

	j	a0
	END(cacheFlush)

LEAF(cacheError)
/*
 * Routine:	cacheError
 * Purpose:	Process a cache error exception.
 * Parameters:	None
 * Returns:	Does not return
 * Notes:	Since we do not return, no registers need be saved.
 */
	.set	reorder
#if 0	
	MFC0(s0, C0_CACHERR)

	/* Check for I-cache/D-cache/S-cache, and do the "right" thing. */

	srl	s1,s0,CE_TYPE_SHFT
	andi	s1,(CE_TYPE_MASK>>CE_TYPE_SHFT)
	be	s1,CE_TYPE_I>>CE_TYPE_SHFT,cacheError_I
	nop
	be	s1,CE_TYPE_D>>CE_TYPE_SHFT,cacheError_D
	nop
	be	s1,CE_TYPE_S>>CE_TYPE_SHFT,cacheError_S
	nop
#endif
	/* Must be SIE */
	END(cacheError)

LEAF(cacheOP)
/*
 * Routine:	cacheOP
 * Purpose:	Perform a cache operation
 * Parameters:	a0 - cacheop_t * - pointer to cache op structure.
 * Returns:	v0 - undefined, for Load data, taghi, taglo, and ECC
 *		     values filled in.
 */
	.set	noreorder
	lw	v0,COP_TAGLO(a0)		/* First setup CP0 */
	MTC0(v0, C0_TAGLO)
	lw	v0,COP_TAGHI(a0)
	MTC0(v0, C0_TAGHI)
	lw	v0,COP_ECC(a0)
	MTC0(v0, C0_ECC)
	
	ld	a1,COP_ADDRESS(a0)		/* Address to use */
	lw	v1,COP_OP(a0)			/* Operation */
	dla	v0,cacheOP_table
	sll	v1,3				/* Index into table. */
	daddu	v0,v1
	jalr	a2,v0
	nop
	MFC0(v0, C0_TAGLO)
	sw	v0,COP_TAGLO(a0)
	MFC0(v0, C0_TAGHI)
	sw	v0,COP_TAGHI(a0)
	MFC0(v0, C0_ECC)
	j	ra
	sw	v0,COP_ECC(a0)
	.set	reorder

        /*
         * There are 32 possible operations to perform, each is
	 * defined in the table below, and uses 2 instructions (8 bytes).
         */

cacheOP_table:
	.set	noreorder
	jr	a2
	cache	0, 0(a1)
	jr	a2
	cache	1, 0(a1)
	jr	a2
	cache	2, 0(a1)
	jr	a2
	cache	3, 0(a1)
	jr	a2
	cache	4, 0(a1)
	jr	a2
	cache	5, 0(a1)
	jr	a2
	cache	6, 0(a1)
	jr	a2
	cache	7, 0(a1)
	jr	a2
	cache	8, 0(a1)
	jr	a2
	cache	9, 0(a1)
	jr	a2
	cache	10, 0(a1)
	jr	a2
	cache	11, 0(a1)
	jr	a2
	cache	12, 0(a1)
	jr	a2
	cache	13, 0(a1)
	jr	a2
	cache	14, 0(a1)
	jr	a2
	cache	15, 0(a1)
	jr	a2
	cache	16, 0(a1)
	jr	a2
	cache	17, 0(a1)
	jr	a2
	cache	18, 0(a1)
	jr	a2
	cache	19, 0(a1)
	jr	a2
	cache	20, 0(a1)
	jr	a2
	cache	21, 0(a1)
	jr	a2
	cache	22, 0(a1)
	jr	a2
	cache	23, 0(a1)
	jr	a2
	cache	24, 0(a1)
	jr	a2
	cache	25, 0(a1)
	jr	a2
	cache	26, 0(a1)
	jr	a2
	cache	27, 0(a1)
	jr	a2
	cache	28, 0(a1)
	jr	a2
	cache	29, 0(a1)
	jr	a2
	cache	30, 0(a1)
	jr	a2
	cache	31, 0(a1)
	.set	reorder
	END(cacheOP)
#if 0	
LEAF(setupScache)
/*
 * Routine:	fillScache
 * Purpose:	Set up a specific region of the SCACHE with dirty
 *		exlusive tags.
 * Parameters:	a0 - physical address
 *		a1 - length (in bytes)
 * Returns:	nothing
 */
	/*
	 * Be sure to fill an entire s-line, otherwise, we could get
	 * errors on non-initialized parts of the sline since the T5
	 * likes to prefetch stuff.
	 */

	daddiu	t1, a1, CACHE_SLINE_SIZE-1
	and	t1, ~(CACHE_SLINE_SIZE - 1)	/* Mutiple of s-lines */
	divu	t3, t1, CACHE_SLINE_SIZE	/* # primary d-lines. */
	dand	a0,~(CACHE_SLINE_SIZE-1)+1	/* Align please - save way */

1:	
	/* Set physical address bits */

	dsrl	a2, t0, 4
	and	a2, CTS_TAG_MASK		/* Physical address bits */
		
	or	a2, 3 << CTS_STATE_SHFT

	jal	stagECC
	move	a0, a2				/* Compute ECC */ 
	or	a2,v0				/* Or in ECC */

	/* TAG is now in a2 with ECC set - write it and continue. */

	.set	noreorder
	MTC0(ta3, C0_TAGLO)
	dsrl	ta3, 32
	MTC0(ta3, C0_TAGHI)
	SCACHE(C_IST, 0(t8)) %%%
	.set	reorder

	sub	L,CACHE_SLINE_SIZE
	daddiu	A,CACHE_SLINE_SIZE
	bgt	L,zero,1b
	
	j	ra
	END(setupScache)

	
#endif
