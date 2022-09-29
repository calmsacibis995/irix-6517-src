#ident "$Revision: 1.27 $"

#include <sys/regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "pod.h"
#include "pod_failure.h"
#include "ip19prom.h"

#define WD_ALIGN_MASK   0x3

/*
 * return the size of the secondary cache as stored in the CPU EAROM
 */
LEAF(read_scachesize)
        li      t0, EV_CACHE_SZ_LOC
        ld      t0, 0(t0)       # Read the CPU board EAROM
        li      v0, 1
        beq     t0, v0, scsz_ret
        move    v0, zero        # A cache size of 1 means no scache
        li      v0, 1
        sll     v0, v0, t0      # Shift 1 left by a0 (cache size = 2^a0)
scsz_ret:
        j       ra
        END(read_scachesize)

LEAF(pon_flush_and_inval)
	move	Ra_save1,ra
	jal	pon_flush_dcache
	jal	pon_flush_scache
	jal	pon_invalidate_dcache
	jal	pon_invalidate_scache
	j	Ra_save1
	END(pon_flush_and_inval)

/*
 * return the size of the primary instruction cache
 */
LEAF(get_icachesize)
        .set    noreorder
        mfc0    t0,C0_CONFIG
	nop
        .set reorder
        and     t0,CONFIG_IC
        srl     t0,CONFIG_IC_SHFT
        addi    t0,MIN_CACH_POW2
        li      v0,1
        sll     v0,t0
        j       ra
        END(get_icachesize)

/*
 * return the size of the primary data cache
 */
LEAF(get_dcachesize)
        .set    noreorder
        mfc0    t0,C0_CONFIG
	nop
        .set reorder
        and     t0,CONFIG_DC
        srl     t0,CONFIG_DC_SHFT
        addi    t0,MIN_CACH_POW2
        li      v0,1
        sll     v0,t0
        j       ra
        END(get_dcachesize)

/*
 * Return the line size of the primary dcache
 */
LEAF(get_dcacheline)
        .set    noreorder
        mfc0    v0, C0_CONFIG
	nop
        .set reorder
        and     v0, CONFIG_DB
        beq     v0, zero, 1f
        li      v0, 32
        j       ra
1:
        li      v0, 16
        j       ra
        END(get_dcacheline)

/*
 * Return the line size of the primary icache
 */
LEAF(get_icacheline)
        .set    noreorder
        mfc0    v0, C0_CONFIG
	nop
        .set reorder
        and     v0, CONFIG_IB
        beq     v0, zero, 1f
        li      v0, 32
        j       ra
1:
        li      v0, 16
        j       ra
        END(get_icacheline)

/*
 * Return the line size of the secondary cache
 */
LEAF(get_scacheline)
	.set    noreorder
	mfc0    v1, C0_CONFIG
	nop
	.set reorder
	and	v0, v1, CONFIG_SC	# Test for scache present
	beq	v0, zero, 1f		# 0 -> cache present, 1 -> no scache
	move	v0, zero		# no scache
	j	ra
1:
	# second cache is present. figure out the block size
	and	v1, v1, CONFIG_SB	# pull out the bits
	srl	v1, CONFIG_SB_SHFT
	li      v0, 1
	addu	v1, MIN_BLKSIZE_SHFT	# turn 0,1,2,3 into 16,32,64,128
	sll	v0, v1
	j	ra
	END(get_scacheline)

/* pod_check_scache0 
 * a0 = number of iterations 
 * a1 = value to put in DE bit of SR 
 */
LEAF(pod_check_scache0)
	move	s3, a0		# Copy the argument

	bnez	a1, 1f

	.set	noreorder
	li	t0, ~SR_DE
	mfc0	t1, C0_SR
	nop; nop; nop; nop
	and	t0, t1		# Turn off DE bit (enable ECC exceptions) 
	mtc0	t0, C0_SR
	la	a0, ecc_enabled_msg
	.set reorder
	b	2f
1:
	.set	noreorder
	li	t0, SR_DE
	mfc0	t1, C0_SR
	nop; nop; nop; nop
	or	t0, t1		# Turn on DE bit (disable ECC exceptions)
	mtc0	t0, C0_SR
	la	a0, ecc_disabled_msg
	.set reorder
