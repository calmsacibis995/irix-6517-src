#define NDEBUG
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
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

#ident "$Author: sfc $ $Revision: 1.43 $"

#ifdef __STDC__
	#pragma weak calloc = _calloc
	#pragma weak malloc = _malloc
	#pragma weak realloc = _realloc
	#pragma weak free = _free
        #pragma weak mallopt = _mallopt
        #pragma weak mallinfo = _mallinfo
        #pragma weak cfree = _cfree
        #pragma weak mallocblksize = _mallocblksize
        #pragma weak recalloc = _recalloc
        #pragma weak memalign = _memalign
#endif

#include "bstring.h"
#include "assert.h"
#include "malloc.h"
#include "mallint.h"
#include "sys/types.h"
#include "errno.h"
#include "string.h"
#include "ulocks.h"

/*
 * libmalloc specific defines, etc.
 */
extern void *_sbrk(size_t);
#define AP(x)	x
#define GROW(x)	_sbrk(x)
#define ADDAARG0()
#define ADDAARG(x)	(x)

/*
	description of arena, free queue, holding blocks etc.
	WARNING -- freeptr MUST be after arena in loaded image!
*/
static struct header arena[2];	/* the second word is a minimal block to
				   start the arena. The first is a busy
				   block to be pointed to by the last
				   block.       */
static struct header freeptr[2];	/* first and last entry in free list */
static struct header ignorefp[2];	/* first and last entry in list of
					   free blocks too small to consider */
static struct header *arenaend;	/* ptr to block marking high end of arena */
static struct holdblk **holdhead;	/* pointer to array of head pointers
				           to holding block chains */
/* In order to save time calculating indices, the array is 1 too
   large, and the first element is unused */
/* 
	Variables controlling algorithm, esp. how holding blocs are
	used
*/
static int numlblks = NUMLBLKS;
static int change = 0;		  /* != 0, once param changes
				     are no longer allowed */
static int blocksz = BLOCKSZ;
static int addtotail = 1;
static int m_debug = 0;		  /* whether to check arena each action */
static int grain = ALIGNSZ;
static int maxfast = MAXFAST;
static unsigned int maxcheck = MAXCHECK;

/* minhead = MAX(MINHEAD, ALIGNSZ); */
static int minhead = 2*(sizeof(struct header *));

/* The following are computable based on the rest (and not user settable) */
static int fastct;
static int ignoresz;
/* number of small block sizes to map to one size */
static int grain2;
static int grain2shift;
static int clronfree = 0;
static char clronfreevalue;

static void *_lmalloc(size_t);
static void _lfree(void *);
static int ntos(int);

void *
_malloc(size_t nbytes)
{
	register void *ret;

	__LOCK_MALLOC();
	ret = _lmalloc(nbytes);
	__UNLOCK_MALLOC();
	return(ret);
}

void
_free(void *ptr)
{
	__LOCK_MALLOC();
	_lfree(ptr);
	__UNLOCK_MALLOC();
}

/*
 * recalloc - realloc with zeroed new stuff
 * NOTE - this ONLY works if ALL realloced stuff was first alloced
 * via arecalloc (otherwise the extra bytes at the end of
 * a block may not be nulled)
 */
void *
_recalloc(void *ptr, size_t num, size_t size)
{
	register char *mp;
	register size_t oldsize, newsize;	/* actual block sizes */

	num *= size;
	oldsize = mallocblksize(ptr);
	mp = realloc(ptr, num);
	newsize = mallocblksize(mp);

	if (mp) {
		if (newsize > oldsize)
			(void)memset(mp + oldsize, 0, newsize - oldsize);
		else
			(void)memset(mp + num, 0, newsize - num);
	}

	return(mp);
}


/*
 * The main algorithm - keep this section and the part of amalloc
 * up to date
 */
static int freespace(struct holdblk *);
void __checkq(void);
static void addhead(struct header *, struct header *);
static void addtail(struct header *, struct header *);
static size_t _lmallocblksize(void *);
static void *_lmemalign(size_t, size_t);

/*     use level memory allocater (malloc, free, realloc)

	-malloc, free, realloc and mallopt form a memory allocator
	similar to malloc, free, and realloc.  The routines
	here are much faster than the original, with slightly worse
	space usage (a few percent difference on most input).  They
	do not have the property that data in freed blocks is left
	untouched until the space is reallocated.

	-Memory is kept in the "arena", a doubly linked list of blocks.
	These blocks are of 3 types.
		1. A free block.  This is a block not in use by the
		   user.  It has a 4 word header. (See description
		   of the free queue.)
		2. An allocated block.  This is a block the user has
		   requested.  It has a 2 word header, pointing
		   to the next block of any sort.
		3. A permanently allocated block.  This covers space
		   aquired by the user directly through sbrk().  It
		   has a 1 word header, as does 2.
	Blocks of type 1 have the lower bit of the pointer to the
	nextblock = 0.  Blocks of type 2 and 3 have that bit set,
	to mark them busy.

	-Unallocated blocks are kept on an unsorted doubly linked
	free list.  

	-Memory is allocated in blocks, with sizes specified by the
	user.  A circular first-fit startegy is used, with a roving
	head of the free queue, which prevents bunching of small
	blocks at the head of the queue.

	-Compaction is performed at free time of any blocks immediately
	following the freed block.  The freed block will be combined
	with a preceding block during the search phase of malloc.
	Since a freed block is added at the front of the free queue,
	which is moved to the end of the queue if considered and
	rejected during the search, fragmentation only occurs if
	a block with a contiguious preceding block that is free is
	freed and reallocated on the next call to malloc.  The
	time savings of this strategy is judged to be worth the
	occasional waste of memory.

	-Small blocks (of size < MAXSIZE)  are not allocated directly.
	A large "holding" block is allocated via a recursive call to
	malloc.  This block contains a header and NUMLBLKS small blocks.
	Holding blocks for a given size of small block (rounded to the
	nearest ALIGNSZ bytes) are kept on a queue with the property that any
	holding block with an unused small block is in front of any without.
	A list of free blocks is kept within the holding block.
*/

