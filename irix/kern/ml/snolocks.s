/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Low-level spin lock routines for non-lock version of locks
 */
#ident	"$Revision: 1.14 $"

#include "ml/ml.h"
#ifdef SPLDEBUG	
#define SPLNESTED(x,y,z,reg,x1,x2)	; 	\
	NESTED(x,y,z)	; 	\
	move	reg, ra	; 	\
	j	x1	; 	\
	END(x)		; 	\
	NESTED(x2,y,z)

#define SPLLEAF(x,y,x1,x2)			\
	LEAF(x)			; 	\
	move	y, ra		;	\
	j	x1		; 	\
	END(x)			; 	\
	LEAF(x2)
	
#define SPLXLEAF(x,y,x1,x2)			\
	move	y, ra		;	\
	j	x1		; 	\
	END(x)			; 	\
	LEAF(x2)
	
#define	SPLEND(x,x2)	END(x2)
	
#else
	
#define SPLNESTED(x,y,z,reg,x1,x2)	NESTED(x,y,z)	
#define	SPLEND(x,x2)	END(x)
#define SPLLEAF(x,y,x1,x2)	LEAF(x)	
#endif			

LEAF(nested_spinlock)
XLEAF(nested_bitlock)
XLEAF(nested_64bitlock)
	.set 	noreorder
	j	ra
	nop
	.set 	reorder
	END(nested_spinlock)	


LEAF(nested_spinunlock)
XLEAF(nested_bitunlock)
XLEAF(nested_64bitunlock)
	.set 	noreorder
	j	ra
	nop
	.set 	reorder
	END(nested_spinunlock)	

/*
 * mutex_spinunlock(mutex_t *lck, int ospl)
 *
 * spunlockspl - Release the given spinlock and restore the spl
 * spunlockspl(lock_t lck, int ospl)
 */
SPLLEAF(mutex_spinunlock, a2, _mutex_spinunlock,__mutex_spinunlock)
#ifdef notdef
XLEAF(spunlockspl)
XLEAF(_spunlockspl)
XLEAF(io_spunlockspl)
XLEAF(_io_spunlockspl)
#endif
	.set	noreorder
	j	splx
	move	a0, a1
	.set 	reorder
	SPLEND(mutex_spinunlock,__mutex_spinunlock)

SPLLEAF(mutex_spinlock,a1,_mutex_spinlock,__mutex_spinlock)
#ifndef SPLDEBUG	
XLEAF(mutex_bitlock)
#endif	
XLEAF(mutex_64bitlock)
	.set	noreorder
	j	splhi
	nop
	.set	reorder
	SPLEND(mutex_spinlock,__mutex_spinlock)
	
#ifdef SPLDEBUG
SPLLEAF(mutex_bitlock,a1,_mutex_bitlock,__mutex_bitlock); 
	.set	noreorder
	j	splhi
	nop
	.set	reorder
	SPLEND(mutex_bitlock,__mutex_bitlock)
#endif /* SPLDEBUG */	

/*
 * mutex_bitlock_spl - lock a lock and raise spl as requested
 *
 * mutex_bitlock_spl(uint *lck, uint bit, int (*splr)())
 */
LEAF(mutex_bitlock_spl)
	.set	noreorder
	j	a2
	nop
	.set	reorder
	END(mutex_bitlock_spl)

/*
 * splockspl - lock a lock and raise spl to at least as high as requested
 * splockspl(lock_t lck, int (*splr)())
 * mutex_spinlock_spl(lock_t lck, int (*splr)())
 */
SPLLEAF(mutex_spinlock_spl,a2,_mutex_spinlock_spl,__mutex_spinlock_spl)
XLEAF(mutex_tryspinlock_spl)
	.set	noreorder
	j	a1
	nop
	.set	reorder
	SPLEND(mutex_spinlock_spl,__mutex_spinlock_spl)

LEAF(nested_spintrylock)
	.set	noreorder
	j	ra
	li	v0, 1
	.set	reorder
	END(nested_spintrylock)

SPLLEAF(mutex_bitunlock,a3,_mutex_bitunlock,__mutex_bitunlock)
XLEAF(mutex_64bitunlock)
	.set	noreorder
	j	splx
	move	a0, a2
	.set 	reorder
	SPLEND(mutex_bitunlock,__mutex_bitunlock)	

