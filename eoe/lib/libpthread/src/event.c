/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* NOTES
 *
 * Key points:
 */

#include "common.h"
#include "delay.h"
#include "mtx.h"
#include "pt.h"
#include "pthreadrt.h"
#include "vp.h"
#include "event.h"

#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>


static void	*evt_thread(void *);

static void	(*evtop_exec)(int);
static void	(*evtop_fork)(void);

static int		evt_id = -1;
static pthread_mutex_t	evt_mtx = PTHREAD_MUTEX_INITIALIZER;


/* evt_thread(arg)
 *
 * This thread receives and dispatches events and deals with
 * timeout requests.
 */
/* ARGSUSED */
static void *
evt_thread(void *arg)
{
	timespec_t	ts;
	sigset_t	waitsigs;
	siginfo_t	info;
	extern int	_sigpoll(const sigset_t *, struct siginfo *,
				 struct timespec *);

	TRACE(T_EVT, ("evt_thread() started"));

	sigemptyset(&waitsigs);
	sigaddset(&waitsigs, SIGPTINTR);

	for (;;) {
		_sigpoll(&waitsigs, &info, timeout_evaluate(&ts));

		TRACE(T_EVT, ("evt_thread event %d %d %d %d",
				info.si_signo, info.si_code, info.si_errno,
				info.si_value.sival_int));
		
		/* Check for a sig event
		 * These events piggy-back on the SIGPTINTR signal
		 * and we invoke the libc code to service it.
		 */
		if (info.si_code != SI_USER && evtop_exec) {
			evtop_exec((int)info.si_value.sival_int);
		}
	}
}


/* evt_start()
 */
int
libc_evt_start(void (*evtop_exf)(int), void (*evtop_forkf)(void))
{
	pt_t	*evt_pt;
	int	ret = 0;

	if (evtop_exf) {	/* libc may call us with callbacks */
		evtop_exec = evtop_exf;
		evtop_fork = evtop_forkf;
	}

	if (evt_id != -1) {	/* fast path */
		return (0);
	}

	pthread_mutex_lock(&evt_mtx);
	if (evt_id == -1
	    && (ret = pt_create_bound(&evt_pt, evt_thread, (void *)0)) == 0) {

		TRACE(T_EVT, ("evt_start()"));
		evt_id = evt_pt->pt_vp->vp_pid;
	}
	pthread_mutex_unlock(&evt_mtx);
	return (ret);
}


/* evt_prompt()
 *
 * Prod the event thread.
 */
void
evt_prompt(void)
{
	TRACE(T_EVT, ("evt_prompt()"));

	ASSERT(sched_entered());
	ASSERT(evt_id != -1);
	prctl(PR_THREAD_CTL, PR_THREAD_KILL, evt_id, SIGPTINTR);
}


/* evt_fork_child()
 */
void
evt_fork_child(void)
{
	pthread_mutex_init(&evt_mtx, 0);
	evt_id = -1;

	if (evtop_fork) {	/* libc event code fork() recovery */
		evtop_fork();
	}
}

