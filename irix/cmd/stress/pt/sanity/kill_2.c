/*
** Purpose: Exercise odd signal cases.
*/

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <Tst.h>

/* ------------------------------------------------------------------ */

pthread_t	sigTarget;
int		handlerCalls;
int		maxSignals;

/* ------------------------------------------------------------------ */

machreg_t	nestLoSp;
machreg_t	nestHiSp;

void
sig_nest_hdl(int s, siginfo_t *si, struct sigcontext *sc)
{
	sigset_t	set;
	machreg_t	sp = sc->sc_regs[29];

	handlerCalls++;
	TstTrace("Nest handler %d sp 0x%lx\n", handlerCalls, sp);

	if (!nestHiSp) {
		nestHiSp = nestLoSp = sp;
	} else {
		if (nestHiSp < sp)
			nestHiSp = sp;
		if (nestLoSp > sp)
			nestLoSp = sp;
	}

	ChkExp( pthread_equal(pthread_self(), sigTarget),
		("Ooops missed 0x%x [0x%x]\n", pthread_self(), sigTarget) );

	sigemptyset(&set);
	ChkInt( pthread_sigmask(SIG_SETMASK, 0, &set), == 0 );
	ChkInt( sigismember(&set, SIGHUP), != 0 );
	sigemptyset(&set);
	ChkInt( sigpending(&set), == 0 );
	ChkInt( sigismember(&set, SIGHUP), == 0 );

	if (handlerCalls == maxSignals) {
		return;
	}

	/* raise signal again */
	ChkInt( pthread_kill(pthread_self(), SIGHUP), == 0 );

	/* check it is now pending */
	sigemptyset(&set);
	ChkInt( sigpending(&set), == 0 );
	ChkInt( sigismember(&set, SIGHUP), != 0 );
}


void *
sig_nest(void *arg)
{
	sigTarget = pthread_self();

	ChkInt( pthread_kill(pthread_self(), SIGHUP), == 0 );
	return (0);
}


TST_BEGIN(sigNest)
{
	struct sigaction act;
	sigset_t	set;

	act.sa_handler = sig_nest_hdl;
	sigfillset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGHUP, &act, 0);
	sigemptyset(&set);
	ChkInt( pthread_sigmask(SIG_SETMASK, &set, 0), == 0 );

	maxSignals = 100;
	handlerCalls = 0;
	nestLoSp = nestHiSp = 0;

	sig_nest(0);

	TstInfo("Done: ran handler %d times lo 0x%lx hi 0x%lx 0x%lx\n",
		handlerCalls, nestLoSp, nestHiSp, nestHiSp - nestLoSp);

	/* check all signals received */
	ChkExp( handlerCalls == maxSignals,
		("Missed signal %d [%d]\n", handlerCalls, maxSignals) );

	/* check same stack base used for handler */
	ChkExp( nestHiSp - nestLoSp == 0,
		("Recursion lo 0x%lx hi 0x%lx 0x%lx\n",
		nestLoSp, nestHiSp, nestHiSp - nestLoSp) );


	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

int	retryHandler;
int	pipeFds[2];
int	doYield;


void
sig_retry_hdl(int s, siginfo_t *si, struct sigcontext *sc)
{
	handlerCalls++;
	TstTrace("Retry handler %d\n", handlerCalls);

	ChkExp( pthread_equal(pthread_self(), sigTarget),
		("Ooops missed 0x%x [0x%x]\n", pthread_self(), sigTarget) );

	if (doYield) {
		sched_yield();	/* catch signal at handler exit */
	}
}

void *
sig_retry(void *arg)
{
	int	ret;
	char	buf;

	sigTarget = pthread_self();

	ChkInt( ret = read(pipeFds[0], &buf, 1), == -1 );
	ChkExp( 0, ("Retried system call was interrupted %d %d\n",
			ret, errno) );
	TstTrace("woke up\n");
	return (0);
}

