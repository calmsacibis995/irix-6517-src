/*
** Purpose: Test
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

size_t	stkMin;

/* ------------------------------------------------------------------ */

void *
stk_align(void *arg)
{
	int	local;

	TstTrace("Running on stack %p\n", &local);
	return (0);
}

TST_BEGIN(stkAlign)
{
	int		p;
	pthread_t	*pts;
	char		**stacks;
	pthread_attr_t	pta;
	size_t		stk_size;
	void		*ret;

	void		*stk_align(void *);

	stk_size = stkMin = sysconf(_SC_THREAD_STACK_MIN);
	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	ChkPtr( stacks = malloc(TST_TRIAL_THREADS * sizeof(void *)), != 0 );
	ChkInt( pthread_attr_init(&pta), == 0 );
	ChkInt( pthread_attr_setstacksize(&pta, stk_size), == 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {

		ChkPtr( stacks[p] = malloc(stk_size), != 0 );
		ChkInt( pthread_attr_setstackaddr(&pta, stacks[p] - p),
			== 0 );

		ChkInt( pthread_create(pts+p, &pta, stk_align, 0), == 0 );
	}
	ChkInt( pthread_attr_destroy(&pta), == 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
		free(stacks[p]);
	}

	free(stacks);
	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void *
stk_reuse(void *arg)
{
	int	local;

	TstTrace("Running on stack %p\n", &local);
	TST_NAP(TST_SNOOZE);
	return (0);
}

TST_BEGIN(stkReuse)
{
	int		p;
	pthread_t	*pts;
	char		**stacks;
	pthread_attr_t	pta;
	size_t		stk_size;
	void		*ret;

	void		*stk_reuse(void *);

	stk_size = stkMin = sysconf(_SC_THREAD_STACK_MIN);
	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	ChkPtr( stacks = malloc(TST_TRIAL_THREADS * sizeof(void *)), != 0 );
	ChkInt( pthread_attr_init(&pta), == 0 );
	ChkInt( pthread_attr_setstacksize(&pta, stk_size), == 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {

		ChkPtr( stacks[p] = malloc(stk_size), != 0 );
		memset(stacks[p], 0xaa, stk_size);
		ChkInt( pthread_attr_setstackaddr(&pta, stacks[p]), == 0 );
		ChkInt( pthread_create(pts+p, &pta, stk_reuse, 0), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
		memset(stacks[p], 0xaa, stk_size);
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {

		ChkInt( pthread_attr_setstackaddr(&pta, stacks[p]), == 0 );
		ChkInt( pthread_create(pts+p, &pta, stk_reuse, 0), == 0 );
	}
	ChkInt( pthread_attr_destroy(&pta), == 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
		free(stacks[p]);
	}

	free(stacks);
	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

#define STK_MAX		0x200000
#define STK_INCR	0x4000

void *
stk_run(void *arg)
{
	int	local;

	TstTrace("Running on stack %p\n", &local);
	sched_yield();
	return (0);
}

void *
stk_create(void *arg)
{
	int		p;
	pthread_t	pts[3];
	pthread_attr_t	pta;
	size_t		stk_size = stkMin;
	void		*ret;

	void		*stk_run(void *);

	ChkInt( pthread_attr_init(&pta), == 0 );

	ChkInt( pthread_attr_setstacksize(&pta, stk_size+1), == 0 );
	ChkInt( pthread_create(pts+0, &pta, stk_run, 0), == 0 );

	ChkInt( pthread_join(pts[0], &ret), == 0 );
	ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );

	for (stk_size += STK_INCR; stk_size < STK_MAX; stk_size += STK_INCR) {

		ChkInt( pthread_attr_setstacksize(&pta, stk_size-1), == 0 );
		ChkInt( pthread_create(pts+0, &pta, stk_run, 0), == 0 );

		ChkInt( pthread_attr_setstacksize(&pta, stk_size), == 0 );
		ChkInt( pthread_create(pts+1, &pta, stk_run, 0), == 0 );

		ChkInt( pthread_attr_setstacksize(&pta, stk_size+1), == 0 );
		ChkInt( pthread_create(pts+2, &pta, stk_run, 0), == 0 );

		for (p = 0; p < 3; p++) {
			ChkInt( pthread_join(pts[p], &ret), == 0 );
			ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
		}
TstTrace("Stk sz %#x\n", stk_size);
	}
	ChkInt( pthread_attr_destroy(&pta), == 0 );

	return (0);
}

TST_BEGIN(stkSize)
{
	int		p;
	int		threads = TST_TRIAL_THREADS / 3;
	pthread_t	*pts;
	pthread_attr_t	pta;
	void		*ret;

	void		*stk_create(void *);

	stkMin = sysconf(_SC_THREAD_STACK_MIN);
	ChkPtr( pts = malloc(threads * sizeof(pthread_t)), != 0 );
	ChkInt( pthread_attr_init(&pta), == 0 );

	ChkInt( pthread_attr_setstacksize(&pta, stkMin), == 0 );
	for (p = 0; p < threads; p++) {
		ChkInt( pthread_create(pts+p, &pta, stk_create, 0), == 0 );
	}
	ChkInt( pthread_attr_destroy(&pta), == 0 );

	for (p = 0; p < threads; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Test stack attributes" )

	TST( stkAlign, "Test various alignments for pthread stacks.",
		"Create byte aligned stacks and start pthreads with them." ),

	TST( stkReuse, "Test reuse of stacks.",
		"Create stacks for a pool of pthreads which exit,"
		"trash the stacks and then reuse them." ),

	TST( stkSize, "Test various sized stacks.",
		"Create pthreads with different sized stacks." ),

TST_FINISH
