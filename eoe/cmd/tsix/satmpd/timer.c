#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/t6satmp.h>
#include "sem.h"
#include "thread.h"
#include "timer.h"

/* type of interval timer to use */
static const int TIMER_TYPE = ITIMER_REAL;

/* name of the alarm signal */
static const int TIMER_SIG = SIGALRM;

static mutex tinq_lock;
static tqueue *tinq_head, *tinq_tail;

static mutex toutq_lock;
static condvar toutq_condvar;
static tqueue *toutq_head, *toutq_tail;

/* this holds the value of current_time()
   whenever an interval timer is started */
static unsigned int timer_set_timestamp;
static unsigned int current_time (void);

/*******************************************************************
 * INITIALIZATION/DESTRUCTION FUNCTIONS
 ******************************************************************/

static void
timer_destroy (void)
{
	(void) condvar_destroy (&toutq_condvar);
	(void) mutex_destroy (&toutq_lock);
	(void) mutex_destroy (&tinq_lock);
}

int
timer_initialize (void)
{
	if (mutex_initialize (&tinq_lock) == -1 ||
	    mutex_initialize (&toutq_lock) == -1 ||
	    condvar_initialize (&toutq_condvar) == -1)
		return (-1);
	if (atexit (timer_destroy) == 0)
		return (0);
	timer_destroy ();
	return (-1);
}

/*******************************************************************
 * TIMEOUT HANDLER THREAD
 ******************************************************************/

static void
toutq_add (tqueue *q)
{
	if (mutex_lock (&toutq_lock) == -1)
		return;

	/* insert into list */
	if (toutq_tail == NULL)
		toutq_head = q;
	else
		toutq_tail->next = q;
	toutq_tail = q;

	(void) condvar_signal (&toutq_condvar);
	mutex_unlock (&toutq_lock);
}

/* ARGSUSED */
static thread_rtn
timeout_thread (void *arg)
{
	tqueue *t, *n;

	(void) mutex_lock (&toutq_lock);
	for(;;)
	{
		if (condvar_wait (&toutq_condvar, &toutq_lock) == -1)
			continue;
		for (t = toutq_head; t != NULL; t = n)
		{
			if (t->notify)
				(*t->notify) (t->client);
			if (t->destroy)
				(*t->destroy) (t->client);
			n = t->next;
			free (t);
		}
		toutq_head = NULL;
		toutq_tail = NULL;
	}
	/* NOTREACHED */
	thread_return (0);
}

/*******************************************************************
 * TIMER THREAD
 ******************************************************************/

static tqueue *
timer_cancel_unlocked (satmp_esi_t esi)
{
	tqueue *t, *p = NULL;

	/* search for entry in chain */
	for (t = tinq_head; t != NULL; p = t, t = t->next)
		if (esi == t->esi)
			break;

	/* if there is no entry, stop here */
	if (t == NULL)
		return (t);

	/* remove from chain */
	if (p == NULL)
	{
		tinq_head = tinq_head->next;
		if (tinq_head == NULL)
			tinq_tail = NULL;
	}
	else
	{
		p->next = t->next;
		if (tinq_tail == t)
			tinq_tail = p;
	}
	
	/* cancel timer */
	t->timeout = 0;
	t->next = NULL;

	/* return pointer to entry */
	return (t);
}

/* subtract time from all timers, enabling any that run out along the way */
static void
update_all_timers_by (unsigned int time)
{
	tqueue *t, *next;

	for (t = tinq_head; t != NULL; t = next)
	{
		next = t->next;
		if (time < t->timeout)
		{
			t->timeout -= time;
		}
		else if (t->ntries-- > 0 && t->retry)
		{
			/* retry */
			t->timeout = t->otimeout;
			(*t->retry) (t->client);
		}
		else
		{
			/* remove from tinq, add to toutq */
			t = timer_cancel_unlocked (t->esi);
			if (t != NULL)
				toutq_add (t);
		}
	}
}

/* return time in tens of milliseconds */
static unsigned int
current_time (void)
{
	struct timeval tp;

	(void) gettimeofday (&tp, (struct timezone *) 0);

	/* ignore overflow */
	return (tp.tv_sec * USECS_PER_SEC + tp.tv_usec);
}

