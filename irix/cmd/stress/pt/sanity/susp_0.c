/*
** Purpose: Test
*/

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <Tst.h>


TST_PROTO(suspSpin);
TST_PROTO(suspWait);
TST_PROTO(suspBlock);
TST_PROTO(suspCancel);

Tst_t	TstTable[] = {

	{ "susp_0", 0,
		"Test suspend and resume interfaces",
		"Non-standard interfaces required for Java" },

	{ "suspSpin", suspSpin,
		"Suspend and resume a pool of spinning threads",
		"Create a pool of victims which spin,\n"
		"create a pool of test threads who,\n"
		"suspend the victims and then join them,\n"
		"resume and then cancel all victims,\n"
		"then wait for test threads\n" },

	{ "suspWait", suspWait,
		"Suspend and resume a pool of waiting threads",
		"Create a pool of victims which wait on a condition,\n"
		"create a pool of test threads who\n"
		"suspend the victims and then join them,\n"
		"resume and cancel all victims,\n"
		"then wait for test threads\n" },

	{ "suspBlock", suspBlock,
		"Suspend and resume a pool of blocked threads",
		"Create a pool of victims which block in a syscall,\n"
		"create a pool of test threads who,\n"
		"suspend the victims and then join them,\n"
		"resume and cancel all victims,\n"
		"then wait for test threads\n" },

	{ "suspCancel", suspCancel,
		"Cancel a thread as it waits for a suspend",
		"Create a pool of threads which wait on a condition,\n"
		"create a second pool who suspend the first,\n"
		"cancel the second pool and join them,\n"
		"broadcast the condition and join the second pool\n" },

	{ 0 }
};


/* ------------------------------------------------------------------ */

pthread_cond_t	waitCV = PTHREAD_COND_INITIALIZER;
pthread_mutex_t	waitMtx = PTHREAD_MUTEX_INITIALIZER;


void	*spinThread(void *);
void	*waitThread(void *);
void	cleanupWait(void *);
void	*blockThread(void *);
void	*suspAgent(void *);
void	suspVictims(void *(*)(void *));

void *
spinThread(void *arg)
{
	int	i;
	int	type;

	TstTrace("Started spinning\n");

	ChkInt( pthread_setcanceltype, (PTHREAD_CANCEL_ASYNCHRONOUS, &type),
		==, 0 );

	for (i = 0; ; i++) ;

	/* NOTREACHED */
}

void
cleanupWait(void *arg)
{
	ChkInt( pthread_mutex_unlock, (&waitMtx), ==, 0 );
}

void *
waitThread(void *arg)
{
	TstTrace("Started waiting\n");

	ChkInt( pthread_mutex_lock, (&waitMtx), ==, 0 );
	pthread_cleanup_push(cleanupWait, 0);

	ChkInt( pthread_cond_wait, (&waitCV, &waitMtx), ==, 0 );

	ChkInt( pthread_cond_wait, (&waitCV, &waitMtx), ==, 0 );

	pthread_cleanup_pop(1);

	return (0);
}

void *
blockThread(void *arg)
{
	char	buf[1];
	int	res;

	TstTrace("Started blocking\n");

	res = read(0, &buf, 1);

	ChkExp( 0, ("read() returned %d err %d\n", res, errno) );

	return (0);
}

void *
suspAgent(void *arg)
{
	int		p;
	pthread_t	pt = *(pthread_t *)arg;
	void		*ret;

	TstTrace("Started suspend\n");

	ChkInt( pthread_suspend_np, (pt), ==, 0 );

	ChkInt( pthread_join, (pt, &ret), ==, 0 );
	ChkExp( ret == PTHREAD_CANCELED,
		("bad join %#x %#x\n", ret, PTHREAD_CANCELED) );
	return (0);
}

void
suspVictims(void *(*func)(void *))
{
	int		p;
	pthread_t	*victims;
	pthread_t	*agents;
	void		*ret;

	ChkPtr( victims= malloc, (TST_TRIAL_THREADS*sizeof(pthread_t)), !=, 0 );
	ChkPtr( agents = malloc, (TST_TRIAL_THREADS*sizeof(pthread_t)), !=, 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create, (victims+p, 0, func, 0), ==, 0 );
	}
	ChkInt( sched_yield, (), ==, 0 );
	TST_NAP(TST_PAUSE);

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create, (agents+p, 0, suspAgent, victims+p),
			==, 0 );
	}
	ChkInt( sched_yield, (), ==, 0 );
	TST_NAP(TST_SNOOZE);

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_resume_np, (victims[p]), ==, 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_cancel, (victims[p]), ==, 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join, (agents[p], &ret), ==, 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(agents);
	free(victims);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(suspSpin)
{
	suspVictims(spinThread);
	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(suspWait)
{
	suspVictims(waitThread);
	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(suspBlock)
{
	suspVictims(blockThread);
	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

pthread_t	parent;

void *
susp_agent(void *arg)
{
	int		p;
	pthread_t	pt = *(pthread_t *)arg;
	void		*ret;

	TstTrace("Started suspend\n");

	ChkInt( pthread_suspend_np, (pt), ==, 0 );
	ChkInt( pthread_join, (parent, 0), ==, 0 );
	ChkExp( 0, ("not cancelled\n") );

	return (0);
}

TST_BEGIN(suspCancel)
{
	int		p;
	pthread_t	*victims;
	pthread_t	*agents;
	void		*ret;
	int		res;

	ChkPtr( victims= malloc, (TST_TRIAL_THREADS*sizeof(pthread_t)), !=, 0 );
	ChkPtr( agents = malloc, (TST_TRIAL_THREADS*sizeof(pthread_t)), !=, 0 );

	parent = pthread_self();

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create, (victims+p, 0, waitThread, 0), ==, 0 );
	}
	ChkInt( sched_yield, (), ==, 0 );
	TST_NAP(TST_PAUSE);

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create, (agents+p, 0, susp_agent, victims+p),
			==, 0 );
	}
	ChkInt( sched_yield, (), ==, 0 );
	TST_NAP(TST_SNOOZE);

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_cancel, (agents[p]), ==, 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join, (agents[p], &ret), ==, 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("bad join %#x %#x\n", ret, PTHREAD_CANCELED) );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		switch (res = pthread_resume_np(victims[p])) {
		case ENOTSTOPPED :
			TstTrace("Fix missed suspend\n");
			ChkInt( pthread_suspend_np, (victims[p]), ==, 0 );
			ChkInt( pthread_resume_np, (victims[p]), ==, 0 );
			break;
		case 0 :
			TstTrace("Suspend done\n");
			break;
		default:
			ChkExp( 0, ("Bad resume %d\n", res) );
		}
	}
	ChkInt( sched_yield, (), ==, 0 );
	TST_NAP(TST_PAUSE);

	ChkInt( pthread_cond_broadcast, (&waitCV), ==, 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join, (victims[p], &ret), ==, 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(agents);
	free(victims);

	TST_RETURN(0);
}