2:	jal	pod_puts

	la	a0, scache_size_message
	jal	pod_puts

	jal	read_scachesize

	move	a0, v0
	jal	pod_puthex

	la	a0, crlf
	jal	pod_puts

	.data
scache_size_message:
	.asciiz	"Scache size: 0x"
scache0_done_message:
	.asciiz	"\rScache0 test complete.                  \r\n"
ecc_enabled_msg:
	.asciiz	"ECC exceptions enabled\r\n"
ecc_disabled_msg:
	.asciiz	"ECC exceptions disabled\r\n"
scache_addr_pass:
	.asciiz "Completed scache address test pass\r\n"
scache_data_pass:
	.asciiz "Completed scache address test pass\r\n"
	.text

1:
	jal	pod_setup_scache

	jal	scache_addr

	bne	v0, zero, 2f

	la	a0, scache_addr_pass

	jal	pod_puts

	jal	scache_data

	bne	v0, zero, 2f

	la	a0, scache_data_pass

	jal	pod_puts

	add	s3, -1

	bnez	s3, 1b

	la	a0, scache0_done_message
	jal	pod_puts

	.set	noreorder
	li	a0, POD_SR
	mtc0	a0, C0_SR
	nop
	.set reorder

	li	a1, EVDIAG_RETURNING
	la	a0, returning_to_pod
	j	pod_handler	

2:
	.set	noreorder
	li	a0, POD_SR
	mtc0	a0, C0_SR
	nop
	.set reorder

	li	a1, EVDIAG_RETURNING
	la	a0, scache0_failed
	j	pod_handler	

	END(pod_check_scache0)

	.data
	.globl	returning_to_pod
returning_to_pod:
	.asciiz	"\r\nReentering POD mode.\r\n"
scache0_failed:
	.asciiz	"\r\n*** Scache0 test failed!  Reentering POD mode.\r\n"
	.text

/* pod_check_scache1 - Check the scache as memory.  Check writeback later. */
LEAF(pod_check_scache1)

	move	Ra_save0, ra

	jal	pod_setup_scache

#if 0	
	jal	scache_data

	bne	v0, zero, 1f
#endif
	# Jump and return the error.

	jal	scache_addr

	# Fall-through - return whatever it returned
1:
	j       Ra_save0

	END(pod_check_scache1)


/*
 * pod_check_pdcache1: fixes up cache, calls other cache tests.
 * Returns 0 on success and EVDIAG_DCACHE_DATA or EVDIAG_DCACHE_ADDR on failure.
 * A Level 0 routine, it calls pon_fix_dcache_parity,
 * dcache_data, and dcache_addr: all level 1 routines.
 */
LEAF(pod_check_pdcache1)
	move	Ra_save0, ra			# Save ra

	jal	pon_fix_dcache_parity		# Set up pdcache for test

	# run cache data test
	jal	dcache_data

	beq	v0, zero, 1f			# Did it pass?  If so, go on.

	j	Ra_save0

1:
	# Data test passed.  Try next test.
	# run cache address-pattern test

	jal	pon_fix_dcache_parity		# Set up pdcache for next test

	jal	dcache_addr


	# STEVE: Put primary tag wrap test here?

	# This should be enough to ensure a healthy cache to be
	# used as stack memory.  Return whatever dcache_addr returned in v0.

	move	ra, Ra_save0
	j	ra
	END(pod_check_pdcache1)

/*
 * Write interesting patterns to each word of cache, read back and
 * validate. On error, return an error code.  The PROM code will do
 * different things depending on whether this test is being done by
 * the master or a slave CPU.  The caller (pod_check_pdcache1) must
 * set up pd-cache state for this test, since pod_fix_dcache_parity
 * is a level-1 routine in calling chain!
 */
LEAF(scache_data)
	move	Ra_save1, ra

	jal	read_scachesize
	move	t0, v0				# get_dcacheline only uses v0
	jal	get_scacheline

	# now test the integrity of each cache line
	# t0: dcache size, v0: block size (t1: saved SR)
	li	t2, POD_SCACHEADDR
	addu	t0, t2				# loop termination
	li	t3, 0xaaaaaaaa			# distinctive pattern
