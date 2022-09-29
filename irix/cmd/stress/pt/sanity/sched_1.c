/*
** Purpose: Test
*/

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

pthread_mutex_t	waitMtx = PTHREAD_MUTEX_INITIALIZER;

int	highestPri;

void	 *mtx_wait_wakeup(void *);
void	 mtx_threads(int, int);

void *
mtx_wait_wakeup(void *arg)
{
	struct	sched_param	spol;
	int			pol;

	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );

	ChkInt( pthread_getschedparam(pthread_self(), &pol, &spol), == 0 );
	ChkExp( spol.sched_priority == highestPri,
		("Priority wrong %d [%d]\n", spol.sched_priority, highestPri) );

	TstTrace("Woke up %d\n", spol.sched_priority);

	highestPri--;
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );
	pthread_exit(0);
	/* NOTREACHED */
}


void
mtx_threads(int increase, int threads)
{
	struct sched_param	spol;
	int		pol;
	int		max_pri;
	int		lowestPri;
	int		adjust = increase ? 1 : -1;

	int		p;
	pthread_t	*pts;
	void		*ret;
	pthread_attr_t	pta;

	ChkExp( threads <= 32, ("Max threads is 32 (priorities)\n") );
	ChkPtr( pts = malloc(threads * sizeof(pthread_t)), != 0 );

	ChkInt( pthread_getschedparam(pthread_self(), &pol, &spol), == 0 );

	spol.sched_priority = increase ? sched_get_priority_min(pol)
				       : sched_get_priority_max(pol);
	if (increase) {
		spol.sched_priority = sched_get_priority_min(pol);
		lowestPri = spol.sched_priority - 1;
		highestPri = lowestPri + threads;
	} else {
		spol.sched_priority = sched_get_priority_max(pol);
		highestPri = spol.sched_priority;
		lowestPri = highestPri - threads;
	}

	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );

	ChkInt( pthread_attr_init(&pta), == 0 );
	ChkInt( pthread_attr_setinheritsched(&pta, PTHREAD_EXPLICIT_SCHED),
		== 0 );

	for (p = 0; p < threads; p++) {
		ChkInt( pthread_attr_setschedparam(&pta, &spol), == 0 );
		ChkInt( pthread_create(pts+p, &pta, mtx_wait_wakeup, 0),
			== 0 );
		spol.sched_priority += adjust;
	}

	TST_NAP(TST_PAUSE);

	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	for (p = 0; p < threads; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	ChkExp( highestPri == lowestPri,
		("Mismatched counter %d [%d]\n", highestPri, lowestPri) );

	free(pts);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(schedMutexUp)
{
	mtx_threads(TRUE, TST_TRIAL_THREADS);
	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(schedMutexDown)
{
	mtx_threads(FALSE, TST_TRIAL_THREADS);
	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(schedCvDown)
{
	TstTrace("Real soon now...\n");
	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Priority based wakeup" )

	TST( schedMutexUp, "Mutex wait increasing/ wakeup",
		"Create threads of increasing priority and have them wait"
		"on a locked mutex"
		"Unlock the mutex and check the wakeup order" ),

	TST( schedMutexDown, "Mutex wait decreasing/ wakeup",
		"Create threads of decreasing priority and have them wait"
		"on a locked mutex"
		"Unlock the mutex and check the wakeup order" ),

	/*
	TST( schedCvDown, "Condition wait decreasing/ wakeup",
		"Create threads of decreasing priority and have them wait"
		"on a condition variable"
		"Signal the condition and check the wakeup order" ),
	 */

TST_FINISH
