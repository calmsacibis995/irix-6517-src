/*
 * IP26prom/cache.s -- prom cache initialization.
 *
 */

#ident "$Revision: 1.14 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/regdef.h>
#include <asm.h>

/*
 * check_scachesize
 * return the size of the secondary cache
 *
 * Assume the gcache tag ram size options are 1,2,4,8,& 16 MB.
 * By loading the following data in the order shown:
 *
 *       Write   Read    Read    Read    Read    Read
 * Addr:   Data:   16MB    8MB     4MB     2MB     1MB
 * index
 *    0       f     f        7       3       1       0
 *  512       e     e        6       2       0       0
 * 1024       d     d        5       1       1       0
 * 1536       c     c        4       0       0       0
 * 2048       b     b        3       3       1       0
 * 2560       a     a        2       2       0       0
 * 3072       9     9        1       1       1       0
 * 3584       8     8        0       0       0       0
 * 4096       7     7        7       3       1       0
 * 4608       6     6        6       2       0       0
 * 5120       5     5        5       1       1       0
 * 5632       4     4        4       0       0       0
 * 6144       3     3        3       3       1       0
 * 6656       2     2        2       2       0       0
 * 7168       1     1        1       1       1       0
 * 7680       0     0        0       0       0       0
 *
 * If the data is then read back (after all data has been written) it
 * should match one of the patterns in the five read columns.  The
 * column it matches gives the size of the cache.
 * we need to check the bus tag ram, odd tag ram and even tag ram
 * and make sure they are consistent.
 *
 * v0: return scache size
 */

LEAF(check_scachesize)
#define HARDCODE_GSIZE			/* XXX -- try realsizing later */
#ifdef HARDCODE_GSIZE
	li	v0,2			# 2MB teton GCache
	j	ra
#else
	.set	noreorder

	/*  Read from a uncached addr --  The IP21 prom does this, presumably
	 * to work around some bug?
	 */
	dli	t0,PHYS_TO_K1(TCC_FIFO)
	ld	t1,0(t0)
	ld	t1,0(t0)

	/* Max GCache tag ram size is 16MB.
	 */
	li	t0,0x1fff		# 8192 entries (16MB)
	li	t1,0			# t1 indicates which index
	li	t2,512			# 512 entries for one value
	li	t3,16			# 16 possible values
1:

	/* Set-up addresses
	 */
	dli	v0,TCC_ETAG_DA		# tag data base
	dli	v1,TCC_ETAG_ST		# tag state base

	/*  Default tag state -- set dirty enable, virtual synonym, and state
	 * to valid.
	 */
	dli	s2,GCACHE_VSYN_E | GCACHE_STATE_EN | \
		(GCACHE_VALID << GCACHE_STATE_SHIFT)

	/* Load tag value.  Mask tag to ensure we use tag 0.
	 */
	dla	s0,tag_value
	lw	s1,0(s0)

2:
	/*  Calculate address for set 0 -- shift index and combine it with
	 * the base address.
	 */
	sll	ta2,t1, TAGADDR_INDEX_SHIFT
	or	ta1,v0,ta2		# address of proc tag addr.
	or	ta3,v1,ta2		# address of proc tag state

        /* Write data and state
	 */
	sd	s1,0(ta1)		# write into proc tag addr.
	sd	s2,0(ta3)		# write into proc tag state

	/* Bump index value and loop.
	 */
	subu	t2,1
	daddu	t1,1
	bnez	t2,2b
	nop

	/*  Re-load tag value with next word.
	 */
	daddiu	s0,4
	lw	s1,0(s0)

	/* Re-init loop control and load next value.
	 */
	li	t2,512
	subu	t3,1
	bnez	t3,2b
	nop

        /* Now read back the data and compare the value for addr index 0
         *	if read back 0xf, report 16mb
         *	if read back 0x7, report 8mb
         *	if read back 0x3, report 4mb
         *	if read back 0x1, report 2mb
         *	if read back 0x0, report 1mb
	 */

	dli	t0,TCC_ETAG_DA		# read back from even tag ram
	ld	a0,0(t0)
	srl	a0,20
	daddiu	a0,1			# gcache even tag ram size

	dli	t0,TCC_OTAG_DA		# read back from odd tag ram
	ld	a1,0(t0)
	srl	a1,20
	daddiu	a1,1			# gcache odd tag ram size

	/*  Compare tag ram sizes.  They must be the same, if not report
	 * an error by blinking the led.
	 */
	bne	a0,a1,error
	nop

        /* Check scache tag size is 1,2,4,8, or 16 only -- if we do not
	 * get a match, report an error by blinking the led.
	 */
	dla	t0,scache_tag_size
	li	t2,5
