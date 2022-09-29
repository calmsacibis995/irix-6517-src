/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/atomic_ops.h>
#include <ksys/cacheops.h>
#include "hw_copy.h"

extern hw_copy_engine mte_engine;
extern hw_copy_engine vice_engine;


#define lock_engine(index) atomicAddInt(&hw_copy_locks[index], 1)
#define free_engine(index) (hw_copy_locks[index] = 0)

static int hw_cpu_delay( void );

static hw_copy_engine good_old_cpu = {
	NULL,
	NULL, NULL, 
	hw_cpu_delay, NULL,
	0, 0, 0
};

static hw_copy_engine *engines[] = { 
	&mte_engine, &vice_engine, &good_old_cpu
};

#define HW_COPY_ENGINE_COUNT (sizeof(engines)/sizeof(engines[0]))
#define CPU_ENGINE (HW_COPY_ENGINE_COUNT-1)

static int hw_copy_locks[HW_COPY_ENGINE_COUNT];

static int alloc_engine(const void *from, const void *to);


#ifdef DEBUG
int hw_copy_disable = 0;
#endif

void
hw_copy_init()
{
	int i;

#ifdef DEBUG
	if (hw_copy_disable)
		return;
#endif

	/* XXX no CRIME for now */
	hw_copy_locks[0] = 1;

	for (i = HW_COPY_ENGINE_COUNT; --i >= 0; ) {
		hw_copy_engine *p = engines[i];

		if (p->init != NULL && (*p->init)())
			/* Permanently disable an engine whose init
			 * routine returns non-zero.
			 */
			hw_copy_locks[i] = 1;
	}
}

#define CONTIG(VADDR) (_PAGESZ - poff(VADDR))

void
hw_copy(void *from, void *to, int bcount, unsigned int flags)
{
	int winner;
	int must_iterate;
	int chunk;

	winner = alloc_engine(from, to);

	if (winner == CPU_ENGINE) {
		bcopy(from, to, bcount);
		return;
	}

	if (!(flags & HW_COPY_FROM_NOT_CACHED)) {
		__dcache_wb_inval((void *)from, bcount);
	}
	if (!(flags & HW_COPY_TO_NOT_CACHED)) {
		/* does the right thing with non-aligned regions */
		__dcache_inval(to, bcount);
	}

	must_iterate = (!(engines[winner]->restrictions & HW_COPY_CONTIGUOUS) ||
			(flags & HW_COPY_CONTIGUOUS));
	do {
		if (must_iterate && !(engines[winner]->max_size)) {
			chunk = bcount;
		} else if (must_iterate && engines[winner]->max_size) {
		        chunk = min(bcount, engines[winner]->max_size);
		} else {
			/* If the engine needs the src and dst to be one
			 * physically contiguous chunk, then either the
			 * caller must assure us that this is so, or we
			 * assume that it isn't (and break up the
			 * operation so as not to cross page boundaries).
			 */
			/* It may be worthwhile to see if the phys pages
			 * happen to be advancent, instead of assuming 
			 * the worst case -wsm7/2/96.
			 */

			int fromchunk = CONTIG(from);
			int tochunk = CONTIG(to);

			chunk = min(fromchunk, tochunk);
			chunk = min(chunk, bcount);
		}

		(*engines[winner]->copy_op)(kvtophys(from), 
					    kvtophys(to),
					    chunk);
		from = (void *)((__psunsigned_t)from + chunk);
		to   = (void *)((__psunsigned_t)to   + chunk);
		bcount -= chunk;
	} while (bcount);

	free_engine(winner);

	if ((flags & HW_COPY_ASYNC) || engines[winner]->spin == NULL)
		return;
	(*engines[winner]->spin)();
}


void
hw_zero(void *addr, int bcount, u_int flags)
{
	int winner;
	int must_iterate;
	int chunk;

	winner = alloc_engine(NULL, NULL);

	if (winner == CPU_ENGINE) {
		bzero(addr, bcount);
		return;
	}

	if (!(flags & HW_COPY_ADDR_NOT_CACHED)) {
		/* does the right thing with non-aligned regions */
		__dcache_inval(addr, bcount);
	}

	must_iterate = (!(engines[winner]->restrictions & HW_COPY_CONTIGUOUS) ||
			(flags & HW_COPY_CONTIGUOUS));
	do {
		if (must_iterate && !(engines[winner]->max_size)) {
			chunk = bcount;
		} else if (must_iterate && engines[winner]->max_size) {
		        chunk = min(bcount, engines[winner]->max_size);
		} else {
			int addrchunk = CONTIG(addr);

			chunk = min(addrchunk, bcount);
		}

		(*engines[winner]->zero_op)(kvtophys(addr), chunk);

		addr = (void *)((__psunsigned_t)addr + chunk);
		bcount -= chunk;
	} while (bcount);


	free_engine(winner);

	if ((flags & HW_COPY_ASYNC) || engines[winner]->spin == NULL)
		return;
	(*engines[winner]->spin)();
}


static int
alloc_engine(const void *from, const void *to)
{
	int i;
	int winner_time = HW_COPY_NOT_AVAILABLE - 1;
	int winner;

	do
	{
		for (i = HW_COPY_ENGINE_COUNT; --i >= 0; )
		{
			hw_copy_engine *p;
			int delay;

			if (hw_copy_locks[i])
				continue;

			p = engines[i];
			if (p->alignment_mask && 
			    ((u_int32_t)from & p->alignment_mask) != 
			    ((u_int32_t)to   & p->alignment_mask))
				continue;
				
			delay = (*p->delay)();
			if (delay < winner_time) {
				winner_time = delay;
				winner = i;
			}
		}

		if (winner == CPU_ENGINE)
			return(winner);

		/* We made our selection, now make sure that someone didn't
		 * come in at a higher interrupt level and steal it
		 */
	} while (lock_engine(winner) != 1);

	return(winner);
}


/* This will win iff all other modules returned HW_COPY_NOT_AVAILABLE */

static int
hw_cpu_delay()
{
	return HW_COPY_NOT_AVAILABLE - 2;
}
