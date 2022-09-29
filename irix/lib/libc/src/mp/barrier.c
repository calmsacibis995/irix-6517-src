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
#ident "$Revision: 1.18 $ $Author: ack $"

#pragma weak barrier = _barrier
#pragma weak free_barrier = _free_barrier
#pragma weak new_barrier = _new_barrier
#pragma weak init_barrier = _init_barrier

#include "synonyms.h"
#include "sys/types.h"
#include "ulocks.h"
#include "stdio.h"
#include "errno.h"

/*
 * simple barrier routines
 */

/*
 * new_barrier - allocate a barrier
 * one must specify how many processes to wait for
 * ONLY works for tasks - shared data space
 */

barrier_t *
new_barrier(usptr_t *cookie)
{
	register barrier_t *b;

	if ((b = usmalloc(sizeof(*b), cookie)) == NULL) {
		setoserror(ENOMEM);
		return( NULL);
	}
	
	b->b_usptr = cookie;
	if ((b->b_plock = usnewlock(cookie)) == (ulock_t) NULL) {
		usfree(b, cookie);
		return(NULL);
	}
	b->b_spin = 1;
	b->b_count = 0;
	b->b_inuse = 0;

	return(b);
}

/*
 * init_barrier - initialize a barrier 
 */

void
init_barrier(barrier_t *b)
{

	b->b_spin = 1;
	b->b_count = 0;
	b->b_inuse = 0;
}

void
free_barrier(barrier_t *b)
{
	usfreelock(b->b_plock, b->b_usptr);
	usfree(b, b->b_usptr);
}

/*
 * barrier - wait for n processes to come in, then release them all
 * when released the barrier is ready to be used again - it doesn't
 * need to be re-initialized
 */
void
barrier(
 register barrier_t *b,		/* barrier to wait at */
 register unsigned n)		/* how many processes to wait for */
{
	/* wait till any previous use is done */
	while (b->b_inuse)
		;

	ussetlock(b->b_plock);

	if (b->b_count++ < (n - 1)) {
		/* wait */
		usunsetlock(b->b_plock);
		while (b->b_spin)
			;
		/* we force ALL processes to acquire lock and decrement
		 * b_count so we know when the barrier is free and we can
		 * raise b_spin
		 */
		ussetlock(b->b_plock);
	} else {
		/* done, start letting processes go - mark barrier as
		 * in use so someone doesn't use it again until ALL
		 * processes have been restarted
		 */
		b->b_inuse = 1;
		b->b_spin = 0;
	}

	if (--b->b_count == 0) {
		/* done with barrier */
		b->b_spin = 1;

		/* let anyone waiting to get in in */
		b->b_inuse = 0;
	}
	usunsetlock(b->b_plock);
}
