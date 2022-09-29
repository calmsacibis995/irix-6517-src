/*
** Purpose: Test
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <Tst.h>

/* ------------------------------------------------------------------ */

/* Nasty max proc problem:
 *	we can quickly exhaust the per-user process limit
 *	so we restrict the number of outstanding forks
 *	and guess at an overhead per pthread proc
 */
int	nProcs;
int	nThreads;
#define TST_TRIAL_PROCS	5
#undef TST_TRIAL_THREADS
#undef TST_TRIAL_PROCS
#define TST_TRIAL_THREADS	nThreads
#define TST_TRIAL_PROCS		nProcs

typedef void *(*ptrToFunc)(void *);

void *
drone(void *arg)
{
	TstTrace("Thread running pid %d\n", getpid());
	return (0);
}

void *
make_threads(void *arg)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	ptrToFunc	child_func = ((ptrToFunc *)arg)[0];
	ptrToFunc	*child_arg = ((ptrToFunc *)arg) + 1;

	TstTrace("Make threads pid %d\n", getpid());

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, child_func, child_arg), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	return (0);
}

void *
make_procs(void *arg)
{
	int		p;
	pid_t		*pids;
	int		status;
	ptrToFunc	child_func = ((ptrToFunc *)arg)[0];
	ptrToFunc	*child_arg = ((ptrToFunc *)arg) + 1;

	TstTrace("Make procs pid %d\n", getpid());

	ChkPtr( pids = malloc(TST_TRIAL_PROCS * sizeof(pid_t)), != 0 );

	for (p = 0; p < TST_TRIAL_PROCS; p++) {

		while ((pids[p] = fork()) == -1) {
			ChkExp( errno == EAGAIN, ("fork failed %d\n", errno) );
			sched_yield();
		}

		if (!pids[p]) {

			/* Child
			 */
			TstTrace("Fork running %d\n", getpid());
			ChkPtr( child_func(child_arg), == 0 );
			exit(5);
		}
	}

	for (p = 0; p < TST_TRIAL_PROCS; p++) {

		ChkInt( waitpid(pids[p], &status, 0), == pids[p] );
		ChkExp( WIFEXITED(status) && WEXITSTATUS(status) == 5,
			("bad wait status pid %d %#x [5]\n", pids[p], status) );
	}

	free(pids);

	return (0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(forks1)
{
	void	*(*jobs[2])(void *);

	jobs[0] = make_threads;
	jobs[1] = drone;
	nProcs = 20;
	nThreads = 20;

	ChkPtr( make_procs(jobs), == 0 );

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(forks2)
{
	void	*(*jobs[3])(void *);

	jobs[0] = make_procs;
	jobs[1] = make_threads;
	jobs[2] = drone;
	nProcs = 5;
	nThreads = 10;

	ChkPtr( make_threads(jobs), == 0 );

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(forks3)
{
	void	*(*jobs[4])(void *);

	jobs[0] = make_threads;
	jobs[1] = make_procs;
	jobs[2] = make_threads;
	jobs[3] = drone;
	nProcs = 5;
	nThreads = 5;

	ChkPtr( make_procs(jobs), == 0 );

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Test fork() semantics" )

	TST( forks1, "Threaded children",
		"Create multiple procs"
		"each proc creates threads"
		"wait for procs to exit." ),

	TST( forks2, "Threaded parent, threaded children",
		"Create multiple threads,"
		"each thread creates multiple procs"
		"each proc creates threads,"
		"each parent waits for its children to exit." ),

	TST( forks3, "Threaded children, threaded grandchildren",
		"Create multiple procs"
		"each proc creates threads"
		"each thread creates multiple sub-procs"
		"each sub-proc creates threads"
		"each parent waits for its children to exit." ),

TST_FINISH
