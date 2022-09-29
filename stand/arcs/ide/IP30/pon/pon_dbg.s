/*
 * pon_dbg.s - miscelleous power-on debug subroutines
 *             put routines here that will NOT be included in production
 */

#if DEBUG	
	
#include "asm.h"
#include "regdef.h"
#include "sys/sbd.h"
#include "sys/RACER/heart.h"
	
/*
 * write_read_secondary_cache_tags: 
 *                             debug routine that writes tag0 & tag1 for
 *                             each way into the address specified, and
 *                             reads what was written. If the read doesnt
 *                             match the write, the trigger pin is activated.
 *
 * this is a debug-only routine that was used to test the 2ndary cache
 * TAG SRAM data
 *
 * args:	
 * a0 :	 tag0 value for way 0
 * a1 :	 tag1 value for way 0
 * a2 :	 tag0 value for way 1
 * a3 :	 tag1 value for way 1
 * a4 :	 addr for way 0
 * a5 :	 addr for way 1 
 *
 */
	
LEAF(write_read_secondary_cache_tags)

	.set	noreorder

	# setup for trigger
	LI	a6, PHYS_TO_K1(HEART_TRIGGER)
	LI	a7, 0x1
	
	# WAY 0 WRITE #########################################################
	
	# set up lo/hi for way 0
	mtc0	a0,C0_TAGLO
	mtc0	a1,C0_TAGHI	

	# set 2ndary cache for way 0
	cache	CACH_SD|C_IST, 0(a4)	# set secondary data cache tag	

	# WAY 1 WRITE #########################################################
	
	# set up lo/hi for way 1
	mtc0	a2,C0_TAGLO
	mtc0	a3,C0_TAGHI	

	# set 2ndary cache for way 1
	cache	CACH_SD|C_IST, 0(a5)	# set secondary data cache tag
	
	# WAY 0 READ #########################################################
	.set	noreorder
	cache	CACH_SD|C_ILT, 0(a4)	# fetch secondary cache tag
	mfc0	t0,C0_TAGLO
	mfc0	t1,C0_TAGHI
	mfc0	v0,C0_ECC
	.set	reorder

	# WAY 1 READ #########################################################
	.set	noreorder
	cache	CACH_SD|C_ILT, 0(a5)	# fetch secondary cache tag
	mfc0	t2,C0_TAGLO
	mfc0	t3,C0_TAGHI
	mfc0	v1,C0_ECC	
	.set	reorder	

	.set	noreorder	
	# Compare Way 0 Read w/ Pattern
	# tag 0
	bne	t0, a0, trigger
	nop
	# tag 1
	bne	t1, a1, trigger
	nop

	# Compare Way 1 Read w/ Pattern
	# tag 0
	bne	t2, a2, trigger
	nop
	# tag 1
	bne	t3, a3, trigger
	nop	

	# OK
	b	done
	li	v0, 0x0
	
trigger:
	# write 1
	sd	a7, 0(a6)
	# write 0
	sd	zero, 0(a6)
	# return error
	li	v0, 0x1
				
	# DONE
done:		
	.set	reorder
	j	ra	
	
	END(write_read_secondary_cache_tags)

#endif /* DEBUG */
