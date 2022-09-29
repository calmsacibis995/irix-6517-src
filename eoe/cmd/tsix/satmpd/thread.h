#ifndef THREAD_HEADER
#define THREAD_HEADER

#ident "$Revision: 1.2 $"

#ifdef PTHREADS
#include <pthread.h>

typedef void *thread_rtn;
typedef pthread_t thread_id;

#define thread_return(x)	return((void *) (x))
#else
#include <sys/prctl.h>

typedef void thread_rtn;
typedef pid_t thread_id;

#define thread_return(x)	return
#endif

typedef thread_rtn (*thread_proc) (void *);

int thread_create (thread_proc, thread_id *, void *);
int thread_sigprocmask (int, sigset_t *, sigset_t *);
int thread_kill (thread_id, int);

#endif /* THREAD_HEADER */
