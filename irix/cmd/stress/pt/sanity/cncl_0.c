/*
** Purpose: Test
*/

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

pthread_cond_t  waitCV = PTHREAD_COND_INITIALIZER;
pthread_mutex_t waitMtx = PTHREAD_MUTEX_INITIALIZER;

void	cnclHandler(void *);
void	cnclHandlerUnlock(void *);

void
cnclHandler(void *arg)
{
	char	*msg = (char *)arg;

	TstTrace("Handler running %s\n", msg);
	ChkInt( strcmp(msg, "GOOD"), == 0 );
}


void
cnclHandlerUnlock(void *arg)
{
	pthread_mutex_t	*mtx = (pthread_mutex_t *)arg;

	ChkInt( pthread_mutex_unlock(mtx), == 0 );
}

/* ------------------------------------------------------------------ */

void *
basic_semantics(void *arg)
{
	int	i;
	int	type;
	int	state;

#define SetType(newtype, oldtype) \
	ChkInt( pthread_setcanceltype(newtype, &type), == 0 ); \
	ChkExp( type == oldtype, ("bad cancel type %d [%d]\n", type, oldtype) );

#define SetState(newstate, oldstate) \
	ChkInt( pthread_setcancelstate(newstate, &state), == 0 ); \
	ChkExp( state == oldstate, \
		("bad cancel state %d [%d]\n", state, oldstate) );

	SetType(PTHREAD_CANCEL_DEFERRED, PTHREAD_CANCEL_DEFERRED);
	SetType(PTHREAD_CANCEL_ASYNCHRONOUS, PTHREAD_CANCEL_DEFERRED);
	SetType(PTHREAD_CANCEL_DEFERRED, PTHREAD_CANCEL_ASYNCHRONOUS);
	SetType(PTHREAD_CANCEL_DEFERRED, PTHREAD_CANCEL_DEFERRED);

	SetType(PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_ENABLE);
	SetType(PTHREAD_CANCEL_DISABLE, PTHREAD_CANCEL_ENABLE);
	SetType(PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DISABLE);
	SetType(PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_ENABLE);

	return (0);
}


TST_BEGIN(basicSemantics)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void		*basic_semantics(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, basic_semantics, 0), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void *
sync_cancel_cvwait(void *arg)
{
	int	i;

	ChkInt( pthread_mutex_lock(&waitMtx), == 0 );

	pthread_cleanup_push(cnclHandlerUnlock, &waitMtx);
	pthread_cleanup_push(cnclHandler, "GOOD");
		TstTrace("waiting\n");
		ChkInt( pthread_cond_wait(&waitCV, &waitMtx), == 0 );
		ChkExp( 0, ("not cancelled\n") );
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);

	ChkInt( pthread_mutex_unlock(&waitMtx), == 0 );

	pthread_exit(0);
	/* NOTREACHED */
}


TST_BEGIN(syncCancelCVwait)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void		*sync_cancel_cvwait(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, sync_cancel_cvwait, 0),
			== 0 );
	}

	TST_NAP(TST_DOZE);

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_cancel(pts[p]), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("join %#x %#x\n", ret, PTHREAD_CANCELED) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

int	pipeFds[2];

void *
sync_cancel_syscall0(void *arg)
{
	char	buf[1];

	pthread_cleanup_push(cnclHandler, "GOOD");
		TstTrace("blocking\n");
		(void)read(pipeFds[0], buf, 1);
		ChkExp( 0, ("not cancelled\n") );
	pthread_cleanup_pop(0);

	pthread_exit(0);
	/* NOTREACHED */
}

void *
sync_cancel_syscall1(void *arg)
{
	pthread_cleanup_push(cnclHandler, "GOOD");
		TstTrace("sleeping\n");
		sleep(10);
		ChkExp( 0, ("not cancelled\n") );
	pthread_cleanup_pop(0);

	pthread_exit(0);
	/* NOTREACHED */
}


TST_BEGIN(syncCancelSyscall)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void		*sync_cancel_syscall0(void *);
	void		*sync_cancel_syscall1(void *);

	CHKPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	/* syscall0 */

	CHKInt( pipe(pipeFds), == 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, sync_cancel_syscall0, 0),
			== 0 );
	}

	TST_NAP(TST_DOZE);

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_cancel(pts[p]), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("join %#x %#x\n", ret, PTHREAD_CANCELED) );
	}

	ChkInt( close(pipeFds[0]), == 0 );
	ChkInt( close(pipeFds[1]), == 0 );

	/* syscall1 */

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, sync_cancel_syscall1, 0),
			== 0 );
	}

	TST_NAP(TST_DOZE);

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_cancel(pts[p]), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("join %#x %#x\n", ret, PTHREAD_CANCELED) );
	}

	free(pts);
	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void *