1:
	move	t4, t2
	sw	t3, 0(t2)			# write to cache
	lw	t4, 0(t2)			# read from cache
	bne	t4, t3, 4f			# branch out on error
	addu	t2, 4				# next word
	bltu	t2, t0, 1b

	li	t2, POD_SCACHEADDR
	li	t3, 0x55555555			# flip previous pattern
1:
	move	t4, t2
	sw	t3, 0(t2)			# write to cache
	sw	t3, 0(t2)			# write to cache
	lw	t4, 0(t2)			# read from cache
	bne	t3, t4, 4f			# branch out on error
	addu	t2, 4				# next word
	bltu	t2, t0, 1b

	# done with no error
	move	v0, zero			# zero return == no error
	j	Ra_save1			# Saved return address

4:
	li	v0, EVDIAG_SCACHE_DATA		# Load return value
	j	Ra_save1		    # Jump to saved return address

	END(scache_data)


/*
 * Write interesting patterns to each word of cache, read back and
 * validate. On error, return an error code.  The PROM code will do
 * different things depending on whether this test is being done by
 * the master or a slave CPU.  The caller (pod_check_pdcache1) must
 * set up pd-cache state for this test, since pod_fix_dcache_parity
 * is a level-1 routine in calling chain!
 */
LEAF(dcache_data)
	move	Ra_save1, ra

	jal	get_dcachesize
	move	t0, v0				# get_dcacheline only uses v0
	jal	get_dcacheline

	.set	noreorder
	mfc0	t1, C0_SR			# save current sr
	li	t2, POD_SR
	mtc0	t2, C0_SR			# ignore cache errors.
	nop
	.set	reorder

	# now test the integrity of each cache line
	# t0: dcache size, v0: block size (t1: saved SR)
	li	t2, POD_STACKADDR
	addu	t0, t2				# loop termination
	li	t3, 0xaaaaaaaa			# distinctive pattern
1:
	sw	t3, 0(t2)			# write to cache
	lw	t4, 0(t2)			# read from cache
	bne	t4, t3, 4f			# branch out on error
	addu	t2, 4				# next word
	bltu	t2, t0, 1b

	li	t2, POD_STACKADDR
	li	t3, 0x55555555			# flip previous pattern
1:
	sw	t3, 0(t2)			# write to cache
	lw	t4, 0(t2)			# read from cache
	bne	t3, t4, 4f			# branch out on error
	addu	t2, 4				# next word
	bltu	t2, t0, 1b

	# done with no error
	.set	noreorder
	mtc0	t1, C0_SR
	nop
	.set	reorder
	move	v0, zero			# zero retn -> no error
	j	Ra_save1			# Saved return address

4:
	li  v0, EVDIAG_DCACHE_DATA
	j   Ra_save1			# Saved return address
	
	END(dcache_data)


/* Primary D-cache address-pattern test.  WARNING: Caller (pon_cache_test)
 * must initialize the pdcache for us; pon_fix_dcache_parity is at our 
 * same-level in calling chain: overwrites ra! */
LEAF(dcache_addr)
	move	Ra_save1, ra

	jal	get_dcachesize
	move	t0, v0				# get_dcacheline only uses v0
	jal	get_dcacheline

	.set	noreorder
	mfc0	t1, C0_SR			# save current sr
	li	t2, POD_SR
	mtc0	t2, C0_SR			# ignore cache errors.
	nop
	.set	reorder

	# now test each cache line
	# t0: dcache size, v0: block size (t1: saved SR)
	li	t2, POD_STACKADDR
	addu	t0, t2				# loop termination

1:
	move	t3, t2
	not	t3
	sw	t3, 0(t2)			# write cache
	addu	t2, 4				# next word
	bltu	t2, t0, 1b

	# now verify the written data
	li	t2, POD_STACKADDR
