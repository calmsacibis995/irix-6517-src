/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
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

#ident "$Author: jwag $ $Revision: 1.47 $"
#undef ADEBUG

#ifdef __STDC__
	#pragma weak amalloc = _amalloc
	#pragma weak afree = _afree
	#pragma weak arealloc = _arealloc
	#pragma weak acalloc = _acalloc
	#pragma weak acreate = _acreate
	#pragma weak adelete = _adelete
	#pragma weak amallopt = _amallopt
	#pragma weak amallinfo = _amallinfo
	#pragma weak amallocblksize = _amallocblksize
	#pragma weak arecalloc = _arecalloc
	#pragma weak amemalign = _amemalign
#endif

#include "synonyms.h"
#include "bstring.h"
#include "assert.h"
#include "stddef.h"
#include "signal.h"
#include "malloc.h"
#include "time.h"
#include "mallint.h"
#include "sys/types.h"
#include "errno.h"
#include "string.h"
#include "ulocks.h"

/*
 * amalloc specific defines, etc.
 */
#define AP(x)	ap->a_##x
#define GROW(x)	adefgrow((x), ap)
#define ADDAARG0()	ap
#define ADDAARG(x)	(x), ap

/* amalloc has slightly different locking criteria - it doesn't look at
 * __usrthread_malloc
 */
#undef __LOCK_MALLOC
#undef __UNLOCK_MALLOC
#define __LOCK_MALLOC() AP(lock) ? ussetlock(AP(lock)) : 0
#define __UNLOCK_MALLOC() AP(lock) ? usunsetlock(AP(lock)) : 1

static void *adefgrow(size_t, void *);
static void *_lmalloc(size_t, arena_t *);
static void _lfree(void *, arena_t *);

void *
amalloc(size_t nbytes, void *p)
{
	register arena_t *ap = p;
	register void *ret;

	if (__LOCK_MALLOC()) {
		ret = _lmalloc(nbytes, ap);
		__UNLOCK_MALLOC();
		return(ret);
	} else
		return(_lmalloc(nbytes, ap));
}

void
afree(void *ptr, void *p)
{
	register arena_t *ap = p;

	__LOCK_MALLOC();
	_lfree(ptr, ap);
	__UNLOCK_MALLOC();
}

/*
 * adefgrow - default grow funciton for amalloc - this is called when
 * malloc needs more memory - it simply doles out bytes until limit is reached
 * If the user set up a grow function then call that.
 * Note that we don't really use the initial size if they provide
 * a grow function..
 */
static void *
adefgrow(size_t need, void *p)
{
	register arena_t *ap = p;
	char *newend;
	void *rv;

	if (AP(grow))
		return(AP(grow)(need, p));

	/* assumes that arena ptr itself is at beginning of arena */
	if (((char *)AP(brk) - (char *)ap) + need >= AP(size)) {
		return((void *)-1L);
	}

	/*
	 * There is still room in the arena - but we may be
	 * a autogrow mapped file and if the file system runs
	 * out of space we would like to return -1, not segv
	 */
	/* compute end */
	newend = (char *)(AP(brk)) + need;

	/*
	 * use a fast system call that we know attempts to copyout
	 * to our location to check if we can access it
	 */
	if ((AP(flags) & MEM_NOAUTOGROW) == 0)
		if (sigprocmask(SIG_NOP, NULL,
			(sigset_t *)((ptrdiff_t)(newend - sizeof(sigset_t)) & ~7)) < 0)
		return((void *)-1L);

	rv = AP(brk);
	AP(brk) = newend;
	return(rv);
}

/*
 * AMALLOC - arbitrary arena malloc
 */