/*
	malloc(nbytes) - give a user nbytes to use
*/

static void *
_lmalloc(register size_t nbytes)
{
	register size_t nb;      /* size of entire block we need */

	/*      on first call, initialize       */
	if (freeptr[0].nextfree == GROUND) {
		/* set up initial defaults */
		AP(grain2) = 0;
		if (((AP(grain) - 1) & AP(grain)) == 0) {
			AP(grain2) = AP(grain) - 1;
			AP(grain2shift) = ntos(AP(grain));
		}

		/* initialize arena */
		AP(arena)[1].nextblk = (struct header *)BUSY;
		AP(arena)[0].nextblk = (struct header *)BUSY;
		AP(arenaend) = &(AP(arena)[1]);
		/* initialize free queue */
		AP(freeptr)[0].nextfree = &(AP(freeptr)[1]);
		AP(freeptr)[1].nextblk = &(AP(arena)[0]);
		AP(freeptr)[1].prevfree = &(AP(freeptr)[0]);
		AP(ignorefp)[0].nextfree = &(AP(ignorefp)[1]);
		AP(ignorefp)[1].prevfree = &(AP(ignorefp)[0]);
		SETIGNSZ;
	}
	if (nbytes == 0)
		return NULL;

	if (nbytes <= AP(maxfast))  {
		/*
			We can allocate out of a holding block
		*/
		register struct holdblk *holdblk; /* head of right sized queue*/
		register struct lblk *lblk;     /* pointer to a little block */
		register int holdn;
		register int chg, g2, g2shift;

		/* load unrolling */
		chg = AP(change);
		g2 = AP(grain2);
		g2shift = AP(grain2shift);

		if (!chg)  {
			register int i;
			/*
			 *	This allocates space for hold block
			 *	pointers by calling malloc recursively.
			 *	Maxfast is temporarily set to 0, to
			 *	avoid infinite recursion.  Allocate
			 *	space for an extra ptr so that an index
			 *	is just (size + MINSMALLHEAD)/grain
			 *	with the first pointer unused.
			 *	Once change is set, changes to algorithm
			 *	parameters are no longer allowed.
			 */
			AP(fastct) = ((AP(maxfast) + MINSMALLHEAD) +
					AP(grain) - 1) / AP(grain);
			/* temporarily alter maxfast, to avoid
			   infinite recursion */
			AP(change) = AP(maxfast);
			AP(maxfast) = 0;
			AP(holdhead) = (struct holdblk **)
				   _lmalloc(ADDAARG(sizeof(struct holdblk *) *
				   (AP(fastct) + 1)));
			if (AP(holdhead) == NULL) {
				/* no more small blocks */
				AP(change) = 0;
				return(_lmalloc(ADDAARG(nbytes)));
			}
			AP(maxfast) = AP(change);
			for(i=1; i<AP(fastct)+1; i++)
				AP(holdhead)[i] = HGROUND;

		}
		/*
		 * Find out which holding block. Add room for header
		 * Round up to grain boundary
		 */
		if (g2) {
			holdn = (int)(nbytes + MINSMALLHEAD + g2);
			holdn >>= g2shift;
			holdblk = AP(holdhead)[holdn];
			nb = holdn << g2shift;
		} else {
			holdn = (int)((nbytes + MINSMALLHEAD + AP(grain) - 1) /
				AP(grain));
			holdblk = AP(holdhead)[holdn];
			nb = holdn * AP(grain);
		}

		/*      look for space in the holding block.  Blocks with
			space will be in front of those without */
		if ((holdblk != HGROUND) && (holdblk->lfreeq != LGROUND))  {
			assert(holdblk->blksz >= nbytes);
			/* there is space */
			lblk = holdblk->lfreeq;
			/* Now make lfreeq point to a free block.
			   If lblk has been previously allocated and 
			   freed, it has a valid pointer to use.
			   Otherwise, lblk is at the beginning of
			   the unallocated blocks at the end of
			   the holding block, so, if there is room, take
			   the next space.  If not, mark holdblk full,
			   and move holdblk to the end of the queue
			*/
			if (lblk < holdblk->unused)  {
				/* move to next holdblk, if this one full */
				if ((holdblk->lfreeq = 
				  CLRSMAL(lblk->header.nextfree)) == LGROUND) {
					AP(holdhead)[holdn] =
							holdblk->nexthblk;
				}
			}  else  if (((char *)holdblk->unused + nb + MINSMALLHEAD) <
				    ((char *)holdblk + holdblk->holdsz))  {
				holdblk->lfreeq = holdblk->unused =
				    (struct lblk *)((char *)holdblk->unused+nb);
			}  else {
				holdblk->lfreeq = LGROUND;
				holdblk->unused += nb;
				AP(holdhead)[holdn] = holdblk->nexthblk;
			}
			/* mark as busy and small */
			lblk->header.holder = (struct holdblk *)SETALL(holdblk);
		}  else  {
			register struct holdblk *newhold;

			/* we need a new holding block */
			assert(ISALIGNED(HOLDSZ(nb)));
			newhold = (struct holdblk *)_lmalloc(ADDAARG(HOLDSZ(nb)));
			if (newhold == NULL)
				return NULL;

			/* add to head of free queue */
			if (holdblk != HGROUND)  {
				newhold->nexthblk = holdblk;
				newhold->prevhblk = holdblk->prevhblk;
				holdblk->prevhblk = newhold;
				newhold->prevhblk->nexthblk = newhold;
			}  else  {
				newhold->nexthblk = newhold->prevhblk = newhold;
			}
			AP(holdhead)[holdn] = newhold;
			/*
			 * set up newhold
			 * Note that freeq and unused point to the last word
			 * of the previous block (the blocks are large enough)
			 */
			lblk = (struct lblk *)((char *)newhold + AHOLDSZ - MINSMALLHEAD);
			newhold->lfreeq = newhold->unused =
			     (struct lblk *)((char *)lblk + nb);
			newhold->blksz = (int)(nb - MINSMALLHEAD);
			newhold->holdsz = (int)HOLDSZ(nb);
			lblk->header.holder = (struct holdblk *)SETALL(newhold);
		}
		CHECKQ;
		return (char *)lblk + MINSMALLHEAD;
	}  else  {
		/*
		**	We need an ordinary block
		*/
		register struct header *blk;
		register struct header *newblk; /* used for creating a block */
		register int tries = 0;		/* number of blocks examined */
		register int try = AP(maxcheck);

		/* get number of bytes we need */
		nb = nbytes + AP(minhead);
		nb = ALIGNIT(nb);
		nb = (nb > MINBLKSZ) ? nb : MINBLKSZ;
		/*
			see if there is a big enough block
			If none exists, you will get to freeptr[1].  
			freeptr[1].next = &arena[0], so when you do the test,
			the result is a large positive number, since arena[0]
			comes before all blocks.  Arena[0] is marked busy so
			that it will not be compacted.  This kludge is for the
			sake of the almighty efficiency.
		*/
		/*   check that a very large request won't cause an inf. loop */

		if (nbytes > MAXALLOC)
			return(NULL);

		blk = AP(freeptr);
		do  {
			blk = blk->nextfree;
			if (tries++ >= try) {
				if (blk != &(AP(freeptr)[1])) {
					MOVEHEAD(blk);
					blk = AP(arenaend)->prevblk;
					if (!TESTBUSY(blk->nextblk)) {
						if (((char *)(blk->nextblk) -
						     (char *)blk) >= nb)
							break;
					}
					blk = &(AP(freeptr)[1]);
				}
				break;
			}
		} while (((char *)(blk->nextblk) - (char *)blk) < nb);

		/* 
		**	if we didn't find a block, get more memory
		*/
		if (blk == &(AP(freeptr)[1]))  {
			register struct header *newend; /* new end of arena */
			register size_t nget;      /* number of words to get */
			register size_t al;	   /* amount newblk off align */
			register struct header *lastblk;

			/* Three cases - 1. There is space between arenaend
					    and the break value that will become
					    a permanently allocated block.
					 2. Case 1 is not true, and the last
					    block is allocated.
					 3. Case 1 is not true, and the last
					    block is free
			*/
			newblk = GROW(0);
			al = (size_t)newblk & (ALIGNSZ - 1);
			lastblk = AP(arenaend)->prevblk;
			if (newblk != (struct header *)((char *)AP(arenaend) + HEADSZ)) {
				/* case 1 */
				/* get size to fetch */
				nget = nb+HEADSZ;
				/* round up to a block */
				/* break into components so ``smart''
				 * compiler won't nop "/BLOCKSZ*BLOCKSZ".
				 */
				nget = nget+AP(blocksz)-1;
				nget /= AP(blocksz);
				nget *= AP(blocksz);
				/* always get aligned amount */
				nget = ALIGNIT(nget);

				/*
				 * if start (newblk) not properly aligned,
				 * get extra space
				 */
				if (al)
					nget += (ALIGNSZ - al);
				assert((((size_t)newblk+nget)%ALIGNSZ) == 0);
				/* get memory */
				if (GROW(nget) == (void *)-1L)
					return NULL;

				/* add to arena */
				newend = (struct header *)((char *)newblk + nget
					 - HEADSZ);
				/* ignore some space to make block aligned */
				if (al)
					newblk = (struct header *)
						 ((char *)newblk +
						 ALIGNSZ - al);
				newend->nextblk = SETBUSY(&(AP(arena)[1]));
				newblk->nextblk = newend;
				AP(arenaend)->nextblk = SETBUSY(newblk);
				newend->prevblk = newblk;
				AP(arena)[1].prevblk = newend;
				newblk->prevblk = AP(arenaend);
				/* adjust other pointers */
				AP(arenaend) = newend;
				blk = newblk;
			} else if (TESTBUSY(lastblk->nextblk))  {
				/* case 2 */
				nget = nb+AP(blocksz)-1;
				nget /= AP(blocksz);
				nget *= AP(blocksz);
				/* always get aligned amount */
				nget = ALIGNIT(nget);
				if (GROW(nget) == (void *)-1L)
					return NULL;

				/* block must be aligned */
				assert(((int)newblk%ALIGNSZ) == 0);
				newend = (struct header *)
				    ((char *)AP(arenaend)+nget);
				newend->nextblk = SETBUSY(&(AP(arena)[1]));
				AP(arenaend)->nextblk = newend;
				newend->prevblk = AP(arenaend);
				AP(arena)[1].prevblk = newend;
				blk = AP(arenaend);
				AP(arenaend) = newend;
			}  else  {
				/* case 3 */
				/* last block in arena is at end of memory and
				   is free */
				nget = nb -
				      ((char *)AP(arenaend) - (char *)lastblk);
				nget = nget+AP(blocksz)-1;
				nget /= AP(blocksz);
				nget *= AP(blocksz);
				/* always get aligned amount */
				nget = ALIGNIT(nget);
				/* if not properly aligned, get extra space */
				if (GROW(nget) == (void *)-1L)
					return NULL;

				assert(((int)newblk%ALIGNSZ) == 0);
				/* combine with last block, put in arena */
				newend = (struct header *)
				    ((char *)AP(arenaend) + nget);
				AP(arenaend) = lastblk->nextblk = newend;
				newend->nextblk = SETBUSY(&(AP(arena)[1]));
				newend->prevblk = lastblk;
				AP(arena)[1].prevblk = newend;
				/* set which block to use */
				blk = lastblk;
				DELFREEQ(blk);
			}
		}  else  {
			register struct header *nblk;      /* next block */

			/* take block found off free queue */
			nblk = blk->nextfree;
			DELFREEQ(blk);
			/*
			 * Make head of free queue immediately follow blk,
			 * unless blk was at the end of the queue.
			 */
			if (nblk != &(AP(freeptr)[1]))   {
				MOVEHEAD(nblk);
			}
		}
		/*
		 * Blk now points to an adequate block.
		 */
		assert(TESTBUSY(blk->prevblk->nextblk));
		assert(TESTBUSY(blk->nextblk->nextblk));
		nbytes = (char *)blk->nextblk - (char *)blk - nb;
		if (nbytes >= MINBLKSZ) {
			/*
			 * Carve out the right size block.
			 * newblk will be the remainder.
			 */
			newblk = (struct header *)((char *)blk + nb);
			newblk->nextblk = blk->nextblk;
			/* mark the block busy */
			blk->nextblk = SETBUSY(newblk);
			/*
			 * If the trailing space is so small it won't
			 * ever fit a regular block, tuck it away on
			 * the ignore free list.  It might be coalesced
			 * when an adjacent block is freed.
			 */
			if (nbytes >= AP(ignoresz)) {
				ADDFREEQ(AP(freeptr), newblk);
			} else {
				ADDFREEQ(AP(ignorefp), newblk);
			}
			newblk->prevblk = blk;
			newblk->nextblk->prevblk = newblk;
		}  else  {
			/* just mark the block busy */
			blk->nextblk = SETBUSY(blk->nextblk);
		}
		assert(ISALIGNED(blk));
		CHECKQ;
		return (char *)blk + AP(minhead);
	}
}


