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

#include "synonyms.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sigevtop.h>
#include "mplib.h"

#if !defined(_LIBC_NOMP)

/* ------------------------------------------------------------------ */

/* Event Thread Ops (SIGEV_THREAD)
 *
 * Maintain a table of event data entries referenced by a key.
 * The key has an index part and a generation part to resolve
 * the execute/tear-down race.
 */

typedef struct {
	uint_t		evtop_gen;	/* generation key */
	uint_t		evtop_flags;

	void		(*evtop_func)(union sigval);
	union sigval	evtop_arg;
	pthread_attr_t	evtop_attr;
	evtop_data_t	evtop_data;	/* additional data for event */
} evtop_t;

/* The table grows dynamically in chunks.
 */
#define EVTOP_CHUNKSIZE	32
#define EVTOP_CHUNKS	32

static int		init_done;
static evtop_t		**evtop_table _INITBSS;
static void		*evtop_lock _INITBSS;

#define evtop_lock()	_mtlib_ctl(MTCTL_RLOCK_LOCK, evtop_lock, 0)
#define evtop_unlock()	_mtlib_ctl(MTCTL_RLOCK_UNLOCK, evtop_lock, 0)
#define evtop_locked()	_mtlib_ctl(MTCTL_RLOCK_ISLOCKED, evtop_lock, 0)

#define evtop_in_use(evtop)	((evtop)->evtop_flags & EVTOP_FBUSY)


/* The following simply combines/dissects a table index, chunk index and
 * a generation number into/from one int sized key.
 */
#define EVKEYSHIFT	18	/* bits for generations */
#define EVKEYMAKE(ti, ci, gen) \
	( ( ((ti) * EVTOP_CHUNKSIZE + (ci)) << EVKEYSHIFT) | (gen))
#define EVKEYUNMAKE(key, ti, ci, gen) \
	*(ti) = ((key) >> EVKEYSHIFT) / EVTOP_CHUNKSIZE,\
	*(ci) = ((key) >> EVKEYSHIFT) % EVTOP_CHUNKSIZE, \
	*(gen) = (key) & ((1 << EVKEYSHIFT) - 1)


static int	evtop_init(void);
static evtop_t	*evtop_get(int);
static int	evtop_cmp(uint_t, evtop_data_t *, evtop_t *);
static void	evtop_zap(evtop_t *);
static void	*evtop_thread(void *);
static void	evtop_exec(int);
static void	evtop_fork_child(void);

#ifdef DEBUG
#	define TRACE(msg)	printf msg
#else
#	define TRACE(msg)
#endif


/* evtop_alloc(ev, flags, data, key)
 *
 * Find entry in table, fill in details and pass back key.
 */
int
__evtop_alloc(const sigevent_t *sev, uint_t flags, evtop_data_t *data, int *key)
{
	int		e;
	evtop_t		*op;
	int		ti;
	int		ci;
	pthread_attr_t	*pa;

	TRACE(("evtop_alloc()"));

	if (e = evtop_init()) {
		return (e);
	}

	evtop_lock();
	for (ti = 0; ti < EVTOP_CHUNKS; ti++) {

		/* Allocate a new chunk if needed */
		if (!(op = evtop_table[ti])) {
			if (!(op = evtop_table[ti] =
		   		malloc(sizeof(evtop_t) * EVTOP_CHUNKSIZE))) {
				evtop_unlock();
				return (EAGAIN);
			}
			memset(op, 0, sizeof(evtop_t) * EVTOP_CHUNKSIZE);
		}

		/* Find a free entry */
		for (ci = 0; ci < EVTOP_CHUNKSIZE; ci++, op++) {
			if (evtop_in_use(op)) {
				continue;
			}

			/* Found one */
			op->evtop_flags = flags | EVTOP_FBUSY;
			evtop_unlock();	/* no events until return */

			*key = EVKEYMAKE(ti, ci, op->evtop_gen);
			if (data) {
				op->evtop_data = *data;
			}
			op->evtop_func = sev->sigev_notify_function;
			op->evtop_arg = sev->sigev_value;
			pa = &op->evtop_attr;
			if (sev->sigev_notify_attributes) {
				*pa = *sev->sigev_notify_attributes;
			} else {
				pthread_attr_init(pa);
			}
			pthread_attr_setdetachstate(pa,PTHREAD_CREATE_DETACHED);
			TRACE(("evtop_alloc key %#x", *key));
			return (0);
		}
	}

	evtop_unlock();
	return (EAGAIN);
}

/* evtop_free(key)
 *
 * Fill in the data part.
 */
void
__evtop_free(int key)
{
	evtop_lock();
	evtop_zap(evtop_get(key));
	evtop_unlock();
}


/* evtop_setid(key, data)
 *
 * Fill in the data part.
 */
void
__evtop_setid(int key, evtop_data_t *data)
{
	evtop_t	*op;

	evtop_lock();
	op = evtop_get(key);
	op->evtop_data = *data;
	evtop_unlock();
}


/* evtop_lookup(typ, data, key)
 *
 * Search the table for this entry and return key
 */