1:
	lw	t3, 0(t2)
	move	t4, t2
	not 	t4
	bne	t3, t4, 4f			# branch out on error
	addu	t2, 4
	bltu	t2, t0, 1b
	b	2f

	# now test each cache line again without inverting the address
	# t0: dcache size, v0: block size (t1: saved SR)
	li	t2, POD_STACKADDR
	addu	t0, t2				# loop termination

1:
	move	t3, t2
	sw	t3, 0(t2)			# write cache
	addu	t2, 4				# next word
	bltu	t2, t0, 1b

	# now verify the written data
	li	t2, POD_STACKADDR
1:
	lw	t3, 0(t2)
	move	t4, t2
	bne	t3, t4, 4f			# branch out on error
	addu	t2, 4
	bltu	t2, t0, 1b

2:
	# done with no error
	.set	noreorder
	mtc0	t1, C0_SR
	nop
	.set	reorder
	move	v0, zero			# zero retn -> no error
	j	Ra_save1			# Jump to saved ra

4:
#if 0
	# STEVE- decide how to report the error later.

	# report error
	move	a1, t3				# expected
	move	a2, t4				# actual
	move	a3, t2				# addr
	PRINT()
	.set 	noreorder
	cache	CACH_PD|C_ILT, 0(a3)		# fetch badaddr line's taglo
	nop
	nop
	nop
	mfc0	a3, C0_TAGLO
	.set 	reorder
	la	a0, pdctag
	jal	pod_puts
	move	a0, a3
	jal	pod_puthex

	li	a0, PON_DCACHE_ADDR
	jal	flash_cc_leds			# doesn't return
#endif /* 0 */
	li	v0, EVDIAG_DCACHE_ADDR
	j	Ra_save1		    # Jump to saved return address
	END(dcache_addr)

LEAF(clear_scache)
	move	Ra_save2, ra

	jal	read_scachesize

	.set	noreorder
	li	t0, SR_DE
	mfc0	t1, C0_SR
	nop; nop; nop; nop
	or	t0, t1		# Turn on DE bit (disable ECC exceptions)
	mtc0	t0, C0_SR
	nop
	.set	reorder

	li	t2, POD_SCACHEADDR	# Scache start addr.
	add	t3, v0, t2		# Loop terminator
1:
	sd	zero, 0(t2)		# Store a nice zero
	addi	t2, 8			# Increment by double words
	bne	t2, t3, 1b	

	# Do the beginning again to make sure it gets out of the dcache
	
	jal	get_dcachesize

	li	t2, POD_SCACHEADDR	# Scache start addr.
	add	t3, v0, t2		# Loop terminator
1:
	sd	zero, 0(t2)		# Store a nice zero
	addi	t2, 8			# Increment by double words
	bne	t2, t3, 1b	
	

	.set	noreorder
	mtc0	t1, C0_SR	# restore the SR
	nop
	.set	reorder

	j	Ra_save2

	END(clear_scache)

/*

/* Secondary D-cache address-pattern test.  WARNING: Caller (pon_cache_test)
 * must initialize the sdcache for us; pod_setup_scache is at our 
 * same-level in calling chain: overwrites ra! */
LEAF(scache_addr)
	move	Ra_save1, ra

	jal	read_scachesize
	move	t0, v0				# read_scachesize only uses v0, a0
	jal	get_scacheline

	.set	reorder

	# now test each cache line
	# t0: scache size, v0: block size (t1: saved SR)
	li	t2, POD_SCACHEADDR
	addu	t0, t2				# loop termination

1:
	move	t3, t2

	not	t3				# Invert address bits
	sw	t3, 0(t2)			# write cache
	addu	t2, 4				# next word
	bltu	t2, t0, 1b

	# now verify the written data
	li	t2, POD_SCACHEADDR
1:
	lw	t3, 0(t2)
	lw	t3, 0(t2)
	move	t4, t2
	not 	t4
	bne	t3, t4, 4f			# branch out on error
	addu	t2, 4
	bltu	t2, t0, 1b

	# done with no error
	move	v0, zero			# zero retn -> no error
	j	Ra_save1			# Jump to saved ra

4:
	# report error
	li	v0, EVDIAG_SCACHE_ADDR
	j	Ra_save1		    # Jump to saved return address
	END(scache_addr)

