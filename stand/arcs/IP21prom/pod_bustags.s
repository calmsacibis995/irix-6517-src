/*
 * pod_cc.s -
 *      POD CPU board tests.
 */
#define NEWTSIM

#include "ml.h"
#include <sys/regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "ip21prom.h"
#include "pod.h"
#include "pod_failure.h"

	.set	reorder

/* This guy's a "level zero routine" */
/* Check and clear the bus tags. */
LEAF(pod_check_bustags)
#if 1
	li	v0, 0
	j	ra
#else
	move	Ra_save0, ra

	.set	noreorder
	DMFC0(v1, C0_CONFIG)
	.set	reorder

	/* read_scachesize */
	jal	read_scachesize

	# v0 = cachesize in bytes

        # Figure out the scache block size

	/* Caclulate the size of bus tag RAM 
	 * 	t1 = size of tag RAM space
	 */

	# v0 still = cachesize in bytes

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


	/* Do uniqueness test with address, inverse address */

	# now test each cache line
	# t1: tag RAM size
	# t0: tag RAM mask


	# now verify the written data

	# Fall through to 9

9:
	# While we're here, clear the puppies out!

	j	Ra_save0
#endif
	END(pod_check_bustags)

