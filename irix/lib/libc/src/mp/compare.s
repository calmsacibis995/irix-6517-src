/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * R4000 atomic operations based on link-load, store-conditional instructions.
 */

#ident	"$Revision: 1.5 $"

#if (!defined(_COMPILER_VERSION) || (_COMPILER_VERSION<700)) /* !Mongoose -- XXX 710  */

#include <regdef.h>
#include <sys/asm.h>

/*
 * Set a word to a new value if it equals a particular value
 * a0 - address of word
 * a1 - old value
 * a2 - new value
 * returns 1 if the values were swapped
 */
LEAF(__compare_and_swap)
	.set 	noreorder

1:
	# Load the old value
	LONG_LL	t0,0(a0)

	# Check for match
	bne	t0,a1,2f
	move	v0,a2

	# Try to set to the new value
	LONG_SC	v0,0(a0)	
	beqzl	v0, 1b
	nop

	# Return success to caller
	j	ra
	nop
2:
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	# Return failure to caller
	j	ra
	move	v0,zero
	.set	reorder
END(__compare_and_swap)

/*
 * int __add_and_fetch (int *, int)
 *
 * Atomic add operation for ragnarok builds
 */		
LEAF(__add_and_fetch)
	.set	noreorder
1:	INT_LL	t0, 0(a0)
	move 	v0, t0
	addu	t0, a1
	INT_SC	t0, 0(a0)	
	beqzl	t0, 1b
	nop
	j	ra
	nop
	.set	reorder
END(__add_and_fetch)
				
#endif
