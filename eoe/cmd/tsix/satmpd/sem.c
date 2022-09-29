#ident "$Revision: 1.2 $"

#include <stdlib.h>
#include "sem.h"

#ifndef PTHREADS
static usptr_t *uhandle;
#endif

#ifndef PTHREADS
static void
sem_destroy (void)
{
	usdetach (uhandle);
}
#endif

int
sem_initialize (void)
{
#ifdef PTHREADS
	return (0);
#else
	if (usconfig (CONF_ARENATYPE, US_SHAREDONLY) == -1 ||
	    (uhandle = usinit (tmpnam ((char *) NULL))) == NULL)
		return (-1);
	if (atexit (sem_destroy) == 0)
		return (0);
	sem_destroy ();
	return (-1);
#endif
}

int
mutex_initialize (mutex *mt)
{
	if (mt == (mutex *) NULL)
		return (-1);
#ifdef PTHREADS
	return (pthread_mutex_init (mt, (pthread_mutexattr_t *) NULL));
#else
	*mt = usnewsema (uhandle, 1);
	return (*mt == NULL ? -1 : 0);
#endif
}

void
mutex_destroy (mutex *mt)
{
	if (mt == (mutex *) NULL)
		return;
#ifdef PTHREADS
	(void) pthread_mutex_destroy (mt);
#else
	usfreesema (*mt, uhandle);
#endif
}

int
mutex_lock (mutex *mt)
{
#ifdef PTHREADS
	return (pthread_mutex_lock (mt));
#else
	return (uspsema (*mt));
#endif
}

void
mutex_unlock (mutex *mt)
{
#ifdef PTHREADS
	(void) pthread_mutex_unlock (mt);
#else
	(void) usvsema (*mt);
#endif
}

int
condvar_initialize (condvar *cv)
{
	if (cv == (condvar *) NULL)
		return (-1);
#ifdef PTHREADS
	return (pthread_cond_init (cv, (pthread_condattr_t *) NULL));
#else
	*cv = usnewsema (uhandle, 0);
	return (*cv == NULL ? -1 : 0);
#endif
}

void
condvar_destroy (condvar *cv)
{
	if (cv == (condvar *) NULL)
		return;
#ifdef PTHREADS
	(void) pthread_cond_destroy (cv);
#else
	usfreesema (*cv, uhandle);
#endif
}

int
condvar_wait (condvar *cv, mutex *mt)
{
#ifdef PTHREADS
	return (pthread_cond_wait (cv, mt));
#else
	(void) usvsema (*mt);
	(void) uspsema (*cv);
	return (uspsema (*mt));
#endif
}

int
condvar_signal (condvar *cv)
{
#ifdef PTHREADS
	return (pthread_cond_signal (cv));
#else
	return (usvsema (*cv));
#endif
}

int
mrlock_initialize (mrlock *mr)
{
	if (mr == (mrlock *) NULL)
		return (-1);
#ifdef PTHREADS
	return (pthread_rwlock_init (mr, (pthread_rwlockattr_t *) NULL));
#else
	return (mutex_initialize (mr));
#endif
}

void
mrlock_destroy (mrlock *mr)
{
#ifdef PTHREADS
	(void) pthread_rwlock_destroy (mr);
#else
	mutex_destroy (mr);
#endif
}

int
mrlock_rdlock (mrlock *mr)
{
#ifdef PTHREADS
	return (pthread_rwlock_rdlock (mr));
#else
	return (mutex_lock (mr));
#endif
}

int
mrlock_wrlock (mrlock *mr)
{
#ifdef PTHREADS
	return (pthread_rwlock_wrlock (mr));
#else
	return (mutex_lock (mr));
#endif
}

void
mrlock_unlock (mrlock *mr)
{
#ifdef PTHREADS
	(void) pthread_rwlock_unlock (mr);
#else
	mutex_unlock (mr);
#endif
}