int
__evtop_lookup(uint_t typ, evtop_data_t *data, int *key)
{
	evtop_t	*op;
	int	ti;
	int	ci;

	TRACE(("evtop_lookup()"));

	if (!init_done) {	/* reject with prejudice */
		return (EINVAL);
	}
	evtop_lock();
	for (ti = 0; ti < EVTOP_CHUNKS && (op = evtop_table[ti]); ti++) {
		for (ci = 0; ci < EVTOP_CHUNKSIZE; ci++, op++) {
			if (evtop_in_use(op) && evtop_cmp(typ, data, op)) {
				*key = EVKEYMAKE(ti, ci, op->evtop_gen);
				evtop_unlock();
				return (0);
			}
		}
	}
	evtop_unlock();
	return (ESRCH);
}


static int
evtop_init(void)
{
	int	e;
	LOCKDECL(l);

	if (init_done) {
		return (0);
	}
	LOCKINIT(l, LOCKMISC);
	if (init_done) {
		UNLOCKMISC(l);
		return (0);
	}

	/* We need to allocate a lock and request the thread lib
	 * invoke callbacks and fork child cleanup.
	 */
	if (!MTLIB_ACTIVE()) {
		UNLOCKMISC(l);
		return (EINVAL);
	}
	if ((e = _mtlib_ctl(MTCTL_RLOCK_ALLOC, 1, &evtop_lock))
	    || (e = _mtlib_ctl(MTCTL_EVTSTART, evtop_exec, evtop_fork_child))) {
		UNLOCKMISC(l);
		return (e);
	}

	/* Allocate the table of pointers (saves space)
	 */
	if ((evtop_table = malloc(sizeof(evtop_t *) * EVTOP_CHUNKS)) == 0) {
		UNLOCKMISC(l);
		return (EAGAIN);
	}
	memset(evtop_table, 0, sizeof(evtop_t *) * EVTOP_CHUNKS);

	init_done = 1;
	UNLOCKMISC(l);

	return (0);
}


/* evtop_get(key)
 *
 * Look up entry using key.
 */
static evtop_t *
evtop_get(int key)
{
	int	ti;
	int	ci;
	int	gen;
	evtop_t	*op;

	TRACE(("evtop_get(%#x)", key));
	assert(evtop_locked());

	EVKEYUNMAKE(key, &ti, &ci, &gen);
	assert(ti < EVTOP_CHUNKS);
	assert(ci < EVTOP_CHUNKSIZE);
	assert(evtop_table[ti]);

	op = evtop_table[ti] + ci;
	return ((op->evtop_gen == gen) ? op : 0);
}


/* evtop_cmp(typ, data, op)
 */
static int
evtop_cmp(uint_t typ, evtop_data_t *data, evtop_t *op)
{
	if (!(op->evtop_flags & typ)) {
		return (0);
	}
	switch (typ) {
	case EVTOP_TMR:
	{
		timer_t	tmrid = *(timer_t *)data;
		return (op->evtop_data.evtop_mqid == tmrid);
	}
	case EVTOP_MQ	:
	{
		mqd_t	mqid = *(mqd_t *)data;
		return (op->evtop_data.evtop_mqid == mqid);
	}
	}
	return (0);
}


/* evtop_zap(op)
 *
 * Release the table entry and free
 */
static void
evtop_zap(evtop_t *op)
{
	TRACE(("evtop_zap(0x%p)", op));

	assert(evtop_in_use(op));
	assert(evtop_locked());

	op->evtop_gen++;
	op->evtop_flags = 0;
}


static void *
evtop_thread(void *arg)
{
	evtop_t		*op = (evtop_t *)arg;
	void		(*evtop_func)(union sigval) = op->evtop_func;
	union sigval	evtop_arg = op->evtop_arg;

	free(arg);
	evtop_func(evtop_arg);
	return (0);
}


/* evtop_exec(key)
 *
 * Called to execute op registered for key.
 */
static void
evtop_exec(int key)
{
	pthread_t	pt;
	evtop_t		*op;
	evtop_t		*op_cpy = malloc(sizeof(evtop_t));

	TRACE(("evtop_exec(%#x)", key));

	if (!op_cpy) {
		return;
	}

	evtop_lock();

	/* Lookup key to find info */
	if (!(op = evtop_get(key))) {
		evtop_unlock();
		free(op_cpy);
		return;
	}
	*op_cpy = *op;

	/* Check for request delete on activation */
	if (op->evtop_flags & EVTOP_FFREE) {
		evtop_zap(op);
	}

	evtop_unlock();

	/* Start thread */
	TRACE(("start thread %#p, %#p",
			op_cpy->evtop_func, op_cpy->evtop_arg.sival_ptr));
	if (pthread_create(&pt, &op_cpy->evtop_attr, evtop_thread, op_cpy)) {
		free(op_cpy);
	}
}


static void
evtop_fork_child(void)
{
	int		ti;

	_mtlib_ctl(MTCTL_RLOCK_FREE, evtop_lock);
	for (ti = 0; ti < EVTOP_CHUNKS; ti++) {
		if (!evtop_table[ti]) {
			break;
		}
		free(evtop_table[ti]);
		evtop_table[ti] = 0;
	}
	free(evtop_table);
}

#else	/* _LIBC_NOMP */

/* ARGSUSED */
int
__evtop_alloc(const sigevent_t *sev, uint_t flags, evtop_data_t *data, int *key)
{ return (EINVAL); }

/* ARGSUSED */
void
__evtop_free(int key)
{ return; }

/* ARGSUSED */
int
__evtop_lookup(uint_t typ, evtop_data_t *data, int *key)
{ return (EINVAL); }

/* ARGSUSED */
void
__evtop_setid(int key, evtop_data_t *data)
{ return; }

#endif	/* _LIBC_NOMP */