/*      free(ptr) - free block that user thinks starts at ptr 

	input - ptr-1 contains the block header.
		If the header points forward, we have a normal
			block pointing to the next block
		if the header points backward, we have a small
			block from a holding block.
		In both cases, the busy bit must be set
*/

static void
_lfree(register void *ptr)
{
	register struct holdblk *holdblk;       /* block holding blk */
	register struct holdblk *oldhead;       /* former head of the hold 
						   block queue containing
						   blk's holder */

	if (ptr == NULL)			/* protect lazy programmers */
		return;
	if (AP(clronfree)) {
		memset(ptr, AP(clronfreevalue), _lmallocblksize(ADDAARG(ptr)));
	}
	if (TESTSMAL(((struct header *)((char *)ptr - MINSMALLHEAD))->nextblk))  {
		register struct lblk *lblk;     /* pointer to freed block */
		register int offset;		/* choice of header lists */

		lblk = (struct lblk *)CLRBUSY((char *)ptr - MINSMALLHEAD);
		assert((struct header *)lblk < AP(arenaend));
		assert((struct header *)lblk > AP(arena));
		/* allow twits (e.g. awk) to free a block twice */
		if (!TESTBUSY(holdblk = lblk->header.holder)) return;
		holdblk = (struct holdblk *)CLRALL(holdblk);
		/* put lblk on its hold block's free list */
		lblk->header.nextfree = SETSMAL(holdblk->lfreeq);
		holdblk->lfreeq = lblk;
		/* move holdblk to head of queue, if its not already there */
		offset = (holdblk->blksz + MINSMALLHEAD)/AP(grain);
		oldhead = AP(holdhead)[offset];
		if (oldhead != holdblk)  {
			/* first take out of current spot */
			AP(holdhead)[offset] = holdblk;
			holdblk->nexthblk->prevhblk = holdblk->prevhblk;
			holdblk->prevhblk->nexthblk = holdblk->nexthblk;
			/* now add at front */
			holdblk->nexthblk = oldhead;
			holdblk->prevhblk = oldhead->prevhblk;
			oldhead->prevhblk = holdblk;
			holdblk->prevhblk->nexthblk = holdblk;
		}
	}  else  {
		register struct header *blk;	    /* real start of block*/
		register struct header *nxt;      /* nxt = blk->nextblk*/

		blk = (struct header *)((char *)ptr - AP(minhead));
		nxt = blk->nextblk;
		/* take care of twits (e.g. awk) who return blocks twice */
		if (!TESTBUSY(nxt))
			return;
		blk->nextblk = nxt = CLRBUSY(nxt);
		/*
		 * See if we can compact with the following chunk.
		 */
		COMPACTFWD(blk, nxt);
		/*
		 * See if we can compact with the preceeding chunk.
		 */
		COMPACTBACK(blk);
		ADDFREEQ(AP(freeptr), blk);
	}
	CHECKQ
}