void *
acreate(
  void *startaddr,	/* starting address for arena */
  size_t length,	/* length in bytes of arena - ignored if growable */
  int aflags,
  usptr_t *header,	/* handle on the header (or NULL) */
  void *(*grow)(size_t, void *))	/* grow function */
{
	register arena_t *ap;

	if (header == NULL && (aflags & MEM_SHARED)) {
		setoserror(EINVAL);
		return(NULL);
	}
	if (length < 1024) {
		setoserror(EINVAL);
		return(NULL);
	}
	/* align start address to double boundary */
	ap = (arena_t *)(((ptrdiff_t)startaddr + 0x7) & ~0x7);
	length -= (size_t)((char *)ap - (char *)startaddr);
	memset(ap, 0, sizeof(arena_t));
	AP(flags) = aflags;
	AP(size) = length;

	/* set up initial defaults */
	AP(m_debug) = 0;
	AP(grain) = ALIGNSZ;
#if MINHEAD > ALIGNSZ
	AP(minhead) = MINHEAD;
#else
	AP(minhead) = ALIGNSZ;
#endif
	AP(numlblks) = NUMLBLKS;
	AP(fastct) = 0;
	AP(maxfast) = MAXFAST;
	AP(blocksz) = BLOCKSZ;
	AP(maxcheck) = MAXCHECK;
	AP(change) = 0;
	AP(addtotail) = 1;
	/* initialize arena */
	AP(arena)[1].nextblk = (struct header *)BUSY;
	AP(arena)[1].prevblk = &(AP(arena)[0]);
	AP(arena)[0].nextblk = (struct header *)BUSY;
	AP(arenaend) = &(AP(arena)[1]);
	/* initialize free queue */
	AP(freeptr)[0].nextfree = &(AP(freeptr)[1]);
	AP(freeptr)[1].nextblk = &(AP(arena)[0]);
	AP(freeptr)[1].prevfree = &(AP(freeptr)[0]);
	AP(ignorefp)[0].nextfree = &(AP(ignorefp)[1]);
	AP(ignorefp)[1].prevfree = &(AP(ignorefp)[0]);
	AP(brk) = (void *) ALIGNIT((char *)ap + sizeof(*ap));

	if (grow == NULL) {
		/* use default grow function */
		/* if cannot grow then make sure we can do some mallocing if
		 * they give us a small arena to work with
		 */
		if (length < 64*1024) {
			AP(blocksz) = 512;
			AP(maxfast) = 0;
		}
	}
	AP(grow) = grow;
	SETIGNSZ;

	AP(usptr) = header;
	/* if a shared area then alloc lock */
	if (AP(flags) & MEM_SHARED) {
		if ((AP(lock) = usnewlock(AP(usptr))) == (ulock_t) NULL) {
			return(NULL);
		}
	}

	return(ap);
}

/*
 * adelete - remove an arena
 */
void
adelete(void *p)
{
	register arena_t *ap = p;
	if (AP(lock))
		usfreelock(AP(lock), AP(usptr));
}

/*
 * arecalloc - realloc with zeroed new stuff
 * NOTE - this ONLY works if ALL realloced stuff was first alloced
 * via arecalloc (otherwise the extra bytes at the end of
 * a block may not be nulled)
 */
void *
arecalloc(void *ptr, size_t num, size_t size, void *p)
{
	register char *mp;
	register size_t oldsize, newsize;	/* actual block sizes */

	num *= size;
	oldsize = amallocblksize(ptr, p);
	mp = arealloc(ptr, num, p);
	newsize = amallocblksize(mp, p);

	if (mp) {
		if (newsize > oldsize)
			(void)memset(mp + oldsize, 0, newsize - oldsize);
		else
			(void)memset(mp + num, 0, newsize - num);
	}

	return(mp);
}


/*
 * The main algorithm - keep this section and the part of libmalloc
 * up to date
 */
