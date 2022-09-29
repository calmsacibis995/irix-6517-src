/*
** Purpose: Test
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <mutex.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

pthread_mutex_t		waitMtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t		waitCV = PTHREAD_COND_INITIALIZER;

volatile unsigned long	threadsRunning;
volatile unsigned long	handlerSync;

int		expSigA;
int		expSigB;

void
checkSigSet(char *name, sigset_t *mask, sigset_t *exp_mask)
{
	int		actual;
	int		expected;
	int		s;

	for (s = 1; s < NSIG; s++) {
		actual = sigismember(mask, s);
		expected = sigismember(exp_mask, s);
		ChkExp(	actual == expected,
			("%s mask wrong for sig %d is %d expected %d\n",
				name, s, actual, expected) );
	}
}

void *
victimThread(void *arg)
{
	while (threadsRunning) {
		sched_yield();
	}

	return (0);
}

/* ------------------------------------------------------------------ */

struct sigaction	exp_disp;

void
hdl_dummy(int sig, siginfo_t *info, void *arg)
{
}

void *
dispChkThread(void *arg)
{
	struct sigaction	act;

	/* Loop around verifying the system signal disposition
	 * against a global expected template.
	 */
	do {
		TstTrace("dispChk running\n");
		ChkInt( sigaction(expSigA, 0, &act), == 0 );
		ChkExp( act.sa_sigaction == exp_disp.sa_sigaction,
			("Actions differ 0x%x [0x%x]\n",
				act.sa_sigaction, exp_disp.sa_sigaction) );

		if (exp_disp.sa_sigaction != SIG_IGN
		    && exp_disp.sa_sigaction != SIG_DFL) {

			checkSigSet("Disposition check: context",
					&act.sa_mask, &exp_disp.sa_mask);

			ChkExp( act.sa_flags == exp_disp.sa_flags,
				("Flags differ 0x%x [0x%x]\n",
					act.sa_flags, exp_disp.sa_flags) );
		}

		ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
		threadsRunning++;
		ChkInt( pthread_cond_wait(&waitCV, &waitMtx), == 0 );
		ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );
	} while (threadsRunning < TST_TRIAL_THREADS);

	return (0);
}


