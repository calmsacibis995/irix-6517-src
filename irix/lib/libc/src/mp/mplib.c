/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* This module implements thread facilities for apps/DSO code which is
 * required to work with either sprocs or pthreads.
 *
 * Scheme
 * #pragma weak pthread calls:
 *	One may use __multi_thread to decide at run time which thread calls
 *	to use; the default should be to use sproc calls.
 *	These pthread calls are purely to permit a DSO to resolve symbols
 *	normally provided by libpthread - they will return an error if executed
 *	at run time.
 *	These interfaces are preempted by libpthread if it is loaded.
 *
 * __multi_thread:
 *		- set at load time if pthreads is used
 *		- set at run time when the first sproc is created
 *	Pthreads is assumed to always be threaded and require protection.
 *	Sprocs and pthreads cannot exist in the same process.
 */

#ifdef __STDC__

#pragma weak pthread_create =		_pthread_create
#pragma weak pthread_exit =		_pthread_exit
#pragma weak pthread_testcancel =	_pthread_testcancel 
#pragma weak pthread_self =		_pthread_self
#pragma weak pthread_mutex_init =	_pthread_mutex_init
#pragma weak pthread_mutex_destroy =	_pthread_mutex_destroy
#pragma weak pthread_mutex_lock =	_pthread_mutex_lock
#pragma weak pthread_mutex_trylock =	_pthread_mutex_trylock
#pragma weak pthread_mutex_unlock =	_pthread_mutex_unlock
#pragma weak pthread_cond_init =	_pthread_cond_init
#pragma weak pthread_cond_destroy = 	_pthread_cond_destroy
#pragma weak pthread_cond_wait =	_pthread_cond_wait
#pragma weak pthread_cond_timedwait =	_pthread_cond_timedwait
#pragma weak pthread_cond_signal =	_pthread_cond_signal
#pragma weak pthread_cond_broadcast =	_pthread_cond_broadcast
#pragma weak pthread_attr_init =	_pthread_attr_init
#pragma weak pthread_attr_destroy =	_pthread_attr_destroy
#pragma weak pthread_attr_setstacksize =	_pthread_attr_setstacksize
#pragma weak pthread_attr_setstackaddr =	_pthread_attr_setstackaddr
#pragma weak pthread_attr_setdetachstate =	 _pthread_attr_setdetachstate
#pragma weak pthread_getconcurrency =	_pthread_getconcurrency 
#pragma weak pthread_setconcurrency =	_pthread_setconcurrency 

#endif

#include "synonyms.h"
#include <sys/types.h>
#include <task.h>
#include <unistd.h>
#include <mplib.h>
#include "us.h"


int
_pthread_create(pthread_t *thread, pthread_attr_t *attr, 
		void *(*start)(void *), void *arg)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_CREATE, thread, attr, start, arg ));
	return (ENOSYS);
}

void
_pthread_exit (void *retval)
{
	MTLIB_INSERT(( MTCTL_PTHREAD_EXIT, retval ));

}

void
_pthread_testcancel (void)
{
	MTLIB_INSERT(( MTCTL_PTHREAD_TESTCANCEL ));

}

pthread_t
_pthread_self (void)
{

	MTLIB_RETURN(( MTCTL_PTHREAD_SELF ));
	return (-1);
}

int
_pthread_mutex_init ( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_MUTEX_INIT, mutex, attr ));
	return (ENOSYS);
}

int
_pthread_mutex_destroy (pthread_mutex_t *mutex)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_MUTEX_DESTROY, mutex ));
	return (ENOSYS);
}

int
_pthread_mutex_lock (pthread_mutex_t *mutex)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_MUTEX_LOCK, mutex ));
	return (ENOSYS);
}

int
_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_MUTEX_TRYLOCK, mutex ));
	return (ENOSYS);
}

int
_pthread_mutex_unlock (pthread_mutex_t *mutex)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_MUTEX_UNLOCK, mutex ));
	return (ENOSYS);
}

int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_COND_INIT, cond, attr ));
	return (ENOSYS);
}

int
_pthread_cond_destroy (pthread_cond_t *cond)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_COND_DESTROY, cond ));
	return (ENOSYS);
}

int
_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_COND_WAIT, cond, mutex ));
	return (ENOSYS);
}

int
_pthread_cond_timedwait (pthread_cond_t *cond,
          pthread_mutex_t *mutex, const struct timespec *abstime)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_COND_TIMEDWAIT, cond, mutex, abstime ));
	return (ENOSYS);
}

int
_pthread_cond_signal(pthread_cond_t *cond)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_COND_SIGNAL, cond ));
	return (ENOSYS);
}

int
_pthread_cond_broadcast (pthread_cond_t *cond)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_COND_BROADCAST, cond ));
	return (ENOSYS);
}

int
_pthread_attr_init(pthread_attr_t *attr)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_ATTR_INIT, attr ));
	return (ENOSYS);
}

int
_pthread_attr_destroy (pthread_attr_t *attr)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_ATTR_DESTROY, attr ));
	return (ENOSYS);
}

int
_pthread_attr_setstacksize (pthread_attr_t *attr, size_t size) 
{
	MTLIB_RETURN(( MTCTL_PTHREAD_ATTR_SETSTACKSIZE, attr, size ));
	return (ENOSYS);
}

int
_pthread_attr_setstackaddr (pthread_attr_t *attr, void *addr)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_ATTR_SETSTACKADDR, attr, addr ));
	return (ENOSYS);
}

int
_pthread_attr_setdetachstate (pthread_attr_t *attr, int detach)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_ATTR_SETDETACHSTATE, attr, detach ));
	return (ENOSYS);
}

int
_pthread_getconcurrency (void)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_GETCONCURRENCY ));
	return (ENOSYS);
}

int
_pthread_setconcurrency (int level)
{
	MTLIB_RETURN(( MTCTL_PTHREAD_SETCONCURRENCY, level ));
	return (ENOSYS);
}

int
_mplib_get_thread_type(void)
{
	return (__multi_thread);
}

unsigned
_mplib_get_thread_id(void)
{

	extern pthread_t pthread_self(void);

	return ((__multi_thread == MT_PTHREAD)
		? pthread_self() : (unsigned)get_pid());
}

