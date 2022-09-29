/**************************************************************************
 *									  *
 * Copyright (C) 1986-1995 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __MPLIB_H__
#define __MPLIB_H__

#ident "$Revision: 1.13 $"

#if !defined(_LIBC_NOMP)
#define MT_NONE		0	/* no sprocs yet/ not a pthread app */
#define MT_SPROC	1	/* sprocs created */
#define MT_PTHREAD	2	/* pthreads app */
#endif

#if !defined(LANGUAGE_ASSEMBLY)
#include "assert.h"
#include "stddef.h"
#include "libc_interpose.h"

enum {
	MTCTL_RLOCK_ALLOC,
	MTCTL_RLOCK_LOCK,
	MTCTL_RLOCK_TRYLOCK,
	MTCTL_RLOCK_ISLOCKED,
	MTCTL_RLOCK_UNLOCK,
	MTCTL_RLOCK_FREE,

	MTCTL_LOCK_ALLOC,
	MTCTL_LOCK_LOCK,
	MTCTL_LOCK_UNLOCK,
	MTCTL_LOCK_FREE,

	MTCTL_SEM_ALLOC,
	MTCTL_SEM_GETVALUE,
	MTCTL_SEM_POST,
	MTCTL_SEM_WAIT,
	MTCTL_SEM_TRYWAIT,
	MTCTL_SEM_FREE,

	MTCTL_BLOCKING,
	MTCTL_UNBLOCKING,

	MTCTL_FORK,
	MTCTL_SCHED_YIELD,
	MTCTL_SETRLIMIT,
	MTCTL_SYSCONF,
	MTCTL_EXIT,

	MTCTL_RAISE,
	MTCTL_SIGACTION,
	MTCTL_SIGLONGJMP,
	MTCTL_SIGPENDING,
	MTCTL_SIGPROCMASK,
	MTCTL_SIGSUSPEND,
	MTCTL_SIGTIMEDWAIT,
	MTCTL_SIGWAIT,

	MTCTL_EVTSTART,

	MTCTL_CNCL_TEST,
	MTCTL_CNCL_LIST,

	MTCTL_BIND_HACK,	/* gfx */
	MTCTL_UNBIND_HACK,

	MTCTL_PRI_HACK,		/* sem/mq */
	MTCTL_KTID_HACK,	/* frs */
	MTCTL_UNCNCL_HACK,	/* DCE */

	MTCTL_PTHREAD_ATTR_INIT,
	MTCTL_PTHREAD_ATTR_SETDETACHSTATE,
	MTCTL_PTHREAD_COND_BROADCAST,
	MTCTL_PTHREAD_COND_INIT,
	MTCTL_PTHREAD_COND_SIGNAL,
	MTCTL_PTHREAD_COND_WAIT,
	MTCTL_PTHREAD_CREATE,
	MTCTL_PTHREAD_EXIT,
	MTCTL_PTHREAD_MUTEX_INIT,
	MTCTL_PTHREAD_MUTEX_LOCK,
	MTCTL_PTHREAD_MUTEX_UNLOCK,
	MTCTL_PTHREAD_RWLOCK_INIT,
	MTCTL_PTHREAD_RWLOCK_RDLOCK,
	MTCTL_PTHREAD_RWLOCK_UNLOCK,
	MTCTL_PTHREAD_RWLOCK_WRLOCK,
	MTCTL_PTHREAD_SELF,
	MTCTL_PTHREAD_SIGMASK,
	MTCTL_PTHREAD_TESTCANCEL,
	MTCTL_PTHREAD_MUTEX_DESTROY,
	MTCTL_PTHREAD_MUTEX_TRYLOCK,
	MTCTL_PTHREAD_COND_DESTROY,
	MTCTL_PTHREAD_COND_TIMEDWAIT,
	MTCTL_PTHREAD_ATTR_DESTROY,
	MTCTL_PTHREAD_ATTR_SETSTACKSIZE,
	MTCTL_PTHREAD_ATTR_SETSTACKADDR,
	MTCTL_PTHREAD_GETCONCURRENCY,
	MTCTL_PTHREAD_SETCONCURRENCY,

	MTCTL_MAX
};

/*
 * macros/defines for threading stdio
 */
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
#include <errno.h>

extern int	_mplib_get_thread_type(void);
extern unsigned	_mplib_get_thread_id(void);
extern void mt_debug(int level, char *str, ...);

/* Multi-thread callbacks
 *
 * C runtime operations that are thread sensitive use these
 * callbacks to the thread runtime to get the right semantic.
 * The function pointer is registered by the thread run-time
 * when (if) it starts.
 */
extern int	(*_mtlib_ctl)(int, ...);
#define MTLIB_ACTIVE()	(_mtlib_ctl != 0)


