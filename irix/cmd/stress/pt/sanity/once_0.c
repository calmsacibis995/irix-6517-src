/*
** Purpose: Test
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <Tst.h>

/* ------------------------------------------------------------------ */

int		onceCount;

/* ------------------------------------------------------------------ */
pthread_once_t	onceFast = PTHREAD_ONCE_INIT;


void *
onceFastFunc(void *arg)
{
	int	i;
	void	onceFastInit(void);

	TstTrace("Started\n");

	for (i = 0; i < 10; i++) {
		ChkInt( pthread_once(&onceFast, onceFastInit), == 0 );
	}

	pthread_exit(0);
	/* NOTREACHED */
}


void
onceFastInit(void)
{
	TstTrace("init once running\n");
	TST_SPIN(TST_PAUSE);
	onceCount++;
}


TST_BEGIN(basicSemantics)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void		*onceFastFunc(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	onceCount = 0;
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, onceFastFunc, 0), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("join %#x %#x\n", ret, 0) );
	}

	ChkExp( onceCount == 1, ("bad once count %d\n", onceCount) );

	free(pts);

	TST_RETURN(0);
}


/* ------------------------------------------------------------------ */
pthread_once_t	onceCancel = PTHREAD_ONCE_INIT;


void *
onceCancelFunc(void *arg)
{
	int	i;
	void	onceCancelInit(void);

	TstTrace("Started\n");

	for (i = 0; i < 10; i++) {
		ChkInt( pthread_once(&onceCancel, onceCancelInit), == 0 );
	}

	pthread_exit(0);
	/* NOTREACHED */
}


void
onceCancelInit(void)
{
	static int	first_time = 1;

	TstTrace("init once running\n");
	TST_SPIN(TST_PAUSE);
	if (first_time) {
		first_time = 0;
		ChkInt( pthread_cancel(pthread_self()), == 0 );
		pthread_testcancel();
		ChkExp( 0, ("cancel failed\n") );
	}
	onceCount++;
}


TST_BEGIN(cancelRestart)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void		*onceCancelFunc(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	onceCount = 0;
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, onceCancelFunc, 0),
		== 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0 || ret == PTHREAD_CANCELED,
			("join %#x %#x\n", ret, 0) );
	}

	ChkExp( onceCount == 1, ("bad once count %d\n", onceCount) );

	free(pts);

	TST_RETURN(0);
}


/* ------------------------------------------------------------------ */
pthread_once_t	onceCancelMany = PTHREAD_ONCE_INIT;
pthread_t	first_cancelled;


void *
onceCancelManyFunc(void *arg)
{
	int	i;
	void	onceCancelManyInit(void);

	TstTrace("Started\n");

	for (i = 0; i < 10; i++) {
		ChkInt( pthread_once(&onceCancelMany, onceCancelManyInit),
			== 0 );
		if ((long)arg % 2) {
			pthread_testcancel();
		}
	}

	pthread_exit(0);
	/* NOTREACHED */
}


void
onceCancelManyInit(void)
{
	static int	first_time = 1;

	TstTrace("init once running\n");
	TST_SPIN(TST_PAUSE);
	if (first_time) {
		first_time = 0;
		first_cancelled = pthread_self();
		pthread_testcancel();
		ChkExp( 0, ("cancel failed\n") );
	}
	onceCount++;
}


TST_BEGIN(cancelSafe)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void		*onceCancelFunc(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	onceCount = 0;
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, onceCancelManyFunc, (void *)p), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_cancel(pts[p]), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		if (p % 2) {
			ChkExp( ret == PTHREAD_CANCELED,
				("should be cancelled -- join %#x\n", ret) );
		} else {
			ChkExp((ret == PTHREAD_CANCELED
				&& pts[p] == first_cancelled)
			       || ret == 0,
			       ("join %#x %#x\n", ret, 0) );
		}
	}

	ChkExp( onceCount == 1, ("bad once count %d\n", onceCount) );

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Dynamic package initialisation" )

	TST( basicSemantics, "Basic functionality",
		"Create threads which call init function"
		"verify the init code is only executed once"
		"and that no one runs until the code is completed" ),

	TST( cancelRestart, "Restart after cancel",
		"Create threads which call init function"
		"first thread into init code cancels itself"
		"verify the init code is only executed once"
		"and that no one runs until the code is completed" ),

	TST( cancelSafe, "pthread_once() is not a cancellation point",
		"Create threads which call init function"
		"Cancel all threads"
		"First thread into init code calls pthread_testcancel"
		"Half the threads check for cancellation"
		"Verify the init code is only executed once"
		"and that no one runs until the code is completed" ),

TST_FINISH