sync_cancel_self(void *arg)
{
	int	i;

	pthread_cleanup_push(cnclHandler, "BAD");
		ChkInt( pthread_cancel(pthread_self()), == 0 );
	pthread_cleanup_pop(0);

	pthread_cleanup_push(cnclHandler, "GOOD");
		TstTrace("sinking\n");
		pthread_testcancel();
	pthread_cleanup_pop(0);

	ChkExp( 0, ("not cancelled\n") );
	pthread_exit(0);
	/* NOTREACHED */
}


TST_BEGIN(syncCancelSelf)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void		*sync_cancel_self(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, sync_cancel_self, 0), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("join %#x %#x\n", ret, PTHREAD_CANCELED) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void *
sync_no_cancel_syscall(void *arg)
{
	int	i;

	pthread_cleanup_push(cnclHandler, "BAD");
		ChkInt( pthread_cancel(pthread_self()), == 0 );
		__write(-1, 0, 0);
	pthread_cleanup_pop(0);

	pthread_exit(0);
	/* NOTREACHED */
}

TST_BEGIN(syncNoCancel)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void		*sync_no_cancel_syscall(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, sync_no_cancel_syscall, 0),
		== 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void *
async_cancel_self(void *arg)
{
	int	i;
	int	type;

	ChkInt( pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &type),
		== 0 );
	ChkExp( type == PTHREAD_CANCEL_DEFERRED,
		("bad cancel type %d [%d]\n", type, PTHREAD_CANCEL_DEFERRED) );

	pthread_cleanup_push(cnclHandler, "GOOD");
		TstTrace("suiciding\n");
		ChkInt( pthread_cancel(pthread_self()), == 0 );
		ChkExp( 0, ("not cancelled\n") );
	pthread_cleanup_pop(0);

	pthread_exit(0);
	/* NOTREACHED */
}


TST_BEGIN(asyncCancelSelf)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void		*async_cancel_self(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, async_cancel_self, 0),
			== 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("join %#x %#x\n", ret, PTHREAD_CANCELED) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

void *
async_cancel_fifo(void *arg)
{
	volatile int	i;

	/* Watch out : async cancel may not be safe
	 */
	TstTrace("started\n");
	pthread_cleanup_push(cnclHandler, "GOOD");
		ChkInt( pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0),
			== 0 );
		for (i = 0; ; i++) ;
		/* NOTREACHED */
		ChkExp( 0, ("not cancelled\n") );
	pthread_cleanup_pop(0);

	pthread_exit(0);
	/* NOTREACHED */
}


TST_BEGIN(asyncCancelFIFO)
{
	int			p;
	pthread_attr_t		pta;
	struct sched_param	sp;
	pthread_t		*pts;
	void			*ret;
	void			*async_cancel_fifo(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	ChkInt( pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp),
		== 0 );

	ChkInt( pthread_attr_init(&pta), == 0 );
	ChkInt( pthread_attr_setschedpolicy(&pta, SCHED_FIFO), == 0 );
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, &pta, async_cancel_fifo, 0),
			== 0 );
	}

	TST_NAP(TST_PAUSE);

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_cancel(pts[p]), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == PTHREAD_CANCELED,
			("join %#x %#x\n", ret, PTHREAD_CANCELED) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Simple cancellation" )

	TST( basicSemantics,
		"Basic cancel state and interfaces", 0 ),

	TST( syncCancelCVwait,
		"Sync cancel condition variable wait", 0 ),

	TST( syncCancelSyscall,
		"Sync cancel syscall", 0 ),

	TST( syncCancelSelf,
		"Pending cancel acted on at cancellation point", 0 ),

	TST( syncNoCancel,
		"Pending cancel ignored by non-cancellation points", 0 ),

	TST( asyncCancelSelf,
		"Async cancel of self acted on immediately", 0 ),

	TST( asyncCancelFIFO,
		"Async cancel works on FIFO policy targets", 0 ),

TST_FINISH