/*      realloc(ptr,size) - give the user a block of size "size", with
			    the contents pointed to by ptr.  Free ptr.
*/
void *
_realloc(
	void *ptr,	    /* block to change size of */
	size_t size)	    /* size to change to */
{
	void *newptr;	      /* pointer to user's new block */

	if (ptr == (void *)NULL)
		return(malloc(size));
	if (size == 0) {
		free(ptr);
		return NULL;
	}

	__LOCK_MALLOC();

	if (TESTSMAL(((struct lblk *)((char *)ptr - MINSMALLHEAD))->header.holder))  {
                register struct lblk *lblk;     /* pointer to freed block */
                register struct holdblk *holdblk;       /* block holding blk */

                lblk = (struct lblk *)CLRBUSY((char *)ptr - MINSMALLHEAD);
                holdblk = (struct holdblk *)CLRALL(lblk->header.holder);
		/*
		 * Small blocks cannot be grown.  Its not worth
		 * testing whether the new size still fits to
		 * possibly avoid a very small copy.
		 */
		/* This makes the assumption that even if the user is 
		   reallocating a free block, malloc doesn't alter the contents
		   of small blocks */
		newptr = _lmalloc(ADDAARG(size));
		if (newptr == NULL)  goto check;
		/* this isn't to save time--its to protect the twits */
		if(ptr != newptr)  {
                        (void)memcpy(newptr, ptr,
                                holdblk->blksz > size ? size : holdblk->blksz);
			_lfree(ADDAARG(ptr));
		}
	}  else  {
		register struct header *blk;    /* block ptr is contained in */
		register size_t trusize;      /* size of block, as allocaters
						   see it*/
		register size_t cpysize;      /* amount to copy */
		register struct header *next;   /* block after blk */

		/* get size we really need */
		trusize = ALIGNIT(size + AP(minhead));
		trusize = (trusize >= MINBLKSZ) ? trusize : MINBLKSZ;

		blk = (struct header *)((char *)ptr - AP(minhead));
		next = blk->nextblk;
		/* deal with twits who reallocate free blocks */
		if (!TESTBUSY(next))  {
			DELFREEQ(blk);
			assert(blk->nextblk->prevblk == blk);
			blk->nextblk = SETBUSY(next);
		}
		next = CLRBUSY(next);
		/* make blk as big as possible */
		if (!TESTBUSY(next->nextblk))  {
			/* have 1 block to coalesce - before doing
			 * check that if we do, we get a big enough
			 * block
			 */
			cpysize = (char *)next->nextblk - (char *)blk;
			if (cpysize >= trusize) {
				/*
				 * At most one following block will be free
				 * since we now coalesce blocks in both
				 * directions on free().
				 */
				DELFREEQ(next);
				next = next->nextblk;
				next->prevblk = blk;
				blk->nextblk = SETBUSY(next);
			}
		}  

		cpysize = (char *)next - (char *)blk;
		if (cpysize >= trusize)  {
			/* carve out the size we need */
			register struct header *newblk; /* remainder */
	
			if (cpysize - trusize >= MINBLKSZ) {
				/* carve out the right size block */
				/* newblk will be the remainder */
				newblk = (struct header *)((char *)blk 
					  + trusize);
				newblk->nextblk = next;
				blk->nextblk = SETBUSY(newblk);
				/* at this point, next is invalid */
				next->prevblk = newblk;
				newblk->prevblk = blk;
				/*
				 * If trailing bytes are <= a_maxfast,
				 * store them on the ignore list.
				 */
				if (cpysize - trusize > AP(ignoresz)) {
					ADDFREEQ(AP(freeptr), newblk);
				} else {
					ADDFREEQ(AP(ignorefp), newblk);
				}
			}
			newptr = ptr;
		}  else  {
			/* bite the bullet, and call malloc */
			cpysize = (size > cpysize) ? cpysize : size;
			newptr = _lmalloc(ADDAARG(size));
			if (newptr == NULL)  goto check;
			(void)memcpy(newptr, ptr, cpysize);
			_lfree(ADDAARG(ptr));
		}       
	}
check:
	CHECKQ;
	__UNLOCK_MALLOC();
	return newptr;
}