20:
	lw	t1,0(t0)
	bne	a1,t1,30f
	nop
	move	v0,a0			# return cache size in MB
	j	ra			# hit -> return to caller
	nop
30:
	daddiu	t0,4			# get next value
	subu	t2,1
	bnez	t2,20b			# try again
	nop

	/* no match falls through to error
	 */
error:
	j	memflash
	li	a0,1			# very fast flash
#endif
END(check_scachesize)

#if 0
	.data
tag_value:
        .word   0xf00000,0xe00000,0xd00000,0xc00000,0xb00000,0xa00000,0x900000
	.word	0x800000,0x700000,0x600000,0x500000,0x400000, 0x300000,0x200000
	.word   0x100000,0
scache_tag_size:
        .word   1,2,4,8,16
	.text
#endif

/* Initialize Store Address Queue by issuing (SAQ_DEPTH) Even and
 * (SAQ_DEPTH) Odd uncached-writes to two even and odd aligned
 * local registers - two undefined TCC registers are used.
 */
LEAF(flush_SAQueue)
	.set	noreorder
	dli	t0,PHYS_TO_K1(SAQ_INIT_ADDRESS)
	li	t1,SAQ_DEPTH		# number of entries in the queue
1:
	sd	zero,0(t0)		# write (even) 8 bytes
	sd	zero,8(t0)		# write (odd) 8 bytes
	addi	t1,-1
	bnez	t1,1b
	nop
	j       ra
	nop
	.set	reorder
END(flush_SAQueue)

/*  Invalidate I and D caches by running code that invalidates the I cache
 * and invalidate D cache by writing tags.
 *
 * Initialize DCache by invalidating it.  DCache is 16KB, with 32Byte line, 
 * 28 bits Tag + 1 bit Exclusive + 8 bits Valid + 32 Bytes Data
 * Need two consecutive writes to half-line boundaries to clear Valid bits
 * for each line.
 */
LEAF(pon_invalidate_IDcaches)
	.set	noreorder
	dmfc0	t9,C0_CONFIG
	.set	reorder

	and	t0,t9, CONFIG_DC		# calc DCache size
	srl	t0,CONFIG_DC_SHFT
	addi	t0,12
	li	s0,1
        sll     s0,t0

	and	t0,t9, CONFIG_IC		# calc ICache size
	srl	t0,CONFIG_IC_SHFT
	addi	t0,11
	li	a1,1
	sll	a1,t0

	and     t1,t9,CONFIG_IB			# convert to lines for flush
	beq     t1,zero,1f
	srl	a1,4				# 16 byte line size
	b	2f
1:
        srl	a1, 5				# 32 byte line size
2:

	# Saved registers before __tfp_icache_flush_text call (only touches a1)
	#	s0 - dcache size
	#	s1 - ra

	move	s1,ra				# save ra
	jal	__tfp_icache_flush_text		# flush icache by running code
	move	ra,s1				# restore ra

	dli	t1,PHYS_TO_K0(0x19000000)	# Starting address for init
	daddu	t2,t1,s0			# t2 is loop terminator

	# Now mark each 1/2 cache line invalid.
1:
	.align	3			# 1/2 quad align the loop
	.set	noreorder
	dmtc0	t1,C0_BADVADDR
	ssnop
	dmtc0	zero,C0_DCACHE		# clear all (4) Valid bits
	ssnop
	ssnop
	ssnop
	dctw				# write to the Dcache Tag 
	ssnop
	ssnop
	.set	reorder

	daddiu	t1,16			# increment to next half-line
	bltu	t1,t2,1b		# loop till thru whole cache

	j	ra
	END(pon_invalidate_IDcaches)
