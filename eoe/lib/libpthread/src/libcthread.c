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

/* NOTES
 *
 * This module implements libc infrastructure for locking.
 *
 * Key points:
	* Turn on locking in libc always for pthreads (regardless of how
	  many threads exist).
	* Preempt the locking interfaces used for various libc subsystems.
	* Implement recursive locks (under protest).
 */

#include "mtx.h"
#include "mtxattr.h"
#include "pt.h"
#include "pthreadrt.h"
#include "vp.h"

#include <errno.h>
#include <malloc.h>
#include <mplib.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>


/* Recursive mutex for libc.
 */
typedef struct recursive_mutex {
	mtx_t	rm_mtx;
	int	rm_recurse;
} recursive_mutex_t;

static slock_t	*libc_malloc_lock;	/* fork() ick */

/* ------------------------------------------------------------------ */


static int	libc_rlock_alloc(int, recursive_mutex_t **);
static int	libc_rlock_lock(recursive_mutex_t *, int);
static int	libc_rlock_trylock(recursive_mutex_t *, int);
static int	libc_rlock_islocked(recursive_mutex_t *, int);
static int	libc_rlock_unlock(recursive_mutex_t *, int);

static int	libc_lock_alloc(int, slock_t **);
static int	libc_lock_lock(slock_t *, int);
static int	libc_lock_unlock(slock_t *, int);

typedef int (*ptctl_func_t)(machreg_t, machreg_t, machreg_t,
			    machreg_t, machreg_t);


/* C runtime thread control jump table
 */

ptctl_func_t ptctl_tab[] = {
	(ptctl_func_t)libc_rlock_alloc,		/* 0 */
	(ptctl_func_t)libc_rlock_lock,
	(ptctl_func_t)libc_rlock_trylock,
	(ptctl_func_t)libc_rlock_islocked,
	(ptctl_func_t)libc_rlock_unlock,
	(ptctl_func_t)_free,

	(ptctl_func_t)libc_lock_alloc,		/* 6 */
	(ptctl_func_t)libc_lock_lock,
	(ptctl_func_t)libc_lock_unlock,
	(ptctl_func_t)_free,

	(ptctl_func_t)libc_sem_init,		/* 10 */
	(ptctl_func_t)libc_sem_getvalue,
	(ptctl_func_t)libc_sem_post,
	(ptctl_func_t)libc_sem_wait,
	(ptctl_func_t)libc_sem_trywait,
	(ptctl_func_t)libc_sem_destroy,

	(ptctl_func_t)libc_blocking,		/* 16 */
	(ptctl_func_t)libc_unblocking,

	(ptctl_func_t)libc_fork,		/* 18 */
	(ptctl_func_t)libc_sched_yield,
	(ptctl_func_t)libc_setrlimit,
	(ptctl_func_t)libc_sysconf,
	(ptctl_func_t)libc_exit,

	(ptctl_func_t)libc_raise,		/* 23 */
	(ptctl_func_t)libc_sigaction,
	(ptctl_func_t)libc_siglongjmp,
	(ptctl_func_t)libc_sigpending,
	(ptctl_func_t)libc_sigprocmask,
	(ptctl_func_t)libc_sigsuspend,
	(ptctl_func_t)libc_sigtimedwait,
	(ptctl_func_t)libc_sigwait,

	(ptctl_func_t)libc_evt_start,		/* 31 */

	(ptctl_func_t)libc_cncl_test,		/* 32 */
	(ptctl_func_t)libc_cncl_list,

	(ptctl_func_t)libc_threadbind,		/* 34 */
	(ptctl_func_t)libc_threadunbind,

	(ptctl_func_t)libc_ptpri,		/* 36 */
	(ptctl_func_t)libc_pttokt,
	(ptctl_func_t)libc_ptuncncl,

	(ptctl_func_t)pthread_attr_init,
	(ptctl_func_t)pthread_attr_setdetachstate,
	(ptctl_func_t)pthread_cond_broadcast,
	(ptctl_func_t)pthread_cond_init,
	(ptctl_func_t)pthread_cond_signal,
	(ptctl_func_t)pthread_cond_wait,
	(ptctl_func_t)pthread_create,
	(ptctl_func_t)pthread_exit,
	(ptctl_func_t)pthread_mutex_init,
	(ptctl_func_t)pthread_mutex_lock,
	(ptctl_func_t)pthread_mutex_unlock,
	(ptctl_func_t)pthread_rwlock_init,
	(ptctl_func_t)pthread_rwlock_rdlock,
	(ptctl_func_t)pthread_rwlock_unlock,
	(ptctl_func_t)pthread_rwlock_wrlock,
	(ptctl_func_t)pthread_self,
	(ptctl_func_t)pthread_sigmask,
	(ptctl_func_t)pthread_testcancel,
	(ptctl_func_t)pthread_mutex_destroy,
	(ptctl_func_t)pthread_mutex_trylock,
	(ptctl_func_t)pthread_cond_destroy,
	(ptctl_func_t)pthread_cond_timedwait,
	(ptctl_func_t)pthread_attr_destroy,
	(ptctl_func_t)pthread_attr_setstacksize,
	(ptctl_func_t)pthread_attr_setstackaddr,
	(ptctl_func_t)pthread_getconcurrency,
	(ptctl_func_t)pthread_setconcurrency,
};
extern int assert_this[sizeof(ptctl_tab) / sizeof(ptctl_func_t) == MTCTL_MAX];


