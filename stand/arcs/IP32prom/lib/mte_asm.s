#include <regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/crime.h>

/*
 * delay for at least a0 microseconds.  TICKS is the number of
 * CRM_TIME ticks/microsecond.  CRM_TIME is a free running 66Mhz
 * counter.
 *
 * XXX: the number of microseconds to delay is expected to be
 * relatively small.  In particular it is assumed that the product
 * of (usecs * TICKS) will yeild a 32 bit value.
 */

#define TICKS	66

LEAF(us_delay)
	li	a1,CRM_TIME|K1BASE
	sd	zero,0(a1)
	li	a2,TICKS
	mult	a0,a2			# total number of ticks
	mflo	a0			# now in a0.
	add	a0,zero			# calculate end count
1:	
	ld	a2,0(a1)		# get current count
	sub	a2,a0,a2
	bgtz	a2,1b			# less than ending count? yes -> again
	j	ra			# no, done.
	END(usecwait)
	
LEAF(write_reg64)
	dsll32	a1,0
	dsrl32	a1,0
	dsll32	a0,0
	or	a0,a1
	.set noreorder
	j	ra
	sd	a0,0(a2)
	.set reorder
	END(write_reg64)

LEAF(get_crm_rev)
	ld	v0,CRM_ID|K1BASE
	.set noreorder
	j	ra
	and	v0,CRM_ID_REV
	.set reorder
	END(get_crm_rev)