static int freespace(arena_t *ap, struct holdblk *holdblk);
void __acheckq(arena_t *ap);
static void addhead(struct header *, struct header *);
static void addtail(struct header *, struct header *);
static size_t _lmallocblksize(void *, arena_t *);
static void *_lmemalign(size_t, size_t, arena_t *);

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
_lmalloc(register size_t nbytes, register arena_t *ap)
{
	register size_t nb;      /* size of entire block we need */

	if (nbytes == 0)
		return NULL;

	if (nbytes <= (size_t)(AP(maxfast)))  {
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
			AP(fastct) = ((AP(maxfast) + (int)(MINSMALLHEAD)) +
					AP(grain) - 1) / AP(grain);
			/* temporarily alter maxfast, to avoid
			   infinite recursion */
			AP(change) = AP(maxfast);
			AP(maxfast) = 0;
			AP(holdhead) = (struct holdblk **)
				   _lmalloc(ADDAARG(sizeof(struct holdblk *) *
				   (unsigned int)(AP(fastct) + 1)));
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
			holdn = (int)(nbytes + MINSMALLHEAD) + g2;
			holdn >>= g2shift;
			holdblk = AP(holdhead)[holdn];
			nb = (unsigned int)holdn << g2shift;
		} else {
			holdn = ((int)(nbytes + MINSMALLHEAD) + AP(grain) - 1) /
				AP(grain);
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
#ifdef ADEBUG
			printf("amalloc: want %d bytes\n", nb);
#endif
			/*
			 * alloc more space. Rather than use the old
			 * algorithm that calls GROW(0) we attempt to
			 * compute the worse case space needed so that we
			 * can call GROW with the proper amount. Otherwise
			 * we end up with a non-usable algorithm - GROW
			 * must return an answer as to where the new space
			 * will be, but has no idea how much is needed!
			 */
			lastblk = AP(arenaend)->prevblk;
			/*
			 * To save memory - make a wild hope guess that
			 * the new chunk will be contiguous with the end of
			 * the arena.
			 * We do this only if the last block is free since
			 * that could save us a bunch
			 */
			if (!TESTBUSY(lastblk->nextblk))  {
				nget = nb -
				      ((char *)AP(arenaend) - (char *)lastblk);
				nget = nget+AP(blocksz)-1;
				nget /= AP(blocksz);
				nget *= AP(blocksz);
				assert((nget % ALIGNSZ) == 0);
#ifdef ADEBUG
				printf("amalloc: try contig of %d\n", nget);
#endif
				/* get memory */
				if ((newblk = GROW(nget)) == (struct header *)-1L)
					return NULL;
				if (newblk == (struct header *)((char *)AP(arenaend) + HEADSZ)) {
					/* great! our assumption was correct
					 * proceed straight to go!
					 */
					goto case3;
				}
				/*
				 * Well, now we're kind of back where we
				 * started - a free block at the 'end'
				 * of the arena.
				 * Here we add it as a 'free' block
				 */
				al = (size_t)newblk & (ALIGNSZ - 1);
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

				/* add to free list */
				ADDFREEQ(AP(freeptr), newblk);

				/*
				 * we could loop - but in the case that
				 * the user's grow function always returns
				 * non-contigous values we don't want to go
				 * into an infinite loop... So terminate here
				 * and ask for really how much we need
				 */
				/* Fall Through */
				lastblk = AP(arenaend)->prevblk;
#ifdef ADEBUG
				printf("amalloc: fall through\n");
#endif
			}

			/*
			 * note that blocksz is always a multiple of ALIGNSZ
			 * Worst case is that the new space isn't contiguous
			 * with the old, and we get back an unaligned address
			 */
			nget = nb + HEADSZ + ALIGNSZ - 1;
			nget = nget+AP(blocksz)-1;
			nget /= AP(blocksz);
			nget *= AP(blocksz);
			assert((nget % ALIGNSZ) == 0);
			/* get memory */
			if ((newblk = GROW(nget)) == (struct header *)-1L) {
#ifdef ADEBUG
				printf("amalloc: return NULL on %d request\n",
					nget);
#endif
				return NULL;
			}

			al = (size_t)newblk & (ALIGNSZ - 1);
			if (newblk != (struct header *)((char *)AP(arenaend) + HEADSZ)) {
				/* case 1 */
				/* add to arena */
#ifdef ADEBUG
				printf("amalloc: case 1\n");
#endif
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
				/* in case GROW doesn't give zeroed mem */
				blk->prevfree = NULL;
			} else if (TESTBUSY(lastblk->nextblk))  {
				/* case 2 */
#ifdef ADEBUG
				printf("amalloc: case 2\n");
#endif
				/* block must be aligned */
				assert(((__psunsigned_t)newblk%ALIGNSZ) == 0);
				newend = (struct header *)
				    ((char *)AP(arenaend)+nget);
				newend->nextblk = SETBUSY(&(AP(arena)[1]));
				AP(arenaend)->nextblk = newend;
				newend->prevblk = AP(arenaend);
				AP(arena)[1].prevblk = newend;
				blk = AP(arenaend);
				/* in case GROW doesn't give zeroed mem */
				blk->prevfree = NULL;
				AP(arenaend) = newend;
			}  else  {
case3:
				/* case 3 */
#ifdef ADEBUG
				printf("amalloc: case 3\n");
#endif
				/* last block in arena is at end of memory and
				   is free */
				assert(((__psunsigned_t)newblk%ALIGNSZ) == 0);
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
_lfree(register void *ptr, register arena_t *ap)
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
arealloc(register void *ptr, size_t size, void *p)
{
	register arena_t *ap = p;
	void *newptr;	      /* pointer to user's new block */

	if (ptr == NULL)
		return(amalloc(size, p));
	if (size == 0) {
		afree(ptr, p);
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
 * acalloc - allocate and clear memory block
 */
void *
acalloc(size_t num, size_t size, void *p)
{
	register void *mp;

	num *= size;
	mp = amalloc(num, p);
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
amallopt(int cmd, int value, void *p)
{
	register arena_t *ap = p;

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
	    case M_CRLOCK:
		if ((AP(lock) = usnewlock(AP(usptr))) == (ulock_t) NULL)
			return 1;
		AP(flags) |= MEM_SHARED;
		return 0;
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
amallinfo(void *p)
{
	register arena_t *ap = p;
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
		    fsp = freespace(ap, hblk);
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

/* ARGSUSED */
static int
freespace(register arena_t *ap, register struct holdblk *holdblk)  
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
	unused = (struct lblk *)((ptrdiff_t)unused + MINSMALLHEAD);
	if ((ptrdiff_t)unused < (ptrdiff_t)holdblk + holdblk->holdsz)
		space += (ptrdiff_t)holdblk + holdblk->holdsz - (ptrdiff_t)unused;
	return space;
}

/*
 * allow dynamic debugging
 * Make non static so one call ccall via dbx
 */
void
__acheckq(register arena_t *ap)
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
		    (void) freespace(ap, hblk);
		    hblk = hblk->nexthblk;
		}  while (hblk != AP(holdhead)[i]);
	    }
	 }
        }
}

size_t
amallocblksize(void *ptr, void *p)
{
	register arena_t *ap = p;
	register size_t ret;

	if (__LOCK_MALLOC()) {
		ret = _lmallocblksize(ADDAARG(ptr));
		__UNLOCK_MALLOC();
		return(ret);
	} else
		return(_lmallocblksize(ADDAARG(ptr)));
}

/*
 * Get real number of bytes in block
 */
static size_t
_lmallocblksize(register void *ptr, arena_t *ap)
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
amemalign(size_t align, size_t nbytes, void *p)
{
	register arena_t *ap = p;
	register void *ret;

	if (__LOCK_MALLOC()) {
		ret = _lmemalign(align, ADDAARG(nbytes));
		__UNLOCK_MALLOC();
		return(ret);
	} else
		return(_lmemalign(align, ADDAARG(nbytes)));
}

#if _MIPS_SZPTR == 32
#define _misaligned(p)	(size_t)((p)&3)
#else
#define _misaligned(p)	(size_t)((p)&7)
#endif
#define _roundup(x,y)	(((((ptrdiff_t)x)+((y)-1))/(y))*(y))

static void *
_lmemalign(size_t align, size_t nbytes, arena_t *ap)
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
		nbytes = AP(maxfast) + 4;
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