void
sigRetryBody(int yield)
{
	struct sigaction act;
	sigset_t	set;
	pthread_t	pt;
	pthread_attr_t	pta;
	void		*ret;
	int		sentSigs = 0;

	act.sa_handler = sig_retry_hdl;
	sigfillset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	sigaction(SIGHUP, &act, 0);
	sigemptyset(&set);
	ChkInt( pthread_sigmask(SIG_SETMASK, &set, 0), == 0 );

	ChkInt( pipe(pipeFds), == 0 );
	maxSignals = 100;
	handlerCalls = 0;
	doYield = yield;

	/* start thread */
	ChkInt( pthread_attr_init(&pta), == 0 );
	ChkInt( pthread_attr_setstacksize(&pta, 0x8000), == 0 );
	ChkInt( pthread_create(&pt, &pta, sig_retry, 0), == 0 );
	ChkInt( pthread_attr_destroy(&pta), == 0 );

	/* pause to let thread block */
	sleep(1);

	/* send signal, wait for counter to reach target
	 */
	while (handlerCalls < maxSignals) {
		ChkInt( pthread_kill(pt, SIGHUP), == 0);
		sentSigs++;
		while (sentSigs != handlerCalls) {
			sched_yield();
		}
	}

	/* cancel thread */
	ChkInt( pthread_cancel(pt), == 0 );
	ChkInt( pthread_join(pt, &ret), == 0 );
	ChkExp( ret == PTHREAD_CANCELED, ("bad join %#x %#x\n", ret, 0) );

	ChkInt( close(pipeFds[0]), == 0 );
	ChkInt( close(pipeFds[1]), == 0 );
}


TST_BEGIN(sigRetry)
{
	TstInfo("No yield test\n");
	sigRetryBody(0);

	TstInfo("Yield test\n");
	sigRetryBody(1);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void
sig_susp_hdl(int s, siginfo_t *si, struct sigcontext *sc)
{
	sigset_t	set;

	TstTrace("Suspend handler\n");
	handlerCalls++;

	ChkInt( pthread_sigmask(SIG_SETMASK, 0, &set), == 0 );
	ChkInt( sigismember(&set, SIGPTRESCHED), == 0 );
	ChkInt( sigismember(&set, SIGPTINTR), == 0 );
}

void *
sig_susp(void *arg)
{
	sigset_t	set;
	int		ret;

	ChkInt( pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0), == 0 );

	sigemptyset(&set);
	CHKInt( ret = sigsuspend(&set), == -1 );

	ChkInt( pthread_sigmask(SIG_SETMASK, 0, &set), == 0 );
	ChkInt( sigismember(&set, SIGPTRESCHED), == 0 );
	ChkInt( sigismember(&set, SIGPTINTR), == 0 );

	/* Now spin - we need to be interrupted to be cancelled
	 */
	for (;;);

	/* NOTREACHED */
	ChkExp( 0, ("sigsuspend() returned %d %d\n",
			ret, errno) );
	return (0);
}

TST_BEGIN(sigSuspKill)
{
	struct sigaction act;
	sigset_t	set;
	pthread_t	pt;
	void		*ret;

	act.sa_handler = sig_susp_hdl;
	sigfillset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGHUP, &act, 0);
	sigfillset(&set);
	ChkInt( pthread_sigmask(SIG_SETMASK, &set, 0), == 0 );

	ChkInt( pthread_create(&pt, 0, sig_susp, 0), == 0 );

	/* pause to let thread block and then
	 * prod it into the signal handler
	 */
	sleep(1);
	handlerCalls = 0;
	ChkInt( pthread_kill(pt, SIGHUP), == 0);

	/* wait a bit then cancel the thread */
	sleep(1);
	ChkInt( pthread_cancel(pt), == 0 );
	ChkInt( pthread_join(pt, &ret), == 0 );
	ChkExp( ret == PTHREAD_CANCELED, ("bad join %#x %#x\n", ret, 0) );

	ChkExp( handlerCalls == 1,
		("Missed signal %d [%d]\n", handlerCalls, 1) );

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise signals II" )

	TST( sigNest, "Verify nested signals do not recurse", 0 ),
	TST( sigRetry, "Verify restart signals do not recurse", 0 ),
	TST( sigSuspKill, "Verify sigsuspend has correct mask", 0 ),

TST_FINISH

