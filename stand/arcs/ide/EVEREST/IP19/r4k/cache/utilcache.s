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


#ifdef	SABLE
#define R4000
#endif	SABLE

#include "asm.h"


#include "regdef.h"
#include "cpu.h"

#include "coher.h"

#/**********************************
#* DEFINE EXTERNALS
#**********************************/
	.globl	scache_linesize

	.globl	error_data	





/*
 *		C a c h e B l k R d ( )
 *
 * CacheBlkRd- Reads data into the primary and secondary caches
 * from memory at the address specified. This routine does not read
 * sequencial locations, but uses the block fill of the cache to read
 * in the data.
 * 
 * register used:
 *	a0 - Memory address to read from
 *	a1 - Number of cache lines to read
 *	a2 - secondary cache line size
 *	t0 - data is read backwords, a0+a1
 *	t1 - data read
 */
LEAF(CacheBlkRd)
	.set	noreorder
	
	addiu	a1, a1, -1		/* number of lines -1 */
	move	t0, a2			/* */
	

1:
	lw	t1, 0(a0)
	addu	a0, t0
	bne	a1, zero, 1b
	addi	a1, a1, (-1)
	j	ra
	nop

	.set	reorder
END(CacheBlkRd)


/*
 *		S t o r e C a c h e W ( )
 *
 * StoreCacheW - Stores data (word) into the primary and secondary caches
 * at the address specified. The cache state is not known to this routine.
 * Calling this routine will cause cache hits/misses.
 * 
 * register used:
 *	a0 - Memory address to read from
 *	a1 - Number of locations to read
 *	a2 - data to write
 *	t0 - tmp=  a0+a1
 */
LEAF(StoreCacheW)
	.set	noreorder
	
	addiu	a1, a1, -1		/* number of words -1 */
	sll	a1, a1, 2		/* convert words to byte offset */

StoreLoop0:
	addu	t0, a0, a1
	sw	a2, 0(t0)
	bne	a1, zero, StoreLoop0
	addi	a1, a1, (-4)
	j	ra
	nop

	.set	reorder
END(StoreCacheW)




/*************************************************************/
/*    P R I M A R Y   D C A C H E   R O U T I N E S          */
/*************************************************************/

/*
 *		C h k P r i m a r y D a t a ( )
 *
 * ChkPrimaryData compares the data in the primary cache starting at the address
 * specified and compares 'n' words.
 *
 * register used:
 *	a0 - starting address to check, kseg2 address
 *	a1 - expected data address, kseg1 address
 *	a2 - number of words to check
 *	a3 - data or'd into expected
 *	t0 - used to reference the actual data in reverse order 
 *	t1 - used to reference expected data in reverse order
 *	t2 - primary data, MAPPED ACCESS
 *	t3 - data from memory, ACCESSED IN KSEG01 SPACE
 *	
 */
LEAF(ChkPrimaryData)
	.set	noreorder

	addiu	a2, a2, -1		/* Words to check -1 */
	sll	a2, a2, 2		/* convert words to byte addr offset */

Compare_Loop1:
	addu	t0, a0, a2		/* start at the end of block */ 
	addu	t1, a1, a2		/* expected table */
	lw	t3, 0(t1)		/* Read expected data */
	lw	t2, 0(t0)		/* Read from primary */
	or	t3, a3
	bne	t2, t3, 1f		/* branch on error */
	move	v0, a2			/* return offset in array on error */

	bne	a2, zero, Compare_Loop1	/* Next word */
	addi	a2, a2, (-4)		/* BDSLOT, decrement number of words */
	move	v0, zero		/* return no error status */

1:
	j	ra
	nop

	.set	reorder
END(ChkPrimaryData)



/*************************************************************/
/*    P R I M A R Y   D C A C H E   R O U T I N E S          */
/*************************************************************/

/*
 *		C h k D T a g _ CS( )
 *
 * ChkDTag_CS - Read the Primary tag location and check the cache state
 * and the PTag.
 *
 * registers used:
 *	a0 - starting address to check
 *	a1 - number of cache lines to check
 *	a2 - expected cache state
 *	a3 - Cache line size
 *	t0 - contents of TAGLO register
 *	t1 - used as mask
 *	t2 - cache state bits
 *	t3 - PTag value
 *	t4 - copy of starting address to check
 *	t5 - Cache line size
 */
LEAF(ChkDTag_CS)
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
	 * Compare the cache state bits with the expected
	 */
	bne	t2, a2, 3f		# wrong cache state was seen
	li	v0, PCSTATE_ERR
	
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
	sw	t2, ACTTAG(a0)		# Actual tag data
					# Note, v0 has error code
	j	ra
	nop

	.set	reorder
END(ChkDtag_CS)

/*
 *		C h k D T a g _ TAG( )
 *
 * ChkDTag_TAG - Read the Primary tag location and check the cache state
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
LEAF(ChkDTag_TAG)
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
	 * extract ptag bits
	 */
	li	t1, TAG_PTAG_MASK
	and	t3, t0, t1
	srl	t3, t3, TAG_PTAG_SHIFT 

	/*
	 * Now take our address and get the tag bits
	 */
	move	t2, t4			# restore address
	li	t1, TAG_PTAG_MASK
	and	t2, t1			# extract the actual tag
	srl	t2, t2, 0xc 

	/*
	 * Now compare the physical tag bits
	 */
	bne	t2, t3,  3f
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
	sw	t2, EXPCS(a0)		# Expected cche state
	sw	t3, ACTTAG(a0)		# Actual tag data
					# Note, v0 has error code
	j	ra
	nop

	.set	reorder
END(ChkDtag_TAG)


/*
 *		C h k I T a g _ CS( )
 *
 * ChkITag_CS - Read the Primary I-cache tag location and check the cache state
 *
 * registers used:
 *	a0 - starting address to check
 *	a1 - number of cache lines to check
 *	a2 - expected cache state
 *	a3 - Cache line size
 *	t0 - contents of TAGLO register
 *	t1 - used as mask
 *	t2 - cache state bits
 *	t3 - PTag value
 *	t4 - copy of starting address to check
 *	t5 - Cache line size
 */
LEAF(ChkITag_CS)
	.set	noreorder

	move	t5, a3			# Get primary data cache line size
	move	t4, a0			# save address for later

1:
	or	a0, 0xa0000000		# Put into Kseg1 space to avoid tlbmiss
	cache	Index_Load_Tag_I,0x0(a0)	# get the index tag for the D
	nop				#needed for hw
	mfc0	t0, C0_TAGLO		# get the data and valid bits
	nop

	/*
	 * extract cache state bits
	 */
	li	t1, 0x80
	and	t2, t0, t1

	/* 
	 * Compare the cache state bits with the expected
	 */
	bne	t2, a2, 3f		# wrong cache state was seen
	li	v0, PCSTATE_ERR
	
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
	sw	t2, ACTTAG(a0)		# Actual tag data
					# Note, v0 has error code
	j	ra
	nop

	.set	reorder
END(ChkItag_CS)