TST_BEGIN(setHandler)
{
	int			p;
	pthread_t		*pts;
	void			*ret;

	expSigA = SIGUSR1;
	exp_disp.sa_flags = 0;
	sigemptyset(&exp_disp.sa_mask);
	exp_disp.sa_sigaction = hdl_dummy;
	ChkInt( sigaction(expSigA, &exp_disp, 0), == 0 );

	threadsRunning = 0;

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, dispChkThread, 0), == 0 );
	}

	while (threadsRunning != TST_TRIAL_THREADS) { sched_yield(); }
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	expSigA = SIGUSR2;
	exp_disp.sa_flags = SA_RESTART;
	sigfillset(&exp_disp.sa_mask);
	sigdelset(&exp_disp.sa_mask, SIGKILL);
	sigdelset(&exp_disp.sa_mask, SIGSTOP);
	exp_disp.sa_sigaction = hdl_dummy;
	ChkInt( sigaction(expSigA, &exp_disp, 0), == 0 );

	threadsRunning = 0;
	ChkInt( pthread_cond_broadcast(&waitCV), == 0 );

	while (threadsRunning != TST_TRIAL_THREADS) { sched_yield(); }
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	expSigA = SIGUSR1;
	exp_disp.sa_sigaction = SIG_IGN;
	ChkInt( sigaction(expSigA, &exp_disp, 0), == 0 );

	threadsRunning = 0;
	ChkInt( pthread_cond_broadcast(&waitCV), == 0 );

	while (threadsRunning != TST_TRIAL_THREADS) { sched_yield(); }
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	expSigA = SIGUSR2;
	exp_disp.sa_sigaction = SIG_DFL;
	ChkInt( sigaction(expSigA, &exp_disp, 0), == 0 );

	ChkInt( pthread_cond_broadcast(&waitCV), == 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void
hdl_signal_B(int sig, siginfo_t *info, void *arg)
{
	sigset_t	set;
	sigset_t	exp_mask;

	ChkExp( sig == expSigB, ("Unexpected signal %d [%d]\n", sig, expSigB) );
	TstTrace("HandlerB running %d\n", handlerSync);

	ChkInt( pthread_sigmask(0, 0, &set), == 0 );
	sigemptyset(&exp_mask);
	sigaddset(&exp_mask, expSigA);
	sigaddset(&exp_mask, expSigB);
	checkSigSet("HandlerB#1: Blocked", &set, &exp_mask);

	/* Now block - allowing the mask to change
	 *
	 * We know (ahem) that the signal will break the cond wait.
	 */
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	handlerSync++;	/* no need to be atomic */
	ChkInt( pthread_cond_wait(&waitCV, &waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	ChkInt( pthread_sigmask(0, 0, &set), == 0 );
	checkSigSet("HandlerB#2: Blocked", &set, &exp_mask);
}

void
hdl_signal_A(int sig, siginfo_t *info, void *arg)
{
	sigset_t	set;
	sigset_t	exp_mask;

	ChkExp( sig == expSigA, ("Unexpected signal %d [%d]\n", sig, expSigA) );
	TstTrace("HandlerA running %d\n", handlerSync);

	ChkInt( pthread_sigmask(0, 0, &set), == 0 );
	sigemptyset(&exp_mask);
	sigaddset(&exp_mask, expSigA);
	checkSigSet("HandlerA#1: Blocked", &set, &exp_mask);

	/* Now block - allowing the mask to change
	 *
	 * This is a little odd - we know that in the nested case
	 * we do not hold the mutex. XXX
	 */
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	handlerSync++;	/* no need to be atomic */
	ChkInt( pthread_cond_wait(&waitCV, &waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	ChkInt( pthread_sigmask(0, 0, &set), == 0 );
	checkSigSet("HandlerA#2: Blocked", &set, &exp_mask);
}

TST_BEGIN(handleNested)
{
	int			p;
	pthread_t		*pts;
	void			*ret;
	struct sigaction	act;

	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	expSigA = SIGUSR1;
	act.sa_sigaction = hdl_signal_A;
	ChkInt( sigaction(expSigA, &act, 0), == 0 );

	expSigB = SIGUSR2;
	act.sa_sigaction = hdl_signal_B;
	ChkInt( sigaction(expSigB, &act, 0), == 0 );

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	threadsRunning = 1;
	handlerSync = 0;

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, victimThread, 0), == 0 );
	}

	/* First handler
	 */
	TstTrace("Send first signal %d\n", expSigA);
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_kill(pts[p], expSigA), == 0);
	}

	/* Wait for them to get into the handler (1)
	 */
	while (handlerSync != TST_TRIAL_THREADS) { sched_yield(); }
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	/* Now send the nested signal
	 */
	TstTrace("Send second signal %d\n", expSigB);
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_kill(pts[p], expSigB), == 0);
	}

	/* Wait for them to get into the handler (2)
	 */
	while (handlerSync != 2 * TST_TRIAL_THREADS) { sched_yield(); }
	sched_yield();
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	/* Now let them go.
	 */
	threadsRunning = 0;
	ChkInt( pthread_cond_broadcast(&waitCV), == 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */


void
hdl_pending_B(int sig, siginfo_t *info, void *arg)
{
	sigset_t	set;
	sigset_t	exp_mask;

	TstTrace("HandlerB running %d\n", handlerSync);

	ChkExp( handlerSync <= TST_TRIAL_THREADS,
		("Handler run too much %d [%d]\n",
			handlerSync, TST_TRIAL_THREADS) );

	sigemptyset(&exp_mask);
	ChkInt( sigpending(&set), == 0 );
	checkSigSet("HandlerB#1: Pending", &set, &exp_mask);

	sigaddset(&exp_mask, expSigA);
	sigaddset(&exp_mask, expSigB);
	ChkInt( pthread_sigmask(0, 0, &set), == 0 );
	checkSigSet("HandlerB#1: Blocked", &set, &exp_mask);

	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	handlerSync++;
	ChkInt( pthread_cond_wait(&waitCV, &waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	sigemptyset(&exp_mask);
	ChkInt( sigpending(&set), == 0 );
	checkSigSet("HandlerB#2: Pending", &set, &exp_mask);

	ChkInt( pthread_kill(pthread_self(), expSigA), == 0);

	sigemptyset(&exp_mask);
	sigaddset(&exp_mask, expSigA);
	ChkInt( sigpending(&set), == 0 );
	checkSigSet("HandlerB#3: Pending", &set, &exp_mask);

	test_then_add((unsigned long *)&handlerSync, 1);
}

void
hdl_pending_A(int sig, siginfo_t *info, void *arg)
{
	sigset_t	set;
	sigset_t	exp_mask;

	TstTrace("HandlerA running %d\n", handlerSync);

	sigemptyset(&exp_mask);
	sigaddset(&exp_mask, expSigB);

	ChkInt( sigpending(&set), == 0 );
	checkSigSet("HandlerA#1: Pending", &set, &exp_mask);

	ChkInt( pthread_sigmask(0, 0, &set), == 0 );
	sigaddset(&exp_mask, expSigA);
	checkSigSet("HandlerA#1: Blocked", &set, &exp_mask);

	/* Signal B is pending so we unblock it for delivery.
	 */
	sigdelset(&set, expSigB);
	ChkInt( pthread_sigmask(SIG_SETMASK, &set, 0), == 0 );

	/* On return from signal handler B check that
	 * signal A is now pending and blocked.
	 */
	sigemptyset(&exp_mask);
	sigaddset(&exp_mask, expSigA);

	ChkInt( sigpending(&set), == 0 );
	checkSigSet("HandlerA#2: Pending", &set, &exp_mask);

	ChkInt( pthread_sigmask(0, 0, &set), == 0 );
	checkSigSet("HandlerA#2: Blocked", &set, &exp_mask);

	test_then_add((unsigned long *)&handlerSync, 1);
}

void
hdl_pending_A1(int sig, siginfo_t *info, void *arg)
{
	sigset_t	set;
	sigset_t	exp_mask;

	TstTrace("HandlerA1 running %d\n", handlerSync);

	ChkExp( handlerSync <= 4 * TST_TRIAL_THREADS,
		("Handler not run %d [%d]\n",
			handlerSync, 3 * TST_TRIAL_THREADS) );

	sigemptyset(&exp_mask);

	ChkInt( sigpending(&set), == 0 );
	checkSigSet("HandlerA1: Pending", &set, &exp_mask);

	sigaddset(&exp_mask, expSigA);
	sigaddset(&exp_mask, expSigB);
	ChkInt( pthread_sigmask(0, 0, &set), == 0 );
	checkSigSet("HandlerA1: Blocked", &set, &exp_mask);

	test_then_add((unsigned long *)&handlerSync, 1);
}


TST_BEGIN(handlePending)
{
	int			p;
	pthread_t		*pts;
	void			*ret;
	struct sigaction	act;
	sigset_t		set;
	sigset_t		orig_set;

	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	expSigA = SIGUSR1;
	act.sa_sigaction = hdl_pending_A;
	ChkInt( sigaction(expSigA, &act, 0), == 0 );

	expSigB = SIGUSR2;
	act.sa_sigaction = hdl_pending_B;
	ChkInt( sigaction(expSigB, &act, 0), == 0 );

	sigemptyset(&set);
	sigaddset(&set, expSigB);
	ChkInt( pthread_sigmask(SIG_SETMASK, &set, &orig_set), == 0 );

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	threadsRunning = 1;
	handlerSync = 0;

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, victimThread, 0), == 0 );
	}

	/* Signal - currently blocked
	 */
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_kill(pts[p], expSigB), == 0);
		sched_yield();
	}
	ChkExp( handlerSync == 0, ("Handler run out of turn\n") );

	/* Second signal - not blocked
	 * The second signal context unblocks first signal.
	 */
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_kill(pts[p], expSigA), == 0);
	}

	/* Wait for them to get into the second signal's handler.
	 */
	while (handlerSync != TST_TRIAL_THREADS) { sched_yield(); }
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	/* Flip the handler for the second signal and let them go.
	 * When released they send themselves a signal which should
	 * remain pending until return to the main context.
	 */
	sigemptyset(&act.sa_mask);
	act.sa_sigaction = hdl_pending_A1;
	ChkInt( sigaction(expSigA, &act, 0), == 0 );

	threadsRunning = 0;
	ChkInt( pthread_cond_broadcast(&waitCV), == 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	ChkExp( handlerSync == 4 * TST_TRIAL_THREADS,
		("Handler not run %d [%d]\n",
			handlerSync, 3 * TST_TRIAL_THREADS) );

	free(pts);
	ChkInt( pthread_sigmask(SIG_SETMASK, &orig_set, 0), == 0 );

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise signal handler operations" )

	TST( setHandler, "Test sigaction",
		"Set a number of signal dispositions"
		"Create threads and verify the disposition is inherited" ),

	/* Non-portable signal handling tests.
	 * Do not try this at home.
	 */
	TST( handleNested, "Nested signal handler context",
		"Create threads and send signal"
		"Each thread checks its mask against the expected mask"
		"then waits on a condition"
		"A second signal is sent to test nested handlers" ),

	TST( handlePending, "Pending signals handler context",
		"Create threads and send two signals, A, B"
		"B is blocked in initial thread context"
		"A is not blocked"
		"Signal B is sent to all threads (B blocked),"
		"then signal A is sent (A delivered),"
		"the handler for A unblocks signal B (B delivered)"
		"Signal A is sent again (A blocked)"
		"Threads return to the initial context (A delivered" ),

	/*
	( handleJump,
		"Jumping out of a signal context",
		"Create threads with signal A blocked"
		"Each thread calls sigsetjmp()"
		"then unblocks signal A which is delivered"
		"Signal A is sent again but remains pending after"
		"the siglongjmp from the handler." ),
	*/
TST_FINISH

