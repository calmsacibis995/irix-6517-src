
/* Set-up cache tag, and then write to both sides of the cache and check
 * the result.  The goal is to check each of the data lines to the scache.
 *
 * This is step4 of the diag PROM series.
 */

#ident	"$Revision: 1.2 $"

#define IP26	1

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <regdef.h>
#include <asm.h>
#include "blink.h"

LEAF(start)
	.set noreorder
	li	v0,SR_CU1|SR_FR
	dmtc0	v0, C0_SR
	ssnop
	ssnop
	ssnop
	dmtc0	zero, C0_CAUSE
	ssnop
	ssnop
	ssnop

	/* Write 0xd46b to PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6)) */
	LI	t0, PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(6))
	li	t1, 0xd46b			# data
	sw	t1, 0(t0)

	/* Load-up the cache tag */
	LI	a0,PHYS_TO_K0(0x19004000)
	dli	v1,PHYS_TO_K1(TCC_ETAG_DA)	# tag data base
	dli	v0,PHYS_TO_K1(TCC_ETAG_ST)	# tag state base
	and	a1,a0,GCACHE_INDEX		# isolate index
	or	v0,a1				# set index in tag addr
	or	v1,a1				# set index in tag addr
	dli	a2,GCACHE_INITSTATE		# tag state init value
	li	a3,0x1000000

	and	a1,a0,GCACHE_TAG		# isolate physical tag
	sd	a1,0(v1)			# tag data set 0
	or	a7,a2,(GCACHE_VALID<<GCACHE_STATE_SHIFT)
	sd	a7,0(v0)			# tag state

	add	a1,a3				# next set
	sd	a2,1<<TAGADDR_SET_SHIFT(v0)	# tag state
	sd	a1,1<<TAGADDR_SET_SHIFT(v1)	# tag data set 1
	add	a1,a3				# next set
	sd	a2,2<<TAGADDR_SET_SHIFT(v0)	# tag state
	sd	a1,2<<TAGADDR_SET_SHIFT(v1)	# tag data set 2
	add	a1,a3				# next set
	sd	a2,3<<TAGADDR_SET_SHIFT(v0)	# tag state
	sd	a1,3<<TAGADDR_SET_SHIFT(v1)	# tag data set 3

	/* data to write to even/odd caches */
	LI	t0,0xaaaaaaaaaaaaaaaa
	LI	t1,0x5555555555555555
	move	a2, a0

	/* blink the LED green three times */
	move	s0, zero
	ori	s1, zero, WAITCOUNT_FAST
	jal	blink2

	/* Write 5's */
	sd	t1,0(a2)
	sd	t1,8(a2)
	ssnop;ssnop;ssnop;ssnop
	ld	a4,0(a2)
	ld	a5,8(a2)

	/* verify 5's on even side */
	move	t2, a4
	ori	a0, zero, 1
	jal	check

	/* blink the LED green three times */
	move	s0, zero
	ori	s1, zero, WAITCOUNT_FAST
	jal	blink2

	/* verify 5's on odd side */
	move	t2, a5
	ori	a0, zero, 1
	jal	check

	/* blink the LED green three times */
	move	s0, zero
	ori	s1, zero, WAITCOUNT_FAST
	jal	blink2

	/* Write A's */
	sd	t0,0(a2)
	sd	t0,8(a2)
	ssnop;ssnop;ssnop;ssnop
	ld	a6,0(a2)
	ld	a7,8(a2)

	/* verify A's on even side */
	move	t2, a6
	move	a0, zero
	jal	check

	/* blink the LED green three times */
	move	s0, zero
	ori	s1, zero, WAITCOUNT_FAST
	jal	blink2

	/* verify A's on odd side */
	move	t2, a7
	move	a0, zero
	jal	check

	/* Spin for-ever */
1:	b	1b				# spin
	nop
	.set	reorder
	END(start)

/* check and blink code */
#include "blink.s"


