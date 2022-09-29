/*
** Purpose: Test
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

void *
swaiter(void *arg)
{
	sigset_t	set;
	int		sig;

	sigfillset(&set);
	pthread_sigmask(SIG_BLOCK, &set, 0);

	TstTrace("sigwait()ing...\n");

	ChkInt( sigwait(&set, &sig), == 0 );

	ChkExp( 0, ("sigwait returned sig %d\n", sig) );

	return (0);
}


TST_BEGIN(exit_sigwait)
{
	int		p;
	pthread_t	*pts;
	void		*ret;

	void	*swaiter(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, swaiter, 0), == 0 );
	}

	TST_NAP(TST_DOZE);

	exit(0);

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise sigwait()" )

	TST( exit_sigwait, "Effect of exit() on sigwait",
		"Create pool of sigwait()ers,"
		"then exit() the main thread,"
		"none of the waiters should return" ),

TST_FINISH
