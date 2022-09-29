/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __MALLINT_H__
#define __MALLINT_H__

#ident "$Revision: 1.14 $ $Author: jwag $"
#include "ulocks.h"

/* template for the header */
struct header {
	struct header *nextblk;
 	struct header *prevblk;
	struct header *nextfree;
	struct header *prevfree;
};
/*
	template for a small block
*/
struct lblk  {
	union {
		struct lblk *nextfree;  /* the next free little block in this
					   holding block.  This field is used
					   when the block is free */
		struct holdblk *holder; /* the holding block containing this
					   little block.  This field is used
					   when the block is allocated */
	}  header;
	char byte;		    /* There is no telling how big this
					   field freally is.  */
};
#define MINSMALLHEAD ((int)sizeof(struct lblk *))

/* 
	template for holding block
*/
struct holdblk {
	struct holdblk *nexthblk;   /* next holding block */
	struct holdblk *prevhblk;   /* previous holding block */
	struct lblk *lfreeq;	/* head of free queue within block */
	struct lblk *unused;	/* pointer to 1st little block never used */
	int blksz;		/* size of little blocks contained */
	int holdsz;		/* useful holdblk constant */
	char head[MINSMALLHEAD];/* maybe header for first small block
				 * in hold group (depends on alignment) */
};

/*
	The following manipulate the free queue

		DELFREEQ will remove x from the free queue
		ADDFREEQ will add an element to the head
			 or tail of the free queue.
		MOVEHEAD will move the free pointers so that
			 x is at the front of the queue
		COMPACTFWD - if 'n' is free, compact with 'b'
			 'b's nextblk must already have been CLRBUSY
		COMPACTBACK - if 'b's predecessor is free coalesce
			note that 'b' is changed to point to new block
*/
#define ADDFREEQ(fptr,x) if (AP(addtotail)) addtail(fptr,x); \
				else addhead(fptr,x);
#define DELFREEQ(x)       (x)->prevfree->nextfree = (x)->nextfree;\
				(x)->nextfree->prevfree = (x)->prevfree;\
				(x)->prevfree = (x)->nextfree = 0; \
				assert((x)->nextfree != (x));\
				assert((x)->prevfree != (x));
#define MOVEHEAD(x)       AP(freeptr)[1].prevfree->nextfree = \
					AP(freeptr)[0].nextfree;\
				AP(freeptr)[0].nextfree->prevfree = \
					AP(freeptr)[1].prevfree;\
				(x)->prevfree->nextfree = &(AP(freeptr)[1]);\
				AP(freeptr)[1].prevfree = (x)->prevfree;\
				(x)->prevfree = &(AP(freeptr)[0]);\
				AP(freeptr)[0].nextfree = (x);\
				assert((x)->nextfree != (x));\
				assert((x)->prevfree != (x));

#define COMPACTFWD(b,n)	{ \
			register struct header *nextnext; \
			if (!TESTBUSY(nextnext = (n)->nextblk))  { \
				DELFREEQ(n); \
				(b)->nextblk = nextnext; \
				nextnext->prevblk = (b); \
			} \
			}

#define COMPACTBACK(b)	{ \
			register struct header *next; \
			if (!TESTBUSY((next = (b)->prevblk)->nextblk)) { \
				DELFREEQ(next); \
				next->nextblk = (b)->nextblk; \
				(b)->nextblk->prevblk = next; \
				(b) = next; \
			} \
			}

#ifndef MAX
#define MAX(a,b)	(a > b ? a : b)
#endif
#define ALIGNIT(x)	(((__psint_t)(x) + ALIGNSZ - 1) & ~(ALIGNSZ - 1))
#define ISALIGNED(x)	(((__psint_t)(x) & (ALIGNSZ - 1)) == 0)
/*
 * Determine minimum size of regular block that should be
 * put on free list.  Blocks smaller than this, but large
 * enough to accomodate a header, are put on the ignore
 * free list to be (possibly) coalesced later.
 */
#define SETIGNSZ	AP(ignoresz) = (int)ALIGNIT(AP(maxfast) + 1 + AP(minhead));
/*
	The following manipulate the busy flag
*/
#define BUSY	1L
#define SETBUSY(x)      ((struct header *)((__psint_t)(x) | BUSY))
#define CLRBUSY(x)      ((struct header *)((__psint_t)(x) & ~BUSY))
#define TESTBUSY(x)     ((__psint_t)(x) & BUSY)
/*
	The following manipulate the small block flag
*/
#define SMAL	2L
#define SETSMAL(x)      ((struct lblk *)((__psint_t)(x) | SMAL))
#define CLRSMAL(x)      ((struct lblk *)((__psint_t)(x) & ~SMAL))
#define TESTSMAL(x)     ((__psint_t)(x) & SMAL)
/*
	The following manipulate both flags.  They must be 
	type coerced
*/
#define SETALL(x)       ((__psint_t)(x) | (SMAL | BUSY))
#define CLRALL(x)       ((__psint_t)(x) & ~(SMAL | BUSY))
/*
	Other useful constants
*/
#define TRUE    1
#define FALSE   0