/*
 * pon_fix_dcache_parity()
 * put dcache into useful state for power-on diagnostics or
 * for running pon command monitor.
 *	all Secondary tags are set to invalid.
 *	all PD tags have correct parity.
 *	all PD lines marked 'dirty exclusive'
 *	all PD data is initialized with correct parity.
 * Do so without causing bus cycles/writebacks to scache.
 * DE bit must be set when writing data; ECC errors from the
 *   previous bogus data are possible.
 * Does not use a stack. Cannot be called from C, since it does
 *   not respect register conventions.
 * Note that this routine is level-1 in the calling chain.
 */
LEAF(pon_fix_dcache_parity)
	move	Ra_save1,ra

	# mark each line 'dirty_exclusive' using R4K store-tag
	# cacheops to prevent interaction with 2ndary
	jal	pon_validate_dcache

	# clear 2ndry tags in case of "quiet" misses
	# of pdcache due to parity/ecc err (READ R4000 BUG)
	jal	pon_invalidate_scache

	# now, since all lines valid and dirty (ensuring no secondary
	# fetches as long as we hit primary-d), initialize the data 
	# in order calculate correct parity and set the WB bit for each
	# line (to prevent anomalous R4K behavior, not to cause eventual
	# writeback).  First, get dcache size.
	# Because the data is bogus, set SR to ignore parity/ecc errors.

	jal	get_dcachesize

	.set	noreorder
	mfc0	t3, C0_SR
	li	t1, POD_SR
	mtc0	t1, C0_SR
	nop
	.set	reorder

	# store to each line, t2 has loop termination
	li	t1, POD_STACKADDR
	addu	t2, t1, v0			# t2 is loop termination
						# (stack address + length)
1:
	sd	zero,0(t1)
	addi	t1, 8				# increment to next doubleword
	bltu	t1, t2, 1b

	.set	noreorder
	mtc0	t3, C0_SR
	nop
	.set	reorder
	j	Ra_save1			# Jump to saved return address
	END(pon_fix_dcache_parity)


LEAF(pod_setup_scache)
	move	Ra_save1,ra
	jal	pon_invalidate_scache		# Clear out scache tags
	jal	pon_validate_dcache		# Set up scache DEx at POD_STACKADDR
	jal	pon_validate_scache		# Set up scache DEx at POD_SCACHEADDR
	jal	clear_scache			# Clear the scache (nice ECC)
	j	Ra_save1
	END(pod_setup_scache)

/*
 * pon_validate_scache(): validate all 
 * 2ndary-cache lines by storing taglo
 * values.  This is a Level 2 routine
 * Must call pon_invalidate_scache first! 
 * (To avoid writebacks)
 * Also invalidate the primary dcache
 */

LEAF(pon_validate_scache)
	move	Ra_save2,ra		# Save away return address

	jal	read_scachesize		# Get dcache size from EAROM
	move	t0,v0

	jal	get_scacheline		# get_scacheline uses v0 and v1

	# t0: scache size, v0: sline size
	# use K0 addrs beginning at POD_SCACHEADDR for CDX cacheop
	#				    (CDX = create dirty exclusive)
	li	t1, POD_SCACHEADDR
	addu	t2, t1, t0		# t2: loop terminator

	# for each scache line write an appropriate dirty exclusive tag
1:
	.set	noreorder
#if 0
	cache	CACH_SD|C_CDX, 0(t1)	# Create dirty exclusive cache line
	nop; nop; nop; nop
	nop; nop; nop; nop
#else
	# t1 contains the current cache line address
	# t3 contains K0TOPHYS(t1)
	# t4 will be the cache tag

	li	t3, 0x1fffffff		# K0TOPHYS mask
	and	t3, t1, t3		# Now a physcial address

	srl	t4, t3, 12		# Put 14..12 in 2..0
	andi	t4, 0x07		# Mask off other bits
	sll	t4, t4, 7		# Put bits in 9..7
	
	srl	t5, t3, 17		# Put bits 35..17 in 18..0
	sll	t5, t5, 13		# Put them in 31..13

	or	t4, t4, t5		# OR them into the tag

	ori	t4, 5 << 10		# OR in dirty exclusive state

	mtc0	t4, C0_TAGLO		# Put the new tag in TAGLO

	cache	CACH_SD|C_IST, 0(t1)	# Store it!

	nop; nop; nop; nop