static void
addhead(register struct header *fptr, register struct header *blk)
{
	blk->prevfree = &(fptr[0]);
	blk->nextfree = fptr[0].nextfree;
	fptr[0].nextfree->prevfree = blk;
	fptr[0].nextfree = blk;
	assert(blk->nextfree != blk);
	assert(blk->prevfree != blk);
}

static void
addtail(register struct header *fptr, register struct header *blk)
{
	blk->nextfree = &(fptr[1]);
	blk->prevfree = fptr[1].prevfree;
	blk->prevfree->nextfree = blk;
	fptr[1].prevfree = blk;
	assert(blk->nextfree != blk);
	assert(blk->prevfree != blk);
}

/*
 * calloc - allocate and clear memory block
 */
void *
_calloc(size_t num, size_t size)
{
	register void *mp;

	num *= size;
	mp = malloc(num);
	if(mp == NULL)
		return(NULL);
	(void)memset(mp, 0, num);
	return(mp);
}

/*
 * convert power of 2 to a shift
 */
static int
ntos(int n)
{
	int i;
	for (i = 0; n; n >>= 1, i++)
		;
	return(i-1);
}

/*      Mallopt - set options for allocation

	Mallopt provides for   control over the allocation algorithm.
	The cmds available are:

	M_MXFAST Set maxfast to value.  Maxfast is the size of the
		 largest small, quickly allocated block.  Maxfast
		 may be set to 0 to disable fast allocation entirely.

	M_NLBLKS Set numlblks   to value.  Numlblks is the number of
		 small blocks per holding block.  Value must be
		 greater than 0.

	M_GRAIN  Set grain to value.  The sizes of all blocks
		 smaller than maxfast are considered to be rounded
		 up to the nearest multiple of grain.    The default
		 value of grain is the smallest number of bytes
		 which will allow alignment of any data type.    Grain
		 will   be rounded up to a multiple of its default,
		 and maxsize will be rounded up to a multiple   of
		 grain.  Value must be greater than 0.

	M_KEEP   Retain data in freed   block until the next malloc,
		 realloc, or calloc.  Value is ignored.
		 This option is provided only for compatibility with
		 the old version of malloc, and is not recommended.

	M_BLKSZ  Changes the amount of memory allocated via sbrk()
		 when malloc needs more memory.  The actual amount
		 allocated is the size needed rounded up to M_BLKSZ.

	M_MXCHK  Set the maximum number of blocks on the free list
		 which will be checked.  If no block is found that
		 is large enough, just call sbrk().

	M_FREEHD Free blocks get put on the head of the free list
		 if ``value'' is set; otherwise on tail of free list.
		 The default is tail.

	M_DEBUG  Turn on sanity checking.  Checks validity of pointers,
		 etc.  Useful only for those hacking malloc itself.

	returns - 0, upon successful completion
		  1, if malloc has previously been called or
		     if value or cmd have illegal values
*/
/*
 * mallopt can ONLY be called (with results) before ANY malloc has occured
 * However, the lock required by malloc calls malloc itself!
 * It seems safe then that we don't bother with a lock on mallopt - it
 * is unlikely that more than 1 process will try to set malloc options on
 * the same arena.
 * The only potential real race is if one process tries to mallopt while
 * another mallocs....
 */

