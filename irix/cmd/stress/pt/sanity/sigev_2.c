/*
** Purpose: Test
*/

#include <pthread.h>
#include <aio.h>
#include <errno.h>
#include <fcntl.h>
#include <mutex.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

volatile unsigned long	threadsStarted;
volatile unsigned long	threadsCompleted;

/* ------------------------------------------------------------------ */

#define MSG_SENDS	100
#define	MSG_FMT		"Message %d/%d"
#define MSG_BUFLEN	50

aiocb_t		*aios;
int		*ctrs;

void
aio_thread(union sigval sv)
{
	int	p = sv.sival_int;
	char	msgBuf[MSG_BUFLEN];
	char	expMsgBuf[MSG_BUFLEN];

	TstTrace("Running %#x %d\n", pthread_self(), p);
	test_then_add((unsigned long *)&threadsStarted, 1);
	sprintf(expMsgBuf, MSG_FMT, p, ctrs[p]);
	ctrs[p]++;

	CHKInt( pread(aios[p].aio_fildes, msgBuf, sizeof(msgBuf),
			aios[p].aio_offset), == sizeof(msgBuf) );
	TstTrace("Msg: %s\n", msgBuf);
	ChkExp( strcmp(msgBuf, expMsgBuf) == 0,
		("bad message %s [%s]\n", msgBuf, expMsgBuf) );
	test_then_add((unsigned long *)&threadsCompleted, 1);
	return;
}


TST_BEGIN(aioSigev)
{
	int		p;
	sigevent_t	sigev;
	FILE		*fp;
	int		messages = 0;
	char		*bufs;
	int		i;

	ChkPtr( aios = calloc(TST_TRIAL_THREADS, sizeof(aiocb_t)), != 0 );
	ChkPtr( ctrs = calloc(TST_TRIAL_THREADS, sizeof(int)), != 0 );
	ChkPtr( bufs = malloc(TST_TRIAL_THREADS * MSG_BUFLEN), != 0 );

	/* Create aio files and aio requests.
	 */
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkPtr( fp = tmpfile(), != 0 );
		aios[p].aio_fildes = fileno(fp);
		aios[p].aio_offset = p * MSG_BUFLEN;
		aios[p].aio_buf = bufs + aios[p].aio_offset;
		aios[p].aio_nbytes = MSG_BUFLEN;
		aios[p].aio_reqprio = 0;

		aios[p].aio_sigevent.sigev_notify = SIGEV_THREAD;
		aios[p].aio_sigevent.sigev_notify_function = aio_thread;
		aios[p].aio_sigevent.sigev_notify_attributes = 0;
		aios[p].aio_sigevent.sigev_value.sival_int = p;
		aios[p].aio_sigevent.sigev_signo = SIGRTMIN;
	}

	/* Issue aio requests and wait for drain.
	 */
	threadsStarted = 0;
	for (i = 0; i < MSG_SENDS; i++) {
		for (p = 0; p < TST_TRIAL_THREADS; p++) {

			sprintf((char *)(aios[p].aio_buf), MSG_FMT, p, i);
			CHKInt( aio_write(aios + p), == 0);
			messages++;
		}
		while (threadsCompleted < messages) {
			sched_yield();
			TST_NAP(TST_PAUSE);
		}
		ChkExp( threadsStarted == messages,
			("Missing messages %d*%d finished %d [%d]\n",
			i+1, TST_TRIAL_THREADS,
			threadsCompleted, messages) );
	}

	/* Delete aio files.
	 */
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		CHKInt( close(aios[p].aio_fildes), == 0 );
	}

	/* Check total events and per-thread events.
	 */
	TstTrace("Total events %d [%d]\n", threadsStarted, messages);
	ChkExp( threadsStarted == messages,
		("Missing messages %d [%d]\n", threadsStarted, messages) );

	messages = MSG_SENDS;
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		TstTrace("Messages[%d] %d [%d]\n", p, ctrs[p], messages);
		ChkExp( ctrs[p] == messages,
			("Missing messages %d %d [%d]\n",
			 p, threadsStarted, messages) );
	}

	free(bufs);
	free(aios);
	free(ctrs);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise SIGEV_THREAD events" )

	TST( aioSigev, "Create thread based on POSIX aio", 0 ),

TST_FINISH

