/*
 * Sproc-based simple thread interface.
 */

#include <semaphore.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ulocks.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <mutex.h>
#include <task.h>

#include "sthr.h"

/*
 * This routine gets called at initialization time.
 */
void _sproc_init(void)
{
	int maxthrs = 1000;
	char* env = getenv("THR_MAX_THRS");
	
	if (env) 
		maxthrs = atoi(env);
	if (usconfig(CONF_INITUSERS, maxthrs) == -1) {
		perror("usconfig");
		abort();
	}
}

int sthr_create(sthr_t* thr, const sthr_attr_t* p, sthr_func_t func, void* arg)
{
	thr->s_pid = sproc(func, PR_SALL, arg);
	if (thr->s_pid < 0) {
		return -1;
	}
	return 0;
}

int sthr_join(const sthr_t* thr)
{
	int wstat = 0;

	do {
		if (waitpid(thr->s_pid, &wstat, 0) < 0 && errno != EINTR) {
			return -1;
		}
	} while (!(WIFEXITED(wstat) || WIFSIGNALED(wstat)));

	return 0;
}

void sthr_exit(void)
{
	exit(0);
}

void sthr_self(sthr_t* t)
{
	t->s_pid = getpid();
}

/*
 * Mutexes are implemented as binary semaphores 
 */
int sthr_mutex_init (sthr_mutex_t* m)
{ return sem_init(&m->m_sem, 0, 1); }

int sthr_mutex_destroy (sthr_mutex_t* m)
{ return sem_destroy(&m->m_sem); }

int sthr_mutex_lock (sthr_mutex_t* m)
{ return sem_wait(&m->m_sem); }

int sthr_mutex_unlock (sthr_mutex_t* m)
{ return sem_post(&m->m_sem); }

/*
 * Conditions have to be hand-rolled for sproc's.
 * 
 * Waiters increment nwaiters before dropping the lock and sem_waiting.
 * Signallers check nwaiters; if it is greater than zero, it sem_posts.
 * 
 * This is safe because the signaller should have updated the predicate under
 * the same lock. However, multiple signallers can see the same waiter, and all
 * post the semaphore. This is OK, because the waiters should always retry their
 * predicate before proceeding.
 */

int sthr_cond_init(sthr_cond_t* sc)
{
	sthr_sproc_cond_t* c = &sc->c_scond;

	c->c_nwaiters = 0;
	return sthr_sem_init(&c->c_wait, 0);
}

int sthr_cond_destroy(sthr_cond_t* sc)
{
	sthr_sproc_cond_t* c = &sc->c_scond;
	return sthr_sem_destroy(&c->c_wait);
}

int sthr_cond_wait(sthr_cond_t* sc, sthr_mutex_t* mtx)
{
	sthr_sproc_cond_t* c = &sc->c_scond;

	/* 
	 * indicate that we are about to wait. Since we only adjust 
	 * c_nwaiters while we have the mutex, we are fine 
	 */
	(void) test_then_add(&c->c_nwaiters, 1UL);
	sthr_mutex_unlock(mtx);
	/* 
	 * the "race" with sthr_cond_signal here doesn't matter, because
	 * c_wait (being a semaphore) will remember the signal, and wake us
	 * up here 
	 */
	sthr_sem_wait(&c->c_wait);
	sthr_mutex_lock(mtx);
	(void) test_then_add(&c->c_nwaiters, -1UL);

	return 0;
}

int sthr_cond_signal(sthr_cond_t* sc)
{
	sthr_sproc_cond_t* c = &sc->c_scond;

	if (test_then_add(&c->c_nwaiters, 0))
		sthr_sem_post(&c->c_wait);

	return 0;
}

int sthr_cond_broadcast(sthr_cond_t* sc)
{
	sthr_sproc_cond_t* c = &sc->c_scond;
	register long nwaiters_cpy = test_then_add(&c->c_nwaiters, 0);

	while (nwaiters_cpy--) {
		sthr_sem_post(&c->c_wait);
	}
	return 0;
}

/*
 * A cancellation-- just kill the child
 */
int sthr_cancel(const sthr_t* sthr)
{ 
	return kill(sthr->s_pid, SIGKILL); 
}

/*
 * preempt some symbols in libpthread that the harness expects to find 
 */
pthread_t pthread_self()
{ 
#ifdef sgi
	return (pthread_t)get_pid(); 
#else
	return (pthread_t)getpid(); 
#endif
}

int pthread_setconcurrency(int level)
{ 
	return 0; 
}