int
_mallopt(int cmd, int value)
{

	switch (cmd)  {
	    case M_DEBUG:
		AP(m_debug) = value;
		return(0);
	    case M_BLKSZ:
		if (value < 512)  {
			return 1;
		}
		AP(blocksz) = (int)ALIGNIT(value);
		return 0;
	    case M_FREEHD:
		AP(addtotail) = !value;
		return 0;
	    case M_MXCHK:
		AP(maxcheck) = value;
		return 0;
	    case M_CLRONFREE:
		AP(clronfree) = 1;
		AP(clronfreevalue) = value;
		return 0;
#ifdef	LEAKY
	    case M_LOG:
		m_logging = value;
		if (!m_logging && (m_logfile >= 0)) {
		    close(m_logfile);
		    m_logfile = -1;
		}
		return 0;
	    case M_LOGFILE:
		m_logfilename = (char*) value;
		if (m_logging && (m_logfile >= 0)) {
		    /* close old log file and switch to new one */
		    close(m_logfile);
		    m_logfile = open(m_logfilename, LOG_MODES, 0644);
		    if (m_logfile < 0) {
			return 1;
		    }
		}
		return 0;
	    case M_LOGDEPTH:
		m_stackdepth = value;
		if (m_stackdepth < 0) m_stackdepth = 0;
		if (m_stackdepth > MAX_PCS) m_stackdepth = MAX_PCS;
		break;
#endif
	}
	/*
	 * Disallow these changes once a small block is allocated.
	 */
	if (AP(change))  {
		return 1;
	}
	switch (cmd)  {
	    case M_MXFAST:
		if (value < 0)  {
			return 1;
		}
		AP(maxfast) = value;
		SETIGNSZ;
		break;
	    case M_NLBLKS:
		if (value <= 1)  {
			return 1;
		}
		AP(numlblks) = value;
		break;
	    case M_GRAIN:
		if (value <= 0)  {
			return 1;
		}
		AP(grain) = (int)ALIGNIT(value);
		AP(grain2) = 0;
		if (((AP(grain) - 1) & AP(grain)) == 0) {
			AP(grain2) = AP(grain) - 1;
			AP(grain2shift) = ntos(AP(grain));
		}
		break;
	    case M_KEEP:
		AP(minhead) = MAX(HEADSZ, AP(minhead));
		SETIGNSZ;
		break;
	    default:
		return 1;
	}
	return 0;
}

/*	mallinfo-provide information about space usage

	input - max; mallinfo will return the size of the
		largest block < max.

	output - a structure containing a description of
		 of space usage, defined in malloc.h
*/
struct mallinfo
_mallinfo(void)
{
	struct header *blk, *next;	/* ptr to ordinary blocks */
	struct holdblk *hblk;		/* ptr to holding blocks */
	struct mallinfo inf;		/* return value */
	register int i;			/* the ubiquitous counter */
	size_t size;			/* size of a block */
	int fsp;			/* free space in 1 hold block */
	int keep;			/* keep cost */

	(void)memset(&inf, 0, sizeof(struct mallinfo));
	__LOCK_MALLOC();

	if (AP(freeptr)->nextfree == GROUND)
	       goto out;
	if ((blk = CLRBUSY(AP(arena)[1].nextblk)) == NULL)
		goto out;
	/* return total space used */
	inf.arena = (char *)AP(arenaend) - (char *)blk;

	/* calc keep cost */
	keep = (AP(minhead) > HEADSZ) ? 0 : HEADSZ - AP(minhead);
	/*
	 * Loop through arena, counting # of blocks
	 * and add space used by blocks.
	 */
	next = CLRBUSY(blk->nextblk);
	while (next != &(AP(arena)[1])) {
		inf.ordblks++;
		size = (char *)next - (char *)blk;
		if (TESTBUSY(blk->nextblk))  {
			inf.uordblks += size;
			inf.keepcost += keep;
		}  else  {
			inf.fordblks += size;
		}
		blk = next;
		next = CLRBUSY(blk->nextblk);
	}
	/* 	if any holding block have been allocated *
	 * 	then examine space in holding blks       */

	if (AP(holdhead)) {
	 for (i=AP(fastct); i>0; i--)  {  /* loop thru ea. chain */
	    hblk = AP(holdhead)[i];
	    if (hblk != HGROUND)  {  /* do only if chain not empty */
	        size = hblk->blksz + MINSMALLHEAD;
		do  {		     /* loop thru 1 hold blk chain */
		    inf.hblks++;    
		    fsp = freespace(hblk);
		    inf.fsmblks += fsp;
		    inf.usmblks += AP(numlblks)*size - fsp;
		    inf.smblks += AP(numlblks);
		    hblk = hblk->nexthblk;
		}  while (hblk != AP(holdhead)[i]);
	    }
	 }
	 inf.hblkhd = ALIGNIT((AP(fastct) + 1) * sizeof(struct holdhead *));
        }
	inf.hblkhd = inf.hblks*AHOLDSZ;
	/* holding block were counted in ordblks, so subtract off
	 * don't forget the holdhead block!
	 */
	inf.ordblks -= (inf.hblks + 1);
	inf.uordblks -= (inf.hblkhd + inf.usmblks + inf.fsmblks);
	inf.keepcost -= inf.hblks*keep;
out:
	__UNLOCK_MALLOC();
	return inf;
}

