#ifndef SEM_HEADER
#define SEM_HEADER

#ident "$Revision: 1.2 $"

#include <sys/types.h>
#ifdef PTHREADS
#include <pthread.h>

typedef pthread_mutex_t mutex;
typedef pthread_cond_t condvar;
typedef pthread_rwlock_t mrlock;
#else
#include <sys/prctl.h>
#include <ulocks.h>

typedef void *mutex;
typedef void *condvar;
typedef void *mrlock;
#endif

int  sem_initialize (void);

int  mutex_initialize (mutex *);
void mutex_destroy (mutex *);
int  mutex_lock (mutex *);
void mutex_unlock (mutex *);

int  condvar_initialize (condvar *);
void condvar_destroy (condvar *);
int  condvar_wait (condvar *, mutex *);
int  condvar_signal (condvar *);

int  mrlock_initialize (mrlock *);
void mrlock_destroy (mrlock *);
int  mrlock_rdlock (mrlock *);
int  mrlock_wrlock (mrlock *);
void mrlock_unlock (mrlock *);

#endif /* SEM_HEADER */
