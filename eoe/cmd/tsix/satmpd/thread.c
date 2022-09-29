#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <signal.h>
#include "thread.h"

int
thread_create (thread_proc tp, thread_id *ti, void *arg)
{
#ifdef PTHREADS
	pthread_attr_t attr;
	int r;
#endif
	if (tp == NULL || ti == NULL)
		return (-1);
#ifdef PTHREADS
	if (pthread_attr_init (&attr) != 0 ||
	    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED) != 0)
		return (-1);
	r = pthread_create (ti, &attr, tp, arg);
	return (pthread_attr_destroy (&attr) == 0 ? r : -1);
#else
	*ti = sproc (tp, PR_SALL, arg);
	return (*ti == -1 ? -1 : 0);
#endif
}

int
thread_sigprocmask (int op, sigset_t *nset, sigset_t *oset)
{
#ifdef PTHREADS
	return (pthread_sigmask (op, nset, oset));
#else
	return (sigprocmask (op, nset, oset));
#endif
}

int
thread_kill (thread_id tid, int sig)
{
#ifdef PTHREADS
	return (pthread_kill (tid, sig));
#else
	return (kill (tid, sig));
#endif
}