/*	freespace - calc. how much space is used in the free
		    small blocks in a given holding block

	input - hblk = given holding block

	returns space used in free small blocks of hblk
*/
static int
freespace(register struct holdblk *holdblk)
{
	register struct lblk *lblk;
	register int space = 0;
	register int size;
	register struct lblk *unused;

	lblk = CLRSMAL(holdblk->lfreeq);
	size = holdblk->blksz + MINSMALLHEAD;
	unused = CLRSMAL(holdblk->unused);
	/* follow free chain */
	while ((lblk != LGROUND) && (lblk != unused))  {
		space += size;
		lblk = CLRSMAL(lblk->header.nextfree);
	}
	unused = (struct lblk *)((__psint_t)unused + MINSMALLHEAD);
	if ((__psint_t)unused < (__psint_t)holdblk + holdblk->holdsz)
		space += (__psint_t)holdblk + holdblk->holdsz - (__psint_t)unused;
	return space;
}

 /* cfree is an undocumented, 
		 obsolete function */

/* ARGSUSED */
 void
 _cfree(p, num, size)
 void *p;
 unsigned num, size;
 {
	free(p);
 }


/*
 * allow dynamic debugging
 * Make non static so one call ccall via dbx
 */
void
__checkq(void)
{
	struct header *blk, *next;	/* ptr to ordinary blocks */
	struct holdblk *hblk;		/* ptr to holding blocks */
	register int i;			/* the ubiquitous counter */
	register struct header *p;

	/* check free pointers */
	p = &(AP(freeptr)[0]);
	/* check forward */
	while(p != &(AP(freeptr)[1]))       {
		p = p->nextfree;
		assert(p->prevfree->nextfree == p);
	}
	/* check backward */
	while(p != &(AP(freeptr)[0]))       {
		p = p->prevfree;
		assert(p->nextfree->prevfree == p);
	}
	/* check free pointers */
	p = &(AP(ignorefp)[0]);
	/* check forward */
	while(p != &(AP(ignorefp)[1]))       {
		p = p->nextfree;
		assert(p->prevfree->nextfree == p);
	}
	/* check backward */
	while(p != &(AP(ignorefp)[0]))       {
		p = p->prevfree;
		assert(p->nextfree->prevfree == p);
	}
	/* now check arena */
	blk = CLRBUSY(AP(arena)[1].nextblk);
	next = CLRBUSY(blk->nextblk);
	while (next != &(AP(arena)[1])) {
		blk = next;
		assert(blk <= AP(arenaend));
		assert(blk >= AP(arena));
		next = CLRBUSY(blk->nextblk);
	}

	/* check any holding blocks and pointers */
	if (AP(holdhead)) {
	 for (i=AP(fastct); i>0; i--)  {  /* loop thru ea. chain */
	    hblk = AP(holdhead)[i];
	    if (hblk != HGROUND)  {  /* do only if chain not empty */
		do  {		     /* loop thru 1 hold blk chain */
		    /* we use freespace to walk thru list for us */
		    (void) freespace(hblk);
		    hblk = hblk->nexthblk;
		}  while (hblk != AP(holdhead)[i]);
	    }
	 }
        }
}

size_t
_mallocblksize(void *ptr)
{
	register size_t ret;

	__LOCK_MALLOC();
	ret = _lmallocblksize(ADDAARG(ptr));
	__UNLOCK_MALLOC();
	return(ret);
}

/*
 * Get real number of bytes in block
 */
static size_t
_lmallocblksize(register void *ptr)
{
	register struct holdblk *holdblk;       /* block holding blk */
	size_t n;

	if (ptr == NULL)			/* protect lazy programmers */
		return(0);
	if (TESTSMAL(((struct header *)((char *)ptr - MINSMALLHEAD))->nextblk))  {
		register struct lblk *lblk;     /* pointer to freed block */

		lblk = (struct lblk *)CLRBUSY((char *)ptr - MINSMALLHEAD);
		assert((struct header *)lblk < AP(arenaend));
		assert((struct header *)lblk > AP(arena));
		/* allow twits (e.g. awk) to free a block twice */
		if (!TESTBUSY(holdblk = lblk->header.holder)) return(0);
		holdblk = (struct holdblk *)CLRALL(holdblk);
		n = holdblk->blksz;
	}  else  {
		register struct header *blk;	    /* real start of block*/
		register struct header *next;      /* next = blk->nextblk*/

		blk = (struct header *)((char *)ptr - AP(minhead));
		next = blk->nextblk;
		/* take care of twits (e.g. awk) who return blocks twice */
		if (!TESTBUSY(next))
			return(0);
		n = (char *)CLRBUSY(next) - (char *)ptr;
	}
	return(n);
}

