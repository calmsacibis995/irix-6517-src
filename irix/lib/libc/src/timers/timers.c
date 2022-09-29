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

#ifdef __STDC__
#	pragma weak timer_create	= _timer_create
#	pragma weak timer_delete	= _timer_delete
#endif 

#include "synonyms.h"
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sigevtop.h>


/* POSIX 1b Timers */


int
timer_create(clockid_t cid, struct sigevent *sev, timer_t *tid)
{
	int		e;
	int		key;
	sigevent_t	sev_kern;
	evtop_data_t	data;
	extern int __timer_create(clockid_t, struct sigevent *, timer_t *);

	/* Go straight to the kernel if no event thread */
	if (!sev || sev->sigev_notify != SIGEV_THREAD) {
		return (__timer_create(cid, sev, tid));
	}

	/* Make sure event code is initialised and get a key */
	if (e = __evtop_alloc(sev, EVTOP_TMR, 0, &key)) {
		setoserror(e);
		return (-1);
	}

	/* Set up kernel sigevent part */
	sev_kern = *sev;			/* copy user input */
	sev_kern.sigev_signo = SIGPTINTR;	/* special event signal */
	sev_kern.sigev_notify = SIGEV_SIGNAL;
	sev_kern.sigev_value.sival_int = key;	/* passed back by signal */

	/* Create disarmed kernel timer */
	if (__timer_create(cid, &sev_kern, tid)) {
		e = oserror();
	}

	/* No timer so undo the event op */
	if (e) {
		__evtop_free(key);
		setoserror(e);
		return (-1);
	}

	/* Fill in the timer id so it can be found by delete
	 * Ideally done in alloc but we didn't have the id.
	 */
	data.evtop_tid = *tid;
	__evtop_setid(key, &data);
	return (0);
}


int
timer_delete(timer_t tid)
{
	int		key;
	evtop_data_t	data;
	extern int __timer_delete(timer_t);

	if (__timer_delete(tid)) {
		return (-1);
	}

	/* Locate any timer event and delete it */
	data.evtop_tid = tid;
	if (__evtop_lookup(EVTOP_TMR, &data, &key) == 0) {
		__evtop_free(key);
	}

	return (0);
}

