/*
 * Pthread-based implementation of thread interface 
 */

#include <pthread.h>
#include "sthr.h"

int sthr_create(sthr_t* thr, const sthr_attr_t* p, sthr_func_t func, void* arg)
{ return pthread_create(&thr->s_pthr, 0, (void*(*)(void*))func, arg); }

int sthr_join(const sthr_t* thr)
{ return pthread_join(thr->s_pthr, 0); }

void sthr_exit(void)
{ pthread_exit(0); }

void sthr_self(sthr_t* t)
{ t->s_pthr = pthread_self(); }

/*
 * Mutexes
 */
int sthr_mutex_init(sthr_mutex_t* mtx)
{ return pthread_mutex_init(&mtx->m_pmutex,0); }

int sthr_mutex_destroy(sthr_mutex_t* mtx)
{ return pthread_mutex_destroy(&mtx->m_pmutex); }

int sthr_mutex_lock(sthr_mutex_t* mtx)
{ return pthread_mutex_lock(&mtx->m_pmutex); }

int sthr_mutex_unlock(sthr_mutex_t* mtx)
{ return pthread_mutex_unlock(&mtx->m_pmutex); }

/*
 * Conditions
 */
int sthr_cond_init(sthr_cond_t* c) 
{ return pthread_cond_init(&c->c_pcond, 0); }

int sthr_cond_destroy(sthr_cond_t* c)
{ return pthread_cond_destroy(&c->c_pcond); }

int sthr_cond_wait(sthr_cond_t* c, sthr_mutex_t* mtx) 
{ return pthread_cond_wait(&c->c_pcond, &mtx->m_pmutex); }

int sthr_cond_signal(sthr_cond_t* c) 
{ return pthread_cond_signal(&c->c_pcond); }

int sthr_cond_broadcast(sthr_cond_t* c) 
{ return pthread_cond_broadcast(&c->c_pcond); }


/* 
 * Cancellation
 */
int sthr_cancel(const sthr_t* thr)
{ return pthread_cancel(thr->s_pthr); }

