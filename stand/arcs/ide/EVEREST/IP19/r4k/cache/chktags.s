/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.2 $"


#include "asm.h"
#include "regdef.h"
/*#include "../include/mach_ops.h"*/
#include "cpu.h"
#include "coher.h"
/*
 *		C h k P T a g ( )
 *
 * ChkPTag - Read the Primary tag location and check the cache state
 * and the PTag.
 *
 * registers used:
 *	a0 - starting address to check
 *	a1 - number of cache lines to check
 *	a2 - expected cache state
 *	a3 - dcache line size
 *	t0 - contents of TAGLO register
 *	t1 - used as mask
 *	t2 - cache state bits
 *	t3 - PTag value
 *	t4 - copy of starting address to check
 *	t5 - Cache line size
 */
LEAF(ChkPTag)
	.set	noreorder

	move	t5, a3			# Get primary data cache line size
	move	t4, a0			# save address for later

1:
	or	a0, 0xa0000000		# Put into Kseg1 space to avoid tlbmiss
	cache	Index_Load_Tag_D,0x0(a0)	# get the index tag for the D
	nop				#needed for hw
	mfc0	t0, C0_TAGLO		# get the data and valid bits
	nop

	/*
	 * extract cache state bits
	 */
	li	t1, TAG_PSTATE_MASK
	and	t2, t0, t1

	/*
	 * extract ptag bits
	 */
	li	t1, TAG_PTAG_MASK
	and	t3, t0, t1
	srl	t3, t3, TAG_PTAG_SHIFT 

	/*
	 * Now take our address and get the tag bits
	 */
	move	a0, t4			# restore address
	li	t1, TAG_PTAG_MASK
	and	a0, t1			# extract the actual tag
	srl	a0, a0, 0xc 

	/* 
	 * Compare the cache state bits with the expected
	 */
	bne	t2, a2, 3f		# wrong cache state was seen
	li	v0, PCSTATE_ERR
	
	/*
	 * Now compare the physical tag bits
	 */
	bne	a0, t3,  3f
	li	v0, PTAG_ERR
	/*
	 * Are we done yet
	 */
	addi  a1, -1
	beq   a1, zero, 2f
	/*
	 * Get next cache line
	 */ 
	add   t4, t5			# BDSLOT
	b     1b
	move	a0, t4			# BDSLOT, a0= next cache line

2:	j    ra
	move  v0, zero			# BDSLOT, Indicate no error

3:
	/*
	 * Get Error data into common area
	 */
	la	a0, error_data
	sw	t4, EXPADR(a0)		# Expected tag address
	sw	a2, EXPCS(a0)		# Expected cche state
	sw	t0, ACTTAG(a0)		# Actual tag data
					# Note, v0 has error code
	j	ra
	nop

	.set	reorder
END(ChkPtag)


/*
 * 		C h k S d T a g ( )
 *
 * ChkSdTag - Read the Secondary tag location and check the cache state
 * STag, and Vindex fields.
 *
 * registers used:
 *	a0 - starting address to check physical address
 *	a1 - number of cache lines to check
 *	a2 - expected cache state
 *	a3 - Virtual address used to check PIDx or VIndex
 *	t0 - contents of TAGLO register
 *	t1 - used as mask
 *	t2 - cache state bits
 *	t3 - STag value
 *	t4 - copy of starting address to check
 *	t5 - Cache line size
 *	t6 - actual virtual index bits
 */
LEAF(ChkSdTag)
	.set	noreorder

	lw	t5, scache_linesize	# Get the secondary cache line size
	move	t4, a0			# save starting address
1:
	or	a0, 0xa0000000		# Put into Kseg1 space to avoid tlbmiss
	cache	Index_Load_Tag_SD,0x0(a0) #get the SC index
	nop
	mfc0	t0, C0_TAGLO		 #get tag and valid bits
	nop

	li	t1, TAG_SSTATE_MASK
	and	t2, t0, t1		 #extract the CACHE STATE BITS

	li	t1, TAG_STAG_MASK
	and	t3, t0, t1		 #extract the TAG BITS for SC
	srl   t3, t3, TAG_STAG_SHIFT 	 #right justify them

	/*
	 * Get the actual virtual index bits
	 */
	li	t1, TAG_VINDEX_MASK
	and	t6, t0, t1
	srl	t6, t6, TAG_VINDEX_SHIFT

	/*
	 * Generate the expected virtual index bits, bits 14-12 of virtual addr
	 */
	move	t7, a3
	li	t1, 0x7000
	and	t7, t1
	srl	t7, 0xc

	/*
	 * Now take our physical address passed in and shift it for comparison
	 * with what was read from the tags
	 */
	move	a0, t4			# restore address
	li	t1, TAG_STAG_MASK
	and	a0, t1			# 
	srl	a0, a0, 0x11

	/*
	 * Check the cache state
	 */
	bne	t2, a2,  4f
	li	v0, SCSTATE_ERR
	/*
	 * Check the tag bits
	 */
	bne	a0, t3, 4f
	li	v0, SCTAG_ERR
	/*
	 * Check the virtual index bits
	 */
	bne	t7, t6, 4f
	li	v0, VIRTINDX_ERR

	/*
	 * Are we done yet
	 */
	addi	a1, -1
	beq	a1, zero, 2f 
	/*
	 * Get next cache line
	 */ 
	add	a3, t5			# bump virtual address
	add	t4, t5			# BDSLOT slot,
	b	1b
	move	a0, t4

2:	j	ra
	move	v0, zero		# BDSLOT, Indicate no error

4:
	/*
	 * Get Error data into common area
	 */
	la	a0, error_data
	sw	t4, EXPADR(a0)		# Expected tag address
	sw	a2, EXPCS(a0)		# Expected cche state
	sw	a3, VIRTADR(a0)		# Virtual address
	sw	t0, ACTTAG(a0)		# Actual tag data
					# Note, v0 has error code
	j	ra
	nop

	.set	reorder
END(ChkSdTag)

LEAF(GetPstate)
	.set	noreorder


	or	a0, 0xa0000000		# Put into Kseg1 space to avoid tlbmiss
	cache	Index_Load_Tag_D,0x0(a0)	# get the index tag for the D
	nop				#needed for hw
	mfc0	t0, C0_TAGLO		# get the data and valid bits
	nop

	/*
	 * extract cache state bits
	 */
	li	t1, TAG_PSTATE_MASK
	and	v0, t0, t1

					# Note, v0 has error code
	j	ra
	nop

	.set	reorder
END(GetPstate)