static int ptctl(int, machreg_t, machreg_t, machreg_t,
		      machreg_t, machreg_t);

/* ------------------------------------------------------------------ */


/*
 * Initialize all libc-defined locks.
 * No need to worry about cleaning up when returning an error --
 * the program is about to abort!
 */
int
libc_locks_init(void)
{
	extern int	__new_libc_threadinit(int, int (*)());

	/* libc thread aware run-time */
	return (__new_libc_threadinit(MT_PTHREAD, ptctl));
}


/* ------------------------------------------------------------------ */

static int
ptctl(int cmd, machreg_t a0, machreg_t a1, machreg_t a2,
	       machreg_t a3, machreg_t a4)
{
	TRACE( T_LIBC, ("callback %d", cmd) );
	ASSERT(cmd < MTCTL_MAX);
	return (ptctl_tab[cmd](a0, a1, a2, a3, a4));
}


/* ------------------------------------------------------------------ */


static int
libc_rlock_alloc(int count, recursive_mutex_t **rmt)
{
	ulong_t	bytes = count * sizeof(recursive_mutex_t);

	if (!(*rmt = _malloc(bytes))) {
		return (ENOMEM);
	}
	memset(*rmt, 0, bytes);
	return (0);
}


static int
libc_rlock_lock(recursive_mutex_t *rmt, int n)
{
	recursive_mutex_t	*rm = rmt + n;

	if (mtx_get_owner(&rm->rm_mtx) != PT) {
		(void)pthread_mutex_lock(&rm->rm_mtx);

		ASSERT(mtx_get_owner(&rm->rm_mtx) == PT);
		ASSERT(rm->rm_recurse == 0);
	} else {
		rm->rm_recurse++;
	}
	return (1);
}


static int
libc_rlock_trylock(recursive_mutex_t *rmt, int n)
{
	recursive_mutex_t	*rm = rmt + n;

	if (mtx_get_owner(&rm->rm_mtx) != PT) {
		return (pthread_mutex_trylock(&rm->rm_mtx));
	}
	rm->rm_recurse++;
	return (0);
}


static int
libc_rlock_islocked(recursive_mutex_t *rmt, int n)
{
	return (mtx_get_owner(&rmt[n].rm_mtx) != 0);
}


static int
libc_rlock_unlock(recursive_mutex_t *rmt, int n)
{
	recursive_mutex_t	*rm = rmt + n;

	ASSERT(mtx_get_owner(&rm->rm_mtx) == PT);
	ASSERT(rm->rm_recurse >= 0);

	if (rm->rm_recurse == 0) {
		(void)pthread_mutex_unlock(&rm->rm_mtx);
	} else {
		rm->rm_recurse--;
	}
	return (0);
}


/* ------------------------------------------------------------------ */


static int
libc_lock_alloc(int count, slock_t **slt)
{
	ulong_t	bytes = count * sizeof(slock_t);

	if (!(*slt = _malloc(bytes))) {
		return (ENOMEM);
	}
	libc_malloc_lock = *slt;	/* for fork() reset */
	memset(*slt, 0, bytes);
	return (0);
}


static int
libc_lock_lock(slock_t *slt, int n)
{
	sched_enter();
	lock_enter(slt + n);
	return (1);
}


static int
libc_lock_unlock(slock_t *slt, int n)
{
	lock_leave(slt + n);
	sched_leave();
	return (1);
}


/* ------------------------------------------------------------------ */


/*
 * libc_fork_prepare()
 * libc_fork_parent()
 * libc_fork_child()
 *
 * Currently we only sync up the malloc lock wrt fork().
 * We need to reset the lock in the child rather than waking up
 * any waiters who are now part of the parent.
 */
void
libc_fork_prepare(void)
{
	(void)libc_lock_lock(libc_malloc_lock, 0);
}

void
libc_fork_parent(void)
{
	(void)libc_lock_unlock(libc_malloc_lock, 0);
}

void
libc_fork_child(void)
{
	lock_init(libc_malloc_lock);
	sched_leave();
}


/* ------------------------------------------------------------------ */

