/*
** Purpose: Test
*/

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <setjmp.h>
#include <unistd.h>

#include <Tst.h>


TST_PROTO(suspGetSP);

Tst_t	TstTable[] = {

	{ "susp_1", 0,
		"Test get stack pointer of suspended thread",
		"Non-standard interfaces required for Java" },

	{ "suspGetSP", suspGetSP,
		"Get stack pointers of pool of spinning threads",
		"Create a pool of victims which spin,\n"
		"suspend the victims and then join them,\n"
		"cancel all victims and then wait for test threads\n" },

	{ 0 }
};


/* ------------------------------------------------------------------ */

pthread_cond_t	waitCV = PTHREAD_COND_INITIALIZER;
pthread_mutex_t	waitMtx = PTHREAD_MUTEX_INITIALIZER;

void	*spinThread(void *);

void *
spinThread(void *arg)
{
	int	i;
	int	type;

	ChkInt( pthread_setcanceltype, (PTHREAD_CANCEL_ASYNCHRONOUS, &type),
		==, 0 );

	TstTrace("Started spinning sp %#x\n", &type);

	for (i = 0; ; i++) ;

	/* NOTREACHED */
	return (0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(suspGetSP)
{
	int		p;
	pthread_t	*victims;
	void		*ret;
	char		**stacks;
	pthread_attr_t	pta;
	size_t		stk_size = sysconf(_SC_THREAD_STACK_MIN);
	char		*sp;

	ChkExp( stk_size > 0, ("stack size busted %ld\n", stk_size) );

	ChkPtr( victims = malloc, (TST_TRIAL_THREADS * sizeof(pthread_t)),
		!=, 0 );
	ChkPtr( stacks = malloc, (TST_TRIAL_THREADS * sizeof(void *)), !=, 0 );

	ChkInt( pthread_attr_init, (&pta), ==, 0 );
	ChkInt( pthread_attr_setstacksize, (&pta, stk_size), ==, 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {

		ChkPtr( stacks[p] = malloc, (stk_size), !=, 0 );
		ChkInt( pthread_attr_setstackaddr, (&pta, stacks[p]), ==, 0 );

		ChkInt( pthread_create, (victims+p, &pta, spinThread, 0),
			==, 0 );
	}
	ChkInt( sched_yield, (), ==, 0 );
	TST_NAP(TST_PAUSE);

	for (p = 0; p < TST_TRIAL_THREADS; p++) {

		ChkInt( pthread_suspend_np, (victims[p]), ==, 0 );
		ChkInt( pthread_getstackpointer_np,
			(victims[p], (void **)&sp), ==, 0 );

		TstTrace("stack %#x..%#x sp %#x %#x\n",
			stacks[p], stacks[p] + stk_size,
			sp, stacks[p] + stk_size - sp);
		ChkExp( sp < stacks[p] + stk_size,
			("bad sp %#x [<%#x]\n", sp, stacks[p] + stk_size) );
		ChkExp( sp > stacks[p], ("bad sp %#x [>%#x]\n", sp, stacks[p]));
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_resume_np, (victims[p]), ==, 0 );
		ChkInt( pthread_cancel, (victims[p]), ==, 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join, (victims[p], &ret), ==, 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("bad join %#x %#x\n", ret, 0) );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		free(stacks[p]);
	}
	free(stacks);
	free(victims);

	TST_RETURN(0);
}
