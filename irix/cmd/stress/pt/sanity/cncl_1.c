/*
** Purpose: Test cancellation points
*/

#include <pthread.h>
#include <aio.h>
#include <mutex.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

volatile unsigned long	threadsStarted;
volatile unsigned long	threadsCancelled;

void	cnclHandler(void *);

/* ------------------------------------------------------------------ */

void
cnclHandler(void *arg)
{
	aiocb_t	*cb = (aiocb_t*)arg;

	TstTrace("Handler running 0x%p\n", cb);
	test_then_add((unsigned long *)&threadsCancelled, 1);
}



void *
aio_suspend_cancel(void *arg)
{
	aiocb_t	**cb = (aiocb_t**)arg;

	TstTrace("Running %#x 0x%p\n", pthread_self(), *cb);
	CHKInt( aio_read(*cb), == 0);
	test_then_add((unsigned long *)&threadsStarted, 1);

	pthread_cleanup_push(cnclHandler, *cb);
		TstTrace("suspending 0x%p\n", *cb);
		CHKInt( aio_suspend((const aiocb_t **)cb, 1, 0), == 0xdead );
		ChkExp( 0, ("%#x not cancelled\n", pthread_self()) );
	pthread_cleanup_pop(0);

	pthread_exit(0);
	/* NOTREACHED */
}


int
aioSuspendCancel(int delay)
{
	int		p;
	int		a;
	pthread_t	*pts;
	void		*ret;
	char		buf;
	char		out;
	FILE		*fp;
	aiocb_t		*aios;
	aiocb_t		**aiop;
	int		pipe_fds[2];
	void		*aio_suspend_cancel(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	ChkPtr( aios = calloc(TST_TRIAL_THREADS, sizeof(aiocb_t)), != 0 );
	ChkPtr( aiop = malloc(TST_TRIAL_THREADS * sizeof(aiocb_t *)), != 0 );

	threadsStarted = 0;
	threadsCancelled = 0;
	ChkInt( pipe(pipe_fds), == 0 );
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		aios[p].aio_fildes = pipe_fds[0];
		aios[p].aio_offset = 0;
		aios[p].aio_buf = &buf;
		aios[p].aio_nbytes = 1;
		aios[p].aio_reqprio = 0;
		aios[p].aio_sigevent.sigev_notify = SIGEV_NONE;
		aiop[p] = aios + p;

		ChkInt( pthread_create(pts+p, 0, aio_suspend_cancel, aiop+p),
			== 0 );
	}

	if (delay) {
		/* Make sure they block in aio_suspend()
		 */
		for (p = 0; p < TST_TRIAL_THREADS; p++) {
			sched_yield();
		}
		TST_NAP(TST_DOZE);
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_cancel(pts[p]), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("join %#x %#x\n", ret, PTHREAD_CANCELED) );
	}

	ChkExp( threadsStarted == TST_TRIAL_THREADS,
		("Started %d [%d]\n", threadsStarted, TST_TRIAL_THREADS) );

	ChkExp( threadsCancelled == TST_TRIAL_THREADS,
		("Cancelled %d [%d]\n", threadsCancelled, TST_TRIAL_THREADS) );

	/* Drain aio requests the hard way.
	 * AIO cancel will not work as some may be in progress.
	 */
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		out = 'a' + p;
		TstTrace("Write %c\n", out);
		CHKInt( write(pipe_fds[1], &out, 1), == 1 );
		CHKInt( aio_suspend((const aiocb_t **)aiop,
				      TST_TRIAL_THREADS, 0), == 0 );

		for (;;)  {
			for (a = 0; a < TST_TRIAL_THREADS; a++) {
				if (!aios[a].aio_buf
				    || aio_error(aiop[a]) == EINPROGRESS) {
					continue;
				}
				ChkInt( aio_return(aiop[a]), == 1);
				TstTrace("Read %c\n", buf);
				ChkExp( buf == out,
					("bad read %#x [%#x]\n", buf, out) );
				aios[a].aio_buf = 0;
				break;
			}
			if (a != TST_TRIAL_THREADS) {
				break;
			}
			sched_yield();
		}
	}
	CHKInt( aio_cancel(pipe_fds[0], 0), == AIO_ALLDONE );
	CHKInt( close(pipe_fds[0]), == 0 );
	CHKInt( close(pipe_fds[1]), == 0 );

	free(aiop);
	free(aios);
	free(pts);

	return (0);
}

/* ------------------------------------------------------------------ */

TST_BEGIN(aioSuspendCancel0)
{
	TST_RETURN(aioSuspendCancel(0));
}

/* ------------------------------------------------------------------ */

TST_BEGIN(aioSuspendCancel1)
{
	TST_RETURN(aioSuspendCancel(1));
}

/* ------------------------------------------------------------------ */

TST_START( "Cancellation points" )

	TST( aioSuspendCancel0,
		"aio suspend cancel entry",
		"Create threads which call aio_read()s on a pipe,"
		"then call aio_suspend() to wait for input."
		"Parent cancels the children immediately after creation."
		"Verify children started and were cancelled."),

	TST( aioSuspendCancel1,
		"aio suspend cancel delayed",
		"Create threads which call aio_read()s on a pipe,"
		"then call aio_suspend() to wait for input."
		"Parent cancels the children after a pause."
		"Verify children started and were cancelled."),

TST_FINISH
