/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Posix.1b unnamed semaphore atomic operations
 */

#include <asm.h>
#include <regdef.h>
#include <sgidefs.h>

#define semp	a0
#define spins	a1

/*
 * int _r4k_sem_post (struct sem_t *)
 *
 * Returns:	0 - semaphore posted, no wakeup is needed
 *		1 - semaphore NOT posted, a waiter is detected
 */
LEAF(_r4k_sem_post)
	.set	noreorder
1:	INT_LL	t0, 0(semp)
	addiu	t0, 1
	blez	t0, 2f			# Branch if a waiter is detected
	li	v0, 1			# BDSLOT
	
	INT_SC	t0, 0(semp)
	beqzl	t0, 1b
	nop				# BDSLOT (not used because of R10K) 
	li	v0, 0			# posted
	
2:	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	j	ra
	nop
	.set	reorder
END(_r4k_sem_post)
		
/*
 * int _r4k_sem_wait (struct sem_t *, int spins)
 *
 * Returns:	0 - semaphore available
 *		1 - semaphore NOT available, block
 */
LEAF(_r4k_sem_wait)
	.set	noreorder
	.set	notransform
1:	INT_LL	t0, 0(semp)
	addiu	t0, -1			# LDSLOT
	bltz	t0, 2f			# semaphore not immediately available
	addiu	spins, -1		# BDSLOT
	
	INT_SC	t0, 0(semp)		# available
	beqzl	t0, 1b
	nop				# BDSLOT (not used because of R10K)	

	j	ra	
	li	v0, 0			
	
2:	bgtz	spins, 1b		# if (spins > 0) retry
	nop				# BDSLOT

	INT_SC	t0, 0(semp)
	beqzl	t0, 1b
	nop				# BDSLOT (not used because of R10K)	
					
	.set	transform
	j	ra	
	li	v0, 1			# semaphore not available
	.set	reorder
END(_r4k_sem_wait)	

/*
 * int _r4k_sem_trywait (struct sem_t *)
 *
 * Returns:	0 - semaphore available
 *		1 - semaphore may not be available, check for prepost
 */
LEAF(_r4k_sem_trywait)
	.set	noreorder
1:	INT_LL	t0, 0(semp)
	addiu	t0, -1
	bltz	t0, 2f			# branch if not available
	li	v0, 1			# BDSLOT
	
	INT_SC	t0, 0(semp)
	beqzl	t0, 1b
	nop				# BDSLOT (not used because of R10K)	
	li	v0, 0			

2:	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	nop; nop; nop; nop; nop; nop; nop
	j	ra	
	nop
	.set	reorder
END(_r4k_sem_trywait)	

/*
 * void _r4k_sem_rewind (struct sem_t *)
 */		
LEAF(_r4k_sem_rewind)
	.set	noreorder
1:	INT_LL	t0, 0(semp)
	addiu	t0, 1
	INT_SC	t0, 0(semp)	
	beqzl	t0, 1b
	nop
	j	ra
	nop
	.set	reorder
END(_r4k_sem_rewind)
