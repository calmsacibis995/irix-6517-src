/* 
 * A "simple thread" interface; can be implemented via sprocs or via 
 * pthreads. We're trying to stay in the common denominator of the two
 * libraries, so this is just the bread and butter
 */

#ifndef STHR_H_
#define STHR_H_

#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/prctl.h>

typedef union sthr {
	pthread_t 	s_pthr;	/* pthread */
	pid_t		s_pid;	/* sproc */
} sthr_t;

typedef struct sthr_attr {
	int foo;	/* XXX */
} sthr_attr_t;
typedef void (*sthr_func_t) (void*);

/* 
 * Since sproc threads don't have return values, thr_join doesn't have 
 * one either.  
 */
int sthr_create(sthr_t* thr, const sthr_attr_t* p, sthr_func_t func, void* arg);
int sthr_join(const sthr_t* thr);
void sthr_exit(void);
void sthr_self(sthr_t* t);
void sthr_yield(void);

/* 
 * Mutexes 
 */
typedef union sthr_mutex {
	sem_t		m_sem;		/* for sproc */
	pthread_mutex_t m_pmutex;
} sthr_mutex_t;

int sthr_mutex_init(sthr_mutex_t*);
int sthr_mutex_lock(sthr_mutex_t*);
int sthr_mutex_unlock(sthr_mutex_t*);
int sthr_mutex_destroy(sthr_mutex_t*);

/*
 * Semaphores
 * 
 * We use sem_t's for both pthreads and sprocs; the man page leads one
 * to believe that this is perfectly kosher 
 */
typedef sem_t sthr_sem_t;
#define sthr_sem_init(sem,val) sem_init(sem, 0, val)
#define sthr_sem_destroy(sem) sem_destroy(sem)
#define sthr_sem_wait(sem) sem_wait(sem)
#define sthr_sem_post(sem) sem_post(sem)

/*
 * Conditions
 */
/* for sproc's, we have to build conditions out of semaphores */
typedef struct sthr_sproc_cond {
	ulong_t		c_nwaiters;
	sthr_sem_t	c_wait;
} sthr_sproc_cond_t;

typedef union sthr_cond {
	sthr_sproc_cond_t	c_scond;
	pthread_cond_t		c_pcond;
} sthr_cond_t;
int sthr_cond_init(sthr_cond_t* c);
int sthr_cond_destroy(sthr_cond_t* c);
int sthr_cond_wait(sthr_cond_t* c, sthr_mutex_t* mtx);
int sthr_cond_signal(sthr_cond_t* c);
int sthr_cond_broadcast(sthr_cond_t* c);

/*
 * The ulocks barrier (3p) implementation is rather unwieldy to use, so we roll
 * our own barriers.
 */
typedef struct sthr_barrier {
	unsigned long b_count;
} sthr_barrier_t;

int sthr_barrier_init(sthr_barrier_t* barrier, ulong_t count);
void sthr_barrier_join(sthr_barrier_t* barrier);
void sthr_barrier_destroy(sthr_barrier_t* barrier);

/*
 * A cancellation interface, to make ending syncronization tests more palatable
 */ 
int sthr_cancel(const sthr_t *thr);


#endif