void *
_memalign(size_t align, size_t nbytes)
{
	register void *ret;

	__LOCK_MALLOC();
	ret = _lmemalign(align, ADDAARG(nbytes));
	__UNLOCK_MALLOC();
	return(ret);
}

#if _MIPS_SZPTR == 32
#define _misaligned(p)	(size_t)((p)&3)
#else
#define _misaligned(p)	(size_t)((p)&7)
#endif
#define _roundup(x,y)	(((((__psint_t)x)+((y)-1))/(y))*(y))

static void *
_lmemalign(size_t align, size_t nbytes)
{
	size_t reqsize, frag_size;
	void *p, *pa;
	register struct header *blk, *lblk, *rblk;
	register struct header *next;

        /*
         * check for valid size and alignment parameters
         */
        if (nbytes == 0 || _misaligned(align) || align == 0) {
                return NULL;
        }
	if (align <= ALIGNSZ && (ALIGNSZ % align) == 0)
		return(_lmalloc(ADDAARG(nbytes)));

        /*
         * Malloc enough memory to guarantee that the result can be
         * aligned correctly.
	 * We may need to split the returned block into 3 - before,
	 * the real block and after. So we make sure that we have enough room
	 * 2 'free' headers (malloc will get room for the 'allocated' header)
	 * Since we can't play with small blocks as easily, we always
	 * round up what we need to be larger than the small block
	 * allocator
         */
        nbytes = ALIGNIT(nbytes);
	if (nbytes <= AP(maxfast))
		nbytes = ALIGNIT(AP(maxfast) + 1);
        reqsize = nbytes + align + 2 * HEADSZ;
        p = _lmalloc(ADDAARG(reqsize));
        if (p == NULL)
                return NULL;

	/*
	 * compute address we want to return
	 * Hassle here is when the alignment boundary is less than a
	 * header - in this case we move to the next alignment boundary
	 */
	pa = (void *)_roundup(p, align);

	/* left block will (almost) always be freed */
	lblk = (struct header *)((char *)p - AP(minhead));
	next = CLRBUSY(lblk->nextblk);
	assert(next);

	/*
	 * If by some miracle p is already aligned there is no
	 * left fragment, just a right fragment
	 */
	if (p == pa) {
		assert(ISALIGNED(pa));
		/* we may or may not have a 'right' block */
		frag_size = (char *)next - ((char *)pa + nbytes);
		if (frag_size >= MINBLKSZ) {
			rblk = (struct header *)((char *)pa + nbytes);

			/* there is a right hand fragment */
			rblk->nextblk = next;
			rblk->prevblk = lblk;
			next->prevblk = rblk;

			/* compact free list forward */
			COMPACTFWD(rblk, next);
			if (((char *)rblk->nextblk - (char *)rblk) >= AP(ignoresz)) {
				ADDFREEQ(AP(freeptr), rblk);
			} else {
				ADDFREEQ(AP(ignorefp), rblk);
			}

			lblk->nextblk = SETBUSY(rblk);
		}
		/*
		 * we either have no right frag or have freed it.
		 * we're done
		 */
		CHECKQ;
		return(pa);
	}

	/*
	 * There will be a left fragment, make sure its large enough
	 * for a header
	 */
	if ((char *)pa - (char *)p < HEADSZ)
		pa = (void *)((char *)pa + align);

	assert(((char *)pa - (char *)p) >= HEADSZ);

	/* blk points to the 'real' block that will be returned */
	blk = (struct header *)((char *)pa - AP(minhead));
	/* make sure we don't look like a small block */
	blk->prevfree = 0;

	/*
	 * we may or may not have a 'right' block
	 * Note that 'align' may be smaller than our default arena
	 * alignment. Thus 'pa' may not be aligned according to
	 * the rules that headers must be on alignment boundaries.
	 */
	frag_size = (char *)next - ((char *)ALIGNIT(pa) + nbytes);
	if (frag_size >= MINBLKSZ) {
		rblk = (struct header *)((char *)ALIGNIT(pa) + nbytes);

		/* there is a right hand fragment */
		rblk->nextblk = next;
		rblk->prevblk = blk;
		next->prevblk = rblk;

		/* compact free list forward */
		COMPACTFWD(rblk, next);
		if (((char *)rblk->nextblk - (char *)rblk) >= AP(ignoresz)) {
			ADDFREEQ(AP(freeptr), rblk);
		} else {
			ADDFREEQ(AP(ignorefp), rblk);
		}

		blk->nextblk = SETBUSY(rblk);
	} else {
		/* no right hand fragment */
		next->prevblk = blk;
		blk->nextblk = lblk->nextblk;
		assert(TESTBUSY(blk->nextblk));
	}

	/* now add lblk to free list */
	blk->prevblk = lblk;
	lblk->nextblk = CLRBUSY(blk);
	
	/* never can have a backward free block
	COMPACTBACK(lblk);
	*/
	assert(TESTBUSY(lblk->prevblk->nextblk));
	if (((char *)lblk->nextblk - (char *)lblk) >= AP(ignoresz)) {
		ADDFREEQ(AP(freeptr), lblk);
	} else {
		ADDFREEQ(AP(ignorefp), lblk);
	}

	CHECKQ;
	return(pa);
}
