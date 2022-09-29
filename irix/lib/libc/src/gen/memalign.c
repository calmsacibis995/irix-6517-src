/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/memalign.c	1.4"

#ifdef __STDC__
	#pragma weak memalign = __memalign
	#pragma weak _memalign = __memalign
#endif

#include "synonyms.h"
#include "mallint.h"
#include "malloc.h"
#if defined(_LIBC_NOMP)
#undef __LOCK_MALLOC
#undef __UNLOCK_MALLOC
#define __LOCK_MALLOC()		(void)1
#define __UNLOCK_MALLOC()	(void)0
#endif
#include <stdlib.h>
#include <errno.h>
#include "gen_extern.h"

#if _MIPS_SZPTR == 32
#define _misaligned(p)	(size_t)((p)&3)
#else
#define _misaligned(p)	(size_t)((p)&7)
#endif
#define	_nextblk(p, size)	((TREE *) ((char *) (p) + (size)))
#define _roundup(x, y)		((((x)+((y)-1))/(y))*(y))

/*
 * memalign(align,nbytes)
 *
 * Description:
 *	Returns a block of specified size on a specified alignment boundary.
 *
 * Algorithm:
 *	Malloc enough to ensure that a block can be aligned correctly.
 *	Find the alignment point and return the fragments
 *	before and after the block.
 *
 * Errors:
 *	Returns NULL and sets errno as follows:
 *	[EINVAL]
 *		if nbytes = 0,
 *		or if alignment is misaligned,
 *	 	or if the heap has been detectably corrupted.
 *	[ENOMEM]
 *		if the requested memory could not be allocated.
 */

VOID *
__memalign(size_t align, size_t nbytes)
{
	size_t	 reqsize;		/* Num of bytes to get from malloc() */
	register TREE	*p;		/* Ptr returned from malloc() */
	register TREE	*blk;		/* For addressing fragment blocks */
	register size_t	blksize;	/* Current (shrinking) block size */
	register TREE	*alignedp;	/* Ptr to properly aligned boundary */
	register TREE	*aligned_blk;	/* The block to be returned */
	register size_t	frag_size;	/* size of fragments fore and aft */
	size_t	 x;			

	/*
	 * check for valid size and alignment parameters
	 */
	if (nbytes == 0 || _misaligned(align) || align == 0) {
		setoserror(EINVAL);
		return NULL;
	}

	/*
	 * Malloc enough memory to guarantee that the result can be
	 * aligned correctly. The worst case is when malloc returns
	 * a block so close to the next alignment boundary that a
	 * fragment of minimum size cannot be created.
	 */
	ROUND(nbytes);
	reqsize = nbytes + align + MINSIZE;
	p = (TREE *) malloc(reqsize);
	if (p == (TREE *) NULL) {
		setoserror(ENOMEM);
		return NULL;
	}

	/*
	 * get size of the entire block (overhead and all)
	 */
	__LOCK_MALLOC();
	blk = BLOCK(p);			/* back up to get length word */
	blksize = SIZE(blk);
	CLRBITS01(blksize);

	/*
	 * locate the proper alignment boundary within the block.
	 */
	x = _roundup((size_t)p, align);		/* ccom work-around */
	alignedp = (TREE *)x;
	aligned_blk = BLOCK(alignedp);

	/*
	 * Check out the space to the left of the alignment
	 * boundary, and split off a fragment if necessary.
	 */
	frag_size = (size_t)aligned_blk - (size_t)blk;
	if (frag_size != 0) {
		/*
		 * Create a fragment to the left of the aligned block.
		 */
		if ( frag_size < sizeof(TREE) ) {
			/*
			 * Not enough space. So make the split
			 * at the other end of the alignment unit.
			 */
			frag_size += align;
			aligned_blk = _nextblk(aligned_blk,align);
		}
		blksize -= frag_size;
		SIZE(aligned_blk) = blksize | BIT0;
		frag_size -= WORDSIZE;
		SIZE(blk) = frag_size | (unsigned int)(ISBIT1(SIZE(blk)) ? BITS01 : BIT0);
#ifdef _LIBC_ABI
		free(DATA(blk));
#else
		__free(DATA(blk));
#endif /* _LIBC_ABI */
	}

	/*
	 * Is there a (sufficiently large) fragment to the
	 * right of the aligned block?
	 */
	frag_size = blksize - nbytes;
	if (frag_size > MINSIZE + WORDSIZE) {
		/*
		 * split and free a fragment on the right
		 */
		blksize = ISBIT1(SIZE(aligned_blk));
		SIZE(aligned_blk) = nbytes;
		blk = NEXT(aligned_blk);
		blksize? SETBITS01(SIZE(aligned_blk)) : SETBIT0(SIZE(aligned_blk));
		SIZE(blk) = (frag_size-WORDSIZE) | BIT0;
#ifdef _LIBC_ABI
		free(DATA(blk));
#else
		__free(DATA(blk));
#endif /* _LIBC_ABI */
	}
	__UNLOCK_MALLOC();
	return(DATA(aligned_blk));
}