#endif
	.set	reorder
	addu	t1, v0			# increment to next line
	bltu	t1, t2, 1b

	j	Ra_save2		# Jump to saved return address

	END(pon_validate_scache)
/*
 * pon_invalidate_scache(): invalidate all 
 * 2ndary-cache lines by storing 0 taglo
 * values.  This is a Level 2 routine
 */
LEAF(pon_invalidate_scache)
	move	t3,ra		# Save away return address

	jal	read_scachesize		# Get dcache size from EAROM
	move	t0,v0			# get_dcacheline only uses v0
	jal	get_scacheline		# get_scacheline uses v0 and v1

	.set	noreorder
	mtc0	zero, C0_TAGLO		# zero taglo == invalid tag
	nop
	.set	reorder

	# t0: scache size, v0: sline size
	# use K0 addrs beginning at POD_STACKADDR for IST cacheop
	#				    (IST == Index Store Tag)
	li	t1, POD_STACKADDR
	addu	t2, t1, t0		# t2: loop terminator

	# for each scache line write zero taglo (taghi is reserved for R4000)
1:
	.set	noreorder
	cache	CACH_SD|C_IST, 0(t1)
	NOP_0_4
	.set	reorder
	addu	t1, v0			# increment to next line
	bltu	t1, t2, 1b

	j	t3		# Jump to saved return address

	END(pon_invalidate_scache)


#define PTAG_LO_MASK    0x7ffff000      /* paddr 30..12 */
					/* Should be a physical address */
LEAF(pon_flush_scache)
	move	Ra_save2, ra
	jal	read_scachesize
	move	t0, v0			# get_scacheline only uses v0
	jal	get_scacheline

	# t0: cache size, v0: line size
	# POD_STACKADDR is largest cache size aligned (32k aligned)
	li	t1, POD_SCACHEADDR
	addu	t2, t1, t0		# t2: loop terminator

	# for each cache line:
	#	writeback and invalidate
1:
	.set	noreorder
	cache	CACH_SD|C_IWBINV, 0(t1)
	NOP_0_4
	.set	reorder
        addu	t1, v0			# increment to next line
	bltu	t1, t2, 1b

	move	ra,Ra_save2
	j	ra
	END(pon_flush_scache)


LEAF(pon_flush_dcache)
	move	Ra_save2, ra
	jal	get_dcachesize
	move	t0, v0			# get_dcacheline only uses v0
	jal	get_dcacheline

	# t0: cache size, v0: line size
	# POD_STACKADDR is largest cache size aligned (32k aligned)
	li	t1, POD_STACKADDR
	addu	t2, t1, t0		# t2: loop terminator

	# for each cache line:
	#	writeback and invalidate
1:
	.set	noreorder
	cache	CACH_PD|C_IWBINV, 0(t1)
	NOP_0_4
	.set	reorder
	addu	t1, v0			# increment to next line
	bltu	t1, t2, 1b

	move	ra,Ra_save2
	j	ra
	END(pon_flush_dcache)

/* pod_zero_icache(): Zero the instruction cache.
 * A Level 1 routine called by POD stuff and possible run cached stuff.
 * Calls pon_flush_dcache
 * Parameter:
 *	a0 = address at which to write the zeroes.
 * Returns:
 *	void
 */
LEAF(pon_zero_icache)
	.set noreorder

	move	Ra_save1, ra
	
	mfc0	s5, C0_SR
	nop

	li	t0, SR_DE
	or	t0, s5

	mtc0	t0, C0_SR
	nop

	jal	get_icachesize
	move	t1, a0			# (BD) get_icachesize doesn't use t1

	# v0: cache size
	# a0: address at which to load zeroes
	addu	t2, t1, v0		# t2: loop terminator

	# Zero an icache's space in the scache