/* Blocking interface macros.
 *
 * The following macros implement a callback mechanism to
 * implement user level signals, cancellation and other thread
 * operations such as execution vehicle starvation.
 * They replace symbol preemption as a mechanism because preemption
 * breaks down if the DSO containing the alternatives is loaded after
 * the symbols it preempts are resolved.
 *
 * They are removed for ABI and NOMP libs.
 *
 * Each macro checks for threads and if not present performs
 * the original operation.
 *
 * Otherwise:
 *	1. MT callback to notify the thread runtime we may block.
 *	1a.  Cancellation processing may be requested in this call.
 *	2. Evaluate the operation.
 *	3. MT callback to notify the thread runtime we are unblocked.
 *	3a.  Notify callback whether the operation was interrupted.
 *	4. Retry the operation if requested to do so by the callback.
 *
 * Supplementary notes:
 *	__MTLIB_BLOCK() and __MTLIB_BLOCK_1() should not be used outside
 *	this file.
 *	Callers select the operation, cancellation and the manner in
 *	which the result is returned.
 *	Step 4 retries the operation in the case that it was interrupted
 *	by the thread run-time.
 *	Some operations cannot be restarted: e.g. timeouts, the suffix "_1"
 *	macro versions provide no-retry variant of step 4.
 *
 * Usage:
 *	The most common use to implement cancellation+blocking around
 *	a system call, and return the syscall value directly.
 *	A variant is to retrieve the syscall value instead of returning.
 *	Lastly there is a version which blocks without cancellation.
 *
 *	Conditional cancellation is implemented by testing whether the
 *	call was made from within the C library.  If it was no cancellation
 *	checking is performed and the call is not treated as a cancellation
 *	point.
 *	This presents a problem if an interface which is a cancellation
 *	point calls another which (eventually) blocks.  Normally, the
 *	cancellation would be implemented at the lower level but then
 *	the conditional check would fail and cancellation would be ignored.
 *	In these cases we "in-line" the lower level so that the cancellation
 *	condition takes place at the right time.
 *	Alternative cancellation schemes involve an additional layer of
 *	interfaces or explicit code in all internal libc calls both
 *	of which are even uglier.
 */

/* Conditional cancellation based on caller location */
extern int	_mtlib_libc_cncl(void *);
#define __MTLIB_COND_CNCL()	_mtlib_libc_cncl((void*)__return_address)
	

/* Blocking cancellation return.
 * Evaluate expression as return value of current routine.
 * Usually wraps system calls.
 */
#define MTLIB_BLOCK_CNCL_RET(_type, _expr)		\
							\
	__MTLIB_BLOCK(	_expr, 				\
			return,		/* !MT */	\
			_type _ret, return _ret, _ret, __MTLIB_COND_CNCL());

/* Blocking cancellation call.
 * Evaluate expression in context of current routine.
 */
#define MTLIB_BLOCK_CNCL_VAL(_val, _expr)		\
							\
	__MTLIB_BLOCK(_expr, 				\
			_val=,		/* !MT */	\
			/*0*/, /*0*/, _val, __MTLIB_COND_CNCL());

/* Blocking non-cancellation return.
 * Evaluate expression as return value of current routine; no cancellation.
 */
#define MTLIB_BLOCK_NOCNCL_RET(_type, _expr)		\
							\
	__MTLIB_BLOCK(_expr, 				\
			return,		/* !MT */	\
			_type _ret, return _ret, _ret, 0);

/* Basic blocking/cancel macro
 * Threaded case notifies thread run-time before call and afterward
 * with an indication of whether the call was interrupted.
 */
#define __MTLIB_BLOCK(_expr, _nomt, _mt1, _mt2, _ret, _cc) {		\
									\
	if (!MTLIB_ACTIVE()) {						\
		_nomt (_expr);	/* Not MT path*/			\
	} else {							\
		int	_flags;						\
		_mt1;							\
									\
		do {							\
			_flags = _mtlib_ctl(MTCTL_BLOCKING, _cc);	\
			_ret = _expr;					\
		} while (_mtlib_ctl(MTCTL_UNBLOCKING,			\
				    _ret == -1 && oserror() == EINTR,	\
				    _flags));				\
		_mt2;							\
	}								\
}

/* Blocking cancellation variation.
 * As above except that we never retry the expression.
 * Usually for timeouts.
 */
#define MTLIB_BLOCK_CNCL_RET_1(_type, _expr)		\
							\
	__MTLIB_BLOCK_1(_expr, 				\
			return,		/* !MT */	\
			_type _ret, return _ret, _ret, __MTLIB_COND_CNCL());

#define MTLIB_BLOCK_CNCL_VAL_1(_val, _expr)		\
							\
	__MTLIB_BLOCK_1(_expr, 				\
			_val=,		/* !MT */	\
			/*0*/, /*0*/, _val, __MTLIB_COND_CNCL());

