/*
** Purpose: Test
*/

#include <pthread.h>
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <mutex.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

volatile unsigned long	threadsStarted;

/* ------------------------------------------------------------------ */

#define MSG_SENDS	100
#define	MSG_FMT		"Message %d/%d"
#define MSG_BUFLEN	50

mqd_t		*mqs;
int		*ctrs;
mq_attr_t	msgQAttr;	/* for msg size */

void
mq_thread(union sigval sv)
{
	int	p = sv.sival_int;
	char	*msgBuf = alloca(msgQAttr.mq_msgsize);
	char	expMsgBuf[MSG_BUFLEN];
	uint_t	pri;

	TstTrace("Running %#x %d\n", pthread_self(), p);
	test_then_add((unsigned long *)&threadsStarted, 1);
	sprintf(expMsgBuf, MSG_FMT, p, ctrs[p]);
	ctrs[p]++;

	CHKInt( mq_receive(mqs[p], msgBuf, msgQAttr.mq_msgsize, &pri), > 0 );
	TstTrace("Msg %s\n", msgBuf);
	ChkExp( strcmp(msgBuf, expMsgBuf) == 0,
		("bad message %s [%s]\n", msgBuf, expMsgBuf) );
	ChkExp( pri == 0, ("bad priority %d [%d]\n", pri, 0) );

	return;
}


TST_BEGIN(mqSigev)
{
	int		p;
	sigevent_t	sigev;
	int		events;
	char		msgBuf[MSG_BUFLEN];
	char		msgQName[L_tmpnam];
	int		i;
	int		l;

	ChkPtr( mqs = calloc(TST_TRIAL_THREADS, sizeof(mqd_t)), != 0 );
	ChkPtr( ctrs = calloc(TST_TRIAL_THREADS, sizeof(int)), != 0 );

	/* Create message queues.
	 */
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkPtr( tmpnam(msgQName), == msgQName );
		CHKInt( mqs[p] = mq_open(msgQName, O_RDWR|O_CREAT|O_EXCL, 0777, 0),
				!= -1 );
		CHKInt( mq_unlink(msgQName), == 0 );

		CHKInt( mq_getattr(mqs[p], &msgQAttr), == 0 );
		ChkExp( msgQAttr.mq_msgsize >= MSG_BUFLEN,
			("msg too small %d [%d]\n",
			 msgQAttr.mq_msgsize, MSG_BUFLEN) );
	}

	/* Set thread notification for each queue and send a message.
	 * Wait for queue to drain before repeating.
	 */
	threadsStarted = 0;
	sigev.sigev_notify = SIGEV_THREAD;
	sigev.sigev_notify_function = mq_thread;
	sigev.sigev_notify_attributes = 0;
	sigev.sigev_signo = SIGRTMIN;
	for (i = 0; i < MSG_SENDS; i++) {
		for (p = 0; p < TST_TRIAL_THREADS; p++) {
			sigev.sigev_value.sival_int = p;
			CHKInt( mq_notify(mqs[p], &sigev), == 0 );

			l = sprintf(msgBuf, MSG_FMT, p, i);
			CHKInt( mq_send(mqs[p], msgBuf, l+1, 0), == 0 );
		}
		for (p = 0; p < TST_TRIAL_THREADS; p++) {
			for (;;) {
				CHKInt( mq_getattr(mqs[p], &msgQAttr),
					== 0 );
				if (!msgQAttr.mq_curmsgs) {
					break;
				}
				sched_yield();
			}
		}
	}

	/* Delete message queues.
	 */
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		CHKInt( mq_close(mqs[p]), == 0 );
	}

	/* Check total events and per-thread events.
	 */
	events = TST_TRIAL_THREADS * MSG_SENDS;
	TstTrace("Total events %d [%d]\n", threadsStarted, events);
	ChkExp( threadsStarted == events,
		("Missing events %d [%d]\n", threadsStarted, events) );

	events = MSG_SENDS;
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		TstTrace("Events[%d] %d [%d]\n", p, ctrs[p], events);
		ChkExp( ctrs[p] == events,
			("Missing events %d %d [%d]\n",
			 p, threadsStarted, events) );
	}

	free(mqs);
	free(ctrs);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise SIGEV_THREAD events" )

	TST( mqSigev, "Create thread based on POSIX messages", 0 ),

TST_FINISH