static tqueue *
shortest_timer (void)
{
	tqueue *t, *s_t;	/* shortest_timer */
	unsigned int old_timer = MANY_USECS;

	s_t = NULL;
	for (t = tinq_head; t != NULL; t = t->next)
	{
		if (t->timeout > 0 && t->timeout < old_timer)
		{
			old_timer = t->timeout;
			s_t = t;
		}
	}
	return (s_t);
}

static void
start_timer (tqueue *t)
{
	struct itimerval ntimer;

	if (t == NULL)
	{
		/* cancel interval timer */
		timerclear (&ntimer.it_value);
		timerclear (&ntimer.it_interval);
		(void) setitimer (TIMER_TYPE, &ntimer, (struct itimerval *) 0);
		return;
	}

	timer_set_timestamp = current_time ();

	/* initialize timer parameters */
	timerclear (&ntimer.it_interval);
	ntimer.it_value.tv_sec = SEC (t->timeout);
	ntimer.it_value.tv_usec = USEC (t->timeout);

	/* install timer parameters */
	(void) setitimer (TIMER_TYPE, &ntimer, (struct itimerval *) NULL);
}

static thread_id timer_tid;

static void
wakeup_timer (void)
{
	(void) thread_kill (timer_tid, TIMER_SIG);
}

/* ARGSUSED */
static thread_rtn
timer_thread (void *arg)
{
	int sig;
	sigset_t set;

	(void) sigemptyset (&set);
	(void) sigaddset (&set, TIMER_SIG);

	while (sigwait (&set, &sig) == 0 && sig == TIMER_SIG)
	{
		if (mutex_lock (&tinq_lock) == -1)
			continue;

		/* update timeout values */
		update_all_timers_by (current_time () - timer_set_timestamp);

		/* start timer for next shortest guy if one exists */
		start_timer (shortest_timer ());

		mutex_unlock (&tinq_lock);
	}
	/* NOTREACHED */
	thread_return (0);
}

void *
timer_cancel (satmp_esi_t esi)
{
	tqueue *t;
	void *client = NULL;

	/* check args && lock queue */
	if (mutex_lock (&tinq_lock) == -1)
		return (client);

	t = timer_cancel_unlocked (esi);
	if (t)
		client = t->client;

	mutex_unlock (&tinq_lock);

	free (t);
	return (client);
}

int
timer_set (time_t timeout, unsigned int ntries, satmp_esi_t esi,
	   void *client, timer_ast notify, timer_ast retry, timer_ast destroy)
{
	tqueue *t;

	/* allocate queue entry */
	t = (tqueue *) malloc (sizeof (*t));
	if (t == NULL)
		return (1);

	/* initialize queue entry */
	t->timeout = timeout;
	t->otimeout = timeout;
	t->ntries = ntries;
	t->esi = esi;
	t->client = client;
	t->notify = notify;
	t->retry = retry;
	t->destroy = destroy;
	t->next = NULL;

	/* lock queue */
	if (mutex_lock (&tinq_lock) == -1)
	{
		free (t);
		return (1);
	}

	/* insert into list */
	if (tinq_tail == NULL)
	{
		tinq_head = t;
		timer_set_timestamp = current_time ();
	}
	else
		tinq_tail->next = t;
	tinq_tail = t;

	/* unlock queue */
	mutex_unlock (&tinq_lock);

	/* signal waiter */
	wakeup_timer ();

	return (0);
}

static void
alarm_handler (void)
{
}

int
timer_threads_start (void)
{
	thread_id tid;
	sigset_t nset;
	struct sigaction act;

	/*
	 * Block TIMER_SIG except when explicitly waiting for it.
	 * Setup TIMER_SIG handler.
	 */
	act.sa_flags = 0;
	act.sa_handler = alarm_handler;
	if (sigemptyset (&nset) == -1 ||
	    sigaddset (&nset, TIMER_SIG) == -1 ||
	    thread_sigprocmask (SIG_BLOCK, &nset, (sigset_t *) NULL) == -1 ||
	    sigemptyset (&act.sa_mask) == -1 ||
	    sigaction (TIMER_SIG, &act, (struct sigaction *) NULL) == -1 ||
	    thread_create (timeout_thread, &tid, (void *) 0) == -1 ||
	    thread_create (timer_thread, &timer_tid, (void *) 0) == -1)
		return (-1);
	return (0);
}