#define __MTLIB_BLOCK_1(_expr, _nomt, _mt1, _mt2, _ret, _cc) {		\
									\
	if (!MTLIB_ACTIVE()) {						\
		_nomt (_expr);	/* Not MT path */			\
	} else {							\
		int	_flags = _mtlib_ctl(MTCTL_BLOCKING, _cc);	\
		_mt1;							\
									\
		_ret = _expr;						\
		(void)_mtlib_ctl(MTCTL_UNBLOCKING, _ret != 0, _flags);	\
		_mt2;							\
	}								\
}

/* Cancellation point.
 */
#define MTLIB_CNCL_TEST()				\
							\
	if (MTLIB_ACTIVE() && __MTLIB_COND_CNCL()) {	\
		(void)_mtlib_ctl(MTCTL_CNCL_TEST);	\
	}

/* Helper macros.
 */
#define MTLIB_VAL(_args, _v)	(MTLIB_ACTIVE() ? _mtlib_ctl _args : _v)
#define MTLIB_RETURN(_args)	if (MTLIB_ACTIVE()) { return _mtlib_ctl _args; }
#define MTLIB_INSERT(_args)	if (MTLIB_ACTIVE()) { _mtlib_ctl _args; }
#define MTLIB_STATUS(_args, _err) \
	if (MTLIB_ACTIVE()) { _err = ( _mtlib_ctl _args ); }


/* Libc locking
 */
#define LOCKFILE(x) \
        (__us_rsthread_stdio) ?  __libc_lockfile(x) : 0

#define ISLOCKFILE(x) \
        (__us_rsthread_stdio) ?  __libc_islockfile(x) : 1

#define UNLOCKFILE(x,v) \
	{ if (v) __libc_unlockfile(x, v); }

#define LOCKOPEN \
        (__us_rsthread_stdio) ?  __libc_lockopen() : 0

#define ISLOCKOPEN \
        (__us_rsthread_stdio) ?  __libc_islockopen() : 1

#define UNLOCKOPEN(v) \
	{ if (v) __libc_unlockopen(v); }

#define LOCKDIR \
	(__us_rsthread_misc) ? __libc_lockdir() : 0

#define UNLOCKDIR(v) \
	{ if (v) __libc_unlockdir(v); }

#define LOCKMISC \
	(__us_rsthread_misc) ? __libc_lockmisc() : 0

#define UNLOCKMISC(v) \
	{ if (v) __libc_unlockmisc(v); }

#define LOCKLOCALE \
        (__us_rsthread_misc) ?  __libc_locklocale() : 0

#define ISLOCKLOCALE \
        (__us_rsthread_misc) ?  __libc_islocklocale() : 1

#define UNLOCKLOCALE(v) \
	{ if (v) __libc_unlocklocale(v); }

#define	LOCKDECLINIT(l,i) \
	void *l = i
#define	LOCKDECL(l) \
	void *l
#define	LOCKINIT(l,i) \
	l = i

#else	/* !(_LIBC_ABI || _LIBC_NOMP */

#define MTLIB_ACTIVE()				0
#define MTLIB_STATUS(_args, _err)               
#define MTLIB_BLOCK_CNCL_RET(_type, _expr)	return (_expr)
#define MTLIB_BLOCK_CNCL_VAL(_val, _expr)	_val = _expr
#define MTLIB_BLOCK_NOCNCL_RET(_type, _expr)	return (_expr)

#define MTLIB_BLOCK_CNCL_RET_1(_type, _expr)	return (_expr)
#define MTLIB_BLOCK_CNCL_VAL_1(_val, _expr)	_val = _expr
#define MTLIB_CNCL_TEST()

#define MTLIB_VAL(_args, _v)	(_v)
#define MTLIB_RETURN(args)
#define MTLIB_INSERT(args)

#define LOCKFILE(x)		0
#define ISLOCKFILE(x)		1
#define UNLOCKFILE(x,v)	
#define LOCKOPEN		0
#define ISLOCKOPEN		1
#define UNLOCKOPEN(v)
#define LOCKDIR			0
#define UNLOCKDIR(v)
#define LOCKMISC		0
#define UNLOCKMISC(v)
#define LOCKLOCALE		0
#define ISLOCKLOCALE		1
#define UNLOCKLOCALE(v)
#define	LOCKDECLINIT(l,i)
#define	LOCKDECL(l)
#define	LOCKINIT(l,i)

#endif /* _LIBC_ABI || _LIBC_NOMP */

extern int __us_rsthread_stdio;
extern int __us_rsthread_misc;
extern int __us_rsthread_malloc;
extern int __us_rsthread_pmq;

#endif	/* LANGUAGE_ASSEMBLY */

#endif
