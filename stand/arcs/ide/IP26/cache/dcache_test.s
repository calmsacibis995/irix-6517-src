/* dcache1
 *
 * This diag tests to make sure that the valid ram in the TFP IU dcache
 * is working correctly.  First the valid ram are tested directly by
 * performing dctw and dctr instructions.  Then they are tested again
 * by loading from one region of memory cached, and then a different
 * region uncached.  At the end, all the tags in the dcache should be
 * invalid. 
 *
 * Ported from MTI dcache1.p dk test.  Tests the IU on-chip DCache.
 *
 * Vswami knows about this -- it came from /hosts/thrill.mti/x/lab/dcache1
 */
#ident "$Revision: 1.4 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/regdef.h>
#include <asm.h>

/* Write data cache tag.
 */
#define dcache_write(addr,value)		\
	dmtc0	addr,C0_BADVADDR;		\
	ssnop;					\
	ssnop;					\
	ssnop;					\
	dmtc0	value,C0_DCACHE;		\
	ssnop;					\
	ssnop;					\
	ssnop;					\
	dctw;					\
	ssnop;					\
	ssnop;					\
	ssnop

/* Read data cache tag.
 */
#define dcache_read(addr,dst)			\
	dmtc0	addr,C0_BADVADDR;		\
	ssnop;					\
	ssnop;					\
	ssnop;					\
	dctr;					\
	ssnop;					\
	ssnop;					\
	ssnop;					\
	dmfc0	dst,C0_DCACHE;			\
	ssnop;					\
	ssnop;					\
	ssnop

/* CPU speed dependent speed delay.
 */
#define delay(sec)				\
	.align	4;				\
	li	v1,sec*20000000;		\
22:	daddi	v1,-1;				\
	bgtz	v1,22b;				\
	nada

/* test_dcache_tags -- test writing/reading dcache tags.
 */
#define DCACHE_SIZE	0x4000
LEAF(test_dcache_tags)
	.align	7				# cache line aligned
	.set	noreorder
	li	t0,0x4000			# dcache size
	move	v0,zero				# error count
	move	t1,zero				# primed == false

3:
	/* Write then read pattern DCACHE_VMASK (valid bits)
	 */
	dli	a1,DCACHE_VMASK			# first pattern
	move	a0,zero				# initial address
1:	dcache_write(a0,a1)			# write tag
	daddiu	a0,8				# bump one primary cache line
	blt	a0,t0,1b			# loop through full cache
 	nop					# BDSLOT

	beqz	t1,1f				# do not delay on prime
	nada					# BDSLOT
	delay(4)

1:	move	a0,zero				# initial address
1:	dcache_read(a0,a2)			# read tag
	beq	a1,a2,2f			# miscompare?
	nop					# BDSLOT
	bnez	t2,2f				# do not fail on prime
	nop					# BDSLOT
	daddiu	v0,1				# bump error count
2:	daddiu	a0,16				# bump one primary cache line
	blt	a0,t0,1b
	nop

	/* Write then read pattern 0.
	 */
	move	a1,zero				# second pattern
	move	a0,zero				# inital address
1:	dcache_write(a0,a1);			# write tag
	daddiu	a0,16				# bump one primary cache line
	blt	a0,t0,1b			# loop through full cache
	nop					# BDSLOT

	beqz	t1,1f				# do not delay on prime
	nada					# BDSLOT
	delay(4)

1:	move a0,zero				# initial address
1:	dcache_read(a0,a2)			# read tag
	beq	a1,a2,2f			# miscompare?
	nop					# BDSLOT
	bnez	t2,2f				# do not fail on prime
	nop					# BDSLOT
	daddiu	v0,0x1000			# bump error count
2:	daddiu	a0,16				# bump one primary cache line
	blt	a0,t0,1b			# loop through full cache
	nop

	beqz	t1,3b				# loop again after prime
	addi	t1,1				# BDSLOT: primed == true

	j	ra				# return
	nada
	.set	reorder
	END(test_dcache_tags)

#define	TEST_ADDRESS	0x08c00000
LEAF(test_dcache_ram)
	.set	noreorder
	move	v0,zero					# init error count

	/* clear_uncached_region
	 */
	dli	a0,PHYS_TO_K1(TEST_ADDRESS)		# address
	daddiu	a1,a0,DCACHE_SIZE			# ending address
1:	sw	zero,0(a0)				# zero word
	daddiu	a0,4					# next word
	bltu	a0,a1,1b				# loop
	nop						# BDSLOT

	/* store_cached_data
	 */
	dli	a0,PHYS_TO_K0(TEST_ADDRESS-DCACHE_SIZE)	# address
	daddiu	a1,a0,DCACHE_SIZE			# ending address
	li	a2,0xffffffff				# test data
1:	sw	a2,0(a0)				# store word
	daddiu	a0,4					# next word
	bltu	a0,a1,1b				# loop
	nop						# BDSLOT

	delay(4)

	/* load_uncached_data
	 */
	dli	a0,PHYS_TO_K1(TEST_ADDRESS)		# address
	daddiu	a1,a0,DCACHE_SIZE			# ending address
1:	lw	a3,0(a0)				# load word
	beq	a3,zero,2f				# mismatch?
	nop						# bdslot
	daddiu	v0,0x1000				# incr error count
2:	daddiu	a0,4					# next word
	bltu	a0,a1,1b				# loop
	nop						# BDSLOT

	delay(4)

	/* load_cached_data
	 */
	dli	a0,PHYS_TO_K0(TEST_ADDRESS-DCACHE_SIZE)	# address
	daddiu	a1,a0,DCACHE_SIZE			# ending address
	dli	a2,0xffffffffffffffff			# test data
1:	lw	a3,0(a0)				# load word
	beq	a3,a2,2f				# mismatch?
	nop						# BDSLOT
	daddiu	v0,1					# incr error count
2:	daddu	a0,4					# next word
	bltu	a0,a1,1b				# bdslot
	nop						# BDSLOT

	j	ra
	nop
	.set	reorder
	END(test_dcache_ram)
