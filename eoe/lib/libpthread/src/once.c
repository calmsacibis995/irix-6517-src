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
 * This module implements the synchronised function call interface, aka
 * dynamic package initialisation.
 *
 * Key points:
	* POSIX requires us to deal with and recover from cancellation.
 */

#include "common.h"
#include "cv.h"
#include "mutex.h"
#include "pt.h"
#include "sys.h"
#include "once.h"

#include <pthread.h>


static pthread_mutex_t	once_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t	once_cv = PTHREAD_COND_INITIALIZER;

static void once_reset(void *);

/*
 * once_reset(control)
 *
 */
static void
once_reset(void *arg)
{
	pthread_once_t	*control = (pthread_once_t *)arg;

	*control = PTHREAD_ONCE_INIT;
	pthread_cond_broadcast(&once_cv);
}


/*
 * pthread_once(once_control, init_routine)
 *
 */
int
pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
	int	old_cncl_state;
	int	cncl_type = PT->pt_cncl_deferred;

	if (*once_control == PTHREAD_ONCE_DONE) {
		return (0);
	}

	pthread_mutex_lock(&once_mtx);

	do {
		if (*once_control == PTHREAD_ONCE_INIT) {

			*once_control = PTHREAD_ONCE_PEND;
			pthread_mutex_unlock(&once_mtx);

			pthread_cleanup_push(once_reset, (void *)once_control);
			(*init_routine)();
#			pragma set woff 1209
			pthread_cleanup_pop(0);
#			pragma reset woff 1209

			pthread_mutex_lock(&once_mtx);
			*once_control = PTHREAD_ONCE_DONE;
			pthread_mutex_unlock(&once_mtx);

			pthread_cond_broadcast(&once_cv);
			return (0);

		}

		/*
		 * pthread_once() is not a cancellation point so we make
		 * sure we don't handle synchronous cancellations in the
		 * condition wait.  If the type is asynchronous, then we
		 * don't bother.
		 */
		while (*once_control == PTHREAD_ONCE_PEND) {
			if (cncl_type == PTHREAD_CANCEL_DEFERRED) {
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,
						       &old_cncl_state);
			}

			pthread_cond_wait(&once_cv, &once_mtx);

			if (cncl_type == PTHREAD_CANCEL_DEFERRED) {
				pthread_setcancelstate(old_cncl_state, NULL);
			}
		}

	} while (*once_control != PTHREAD_ONCE_DONE);

	pthread_mutex_unlock(&once_mtx);
	return (0);
}