1:
	sd	zero, 0(t1)		# Clear doubleword
	addiu	t1, 8			# Next doubleword (BD)
	bltu	t1, t2, 1b
	nop				# (BD)

	# Flush the dcache
	jal	pon_flush_dcache
	nop

	jal	get_icachesize
	nop
	jal	get_icacheline
	move	t0, v0			# get_dcacheline only uses v0 (BD)

	# t0 = icache size, v0 = icache linesize
	# Fill the icache line by line
	addu	t2, a0, t0		# t2: loop terminator
1:
	cache	C_FILL|CACH_PI, 0(a0)
	NOP_0_4

	addu	a0, v0			# increment to next line
	bltu	a0, t2, 1b
	nop				# (BD)

	mtc0	s5, C0_SR

	move	ra,Ra_save1
	j	ra
	nop
	END(pon_zero_icache)


/* pon_validate_dcache(): set all PD lines to dirty exclusive
 * state, with addrs beginning at POD_STACKADDR and rising
 * contiguously for 'dcachesize'.  All further cached-accesses
 * within this range will now hit, eliminating any interaction
 * with 2ndary and memory.
 */
#define PTAG_LO_MASK    0x7ffff000      /* paddr 30..12 */
					/* Should be a physical address */
LEAF(pon_validate_dcache)
	.set	reorder
	move	Ra_save2, ra
	jal	get_dcachesize
	move	t0, v0			# get_dcacheline only uses v0
	jal	get_dcacheline

	# t0: cache size, v0: line size
	# POD_STACKADDR is largest cache size aligned (32k aligned)
	li	t1, POD_STACKADDR
	addu	t2, t1, t0		# t2: loop terminator

	# for each cache line:
	#	mark line dirty exclusive
1:
	and	t0, t1, PTAG_LO_MASK	# grab taglo addr bits from caddr
	srl	t0, PADDR_SHIFT	
	or	t0, PDIRTYEXCL		# make tag-state dirty exclusive
	.set	noreorder
	mtc0	t0, C0_TAGLO
	NOP_0_4				# give it time to get through pipe
	cache	CACH_PD|C_IST, 0(t1)	# store tag in PD cache 
	NOP_0_4
	.set	reorder
	addu	t1, v0			# increment to next line
	bltu	t1, t2, 1b

	move	ra,Ra_save2
	j	ra
	END(pon_validate_dcache)


LEAF(pon_invalidate_dcache)
	move	t3, ra
	jal	get_dcachesize
	move	t0, v0			# get_dcacheline only uses v0
	jal	get_dcacheline

	# t0: cache size, v0: line size
	# POD_STACKADDR is largest cache size aligned (32k aligned)
	li	t1, POD_STACKADDR
	addu	t2, t1, t0		# t2: loop terminator

	# for each cache line:
	#	mark line dirty exclusive
1:
	.set	noreorder
	mtc0	zero, C0_TAGLO
	NOP_0_4				# give it time to get through pipe
	cache	CACH_PD|C_IST, 0(t1)	# store tag in PD cache 
	NOP_0_4
	.set	reorder
	addu	t1, v0			# increment to next line
	bltu	t1, t2, 1b

	j	t3
	END(pon_invalidate_dcache)


LEAF(pon_invalidate_icache)
	move	Ra_save2, ra
	jal	get_icachesize
	move	t0, v0			# get_dcacheline only uses v0
	jal	get_icacheline

	# t0: cache size, v0: line size
	# POD_STACKADDR is largest cache size aligned (32k aligned)
	li	t1, POD_STACKADDR
	addu	t2, t1, t0		# t2: loop terminator

	# for each cache line:
	#	mark line dirty exclusive
1:
	.set	noreorder
	mtc0	zero, C0_TAGLO
	NOP_0_4				# give it time to get through pipe
	cache	CACH_PI|C_IST, 0(t1)	# store tag in PI cache 
	NOP_0_4
	.set	reorder
	addu	t1, v0			# increment to next line
	bltu	t1, t2, 1b

	move	ra,Ra_save2
	j	ra
	END(pon_invalidate_icache)

/*
 * fill_ipline(caddr)
 * Fill Primary Instruction Cache line via k0seg
 * addr should always be in k0seg 
 */
