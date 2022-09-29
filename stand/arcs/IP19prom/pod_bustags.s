/*
 * pod_cc.s -
 *      POD CPU board tests.
 */
#define NEWTSIM

#include <regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "pod.h"
#include "pod_failure.h"

	.set	reorder

/* This guy's a "level zero routine" */
/* Check and clear the bus tags. */
LEAF(pod_check_bustags)

	move	Ra_save0, ra

	.set	noreorder
	mfc0	v1, C0_CONFIG
	.set	reorder

	/* read_scachesize */
	jal	read_scachesize

	# vo = cachesize in bytes

        # Figure out the scache block size
        and	v1, v1, CONFIG_SB	# Pull out the bits
        srl	v1, CONFIG_SB_SHFT
        addu	v1, 4			# Turn 0,1,2,3 into 4,5,6,7

	srl	t1, v0, v1		# Divide scache size by line size.
	sll	t1, 3			# Four bytes per doubleword

	/* Caclulate the size of bus tag RAM 
	 * 2^18 = 256k is the smallest possible cache.
	 * It requires 24 bits of bustags.
	 * There's a tag for each scache line.
	 * 	t1 = size of tag RAM space
	 */

	# vo still = cachesize in bytes

	move	t0, zero
	srl	v0, 19
1:	
	sll	t0, 1
	srl	v0, 1
	ori	t0, 1
	bnez	v0, 1b

	not	t0		# t0 = mask for bus tag testing
	li	t2, 0x00ffffff	# 24 bits of 1s
	and	t0, t2		# Proper mask

	/* Bus tags are 24 bits aligned at the top of a word.
	 *	Tag RAM space size == (SCache_size >> 7) * 4
	 */

	/* Test As and 5s for adjacent bits stuck together. */

	# now test each bus tag
	# t1: tag RAM size
	# t0: tag RAM mask

	li      t2, EV_BUSTAG_BASE
	addu    t4, t2, t1			# loop termination
	li      t3, 0xaaaaaaaa			# distinctive pattern
	and	t3, t0				# Masked pattern
1:
	sd	t3, 0(t2)			# write to tags
	ld	t5, 0(t2)			# read from tags
	bne	t5, t3, 5f			# branch out on error
	addu	t2, 8				# next word
	bne	t2, t4, 1b

	li      t2, EV_BUSTAG_BASE
	li      t3, 0x55555555			# distinctive pattern
	and	t3, t0				# Masked pattern
1:
	sd	t3, 0(t2)			# write to tags
	ld	t5, 0(t2)			# read from tags
	bne	t5, t3, 6f			# branch out on error
	addu	t2, 8				# next word
	bne	t2, t4, 1b

	/* Do uniqueness test with address, inverse address */

	# now test each cache line
	# t1: tag RAM size
	# t0: tag RAM mask

	li      t2, EV_BUSTAG_BASE
1:
	and	t3, t0, t2
	sd	t3, 0(t2)			# write tags
	addu	t2, 8				# next word
	bne	t2, t4, 1b

	# now verify the written data
	li	t2, EV_BUSTAG_BASE
1:
	ld	t3, 0(t2)
	and	t5, t0, t2
	bne	t3, t5, 4f			# branch out on error
	addu	t2, 8
	bne	t2, t4, 1b

	move	v0, zero			# Return success
	b	9f				# branch to clearing code

5:
6:
	li	v0, EVDIAG_BUSTAGDATA
	b	9f
4:
	li	v0, EVDIAG_BUSTAGADDR
	# Fall through to 9

9:
	# While we're here, clear the puppies out!
	li      t2, EV_BUSTAG_BASE
1:
	sd	zero, 0(t2)			# write tags
	addu	t2, 8				# next word
	bne	t2, t4, 1b

	j	Ra_save0
	END(pod_check_bustags)

