/*
** Purpose: Test
*/

#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mutex.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

pthread_mutex_t		waitMtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t		waitCV = PTHREAD_COND_INITIALIZER;

volatile unsigned long	threadsRunning;


void *
victimThread(void *arg)
{
	int	id = (int)(long)arg;

	TstTrace("%d: victim running\n", id);
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	threadsRunning++;
	ChkInt( pthread_cond_wait(&waitCV, &waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );
	TstTrace("%d: victim done.\n", id);
	return (0);
}


int
stressMax(int threadMax, int checkEagain)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	pthread_attr_t	pta;
	size_t		stk_size;

	threadsRunning = 0;

	TstInfo("creating %d threads\n", threadMax);
	ChkPtr( pts = malloc(threadMax * sizeof(pthread_t)), != 0 );
	threadMax--;

	stk_size = sysconf(_SC_THREAD_STACK_MIN);
	ChkInt( pthread_attr_init(&pta), == 0 );
	ChkInt( pthread_attr_setstacksize(&pta, stk_size), == 0 );

	for (p = 0; p < threadMax; p++) {
		ChkInt( pthread_create(pts+p, &pta, victimThread, (void*)p),
			== 0 );
	}

	if (checkEagain) {
		ChkInt( pthread_create(pts+p, &pta, victimThread, 0),
		== EAGAIN );
	}

	ChkInt( pthread_attr_destroy(&pta), == 0 );

	while (threadsRunning != threadMax) { sched_yield(); }
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	ChkInt( pthread_cond_broadcast(&waitCV), == 0 );

	for (p = 0; p < threadMax; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	return (0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(maxThread)
{
	long		threadMax;

	ChkInt( threadMax = sysconf(_SC_THREAD_THREADS_MAX), > 0 );

	stressMax(threadMax, 1);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void
check_limits(struct rlimit *exp_lim, char *msg)
{
	struct rlimit	rlim;

	CHKInt( getrlimit(RLIMIT_PTHREAD, &rlim), == 0 );
	ChkExp( rlim.rlim_cur == exp_lim->rlim_cur
		&& rlim.rlim_max == exp_lim->rlim_max,
		("bad limits (%s) cur %d [%d] max %d [%d]", msg,
		rlim.rlim_cur, exp_lim->rlim_max,
		rlim.rlim_cur, exp_lim->rlim_max) );
}

TST_BEGIN(rlimThread0)
{
	struct rlimit	rlim_base;
	struct rlimit	rlim0;
	long		threadMax;

	TstInfo("Sanity checks\n" );
	CHKInt( getrlimit(RLIMIT_PTHREAD, &rlim_base), == 0 );
	ChkExp( rlim_base.rlim_cur <= rlim_base.rlim_max,
		("bad default limits cur %d max %d",
		rlim_base.rlim_cur, rlim_base.rlim_max) );
	TstInfo("Base values cur %d max %d\n",
			rlim_base.rlim_cur, rlim_base.rlim_max);

	ChkInt( threadMax = sysconf(_SC_THREAD_THREADS_MAX),
		== rlim_base.rlim_cur );

	/* ------------------------------------------------------------------ */
	rlim0 = rlim_base;
	rlim0.rlim_max++;

	TstInfo("Raise hard limit to %d\n", rlim0.rlim_max);

	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == -1 );
	ChkExp( errno == EINVAL,
		("errno wrong (hard limit raised) %d [%d]\n", errno, EINVAL) );
	check_limits(&rlim_base, "hard limit raised");

	/* ------------------------------------------------------------------ */
	rlim0 = rlim_base;
	rlim0.rlim_cur = rlim_base.rlim_max + 1;

	TstInfo("Raise soft limit to %d\n", rlim0.rlim_cur);

	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == -1 );
	ChkExp( errno == EINVAL,
		("errno wrong (soft limit too big) %d [%d]\n", errno, EINVAL) );
	check_limits(&rlim_base, "soft limit too big");

	/* ------------------------------------------------------------------ */
	rlim0.rlim_cur = rlim_base.rlim_max;
	rlim0.rlim_max = rlim0.rlim_cur - 1;

	TstInfo("Set limits to %d %d\n", rlim0.rlim_cur, rlim0.rlim_max);

	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == -1 );
	ChkExp( errno == EINVAL,
		("errno wrong (soft limit > hard) %d [%d]\n",
		 errno, EINVAL) );
	check_limits(&rlim_base, "soft limit > hard");

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(rlimThread1)
{
	struct rlimit	rlim_base;
	struct rlimit	rlim0;

	CHKInt( getrlimit(RLIMIT_PTHREAD, &rlim_base), == 0 );
	rlim0 = rlim_base;

	if (rlim_base.rlim_cur == rlim_base.rlim_max) {
		TstInfo("Limits equal - test terminated\n");
		TST_RETURN(0);
	}
	TstInfo("Base values cur %d max %d\n",
			rlim_base.rlim_cur, rlim_base.rlim_max);

	/* ------------------------------------------------------------------ */
	rlim0.rlim_cur = (rlim0.rlim_max + rlim0.rlim_cur)/2;

	TstInfo("Raise soft limit to %d\n", rlim0.rlim_cur);
	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == 0 );
	check_limits(&rlim0, "raise soft limit");
	stressMax(rlim0.rlim_cur, 1);

	rlim0.rlim_cur++;
	TstInfo("Raise soft limit to %d\n", rlim0.rlim_cur);
	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == 0 );
	check_limits(&rlim0, "raise soft limit");
	stressMax(rlim0.rlim_cur, 1);

	rlim0.rlim_cur++;
	TstInfo("Raise soft limit to %d\n", rlim0.rlim_cur);
	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == 0 );
	check_limits(&rlim0, "raise soft limit");
	stressMax(rlim0.rlim_cur, 1);

	/* ------------------------------------------------------------------ */
	rlim0.rlim_max = (rlim0.rlim_max + rlim0.rlim_cur)/2;

	TstInfo("Lower hard limit to %d\n", rlim0.rlim_max);
	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == 0 );
	check_limits(&rlim0, "lower hard limit");
	stressMax(rlim0.rlim_cur, 1);

	/* ------------------------------------------------------------------ */
	rlim0.rlim_cur = (rlim0.rlim_cur + rlim_base.rlim_cur)/2;

	TstInfo("Lower soft limit to %d\n", rlim0.rlim_cur);
	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == 0 );
	check_limits(&rlim0, "lower soft limit");
	stressMax(rlim0.rlim_cur, 0);

	/* ------------------------------------------------------------------ */
	rlim0.rlim_cur = (rlim_base.rlim_cur)/2;

	TstInfo("Lower soft limit to %d\n", rlim0.rlim_cur);
	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == 0 );
	check_limits(&rlim0, "lower soft limit");
	stressMax(rlim0.rlim_cur, 0);

	/* ------------------------------------------------------------------ */
	rlim0.rlim_max = (rlim0.rlim_cur + rlim_base.rlim_cur)/2;

	TstInfo("Lower hard limit to %d\n", rlim0.rlim_max);
	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == 0 );
	check_limits(&rlim0, "lower hard limit");
	stressMax(rlim0.rlim_cur, 0);

	/* ------------------------------------------------------------------ */
	rlim0.rlim_cur = rlim0.rlim_max;

	TstInfo("Set limits to %d %d\n", rlim0.rlim_cur, rlim0.rlim_max);
	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == 0 );
	check_limits(&rlim0, "set limits");
	stressMax(rlim0.rlim_cur, 0);

	rlim0.rlim_cur = rlim0.rlim_max = rlim0.rlim_max - 1;

	TstInfo("Set limits to %d %d\n", rlim0.rlim_cur, rlim0.rlim_max);
	CHKInt( setrlimit(RLIMIT_PTHREAD, &rlim0), == 0 );
	check_limits(&rlim0, "set limits");
	stressMax(rlim0.rlim_cur, 0);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise pthread creation" )

	TST( maxThread, "Test max",
		"Create threads (which block)"
		"Verify that sysconf() max are created and run" ),

	TST( rlimThread0, "Test rlimit error cases",
		"Verify sanity of current limits"
		"Verify hard limit cannot be raised"
		"Verify soft limit cannot be set above current hard limit"
		"Verify soft limit cannot be set above new hard limit" ),

	TST( rlimThread1, "Test rlimit cases",
		"Assume configured soft limit != hard limit"
		"Create threads (which block)"
		"Verify the soft limit can be raised"
		"Verify the hard limit can be lowered above soft default"
		"Verify the soft limit can be lowered above soft default"
		"Verify the soft limit can be lowered below soft default"
		"Verify the hard limit can be lowered below soft default"
		"Verify the soft limit can be equal to hard limit" ),
TST_FINISH

