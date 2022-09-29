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

volatile unsigned long	threadsStarted;
volatile unsigned long	handlerSync;

void hdlSig(int, siginfo_t *, void *);

void
hdlSig(int sig, siginfo_t *info, void *arg)
{
	ChkExp( sig == SIGHUP, ("Unexpected signal %d\n", sig) );
	TstTrace("Handler running %d\n", handlerSync);
	test_then_add((unsigned long *)&handlerSync, 1);
}

/* ------------------------------------------------------------------ */


void
interruptWait(void *(*waiter)(void *), int restart)
{
	int			p;
	pthread_t		*pts;
	void			*ret;
	struct sigaction	act;

	act.sa_sigaction = hdlSig;
	act.sa_flags = (restart ? SA_RESTART : 0);
	sigemptyset(&act.sa_mask);
	ChkInt( sigaction(SIGHUP, &act, 0), == 0 );
	
	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	threadsStarted = 0;
	handlerSync = 0;

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, waiter, 0), == 0 );
	}

	/* Wait for them to start
	 */
	while (threadsStarted != TST_TRIAL_THREADS) {
		sched_yield();
	}
	usleep(250000);
	threadsStarted = 0;

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_kill(pts[p], SIGHUP), == 0);
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	ChkExp( handlerSync == TST_TRIAL_THREADS,
		("Handler not run %d [%d]\n", handlerSync, TST_TRIAL_THREADS) );


	free(pts);
}

/* ------------------------------------------------------------------ */

void *
cv_wait(void *arg)
{
	test_then_add((unsigned long *)&threadsStarted, 1);

	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_cond_wait(&waitCV, &waitMtx), == 0 );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );
	TstTrace("woke up\n");

	return (0);
}

TST_BEGIN(cvKill)
{
	interruptWait(cv_wait, 1);
	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

#define SLEEP	20

void *
sleep_wait(void *arg)
{
	unsigned int	ret;

	test_then_add((unsigned long *)&threadsStarted, 1);

	ChkInt( ret = sleep(SLEEP), != 0 );

	ChkExp( ret > 0 && ret <= SLEEP, ("bad sleep %d [<%d]\n", ret, SLEEP) );
	TstTrace("woke up %d\n", ret);

	return (0);
}


TST_BEGIN(sleepKill)
{
	interruptWait(sleep_wait, 1);
	TST_RETURN(0);
}


/* ------------------------------------------------------------------ */

#define USLEEP	900000	/* maximise chance of being in usleep() */

void *
usleep_wait(void *arg)
{
	int	count = 0;

	test_then_add((unsigned long *)&threadsStarted, 1);

	while (!usleep(USLEEP)) {
		ChkExp( count++ < 100, ("Not signalled in %d secs\n",
					(USLEEP * count) / 1000000) );
	}

	ChkExp( errno == EINVAL, ("bad usleep %d\n", errno) );
	TstTrace("woke up\n");

	return (0);
}


TST_BEGIN(usleepKill)
{
	interruptWait(usleep_wait, 1);
	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void *
nanosleep_wait(void *arg)
{
	unsigned int	ret;
	struct timespec	in_time;
	struct timespec	out_time;

	in_time.tv_sec = SLEEP;
	in_time.tv_nsec = 0;

	test_then_add((unsigned long *)&threadsStarted, 1);

	ChkInt( ret = nanosleep(&in_time, &out_time), != 0 );

	ChkExp( out_time.tv_sec > 0 && out_time.tv_sec < SLEEP,
		("bad nanosleep %d [<%d]\n", out_time.tv_sec, SLEEP) );
	TstTrace("woke up %d\n", out_time.tv_sec);

	return (0);
}


TST_BEGIN(nanosleepKill)
{
	interruptWait(nanosleep_wait, 1);
	TST_RETURN(0);
}


/* ------------------------------------------------------------------ */

int	pipeFds[2];

void *
syscall_wait(void *arg)
{
	char	buf[1];

	test_then_add((unsigned long *)&threadsStarted, 1);

	ChkInt( read(pipeFds[0], &buf, 1), == -1 );
	ChkExp( errno == EINTR, ("read() failed %d [%d]\n", errno, EINTR) );
	TstTrace("woke up\n");

	return (0);
}

TST_BEGIN(syscallKill)
{
	ChkInt( pipe(pipeFds), == 0 );

	interruptWait(syscall_wait, 0);

	ChkInt( close(pipeFds[0]), == 0 );
	ChkInt( close(pipeFds[1]), == 0 );

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise signals" )

	TST( cvKill, "Interrupt a thread in a condition wait call", 0 ),

	TST( sleepKill, "Interrupt a thread in a sleep call", 0 ),

	TST( usleepKill, "Interrupt a thread in a usleep call", 0 ),

	TST( nanosleepKill, "Interrupt a thread in a nanosleep call", 0 ),

	TST( syscallKill, "Interrupt a thread in a blocking call", 0 ),

TST_FINISH