/* number of bytes to align to  (must be at least 4, because lower 2 bits
   are used for flags */
#ifdef LIBMALLOC
#if (_MIPS_SZLONG == 32)
#define ALIGNSZ 8
#else
#define ALIGNSZ 16	/* 2*(sizeof(struct header *)) Min size */
#endif /* _MIPS_SZLONG */
#else
#define ALIGNSZ 16
#endif /* LIBMALLOC */

/*
 * minhead is the larger of either:
 * sizeof an allocated block header OR
 * the alignment factor
 */
/*#define MINHEAD	8		UNUSED - sizeof allocated block header */

#define HEADSZ  ((int)sizeof(struct header))   /* size of unallocated block header */
#define MINBLKSZ	HEADSZ

#define BLOCKSZ (8*1024)	/* memory is gotten from sbrk in
				   multiples of BLOCKSZ */
#define GROUND  (struct header *)0
#define LGROUND (struct lblk *)0	/* ground for a queue within a holding
					   block	*/
#define HGROUND (struct holdblk *)0     /* ground for the holding block queue */
#ifndef NULL
#define NULL    0
#endif
/*
	Structures and constants describing the holding blocks
*/
#define NUMLBLKS	100   		/* default # of small blocks per
						holding block */
/* size of a holding block with small blocks of size blksz */
#define AHOLDSZ		(ALIGNIT(sizeof(struct holdblk)))
#define HOLDSZ(blksz) 	(AHOLDSZ + (blksz)*AP(numlblks))
#define MAXFAST		28		/* default maximum size block for fast
					     allocation */
/*
 * Default maximum number of blocks to check
 * before giving up and calling sbrk().
 */
#define MAXCHECK	100
#if (_MIPS_SZLONG == 32)
#define MAXALLOC	0x80000000	/* maximum bytes/malloc call */
#else
#define MAXALLOC	0x8000000000000000	/* maximum bytes/malloc call */
#endif


#ifdef LIBMALLOC
#define CHECKQ  	if (AP(m_debug)) __checkq();
#else
#define CHECKQ  	if (AP(m_debug)) __checkq(ap);
#endif

/*
 * declaration of arena header for multiple arena malloc
 */
typedef struct arena_s {
	int a_flags;		   /* various flags - see malloc.h */
	/* 
		Variables controlling algorithm, esp. how holding blocs are
		used
	*/
	int a_numlblks;
	int a_minhead;	
	int a_change;		/* != 0, once param changes are no
					longer allowed */
	int a_fastct;
	int a_maxfast;
	int a_ignoresz;
	int a_blocksz;
	int a_maxcheck;
	int a_addtotail;
	/* number of small block sizes to map to one size */
	int a_grain;
	struct header a_arena[2];	/* the second word is a minimal block to
					   start the arena. The first is a busy
					   block to be pointed to by the last
					   block.       */
	struct header a_freeptr[2];	/* first and last entry in free list */
	struct header a_ignorefp[2];	/* first and last entry in list of
					   free blocks too small to consider */
	struct header *a_arenaend; /* ptr to block marking high end of arena */
	struct holdblk **a_holdhead; /* pointer to array of head pointers
					     to holding block chains */
	/* In order to save time calculating indices, the array is 1 too
	   large, and the first element is unused */
	int a_m_debug;		/* whether to check arena each action */
	usptr_t *a_usptr;	/* pointer to header for user sync */
	ulock_t a_lock;		/* lock */
	void *(*a_grow)(size_t, void *);	/* grow function */
	size_t	a_size;	/* max size in bytes for arena */
	void	*a_brk;		/* brk value for adefgrow */
	int	a_grain2;	/* if a_grain is pow2 then this is grain-1 */
	int	a_grain2shift;	/* if a_grain is pow2 then this is shift val */
	int	a_clronfree;	/* if set then clear block on free */
	int	a_clronfreevalue;	/* set to this char value */
	int	a_padbuf[7];	/* some padding for shared lib compatibility */
} arena_t;

#endif /*__MALLINT_H__*/
