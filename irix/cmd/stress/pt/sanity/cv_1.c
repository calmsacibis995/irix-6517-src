/*
** Purpose: Benchmark
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <Tst.h>

/* ------------------------------------------------------------------ */

pthread_cond_t  waitCV = PTHREAD_COND_INITIALIZER;
pthread_mutex_t	waitMtx = PTHREAD_MUTEX_INITIALIZER;


/* ------------------------------------------------------------------ */

void *
cvt_no_wait(void *arg)
{
	timespec_t	ts;
	int		c;

	BnchDecl(tv_start);
	BnchDecl(tv_stop);

	ts.tv_sec = ts.tv_nsec = 0;

	BnchStamp(&tv_start);
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_cond_timedwait(&waitCV, &waitMtx, &ts),
		== ETIMEDOUT );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );

	BnchStamp(&tv_start);
	for (c = 0; c < LARGE_BENCH_LOOP; c++) {
		(void)pthread_cond_timedwait(&waitCV, &waitMtx, &ts);
	}
	BnchStamp(&tv_stop);

	BnchDelta(&tv_start, &tv_start, &tv_stop);
	BnchDivide(&tv_start, LARGE_BENCH_LOOP);
	BnchPrint("cvt_no_wait", &tv_start);

	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	return (0);
}

TST_BEGIN(cvtNoWait)
{
	pthread_t	pt;
	void		*ret;

	BnchTries("cvtNoWait", LARGE_BENCH_LOOP);

	ChkInt( pthread_create(&pt, 0, cvt_no_wait, 0), == 0 );
	ChkInt( pthread_join(pt, &ret), == 0 );
	ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

#define REPEAT	10

void *
cvt_wait(void *arg)
{
#	undef		NSEC_PER_SEC
#	define		NSEC_PER_SEC 1000000000
	timespec_t	ts;
	struct timeval	tv;
	long long	period = *(long long *)arg;
	int		s = period / NSEC_PER_SEC;
	int		ns = period % NSEC_PER_SEC;
	char		buf[100];
	int		c;

	BnchDecl(tv_start);
	BnchDecl(tv_stop);
	BnchDecl(tv_accum);

	BnchStamp(&tv_start);
	ts.tv_sec = ts.tv_nsec = 0;
	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );
	ChkInt( pthread_cond_timedwait(&waitCV, &waitMtx, &ts),
		== ETIMEDOUT );
	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	BnchZero(&tv_accum);
	for (c = 0; c < REPEAT; c++) {

		ChkInt( pthread_mutex_lock(&waitMtx), == 0 );

		ChkInt( gettimeofday(&tv, 0), == 0 );
		ts.tv_sec = tv.tv_sec + s;
		ts.tv_nsec = tv.tv_usec * 1000 + ns;
		if (ts.tv_nsec > NSEC_PER_SEC) {
			ts.tv_sec++;
			ts.tv_nsec -= NSEC_PER_SEC;
		}

		BnchStamp(&tv_start);
		(void)pthread_cond_timedwait(&waitCV, &waitMtx, &ts);
		BnchStamp(&tv_stop);

		BnchDelta(&tv_start, &tv_start, &tv_stop);
		BnchAccum(&tv_accum, &tv_start);
		ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );
	}

	BnchDivide(&tv_accum, REPEAT);
	sprintf(buf, "cvt_wait_%010lldns", period);
	BnchPrint(buf, &tv_accum);

	return (0);
}

TST_BEGIN(cvtWait)
{
	long long	t;
	pthread_t	pt;
	void		*ret;

	BnchTries("cvtWait", REPEAT);

	for (t = NSEC_PER_SEC/4; t <= NSEC_PER_SEC * 2; t += NSEC_PER_SEC/4) {
		
		ChkInt( pthread_create(&pt, 0, cvt_wait, &t), == 0 );
		ChkInt( pthread_join(pt, &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */


TST_START( "cv timed wait testing/timing" )

	TST( cvtNoWait,
		"Wait with an expired timeout",
		"Allocate a cv and mtx,\n"
		"time waits (timeouts) on the condition.\n" ),

	TST( cvtWait,
		"XXX Wait with various timeouts",

		/* XXX This test suffers from the absolute timeout
		   requirement - ideally the timeout should include
		   reading the stamp.
		 */

		"Allocate a cv and mtx,\n"
		"time waits with various timeouts on the condition.\n" ),

TST_FINISH


