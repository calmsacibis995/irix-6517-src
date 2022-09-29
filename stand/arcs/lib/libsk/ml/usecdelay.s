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

LEAF(usecwait)
	li	a1,CRM_TIME|K1BASE
	sd	zero,0(a1)
	li	a2,TICKS
	mult	a0,a2			# total number of ticks
	mflo	a0			# now in a0.
	add	a0,zero			# calculate end count
1:	
	ld	a2,0(a1)		# get current count
	dsll32	a2,0			# clear upper word to fix crime1.3
	dsra32	a2,0			#   stuck bit 32
	sub	a2,a0,a2
	bgtz	a2,1b			# less than ending count? yes -> again
	j	ra			# no, done.
	END(usecwait)