LEAF(fill_ipline)
	.set	noreorder
	cache	CACH_PI|C_FILL, 0(a0)
	.set	reorder

	j	ra
	END(fill_ipline)


/*
 * write_ipline(caddr)
 * Write Primary Instruction Cache line back to secondary
 * addr should always be in k0seg 
 */
LEAF(write_ipline)
	.set	noreorder
	cache	CACH_PI|C_HWB, 0(a0)	# writeback the line
	NOP_0_1
	.set	reorder
	j	ra
	END(write_ipline)

/* pd_hwbinv(caddr): Primary Data cache Hit Writeback Invalidate.
 * a0: K0-seg virtual address */
LEAF(pd_hwbinv)
	li	t0, WD_ALIGN_MASK	# cacheops must be wd aligned
	not	t0
	and	a0, a0, t0
	.set	noreorder
	cache	CACH_PD|C_HWBINV, 0(a0)
	nop
	nop
	nop
	.set	reorder
	j	ra
	END(pd_hwbinv)

/* _read_tag(WhichCache, address, &destination)
 * WhichCache == CACH_PI, CACH_PD, CACH_SI, or CACH_SD.
 * address may be in KUSER or K0SEG space.
 * destination is a pointer to two uints.
 * a0: WhichCache
 * a1: address
 * a2: destination ptr
 * returns value of C0_ECC
 */
LEAF(_read_tag)
	li	t0, WD_ALIGN_MASK	# cacheops must be wd aligned
	not	t0
	and	a1, a1, t0
	li	t0, CACH_PD
	beq	a0, t0, rprimd	# primary d-cache tag
	bltu	a0, t0, rprimi	# fetch primary i-cache tag
	.set	noreorder
	cache	CACH_SD|C_ILT, 0(a1)	# fetch secondary (S|D) cache tag
	.set	reorder

getout:	
	.set	noreorder
	nop
	nop
	nop
	nop
	mfc0	t0, C0_TAGLO
	# DO NOT READ C0_TAGHI IN CURRENT IMPLEMENTATION OF R4K!
	nop
	nop
	mfc0	v0, C0_ECC
	nop
	nop
	nop
	sw	t0, 0(a2)	# taglo is 1st uint in tag struct
	# sw	t1, 4(a2)	# taghi is 2nd
	.set	reorder
	j	ra

rprimi:	.set	noreorder
	cache	CACH_PI|C_ILT, 0(a1)	# fetch primary instruction cache tag
	.set	reorder
	b	getout

rprimd:	.set	noreorder
	cache	CACH_PD|C_ILT, 0(a1)	# fetch primary data cache tag
	.set	reorder
	b	getout

	END(_read_tag)


/*
 * _write_tag(WhichCache, address, &srcstruct)
 * a0: WhichCache == CACH_PD, CACH_PI, or CACH_SD
 * a1: write tag which contains this addr
 * a2: srcstruct is ptr to two uints.
 */

LEAF(_write_tag)
	li	t0, WD_ALIGN_MASK	# cacheops must be wd aligned
	not	t0
	and	a1, a1, t0
	lw	t0, 0(a2)	# a2 pts to taglo: 1st uint in tag struct
	# lw	t1, 4(a2)	# taghi is 2nd

	.set	noreorder
	mtc0	t0,C0_TAGLO
	# DO NOT WRITE C0_TAGHI IN CURRENT IMPLEMENTATION OF R4K!
	li	v0, 0		# success by default
	.set	reorder		# this is at least 3 clocks

	li	t0, CACH_PD
	beq	a0, t0, wprimd	# set primary data cache tag?
	bltu	a0, t0, wprimi	# set primary instruction cache tag?
	.set	noreorder
	nop
	nop
	cache	CACH_SD|C_IST, 0(a1)	# no, set secondary cache tag
	nop
	nop
	.set	reorder

setout: j	ra

wprimi:	.set	noreorder
	cache	CACH_PI|C_IST, 0(a1)	# set primary instruction cache tag
	.set	reorder
	b	setout

wprimd:	.set	noreorder
	cache	CACH_PD|C_IST, 0(a1)	# set primary data cache tag
	.set	reorder
	b	setout

	END(_write_tag)

