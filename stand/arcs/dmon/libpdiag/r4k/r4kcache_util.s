#ident	"$Id: r4kcache_util.s,v 1.1 1994/07/21 01:25:51 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"

#include "pdiag.h"
#include "monitor.h"

#define PHYS_RAMBASE            0x08000000
#define K0_RAMBASE              X_TO_K0(PHYS_RAMBASE)
#define MINCACHE        (128*1024)
#define MAXCACHE        (4*1024*1024)

/*
 * WriteDcacheW(u_init address, u_int word_count, u_int pattern)
 *
 *	Write the data cache with the pattern. This will cause the
 *	cache state to be changed to dirty exclusive. This routine
 *	assumes a correct cachable address as input.
 *
 * register usage:
 *	a0 - input address		a1 - word count
 *	a2 - pattern to write
 */
LEAF(WriteDcacheW)

	and	t0,a0,0x03		# check word boundary
	bne	t0,zero,11f

	sll	t0,a1,2			# get byte offset
	addu	a1,a0,t0		# get last address + 1
1:
	bgeu	a0,a1,done		# reach the last word yet?
	sw	a2,(a0)			# store pattern into the location
	addiu	a0,4			# next address
	b	1b
done:
	move	v0,zero
	j	ra
11:
	PRINT("\nAddress not at word boundary")
	li	v0,1
	j	ra

END(WriteDcacheW)



/*
 * CheckDcacheW(u_init address, u_int word_count, u_int pattern)
 *
 *	Check the data cache with the pattern. This routine
 *	assumes the cache line is valid.
 *
 * register usage:
 *	a0 - input address		a1 - word count
 *	a2 - pattern to compare
 */
LEAF(CheckDcacheW)

	and	t0,a0,0x03		# check word boundary
	bne	t0,zero,11f

	sll	t0,a1,2			# get byte offset
	addu	a1,a0,t0		# get last address + 1
1:
	bgeu	a0,a1,2f		# reach the last word yet?
	lw	t0,(a0)			# read the word
	bne	a2,t0,12f		# branch if not compare
	addiu	a0,4			# next address
	b	1b
2:
	move	v0,zero
	j	ra
11:
	PRINT("\nAddress not at word boundary")
12:
	move	a1,a2			# expected value for error routine
	move	a2,t0			# actual value for error display
	li	v0,1
	j	ra

END(CheckDcacheW)


/* Code taken from lib/libsk/ml and IP22prom directories. Didn't link
 * to the code because the debug prom does not use the stack
 */
/*
 * Invalidate the cache in question.
 * Contents of the cache is invalidated - no writebacks occur.
 * These do not assume memory is working, or stack pointer valid.
 *
 * These invalidate without causing writebacks. They
 * are useful for cleaning up the random power-on state.
 *
 * a0: cache size
 * a1: cache line size
 */
LEAF(invalidate_icache)
	.set	noreorder
	mtc0	zero,C0_TAGLO
	mtc0	zero,C0_TAGHI
	li	t0,K0_RAMBASE
	addu	a0,t0		# a0: cache size + K0base for loop termination
	nop
	.set	reorder
1:
	.set	noreorder
	cache	CACH_PI|C_IST,0(t0)
	.set	reorder
	addu	t0,a1
	bltu	t0,a0,1b		# a0 is termination count

	j	ra
	END(invalidate_icache)


/*
 * cache_init()- Is an assembly version of the IP22prom/init_caches 
 * routine. It inits the caches from a power up state.
 */
LEAF(cache_init)
	.set	noreorder
	/* Save Ra */
	move	t6, ra

	/* Save status register then set DE bit */
	jal	GetSR
	nop
	or	a0, v0, SR_DE		/* Branch delay slot */
	jal	SetSR
	nop

	move	a0, v0
	jal	push_nvram
	

	nop
	/* Go zero all the tags */
	jal	invalidate_caches
	nop

	/* Check for a secondary cache */
	jal	noSCache
	nop
	bne	v0, zero, 1f
	nop
	/* Must be a secondary cache */
	jal	init_load_scache
	nop
1:
	/* init the data array and parity */
	jal	init_load_dcache
	nop
	jal	init_load_icache
	nop

	/* Now that parity is straight invalid the caches */
	jal	invalidate_caches
	nop
	jal	pop_nvram
	nop
	jal	SetSR
	move	a0, v0
	j	t6
	nop
	.set	reorder
END(cache_init)


/*
 * invalidate_caches()- Zeros the tags in the icache, dcache, and secondary
 * cache.
 */
LEAF(invalidate_caches)

	.set	noreorder
	/* Save the return address */
	move	t7, ra

	/* Get the icache size */
	jal	icache_size
	nop
	move	a0, v0
	/* Get the icache linesize */
	jal	icache_ln_size
	nop
	/* Go init the icache tags to zero, pass size and linesize*/
	jal	invalidate_icache
	move	a1, v0


	/* Get the dcache size */
	jal	dcache_size
	nop
	move	a0, v0
	/* Get the line size */
	jal	dcache_ln_size
	nop
	/* Set the data tags to zero */
	jal	invalidate_dcache
	move	a1, v0


	/* Check for a secondary cache */
	jal	noSCache
	nop
	bne	v0, zero, 1b
	nop
	/* Determine the secondary size */
	jal	size_2nd_cache
	nop
	move	a0, v0
	nop
	/* Get the line size */
	jal	scache_ln_size
	nop
	jal	invalidate_scache
	move	a1, v0
	nop

1:
	j	t7
	nop
	.set	reorder
END(invalidate_caches)
